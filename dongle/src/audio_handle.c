#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net_buf.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_audio.h>

#include "audio_handle.h"
#include "esb_handle.h"
#include "drv_flash.h"

LOG_MODULE_DECLARE(smart_dongle, CONFIG_ESB_PRX_APP_LOG_LEVEL);


#define AUDIO_HANDLE_STACK_SIZE        2048
#define AUDIO_HANDLE_PRIORITY          3


NET_BUF_POOL_FIXED_DEFINE(pool_out, CONFIG_FIFO_FRAME_SPLIT_NUM, USB_FRAME_SIZE_STEREO, 8, net_buf_destroy);

static const struct device *const mic_dev = DEVICE_DT_GET_ONE(usb_audio_mic);

extern struct k_msgq esb_queue1;
extern struct k_msgq esb_queue2;

extern struct k_sem esb_sem;


extern int leds_toggle(uint8_t idx);




/*
* single channel transform to dual channels.
*
* @param[in] src_audio1 	Pointer to the single channel source pcm stream.
* @param[in] src_audio2 	Pointer to the second single channel source pcm stream (if needed).
*			 frames	   		Length of the stram buffer
*			 dst_audio		Pointer to the target dual channel pcm stream
*/
static void mono_to_stereo(int16_t* src_audio1, int16_t* src_audio2, int frames, int16_t* dst_audio) 
{
    for (int i = 0; i < frames; i++) 
    {
        dst_audio[2 * i] = src_audio1[i];
        dst_audio[2 * i + 1] = src_audio2[i];
    }
}


static void handle_audio_data(const struct device *dev)
{
	// LOG_INF("data were requested from the device and may be send to the Host!");
	static uint32_t timeCount = 0;

	if(0 == (timeCount++ % 50))
		leds_toggle(2);

    int ret = 0;
    void *frame_buffer1 = NULL;
	void *frame_buffer2 = NULL;
    size_t data_out_size = 0;
     
    struct net_buf *buf_out;

	buf_out = net_buf_alloc(&pool_out, K_NO_WAIT);
	if (!buf_out) 
	{
		LOG_ERR("Failed to allocate data buffer");
		// return;
	}

    if(k_msgq_get(&esb_queue1, &frame_buffer1, K_NO_WAIT) != 0)
    {
        // LOG_WRN("USB audio TX underrun");
		// net_buf_unref(buf_out);
		// return;
    }
	if(k_msgq_get(&esb_queue2, &frame_buffer2, K_NO_WAIT) != 0)
    {

        // LOG_WRN("USB audio TX underrun");
		// net_buf_unref(buf_out);
		// return;
    }
   
    // LOG_HEXDUMP_INF(frame_buffer, 8, "Receive audio queue");
	if(frame_buffer1 && frame_buffer2)
	{
		mono_to_stereo((int16_t*) frame_buffer1, (int16_t*) frame_buffer2, MAX_BLOCK_SIZE, (int16_t*)buf_out->data);
		/** free the memory slab */
		free_esb_slab_memory(frame_buffer1);	
		free_esb_slab_memory(frame_buffer2);
	}
	else if(frame_buffer1)
	{
		mono_to_stereo((int16_t*) frame_buffer1, (int16_t*) frame_buffer1, MAX_BLOCK_SIZE, (int16_t*)buf_out->data);
		free_esb_slab_memory(frame_buffer1);	
	}
	else if(frame_buffer2)
	{
		// LOG_HEXDUMP_INF(frame_buffer2, 8, "Receive audio queue");
		mono_to_stereo((int16_t*) frame_buffer2, (int16_t*) frame_buffer2, MAX_BLOCK_SIZE, (int16_t*)buf_out->data);
		free_esb_slab_memory(frame_buffer2);	
	}
	else
	{
		LOG_ERR("Both audio buffers are NULL");
		net_buf_unref(buf_out);
		return;
	}

	data_out_size =  buf_out->size;
	// LOG_INF("USB audio TX data size: %d", data_out_size);
#if 1
	 /** USB audio driver handle the pcm stream*/
	if (data_out_size == usb_audio_get_in_frame_size(dev)) 
	{
		ret = usb_audio_send(dev, buf_out, data_out_size);
		if (ret) {
			LOG_WRN("USB TX failed, ret: %d", ret);
			net_buf_unref(buf_out);
		}
		// else
		// {	
		// 	LOG_INF("usb audio send %d bytes succeed!\t", data_out_size);
		// }
	} 
#else
	if (data_out_size == FLASH_PAGE_SIZE) 
    {
		ret = soc_flash_write(total_size, buf_out->data, data_out_size);
		if (ret) {
			LOG_WRN("write flash failed, ret: %d", ret);
			net_buf_unref(buf_out);
		}
		// else
		// {	
		// 	LOG_INF("usb audio send %d bytes succeed!\t", data_out_size);
		// }
	} 
#endif
    else 
    {
		LOG_WRN("Wrong size write: %d", data_out_size);
	}
}


static void feature_update(const struct device *dev,
			   const struct usb_audio_fu_evt *evt)
{
	LOG_DBG("Control selector %d for channel %d updated",
		evt->cs, evt->channel);
	switch (evt->cs) {
	case USB_AUDIO_FU_MUTE_CONTROL:
	default:
		break;
	}
}



static const struct usb_audio_ops mic_ops = {
	.data_request_cb = handle_audio_data,
	.feature_update_cb = feature_update,
};



static void esb_audio_data_handle(void *, void *, void *)
{
	int ret;
	// soc_flash_init();

	if (!device_is_ready(mic_dev)) {
		LOG_ERR("Device USB Microphone is not ready");
		return;
	}
	LOG_INF("Found USB Microphone Device");

	usb_audio_register(mic_dev, &mic_ops);

	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return;
	}

	LOG_INF("USB enabled");
	LOG_INF("mic_frame_size = %d\t ", usb_audio_get_in_frame_size(mic_dev));

    while(1)
    {
        // k_sem_take(&esb_sem, K_FOREVER);
		esb_buffer_handle();
    }
}


K_THREAD_DEFINE(esb_audio_service, AUDIO_HANDLE_STACK_SIZE,
                esb_audio_data_handle, NULL, NULL, NULL,
                K_PRIO_PREEMPT(AUDIO_HANDLE_PRIORITY), 0, 0);




