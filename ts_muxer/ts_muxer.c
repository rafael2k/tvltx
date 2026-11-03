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

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "output.h"

#include "ring_buffer.h"

#define TS_PACKET_SIZE 188
#define MAX_STREAMS 8

#define STUFFING_PID 0x1FFF

int has_payload(uint8_t *buffer) {
  
  if (buffer[1] == 0x1f && buffer[2] == 0xff)
    return 0;
  else
    return 1;

}

int main(int argc, char **argv, char **envp)
{
  int number_of_streams = 0;
  FILE *stream[8]; 
  FILE *output = NULL;
  int multiplex_frame_size = 0;
  uint8_t *buffer_ts;
  uint8_t *mpx_frame;
  uint8_t *ocupied_packets_mpx_frame;
  static uint8_t stuffing_packet[TS_PACKET_SIZE];
  

  if (argc < 6) {
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: %s number_of_streams <stream1.ts> <stream2.ts> ... <multiplex_frame_size> <multiplex_config.txt> <local_output.ts>\n", argv[0]);
    fprintf(stderr, "Example without local output:\n");
    fprintf(stderr, "%s 2 /tmp/stream-ld.ts /tmp/stream-hd.ts 4352 /etc/tvltx.cfg\n", argv[0]);
    fprintf(stderr, "Example with local output:\n");
    fprintf(stderr, "%s 2 /tmp/stream-ld.ts /tmp/stream-hd.ts 4352 /etc/tvltx.cfg monitor.ts\n", argv[0]);
    fprintf(stderr, "Max number of streams: %d\n", MAX_STREAMS);
    fprintf(stderr, "\n");
    fprintf(stderr, "License: GNU Affero General Public License version 3 or (at your option) any later version.: %d\n", MAX_STREAMS);
    fprintf(stderr, "Copyright (C) 2012-2015 Rafael Diniz\n");
    fprintf(stderr, "\n");
    return EXIT_FAILURE;
  }
  
  number_of_streams = atoi(argv[1]);
  multiplex_frame_size = atoi(argv[2 + number_of_streams]);

  fprintf(stderr, "Multiplex size = %d TS packets.\n", multiplex_frame_size);

  buffer_ts = (uint8_t *) malloc(TS_PACKET_SIZE * multiplex_frame_size);
  mpx_frame = (uint8_t *) malloc(TS_PACKET_SIZE * multiplex_frame_size);
  ocupied_packets_mpx_frame = (uint8_t *) malloc(multiplex_frame_size);


  for (int i = 0; i < number_of_streams; i++){
    int argv_pos = i + 2;

    if ( (stream[i] = fopen(argv[argv_pos],"r")) == NULL ) {
      printf("error opening %s\n", argv[argv_pos]);
      return EXIT_FAILURE;
    }
  }

  if ( (mod_pars.config_file = fopen(argv[number_of_streams + 3],"r")) == NULL ){
    fprintf(stderr, "Multiplex config file could not be opened - aborting.\n");
    return EXIT_FAILURE;
  }
  
  if (argc > number_of_streams + 4) {
    if ( (output = fopen(argv[number_of_streams + 4],"w")) == NULL ) {
      printf("error opening %s\n", argv[number_of_streams + 2]);
      return EXIT_FAILURE;
    }
    printf("Local output enabled!\n");
  }

  // the output buffer stuff
  ring_buffer_create(&output_buffer, 28);
  pthread_mutex_init(&output_mutex, NULL);
  pthread_cond_init(&output_cond, NULL);

  if(pthread_create(&output_mod_TID, NULL, output_mod_thread_dektec, NULL)) {
    printf("Failed to create Dektec thread.\n");
    return EXIT_FAILURE;
  }

  
  // fill our stuffing packet
  stuffing_packet[0] = 0x47;
  stuffing_packet[1] = STUFFING_PID >> 8; 
  stuffing_packet[2] = STUFFING_PID & 0xff; 
  stuffing_packet[3] = 0x10; 
  stuffing_packet[4] = 183; 
  stuffing_packet[5] = 0x00; 

  memset (stuffing_packet + 6, 0xff, TS_PACKET_SIZE - 6);
  
  while (1) {
    
    // no packets taken in the mpx frame
    memset(ocupied_packets_mpx_frame, 0, multiplex_frame_size);
    
    // place stuffing packets
    for (int k = 0; k < multiplex_frame_size; k++) { 
      memcpy(&mpx_frame[TS_PACKET_SIZE * k], stuffing_packet, TS_PACKET_SIZE);
    }
    
    // the multiplex!
    for ( int i = 0; i < number_of_streams; i++ ) {
      // printf("stream number: %d\n", i);

      if (fread(buffer_ts, TS_PACKET_SIZE, multiplex_frame_size, stream[i]) != multiplex_frame_size) {
	printf("error in fread of stream %d\n", i);
	return EXIT_FAILURE;
      }
      
      int packet_index;
      for (int j = 0; j < multiplex_frame_size; j++ ) {
	packet_index = j;
	
	if ( has_payload( &buffer_ts[j * TS_PACKET_SIZE] ) ) {

	  while( ocupied_packets_mpx_frame[packet_index] == 1) 
	    packet_index++;
	 
	  /* For debug purposes... */
	  //if (j != packet_index)
	  //  printf("original pkt location %d, multiplex location %d\n", j, packet_index);
	  
	  ocupied_packets_mpx_frame[packet_index] = 1;
	  memcpy(&mpx_frame[packet_index * TS_PACKET_SIZE], &buffer_ts[j * TS_PACKET_SIZE], TS_PACKET_SIZE);
	  
	}
	else {
	  // printf("stuffing input at %d\n", j);
	}
      } // for (int j = 0; j < multiplex_frame_size; j++ )		
    } // for ( int i = 0; i < number_of_streams; i++ ) 
    
    // inserir as tabelas?    

    void *addr;
    int multiplex_size = TS_PACKET_SIZE * multiplex_frame_size;

    // escrever a saida..
  try_again_write_to_output:
    if(ring_buffer_count_free_bytes(&output_buffer) >= multiplex_size) {
      pthread_mutex_lock(&output_mutex);
      addr = ring_buffer_write_address(&output_buffer);
      memcpy(addr, mpx_frame, multiplex_size);
      ring_buffer_write_advance(&output_buffer, multiplex_size);
      pthread_cond_signal(&output_cond);
      pthread_mutex_unlock(&output_mutex);
    }
    else {
      pthread_mutex_lock( &output_mutex );
      pthread_cond_wait(&output_cond, &output_mutex);   
      pthread_mutex_unlock( &output_mutex );
      goto try_again_write_to_output;
    }

    if (output != NULL)
      fwrite(mpx_frame, TS_PACKET_SIZE, multiplex_frame_size, output);

  } // while (1)
  

  return EXIT_SUCCESS;
}

