/*
    TVLTx - TV Lovers Transmitter, a BTS ISDB-Tb transmitter
    Copyright (C) 2012-2015 Rafael Diniz

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
#include <time.h>
#include <signal.h>

#include "bts_tx.h"

#include "ring_buffer.h"

#define TS_PACKET_SIZE 204
#define PACKETS_PER_BUFFER 20
#define BUFFER_SIZE 4080 // PACKETS_PER_BUFFER * TS_PACKET_SIZE

#define START_DEBUG_ENABLED 0 // when in debug mode, start with ts output enabled

int debug_enabled = START_DEBUG_ENABLED; 

#if (WRITE_BUFFER_STATUS == 1)
int start_shm()
{
    shm_fd = shm_open("dektec_status", O_RDWR | O_CREAT, 0666 );
    if( shm_fd == -1 ) {
	fprintf(stderr, "Failed to create SHM!\n");
	return EXIT_FAILURE;
    }

    /* Set the memory object's size */
    if( ftruncate(shm_fd, SHM_MEMORY_SIZE) == -1 ) {
	fprintf(stderr, "ftruncate() failed!\n");
	return EXIT_FAILURE;
    }

    /* Map the memory object */
    shm_buffer = (uint8_t *) mmap( 0, SHM_MEMORY_SIZE,
					 PROT_READ | PROT_WRITE,
					 MAP_SHARED, shm_fd, 0 );
    if( shm_buffer == MAP_FAILED ) {
	fprintf(stderr, "mmap failed!\n");
	return EXIT_FAILURE;
    }

    // control = 1
    memset(shm_buffer, 1, 1);
    // size = 0
    memset(shm_buffer+1, 0, 4);

    return EXIT_SUCCESS;
}

// return 1 when more data needs to be received, -1 less data, 0 means it's all good 
int8_t buffer_control(int buffer_size)
{
    static int pass = 0;
    static int min_threshold, max_threshold, mean_value;
    int diff;

    if (pass == 0)
    {
	min_threshold = buffer_size;
	pass++;
	return 0;
    }
    
    if (pass == 1)
    {
	if (min_threshold > buffer_size)
	{
	    max_threshold = min_threshold;
	    min_threshold = buffer_size;
	}
	else
	{
	    max_threshold = buffer_size;
	}
	mean_value = (max_threshold + min_threshold) / 2;
	pass++;
	fprintf(stderr, "BUFFER CONTROL MEAN VALUE: %d  MAX BUFFER DRIFT ALLOWED: %d\n", mean_value, MAX_BUFFER_DRIFT);
	return 0;
    } 
    
    if (pass == 2)
    {
	if (mean_value > buffer_size)
	{
	    diff = mean_value - buffer_size;
	    if (diff > MAX_BUFFER_DRIFT)
		return 1;
	}
	else
	{
	    diff = buffer_size - mean_value;

	    // TODO: NOT YET IMPLEMENTED IN MUXER
	    //if (diff > MAX_BUFFER_DRIFT)
	    //    return -1;
	    return 0;
	}
    }

    return 0;
    
}
#endif

static void catch_function(int signo)
{
    pthread_mutex_lock(&debug_mutex);

    if (debug_enabled == 0)
    {
	time_t tm = time(NULL);
	char filename[256];
	sprintf(filename, "monitor-%llu.ts", tm); 
	if ( (debug_output = fopen(filename,"w")) == NULL )
	{
	    fprintf(stderr,"error opening %s\n", filename);
	}
	else
	{
	    fprintf(stderr, "                                     Writing debug to: %s\n", filename);
	    debug_enabled = 1;
	}
    }
    else
    {
	debug_enabled = 0;
	fclose(debug_output);
    }
    pthread_mutex_unlock(&debug_mutex);    
}

int main(int argc, char **argv, char **envp)
{
    char input_bts[512];
    char output_param[512];
    output_param[0] = 0;
    
    void *addr;
    FILE *stream; 
    FILE *output = NULL;
    uint8_t *buffer_ts;
    int debug_mode = 0;
    
    if (argc < 4) {
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage for Modulation: %s -i <stream.bts> -t MOD -f <frequency> -p <power> [-o <local_output.bts>]\n", argv[0]);
	fprintf(stderr, "Usage for ASI output: %s -i <stream.bts> -t ASI [-o <local_output.bts]>\n", argv[0]);	
	fprintf(stderr, "\n");
	fprintf(stderr, "License: GNU Affero General Public License version 3 or (at your option) any later version.\n");
	fprintf(stderr, "Copyright (C) 2012-2015 Rafael Diniz\n");
	fprintf(stderr, "\n");
	return EXIT_FAILURE;
    }

    int opt;
    while ((opt = getopt(argc, argv, "i:t:f:p:o:")) != -1)
    {
	switch (opt)
	{
	case 'i':
	    strcpy(input_bts, optarg);
	    break;
	case 't':
	    if (!strcmp("ASI", optarg))
		mod_pars.output_type = OUTPUT_ASI;
	    if (!strcmp("MOD", optarg))
		mod_pars.output_type = OUTPUT_MODULATION;
	    break;
	case 'f':
	    strcpy(mod_pars.freq, optarg);
	    break;
	case 'p':
	    strcpy(mod_pars.power, optarg);
	    break;
	case 'o':
	    strcpy(output_param, optarg);
	    break;
	default:
	    fprintf(stderr, "Input parameter error: %s\n", optarg);
	    return EXIT_FAILURE;	    
	}
    }

#if (WRITE_BUFFER_STATUS == 1)
    start_shm();
#endif
    
    buffer_ts = (uint8_t *) malloc(BUFFER_SIZE);
    
    if ( (stream = fopen(input_bts,"r")) == NULL ) {
        fprintf(stderr, "error opening %s\n", input_bts);
	return EXIT_FAILURE;
    }

    if (output_param[0] != 0) {
	if (!strcmp("DEBUG", output_param))
	{
	    debug_mode = 1;
	    pthread_mutex_init(&debug_mutex, NULL);
	    if (signal(SIGUSR1, catch_function) == SIG_ERR) {
		fputs("An error occurred while setting a signal handler.\n", stderr);
		return EXIT_FAILURE;
	    }
	    if (debug_enabled)
	    {
		time_t tm = time(NULL);
		char filename[256];
		sprintf(filename, "monitor-%llu.ts", tm); 
		if ( (debug_output = fopen(filename,"w")) == NULL )
		{
		    fprintf(stderr,"error opening %s\n", filename);
		    return EXIT_FAILURE;
		}
		fprintf(stderr, "                                     Writing debug to: %s\n", filename);
	    }
	}
	else
	{
	    if ( (output = fopen(output_param,"w")) == NULL )
	    {
		fprintf(stderr,"error opening %s\n", output_param);
		return EXIT_FAILURE;
	    }
	    fprintf(stderr, "Local output to %s enabled!\n", output_param);
	}
    }

    // the output buffer stuff
    ring_buffer_create(&output_buffer, 28);
    pthread_mutex_init(&output_mutex, NULL);
    pthread_cond_init(&output_cond, NULL);

    if (mod_pars.output_type == OUTPUT_ASI)
    {
	if(pthread_create(&output_asi_TID, NULL, output_asi_thread_dektec, NULL))
	{
	    fprintf(stderr,"Failed to create Dektec thread.\n");
	    return EXIT_FAILURE;
	}
	
    }
    
    if (mod_pars.output_type == OUTPUT_MODULATION)
    {
	if(pthread_create(&output_mod_TID, NULL, output_mod_thread_dektec, NULL))
	{
	    fprintf(stderr,"Failed to create Dektec thread.\n");
	    return EXIT_FAILURE;
	}
    }

    while (1) {

        // printf("before read!\n");
      
	if (fread(buffer_ts, TS_PACKET_SIZE, PACKETS_PER_BUFFER, stream) != PACKETS_PER_BUFFER) {
	    printf("error in fread of stream\n");
	    return EXIT_FAILURE;
	}

	// printf("After read!\n");

	// escrever a saida..
    try_again_write_to_output:
	if(ring_buffer_count_free_bytes(&output_buffer) >= BUFFER_SIZE) {
	  // printf("Before lock!\n");
	    pthread_mutex_lock(&output_mutex);
	    addr = ring_buffer_write_address(&output_buffer);
	    memcpy(addr, buffer_ts, BUFFER_SIZE);
	    ring_buffer_write_advance(&output_buffer, BUFFER_SIZE);
	    pthread_cond_signal(&output_cond);
	    pthread_mutex_unlock(&output_mutex);
	    //  printf("writting to buffer\n");
	}
	else {
	    pthread_mutex_lock( &output_mutex );
	    pthread_cond_wait(&output_cond, &output_mutex);   
	    pthread_mutex_unlock( &output_mutex );
	    goto try_again_write_to_output;
	}

	if (debug_mode)
	{
	    if (debug_enabled)
	    {
		pthread_mutex_lock(&debug_mutex);
		fwrite(buffer_ts, TS_PACKET_SIZE, PACKETS_PER_BUFFER, debug_output);
		pthread_mutex_unlock(&debug_mutex);
	    }
	}
	
	if (output != NULL)
	{
	    fwrite(buffer_ts, TS_PACKET_SIZE, PACKETS_PER_BUFFER, output);
	}
      
    } // while (1)
  
  
    return EXIT_SUCCESS;
}

