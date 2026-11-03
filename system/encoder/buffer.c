/*
    TVLTx - TV Lovers Encoder
    Copyright (C) 2012-2017 SDR Telecom

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

#include "buffer.h"

inline void read_buffer(buffer *buf_in, uint8_t *buffer_out, int size) {
    void *addr;

try_again_read:
    if ( ring_buffer_count_bytes( &buf_in->buf ) >= size ) {
        pthread_mutex_lock( &buf_in->mutex );
        addr = ring_buffer_read_address( &buf_in->buf );
        memcpy( buffer_out, addr, size );
        ring_buffer_read_advance( &buf_in->buf, size );
        pthread_cond_signal( &buf_in->cond );
        pthread_mutex_unlock( &buf_in->mutex );
    }
    else {
        pthread_mutex_lock( &buf_in->mutex );
        pthread_cond_wait( &buf_in->cond, &buf_in->mutex );
        pthread_mutex_unlock( &buf_in->mutex );
        goto try_again_read;
    }


}

inline void write_buffer(buffer *buf_out, uint8_t *buffer_in, int size) {
    void *addr;

try_again_write:
    if ( ring_buffer_count_free_bytes ( &buf_out->buf ) >= size){
            pthread_mutex_lock( &buf_out->mutex );
            addr = ring_buffer_write_address( &buf_out->buf );
            memcpy( addr, buffer_in, size );
            ring_buffer_write_advance ( &buf_out->buf, size );
            pthread_cond_signal( &buf_out->cond );
            pthread_mutex_unlock( &buf_out->mutex );
        }
        else {
            pthread_mutex_lock( &buf_out->mutex );
            pthread_cond_wait( &buf_out->cond, &buf_out->mutex );
            pthread_mutex_unlock( &buf_out->mutex );
            goto try_again_write;
        }
}

void initialize_buffer(buffer *buf, int mag) // size is 2^mag
{
    pthread_mutex_init( &buf->mutex, NULL );
    pthread_cond_init( &buf->cond, NULL );
    ring_buffer_create( &buf->buf, mag );
}
