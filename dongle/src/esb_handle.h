#ifndef __ESB_HANDLE_H__
#define __ESB_HANDLE_H__

#include <stdint.h>


#define MAX_SAMPLE_RATE             16000
#define SAMPLE_BIT_WIDTH            16
#define BYTES_PER_SAMPLE            sizeof(int16_t)

/* Size of a block for 10 ms of audio data. */
#define BLOCK_SIZE(_sample_rate, _number_of_ms) \
	(BYTES_PER_SAMPLE * (_sample_rate / 1000) * _number_of_ms)

#define MAX_BLOCK_SIZE              BLOCK_SIZE(MAX_SAMPLE_RATE, 1)

#define ADPCM_BLOCK_SIZE			(MAX_BLOCK_SIZE/4 + 3)

#define ESB_BLOCK_SIZE              (MAX_BLOCK_SIZE)
#define ESB_BLOCK_COUNT             600


int esb_initialize(void);

void esb_buffer_handle(void);

void free_esb_slab_memory(void *buffer);

#endif
