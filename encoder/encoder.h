/*
    TVLTx - TV Lovers Encoder
    Copyright (C) 2016 SDR Telecom

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

#ifndef HAVE_ENCODER_H__
#define HAVE_ENCODER_H__

#include "encoder_opts.h"

#define PTS_CLOCK 90000
#define PTS_MASK 0x1FFFFFFFFLL;

// video macros
#define TRANSFER_SIZE 518400

#define MAX_H264_F_SIZE 1048576 // 2^20 is should be enough for very high bitrates...

int encoder_start(service_config *service);
void *encoder_video_thread(void *service_v);
void *encoder_audio_thread(void *service_v);
void *encoder_video_helper_thread(void *helper_h);



#endif /* HAVE_ENCODER__ */
