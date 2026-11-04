/*
    TVLTx - TV Lovers Encoder
    Copyright (C) 2016-2026 SDR Telecom

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
 * Blackmagic DeckLink input, DeckLink SDK 16.0 API.
 *
 * libDeckLinkAPI.so (Blackmagic Desktop Video) is loaded at run time through
 * DeckLinkAPIDispatch.cpp, so TVLTx builds without the SDK library; without
 * the driver the input reports it and sends color bars and silence.
 *
 * Frames are captured as 8-bit YUV 4:2:2 (UYVY, what the encoder feeds to
 * x264), or as 8-bit BGRA converted to UYVY when the source is RGB 4:4:4.
 * Video and audio are delivered through the shared synchronisation (sync.h):
 * an SDI source that is not genlocked to the modulator has its frames
 * repeated/dropped and its audio samples inserted/dropped to follow the
 * master clock (the modulator in production), so the multiplex stays stable.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Capture.h"

extern "C" {
#include "../macros.h"
#include "../input.h"
}

extern "C" {
    extern bool running;
}

#define NO_SIGNAL_TIMEOUT_NS  500000000LL   /* 0.5 s without frames/audio */
#define WATCHDOG_US           100000

/* ------------------------------------------------------------------------ */

struct bmd_mode_map {
    int tvltx_mode;
    BMDDisplayMode bmd_mode;
};

/* 720p25/29.97/30 are not SDI/HDMI capture formats, so they are absent */
static const bmd_mode_map mode_table[] = {
    { VIDEO_MODE_480_I_5994,  bmdModeNTSC },
    { VIDEO_MODE_480_P_2997,  bmdModeNTSCp },
    { VIDEO_MODE_576_I_50,    bmdModePAL },
    { VIDEO_MODE_576_P_25,    bmdModePALp },
    { VIDEO_MODE_1080_P_25,   bmdModeHD1080p25 },
    { VIDEO_MODE_1080_P_2997, bmdModeHD1080p2997 },
    { VIDEO_MODE_1080_P_30,   bmdModeHD1080p30 },
    { VIDEO_MODE_1080_P_50,   bmdModeHD1080p50 },
    { VIDEO_MODE_1080_P_5994, bmdModeHD1080p5994 },
    { VIDEO_MODE_1080_P_60,   bmdModeHD1080p6000 },
    { VIDEO_MODE_1080_I_50,   bmdModeHD1080i50 },
    { VIDEO_MODE_1080_I_5994, bmdModeHD1080i5994 },
    { VIDEO_MODE_1080_I_60,   bmdModeHD1080i6000 },
    { VIDEO_MODE_720_P_50,    bmdModeHD720p50 },
    { VIDEO_MODE_720_P_5994,  bmdModeHD720p5994 },
    { VIDEO_MODE_720_P_60,    bmdModeHD720p60 },
};

static BMDDisplayMode bmd_mode_for(int tvltx_mode)
{
    for (size_t i = 0; i < sizeof(mode_table) / sizeof(mode_table[0]); i++)
        if (mode_table[i].tvltx_mode == tvltx_mode)
            return mode_table[i].bmd_mode;
    return 0;
}

static BMDVideoConnection bmd_connection_for(int input_type)
{
    switch (input_type) {
    case INPUT_TYPE_SDI:        return bmdVideoConnectionSDI;
    case INPUT_TYPE_HDMI:       return bmdVideoConnectionHDMI;
    case INPUT_TYPE_OPTICALSDI: return bmdVideoConnectionOpticalSDI;
    case INPUT_TYPE_COMPONENT:  return bmdVideoConnectionComponent;
    case INPUT_TYPE_COMPOSITE:  return bmdVideoConnectionComposite;
    case INPUT_TYPE_SVIDEO:     return bmdVideoConnectionSVideo;
    default:                    return bmdVideoConnectionUnspecified;
    }
}

static bool is_analog(BMDVideoConnection c)
{
    return c == bmdVideoConnectionComponent || c == bmdVideoConnectionComposite ||
           c == bmdVideoConnectionSVideo;
}

/* 8-bit BGRA (full range) -> UYVY (limited range), BT.709 for HD, BT.601 for SD */
static void bgra_to_uyvy(const uint8_t *src, long src_stride, uint8_t *dst, int w, int h, bool hd)
{
    /* coefficients scaled by 2^16, including the 219/255 and 224/255 range */
    const int yr = hd ? 11966 : 16829, yg = hd ? 40254 : 33039, yb = hd ? 4064 : 6416;
    const int ur = hd ? -6596 : -9714, ug = hd ? -22189 : -19070, ub = 28784;
    const int vr = 28784, vg = hd ? -26145 : -24103, vb = hd ? -2639 : -4681;

    for (int y = 0; y < h; y++) {
        const uint8_t *s = src + (size_t) y * src_stride;
        uint8_t *d = dst + (size_t) y * w * 2;
        for (int x = 0; x < w; x += 2, s += 8, d += 4) {
            int b0 = s[0], g0 = s[1], r0 = s[2];
            int b1 = s[4], g1 = s[5], r1 = s[6];
            int r = (r0 + r1) >> 1, g = (g0 + g1) >> 1, b = (b0 + b1) >> 1;
            d[0] = (uint8_t) (((ur * r + ug * g + ub * b) >> 16) + 128);
            d[1] = (uint8_t) (((yr * r0 + yg * g0 + yb * b0) >> 16) + 16);
            d[2] = (uint8_t) (((vr * r + vg * g + vb * b) >> 16) + 128);
            d[3] = (uint8_t) (((yr * r1 + yg * g1 + yb * b1) >> 16) + 16);
        }
    }
}

static const char *connection_name(BMDVideoConnection c)
{
    switch (c) {
    case bmdVideoConnectionSDI:        return "SDI";
    case bmdVideoConnectionHDMI:       return "HDMI";
    case bmdVideoConnectionOpticalSDI: return "optical SDI";
    case bmdVideoConnectionComponent:  return "component";
    case bmdVideoConnectionComposite:  return "composite";
    case bmdVideoConnectionSVideo:     return "S-Video";
    default:                           return "default";
    }
}

/* ------------------------------------------------------------------------ */

DeckLinkInput::DeckLinkInput(input_config *in)
    : input(in), ref_count(1), device(NULL), dl_input(NULL), config(NULL), attributes(NULL),
      display_mode(0), pixel_format(bmdFormat8BitYUV), connection(bmdVideoConnectionUnspecified),
      format_detection(false), channels(2), epoch_ns(0), frame(NULL), bars(NULL), pcm(NULL),
      pcm_frames(0), last_video_ns(0), last_audio_ns(0), no_signal(false), mode_mismatch(false)
{
    memset(&mode, 0, sizeof(mode));
    pthread_mutex_init(&lock, NULL);
}

DeckLinkInput::~DeckLinkInput()
{
    free(frame);
    free(bars);
    free(pcm);
    pthread_mutex_destroy(&lock);
}

HRESULT DeckLinkInput::QueryInterface(REFIID iid, LPVOID *ppv)
{
    REFIID unknown = IID_IUnknown;
    if (memcmp(&iid, &unknown, sizeof(REFIID)) == 0 ||
        memcmp(&iid, &IID_IDeckLinkInputCallback, sizeof(REFIID)) == 0) {
        *ppv = static_cast<IDeckLinkInputCallback *>(this);
        AddRef();
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

ULONG DeckLinkInput::AddRef(void)
{
    return __sync_add_and_fetch(&ref_count, 1);
}

ULONG DeckLinkInput::Release(void)
{
    int32_t r = __sync_sub_and_fetch(&ref_count, 1);
    if (r == 0)
        delete this;
    return r;
}

/* ------------------------------------------------------------------------ */

bool DeckLinkInput::open_device()
{
    IDeckLinkIterator *it = CreateDeckLinkIteratorInstance();
    if (!it) {
        tvltx_info("DeckLink: input %d: the DeckLink driver (Blackmagic Desktop Video) is not installed\n",
                   input->input_id);
        return false;
    }

    IDeckLink *d = NULL;
    int idx = input->card_number;
    while (it->Next(&d) == S_OK) {
        if (idx-- == 0)
            break;
        d->Release();
        d = NULL;
    }
    it->Release();
    if (!d) {
        tvltx_info("DeckLink: input %d: no DeckLink card number %d\n", input->input_id, input->card_number);
        return false;
    }
    device = d;

    const char *name = NULL;
    if (device->GetDisplayName(&name) == S_OK && name) {
        tvltx_info("DeckLink: input %d: card %d: %s\n", input->input_id, input->card_number, name);
        free((void *) name);
    }

    if (device->QueryInterface(IID_IDeckLinkInput, (void **) &dl_input) != S_OK) {
        tvltx_info("DeckLink: input %d: card has no capture input\n", input->input_id);
        return false;
    }
    if (device->QueryInterface(IID_IDeckLinkConfiguration, (void **) &config) != S_OK)
        config = NULL;
    if (device->QueryInterface(IID_IDeckLinkProfileAttributes, (void **) &attributes) != S_OK)
        attributes = NULL;
    return true;
}

void DeckLinkInput::close_device()
{
    if (dl_input) {
        dl_input->SetCallback(NULL);
        dl_input->Release();
        dl_input = NULL;
    }
    if (config) { config->Release(); config = NULL; }
    if (attributes) { attributes->Release(); attributes = NULL; }
    if (device) { device->Release(); device = NULL; }
}

bool DeckLinkInput::start_capture()
{
    HRESULT r;

    connection = bmd_connection_for(input->input_type);
    if (connection != bmdVideoConnectionUnspecified && config) {
        int64_t available = 0;
        if (attributes && attributes->GetInt(BMDDeckLinkVideoInputConnections, &available) == S_OK &&
            !(available & connection))
            tvltx_info("DeckLink: input %d: card has no %s input\n", input->input_id, connection_name(connection));
        if (config->SetInt(bmdDeckLinkConfigVideoInputConnection, connection) != S_OK)
            tvltx_info("DeckLink: input %d: could not select the %s input\n", input->input_id,
                       connection_name(connection));
    }
    if (config) {
        BMDAudioConnection ac = is_analog(connection) ? bmdAudioConnectionAnalog : bmdAudioConnectionEmbedded;
        if (config->SetInt(bmdDeckLinkConfigAudioInputConnection, ac) != S_OK)
            tvltx_info("DeckLink: input %d: could not select %s audio\n", input->input_id,
                       ac == bmdAudioConnectionAnalog ? "analog" : "embedded");
    }

    bool supported = false;
    BMDDisplayMode actual = 0;
    r = dl_input->DoesSupportVideoMode(connection, display_mode, bmdFormat8BitYUV, bmdNoVideoInputConversion,
                                       bmdSupportedVideoModeDefault, &actual, &supported);
    if (r != S_OK || !supported) {
        tvltx_info("DeckLink: input %d: card does not capture the configured mode on %s\n", input->input_id,
                   connection_name(connection));
        return false;
    }

    IDeckLinkDisplayMode *dm = NULL;
    if (dl_input->GetDisplayMode(display_mode, &dm) == S_OK && dm) {
        BMDTimeValue duration = 0;
        BMDTimeScale scale = 0;
        const char *name = NULL;
        dm->GetFrameRate(&duration, &scale);
        dm->GetName(&name);
        tvltx_info("DeckLink: input %d: capturing %s (%ldx%ld) on %s\n", input->input_id, name ? name : "?",
                   dm->GetWidth(), dm->GetHeight(), connection_name(connection));
        if (duration > 0 && (int64_t) scale * mode.fps_den != (int64_t) duration * mode.fps_num)
            tvltx_info("DeckLink: input %d: card runs at %lld/%lld fps, encoder at %d/%d fps: "
                       "frames will be dropped/repeated to match\n", input->input_id,
                       (long long) scale, (long long) duration, mode.fps_num, mode.fps_den);
        free((void *) name);
        dm->Release();
    }

    format_detection = false;
    if (attributes)
        attributes->GetFlag(BMDDeckLinkSupportsInputFormatDetection, &format_detection);

    pixel_format = bmdFormat8BitYUV;
    dl_input->SetCallback(this);

    r = dl_input->EnableVideoInput(display_mode, pixel_format,
                                   format_detection ? bmdVideoInputEnableFormatDetection : bmdVideoInputFlagDefault);
    if (r != S_OK) {
        tvltx_info("DeckLink: input %d: cannot enable video input%s\n", input->input_id,
                   r == E_ACCESSDENIED ? " (card busy: is another application using it?)" : "");
        return false;
    }

    if (dl_input->EnableAudioInput(bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, channels) != S_OK)
        tvltx_info("DeckLink: input %d: cannot capture %d audio channels, sending silence\n",
                   input->input_id, channels);

    last_video_ns = last_audio_ns = sync_now_ns();
    if (dl_input->StartStreams() != S_OK) {
        tvltx_info("DeckLink: input %d: cannot start capture\n", input->input_id);
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------------ */

void DeckLinkInput::deliver_video(IDeckLinkVideoInputFrame *video, int64_t t)
{
    last_video_ns = t;

    bool usable = !(video->GetFlags() & bmdFrameHasNoInputSource) && !mode_mismatch;
    long w = video->GetWidth(), h = video->GetHeight(), stride = video->GetRowBytes();

    if (usable && (w != mode.width || h != mode.height)) {
        if (!no_signal)
            tvltx_info("DeckLink: input %d: card delivers %ldx%ld, encoder expects %dx%d\n",
                       input->input_id, w, h, mode.width, mode.height);
        usable = false;
    }

    /* SDK 14.3+: frame bytes are reached through IDeckLinkVideoBuffer, and
     * only between StartAccess() and EndAccess(). Copy into our own frame and
     * release the card's buffer before delivering (which may block). */
    if (usable) {
        IDeckLinkVideoBuffer *vbuf = NULL;
        void *bytes = NULL;
        usable = false;
        if (video->QueryInterface(IID_IDeckLinkVideoBuffer, (void **) &vbuf) == S_OK && vbuf) {
            if (vbuf->StartAccess(bmdBufferAccessRead) == S_OK) {
                if (vbuf->GetBytes(&bytes) == S_OK && bytes) {
                    if (video->GetPixelFormat() == bmdFormat8BitBGRA) {
                        bgra_to_uyvy((const uint8_t *) bytes, stride, frame, mode.width, mode.height,
                                     mode.height >= 720);
                    } else if (stride == (long) mode.width * 2) {
                        memcpy(frame, bytes, (size_t) mode.width * mode.height * 2);
                    } else {
                        for (int y = 0; y < mode.height; y++)
                            memcpy(frame + (size_t) y * mode.width * 2,
                                   (const uint8_t *) bytes + (size_t) y * stride, (size_t) mode.width * 2);
                    }
                    usable = true;
                }
                vbuf->EndAccess(bmdBufferAccessRead);
            }
            vbuf->Release();
        }
    }

    if (!usable) {
        if (!no_signal)
            tvltx_info("DeckLink: input %d: no usable input signal, sending color bars\n", input->input_id);
        no_signal = true;
        sync_video_deliver(&sv, bars, t);
        return;
    }
    if (no_signal) {
        tvltx_info("DeckLink: input %d: input signal back\n", input->input_id);
        no_signal = false;
    }
    sync_video_deliver(&sv, frame, t);
}

void DeckLinkInput::deliver_audio(IDeckLinkAudioInputPacket *audio, int64_t t)
{
    long n = audio->GetSampleFrameCount();
    void *bytes = NULL;
    if (n <= 0 || audio->GetBytes(&bytes) != S_OK || !bytes)
        return;
    last_audio_ns = t;
    if ((size_t) n + 1 > pcm_frames)
        n = (long) pcm_frames - 1;
    memcpy(pcm, bytes, (size_t) n * channels * sizeof(int16_t));
    /* the packet arrives with its frame: its last sample is (about) now */
    sync_audio_deliver(&sa, pcm, (int) n, channels, t);
}

HRESULT DeckLinkInput::VideoInputFrameArrived(IDeckLinkVideoInputFrame *video, IDeckLinkAudioInputPacket *audio)
{
    int64_t t = sync_now_ns();
    pthread_mutex_lock(&lock);
    if (video)
        deliver_video(video, t);
    if (audio)
        deliver_audio(audio, t);
    pthread_mutex_unlock(&lock);
    return S_OK;
}

HRESULT DeckLinkInput::VideoInputFormatChanged(BMDVideoInputFormatChangedEvents events, IDeckLinkDisplayMode *new_mode,
                                               BMDDetectedVideoInputFormatFlags flags)
{
    const char *name = NULL;
    BMDDisplayMode detected = new_mode->GetDisplayMode();
    new_mode->GetName(&name);

    pthread_mutex_lock(&lock);

    if (detected != display_mode) {
        /* the encoder runs at a fixed size and rate: keep the configured mode */
        if (!mode_mismatch)
            tvltx_info("DeckLink: input %d: input signal is %s, not the configured mode: "
                       "sending color bars until it matches\n", input->input_id, name ? name : "?");
        mode_mismatch = true;
    } else {
        if (mode_mismatch)
            tvltx_info("DeckLink: input %d: input signal matches the configured mode again\n", input->input_id);
        mode_mismatch = false;

        /* RGB 4:4:4 sources are captured as BGRA and converted, YUV ones as UYVY */
        BMDPixelFormat wanted = (flags & bmdDetectedVideoInputRGB444) ? bmdFormat8BitBGRA : bmdFormat8BitYUV;
        if (wanted != pixel_format || (events & bmdVideoInputFieldDominanceChanged)) {
            pixel_format = wanted;
            dl_input->PauseStreams();
            if (dl_input->EnableVideoInput(display_mode, pixel_format, bmdVideoInputEnableFormatDetection) != S_OK)
                tvltx_info("DeckLink: input %d: failed to re-enable video input\n", input->input_id);
            dl_input->FlushStreams();
            dl_input->StartStreams();
            tvltx_info("DeckLink: input %d: input is %s %s, capturing 8-bit %s\n", input->input_id,
                       name ? name : "?", (flags & bmdDetectedVideoInputRGB444) ? "RGB" : "YUV",
                       pixel_format == bmdFormat8BitBGRA ? "BGRA" : "YUV 4:2:2");
        }
    }

    pthread_mutex_unlock(&lock);
    free((void *) name);
    return S_OK;
}

/* ------------------------------------------------------------------------ */

void DeckLinkInput::watchdog()
{
    int64_t t = sync_now_ns();
    pthread_mutex_lock(&lock);
    if (t - last_video_ns > NO_SIGNAL_TIMEOUT_NS) {
        if (!no_signal)
            tvltx_info("DeckLink: input %d: no frames from the card, sending color bars\n", input->input_id);
        no_signal = true;
        sync_video_fill(&sv, bars, t);
    }
    if (t - last_audio_ns > NO_SIGNAL_TIMEOUT_NS)
        sync_audio_fill(&sa, t);
    sync_video_stats(&sv, "DeckLink");
    sync_audio_stats(&sa, "DeckLink");
    pthread_mutex_unlock(&lock);
}

void DeckLinkInput::no_device_loop()
{
    /* keep the encoder fed so the rest of the multiplex stays on air */
    tvltx_info("DeckLink: input %d: running without a capture card, sending color bars and silence\n",
               input->input_id);
    while (running) {
        int64_t t = sync_now_ns();
        sync_video_fill(&sv, bars, t);
        sync_audio_fill(&sa, t);
        usleep(20000);
    }
}

void DeckLinkInput::run()
{
    if (sync_mode_lookup(input->video_input_mode, &mode)) {
        tvltx_info("DeckLink: input %d: unknown video mode %d\n", input->input_id, input->video_input_mode);
        return;
    }

    size_t frame_size = (size_t) mode.width * mode.height * 2;
    channels = input->audio_input_channel_nr > 0 ? input->audio_input_channel_nr : 2;
    if (channels > SYNC_MAX_CHANNELS)
        channels = SYNC_MAX_CHANNELS;
    pcm_frames = 8192 + 1;   /* > one frame of audio at any rate, plus one for corrections */
    frame = (uint8_t *) malloc(frame_size);
    bars = (uint8_t *) malloc(frame_size);
    pcm = (int16_t *) calloc(pcm_frames * channels, sizeof(int16_t));
    if (!frame || !bars || !pcm) {
        tvltx_info("DeckLink: input %d: out of memory\n", input->input_id);
        return;
    }
    sync_fill_black(frame, mode.width, mode.height);
    sync_fill_bars(bars, mode.width, mode.height);

    epoch_ns = sync_now_ns();
    sync_video_init(&sv, input, epoch_ns, mode.fps_num, mode.fps_den, (int) frame_size);
    sync_audio_init(&sa, input, epoch_ns);

    display_mode = bmd_mode_for(input->video_input_mode);
    if (!display_mode) {
        tvltx_info("DeckLink: input %d: the configured mode is not a DeckLink capture format "
                   "(720p is only 50/59.94/60)\n", input->input_id);
        no_device_loop();
        return;
    }

    if (!open_device() || !start_capture()) {
        close_device();
        no_device_loop();
        return;
    }

    while (running) {
        usleep(WATCHDOG_US);
        watchdog();
    }

    dl_input->StopStreams();
    dl_input->DisableAudioInput();
    dl_input->DisableVideoInput();
    close_device();
    tvltx_debug("Exiting DeckLink input thread.\n");
}

/* ------------------------------------------------------------------------ */

extern "C" void *input_bmd_thread(void *input_v)
{
    DeckLinkInput *capture = new DeckLinkInput((input_config *) input_v);
    capture->run();
    capture->Release();
    return NULL;
}
