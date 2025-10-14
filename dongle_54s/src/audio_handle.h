
#ifndef __USB_AUDIO_APP_H__
#define __USB_AUDIO_APP_H__

#include <stdint.h>
#include "opus_application_config.h"


#define MAX_SAMPLE_RATE             16000
#define SAMPLE_BIT_WIDTH            16
#define BYTES_PER_SAMPLE            sizeof(int16_t)


#define USB_FRAME_SIZE_STEREO               (((MAX_SAMPLE_RATE * BYTES_PER_SAMPLE) / 1000) *2)

#define CONFIG_FIFO_FRAME_SPLIT_NUM         100




/* Size of a block for 1 ms of audio data. */
#define BLOCK_SIZE(_sample_rate, _number_of_ms) \
	(BYTES_PER_SAMPLE * (_sample_rate / 1000) * _number_of_ms)


#define PCM_BLOCK_SIZE				BLOCK_SIZE(MAX_SAMPLE_RATE, 1) 			// 1 ms
#define FRAME_SIZE                  (MAX_SAMPLE_RATE / 1000)
#define PCM_FRAME_BYTES 			(CONFIG_AUDIO_FRAME_SIZE_SAMPLES * CONFIG_OPUS_CHANNELS)

#define PCM_BLOCK_COUNT             100



#endif
