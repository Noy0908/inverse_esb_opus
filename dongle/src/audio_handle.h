#ifndef __USB_AUDIO_APP_H__
#define __USB_AUDIO_APP_H__

#include <stdint.h>


#define CONFIG_AUDIO_SAMPLE_RATE_HZ         16000
#define CONFIG_AUDIO_BIT_DEPTH_OCTETS       2

#define USB_FRAME_SIZE_STEREO               (((CONFIG_AUDIO_SAMPLE_RATE_HZ * CONFIG_AUDIO_BIT_DEPTH_OCTETS) / 1000) *2)

#define CONFIG_FIFO_FRAME_SPLIT_NUM         10



void usb_audio_init(void);


#endif






