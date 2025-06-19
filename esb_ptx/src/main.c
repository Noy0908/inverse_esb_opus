/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
MODIFIED SAMPLE TO INCLUDE EXTENSIONS ++
*/

#include <zephyr/kernel.h>
#include <zephyr/console/console.h>
#include <app_event_manager.h>
#include <string.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h> 
#include <esb.h>
// #include "app_bt_hid.h"
// #include "app_timeslot.h"
// #include "app_esb.h"
#include "drv_mic.h"
#include "mic_work_event.h"
#include "sound_service.h"
#include "../../lib/inv_esb_lib/radio.h"
#include "../../lib/inv_esb_lib/radio_config.h"

LOG_MODULE_REGISTER(main, CONFIG_ESB_BT_LOG_LEVEL);

#define DEV_NUM			1


#define FW_VERSION		"1.2.9"

#define MOV_LED			DK_LED1
#define TIMESLOT_LED	DK_LED2

/* Key used to turn on circle drawing*/
#define KEY_ON_MASK		DK_BTN1_MSK
/* Key used to turn off circle drawing */
#define KEY_OFF_MASK    DK_BTN2_MSK

#define ESB_PKT_SIZE	72


/*
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#define SW0_NODE	DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,{0});
static struct gpio_callback button_cb_data;


static const struct gpio_dt_spec leds[] = {
	// GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
};


/***********inv_esb_variables *********************************************************/
static radio_power_t tx_power = RADIO_TX_POWER_4DBM;

static uint8_t			rx_packet[MAX_PAYLOAD_SIZE];              /**< Packet to receive */

static uint8_t			tx_packet[] = { 0, '1', '2', '3', '4', '5', '6','7', \
						'a', 'b', 'c', 'd', 'e', 'f', 'g','h', \
						'0', '1', '2', '3', '4', '5', '6','7', \
						'a', 'b', 'c', 'd', 'e', 'f', 'g','h', };

static bool radio_is_up;
static uint8_t radio_dev_num = DEV_NUM;
static uint8_t radio_group;
						

static int set_radio_and_start(void);

/**************************************************************************************** */



extern struct k_msgq m_msgq_tx_payloads; 


static int leds_init(void)
{
	int err;

	if (!device_is_ready(leds[0].port)) {
		LOG_ERR("LEDs port not ready");
		return -ENODEV;
	}


	for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
		err = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT);
		if (err) {
			LOG_ERR("Cannot configure LED gpio");
			return err;
		}
		gpio_pin_set(leds[0].port, leds[i].pin, 0);
	}

	return 0;
}


void turn_on_off_led(bool onOff)
{
	size_t i = 0;
	if(onOff == true)
	{
		for ( i=0; i < ARRAY_SIZE(leds); i++) 
		{	
			gpio_pin_set(leds[0].port, leds[i].pin, 1);
		}
	}
	else
	{
		for (i = 0; i < ARRAY_SIZE(leds); i++) 
		{	
			gpio_pin_set(leds[0].port, leds[i].pin, 0);
		}
	}
}


static volatile bool button_flag = false;
static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	button_flag = !button_flag;

	LOG_INF("Button pressed at %d	 button_flag=%d\n", k_cycle_get_32(),button_flag);

	// wake up device and trigger micphone to work
	if(button_flag)
	{
		// struct mic_work_event *mic_event = new_mic_work_event();
		// mic_event->type = MIC_STATUS_START;
		// APP_EVENT_SUBMIT(mic_event);
		if(!radio_is_up)
		{
			set_radio_and_start();
			radio_is_up = true;
		}
	}
	else
	{
		// struct mic_work_event *mic_event = new_mic_work_event();
		// mic_event->type = MIC_STATUS_STOP;
		// APP_EVENT_SUBMIT(mic_event);
		if(radio_is_up)
		{
			radio_stop();
			radio_is_up = false;
		}
	}
}


static int buttons_init( void )
{
	int err = 0;
	
	if (!device_is_ready(button.port)) {
		LOG_ERR("Could not bind to debug port");
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (err != 0) {
		LOG_ERR("Error %d: failed to configure %s pin %d\n",
		       err, button.port->name, button.pin);
		return err;
	}

	err = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_FALLING);
	if (err != 0) {
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d\n",
			err, button.port->name, button.pin);
		return err;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	LOG_INF("Set up button at %s pin %d\r\n", button.port->name, button.pin);

	return 0;
}


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

static int set_radio_and_start(void)
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



static void init_radio_config(void)
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
	radio_init.rtc_tick_val			= PERIPH_RTC_RX_OPERATE_PERIOD;
	radio_init.scan_timer_val		= PERIPH_TIMER_SCAN_US;
	radio_init.periph_tx_timer_val		= PERIPH_TIMER_TX_DELAY_PERIOD;
	radio_init.periph_rx_search_rtc_val	= PERIPH_RTC_RX_SEARCH_PERIOD;
	radio_init.periph_rtc_tick_adj_val	= PERIPH_RTC_RX_OPERATE_ADJ_PERIOD;

	radio_setup(&radio_init);
}




int main(void)
{
	int err;

	NRF_POWER->DCDCEN =1;  // Enable DCDC

	err = leds_init();
	if (err) {
		LOG_ERR("LEDs init failed (err %d)", err);
		return err;
	}
	
	err = buttons_init();
	if (err) {
		LOG_ERR("Cannot init buttons (err: %d)\n", err);
	}

	drv_audio_init();

	err = app_event_manager_init();
	if (err) {
		LOG_ERR("Unable to init Application Event Manager (%d)", err);
		return err;
	}

	init_radio_config();

	LOG_INF("ESB BLE Multiprotocol Example, version is %s!\r\n",FW_VERSION);
	LOG_INF("Main thread priority is %d!\r\n",k_thread_priority_get(k_current_get()));

	
#if 0
	while (1) {		
		if (get_timeslot_status()) 
			pull_packet_from_tx_msgq();
		else
			k_sleep(K_MSEC(1));
	}
#endif
	return 0;
}
