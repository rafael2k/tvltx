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
 * Input synchronisation shared by the capture drivers (V4L2/ALSA, DeckLink).
 *
 * Master clock
 *   All inputs run on one timeline, the "master clock". It is
 *   CLOCK_MONOTONIC plus a slowly varying offset. When a physical output
 *   (the DekTec modulator/ASI card) is transmitting, it reports how much of
 *   the transport stream the hardware has actually sent out, and the offset
 *   tracks the difference between that and CLOCK_MONOTONIC, smoothed with a
 *   10 s time constant. The modulator is then the time master: inputs whose
 *   clocks are not locked to it (SDI not genlocked, USB grabbers, sound
 *   cards) are corrected so that they deliver exactly the nominal frame and
 *   sample rates *on the modulator's timeline*, and the multiplex neither
 *   underflows nor builds up latency. Without a physical output the master
 *   clock is just CLOCK_MONOTONIC.
 *
 * Video cadence
 *   Delivers exactly elapsed * fps frames: a captured frame is dropped when
 *   the source runs ahead by more than one frame, and repeated when it falls
 *   behind by more than one frame.
 *
 * Audio
 *   Delivers exactly elapsed * 48000 sample frames: silence is inserted (or
 *   samples dropped) to align at start-up and after overruns, and single
 *   sample frames are repeated/dropped (at most one per period, i.e. about
 *   1000 ppm) to follow clock drift, based on a smoothed drift estimate.
 *   Each audio stream receives its own channels according to its "mapping".
 */

#ifndef HAVE_SYNC_H__
#define HAVE_SYNC_H__

#include <stdint.h>
#include <stdbool.h>

#include "encoder_opts.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SYNC_AUDIO_RATE      48000
#define SYNC_MAX_CHANNELS    16

/* ---- master clock ------------------------------------------------------ */

int64_t sync_monotonic_ns(void);
/* current time on the master timeline */
int64_t sync_now_ns(void);
/* convert a CLOCK_MONOTONIC timestamp (e.g. a V4L2 buffer time) to master time */
int64_t sync_from_monotonic_ns(int64_t mono_ns);
/* Called by the physical output: at CLOCK_MONOTONIC time mono_ns the
 * hardware has transmitted master_elapsed_s seconds of transport stream
 * since it started sending. */
void sync_master_update(int64_t mono_ns, double master_elapsed_s);
/* is a physical output driving the master clock? */
bool sync_master_active(void);
/* master clock rate relative to CLOCK_MONOTONIC, in ppm (0 if inactive) */
double sync_master_ppm(void);

/* ---- input modes ------------------------------------------------------- */

typedef struct
{
    int width;
    int height;
    int fps_num;
    int fps_den;
    bool interlaced;
} sync_mode;

/* frame size/rate the encoder expects for an input video mode; -1 if unknown */
int sync_mode_lookup(int video_input_mode, sync_mode *m);

/* ---- video ------------------------------------------------------------- */

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
    int64_t last_stats_ns;
} sync_video;

void sync_video_init(sync_video *v, input_config *input, int64_t epoch_ns,
                     int fps_num, int fps_den, int frame_size);
/* a captured frame, t_ns = master time of capture */
void sync_video_deliver(sync_video *v, const uint8_t *frame, int64_t t_ns);
/* keep the cadence with a substitute frame (no signal / no device) */
void sync_video_fill(sync_video *v, const uint8_t *frame, int64_t t_ns);
/* log repeat/drop counters about once a minute */
void sync_video_stats(sync_video *v, const char *tag);

/* UYVY 4:2:2 helpers */
void sync_fill_black(uint8_t *frame, int width, int height);
void sync_fill_bars(uint8_t *frame, int width, int height);

/* ---- audio ------------------------------------------------------------- */

typedef struct
{
    input_config *input;
    int64_t epoch_ns;
    int64_t written;        /* sample frames delivered since the epoch */
    double drift_avg;       /* smoothed drift, in sample frames */
    bool aligned;
    uint64_t corr_up;
    uint64_t corr_down;
    int64_t last_stats_ns;
} sync_audio;

void sync_audio_init(sync_audio *a, input_config *input, int64_t epoch_ns);
/* Captured interleaved S16 audio: `frames` sample frames of `channels`
 * channels; `pcm` must have room for frames + 1 sample frames. t_ns is the
 * master time of the last sample. */
void sync_audio_deliver(sync_audio *a, int16_t *pcm, int frames, int channels, int64_t t_ns);
/* re-align on the next delivery (after an overrun / stream restart) */
void sync_audio_resync(sync_audio *a);
/* no audio source: wait for and deliver the next period of silence */
void sync_audio_silence_period(sync_audio *a, int frames);
/* no audio source: deliver silence up to master time t_ns (non-blocking) */
void sync_audio_fill(sync_audio *a, int64_t t_ns);
void sync_audio_stats(sync_audio *a, const char *tag);

/* fan-out: write audio to every audio stream fed by this input, honouring
 * each stream's channel count and "mapping" */
void sync_audio_write(input_config *input, const int16_t *pcm, int frames, int in_ch);

#ifdef __cplusplus
};
#endif

#endif /* HAVE_SYNC_H__ */
