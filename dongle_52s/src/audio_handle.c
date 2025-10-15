#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net_buf.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_audio.h>
#include <pcm_mix.h>

#include "audio_handle.h"
#include "esb_handle.h"


LOG_MODULE_DECLARE(smart_dongle, CONFIG_ESB_PRX_APP_LOG_LEVEL);


#define AUDIO_HANDLE_STACK_SIZE        	30720
#define AUDIO_HANDLE_PRIORITY          	3

#define OPUS_DECODER_SIZE   			17944


NET_BUF_POOL_FIXED_DEFINE(pool_out, CONFIG_FIFO_FRAME_SPLIT_NUM, USB_FRAME_SIZE_STEREO, 8, net_buf_destroy);

K_MSGQ_DEFINE(esb_queue1, PCM_BLOCK_SIZE, PCM_BLOCK_COUNT, 4);
K_MSGQ_DEFINE(esb_queue2, PCM_BLOCK_SIZE, PCM_BLOCK_COUNT, 4);

static const struct device *const mic_dev = DEVICE_DT_GET_ONE(usb_audio_mic);

/******************************** opus decoder variables ******************************************/
static uint8_t m_opus_channels   = CONFIG_OPUS_CHANNELS;
__ALIGN(4) static uint8_t m_opus_decoder[OPUS_DECODER_SIZE];
static OpusDecoder * const m_opus_decoder_state = (OpusDecoder *)m_opus_decoder;

/** this pointer variable used for transport the message queue to USB audio thread.*/
// void *block_ptr = NULL;

extern struct k_msgq m_msgq_rx_payloads;

extern int leds_toggle(uint8_t idx);



static void handle_audio_data(const struct device *dev)
{
	// LOG_INF("data were requested from the device and may be send to the Host!");
	static uint32_t timeCount = 0;

    int ret = 0;
	volatile bool channel1_flag = false;
	volatile bool channel2_flag = false;
	// int16_t frame_buffer1[FRAME_SIZE] = {0};
	// int16_t frame_buffer2[FRAME_SIZE] = {0};
    size_t data_out_size = 0;
     
    struct net_buf *buf_out;

	buf_out = net_buf_alloc(&pool_out, K_NO_WAIT);
	if (!buf_out) 
	{
		LOG_ERR("Failed to allocate data buffer");
		// return;
	}

    if(k_msgq_get(&esb_queue1, (int16_t*)buf_out->data, K_NO_WAIT) == 0)
    {
        channel1_flag = true;
    }
	else if(k_msgq_get(&esb_queue2, (int16_t*)buf_out->data, K_NO_WAIT) == 0)
    {
        channel2_flag = true;
    }
   
	if(!channel1_flag && !channel2_flag)
	{
		LOG_ERR("Both audio buffers are NULL");
		// net_buf_unref(buf_out);
		// return;
		memset(buf_out->data, 0, buf_out->size);
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
		else
		{	
			if(0 == (timeCount++ % 50))
				leds_toggle(2);
			// LOG_INF("usb audio send %d bytes succeed!\t", data_out_size);
		}
	} 
    else 
#endif
    {
		// LOG_WRN("Wrong size write: %d", data_out_size);
		net_buf_unref(buf_out);
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


/*********************************opus decoder*********************************************/

static void opus_decoder_configure(void)
{
        printk("opus_decoder_get_size() = %d\n", opus_decoder_get_size(m_opus_channels));        
        __ASSERT_NO_MSG(opus_decoder_get_size(m_opus_channels) <= sizeof(m_opus_decoder));
        __ASSERT_NO_MSG(opus_decoder_init(m_opus_decoder_state, CONFIG_AUDIO_SAMPLING_FREQUENCY, m_opus_channels) == OPUS_OK);
}



void esb_buffer_handle(void)
{
    int err = 0;
    struct inv_esb_payload rx_payload;
	int16_t block_ptr[PCM_FRAME_BYTES];
	// 1ms buffer
    // int16_t slice_buf[FRAME_SIZE];

	if(k_msgq_get(&m_msgq_rx_payloads, &rx_payload, K_FOREVER) == 0)
    {
		uint16_t pcm_index = 0;
		int frame_size = 0;
		uint8_t devID = rx_payload.dev_id;
		uint32_t packet_id = rx_payload.data[0] | (rx_payload.data[1] << 8) | (rx_payload.data[2] << 16) | (rx_payload.data[3] << 24);

        // LOG_INF("Packet received[%d] from %d, 0x%02x, 0x%02x, 0x%02x, 0x%02x  ", rx_payload.length,			
		// 		devID, rx_payload.data[0],rx_payload.data[1], rx_payload.data[2],rx_payload.data[3]);

		frame_size = opus_decode(m_opus_decoder_state, 
								&rx_payload.data[4], 
								CONFIG_AUDIO_FRAME_SIZE_BYTES, 
								block_ptr, 
								CONFIG_AUDIO_FRAME_SIZE_SAMPLES, 0);

		// LOG_INF("%d--%d", packet_id, frame_size);
		if(frame_size != CONFIG_AUDIO_FRAME_SIZE_SAMPLES)	
		{															
			// LOG_INF("%d--%d: 0x%02x, 0x%02x, 0x%02x, 0x%02x", rx_payload.length, frame_size, rx_payload.data[0],rx_payload.data[1],
			// 		rx_payload.data[MAX_PAYLOAD_SIZE-2],rx_payload.data[MAX_PAYLOAD_SIZE-1]);
			return;
		}
	#if 1
		/** send the PCM data to USB audio driver*/
		if(devID == 1)
		{
			while(pcm_index + FRAME_SIZE <= frame_size * 2) // 2 channels
			{
				err = k_msgq_put(&esb_queue1, &block_ptr[pcm_index], K_NO_WAIT);
				if(!err)
				{
					pcm_index += FRAME_SIZE;
				}
				else
				{
					LOG_ERR("1[%d] Message sent error: %d", pcm_index, err);
					break;
				}
			}
			// LOG_INF("esb_queue1: %d", pcm_index);
		}
		else if(devID == 2)
		{	
			while(pcm_index + FRAME_SIZE <= frame_size * 2) // 2 channels
			{
				err = k_msgq_put(&esb_queue2, &block_ptr[pcm_index], K_NO_WAIT);			
				if(!err)
				{
					pcm_index += FRAME_SIZE;
				}
				else
				{
					LOG_ERR("2[%d] Message sent error: %d", pcm_index, err);
					break;
				}
			}
			// LOG_INF("esb_queue2: %d", pcm_index);
		}
		else
		{
			err = -EINVAL;
		}
	#endif
    } 
    else 
    {
        LOG_ERR("Error while reading esb rx packet");
    }
}

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

	opus_decoder_configure();

	LOG_INF("USB enabled");
	LOG_INF("mic_frame_size = %d\t ", usb_audio_get_in_frame_size(mic_dev));

    while(1)
    {
		esb_buffer_handle();
    }
}


K_THREAD_DEFINE(esb_audio_service, AUDIO_HANDLE_STACK_SIZE,
                esb_audio_data_handle, NULL, NULL, NULL,
                K_PRIO_PREEMPT(AUDIO_HANDLE_PRIORITY), 0, 0);




