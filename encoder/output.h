/*
    TVLTx - TV Lovers Encoder
    Copyright (C) 2016-2017 SDR Telecom

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

#ifndef HAVE_OUTPUT_H__
#define HAVE_OUTPUT_H__

// tmm uses 128*1024 + 5
#define PES_SHM_MEMORY_SIZE 512005

int output_start(service_config *service);
void *output_video_thread(void *service_v);
void *output_audio_thread(void *service_a);

#endif /* HAVE_OUTPUT__ */
