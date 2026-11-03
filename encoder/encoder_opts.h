/*
    TVLTx - TV Lovers Encoder
    Copyright (C) 2015-2016 SDR Telecom

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

#ifndef HAVE_ENCODER_OPTS_H__
#define HAVE_ENCODER_OPTS_H__

#include <stdbool.h>
#include "ring_buffer.h"
#include "buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

// global definitions
#define MAX_AUDIO_PROGRAMS 16

#define CARD_TYPE_TEST 0
#define CARD_TYPE_DEKTEC 1
#define CARD_TYPE_BLACKMAGIC 2
#define CARD_TYPE_DVEO 3

#define INPUT_TYPE_NONE 0
#define INPUT_TYPE_SDI 1
#define INPUT_TYPE_HDMI 2
#define INPUT_TYPE_COMPOSITE 3
#define INPUT_TYPE_COMPONENT 4
#define INPUT_TYPE_OPTICALSDI 5
#define INPUT_TYPE_SVIDEO 6

#define VIDEO_MODE_NONE 0
#define VIDEO_MODE_1080_P_60 1
#define VIDEO_MODE_1080_P_5994 2
#define VIDEO_MODE_1080_P_50 3
#define VIDEO_MODE_1080_I_5994 4
#define VIDEO_MODE_1080_I_60 5
#define VIDEO_MODE_1080_I_50 6
#define VIDEO_MODE_1080_P_30 7
#define VIDEO_MODE_1080_P_2997 8
#define VIDEO_MODE_1080_P_25 9
#define VIDEO_MODE_720_P_60 10
#define VIDEO_MODE_720_P_5994 11
#define VIDEO_MODE_720_P_50 12
#define VIDEO_MODE_720_P_30 13
#define VIDEO_MODE_720_P_2997 14
#define VIDEO_MODE_720_P_25 15
#define VIDEO_MODE_480_I_5994 16
#define VIDEO_MODE_480_P_2997 17
#define VIDEO_MODE_576_I_50 18
#define VIDEO_MODE_576_P_25 19
#define VIDEO_MODE_288_P_2997 20
#define VIDEO_MODE_288_P_25 21
#define VIDEO_MODE_240_P_2997 22
#define VIDEO_MODE_240_P_25 23
#define VIDEO_MODE_180_P_2997 24
#define VIDEO_MODE_180_P_25 25

// service structure
typedef struct
{
    int service_number;
    int input_id;
    int video_output_mode;
    int video_bitrate;
    int gop_size;
    int nr_audio_streams;
    int nr_channels[MAX_AUDIO_PROGRAMS];
    int audio_bitrate[MAX_AUDIO_PROGRAMS];
    int audio_mapping[MAX_AUDIO_PROGRAMS];
    int av_offset;
    bool is_oneseg;
    char audio_shm_name[MAX_AUDIO_PROGRAMS][32];
    char video_shm_name[32];
    buffer buf_video_in;
    buffer buf_video_out;
    buffer buf_audio_in[MAX_AUDIO_PROGRAMS];
    buffer buf_audio_out[MAX_AUDIO_PROGRAMS];
} service_config;

typedef struct
{
    int input_id;
    int card_number;
    int input_type;
    int video_input_mode;
    int audio_input_channel_nr;
    int card_type;
}  input_config;

//TODO: make the "typedef struct {...} output_config;".

#ifdef __cplusplus
};
#endif

#endif /* HAVE_ENCODER_OPTS__ */
