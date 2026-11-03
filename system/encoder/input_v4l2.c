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
 * The video device and the sound card normally run from different, free-
 * running clocks, so both streams are slaved to the master clock (see sync.h:
 * the modulator when one is transmitting, CLOCK_MONOTONIC otherwise) from a
 * common per-input epoch: video frames are repeated/dropped and audio
 * samples inserted/dropped so that each stream always delivers exactly
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
#include "sync.h"

extern service_config services[MAX_SERVICES];
extern bool running;
extern int nr_services;

#define V4L2_NR_BUFFERS       8
#define NO_SIGNAL_TIMEOUT_NS  500000000LL   // 0.5 s without frames -> color bars

#define AUDIO_PERIOD          1024          // frames per ALSA read

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
    sync_mode sm;
    memset(m, 0, sizeof(*m));
    if (sync_mode_lookup(video_input_mode, &sm))
        return -1;
    m->width = sm.width;
    m->height = sm.height;
    m->fps_num = sm.fps_num;
    m->fps_den = sm.fps_den;
    m->interlaced = sm.interlaced;
    // analog standard family for SD modes; digital (HDMI/SDI) sources have none
    if (sm.height == 486)
        m->std = V4L2_STD_525_60;
    else if (sm.height == 576)
        m->std = V4L2_STD_625_50;
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
    sync_video vc;

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

    sync_video_init(&vc, input, ctx->epoch_ns, m.fps_num, m.fps_den, m.width * m.height * 2);

    frame = malloc(vc.frame_size);
    bars = malloc(vc.frame_size);
    if (!frame || !bars)
        goto bail;
    sync_fill_black(frame, m.width, m.height);
    sync_fill_bars(bars, m.width, m.height);

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

    int64_t last_frame_ns = sync_now_ns();
    int poll_ms = (int) (2000LL * m.fps_den / m.fps_num) + 1; // ~2 frame periods

    while (running) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        int r = poll(&pfd, 1, poll_ms);
        int64_t t = sync_now_ns();

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
                ts = sync_from_monotonic_ns((int64_t) buf.timestamp.tv_sec * 1000000000LL +
                                            buf.timestamp.tv_usec * 1000LL);

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
            sync_video_deliver(&vc, frame, ts);
        }
        else if (t - last_frame_ns > NO_SIGNAL_TIMEOUT_NS) {
            if (!no_signal) {
                tvltx_info("V4L2: input %d: no video signal, sending color bars\n", input->input_id);
                no_signal = true;
            }
            sync_video_fill(&vc, bars, t);
        }

        sync_video_stats(&vc, "V4L2");
    }
    goto bail;

no_device:
    // keep the encoder fed so the rest of the multiplex stays on air
    tvltx_info("V4L2: input %d: running without a video device, sending color bars\n", input->input_id);
    while (running) {
        sync_video_fill(&vc, bars, sync_now_ns());
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
        if (tries[i] <= 0 || tries[i] > SYNC_MAX_CHANNELS || (i > 0 && tries[i] >= *channels))
            continue;
        // soft resampling allowed, so "plughw:" / "default" devices can adapt
        err = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                                 tries[i], SYNC_AUDIO_RATE, 1, 200000);
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
    snd_pcm_t *pcm = NULL;
    sync_audio sa;

    sync_audio_init(&sa, input, ctx->epoch_ns);

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

    // one spare frame so the corrector can repeat the last sample frame
    int16_t pcm_buf[(AUDIO_PERIOD + 1) * SYNC_MAX_CHANNELS];

    while (running) {
        if (!pcm) {
            sync_audio_silence_period(&sa, AUDIO_PERIOD);
            continue;
        }

        snd_pcm_sframes_t n = snd_pcm_readi(pcm, pcm_buf, AUDIO_PERIOD);
        if (n < 0) {
            if (n == -EPIPE)
                tvltx_debug("ALSA: input %d: overrun\n", input->input_id);
            sync_audio_resync(&sa);
            if (snd_pcm_recover(pcm, (int) n, 1) < 0) {
                tvltx_info("ALSA: read error: %s\n", snd_strerror((int) n));
                struct timespec ts = { 0, 100000000 };
                nanosleep(&ts, NULL);
            }
            continue;
        }
        if (n == 0)
            continue;

        // master time of the last sample read: now, minus what is still queued
        snd_pcm_sframes_t delay = 0;
        if (snd_pcm_delay(pcm, &delay) < 0 || delay < 0)
            delay = 0;
        int64_t t = sync_now_ns() - (int64_t) delay * 1000000000LL / SYNC_AUDIO_RATE;

        sync_audio_deliver(&sa, pcm_buf, (int) n, channels, t);
        sync_audio_stats(&sa, "ALSA");
    }

    if (sa.corr_up || sa.corr_down)
        tvltx_debug("ALSA: input %d: clock drift corrections: %llu samples inserted, %llu dropped\n",
                    input->input_id, (unsigned long long) sa.corr_up, (unsigned long long) sa.corr_down);
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
    ctx->epoch_ns = sync_now_ns();

    if (pthread_create(&tid, NULL, v4l2_video_thread, ctx))
        return EXIT_FAILURE;
    pthread_detach(tid);
    if (pthread_create(&tid, NULL, alsa_audio_thread, ctx))
        return EXIT_FAILURE;
    pthread_detach(tid);

    return EXIT_SUCCESS;
}
