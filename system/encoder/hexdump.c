/*
    TVLTx - TV Lovers Encoder
    Copyright (C) 2017 SDR Telecom

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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <pthread.h>

#include "macros.h"

/* number of bytes per line */
#define LINE_WIDTH 16

void hexdump(uint8_t *data, uint32_t nbytes, char *string)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    uint32_t nout;
    int i;
    pthread_mutex_lock(&mutex);

    tvltx_debug(string);

    nout = 0;
    while(nout < nbytes)
    {
        /* byte offset at start of line */
        if((nout % LINE_WIDTH) == 0)
            tvltx_debug("0x%.8x  ", nout);
        /* the byte value in hex */
        tvltx_debug("%.2x ", data[nout]);
        /* the ascii equivalent at the end of the line */
        if((nout % LINE_WIDTH) == (LINE_WIDTH - 1))
        {
            tvltx_debug(" ");
            for(i = LINE_WIDTH-1; i >= 0; i--)
                tvltx_debug("%c", isprint(data[nout - i]) ? data[nout - i] : '.');
            tvltx_debug("\n");
        }
        nout++;
    }

    /* the ascii equivalent if we haven't just done it */
    if((nout % LINE_WIDTH) != 0)
    {
        /* pad to the start of the ascii equivalent */
        for(i=(nout % LINE_WIDTH); i < LINE_WIDTH; i++)
            tvltx_debug("   ");
        tvltx_debug(" ");
        /* print the ascii equivalent */
        nout--;
        for(i=(nout % LINE_WIDTH); i>=0; i--)
            tvltx_debug("%c", isprint(data[nout - i]) ? data[nout - i] : '.');
        tvltx_debug("\n");
    }

    pthread_mutex_unlock(&mutex);

    return;
}
