#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "esb_handler.h"


LOG_MODULE_REGISTER(esb_handler, LOG_LEVEL_INF);


#define DEV_NUM			2


/***********inv_esb_variables *********************************************************/
static radio_power_t tx_power = RADIO_TX_POWER_4DBM;

static uint8_t			rx_packet[MAX_PAYLOAD_SIZE];              /**< Packet to receive */

static uint8_t			tx_packet[] = { 0, '1', '2', '3', '4', '5', '6','7', \
						'a', 'b', 'c', 'd', 'e', 'f', 'g','h', \
						'0', '1', '2', '3', '4', '5', '6','7', \
						'a', 'b', 'c', 'd', 'e', 'f', 'g','h', };


static uint8_t radio_dev_num = DEV_NUM;
static uint8_t radio_group;

extern void turn_on_off_led(bool onOff);

static void radio_evt_cb(radio_evt_t const * p_event)
{
	switch(p_event->evt_id) {
	case RADIO_EVENT_PERIPH_POLL_RCV:
		if (p_event->data_len) {
			uint8_t data0 = rx_packet[0];
			 // Set LEDs identical to the ones on the PTX.
			//  gpio_pin_set(led_port, led_pins[0], !(data0%8>0 && data0%8<=4));
			//  gpio_pin_set(led_port, led_pins[1], !(data0%8>1 && data0%8<=5));
			//  gpio_pin_set(led_port, led_pins[2], !(data0%8>2 && data0%8<=6));
			//  gpio_pin_set(led_port, led_pins[3], !(data0%8>3));
			turn_on_off_led(!(data0%8>0 && data0%8<=4));
		}
		break;
	case RADIO_EVENT_PERIPH_DATA_SND:
		//Update TX packet
		tx_packet[0]++;
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

	return err;
}



void inverse_esb_init(void)
{
	radio_init_t radio_init;
	radio_address_t address = RADIO_ADDR_CONFIG;

	radio_init.num_subevts			= NUM_OF_SUBEVTS;
	radio_init.num_periphs			= NUM_OF_PERIPHS;
	radio_init.dev_num				= DEV_NUM;
	radio_init.mode					= PHY_MODE;
	radio_init.event_callback		= radio_evt_cb;
	radio_init.address				= address;
	radio_init.tx_power				= tx_power;
	radio_init.tx_buf				= tx_packet;
	radio_init.rx_buf				= rx_packet;
	radio_init.tx_length			= PERIPH_PKT_SIZE;
	radio_init.channel_tab			= RF_CHANNEL_TAB;
	radio_init.channel_tab_size		= sizeof(RF_CHANNEL_TAB);
	// radio_init.rtc_tick_val			= PERIPH_RTC_RX_OPERATE_PERIOD;
	radio_init.scan_timer_val		= PERIPH_TIMER_SCAN_US;
	radio_init.periph_tx_timer_val		= PERIPH_TIMER_TX_DELAY_PERIOD;
	radio_init.periph_rx_search_rtc_val	= PERIPH_RTC_RX_SEARCH_PERIOD;
	radio_init.periph_rtc_tick_adj_val	= PERIPH_RTC_RX_OPERATE_ADJ_PERIOD;

	radio_setup(&radio_init);
}
