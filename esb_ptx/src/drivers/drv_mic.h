#ifndef __DRV_MIC_H__
#define __DRV_MIC_H__

#include <stdint.h>

#define MAX_SAMPLE_RATE     16000
#define SAMPLE_BIT_WIDTH    16
#define BYTES_PER_SAMPLE    sizeof(int16_t)

/* Size of a block for 3 ms of audio data. */
#define BLOCK_SIZE(_sample_rate, _number_of_channels) \
	(BYTES_PER_SAMPLE * (_sample_rate / 1000) * _number_of_channels)

#define MAX_BLOCK_SIZE      BLOCK_SIZE(MAX_SAMPLE_RATE, 1)

#define BLOCK_COUNT         60

/**@brief Compressed audio frame representation.
 */

int drv_audio_init(void);

int drv_mic_start(void);

int drv_mic_stop(void);

uint32_t read_audio_data(void **buffer, int32_t timeout);

void free_audio_memory(void *buffer);

int test_pdm_transfer(size_t block_count);


#endif