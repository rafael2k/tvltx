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


#include "encoder_opts.h"
#include "macros.h"
#include "pes.h"
#include "encoder.h"
#include "hexdump.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <poll.h>
#include <unistd.h>

#include <aacenc_lib.h>

extern input_config inputs[6];
extern service_config services[10];
extern bool running;

int encoder_start(service_config *service)
{
    pthread_t tid[2];

    pthread_create(&tid[0], NULL, encoder_video_thread, (void *)service);
    pthread_create(&tid[1], NULL, encoder_audio_thread, (void *)service);

    return EXIT_SUCCESS;
}

void *encoder_video_thread(void *service_v)
{
    service_config *service = service_v;
    input_config *input = &inputs[service->input_id];
    int bitrate = service->video_bitrate;

    uint8_t input_buffer[TRANSFER_SIZE];
    char fifo_input_name[64];
    char fifo_output_name[64];
    char input_resolution[128];
    char output_resolution[128];
    int ret_code;
    char x264_cmd[1024];
    int fps_num, fps_den;

    sprintf(fifo_input_name, "input-XXXXXX");
    mktemp(fifo_input_name);
    strcat(fifo_input_name, ".yuv");
    unlink(fifo_input_name);

    sprintf(fifo_output_name, "output-XXXXXX");
    mktemp(fifo_output_name);
    strcat(fifo_output_name, ".264");
    unlink(fifo_output_name);

    ret_code = mkfifo(fifo_input_name, S_IRWXU | S_IRWXG | S_IRWXO);
    if (ret_code != 0)
        return (void *) EXIT_FAILURE;

    ret_code = mkfifo(fifo_output_name, S_IRWXU | S_IRWXG | S_IRWXO);
    if (ret_code != 0)
        return (void *) EXIT_FAILURE;

    switch(input->video_input_mode)
    {
    case VIDEO_MODE_1080_P_60:
        strcpy(input_resolution, " --input-res 1920x1080 --input-csp uyvy422 --fps 60");
        fps_num = 60; fps_den = 1;
        break;
    case VIDEO_MODE_1080_P_5994:
        strcpy(input_resolution, " --input-res 1920x1080 --input-csp uyvy422 --fps 60000/1001 ");
        fps_num = 60000; fps_den = 1001;
        break;
    case VIDEO_MODE_1080_P_50:
        strcpy(input_resolution, " --input-res 1920x1080 --input-csp uyvy422 --fps 50 ");
        fps_num = 50; fps_den = 1;
        break;
    case VIDEO_MODE_1080_I_5994:
        strcpy(input_resolution, " --input-res 1920x1080 --input-csp uyvy422 --fps 30000/1001 ");
        fps_num = 30000; fps_den = 1001;
        break;
    case VIDEO_MODE_1080_I_60:
        strcpy(input_resolution, " --input-res 1920x1080 --input-csp uyvy422 --fps 30 ");
        fps_num = 30; fps_den = 1;
        break;
    case VIDEO_MODE_1080_I_50:
        strcpy(input_resolution, " --input-res 1920x1080 --input-csp uyvy422 --fps 25 ");
        fps_num = 25; fps_den = 1;
        break;
    case VIDEO_MODE_1080_P_30:
        strcpy(input_resolution, " --input-res 1920x1080 --input-csp uyvy422 --fps 30 ");
        fps_num = 30; fps_den = 1;
        break;
    case VIDEO_MODE_1080_P_2997:
        strcpy(input_resolution, " --input-res 1920x1080 --input-csp uyvy422 --fps 30000/1001 ");
        fps_num = 30000; fps_den = 1001;
        break;
    case VIDEO_MODE_1080_P_25:
        strcpy(input_resolution, " --input-res 1920x1080 --input-csp uyvy422 --fps 25 ");
        fps_num = 25; fps_den = 1;
        break;
    case VIDEO_MODE_720_P_60:
        strcpy(input_resolution, " --input-res 1280x720 --input-csp uyvy422 --fps 60 ");
        fps_num = 60; fps_den = 1;
        break;
    case VIDEO_MODE_720_P_5994:
        strcpy(input_resolution, " --input-res 1280x720 --input-csp uyvy422 --fps 60000/1001 ");
        fps_num = 60000; fps_den = 1001;
        break;
    case VIDEO_MODE_720_P_50:
        strcpy(input_resolution, " --input-res 1280x720 --input-csp uyvy422 --fps 50 ");
        fps_num = 50; fps_den = 1;
        break;
    case VIDEO_MODE_720_P_30:
        strcpy(input_resolution, " --input-res 1280x720 --input-csp uyvy422 --fps 30 ");
        fps_num = 30; fps_den = 1;
        break;
    case VIDEO_MODE_720_P_2997:
        strcpy(input_resolution, " --input-res 1280x720 --input-csp uyvy422 --fps 30000/1001 ");
        fps_num = 30000; fps_den = 1001;
        break;
    case VIDEO_MODE_720_P_25:
        strcpy(input_resolution, " --input-res 1280x720 --input-csp uyvy422 --fps 25 ");
        fps_num = 25; fps_den = 1;
        break;
    case VIDEO_MODE_576_I_50:
        strcpy(input_resolution, " --input-res 720x576 --input-csp uyvy422 --fps 25 ");
        fps_num = 25; fps_den = 1;
        break;
    case VIDEO_MODE_576_P_25:
        strcpy(input_resolution, " --input-res 720x576 --input-csp uyvy422 --fps 25 ");
        fps_num = 25; fps_den = 1;
        break;
    case VIDEO_MODE_480_I_5994:
        strcpy(input_resolution, " --input-res 720x486 --input-csp uyvy422 --fps 30000/1001 ");
        fps_num = 30000; fps_den = 1001;
        break;
    case VIDEO_MODE_480_P_2997:
        strcpy(input_resolution, " --input-res 720x486 --input-csp uyvy422 --fps 30000/1001 ");
        fps_num = 30000; fps_den = 1001;
        break;
    default:
        tvltx_debug("No valid input video mode.\n");
        return (void *) EXIT_FAILURE;
    }

    output_resolution[0] = 0;
    char interlaced[16];
    interlaced[0] = 0;

    switch(service->video_output_mode){
    case VIDEO_MODE_1080_P_60:
        if (fps_num == 60 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:1920,1080 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_1080_P_5994:
        if (fps_num == 60000 && fps_den == 1001)
            strcpy(output_resolution, " --vf resize:1920,1080 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_1080_P_50:
        if (fps_num == 50 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:1920,1080 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_1080_I_5994:
        strcpy(interlaced, " --tff ");
        if (fps_num == 30000 && fps_den == 1001)
                strcpy(output_resolution, " --vf resize:1920,1080 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_1080_I_60:
        strcpy(interlaced, " --tff ");
        if (fps_num == 30 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:1920,1080 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_1080_I_50:
        strcpy(interlaced, " --tff ");
        if (fps_num == 25 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:1920,1080 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_1080_P_30:
        if (fps_num == 30 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:1920,1080 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_1080_P_2997:
        if (fps_num == 30000 && fps_den == 1001)
            strcpy(output_resolution, " --vf resize:1920,1080 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_1080_P_25:
        if (fps_num == 25 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:1920,1080 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_720_P_60:
        if (fps_num == 60 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:1280,720 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_720_P_5994:
            if (fps_num == 60000 && fps_den == 1001)
                strcpy(output_resolution, " --vf resize:1280,720 ");
            else
                goto fps_mismatch;
            break;
    case VIDEO_MODE_720_P_50:
        if (fps_num == 50 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:1280,720 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_720_P_30:
        if (fps_num == 30 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:1280,720 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_720_P_2997:
        if (fps_num == 30000 && fps_den == 1001)
            strcpy(output_resolution, " --vf resize:1280,720 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_720_P_25:
        if (fps_num == 25 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:1280,720 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_480_I_5994:
        strcpy(interlaced, " --bff ");
        if (fps_num == 30000 && fps_den == 1001)
            strcpy(output_resolution, " --vf resize:720,480 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_480_P_2997:
        if (fps_num == 30000 && fps_den == 1001)
            strcpy(output_resolution, " --vf resize:720,480 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_576_I_50:
        strcpy(interlaced, " --tff ");
        if (fps_num == 25 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:720,576 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_576_P_25:
        if (fps_num == 25 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:720,576 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_288_P_30:
        if (fps_num == 30 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:352,288 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_288_P_2997:
        if (fps_num == 30000 && fps_den == 1001)
            strcpy(output_resolution, " --vf resize:352,288 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_288_P_25:
        if (fps_num == 25 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:352,288 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_240_P_30:
        if (fps_num == 30 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:320,240 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_240_P_2997:
        if (fps_num == 30000 && fps_den == 1001)
            strcpy(output_resolution, " --vf resize:320,240 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_240_P_25:
        if (fps_num == 25 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:320,240 ");
        else
                goto fps_mismatch;
        break;
    case VIDEO_MODE_180_P_30:
        if (fps_num == 30 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:320,180 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_180_P_2997:
        if (fps_num == 30000 && fps_den == 1001)
            strcpy(output_resolution, " --vf resize:320,180 ");
        else
            goto fps_mismatch;
        break;
    case VIDEO_MODE_180_P_25:
        if (fps_num == 25 && fps_den == 1)
            strcpy(output_resolution, " --vf resize:320,180 ");
        else
            goto fps_mismatch;
        break;
    default:
    fps_mismatch:
        // it seems x264 can convert fps - check!
        tvltx_debug("Output FPS is different than Input FPS. FPS conversion not supported yet!\n");
        return (void *) EXIT_FAILURE;
    }

    if (input->video_input_mode == service->video_output_mode)
        output_resolution[0] = 0;

    // 525-line inputs are 720x486 (SDI); encode 720x480 with the usual crop of
    // 4 top and 2 bottom lines (even, so the field order is kept): x264 needs
    // a height divisible by 4 for interlaced 4:2:0
    if (input->video_input_mode == VIDEO_MODE_480_I_5994 ||
        input->video_input_mode == VIDEO_MODE_480_P_2997) {
        char vf_chain[128];
        const char *resize = strstr(output_resolution, "resize:");
        if (resize)
            snprintf(vf_chain, sizeof(vf_chain), " --vf crop:0,4,0,2/%s", resize);
        else
            snprintf(vf_chain, sizeof(vf_chain), " --vf crop:0,4,0,2 ");
        strcpy(output_resolution, vf_chain);
    }

    // x264 command line creation
    sprintf(x264_cmd, "x264 --aud --nal-hrd cbr --vbv-init 1.0 --demuxer lavf %s -B %d --vbv-maxrate %d --vbv-bufsize %d  --min-keyint %d --keyint %d %s %s %s %s -o %s ",
            input_resolution, bitrate, bitrate, bitrate,
            service->gop_size, service->gop_size,
            output_resolution[0]? output_resolution: "",
            interlaced[0] ? interlaced: "",
            service->is_oneseg? " --profile baseline --level 13 " : " --profile high --level 40 --preset veryfast ",
            fifo_input_name, fifo_output_name);

    tvltx_debug("x264 service %d command line: %s\n", service->service_number, x264_cmd);

    // runs the command
    FILE *x264_output = popen(x264_cmd, "r");
    char x264_txt_output[128];

    if (x264_output == NULL){
        tvltx_debug("x264 popen() error\n");
        return (void *) EXIT_FAILURE;
    }

    struct pollfd pfds[1];
    pfds[0].fd = fileno(x264_output);
    pfds[0].events = POLLIN;

    // we must open the fifo before any other thread try to play with them
    FILE *fifo_input = fopen(fifo_input_name, "w");
    FILE *fifo_output = fopen(fifo_output_name, "r");

    if (!fifo_input || !fifo_output){
        tvltx_debug("fifos opening error\n");
        return (void *) EXIT_FAILURE;
    }

    // TODO: create reader thread
    pthread_t tid;
    struct {
        service_config *service;
        FILE *fifo;
    } helper_par;
    helper_par.service = service;
    helper_par.fifo = fifo_output;
    pthread_create(&tid, NULL, encoder_video_helper_thread, (void *)&helper_par);



    while (running) {
        // cleans x264 output buffer
        int x264_out_avail = poll(pfds, 1, 0);
        if (x264_out_avail && (pfds[0].revents & POLLIN)) {
            while (fgets(x264_txt_output, 128, x264_output) != NULL)
                tvltx_debug("%s\n", x264_txt_output);
        }

        read_buffer(&service->buf_video_in, input_buffer, TRANSFER_SIZE);

        int out = fwrite(input_buffer, TRANSFER_SIZE, 1, fifo_input);
        if (out != 1){
            tvltx_debug("Write to x264 fifo failed\n");
            goto nothing_to_do;
        }
    }

nothing_to_do:
    tvltx_debug ("Exiting video thread\n");
    fclose(fifo_input);
    fclose(fifo_output);
    pclose(x264_output);

    return EXIT_SUCCESS;
}

void *encoder_audio_thread(void *service_a)
{
    service_config *service = service_a;
    HANDLE_AACENCODER handle[service->nr_audio_streams];
    int frame_lengths[service->nr_audio_streams];
    uint64_t sample_count[service->nr_audio_streams];

    for (int i = 0; i < service->nr_audio_streams; i++) { // for each service
        int bitrate = service->audio_bitrate[i] * 1000;
        int channels = service->nr_channels[i];
        int sample_rate = 48000;
        sample_count[i] = 0;

// AUDIO_OBJECT_TYPE: AOT_AAC_LC, AOT_SBR, AOT_PS, AOT_MPEGS
// CHANNEL_MODE: MODE_2, MODE_1_2_2_1
        CHANNEL_MODE mode = MODE_2;
        AACENC_InfoStruct info = { 0 };
        int aot;
        int afterburner = 1;

        // the AAC aot is LC for bitrate > 64k, SBR bitrate between 32k and 64k, and PS for bitrate < 32k
        if (bitrate > 64000)
            aot = AOT_AAC_LC;
        if (bitrate <= 64000 && bitrate > 32000)
            aot = AOT_SBR;
        if (bitrate <= 32000)
            aot = AOT_PS;

        if (aacEncOpen(&handle[i], 0, channels) != AACENC_OK) {
            tvltx_info("Unable to open encoder\n");
            return (void *) EXIT_FAILURE;
        }

        if (aacEncoder_SetParam(handle[i], AACENC_AOT, aot) != AACENC_OK) {
            tvltx_info("Unable to set the AOT\n");
            return (void *) EXIT_FAILURE;
        }

        if (aacEncoder_SetParam(handle[i], AACENC_SAMPLERATE, sample_rate) != AACENC_OK) {
            tvltx_info("Unable to set the AOT\n");
            return (void *) EXIT_FAILURE;
        }

        if (aacEncoder_SetParam(handle[i], AACENC_CHANNELMODE, mode) != AACENC_OK) {
            tvltx_info("Unable to set the channel mode\n");
            return (void *) EXIT_FAILURE ;
        }

        // 0: MPEG channel ordering, 1: WAVE file format channel ordering
        if (aacEncoder_SetParam(handle[i], AACENC_CHANNELORDER, 0) != AACENC_OK) {
            tvltx_info("Unable to set the wav channel order\n");
            return (void *) EXIT_FAILURE ;
        }

        if (aacEncoder_SetParam(handle[i], AACENC_BITRATE, bitrate) != AACENC_OK) {
            tvltx_info("Unable to set the bitrate\n");
            return (void *) EXIT_FAILURE;
        }

        // 0: raw AU, 1: ADIF, 2: ADTS, 6: LATM+muxcfgpresent, 7: LATM+oob, 10: LOAS
        if (aacEncoder_SetParam(handle[i], AACENC_TRANSMUX, 10) != AACENC_OK) {
            tvltx_info("Unable to set the LATM/LOAS transmux\n");
            return (void *) EXIT_FAILURE;
        }

        if (aacEncoder_SetParam(handle[i], AACENC_AFTERBURNER, afterburner) != AACENC_OK) {
            tvltx_info("Unable to set the afterburner mode\n");
            return (void *) EXIT_FAILURE;
        }

        if (aacEncoder_SetParam(handle[i], AACENC_HEADER_PERIOD, 10) != AACENC_OK) {
            tvltx_info("Unable to set the header period\n");
            return (void *) EXIT_FAILURE;
        }

        // 0: Implicit backward, 1: Explicit backward, 2: Explicit hierarchical
        if (aacEncoder_SetParam(handle[i], AACENC_SIGNALING_MODE, 2) != AACENC_OK) {
            tvltx_info("Unable to set the header period\n");
            return (void *) EXIT_FAILURE;
        }

        if (aacEncEncode(handle[i], NULL, NULL, NULL, NULL) != AACENC_OK) {
            tvltx_info("Unable to initialize the encoder\n");
            return (void *) EXIT_FAILURE;
        }

        if (aacEncInfo(handle[i], &info) != AACENC_OK) {
            tvltx_info("Unable to get the encoder info\n");
            return (void *) EXIT_FAILURE;
        }

        tvltx_debug("audio stream %d  frame_lenght = %d\n", i, info.frameLength);
        frame_lengths[i] = info.frameLength;
    }

    while (running){

        for (int i = 0; i < service->nr_audio_streams; i++) { // for each service
            int channels = service->nr_channels[i];
            int input_size = channels * frame_lengths[i] * 2;

            AACENC_BufDesc in_buf = { 0 }, out_buf = { 0 };
            AACENC_InArgs in_args = { 0 };
            AACENC_OutArgs out_args = { 0 };
            int in_identifier = IN_AUDIO_DATA;
            int in_size, in_elem_size;
            int out_identifier = OUT_BITSTREAM_DATA;
            int out_size, out_elem_size;
            void *in_ptr, *out_ptr;
            uint8_t outbuf[20480];
            AACENC_ERROR err;

            uint8_t input_buf[input_size];
            int16_t convert_buf[input_size/2];

            read_buffer(&service->buf_audio_in[i], input_buf, input_size);

            for (int j = 0; j < input_size/2; j++) {
                const uint8_t* in = &input_buf[2*j];
                convert_buf[j] = in[0] | (in[1] << 8);
            }

            in_ptr = convert_buf;
            in_size = input_size;
            in_elem_size = 2;

            in_args.numInSamples = input_size/2;
            in_buf.numBufs = 1;
            in_buf.bufs = &in_ptr;
            in_buf.bufferIdentifiers = &in_identifier;
            in_buf.bufSizes = &in_size;
            in_buf.bufElSizes = &in_elem_size;

            out_ptr = outbuf;
            out_size = sizeof(outbuf);
            out_elem_size = 1;
            out_buf.numBufs = 1;
            out_buf.bufs = &out_ptr;
            out_buf.bufferIdentifiers = &out_identifier;
            out_buf.bufSizes = &out_size;
            out_buf.bufElSizes = &out_elem_size;

            if ((err = aacEncEncode(handle[i], &in_buf, &out_buf, &in_args, &out_args)) != AACENC_OK) {
                if (err == AACENC_ENCODE_EOF){
                    tvltx_debug("AACENC EOF Error.\n");
                    break;
                }
                if (err == AACENC_INVALID_HANDLE){
                    tvltx_debug("AACENC AACENC_INVALID_HANDLE Error.\n");
                    break;
                }
                if (err == AACENC_MEMORY_ERROR){
                    tvltx_debug("AACENC AACENC_MEMORY_ERROR Error.\n");
                    break;
                }
                if (err == AACENC_UNSUPPORTED_PARAMETER){
                    tvltx_debug("AACENC AACENC_UNSUPPORTED_PARAMETER Error.\n");
                    break;
                }
                if (err == AACENC_INVALID_CONFIG){
                    tvltx_debug("AACENC AACENC_INVALID_CONFIG Error.\n");
                    break;
                }
                if (err == AACENC_ENCODE_ERROR){
                    tvltx_debug("AACENC AACENC_ENCODE_ERROR Error.\n");
                    break;
                }

                tvltx_debug("AAC Encoding failed\n");
                return (void *) EXIT_FAILURE;
            }
            if (out_args.numOutBytes == 0){
                tvltx_debug("AAC Encoder output bytes == 0\n");
                continue;
            }

            sample_count[i] += (uint64_t) in_args.numInSamples / channels;

            // create the PES header,  write header (with AAC PES frame size) and the PES
            uint8_t pes_header_f[32];
            uint8_t *pes_header = pes_header_f + sizeof(int); // to accomodate buffer_transfer_size before the actual PES header
            int pes_header_size;
            int pes_size;

            uint64_t pts;
            int es_size = out_args.numOutBytes;
            pts = (uint64_t) ((PTS_CLOCK * (sample_count[i] * (1.0/48000.0)) ) + 0 /* put offset for lipsync issues here */ ) & PTS_MASK;

            pes_get_header(pes_header, &pes_header_size, es_size,
                           1 /* with size*/, 0 /* no dts */, 0 /* no dts */, pts, PES_STREAM_ID_AUDIO_MPEG);

            pes_size = pes_header_size + es_size;
            memcpy(pes_header_f, &pes_size, sizeof(int));

            // write header with size and pes header
            write_buffer(&service->buf_audio_out[i], pes_header_f, pes_header_size + sizeof(int));

            // write AAC payload
            write_buffer(&service->buf_audio_out[i], outbuf, out_args.numOutBytes);
        }
    }

    tvltx_debug ("Exiting audio thread\n");
    for (int i = 0; i < service->nr_audio_streams; i++) {
        aacEncClose(&handle[i]);
    }

    return EXIT_SUCCESS;
}

void *encoder_video_helper_thread(void *helper_h)
{
    typedef struct {
        service_config *service;
        FILE *fifo;
    } helper_par;
    helper_par *helper = helper_h;
    service_config *service = helper->service;
    x264_header header;
    uint64_t dts, pts;
    int payload_size;
    // TODO: implement Priority Flag in PES header writer
    // int is_keyframe;
    uint8_t buffer[MAX_H264_F_SIZE];

    while (running) {
        int out = fread(&header, sizeof(x264_header), 1, helper->fifo);
        if (out != 1){
            tvltx_debug("Read from x264 fifo failed\n");
            return (void *) EXIT_FAILURE;
        }
        payload_size = header.payload_size;
        // tvltx_debug("payload size = %d\n", payload_size);
        // is_keyframe = header.is_keyframe;
        dts = (uint64_t) ((((long double) header.dts - 1) * ((long double) 90000.0)) + (long double) 0.5) & PTS_MASK; // +0.5 for better rounding
        pts = (uint64_t) ((((long double) header.pts - 1) * ((long double) 90000.0)) + (long double) 0.5) & PTS_MASK; // +0.5 for better rounding

        if (payload_size > MAX_H264_F_SIZE){
            tvltx_debug("H.264 frame size is bigger than buffer size! Increase-me!\n");
            return (void *) EXIT_FAILURE;
        }

        // tvltx_debug("video thread read payload_size %d\n", payload_size);
        out = fread(buffer, 1, payload_size, helper->fifo);
        if (out != payload_size){
            tvltx_debug("Read from x264 fifo failed out = %d payload = %d\n", out, payload_size);
            return (void *) EXIT_FAILURE;
        }

        // hexdump(buffer, payload_size, "after x264 fread\n");
        uint8_t pes_header_f[32];
        uint8_t *pes_header = pes_header_f + sizeof(int); // to accomodate buffer_transfer_size before the actual PES header
        int pes_header_size;


        if (service->is_oneseg == false)
            pes_get_header(pes_header, &pes_header_size, payload_size,
                           0 /* with size*/, 1 /* with dts */, dts, pts, PES_STREAM_ID_VIDEO_MPEG);
        else // is_oneseg == true
            pes_get_header(pes_header, &pes_header_size, payload_size,
                           0 /* with size*/, 0 /* without dts */, dts, pts, PES_STREAM_ID_VIDEO_MPEG);

        int pes_size = pes_header_size + payload_size;
        memcpy(pes_header_f, &pes_size, sizeof(int));

        // write header with size and pes header
        write_buffer(&service->buf_video_out, pes_header_f, pes_header_size + sizeof(int));

        // tvltx_debug("before write in  h.264 encoder to output buffer\n");
        // write H.264 payload
        write_buffer(&service->buf_video_out, buffer, payload_size);
    }

    return EXIT_SUCCESS;
}
