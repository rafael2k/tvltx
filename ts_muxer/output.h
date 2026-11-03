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


#ifndef HAVE_OUTPUT__
#define HAVE_OUTPUT__

#include "ring_buffer.h"
#include <pthread.h>

typedef struct {
  int mode;
  int guard_interval;
  int layer1_mod_type;
  int layer1_coderate;
  int layer1_time_interleave;
  int layer2_mod_type;
  int layer2_coderate;
  int layer2_time_interleave;
  int layer3_mod_type;
  int layer3_coderate;
  int layer3_time_interleave;
  /* TODO: ISDB-Tb layer3 support */  
  FILE *config_file;
} modulation_parameters;

modulation_parameters mod_pars;

pthread_t           output_asi_TID;
pthread_t           output_mod_TID;

pthread_mutex_t     output_mutex;
pthread_cond_t      output_cond;
struct ring_buffer  output_buffer;

void *output_asi_thread_dektec(void *nothing);
void *output_mod_thread_dektec(void *nothing);


#endif /* HAVE_OUTPUT__ */
