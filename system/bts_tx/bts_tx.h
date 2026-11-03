/*
    TVLTx - TV Lovers Transmitter, an ISDB-Tb transmitter
    Copyright (C) 2012-2018 Rafael Diniz

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

#include "buffer.h"
#include <pthread.h>

#define MAX_OUTPUTS 8

#define OUTPUT_NONE 0
#define OUTPUT_ASI 1
#define OUTPUT_MODULATION 2

typedef struct {
    int output_id;
    int card_number;
    int card_type;
    char freq[32];
    char power[32];
    int output_type;
    int bitrate;
    int packet_size;

    pthread_t           output_asi_TID;
    pthread_t           output_mod_TID;

    buffer buffer_out;

} output_parameters;

void *output_asi_thread_dektec(void *output_v);
void *output_mod_thread_dektec(void *output_v);

extern output_parameters mod_pars[MAX_OUTPUTS];
extern int nr_outputs;

#endif /* HAVE_BTS_TX__ */
