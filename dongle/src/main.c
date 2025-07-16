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
// #include <esb.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include "esb_handle.h"
// #include "dvi_adpcm.h"
#include "audio_handle.h"



LOG_MODULE_REGISTER(smart_dongle, CONFIG_ESB_PRX_APP_LOG_LEVEL);

#define FW_VERSION				"1.2.0"


static const struct gpio_dt_spec leds[] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios),
};



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

		gpio_pin_set(leds[i].port, leds[i].pin, 0);
	}

	return 0;
}



int leds_toggle(uint8_t idx) 
{
	// for (size_t i = 0; i < ARRAY_SIZE(leds); i++) 
	{
		gpio_pin_toggle(leds[0].port, leds[idx].pin);
	}

	return 0;
}

int led_on_off(uint8_t idx, bool on)
{
	if (idx >= ARRAY_SIZE(leds)) {
		LOG_ERR("Invalid LED index %u", idx);
		return -EINVAL;
	}

	int err = gpio_pin_set(leds[0].port, leds[idx].pin, on ? 1 : 0);
	if (err) {
		LOG_ERR("Failed to set LED%u state, err %d", idx, err);
		return err;
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

	// LOG_DBG("HF clock started, fw_version is %s", FW_VERSION);
	return 0;
}




int main(void)
{
	int err;

	// err = clocks_start();
	// if (err) {
	// 	return err;
	// }

	err = leds_init();
	if (err) {
		return err;
	}

	inverse_esb_init();

	radio_start_poll();

	LOG_DBG("Inverse esb dongle demo, fw_version is %s", FW_VERSION);

	/* return to idle thread */
	return 0;
}
