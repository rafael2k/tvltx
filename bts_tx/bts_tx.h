/*
    TVLTx - TV Lovers Transmitter, a MPEG2-TS cbr simple muxer and ISDB-Tb transmitter
    Copyright (C) 2012 Rafael Diniz

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


#ifndef HAVE_BTS_TX__
#define HAVE_BTS_TX__

#include "ring_buffer.h"
#include <pthread.h>

// set to 1 to write buffer status to shm
#define WRITE_BUFFER_STATUS 1

#define OUTPUT_NONE 0
#define OUTPUT_ASI 1
#define OUTPUT_MODULATION 2

#if (WRITE_BUFFER_STATUS == 1)
#define SHM_MEMORY_SIZE 6
#define MAX_BUFFER_DRIFT 3000000

int shm_fd;
uint8_t *shm_buffer;
int8_t buffer_control(int buffer_size);
#endif

typedef struct {
    char freq[32];
    char power[32];
    int output_type;
} modulation_parameters;

modulation_parameters mod_pars;

pthread_t           output_asi_TID;
pthread_t           output_mod_TID;

pthread_mutex_t     output_mutex;
pthread_cond_t      output_cond;
struct ring_buffer  output_buffer;

FILE                *debug_output;
pthread_mutex_t     debug_mutex;

void *output_asi_thread_dektec(void *nothing);
void *output_mod_thread_dektec(void *nothing);


#endif /* HAVE_BTS_TX__ */
