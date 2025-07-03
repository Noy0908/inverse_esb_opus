#ifndef __USB_AUDIO_APP_H__
#define __USB_AUDIO_APP_H__

#include <stdint.h>


#define MAX_SAMPLE_RATE             16000
#define SAMPLE_BIT_WIDTH            16
#define BYTES_PER_SAMPLE            sizeof(int16_t)

// #define CONFIG_AUDIO_SAMPLE_RATE_HZ         16000
// #define CONFIG_AUDIO_BIT_DEPTH_OCTETS       2

#define USB_FRAME_SIZE_STEREO               (((MAX_SAMPLE_RATE * BYTES_PER_SAMPLE) / 1000) *2)

#define CONFIG_FIFO_FRAME_SPLIT_NUM         100




/* Size of a block for 10 ms of audio data. */
#define BLOCK_SIZE(_sample_rate, _number_of_ms) \
	(BYTES_PER_SAMPLE * (_sample_rate / 1000) * _number_of_ms)

#define MAX_BLOCK_SIZE              (BLOCK_SIZE(MAX_SAMPLE_RATE, 1) * 5)	// 10 ms

#define PCM_BLOCK_SIZE				BLOCK_SIZE(MAX_SAMPLE_RATE, 1) 			// 1 ms
#define FRAME_SIZE                  (MAX_SAMPLE_RATE / 1000)

#define ESB_BLOCK_COUNT             10
#define PCM_BLOCK_COUNT             100



#endif






