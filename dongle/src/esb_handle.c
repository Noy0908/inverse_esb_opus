#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
// #include <esb.h>
#include "dvi_adpcm.h"
#include "esb_handle.h"


LOG_MODULE_DECLARE(smart_dongle, CONFIG_ESB_PRX_APP_LOG_LEVEL);


#define DEV_NUM					0xFFFF //0xFFFF being central


/*************inverse esb variables ******************************************************/

static bool new_pkt_received;


/*****************************************************************************************/

/* Driver will allocate blocks from this slab to save adpcm data into them.
 * Application, after getting a given block then push it to the esb send message queue,
 * needs to free that block.
 */

// K_MEM_SLAB_DEFINE(esb_slab, FRAME_SIZE, PCM_BLOCK_COUNT, 4);

K_MSGQ_DEFINE(esb_queue1, FRAME_SIZE, ESB_BLOCK_COUNT, 4);
K_MSGQ_DEFINE(esb_queue2, FRAME_SIZE, ESB_BLOCK_COUNT, 4);

K_MSGQ_DEFINE(m_msgq_rx_payloads, sizeof(struct inv_esb_payload), 60, 4);



/** this pointer variable used for transport the message queue to USB audio thread.*/
void *block_ptr = NULL;

extern dvi_adpcm_state_t m_adpcm_state;


extern int leds_toggle(uint8_t idx);


static int received_esb_package_enqueue(uint8_t devID, const uint8_t *buf, uint32_t length)
{
	int ret = 0;
	static struct inv_esb_payload rx_payload;
	rx_payload.dev_id = devID;
	memcpy(rx_payload.data, buf, length);
	rx_payload.length = length;
	ret = k_msgq_put(&m_msgq_rx_payloads, &rx_payload, K_NO_WAIT);
	if (ret)  {
		LOG_INF("Audio message queue is full");
		return -ENOMEM;
	}
	return ret;
}



static void radio_evt_cb(radio_evt_t const * p_event)
{
	static uint32_t timeCount1 = 0;
	static uint32_t timeCount2 = 0;

	switch(p_event->evt_id) {
	case RADIO_EVENT_CENTRAL_DATA_RCV:
		if (p_event->data_len) {
			// Toggle one of the LEDs.
			if (p_event->periph_num == 1) {
				if(0 == (timeCount1++ % 50))
					leds_toggle(0);
			} else if (p_event->periph_num == 2) {
				if(0 == (timeCount2++ % 50))
					leds_toggle(1);
			} 
			// LOG_INF("Rec %d from %d: %d", p_event->data_len, p_event->periph_num, p_event->data[0]);

			/** enqueue compressed audio data to message queue */
			received_esb_package_enqueue(p_event->periph_num, (const uint8_t *)p_event->data, p_event->data_len);
		}
		new_pkt_received = true;
		break;
	case RADIO_EVENT_CENTRAL_POLL_END:
		if (new_pkt_received) {
			new_pkt_received = false;

			increase_poll_index();
		}
		break;
	default:
		break;
	}
}


void inverse_esb_init(void)
{
	radio_init_t radio_init;

	radio_init.dev_num			= DEV_NUM;
	radio_init.mode				= PHY_MODE;
	radio_init.event_callback	= radio_evt_cb;

	radio_setup(&radio_init);
}





void esb_buffer_handle(void)
{
    int err = 0;
    int frame_size = 0;
	uint8_t adpcm_index = 0;
    struct inv_esb_payload rx_payload;
	uint8_t pcm_block[MAX_BLOCK_SIZE];

	if(k_msgq_get(&m_msgq_rx_payloads, &rx_payload, K_FOREVER) == 0)
    {
		uint8_t devID = rx_payload.dev_id;
        // LOG_INF("Packet received[%d] from %d, 0x%02x, 0x%02x, 0x%02x, 0x%02x  ", rx_payload.length,			
		// 		devID, rx_payload.data[0],rx_payload.data[1], rx_payload.data[2],rx_payload.data[3]);

		dvi_adpcm_decode(rx_payload.data, ADPCM_BLOCK_SIZE, pcm_block, &frame_size, &m_adpcm_state);
		// LOG_INF("[%d]:adpcm_index=%d, ADPCMdecompress %u bytes", devID, adpcm_index, frame_size);
		if(devID == 1)
		{
			while(adpcm_index + FRAME_SIZE <= frame_size)
			{
				err = k_msgq_put(&esb_queue1,  &pcm_block[adpcm_index], K_NO_WAIT);
				if(!err)
				{
					adpcm_index += FRAME_SIZE;
				}
				else 
				{
					break;
				}
			}
		}
		else if(devID == 2)
		{
			while(adpcm_index + FRAME_SIZE <= frame_size)
			{
				err = k_msgq_put(&esb_queue2, &pcm_block[adpcm_index], K_NO_WAIT);
				if(!err)
				{
					adpcm_index += FRAME_SIZE;
				}
				else 
				{
					break;
				}
			}
		}
		else
		{
			err = -EINVAL;
		}
    } 
    else 
    {
        LOG_ERR("Error while reading esb rx packet");
    }
}

