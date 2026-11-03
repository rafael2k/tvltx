/*
    TVLTx - TV Lovers Encoder
    Copyright (C) 2015-2017 SDR Telecom

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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

#include "encoder_opts.h"
#include "macros.h"
#include "input.h"
#include "encoder.h"
#include "output.h"

// global var with all the services configuration
service_config services[10];
input_config inputs[6];

int nr_services;
int nr_inputs;

bool running;
static void sigfunc(int signum);

int main(int argc, char **argv, char **envp)
{
    // we should populate this from a config file
    services[0].service_number = 0;
    services[0].is_oneseg = false;
    services[0].input_id = 0;
    services[0].gop_size = 30;
    nr_services = 1;

    inputs[0].input_id = 0;
    inputs[0].audio_input_channel_nr = 2;
    inputs[0].card_type = CARD_TYPE_BLACKMAGIC;
    nr_inputs = 1;

    tvltx_info("TVLTx Encoder Version 0.1.\n");
    tvltx_info("Copyright: SDR Telecom 2015-2017.\n");
    tvltx_info("License: GNU AFFERO Version 3 plus non-commercial use restriction.\n");

    if (argc < 3)
    {
    usage_info:
        tvltx_info("Usage: %s [OPTIONS]\n\n", argv[0]);
        tvltx_info("OPTIONS:\n");
        tvltx_info("    -c          card number\n");
        tvltx_info("    -t          interface type (COMPOSITE, COMPONENT, HDMI, SDI)\n");
        tvltx_info("    -b          number of input channels (2, 8 or 16)\n");
//     tvltx_info("    -i          interface number\n");
        tvltx_info("    -i          input mode (1080i5994, 1080i50, 1080p2997, 1080p25, 720p2997, 720p25, 480i5994, 576i50) \n");
        tvltx_info("    -o          output mode (1080i5994, 1080i50, 1080p2997, 1080p25, 720p2997, 720p25, 480i5994, 576i50, 288p2997, 288p25, 240p2997, 240p25, 180p2997, 180p25) \n");
        tvltx_info("    -v          video bitrate, in kbit/s\n");
        tvltx_info("    -n          number of audio streams (1 to 8)\n");
        tvltx_info("    -a          audio bitrates, comma separated (12 up to 384, unit in kbit/s), for example, with 2 audio streams, \"-l 96,128\"\n");
        tvltx_info("    -j          input audio mapping, comma separated, for example, first two audio input signals for first stereo audio, and consecutive two for second stereo stream, \"-j 0x3,0xC\"\n");
        tvltx_info("    -s          shared memory identifier, for video and audio outputs, for example, \"-s video_mem,audio1_mem,audio2_mem\"\n");
//     tvltx_info("    -x          audio/video offset\n");
        tvltx_info("    -w          enable 1-seg PES adaptations\n");
        tvltx_info("    -z          enable input from files (video.yuv and audio.pcm, formats: uyvy422 and int16).\n");

        return EXIT_SUCCESS;
    }


    int opt;
    while ((opt = getopt(argc, argv, "c:t:b:i:o:v:n:a:j:s:x:wz")) != -1)
    {
        switch (opt)
        {
        case 's':
            sscanf(optarg, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,]", services[0].video_shm_name, services[0].audio_shm_name[0], services[0].audio_shm_name[1], services[0].audio_shm_name[2], services[0].audio_shm_name[3], services[0].audio_shm_name[4], services[0].audio_shm_name[5], services[0].audio_shm_name[6], services[0].audio_shm_name[7]);
            break;
        case 'w':
            services[0].is_oneseg = true;
            break;
        case 'c':
            inputs[0].card_number = 0;
            break;
        case 't':
            if (!strcmp("SDI", optarg))
                inputs[0].input_type = INPUT_TYPE_SDI;

            if (!strcmp("HDMI", optarg))
                inputs[0].input_type = INPUT_TYPE_HDMI;

            if (!strcmp("COMPOSITE", optarg))
                inputs[0].input_type = INPUT_TYPE_COMPOSITE;

            if (!strcmp("COMPONENT", optarg))
                inputs[0].input_type = INPUT_TYPE_COMPONENT;

            break;

        case 'i':
            if (!strcmp("1080p60", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_1080_P_60;

            if (!strcmp("1080p5994", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_1080_P_5994;

            if (!strcmp("1080p50", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_1080_P_50;

            if (!strcmp("1080i5994", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_1080_I_5994;

            if (!strcmp("1080i60", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_1080_I_60;

            if (!strcmp("1080i50", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_1080_I_50;

            if (!strcmp("1080p30", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_1080_P_30;

            if (!strcmp("1080p2997", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_1080_P_2997;

            if (!strcmp("1080p25", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_1080_P_25;

            if (!strcmp("720p60", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_720_P_60;

            if (!strcmp("720p5994", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_720_P_5994;

            if (!strcmp("720p50", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_720_P_50;

            if (!strcmp("720p30", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_720_P_30;

            if (!strcmp("720p2997", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_720_P_2997;

            if (!strcmp("720p25", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_720_P_25;

            if (!strcmp("480i5994", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_480_I_5994;

            if (!strcmp("480p2997", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_480_P_2997;

            if (!strcmp("576i50", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_576_I_50;

            if (!strcmp("576p25", optarg))
                inputs[0].video_input_mode = VIDEO_MODE_576_P_25;

            break;
        case 'o':
            if (!strcmp("1080p60", optarg))
                services[0].video_output_mode = VIDEO_MODE_1080_P_60;

            if (!strcmp("1080p5994", optarg))
                services[0].video_output_mode = VIDEO_MODE_1080_P_5994;

            if (!strcmp("1080p50", optarg))
                services[0].video_output_mode = VIDEO_MODE_1080_P_50;

            if (!strcmp("1080i5994", optarg))
                services[0].video_output_mode = VIDEO_MODE_1080_I_5994;

            if (!strcmp("1080i60", optarg))
                services[0].video_output_mode = VIDEO_MODE_1080_I_60;

            if (!strcmp("1080i50", optarg))
                services[0].video_output_mode = VIDEO_MODE_1080_I_50;

            if (!strcmp("1080p30", optarg))
                services[0].video_output_mode = VIDEO_MODE_1080_P_30;

            if (!strcmp("1080p2997", optarg))
                services[0].video_output_mode = VIDEO_MODE_1080_P_2997;

            if (!strcmp("1080p25", optarg))
                services[0].video_output_mode = VIDEO_MODE_1080_P_25;

            if (!strcmp("720p60", optarg))
                services[0].video_output_mode = VIDEO_MODE_720_P_60;

            if (!strcmp("720p5994", optarg))
                services[0].video_output_mode = VIDEO_MODE_720_P_5994;

            if (!strcmp("720p50", optarg))
                services[0].video_output_mode = VIDEO_MODE_720_P_50;

            if (!strcmp("720p30", optarg))
                services[0].video_output_mode = VIDEO_MODE_720_P_30;

            if (!strcmp("720p2997", optarg))
                services[0].video_output_mode = VIDEO_MODE_720_P_2997;

            if (!strcmp("720p25", optarg))
                services[0].video_output_mode = VIDEO_MODE_720_P_25;

            if (!strcmp("480i5994", optarg))
                services[0].video_output_mode = VIDEO_MODE_480_I_5994;

            if (!strcmp("480p2997", optarg))
                services[0].video_output_mode = VIDEO_MODE_480_P_2997;

            if (!strcmp("576i50", optarg))
                services[0].video_output_mode = VIDEO_MODE_576_I_50;

            if (!strcmp("576p25", optarg))
                services[0].video_output_mode = VIDEO_MODE_576_P_25;

            if (!strcmp("288p2997", optarg))
                services[0].video_output_mode = VIDEO_MODE_288_P_2997;

            if (!strcmp("288p25", optarg))
                services[0].video_output_mode = VIDEO_MODE_288_P_25;

            if (!strcmp("240p2997", optarg))
                services[0].video_output_mode = VIDEO_MODE_240_P_2997;

            if (!strcmp("240p25", optarg))
                services[0].video_output_mode = VIDEO_MODE_240_P_25;

            if (!strcmp("180p2997", optarg))
                services[0].video_output_mode = VIDEO_MODE_180_P_2997;

            if (!strcmp("180p25", optarg))
                services[0].video_output_mode = VIDEO_MODE_180_P_25;

            break;

        case 'v':
            services[0].video_bitrate = atoi(optarg);
            break;

        case 'b':
            services[0].nr_channels[0] = atoi(optarg);
            break;

        case 'n':
            services[0].nr_audio_streams = atoi(optarg);
            break;
        case 'a':
            sscanf(optarg, "%d,%d,%d,%d,%d,%d,%d,%d", &services[0].audio_bitrate[0], &services[0].audio_bitrate[1], &services[0].audio_bitrate[2], &services[0].audio_bitrate[3], &services[0].audio_bitrate[4], &services[0].audio_bitrate[5], &services[0].audio_bitrate[6], &services[0].audio_bitrate[7]);
            break;
        case 'j':
            sscanf(optarg, "%i,%i,%i,%i,%i,%i,%i,%i", &services[0].audio_mapping[0], &services[0].audio_mapping[1], &services[0].audio_mapping[2], &services[0].audio_mapping[3], &services[0].audio_mapping[4], &services[0].audio_mapping[5], &services[0].audio_mapping[6], &services[0].audio_mapping[7]);
            break;
        case 'z':
            inputs[0].card_type = CARD_TYPE_TEST;
            break;
        default:
            goto usage_info;
        }
    }

    // signal handlers
    signal(SIGINT, sigfunc);
    signal(SIGTERM, sigfunc);
    signal(SIGHUP, sigfunc);

    // do the verifications for error cheking... TODO

    // initialize buffers
    for (int i = 0; i < nr_services; i++){
        initialize_buffer(&services[i].buf_video_in, 29);
        initialize_buffer(&services[i].buf_video_out, 28);
        for (int j = 0; j < services[i].nr_audio_streams; j++){
            initialize_buffer(&services[i].buf_audio_in[j], 26);
            initialize_buffer(&services[i].buf_audio_out[j], 26);
        }
    }

    // run worker threads
    running = true;

    // start inputs
    for (int i = 0; i < nr_inputs; i++){
        input_start(&inputs[i]);
    }

    // start services
    for (int i = 0; i < nr_services; i++){
        encoder_start(&services[i]);
        output_start(&services[i]);
    }

    // start outputs - TODO

    pthread_exit(EXIT_SUCCESS);
    return EXIT_SUCCESS;

}

static void sigfunc(int signum)
{
    if (signum == SIGINT || signum == SIGTERM){
        running = false;
        tvltx_info("Exiting...\n");
        sleep(2);
        exit(EXIT_SUCCESS);
    }
    if (signum == SIGHUP){
        tvltx_info("SIGHUP received.\n");
        // reload configurations...
    }
}
