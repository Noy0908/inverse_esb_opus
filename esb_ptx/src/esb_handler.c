#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "esb_handler.h"


LOG_MODULE_REGISTER(esb_handler, LOG_LEVEL_INF);


#define DEV_NUM			2


/***********inv_esb_variables *********************************************************/
static uint8_t radio_dev_num = DEV_NUM;
static uint8_t radio_group;

static uint8_t tx_packet[] = { 0, '1', '2', '3', '4', '5', '6','7', \
				'a', 'b', 'c', 'd', 'e', 'f', 'g','h', \
				'0', '1', '2', '3', '4', '5', '6','7', \
				'a', 'b', 'c', 'd', 'e', 'f', 'g','h', };


extern int leds_toggle(uint8_t idx);

static void radio_evt_cb(radio_evt_t const * p_event)
{
	switch(p_event->evt_id) {
	case RADIO_EVENT_PERIPH_POLL_RCV:
		if (p_event->data_len) {
			bool last_send_flag = p_event->data[0];
			if(last_send_flag)
			{
				delete_tx_item_from_queue();
				/** below code just for test */
				tx_packet[0]++;
				inv_esb_package_enqueue(tx_packet, sizeof(tx_packet));
			}
		}
		leds_toggle(1);


		break;
	default:
		break;
	}
}




static uint16_t get_periph_dev_num(void)
{
	return (radio_group << 8) + radio_dev_num;
}

int inverse_esb_start(void)
{
	int err;

	err = radio_set_dev_num(get_periph_dev_num());

	if (!err) {
		radio_start_receive();
	} else {
		LOG_ERR("Setting radio failed (err %d)", err);
	}

	inv_esb_package_enqueue(tx_packet, sizeof(tx_packet));			//for test

	return err;
}



void inverse_esb_init(void)
{
	radio_init_t radio_init;

	radio_init.dev_num				= DEV_NUM;
	radio_init.mode					= PHY_MODE;
	radio_init.event_callback		= radio_evt_cb;

	radio_setup(&radio_init);
}
