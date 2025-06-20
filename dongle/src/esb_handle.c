#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
// #include <esb.h>
#include "dvi_adpcm.h"
#include "esb_handle.h"


LOG_MODULE_DECLARE(smart_dongle, CONFIG_ESB_PRX_APP_LOG_LEVEL);


/* Driver will allocate blocks from this slab to save adpcm data into them.
 * Application, after getting a given block then push it to the esb send message queue,
 * needs to free that block.
 */

K_MEM_SLAB_DEFINE(esb_slab, ESB_BLOCK_SIZE, ESB_BLOCK_COUNT, 4);

K_MSGQ_DEFINE(esb_queue, 4, ESB_BLOCK_COUNT, 4);

K_SEM_DEFINE(esb_sem, 0, 1);

/** this pointer variable used for transport the message queue to USB audio thread.*/
void *block_ptr = NULL;

extern dvi_adpcm_state_t m_adpcm_state;

#if 0
static void event_handler(struct esb_evt const *event)
{
	switch (event->evt_id) {
	case ESB_EVENT_TX_SUCCESS:
		// LOG_DBG("TX SUCCESS EVENT");
		break;
	case ESB_EVENT_TX_FAILED:
		LOG_DBG("TX FAILED EVENT");
		break;
	case ESB_EVENT_RX_RECEIVED:
        // esb_buffer_handle();
		/* notify thread that data is available */
    	k_sem_give(&esb_sem);

		// if (esb_read_rx_payload(&rx_payload) == 0) {
		// 	LOG_INF("Packet received, len %d : "
		// 		"0x%02x, 0x%02x, 0x%02x, 0x%02x ",				
		// 		rx_payload.length, rx_payload.data[0],
		// 		rx_payload.data[1], rx_payload.data[2],
		// 		rx_payload.data[3] );
		// 	gpio_pin_toggle(leds[0].port, leds[0].pin);			
		// } else {
		// 	LOG_ERR("Error while reading rx packet");
		// }
		break;
	}
}

int esb_initialize(void)
{
	int err;
	/* These are arbitrary default addresses. In end user products
	 * different addresses should be used for each set of devices.
	 */
	uint8_t base_addr_0[4] = {0xE7, 0xE5, 0xE7, 0xE5};
	uint8_t base_addr_1[4] = {0xC2, 0xC2, 0xC2, 0xC2};
	uint8_t addr_prefix[8] = {0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8};

	struct esb_config config = ESB_DEFAULT_CONFIG;

	config.protocol = ESB_PROTOCOL_ESB_DPL;
	config.retransmit_delay = 250;//600;
	config.retransmit_count = 1;
	config.bitrate = ESB_BITRATE_2MBPS;
	config.mode = ESB_MODE_PRX;
	config.event_handler = event_handler;
	config.selective_auto_ack = true;

	err = esb_init(&config);
	if (err) {
		return err;
	}

	err = esb_set_base_address_0(base_addr_0);
	if (err) {
		return err;
	}

	err = esb_set_base_address_1(base_addr_1);
	if (err) {
		return err;
	}

	err = esb_set_prefixes(addr_prefix, ARRAY_SIZE(addr_prefix));
	if (err) {
		return err;
	}

	return 0;
}
#endif

// static void mono_to_stereo(int16_t* src_audio, int frames, int16_t* dst_audio) 
// {
//     for (int i = 0; i < frames; i++) 
//     {
//         dst_audio[2 * i] = src_audio[i];
//         dst_audio[2 * i + 1] = src_audio[i];
//     }
// }


void esb_buffer_handle(void)
{
#if 0
    int err = 0;
    int frame_size = 0;
	uint8_t adpcm_index = 0;
    struct esb_payload rx_payload;

    if (esb_read_rx_payload(&rx_payload) == 0) 
    {
        LOG_INF("Packet received, len %d ", rx_payload.length);
        //     "0x%02x, 0x%02x, 0x%02x, 0x%02x ",				
        //     rx_payload.length, rx_payload.data[0],
        //     rx_payload.data[1], rx_payload.data[2],
        //     rx_payload.data[3] );
	#if 1
		while(adpcm_index + ADPCM_BLOCK_SIZE <= rx_payload.length)
		{
			if(k_mem_slab_alloc(&esb_slab, (void **) &block_ptr, K_MSEC(1)) == 0)
			{
				dvi_adpcm_decode(&(rx_payload.data[adpcm_index]), ADPCM_BLOCK_SIZE, block_ptr, &frame_size, &m_adpcm_state);
				// LOG_INF("adpcm_index=%d, ADPCMdecompress %u bytes", adpcm_index, frame_size);
				// LOG_HEXDUMP_INF(block_ptr, 8, "ADPCM decompress");
    
				/** send the PCM data to USB audio driver*/
				err = k_msgq_put(&esb_queue, &block_ptr, K_NO_WAIT);
				if (err) {
					LOG_ERR("Message sent error: %d", err);
				}

				adpcm_index += ADPCM_BLOCK_SIZE;
			}
			else 
			{
				LOG_ERR("Memory allocation for ESB receive time-out");
				break;
			}	
		}
	#endif
    } 
    else 
    {
        LOG_ERR("Error while reading esb rx packet");
    }
#endif
}


void free_esb_slab_memory(void *buffer)
{
	k_mem_slab_free(&esb_slab, buffer);
}