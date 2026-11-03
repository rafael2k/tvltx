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


#include <string.h>

#include "encoder_opts.h"
#include "macros.h"
#include "pes.h"

int pes_get_header(uint8_t *pes_header, int *pes_header_size, int es_size,
                   int set_size_field, int set_dts_field, uint64_t dts,
                   uint64_t pts, uint8_t stream_id) {

    pes_init(pes_header);
    pes_set_streamid(pes_header,  stream_id); // stream_id

    if (set_dts_field){
        if (set_size_field){
            pes_set_length(pes_header, es_size + PES_HEADER_SIZE_PTSDTS - PES_HEADER_SIZE);
        }
        else{
            pes_set_length(pes_header, 0);
        }
        pes_set_headerlength(pes_header, 10); // PTS + DTS length
        pes_set_dataalignment(pes_header);
        pes_set_pts(pes_header, pts);
        pes_set_dts(pes_header, dts);

        *pes_header_size = PES_HEADER_SIZE_PTSDTS;
    }
    else { // no dts
        if (set_size_field){
            pes_set_length(pes_header, es_size + PES_HEADER_SIZE_PTS - PES_HEADER_SIZE);
        }
        else{
            pes_set_length(pes_header, 0);
        }
        pes_set_headerlength(pes_header, 5); // PTS length
        pes_set_dataalignment(pes_header);
        pes_set_pts(pes_header, pts);

        *pes_header_size = PES_HEADER_SIZE_PTS;
    }

    return EXIT_SUCCESS;
}
