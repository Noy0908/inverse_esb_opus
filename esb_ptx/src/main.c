/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
MODIFIED SAMPLE TO INCLUDE EXTENSIONS ++
*/

#include <zephyr/kernel.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/console/console.h>
#include <app_event_manager.h>
#include <string.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h> 
#include <dk_buttons_and_leds.h>
// #include <esb.h>
// #include "app_bt_hid.h"
// #include "app_timeslot.h"
// #include "app_esb.h"
#include "drv_mic.h"
#include "mic_work_event.h"
#include "sound_service.h"
#include "esb_handler.h"

LOG_MODULE_REGISTER(main, CONFIG_ESB_BT_LOG_LEVEL);


#define FW_VERSION		"1.2.3"

/*
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#define USER_BUTTON             DK_BTN2_MSK

static volatile bool button_flag = false;


static const struct gpio_dt_spec leds[] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios),
};



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
		gpio_pin_set(leds[i].port, leds[i].pin, 0);
	}

	return 0;
}


void turn_on_off_led(uint8_t idx, bool onOff)
{
	if(onOff == true)
	{
		gpio_pin_set(leds[idx].port, leds[idx].pin, 1);
	}
	else
	{	
		gpio_pin_set(leds[idx].port, leds[idx].pin, 0);
	}
}

int leds_toggle(uint8_t idx) 
{
	gpio_pin_toggle(leds[idx].port, leds[idx].pin);

	return 0;
}


static void button_changed(uint32_t button_state, uint32_t has_changed)
{

	// printk("Button state: 0x%08x, has changed: 0x%08x", button_state, has_changed);
	if (has_changed & USER_BUTTON) 
	{
		uint32_t user_button_state = button_state & USER_BUTTON;
		if (!user_button_state) 
		{
			button_flag = !button_flag;
	
			if(button_flag)
			{
				struct mic_work_event *mic_event = new_mic_work_event();
				mic_event->type = MIC_STATUS_START;
				APP_EVENT_SUBMIT(mic_event);
				// LOG_INF("Button pressed, micphone start to work!");
			}
			else
			{
				struct mic_work_event *mic_event = new_mic_work_event();
				mic_event->type = MIC_STATUS_STOP;
				APP_EVENT_SUBMIT(mic_event);
				// LOG_INF("Button released, micphone stop to work!");
			}
		}
	}
}

static int init_button(void)
{
	int err;

	err = dk_buttons_init(button_changed);
	if (err) {
		printk("Cannot init buttons (err: %d)\n", err);
	}

	return err;
}


int main(void)
{
	int err;

	err = leds_init();
	if (err) {
		LOG_ERR("LEDs init failed (err %d)", err);
		return err;
	}
	
	err = init_button();
	if (err) {
		LOG_ERR("Cannot init buttons (err: %d)\n", err);
	}

	drv_audio_init();

	err = app_event_manager_init();
	if (err) {
		LOG_ERR("Unable to init Application Event Manager (%d)", err);
		return err;
	}

	inverse_esb_init();

	NRF_POWER->TASKS_CONSTLAT = 1; // Enable constant latency mode

	LOG_INF("ESB BLE Multiprotocol Example, version is %s!\r\n",FW_VERSION);
	LOG_INF("Main thread priority is %d!\r\n",k_thread_priority_get(k_current_get()));

#if 0
	while (1) {	
		if(debounce_is_ongoing)	
		{
			k_sleep(K_MSEC(100)); // debounce delay
			debounce_is_ongoing = false;
		}
		else
		{
			k_yield(); // yield to other threads
		}
		// LOG_INF("Main thread is running, button_flag=%d", button_flag);
		// k_sleep(K_SECONDS(1));
	}
#endif
	return 0;
}
