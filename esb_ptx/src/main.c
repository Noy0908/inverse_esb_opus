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
#include "app_bt_hid.h"
#include "app_timeslot.h"
#include "app_esb.h"
#include "drv_mic.h"
#include "mic_work_event.h"
#include "sound_service.h"

LOG_MODULE_REGISTER(main, CONFIG_ESB_BT_LOG_LEVEL);

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

static bool is_bt_connected = false;

extern struct k_msgq m_msgq_tx_payloads; 


void on_bt_callback(app_bt_event_t *event)
{
	switch(event->evt_type) {
		case APP_BT_EVT_CONNECTED:
			is_bt_connected = true;
			LOG_INF("BT CONNECTED");
			break;
		case APP_BT_EVT_DISCONNECTED:
			is_bt_connected = false;
			LOG_INF("BT DISCONNECTED");
			break;
	}
}


bool get_ble_status(void)
{
	return is_bt_connected;
}



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
		struct mic_work_event *mic_event = new_mic_work_event();
		mic_event->type = MIC_STATUS_START;
		APP_EVENT_SUBMIT(mic_event);
	}
	else
	{
		struct mic_work_event *mic_event = new_mic_work_event();
		mic_event->type = MIC_STATUS_STOP;
		APP_EVENT_SUBMIT(mic_event);
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



int main(void)
{
	int err;

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

	LOG_INF("ESB BLE Multiprotocol Example, version is %s!\r\n",FW_VERSION);
	LOG_INF("Main thread priority is %d!\r\n",k_thread_priority_get(k_current_get()));

	err = app_bt_init(on_bt_callback);
	if (err) {
		LOG_ERR("app_bt init failed (err %d)", err);
		return err;
	}

	err = app_esb_init(APP_ESB_MODE_PTX);
	if (err) {
		LOG_ERR("app_esb init failed (err %d)", err);
		return err;
	}
	
	timeslot_init();

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
