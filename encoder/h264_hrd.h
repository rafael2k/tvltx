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
 * H.264 HRD timing from an Annex B stream.
 *
 * Recovers each access unit's CPB removal time (decode time) and DPB output
 * time (presentation time) from the stream itself, as signalled by an
 * encoder run with NAL HRD parameters (x264 --nal-hrd): the SPS VUI timing
 * and HRD fields, the buffering period SEI (initial_cpb_removal_delay) and
 * the picture timing SEI (cpb_removal_delay, dpb_output_delay), following
 * ITU-T H.264 Annex C. The arithmetic matches x264's ratecontrol.c, so the
 * results equal x264's own hrd_timing values.
 */

#ifndef HAVE_H264_HRD_H__
#define HAVE_H264_HRD_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    /* from the SPS */
    bool have_sps;
    uint32_t num_units_in_tick;
    uint32_t time_scale;
    bool nal_hrd;
    bool vcl_hrd;
    bool pic_struct_present;
    int cpb_cnt;
    int initial_cpb_removal_delay_length;
    int cpb_removal_delay_length;
    int dpb_output_delay_length;

    /* HRD state */
    bool have_anchor;
    double nominal_removal_time;   /* of the last buffering-period AU, seconds */

    /* current access unit */
    bool au_buffering_period;
    uint32_t au_initial_cpb_removal_delay;
    bool au_pic_timing;
    uint32_t au_cpb_removal_delay;
    uint32_t au_dpb_output_delay;
    bool au_idr;
} h264_hrd;

void h264_hrd_init(h264_hrd *h);

/* Feed one NAL unit of the current access unit (without start code). */
void h264_hrd_nal(h264_hrd *h, const uint8_t *nal, size_t len);

/* Close the current access unit: its CPB removal and DPB output times in
 * seconds, and whether it is a random access point (IDR or buffering
 * period). Returns 0, or -1 if the timing is not known (no SPS/HRD yet, no
 * picture timing SEI). Resets the per-AU state either way. */
int h264_hrd_au_end(h264_hrd *h, double *cpb_removal_time, double *dpb_output_time, bool *keyframe);

/* ---- access unit reader ------------------------------------------------
 * Splits an Annex B byte stream into access units at access unit delimiters
 * (x264 --aud) and times each one with h264_hrd. */

typedef void (*h264_au_cb)(void *user, const uint8_t *au, size_t len, int timing_ok,
                           double cpb_removal_time, double dpb_output_time, bool keyframe);

typedef struct
{
    uint8_t *buf;
    size_t len;
    size_t cap;
    size_t au_start;    /* start of the current AU in buf, or SIZE_MAX */
    size_t nal_start;   /* first byte after the current NAL's start code, or SIZE_MAX */
    size_t scan;        /* where to continue looking for start codes */
    h264_hrd hrd;
} h264_au_reader;

/* max_au: largest access unit accepted, in bytes */
int h264_au_reader_init(h264_au_reader *r, size_t max_au);
void h264_au_reader_free(h264_au_reader *r);
/* Append stream bytes; cb is called for every completed access unit.
 * Returns 0, or -1 if an access unit exceeds max_au. */
int h264_au_reader_push(h264_au_reader *r, const uint8_t *data, size_t n, h264_au_cb cb, void *user);
/* End of stream: complete the last access unit. */
void h264_au_reader_flush(h264_au_reader *r, h264_au_cb cb, void *user);

#ifdef __cplusplus
};
#endif

#endif /* HAVE_H264_HRD_H__ */
