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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "sync.h"
#include "macros.h"
#include "buffer.h"

extern service_config services[MAX_SERVICES];
extern bool running;
extern int nr_services;

/* Master clock: second-order (type 2) loop tracking the physical output's
 * offset *and* rate against CLOCK_MONOTONIC, so a constant clock drift is
 * followed with no steady-state error, while reporting jitter (FIFO load
 * granularity, scheduling) is smoothed. Critically damped, ~10 s. */
#define MASTER_TAU_S           10.0
#define MASTER_ZETA            1.0
/* larger steps mean the output restarted: re-base instead of slewing */
#define MASTER_REBASE_NS       1000000000.0

#define AUDIO_SOFT_DRIFT       (SYNC_AUDIO_RATE / 100)  /* 10 ms: 1-sample corrections */
#define AUDIO_HARD_DRIFT       (SYNC_AUDIO_RATE / 25)   /* 40 ms: re-align */
#define STATS_INTERVAL_NS      (60 * 1000000000LL)

/* ------------------------------------------------------------------------ */
/* Master clock                                                              */
/* ------------------------------------------------------------------------ */

static pthread_mutex_t master_lock = PTHREAD_MUTEX_INITIALIZER;
static bool master_active = false;
static double master_base_ns;      /* CLOCK_MONOTONIC time of master time 0 */
static double master_offset_ns;    /* filtered (master - monotonic), at master_last_ns */
static double master_rate;         /* d(offset)/dt, i.e. master rate error (ns/ns) */
static int64_t master_last_ns;

int64_t sync_monotonic_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t) ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

/* offset at monotonic time mono_ns, extrapolated with the estimated rate */
static double master_offset_at(int64_t mono_ns)
{
    pthread_mutex_lock(&master_lock);
    double o = master_offset_ns;
    if (master_active)
        o += master_rate * (double) (mono_ns - master_last_ns);
    pthread_mutex_unlock(&master_lock);
    return o;
}

int64_t sync_now_ns(void)
{
    int64_t m = sync_monotonic_ns();
    return m + (int64_t) master_offset_at(m);
}

int64_t sync_from_monotonic_ns(int64_t mono_ns)
{
    return mono_ns + (int64_t) master_offset_at(mono_ns);
}

void sync_master_update(int64_t mono_ns, double master_elapsed_s)
{
    double master_ns = master_elapsed_s * 1e9;

    pthread_mutex_lock(&master_lock);
    if (!master_active) {
        /* start without a step: master time = current timeline */
        master_base_ns = (double) mono_ns + master_offset_ns - master_ns;
        master_rate = 0;
        master_active = true;
        master_last_ns = mono_ns;
        pthread_mutex_unlock(&master_lock);
        tvltx_info("sync: physical output is now the master clock\n");
        return;
    }

    double dt = (double) (mono_ns - master_last_ns) / 1e9;   /* seconds */
    if (dt <= 0) {
        pthread_mutex_unlock(&master_lock);
        return;
    }
    if (dt > 1.0) dt = 1.0;  /* after a long gap, take a bounded step */

    double predicted = master_offset_ns + master_rate * (double) (mono_ns - master_last_ns);
    double raw = master_base_ns + master_ns - (double) mono_ns;
    if (raw - predicted > MASTER_REBASE_NS || predicted - raw > MASTER_REBASE_NS) {
        /* output restarted or stalled: continue from the current timeline */
        master_base_ns = (double) mono_ns + predicted - master_ns;
        raw = predicted;
        tvltx_info("sync: physical output timeline jumped, re-based master clock\n");
    }

    /* type-2 loop: proportional on the offset error, integral into the rate */
    double wn = 1.0 / MASTER_TAU_S;
    double err = raw - predicted;
    master_offset_ns = predicted + 2.0 * MASTER_ZETA * wn * dt * err;
    master_rate += wn * wn * dt * err / 1e9;   /* err in ns, rate in ns per ns */
    master_last_ns = mono_ns;
    pthread_mutex_unlock(&master_lock);
}

bool sync_master_active(void)
{
    pthread_mutex_lock(&master_lock);
    bool a = master_active;
    pthread_mutex_unlock(&master_lock);
    return a;
}

double sync_master_ppm(void)
{
    pthread_mutex_lock(&master_lock);
    double ppm = master_active ? master_rate * 1e6 : 0;
    pthread_mutex_unlock(&master_lock);
    return ppm;
}

/* ------------------------------------------------------------------------ */
/* Modes                                                                     */
/* ------------------------------------------------------------------------ */

int sync_mode_lookup(int video_input_mode, sync_mode *m)
{
    switch (video_input_mode)
    {
    case VIDEO_MODE_1080_P_60:   *m = (sync_mode){1920, 1080, 60,    1,    false}; break;
    case VIDEO_MODE_1080_P_5994: *m = (sync_mode){1920, 1080, 60000, 1001, false}; break;
    case VIDEO_MODE_1080_P_50:   *m = (sync_mode){1920, 1080, 50,    1,    false}; break;
    case VIDEO_MODE_1080_I_5994: *m = (sync_mode){1920, 1080, 30000, 1001, true};  break;
    case VIDEO_MODE_1080_I_60:   *m = (sync_mode){1920, 1080, 30,    1,    true};  break;
    case VIDEO_MODE_1080_I_50:   *m = (sync_mode){1920, 1080, 25,    1,    true};  break;
    case VIDEO_MODE_1080_P_30:   *m = (sync_mode){1920, 1080, 30,    1,    false}; break;
    case VIDEO_MODE_1080_P_2997: *m = (sync_mode){1920, 1080, 30000, 1001, false}; break;
    case VIDEO_MODE_1080_P_25:   *m = (sync_mode){1920, 1080, 25,    1,    false}; break;
    case VIDEO_MODE_720_P_60:    *m = (sync_mode){1280, 720,  60,    1,    false}; break;
    case VIDEO_MODE_720_P_5994:  *m = (sync_mode){1280, 720,  60000, 1001, false}; break;
    case VIDEO_MODE_720_P_50:    *m = (sync_mode){1280, 720,  50,    1,    false}; break;
    case VIDEO_MODE_720_P_30:    *m = (sync_mode){1280, 720,  30,    1,    false}; break;
    case VIDEO_MODE_720_P_2997:  *m = (sync_mode){1280, 720,  30000, 1001, false}; break;
    case VIDEO_MODE_720_P_25:    *m = (sync_mode){1280, 720,  25,    1,    false}; break;
    /* the encoder feeds x264 with 720x486 for 525-line modes */
    case VIDEO_MODE_480_I_5994:  *m = (sync_mode){720,  486,  30000, 1001, true};  break;
    case VIDEO_MODE_480_P_2997:  *m = (sync_mode){720,  486,  30000, 1001, false}; break;
    case VIDEO_MODE_576_I_50:    *m = (sync_mode){720,  576,  25,    1,    true};  break;
    case VIDEO_MODE_576_P_25:    *m = (sync_mode){720,  576,  25,    1,    false}; break;
    default:
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Video                                                                     */
/* ------------------------------------------------------------------------ */

void sync_video_init(sync_video *v, input_config *input, int64_t epoch_ns,
                     int fps_num, int fps_den, int frame_size)
{
    memset(v, 0, sizeof(*v));
    v->input = input;
    v->epoch_ns = epoch_ns;
    v->fps_num = fps_num;
    v->fps_den = fps_den;
    v->frame_size = frame_size;
    v->last_stats_ns = epoch_ns;
}

static void video_write(sync_video *v, const uint8_t *frame)
{
    for (int i = 0; i < nr_services; i++) {
        if (services[i].input_id == v->input->input_id)
            write_buffer(&services[i].buf_video_in, (uint8_t *) frame, v->frame_size);
    }
    v->frames_out++;
}

/* number of frames that should have been delivered at time t (frame 0 at epoch) */
static uint64_t video_expected(const sync_video *v, int64_t t)
{
    if (t <= v->epoch_ns)
        return 1;
    double elapsed = (double) (t - v->epoch_ns) / 1e9;
    return (uint64_t) (elapsed * v->fps_num / v->fps_den) + 1;
}

void sync_video_deliver(sync_video *v, const uint8_t *frame, int64_t t_ns)
{
    uint64_t expected = video_expected(v, t_ns);

    if (v->frames_out >= expected + 1) {
        v->frames_drop++;
        return;
    }

    video_write(v, frame);

    while (running && v->frames_out + 1 < expected) {
        video_write(v, frame);
        v->frames_dup++;
    }
}

void sync_video_fill(sync_video *v, const uint8_t *frame, int64_t t_ns)
{
    uint64_t expected = video_expected(v, t_ns);
    while (running && v->frames_out < expected)
        video_write(v, frame);
}

void sync_video_stats(sync_video *v, const char *tag)
{
    int64_t t = sync_now_ns();
    if (t - v->last_stats_ns < STATS_INTERVAL_NS)
        return;
    v->last_stats_ns = t;
    if (v->frames_dup || v->frames_drop)
        tvltx_debug("%s: input %d: %llu frames out, %llu repeated, %llu dropped%s\n", tag,
                    v->input->input_id, (unsigned long long) v->frames_out,
                    (unsigned long long) v->frames_dup, (unsigned long long) v->frames_drop,
                    sync_master_active() ? " (locked to physical output)" : "");
}

void sync_fill_black(uint8_t *frame, int width, int height)
{
    uint32_t *p = (uint32_t *) frame;
    for (int i = 0; i < width * height / 2; i++)
        *p++ = 0x10801080; /* U=0x80 Y=0x10 V=0x80 Y=0x10 (little endian) */
}

void sync_fill_bars(uint8_t *frame, int width, int height)
{
    /* 75% color bars, as the DeckLink input always used on "no input source" */
    static const uint32_t bars[8] = {
        0xEA80EA80, 0xD292D210, 0xA910A9A5, 0x90229035,
        0x6ADD6ACA, 0x51EF515A, 0x286D28EF, 0x10801080 };
    uint32_t *p = (uint32_t *) frame;
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x += 2)
            *p++ = bars[(x * 8) / width];
}

/* ------------------------------------------------------------------------ */
/* Audio                                                                     */
/* ------------------------------------------------------------------------ */

void sync_audio_write(input_config *input, const int16_t *pcm, int frames, int in_ch)
{
    for (int i = 0; i < nr_services; i++) {
        if (services[i].input_id != input->input_id)
            continue;

        for (int j = 0; j < services[i].nr_audio_streams; j++) {
            int nch = services[i].nr_channels[j];
            int map[SYNC_MAX_CHANNELS];
            int count = 0;
            unsigned int mask = services[i].audio_mapping[j];

            if (nch <= 0)
                continue;
            if (nch > SYNC_MAX_CHANNELS)
                nch = SYNC_MAX_CHANNELS;

            /* "mapping" is a bitmask of input channels, taken in ascending order */
            for (int c = 0; c < 32 && count < nch; c++)
                if (mask & (1u << c))
                    map[count++] = c;
            if (count == 0)
                for (; count < nch; count++)
                    map[count] = count;
            for (; count < nch; count++)
                map[count] = -1;

            int16_t *out = malloc((size_t) frames * nch * sizeof(int16_t));
            if (!out)
                continue;
            for (int f = 0; f < frames; f++)
                for (int k = 0; k < nch; k++)
                    out[f * nch + k] = (map[k] >= 0 && map[k] < in_ch) ? pcm[f * in_ch + map[k]] : 0;

            write_buffer(&services[i].buf_audio_in[j], (uint8_t *) out, frames * nch * 2);
            free(out);
        }
    }
}

static void audio_silence(sync_audio *a, int64_t frames)
{
    static const int16_t zeros[1024 * SYNC_MAX_CHANNELS];
    while (frames > 0 && running) {
        int n = frames > 1024 ? 1024 : (int) frames;
        sync_audio_write(a->input, zeros, n, SYNC_MAX_CHANNELS);
        frames -= n;
    }
}

void sync_audio_init(sync_audio *a, input_config *input, int64_t epoch_ns)
{
    memset(a, 0, sizeof(*a));
    a->input = input;
    a->epoch_ns = epoch_ns;
    a->last_stats_ns = epoch_ns;
}

void sync_audio_resync(sync_audio *a)
{
    a->aligned = false;
}

void sync_audio_deliver(sync_audio *a, int16_t *pcm, int n, int channels, int64_t t_ns)
{
    if (n <= 0)
        return;

    int64_t expected = (int64_t) ((double) (t_ns - a->epoch_ns) * SYNC_AUDIO_RATE / 1e9);
    int64_t drift = a->written + n - expected; /* > 0: ahead, < 0: behind */

    if (!a->aligned || drift < -AUDIO_HARD_DRIFT || drift > AUDIO_HARD_DRIFT) {
        a->aligned = true;
        a->drift_avg = 0;
        if (drift < 0) {
            /* start-up, overrun or a gap in the source: catch up with silence */
            audio_silence(a, -drift);
            a->written += -drift;
        } else if (drift >= n) {
            /* far ahead: drop this block */
            return;
        } else {
            /* ahead by less than a block: drop its first samples */
            memmove(pcm, &pcm[drift * channels], (size_t) (n - drift) * channels * sizeof(int16_t));
            n -= (int) drift;
        }
    } else {
        /* ~1.4 s time constant at 1024-frame blocks */
        a->drift_avg += (drift - a->drift_avg) / 64.0;
        if (a->drift_avg < -AUDIO_SOFT_DRIFT) {
            /* source clock slow against the master: repeat the last sample frame */
            memcpy(&pcm[n * channels], &pcm[(n - 1) * channels], channels * sizeof(int16_t));
            n++;
            a->drift_avg += 1;
            a->corr_up++;
        } else if (a->drift_avg > AUDIO_SOFT_DRIFT && n > 1) {
            /* source clock fast: drop the last sample frame */
            n--;
            a->drift_avg -= 1;
            a->corr_down++;
        }
    }

    sync_audio_write(a->input, pcm, n, channels);
    a->written += n;
}

void sync_audio_silence_period(sync_audio *a, int frames)
{
    /* wait until the period is due on the master timeline */
    int64_t due = a->epoch_ns + (int64_t) ((double) (a->written + frames) * 1e9 / SYNC_AUDIO_RATE);
    int64_t mono = sync_monotonic_ns() + (due - sync_now_ns());
    struct timespec ts = { mono / 1000000000LL, mono % 1000000000LL };
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
    audio_silence(a, frames);
    a->written += frames;
}

void sync_audio_fill(sync_audio *a, int64_t t_ns)
{
    int64_t expected = (int64_t) ((double) (t_ns - a->epoch_ns) * SYNC_AUDIO_RATE / 1e9);
    if (expected > a->written) {
        audio_silence(a, expected - a->written);
        a->written = expected;
    }
    /* realign smoothly once real audio comes back */
    a->aligned = false;
}

void sync_audio_stats(sync_audio *a, const char *tag)
{
    int64_t t = sync_now_ns();
    if (t - a->last_stats_ns < STATS_INTERVAL_NS)
        return;
    a->last_stats_ns = t;
    tvltx_debug("%s: input %d: drift %+.1f ms, %llu/%llu samples inserted/dropped%s\n", tag,
                a->input->input_id, a->drift_avg * 1000.0 / SYNC_AUDIO_RATE,
                (unsigned long long) a->corr_up, (unsigned long long) a->corr_down,
                sync_master_active() ? " (locked to physical output)" : "");
}
