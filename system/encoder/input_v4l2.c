/*
    TVLTx - TV Lovers Encoder
    Copyright (C) 2026 SDR Telecom

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * Video4Linux2 + ALSA input driver.
 *
 * Replaces the Blackmagic DeckLink input with generic Linux capture devices
 * (USB/PCIe HDMI grabbers, analog composite/S-Video capture boards, webcams).
 *
 * It feeds the encoder exactly what the DeckLink driver does:
 *   - video: packed UYVY 4:2:2 8-bit frames, of the size implied by the
 *            input card "mode" (e.g. 720x486 for 480i, as x264 is told so);
 *   - audio: interleaved signed 16-bit little-endian PCM at 48 kHz, one
 *            buffer per audio stream, with the stream's channel count.
 *
 * Because the video device and the sound card normally run from different
 * (and free-running) clocks, both streams are slaved to CLOCK_MONOTONIC from
 * a common per-input epoch: video frames are duplicated/dropped and audio
 * samples are inserted/dropped so that each stream always delivers exactly
 * "elapsed time x nominal rate" units. This keeps A/V in sync and the encoder
 * at a constant frame/sample rate even with cheap capture hardware. When the
 * video source disappears, color bars are generated (same as DeckLink).
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <alsa/asoundlib.h>

#include "encoder_opts.h"
#include "macros.h"
#include "input.h"
#include "buffer.h"

extern service_config services[MAX_SERVICES];
extern bool running;
extern int nr_services;

#define V4L2_NR_BUFFERS       8
#define NO_SIGNAL_TIMEOUT_NS  500000000LL   // 0.5 s without frames -> color bars

#define AUDIO_RATE            48000
#define AUDIO_PERIOD          1024          // frames per ALSA read
#define AUDIO_SOFT_DRIFT      (AUDIO_RATE / 100)  // 10 ms: start 1-sample corrections
#define AUDIO_HARD_DRIFT      (AUDIO_RATE / 25)   // 40 ms: insert silence / drop period
#define MAX_CAPTURE_CHANNELS  16

typedef struct
{
    int width;
    int height;
    int fps_num;
    int fps_den;
    bool interlaced;
    v4l2_std_id std;   // analog standard family, 0 for digital modes
} v4l2_mode;

typedef struct
{
    input_config *input;
    int64_t epoch_ns;
} v4l2_input_ctx;

static v4l2_input_ctx contexts[MAX_INPUT_INTERFACES];

static int64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t) ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static int xioctl(int fd, unsigned long request, void *arg)
{
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

static int mode_lookup(int video_input_mode, v4l2_mode *m)
{
    memset(m, 0, sizeof(*m));
    switch (video_input_mode)
    {
    case VIDEO_MODE_1080_P_60:   *m = (v4l2_mode){1920, 1080, 60,    1,    false, 0}; break;
    case VIDEO_MODE_1080_P_5994: *m = (v4l2_mode){1920, 1080, 60000, 1001, false, 0}; break;
    case VIDEO_MODE_1080_P_50:   *m = (v4l2_mode){1920, 1080, 50,    1,    false, 0}; break;
    case VIDEO_MODE_1080_I_5994: *m = (v4l2_mode){1920, 1080, 30000, 1001, true,  0}; break;
    case VIDEO_MODE_1080_I_60:   *m = (v4l2_mode){1920, 1080, 30,    1,    true,  0}; break;
    case VIDEO_MODE_1080_I_50:   *m = (v4l2_mode){1920, 1080, 25,    1,    true,  0}; break;
    case VIDEO_MODE_1080_P_30:   *m = (v4l2_mode){1920, 1080, 30,    1,    false, 0}; break;
    case VIDEO_MODE_1080_P_2997: *m = (v4l2_mode){1920, 1080, 30000, 1001, false, 0}; break;
    case VIDEO_MODE_1080_P_25:   *m = (v4l2_mode){1920, 1080, 25,    1,    false, 0}; break;
    case VIDEO_MODE_720_P_60:    *m = (v4l2_mode){1280, 720,  60,    1,    false, 0}; break;
    case VIDEO_MODE_720_P_5994:  *m = (v4l2_mode){1280, 720,  60000, 1001, false, 0}; break;
    case VIDEO_MODE_720_P_50:    *m = (v4l2_mode){1280, 720,  50,    1,    false, 0}; break;
    case VIDEO_MODE_720_P_30:    *m = (v4l2_mode){1280, 720,  30,    1,    false, 0}; break;
    case VIDEO_MODE_720_P_2997:  *m = (v4l2_mode){1280, 720,  30000, 1001, false, 0}; break;
    case VIDEO_MODE_720_P_25:    *m = (v4l2_mode){1280, 720,  25,    1,    false, 0}; break;
    // the encoder feeds x264 with 720x486 for 525-line modes (DeckLink size)
    case VIDEO_MODE_480_I_5994:  *m = (v4l2_mode){720,  486,  30000, 1001, true,  V4L2_STD_525_60}; break;
    case VIDEO_MODE_480_P_2997:  *m = (v4l2_mode){720,  486,  30000, 1001, false, V4L2_STD_525_60}; break;
    case VIDEO_MODE_576_I_50:    *m = (v4l2_mode){720,  576,  25,    1,    true,  V4L2_STD_625_50}; break;
    case VIDEO_MODE_576_P_25:    *m = (v4l2_mode){720,  576,  25,    1,    false, V4L2_STD_625_50}; break;
    default:
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Video                                                                      */
/* ------------------------------------------------------------------------ */

typedef struct
{
    void *start;
    size_t length;
} mmap_buffer;

typedef struct
{
    input_config *input;
    int64_t epoch_ns;
    int fps_num;
    int fps_den;
    int frame_size;
    uint64_t frames_out;
    uint64_t frames_dup;
    uint64_t frames_drop;
} video_cadence;

static void video_write(video_cadence *vc, uint8_t *frame)
{
    for (int i = 0; i < nr_services; i++) {
        if (services[i].input_id == vc->input->input_id)
            write_buffer(&services[i].buf_video_in, frame, vc->frame_size);
    }
    vc->frames_out++;
}

// number of frames that should have been delivered at time t (frame 0 at epoch)
static uint64_t video_expected(video_cadence *vc, int64_t t)
{
    if (t <= vc->epoch_ns)
        return 1;
    double elapsed = (double) (t - vc->epoch_ns) / 1e9;
    return (uint64_t) (elapsed * vc->fps_num / vc->fps_den) + 1;
}

/* Deliver a captured frame keeping a constant cadence: +-1 frame of slack,
 * drop when the source runs fast, repeat the frame when it runs slow. */
static void video_deliver(video_cadence *vc, uint8_t *frame, int64_t t)
{
    uint64_t expected = video_expected(vc, t);

    if (vc->frames_out >= expected + 1) {
        vc->frames_drop++;
        return;
    }

    video_write(vc, frame);

    while (running && vc->frames_out + 1 < expected) {
        video_write(vc, frame);
        vc->frames_dup++;
    }
}

static void video_fill(video_cadence *vc, uint8_t *frame, int64_t t)
{
    uint64_t expected = video_expected(vc, t);
    while (running && vc->frames_out < expected)
        video_write(vc, frame);
}

static void fill_black(uint8_t *frame, int width, int height)
{
    uint32_t *p = (uint32_t *) frame;
    for (int i = 0; i < width * height / 2; i++)
        *p++ = 0x10801080; // U=0x80 Y=0x10 V=0x80 Y=0x10 (little endian)
}

static void fill_bars(uint8_t *frame, int width, int height)
{
    // same 75% bars as the DeckLink driver uses on "no input source"
    static const uint32_t bars[8] = {
        0xEA80EA80, 0xD292D210, 0xA910A9A5, 0x90229035,
        0x6ADD6ACA, 0x51EF515A, 0x286D28EF, 0x10801080 };
    uint32_t *p = (uint32_t *) frame;
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x += 2)
            *p++ = bars[(x * 8) / width];
}

typedef struct
{
    uint32_t pixelformat;
    int src_width;
    int src_height;
    int bytesperline;
    // destination (encoder frame) and the region we copy into / from
    int dst_width;
    int dst_height;
    int copy_width;
    int copy_height;
    int dst_x;
    int dst_y;
    int src_x;
    int src_y;
} frame_geometry;

static void setup_geometry(frame_geometry *g)
{
    g->copy_width  = (g->src_width < g->dst_width ? g->src_width : g->dst_width) & ~1;
    g->copy_height = g->src_height < g->dst_height ? g->src_height : g->dst_height;

    // keep x on a pixel pair and y even, so the field order is preserved
    g->dst_x = ((g->dst_width  - g->copy_width)  / 2) & ~1;
    g->src_x = ((g->src_width  - g->copy_width)  / 2) & ~1;
    g->dst_y = ((g->dst_height - g->copy_height) / 2) & ~1;
    g->src_y = ((g->src_height - g->copy_height) / 2) & ~1;

    // 720x480 into the 720x486 frame used for 525-line video
    if (g->dst_height == 486 && g->copy_height == 480)
        g->dst_y = 4;
}

/* Convert one captured frame to UYVY into dst (borders are left untouched). */
static void convert_frame(const frame_geometry *g, const uint8_t *src, uint8_t *dst)
{
    int dst_stride = g->dst_width * 2;

    for (int y = 0; y < g->copy_height; y++) {
        int sy = y + g->src_y;
        uint8_t *d = dst + (size_t) (y + g->dst_y) * dst_stride + g->dst_x * 2;

        switch (g->pixelformat)
        {
        case V4L2_PIX_FMT_UYVY: {
            const uint8_t *s = src + (size_t) sy * g->bytesperline + g->src_x * 2;
            memcpy(d, s, g->copy_width * 2);
            break;
        }
        case V4L2_PIX_FMT_YUYV: {
            const uint8_t *s = src + (size_t) sy * g->bytesperline + g->src_x * 2;
            for (int x = 0; x < g->copy_width * 2; x += 4) {
                d[x + 0] = s[x + 1]; // U
                d[x + 1] = s[x + 0]; // Y0
                d[x + 2] = s[x + 3]; // V
                d[x + 3] = s[x + 2]; // Y1
            }
            break;
        }
        case V4L2_PIX_FMT_NV12: {
            const uint8_t *s_y  = src + (size_t) sy * g->bytesperline + g->src_x;
            const uint8_t *s_uv = src + (size_t) g->src_height * g->bytesperline +
                                  (size_t) (sy / 2) * g->bytesperline + g->src_x;
            for (int x = 0; x < g->copy_width; x += 2) {
                d[2 * x + 0] = s_uv[x];
                d[2 * x + 1] = s_y[x];
                d[2 * x + 2] = s_uv[x + 1];
                d[2 * x + 3] = s_y[x + 1];
            }
            break;
        }
        case V4L2_PIX_FMT_YUV420: {
            int c_stride = g->bytesperline / 2;
            const uint8_t *s_y = src + (size_t) sy * g->bytesperline + g->src_x;
            const uint8_t *s_u = src + (size_t) g->src_height * g->bytesperline +
                                 (size_t) (sy / 2) * c_stride + g->src_x / 2;
            const uint8_t *s_v = src + (size_t) g->src_height * g->bytesperline +
                                 (size_t) (g->src_height / 2) * c_stride +
                                 (size_t) (sy / 2) * c_stride + g->src_x / 2;
            for (int x = 0; x < g->copy_width; x += 2) {
                d[2 * x + 0] = s_u[x / 2];
                d[2 * x + 1] = s_y[x];
                d[2 * x + 2] = s_v[x / 2];
                d[2 * x + 3] = s_y[x + 1];
            }
            break;
        }
        }
    }
}

static size_t min_frame_bytes(const frame_geometry *g)
{
    size_t luma = (size_t) g->bytesperline * g->src_height;
    switch (g->pixelformat)
    {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_YUV420:
        return luma + luma / 2;
    default:
        return luma;
    }
}

static const char *fourcc_str(uint32_t f, char out[5])
{
    out[0] = f & 0xff; out[1] = (f >> 8) & 0xff;
    out[2] = (f >> 16) & 0xff; out[3] = (f >> 24) & 0xff; out[4] = 0;
    return out;
}

/* Select the V4L2 input (connector) matching the configured input "type". */
static void select_video_input(int fd, int input_type)
{
    const char *want[3] = { NULL, NULL, NULL };

    switch (input_type)
    {
    case INPUT_TYPE_COMPOSITE:  want[0] = "composite"; want[1] = "cvbs"; break;
    case INPUT_TYPE_SVIDEO:     want[0] = "s-video"; want[1] = "svideo"; want[2] = "y/c"; break;
    case INPUT_TYPE_COMPONENT:  want[0] = "component"; want[1] = "ypbpr"; break;
    case INPUT_TYPE_HDMI:       want[0] = "hdmi"; break;
    case INPUT_TYPE_SDI:
    case INPUT_TYPE_OPTICALSDI: want[0] = "sdi"; break;
    default:
        return; // keep the device's current input
    }

    struct v4l2_input in;
    for (int idx = 0; ; idx++) {
        memset(&in, 0, sizeof(in));
        in.index = idx;
        if (xioctl(fd, VIDIOC_ENUMINPUT, &in) == -1)
            break;
        for (int w = 0; w < 3 && want[w]; w++) {
            if (strcasestr((const char *) in.name, want[w])) {
                if (xioctl(fd, VIDIOC_S_INPUT, &idx) == -1)
                    tvltx_info("V4L2: could not select input %d (%s): %s\n", idx, in.name, strerror(errno));
                else
                    tvltx_info("V4L2: selected input %d (%s)\n", idx, in.name);
                return;
            }
        }
    }
    tvltx_info("V4L2: no device input matches the configured type, keeping the current one\n");
}

static v4l2_std_id parse_video_std(const char *s)
{
    if (!s || !s[0] || !strcasecmp(s, "auto")) return 0;
    if (!strcasecmp(s, "pal-m"))  return V4L2_STD_PAL_M;
    if (!strcasecmp(s, "pal-n"))  return V4L2_STD_PAL_N;
    if (!strcasecmp(s, "pal-nc")) return V4L2_STD_PAL_Nc;
    if (!strcasecmp(s, "pal"))    return V4L2_STD_PAL;
    if (!strcasecmp(s, "pal-60")) return V4L2_STD_PAL_60;
    if (!strcasecmp(s, "ntsc"))   return V4L2_STD_NTSC_M;
    if (!strcasecmp(s, "ntsc-j")) return V4L2_STD_NTSC_M_JP;
    if (!strcasecmp(s, "secam"))  return V4L2_STD_SECAM;
    tvltx_info("V4L2: unknown videostd '%s', using auto-detection\n", s);
    return 0;
}

/* Analog devices need a TV standard; digital ones (HDMI/SDI) don't have it. */
static void set_video_std(int fd, const v4l2_mode *m, const char *std_name)
{
    if (m->std == 0)
        return;

    v4l2_std_id std = parse_video_std(std_name);
    if (std == 0) {
        v4l2_std_id detected = m->std;
        if (xioctl(fd, VIDIOC_QUERYSTD, &detected) == 0 && (detected & m->std))
            std = detected & m->std;
        else
            std = m->std == V4L2_STD_525_60 ? V4L2_STD_NTSC_M : V4L2_STD_PAL;
    }

    if (xioctl(fd, VIDIOC_S_STD, &std) == -1) {
        if (errno != ENOTTY && errno != ENODATA)
            tvltx_info("V4L2: VIDIOC_S_STD failed: %s\n", strerror(errno));
    } else {
        tvltx_info("V4L2: video standard set to 0x%llx\n", (unsigned long long) std);
    }
}

static int set_video_format(int fd, const v4l2_mode *m, frame_geometry *g)
{
    static const uint32_t formats[] = {
        V4L2_PIX_FMT_UYVY, V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_YUV420 };
    struct v4l2_format fmt;
    char fcc[5];

    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++) {
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = m->width;
        fmt.fmt.pix.height = m->height;
        fmt.fmt.pix.pixelformat = formats[i];
        fmt.fmt.pix.field = m->interlaced ? V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;

        if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
            continue;
        if (fmt.fmt.pix.pixelformat != formats[i])
            continue;

        if (fmt.fmt.pix.field == V4L2_FIELD_ALTERNATE ||
            fmt.fmt.pix.field == V4L2_FIELD_TOP ||
            fmt.fmt.pix.field == V4L2_FIELD_BOTTOM ||
            fmt.fmt.pix.field == V4L2_FIELD_SEQ_TB ||
            fmt.fmt.pix.field == V4L2_FIELD_SEQ_BT) {
            tvltx_info("V4L2: device only offers field-separated capture (field=%u) for %s, not supported\n",
                       fmt.fmt.pix.field, fourcc_str(formats[i], fcc));
            continue;
        }

        g->pixelformat = fmt.fmt.pix.pixelformat;
        g->src_width = fmt.fmt.pix.width;
        g->src_height = fmt.fmt.pix.height;
        g->bytesperline = fmt.fmt.pix.bytesperline;
        if (g->bytesperline == 0)
            g->bytesperline = g->pixelformat == V4L2_PIX_FMT_UYVY ||
                              g->pixelformat == V4L2_PIX_FMT_YUYV ? g->src_width * 2 : g->src_width;

        tvltx_info("V4L2: capturing %dx%d %s (field %u, stride %d)\n", g->src_width, g->src_height,
                   fourcc_str(g->pixelformat, fcc), fmt.fmt.pix.field, g->bytesperline);
        if (g->src_width != m->width || g->src_height != m->height)
            tvltx_info("V4L2: device size differs from configured mode %dx%d, cropping/padding (no scaling)\n",
                       m->width, m->height);
        if (m->interlaced && fmt.fmt.pix.field == V4L2_FIELD_NONE)
            tvltx_info("V4L2: warning: interlaced mode configured but device delivers progressive frames\n");
        return 0;
    }

    tvltx_info("V4L2: device supports none of UYVY, YUYV, NV12, YU12 at %dx%d (MJPEG is not supported)\n",
               m->width, m->height);
    return -1;
}

static void set_frame_rate(int fd, const v4l2_mode *m)
{
    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(fd, VIDIOC_G_PARM, &parm) == -1 ||
        !(parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME))
        return;

    parm.parm.capture.timeperframe.numerator = m->fps_den;
    parm.parm.capture.timeperframe.denominator = m->fps_num;
    if (xioctl(fd, VIDIOC_S_PARM, &parm) == -1) {
        tvltx_info("V4L2: VIDIOC_S_PARM failed: %s\n", strerror(errno));
        return;
    }
    tvltx_info("V4L2: device frame interval %u/%u s (wanted %d/%d)\n",
               parm.parm.capture.timeperframe.numerator,
               parm.parm.capture.timeperframe.denominator, m->fps_den, m->fps_num);
}

static void *v4l2_video_thread(void *arg)
{
    v4l2_input_ctx *ctx = arg;
    input_config *input = ctx->input;
    v4l2_mode m;
    frame_geometry g;
    mmap_buffer buffers[V4L2_NR_BUFFERS];
    unsigned int nr_buffers = 0;
    uint8_t *frame = NULL, *bars = NULL;
    char dev[sizeof(input->video_device)];
    int fd = -1;
    bool streaming = false, no_signal = false;
    video_cadence vc;

    memset(&g, 0, sizeof(g));
    memset(buffers, 0, sizeof(buffers));

    if (mode_lookup(input->video_input_mode, &m)) {
        tvltx_info("V4L2: input %d: unsupported video mode %d\n", input->input_id, input->video_input_mode);
        return (void *) EXIT_FAILURE;
    }

    if (input->video_device[0])
        snprintf(dev, sizeof(dev), "%s", input->video_device);
    else
        snprintf(dev, sizeof(dev), "/dev/video%d", input->card_number);

    g.dst_width = m.width;
    g.dst_height = m.height;

    memset(&vc, 0, sizeof(vc));
    vc.input = input;
    vc.epoch_ns = ctx->epoch_ns;
    vc.fps_num = m.fps_num;
    vc.fps_den = m.fps_den;
    vc.frame_size = m.width * m.height * 2;

    frame = malloc(vc.frame_size);
    bars = malloc(vc.frame_size);
    if (!frame || !bars)
        goto bail;
    fill_black(frame, m.width, m.height);
    fill_bars(bars, m.width, m.height);

    fd = open(dev, O_RDWR | O_NONBLOCK);
    if (fd == -1) {
        tvltx_info("V4L2: cannot open %s: %s\n", dev, strerror(errno));
        goto no_device;
    }

    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        tvltx_info("V4L2: %s is not a V4L2 device\n", dev);
        goto no_device;
    }
    uint32_t caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
    if (!(caps & V4L2_CAP_VIDEO_CAPTURE) || !(caps & V4L2_CAP_STREAMING)) {
        tvltx_info("V4L2: %s (%s) has no single-planar streaming capture support\n", dev, cap.card);
        goto no_device;
    }
    tvltx_info("V4L2: input %d: %s: %s (%s)\n", input->input_id, dev, cap.card, cap.driver);

    select_video_input(fd, input->input_type);
    set_video_std(fd, &m, input->video_std);
    if (set_video_format(fd, &m, &g))
        goto no_device;
    set_frame_rate(fd, &m);
    setup_geometry(&g);

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = V4L2_NR_BUFFERS;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1 || req.count < 2) {
        tvltx_info("V4L2: VIDIOC_REQBUFS failed: %s\n", strerror(errno));
        goto no_device;
    }
    if (req.count > V4L2_NR_BUFFERS)
        req.count = V4L2_NR_BUFFERS;

    for (nr_buffers = 0; nr_buffers < req.count; nr_buffers++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = nr_buffers;
        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)
            break;
        buffers[nr_buffers].length = buf.length;
        buffers[nr_buffers].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffers[nr_buffers].start == MAP_FAILED) {
            buffers[nr_buffers].start = NULL;
            break;
        }
        if (xioctl(fd, VIDIOC_QBUF, &buf) == -1)
            break;
    }
    if (nr_buffers < req.count) {
        tvltx_info("V4L2: buffer setup failed: %s\n", strerror(errno));
        goto no_device;
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        tvltx_info("V4L2: VIDIOC_STREAMON failed: %s\n", strerror(errno));
        goto no_device;
    }
    streaming = true;

    int64_t last_frame_ns = now_ns();
    int64_t last_stats_ns = last_frame_ns;
    int poll_ms = (int) (2000LL * m.fps_den / m.fps_num) + 1; // ~2 frame periods

    while (running) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        int r = poll(&pfd, 1, poll_ms);
        int64_t t = now_ns();

        if (r == -1 && errno != EINTR) {
            tvltx_info("V4L2: poll error: %s\n", strerror(errno));
            break;
        }

        if (r > 0 && (pfd.revents & POLLIN)) {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
                if (errno == EAGAIN)
                    continue;
                tvltx_info("V4L2: VIDIOC_DQBUF failed: %s\n", strerror(errno));
                break;
            }

            // use the capture time, so frames queued while we were blocked
            // by the encoder are placed correctly on the timeline
            int64_t ts = t;
            if ((buf.flags & V4L2_BUF_FLAG_TIMESTAMP_MASK) == V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC &&
                (buf.timestamp.tv_sec || buf.timestamp.tv_usec))
                ts = (int64_t) buf.timestamp.tv_sec * 1000000000LL + buf.timestamp.tv_usec * 1000LL;

            bool good = !(buf.flags & V4L2_BUF_FLAG_ERROR) && buf.index < nr_buffers &&
                        buf.bytesused >= min_frame_bytes(&g);
            if (good)
                convert_frame(&g, buffers[buf.index].start, frame);

            if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
                tvltx_info("V4L2: VIDIOC_QBUF failed: %s\n", strerror(errno));
                break;
            }

            if (!good)
                continue;

            if (no_signal) {
                tvltx_info("V4L2: input %d: video signal back\n", input->input_id);
                no_signal = false;
            }
            last_frame_ns = t;
            video_deliver(&vc, frame, ts);
        }
        else if (t - last_frame_ns > NO_SIGNAL_TIMEOUT_NS) {
            if (!no_signal) {
                tvltx_info("V4L2: input %d: no video signal, sending color bars\n", input->input_id);
                no_signal = true;
            }
            video_fill(&vc, bars, t);
        }

        if (t - last_stats_ns > 60 * 1000000000LL) {
            if (vc.frames_dup || vc.frames_drop)
                tvltx_debug("V4L2: input %d: %llu frames out, %llu repeated, %llu dropped\n", input->input_id,
                            (unsigned long long) vc.frames_out, (unsigned long long) vc.frames_dup,
                            (unsigned long long) vc.frames_drop);
            last_stats_ns = t;
        }
    }
    goto bail;

no_device:
    // keep the encoder fed so the rest of the multiplex stays on air
    tvltx_info("V4L2: input %d: running without a video device, sending color bars\n", input->input_id);
    while (running) {
        int64_t t = now_ns();
        video_fill(&vc, bars, t);
        struct timespec ts = { 0, 20000000 }; // 20 ms
        nanosleep(&ts, NULL);
    }

bail:
    if (streaming) {
        enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(fd, VIDIOC_STREAMOFF, &t);
    }
    for (unsigned int i = 0; i < V4L2_NR_BUFFERS; i++)
        if (buffers[i].start)
            munmap(buffers[i].start, buffers[i].length);
    if (fd != -1)
        close(fd);
    free(frame);
    free(bars);
    tvltx_debug("Exiting V4L2 video input thread.\n");
    return EXIT_SUCCESS;
}

/* ------------------------------------------------------------------------ */
/* Audio                                                                      */
/* ------------------------------------------------------------------------ */

/* Send captured audio to every audio stream fed by this input. Each stream
 * gets nr_channels[j] channels picked by its "mapping" bitmask (the input
 * channel indices, in ascending order); without a mapping it takes the first
 * channels. Missing channels are silent. */
static void audio_write(input_config *input, const int16_t *pcm, int frames, int in_ch)
{
    for (int i = 0; i < nr_services; i++) {
        if (services[i].input_id != input->input_id)
            continue;

        for (int j = 0; j < services[i].nr_audio_streams; j++) {
            int nch = services[i].nr_channels[j];
            int map[MAX_CAPTURE_CHANNELS];
            int count = 0;
            unsigned int mask = services[i].audio_mapping[j];

            if (nch <= 0)
                continue;
            if (nch > MAX_CAPTURE_CHANNELS)
                nch = MAX_CAPTURE_CHANNELS;

            for (int c = 0; c < 32 && count < nch; c++)
                if (mask & (1u << c))
                    map[count++] = c;
            if (count == 0)
                for (; count < nch; count++)
                    map[count] = count;
            for (; count < nch; count++)
                map[count] = -1;

            int16_t out[frames * nch];
            for (int f = 0; f < frames; f++)
                for (int k = 0; k < nch; k++)
                    out[f * nch + k] = (map[k] >= 0 && map[k] < in_ch) ? pcm[f * in_ch + map[k]] : 0;

            write_buffer(&services[i].buf_audio_in[j], (uint8_t *) out, frames * nch * 2);
        }
    }
}

static void audio_silence(input_config *input, int64_t frames)
{
    int16_t zeros[AUDIO_PERIOD * MAX_CAPTURE_CHANNELS];
    memset(zeros, 0, sizeof(zeros));
    while (frames > 0 && running) {
        int n = frames > AUDIO_PERIOD ? AUDIO_PERIOD : (int) frames;
        audio_write(input, zeros, n, MAX_CAPTURE_CHANNELS);
        frames -= n;
    }
}

static snd_pcm_t *audio_open(const char *dev, int *channels)
{
    snd_pcm_t *pcm = NULL;
    int err = snd_pcm_open(&pcm, dev, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        tvltx_info("ALSA: cannot open '%s': %s\n", dev, snd_strerror(err));
        return NULL;
    }

    int tries[3] = { *channels, 2, 1 };
    for (int i = 0; i < 3; i++) {
        if (tries[i] <= 0 || tries[i] > MAX_CAPTURE_CHANNELS || (i > 0 && tries[i] >= *channels))
            continue;
        // soft resampling allowed, so "plughw:" / "default" devices can adapt
        err = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                                 tries[i], AUDIO_RATE, 1, 200000);
        if (err == 0) {
            if (tries[i] != *channels)
                tvltx_info("ALSA: '%s' does not do %d channels, capturing %d\n", dev, *channels, tries[i]);
            *channels = tries[i];
            return pcm;
        }
    }
    tvltx_info("ALSA: cannot configure '%s' for S16_LE/48kHz: %s\n", dev, snd_strerror(err));
    snd_pcm_close(pcm);
    return NULL;
}

static void *alsa_audio_thread(void *arg)
{
    v4l2_input_ctx *ctx = arg;
    input_config *input = ctx->input;
    char dev[sizeof(input->audio_device)];
    int channels = input->audio_input_channel_nr > 0 ? input->audio_input_channel_nr : 2;
    int64_t written = 0;  // frames delivered since the epoch
    snd_pcm_t *pcm = NULL;
    uint64_t corr_up = 0, corr_down = 0;
    double drift_avg = 0;  // smoothed drift, the delay report is coarse on some devices
    bool aligned = false;  // hard-align to the timeline on start and after xruns
    int64_t last_stats_ns = now_ns();

    if (input->audio_device[0])
        snprintf(dev, sizeof(dev), "%s", input->audio_device);
    else
        snprintf(dev, sizeof(dev), "default");

    if (strcasecmp(dev, "none") != 0) {
        pcm = audio_open(dev, &channels);
        if (pcm)
            tvltx_info("ALSA: input %d: capturing from '%s', %d channels\n", input->input_id, dev, channels);
    }
    if (!pcm)
        tvltx_info("ALSA: input %d: no audio device, sending silence\n", input->input_id);

    // one spare frame so we can duplicate the last sample frame
    int16_t pcm_buf[(AUDIO_PERIOD + 1) * MAX_CAPTURE_CHANNELS];

    while (running) {
        if (!pcm) {
            // silence paced by the monotonic clock
            int64_t next = ctx->epoch_ns + (written + AUDIO_PERIOD) * 1000000000LL / AUDIO_RATE;
            struct timespec ts = { next / 1000000000LL, next % 1000000000LL };
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
            audio_silence(input, AUDIO_PERIOD);
            written += AUDIO_PERIOD;
            continue;
        }

        snd_pcm_sframes_t n = snd_pcm_readi(pcm, pcm_buf, AUDIO_PERIOD);
        if (n < 0) {
            if (n == -EPIPE)
                tvltx_debug("ALSA: input %d: overrun\n", input->input_id);
            aligned = false;
            if (snd_pcm_recover(pcm, (int) n, 1) < 0) {
                tvltx_info("ALSA: read error: %s\n", snd_strerror((int) n));
                struct timespec ts = { 0, 100000000 };
                nanosleep(&ts, NULL);
            }
            continue;
        }
        if (n == 0)
            continue;

        // where on the timeline is the last sample we just read?
        snd_pcm_sframes_t delay = 0;
        if (snd_pcm_delay(pcm, &delay) < 0 || delay < 0)
            delay = 0;
        int64_t t = now_ns();
        int64_t expected = (t - ctx->epoch_ns) * AUDIO_RATE / 1000000000LL - delay;
        int64_t drift = written + n - expected; // > 0: ahead, < 0: behind

        if (!aligned || drift < -AUDIO_HARD_DRIFT || drift > AUDIO_HARD_DRIFT) {
            aligned = true;
            drift_avg = 0;
            if (drift < 0) {
                // start-up or overrun: catch up with silence
                audio_silence(input, -drift);
                written += -drift;
            } else if (drift >= n) {
                // way ahead: drop this period
                continue;
            } else {
                // ahead by less than a period: drop its first samples
                memmove(pcm_buf, &pcm_buf[drift * channels], (n - drift) * channels * sizeof(int16_t));
                n -= drift;
            }
        }
        else {
            // ~1.4 s time constant at 1024-frame periods
            drift_avg += (drift - drift_avg) / 64.0;
            if (drift_avg < -AUDIO_SOFT_DRIFT) {
                // slow sound card clock: repeat the last sample frame
                memcpy(&pcm_buf[n * channels], &pcm_buf[(n - 1) * channels], channels * sizeof(int16_t));
                n++;
                drift_avg += 1;
                corr_up++;
            }
            else if (drift_avg > AUDIO_SOFT_DRIFT && n > 1) {
                // fast sound card clock: drop the last sample frame
                n--;
                drift_avg -= 1;
                corr_down++;
            }
        }

        audio_write(input, pcm_buf, (int) n, channels);
        written += n;

        if (t - last_stats_ns > 60 * 1000000000LL) {
            tvltx_debug("ALSA: input %d: drift %+.1f ms, %llu/%llu samples inserted/dropped\n",
                        input->input_id, drift_avg * 1000.0 / AUDIO_RATE,
                        (unsigned long long) corr_up, (unsigned long long) corr_down);
            last_stats_ns = t;
        }
    }

    if (corr_up || corr_down)
        tvltx_debug("ALSA: input %d: clock drift corrections: %llu samples inserted, %llu dropped\n",
                    input->input_id, (unsigned long long) corr_up, (unsigned long long) corr_down);
    if (pcm)
        snd_pcm_close(pcm);
    tvltx_debug("Exiting ALSA audio input thread.\n");
    return EXIT_SUCCESS;
}

/* ------------------------------------------------------------------------ */

int input_v4l2_start(input_config *input)
{
    pthread_t tid;

    if (input->input_id < 0 || input->input_id >= MAX_INPUT_INTERFACES)
        return EXIT_FAILURE;

    v4l2_input_ctx *ctx = &contexts[input->input_id];
    ctx->input = input;
    // common time base for audio and video of this input
    ctx->epoch_ns = now_ns();

    if (pthread_create(&tid, NULL, v4l2_video_thread, ctx))
        return EXIT_FAILURE;
    pthread_detach(tid);
    if (pthread_create(&tid, NULL, alsa_audio_thread, ctx))
        return EXIT_FAILURE;
    pthread_detach(tid);

    return EXIT_SUCCESS;
}
