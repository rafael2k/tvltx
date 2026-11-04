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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "h264_hrd.h"

#define NAL_IDR     5
#define NAL_SEI     6
#define NAL_SPS     7

#define SEI_BUFFERING_PERIOD 0
#define SEI_PIC_TIMING       1

#define MAX_RBSP 4096   /* SPS and SEI are small; larger ones are cut short */

/* ---- bit reader over the RBSP (emulation prevention bytes removed) ------ */

typedef struct
{
    uint8_t data[MAX_RBSP];
    size_t size;
    size_t bit;
    bool error;
} bits;

static void bits_init(bits *b, const uint8_t *nal, size_t len)
{
    size_t zeros = 0;
    b->size = 0;
    b->bit = 0;
    b->error = false;
    for (size_t i = 0; i < len && b->size < MAX_RBSP; i++) {
        if (zeros >= 2 && nal[i] == 0x03) {   /* emulation_prevention_three_byte */
            zeros = 0;
            continue;
        }
        zeros = (nal[i] == 0) ? zeros + 1 : 0;
        b->data[b->size++] = nal[i];
    }
}

static uint32_t u(bits *b, int n)
{
    uint32_t v = 0;
    for (int i = 0; i < n; i++) {
        if (b->bit >= b->size * 8) {
            b->error = true;
            return 0;
        }
        v = (v << 1) | ((b->data[b->bit / 8] >> (7 - b->bit % 8)) & 1);
        b->bit++;
    }
    return v;
}

static uint32_t ue(bits *b)
{
    int zeros = 0;
    while (!b->error && u(b, 1) == 0)
        if (++zeros > 31) {
            b->error = true;
            return 0;
        }
    if (b->error)
        return 0;
    return ((1u << zeros) - 1) + u(b, zeros);
}

static int32_t se(bits *b)
{
    uint32_t k = ue(b);
    return (k & 1) ? (int32_t) ((k + 1) / 2) : -(int32_t) (k / 2);
}

static bool more_rbsp_data(bits *b)
{
    /* rbsp_trailing_bits: a stop bit then zeros up to the end */
    if (b->bit >= b->size * 8)
        return false;
    size_t last = b->size;
    while (last > 0 && b->data[last - 1] == 0)
        last--;
    if (last == 0)
        return false;
    uint8_t lb = b->data[last - 1];
    int trailing = 0;
    while (!(lb & (1 << trailing)))
        trailing++;
    size_t stop_bit = (last - 1) * 8 + (7 - trailing);
    return b->bit < stop_bit;
}

/* ---- SPS ---------------------------------------------------------------- */

static void skip_scaling_list(bits *b, int size)
{
    int last = 8, next = 8;
    for (int j = 0; j < size; j++) {
        if (next != 0)
            next = (last + se(b) + 256) % 256;
        last = (next == 0) ? last : next;
    }
}

static void hrd_parameters(h264_hrd *h, bits *b)
{
    h->cpb_cnt = (int) ue(b) + 1;
    u(b, 4); /* bit_rate_scale */
    u(b, 4); /* cpb_size_scale */
    for (int i = 0; i < h->cpb_cnt && !b->error; i++) {
        ue(b); /* bit_rate_value_minus1 */
        ue(b); /* cpb_size_value_minus1 */
        u(b, 1); /* cbr_flag */
    }
    h->initial_cpb_removal_delay_length = (int) u(b, 5) + 1;
    h->cpb_removal_delay_length = (int) u(b, 5) + 1;
    h->dpb_output_delay_length = (int) u(b, 5) + 1;
    u(b, 5); /* time_offset_length */
}

static void parse_sps(h264_hrd *h, bits *b)
{
    uint32_t profile_idc = u(b, 8);
    u(b, 8); /* constraint flags */
    u(b, 8); /* level_idc */
    ue(b);   /* seq_parameter_set_id */
    if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 244 ||
        profile_idc == 44 || profile_idc == 83 || profile_idc == 86 || profile_idc == 118 ||
        profile_idc == 128 || profile_idc == 138 || profile_idc == 139 || profile_idc == 134 ||
        profile_idc == 135) {
        uint32_t chroma_format_idc = ue(b);
        if (chroma_format_idc == 3)
            u(b, 1); /* separate_colour_plane_flag */
        ue(b); /* bit_depth_luma_minus8 */
        ue(b); /* bit_depth_chroma_minus8 */
        u(b, 1); /* qpprime_y_zero_transform_bypass_flag */
        if (u(b, 1)) { /* seq_scaling_matrix_present_flag */
            int lists = (chroma_format_idc != 3) ? 8 : 12;
            for (int i = 0; i < lists && !b->error; i++)
                if (u(b, 1))
                    skip_scaling_list(b, i < 6 ? 16 : 64);
        }
    }
    ue(b); /* log2_max_frame_num_minus4 */
    uint32_t poc_type = ue(b);
    if (poc_type == 0) {
        ue(b); /* log2_max_pic_order_cnt_lsb_minus4 */
    } else if (poc_type == 1) {
        u(b, 1); /* delta_pic_order_always_zero_flag */
        se(b);   /* offset_for_non_ref_pic */
        se(b);   /* offset_for_top_to_bottom_field */
        uint32_t n = ue(b);
        for (uint32_t i = 0; i < n && !b->error; i++)
            se(b);
    }
    ue(b);   /* max_num_ref_frames */
    u(b, 1); /* gaps_in_frame_num_value_allowed_flag */
    ue(b);   /* pic_width_in_mbs_minus1 */
    ue(b);   /* pic_height_in_map_units_minus1 */
    if (!u(b, 1)) /* frame_mbs_only_flag */
        u(b, 1);  /* mb_adaptive_frame_field_flag */
    u(b, 1); /* direct_8x8_inference_flag */
    if (u(b, 1)) { /* frame_cropping_flag */
        ue(b); ue(b); ue(b); ue(b);
    }

    h->nal_hrd = h->vcl_hrd = h->pic_struct_present = false;
    h->num_units_in_tick = h->time_scale = 0;
    if (u(b, 1)) { /* vui_parameters_present_flag */
        if (u(b, 1)) { /* aspect_ratio_info_present_flag */
            if (u(b, 8) == 255) { /* Extended_SAR */
                u(b, 16);
                u(b, 16);
            }
        }
        if (u(b, 1)) /* overscan_info_present_flag */
            u(b, 1);
        if (u(b, 1)) { /* video_signal_type_present_flag */
            u(b, 3); u(b, 1);
            if (u(b, 1)) { /* colour_description_present_flag */
                u(b, 8); u(b, 8); u(b, 8);
            }
        }
        if (u(b, 1)) { /* chroma_loc_info_present_flag */
            ue(b); ue(b);
        }
        if (u(b, 1)) { /* timing_info_present_flag */
            h->num_units_in_tick = u(b, 32);
            h->time_scale = u(b, 32);
            u(b, 1); /* fixed_frame_rate_flag */
        }
        h->nal_hrd = u(b, 1);
        if (h->nal_hrd)
            hrd_parameters(h, b);
        h->vcl_hrd = u(b, 1);
        if (h->vcl_hrd)
            hrd_parameters(h, b);
        if (h->nal_hrd || h->vcl_hrd)
            u(b, 1); /* low_delay_hrd_flag */
        h->pic_struct_present = u(b, 1);
    }
    h->have_sps = !b->error && h->time_scale > 0 && h->num_units_in_tick > 0;
}

/* ---- SEI ---------------------------------------------------------------- */

static void parse_sei(h264_hrd *h, bits *b)
{
    while (more_rbsp_data(b) && !b->error) {
        uint32_t type = 0, size = 0, v;
        do { v = u(b, 8); type += v; } while (v == 0xFF && !b->error);
        do { v = u(b, 8); size += v; } while (v == 0xFF && !b->error);
        if (b->error)
            return;
        size_t start = b->bit;

        if (type == SEI_BUFFERING_PERIOD && h->have_sps) {
            ue(b); /* seq_parameter_set_id */
            if (h->nal_hrd) {
                for (int i = 0; i < h->cpb_cnt && !b->error; i++) {
                    uint32_t delay = u(b, h->initial_cpb_removal_delay_length);
                    u(b, h->initial_cpb_removal_delay_length); /* offset */
                    if (i == 0)
                        h->au_initial_cpb_removal_delay = delay;
                }
            }
            /* VCL HRD values follow; TVLTx uses the NAL HRD (x264 --nal-hrd) */
            h->au_buffering_period = !b->error && h->nal_hrd;
        } else if (type == SEI_PIC_TIMING && h->have_sps) {
            if (h->nal_hrd || h->vcl_hrd) {
                h->au_cpb_removal_delay = u(b, h->cpb_removal_delay_length);
                h->au_dpb_output_delay = u(b, h->dpb_output_delay_length);
                h->au_pic_timing = !b->error;
            }
        }

        /* next message: skip whatever of this payload was not read */
        b->bit = start + (size_t) size * 8;
    }
}

/* ---- public ------------------------------------------------------------- */

void h264_hrd_init(h264_hrd *h)
{
    memset(h, 0, sizeof(*h));
}

void h264_hrd_nal(h264_hrd *h, const uint8_t *nal, size_t len)
{
    if (len < 1)
        return;
    int type = nal[0] & 0x1F;
    if (type == NAL_IDR) {
        h->au_idr = true;
        return;
    }
    if (type != NAL_SPS && type != NAL_SEI)
        return;

    static __thread bits b;   /* 4 KiB, per encoder thread */
    bits_init(&b, nal + 1, len - 1);
    if (type == NAL_SPS)
        parse_sps(h, &b);
    else
        parse_sei(h, &b);
}

int h264_hrd_au_end(h264_hrd *h, double *cpb_removal_time, double *dpb_output_time, bool *keyframe)
{
    int ret = -1;
    *keyframe = h->au_idr || h->au_buffering_period;

    if (h->have_sps && h->au_pic_timing) {
        double t_r = 0;
        /* same arithmetic as x264 (encoder/ratecontrol.c) */
        if (!h->have_anchor) {
            if (h->au_buffering_period) {
                t_r = h->nominal_removal_time = (double) h->au_initial_cpb_removal_delay / 90000;
                h->have_anchor = true;
                ret = 0;
            }
        } else {
            t_r = h->nominal_removal_time + (double) h->au_cpb_removal_delay *
                  h->num_units_in_tick / h->time_scale;
            if (h->au_buffering_period)
                h->nominal_removal_time = t_r;
            ret = 0;
        }
        if (ret == 0) {
            *cpb_removal_time = t_r;
            *dpb_output_time = (double) h->au_dpb_output_delay * h->num_units_in_tick / h->time_scale + t_r;
        }
    }

    h->au_buffering_period = false;
    h->au_pic_timing = false;
    h->au_idr = false;
    return ret;
}

/* ---- access unit reader ------------------------------------------------- */

#define NAL_AUD 9
#define NONE SIZE_MAX

int h264_au_reader_init(h264_au_reader *r, size_t max_au)
{
    memset(r, 0, sizeof(*r));
    r->cap = max_au + 65536;
    r->buf = malloc(r->cap);
    r->au_start = r->nal_start = NONE;
    h264_hrd_init(&r->hrd);
    return r->buf ? 0 : -1;
}

void h264_au_reader_free(h264_au_reader *r)
{
    free(r->buf);
    r->buf = NULL;
}

static void emit_au(h264_au_reader *r, size_t end, h264_au_cb cb, void *user)
{
    double cpb = 0, dpb = 0;
    bool key = false;
    int ok = h264_hrd_au_end(&r->hrd, &cpb, &dpb, &key);
    cb(user, r->buf + r->au_start, end - r->au_start, ok == 0, cpb, dpb, key);
}

int h264_au_reader_push(h264_au_reader *r, const uint8_t *data, size_t n, h264_au_cb cb, void *user)
{
    while (n > 0) {
        size_t take = r->cap - r->len;
        if (take == 0)
            return -1;  /* one access unit larger than max_au */
        if (take > n)
            take = n;
        memcpy(r->buf + r->len, data, take);
        r->len += take;
        data += take;
        n -= take;

        size_t i = r->scan;
        /* a start code needs 3 bytes and the NAL header 1 more */
        while (i + 3 < r->len) {
            if (r->buf[i] != 0 || r->buf[i + 1] != 0 || r->buf[i + 2] != 1) {
                i++;
                continue;
            }
            size_t sc = (i > 0 && r->buf[i - 1] == 0) ? i - 1 : i;  /* zero_byte of a 4-byte start code */
            size_t payload = i + 3;
            if (r->nal_start != NONE)
                h264_hrd_nal(&r->hrd, r->buf + r->nal_start, sc - r->nal_start);
            if ((r->buf[payload] & 0x1F) == NAL_AUD) {
                if (r->au_start != NONE && sc > r->au_start)
                    emit_au(r, sc, cb, user);
                r->au_start = sc;
            } else if (r->au_start == NONE) {
                r->au_start = sc;   /* stream without AUDs at the start */
            }
            r->nal_start = payload;
            i = payload;
        }
        r->scan = i;

        /* drop what precedes the current access unit */
        size_t keep_from = (r->au_start != NONE) ? r->au_start : (r->len > 3 ? r->len - 3 : 0);
        if (keep_from > 0) {
            memmove(r->buf, r->buf + keep_from, r->len - keep_from);
            r->len -= keep_from;
            r->scan -= keep_from;
            if (r->au_start != NONE) r->au_start -= keep_from;
            if (r->nal_start != NONE) r->nal_start -= keep_from;
        }
    }
    return 0;
}

void h264_au_reader_flush(h264_au_reader *r, h264_au_cb cb, void *user)
{
    if (r->nal_start != NONE && r->nal_start < r->len)
        h264_hrd_nal(&r->hrd, r->buf + r->nal_start, r->len - r->nal_start);
    if (r->au_start != NONE && r->len > r->au_start)
        emit_au(r, r->len, cb, user);
    r->len = r->scan = 0;
    r->au_start = r->nal_start = NONE;
}
