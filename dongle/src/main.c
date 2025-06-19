/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>
#include <nrf.h>
#include <esb.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include "esb_handle.h"
#include "dvi_adpcm.h"
#include "audio_handle.h"
#include "../../lib/inv_esb_lib/radio.h"
#include "../../lib/inv_esb_lib/radio_config.h"


LOG_MODULE_REGISTER(smart_dongle, CONFIG_ESB_PRX_APP_LOG_LEVEL);

#define FW_VERSION				"1.2.1"

#define DEV_NUM					0xFFFF //0xFFFF being central

dvi_adpcm_state_t    m_adpcm_state;

static const struct gpio_dt_spec leds[] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios),
};


static struct esb_payload tx_payload = ESB_CREATE_PAYLOAD(0,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17);

/*************inverse esb variables ******************************************************/

static bool new_pkt_received;

static radio_power_t tx_power = RADIO_TX_POWER_4DBM;

//32 bytes packet
static uint8_t tx_packet[] = { 1,  2,  3,  4,  5,  6,  7,  8, 
								9, 10, 11, 12, 13, 14, 15, 16,
								17, 18, 19, 20, 21, 22, 23, 24,
								25, 26, 27, 28, 29, 30, 31, 32} ;                    /**< Packet to transmit. */

static uint8_t rx_packet[NUM_OF_PERIPHS][MAX_PAYLOAD_SIZE];


/*****************************************************************************************/


static int leds_init(void)
{
	if (!device_is_ready(leds[0].port)) {
		LOG_ERR("LEDs port not ready");
		return -ENODEV;
	}

	for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
		int err = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT);

		if (err) {
			LOG_ERR("Unable to configure LED%u, err %d.", i, err);
			return err;
		}

		gpio_pin_set(leds[0].port, leds[i].pin, 0);
	}

	return 0;
}


int leds_toggle(void)
{
	// for (size_t i = 0; i < ARRAY_SIZE(leds); i++) 
	{
		gpio_pin_toggle(leds[0].port, leds[0].pin);
	}

	return 0;
}





int clocks_start(void)
{
	int err;
	int res;
	struct onoff_manager *clk_mgr;
	struct onoff_client clk_cli;

	clk_mgr = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
	if (!clk_mgr) {
		LOG_ERR("Unable to get the Clock manager");
		return -ENXIO;
	}

	sys_notify_init_spinwait(&clk_cli.notify);

	err = onoff_request(clk_mgr, &clk_cli);
	if (err < 0) {
		LOG_ERR("Clock request failed: %d", err);
		return err;
	}

	do {
		err = sys_notify_fetch_result(&clk_cli.notify, &res);
		if (!err && res) {
			LOG_ERR("Clock could not be started: %d", res);
			return res;
		}
	} while (err);

	LOG_DBG("HF clock started, fw_version is %s", FW_VERSION);
	return 0;
}



static void radio_evt_cb(radio_evt_t const * p_event)
{
	switch(p_event->evt_id) {
	case RADIO_EVENT_CENTRAL_DATA_RCV:
		if (p_event->data_len) {
			// Toggle one of the LEDs.
			if (p_event->periph_num == 1) {
				gpio_pin_set(leds[0].port, leds[0].pin, rx_packet[0][0] & 0x1);
			} else if (p_event->periph_num == 2) {
				gpio_pin_set(leds[0].port, leds[1].pin, rx_packet[1][0] & 0x1);
			} 
			// else if (p_event->periph_num == 3) {
			// 	gpio_pin_set(led_port, led_pins[2], rx_packet[2][0] & 0x1);
			// } else if (p_event->periph_num == 4) {
			// 	gpio_pin_set(led_port, led_pins[3], rx_packet[3][0] & 0x1);
			// }
			LOG_INF("Rec %d from %d: %d", p_event->data_len, p_event->periph_num, rx_packet[p_event->periph_num - 1][0]);
		}
		new_pkt_received = true;
		break;
	case RADIO_EVENT_CENTRAL_POLL_END:
		if (new_pkt_received) {
			new_pkt_received = false;

			tx_packet[0]++;
		}
		break;
	default:
		break;
	}
}


static void init_radio_config(void)
{
	radio_init_t radio_init;
	radio_address_t address = RADIO_ADDR_CONFIG;

	radio_init.num_subevts		= NUM_OF_SUBEVTS;
	radio_init.num_periphs		= NUM_OF_PERIPHS;
	radio_init.dev_num			= DEV_NUM;
	radio_init.mode				= PHY_MODE;
	radio_init.event_callback	= radio_evt_cb;
	radio_init.address			= address;
	radio_init.tx_power			= tx_power;
	radio_init.tx_buf			= tx_packet;
	radio_init.rx_buf			= &rx_packet[0][0];
	radio_init.tx_length		= CENTRAL_PKT_SIZE;
	radio_init.channel_tab		= RF_CHANNEL_TAB;
	radio_init.channel_tab_size	= sizeof(RF_CHANNEL_TAB);
	radio_init.rtc_tick_val		= CENTRAL_RTC_EVENT_TICKS;
	radio_init.scan_timer_val	= CENTRAL_TIMER_SCAN_US;
 
	radio_setup(&radio_init);
}


int main(void)
{
	int err;

	LOG_INF("Enhanced ShockBurst prx sample");

	err = clocks_start();
	if (err) {
		return err;
	}

	err = leds_init();
	if (err) {
		return err;
	}

	dvi_adpcm_init_state(&m_adpcm_state);

	// usb_audio_init();

	// err = esb_initialize();
	// if (err) {
	// 	LOG_ERR("ESB initialization failed, err %d", err);
	// 	return err;
	// }

	// LOG_INF("Initialization complete");

	// err = esb_write_payload(&tx_payload);
	// if (err) {
	// 	LOG_ERR("Write payload, err %d", err);
	// 	return err;
	// }

	// LOG_INF("Setting up for packet receiption");

	// err = esb_start_rx();
	// if (err) {
	// 	LOG_ERR("RX setup failed, err %d", err);
	// 	return err;
	// }

	init_radio_config();

	radio_start_poll();

	while(1)
	{
		/* Handle audio data */
		handle_audio_data();

		// LOG_DBG("Waiting for ESB events...");
	}

	/* return to idle thread */
	return 0;
}
