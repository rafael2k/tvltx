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
#include "output.h"
#include "hexdump.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

extern bool running;

int output_start(service_config *service)
{
    pthread_t          tid_v;
    pthread_t          tid_a;

    pthread_create(&tid_v, NULL, output_video_thread, (void *)service);
    pthread_create(&tid_a, NULL, output_audio_thread, (void *)service);

    return EXIT_SUCCESS;
}

void *output_video_thread(void *service_v)
{
   service_config *service = service_v;
   uint8_t *video_pes_buffer;
   int video_pes_shm_fd;

   char *video_pes_output_filename = service->video_shm_name;

   video_pes_shm_fd = shm_open(video_pes_output_filename, O_RDWR | O_CREAT, 0666 );
   if( video_pes_shm_fd == -1 ) {
       tvltx_debug("Video PES output shared memory creation failed!\n");
       return (void *) EXIT_FAILURE;
   }

   /* Set the memory object's size */
   if( ftruncate(video_pes_shm_fd, PES_SHM_MEMORY_SIZE) == -1 ) {
       tvltx_debug("Video PES output ftruncate failed!\n");
       return (void *) EXIT_FAILURE;
   }

   /* Map the memory object */
   video_pes_buffer = (uint8_t *) mmap( 0, PES_SHM_MEMORY_SIZE,
                                        PROT_READ | PROT_WRITE,
                                        MAP_SHARED, video_pes_shm_fd, 0 );

   if( video_pes_buffer == MAP_FAILED ) {
       tvltx_debug("Video PES mmap failed!\n");
       return (void *) EXIT_FAILURE;
   }

   // control = 1
   memset(video_pes_buffer, 1, 1);
   // size = 0
   memset(video_pes_buffer+1, 0, 4);

   while (running){
       int pes_size;
       uint8_t pes_buffer[PES_SHM_MEMORY_SIZE];

       // read header with size
       read_buffer(&service->buf_video_out, (uint8_t *) &pes_size, sizeof(int));
       // read the PES payload
//        tvltx_debug("before pes size read %d\n", pes_size);
       read_buffer(&service->buf_video_out, pes_buffer, pes_size);

       // hexdump(pes_buffer, pes_size, "after read_buffer in output.c\n");

       if (pes_size + 5 > PES_SHM_MEMORY_SIZE){
           tvltx_debug("PES SHM Output using insufficient sized buffer!\n");
           return (void *) EXIT_FAILURE;
       }

   video_pes_again:
       // if we can have control
       if (video_pes_buffer[0] == 1)
       {
           // tvltx_debug("video inside write to tm-muxer pes size=%d\n", pes_size);
           memcpy(video_pes_buffer + 1, &pes_size, sizeof(int));
           memcpy(video_pes_buffer + 5, pes_buffer, pes_size);
           // grant access to foreign
           memset(video_pes_buffer, 0, 1);
       }
       else
       {
           // tvltx_debug("video waiting inside write to tm-muxer pes size=%d\n", pes_size);
           usleep(10000);
           goto video_pes_again;
       }
   }

   return EXIT_SUCCESS;
}

void *output_audio_thread(void *service_a)
{
    service_config *service = service_a;
    uint8_t *audio_pes_buffer[service->nr_audio_streams];
    int audio_pes_shm_fd[service->nr_audio_streams];

    for (int i = 0; i < service->nr_audio_streams; i++) { // for each service
        char *audio_pes_output_filename = service->audio_shm_name[i];

        audio_pes_shm_fd[i] = shm_open(audio_pes_output_filename, O_RDWR | O_CREAT, 0666 );
        if( audio_pes_shm_fd[i] == -1 ) {
            tvltx_debug("Audio PES output shared memory creation failed!\n");
            return (void *) EXIT_FAILURE;
        }

        /* Set the memory object's size */
        if( ftruncate(audio_pes_shm_fd[i], PES_SHM_MEMORY_SIZE) == -1 ) {
            tvltx_debug("Audio PES output ftruncate failed!\n");
            return (void *) EXIT_FAILURE;
        }

        /* Map the memory object */
        audio_pes_buffer[i] = (uint8_t *) mmap( 0, PES_SHM_MEMORY_SIZE,
                                          PROT_READ | PROT_WRITE,
                                          MAP_SHARED, audio_pes_shm_fd[i], 0 );
        if( audio_pes_buffer[i] == MAP_FAILED ) {
            tvltx_debug("Audio PES mmap failed!\n");
            return (void *) EXIT_FAILURE;
        }

        // control = 1
        memset(audio_pes_buffer[i], 1, 1);
        // size = 0
        memset(audio_pes_buffer[i]+1, 0, 4);
    }

    while (running){
        for (int i = 0; i < service->nr_audio_streams; i++) {
            int pes_size;
            uint8_t pes_buffer[PES_SHM_MEMORY_SIZE];

            // tvltx_debug("before audio read in output\n");
            // read header with size
            read_buffer(&service->buf_audio_out[i], (uint8_t *) &pes_size, sizeof(int));
            // read the PES payload
            read_buffer(&service->buf_audio_out[i], pes_buffer, pes_size);

            if (pes_size + 5 > PES_SHM_MEMORY_SIZE){
                tvltx_debug("PES SHM Output using insufficient sized buffer!\n");
                return (void *) EXIT_FAILURE;
            }

            // if we have control of the buffer
        audio_pes_again:
            if (audio_pes_buffer[i][0] == 1)
            {
                // tvltx_debug("audio write in output pes size = %d\n", pes_size);
                memcpy(audio_pes_buffer[i] + 1, &pes_size, sizeof(int));
                memcpy(audio_pes_buffer[i] + 5, pes_buffer, pes_size);
                // grant access to foreign
                memset(audio_pes_buffer[i], 0, 1);
            }
            else
            {
                // tvltx_debug("audio wait write in output pes size %d\n", pes_size);
                usleep(10000);
                goto audio_pes_again;
            }
        }
    }

    return EXIT_SUCCESS;
}
