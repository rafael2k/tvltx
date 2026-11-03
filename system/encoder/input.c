/*
    TVLTx - TV Lovers Encoder
    Copyright (C) 2016-2018 SDR Telecom

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


#include "encoder_opts.h"
#include "macros.h"
#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>

extern input_config inputs[6];
extern service_config services[10];
extern bool running;
extern int nr_inputs;
extern int nr_services;

int input_start(input_config *input)
{
    pthread_t          tid;

#ifdef HAS_BLACKMAGIC__
    if (input->card_type == CARD_TYPE_BLACKMAGIC)
        pthread_create(&tid, NULL, input_bmd_thread, (void *)input);
#endif

#ifdef HAS_V4L2__
    if (input->card_type == CARD_TYPE_V4L2)
        return input_v4l2_start(input);
#endif

    if(input->card_type == CARD_TYPE_TEST){
        pthread_create(&tid, NULL, input_test_thread_v, (void *)input);
        pthread_create(&tid, NULL, input_test_thread_a, (void *)input);
    }
    return EXIT_SUCCESS;
}

void *input_test_thread_a(void *input_a)
{
   input_config *input = input_a;
    int audio_frame_size = 4*48000/25;

    uint8_t audio_frame[audio_frame_size];

    FILE *audio_in = fopen("audio.pcm", "r");

    int rcv;
    while(running){

        if (ring_buffer_count_bytes(&services[0].buf_audio_in[0] .buf)  >= 3 * 2 * 2 * 48000 ){ // 3s buffer for one stereo 16bit input
            usleep(200000);  // 0.2s
            continue;
        }
        rcv = fread(audio_frame, 1, audio_frame_size, audio_in);

        if (rcv != audio_frame_size)
            tvltx_debug("FODEUUUUUUUUUUU\n");

        if (!rcv){
            tvltx_debug("Rewinding audio input\n");
            rewind(audio_in);
//            running = false;
            continue;
        }

        for (int i = 0; i < nr_services; i++){
            if (services[i].input_id == input->input_id){
                for (int j = 0; j < services[i].nr_audio_streams; j++){
                    // tvltx_debug("wrote audio buffer: %d\n", rcv);
                    write_buffer(&services[i].buf_audio_in[j], audio_frame, rcv); 
                }
            }
        }
    }
    tvltx_debug("Exiting test audio input thread.\n");

    return EXIT_SUCCESS;
}

void *input_test_thread_v(void *input_v)
{
    input_config *input = input_v;
    int video_frame_size = 0;

    switch(input->video_input_mode)
    {
    case VIDEO_MODE_1080_P_60:
    case VIDEO_MODE_1080_P_5994:
    case VIDEO_MODE_1080_P_50:
    case VIDEO_MODE_1080_I_5994:
    case VIDEO_MODE_1080_I_60:
    case VIDEO_MODE_1080_I_50:
    case VIDEO_MODE_1080_P_30:
    case VIDEO_MODE_1080_P_2997:
    case VIDEO_MODE_1080_P_25:
        video_frame_size  = 1920*1080*2;
        break;
    case VIDEO_MODE_720_P_60:
    case VIDEO_MODE_720_P_5994:
    case VIDEO_MODE_720_P_50:
    case VIDEO_MODE_720_P_30:
    case VIDEO_MODE_720_P_2997:
    case VIDEO_MODE_720_P_25:
        video_frame_size  = 1280*720*2;
        break;
    case VIDEO_MODE_576_I_50:
    case VIDEO_MODE_576_P_25:
        video_frame_size  = 720*576*2;
        break;
    case VIDEO_MODE_480_I_5994:
    case VIDEO_MODE_480_P_2997:
        video_frame_size  = 720*486*2;
        break;
    default:
        tvltx_debug("No valid input video mode.\n");
        return (void *) EXIT_FAILURE;
    }

    uint8_t video_frame[video_frame_size];


    FILE *video_in = fopen("video.yuv", "r");

    int rcv;

    while(running){
        // loop here using async I/O

        if (ring_buffer_count_bytes(&services[0].buf_video_in.buf )  >= (video_frame_size * 90) ){ // 3s buffer
            usleep(200000);  // 0.2s
            continue;
        }

        rcv = fread(video_frame, 1, video_frame_size, video_in);

        if (rcv != video_frame_size)
            tvltx_debug("FODEUUUUUUUUUUU\n");

        if (!rcv){
            tvltx_debug("Rewinding video input\n");
            rewind(video_in);
            continue;
        }

        for (int i = 0; i < nr_services; i++){
            if (services[i].input_id == input->input_id){
                // tvltx_debug("wrote video buffer: %d\n", rcv);
                write_buffer(&services[i].buf_video_in, video_frame, rcv);
            }
        }
    }

    tvltx_debug("Exiting test video input thread.\n");

    return EXIT_SUCCESS;
}
