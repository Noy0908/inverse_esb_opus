// #include "circle_test.h" 

#include <nrfx_timer.h>
#include "app_bt_hid.h"
#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <soc.h>
#include <assert.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>
#include <bluetooth/services/hids.h>
#include <zephyr/bluetooth/services/dis.h>
#include <dk_buttons_and_leds.h>

LOG_MODULE_REGISTER(app_bt_hid, CONFIG_ESB_BT_LOG_LEVEL);

/** @brief Symbol specifying timer instance to be used. */
#define TIMER_INST_IDX 3

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

#define BASE_USB_HID_SPEC_VERSION   0x0101


#if 1
/* Number of pixels by which the cursor is moved when a button is pushed. */
#define MOVEMENT_SPEED              5
/* Number of input reports in this application. */
#define INPUT_REPORT_COUNT          3
/* Length of Mouse Input Report containing button data. */
#define INPUT_REP_BUTTONS_LEN       3
/* Length of Mouse Input Report containing movement data. */
#define INPUT_REP_MOVEMENT_LEN      3
/* Length of Mouse Input Report containing media player data. */
#define INPUT_REP_MEDIA_PLAYER_LEN  1
/* Index of Mouse Input Report containing button data. */
#define INPUT_REP_BUTTONS_INDEX     0
/* Index of Mouse Input Report containing movement data. */
#define INPUT_REP_MOVEMENT_INDEX    1
/* Index of Mouse Input Report containing media player data. */
#define INPUT_REP_MPLAYER_INDEX     2
/* Id of reference to Mouse Input Report containing button data. */
#define INPUT_REP_REF_BUTTONS_ID    1
/* Id of reference to Mouse Input Report containing movement data. */
#define INPUT_REP_REF_MOVEMENT_ID   2
/* Id of reference to Mouse Input Report containing media player data. */
#define INPUT_REP_REF_MPLAYER_ID    3
#endif
/* HIDs queue size. */
#define HIDS_QUEUE_SIZE 10

#define REPORT_RATE	8000  //8ms



static app_bt_callback_t m_callback;

static app_bt_event_t 	  m_event;


// static const uint8_t  dbg_pins[] = {31, 30, 29, 28, 04, 03};

// static const struct device *dbg_port= DEVICE_DT_GET(DT_NODELABEL(gpio0));


/* HIDS instance. */
BT_HIDS_DEF(hids_obj,
	    INPUT_REP_BUTTONS_LEN,
	    INPUT_REP_MOVEMENT_LEN,
	    INPUT_REP_MEDIA_PLAYER_LEN);

struct k_work hids_work;

struct mouse_pos {
	int16_t x_val;
	int16_t y_val;
};

/* Mouse movement queue. */
K_MSGQ_DEFINE(hids_queue,
	      sizeof(struct mouse_pos),
	      HIDS_QUEUE_SIZE,
	      4);

#if CONFIG_BT_DIRECTED_ADVERTISING
/* Bonded address queue. */
K_MSGQ_DEFINE(bonds_queue,
	      sizeof(bt_addr_le_t),
	      CONFIG_BT_MAX_PAIRED,
	      4);
#endif

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
					  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static struct conn_mode {
	struct bt_conn *conn;
	bool in_boot_mode;
} conn_mode[CONFIG_BT_HIDS_MAX_CLIENT_COUNT];

static struct k_work adv_work;


#if 0
/**************************  Timer Functions ********************************/

static nrfx_timer_t timer_inst = NRFX_TIMER_INSTANCE(TIMER_INST_IDX);


/**
 * @brief Function for handling TIMER driver events.
 *
 * @param[in] event_type Timer event.
 * @param[in] p_context  General purpose parameter set during initialization of
 *                       the timer. This parameter can be used to pass
 *                       additional information to the handler function, for
 *                       example, the timer ID.
 */
static void timer_handler(nrf_timer_event_t event_type, void * p_context)
{
    if(event_type == NRF_TIMER_EVENT_COMPARE0)
    {
    	struct mouse_pos pos;
		static int8_t 	circle_data[2];
		
//   gpio_pin_toggle(dbg_port, dbg_pins[1]);

		circle_test_get(circle_data);

		pos.x_val = (int16_t) circle_data[0];
		pos.y_val = (int16_t) circle_data[1];

	
		int err = k_msgq_put(&hids_queue, &pos, K_NO_WAIT);
		if (err) {
			LOG_WRN("No space in the queue, err %d", err);
			return;
		}
		
		k_work_submit(&hids_work);
    }
}


void timer_start(uint16_t time_us)
{

	 nrfx_timer_clear(&timer_inst); 
	 
	 /* Creating variable desired_ticks to store the output of nrfx_timer_ms_to_ticks function */
    uint32_t desired_ticks = nrfx_timer_us_to_ticks(&timer_inst, time_us);
	
	/*
     * Setting the timer channel NRF_TIMER_CC_CHANNEL0 in the extended compare mode to stop the timer and
     * trigger an interrupt if internal counter register is equal to desired_ticks.
     */
    nrfx_timer_extended_compare(&timer_inst, NRF_TIMER_CC_CHANNEL0, desired_ticks,
                                NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, true);
	
	nrfx_timer_enable(&timer_inst);
	LOG_INF("Timer Started ..." );
    

}

void timer_stop(void)
{
	nrfx_timer_disable(&timer_inst);
	LOG_INF("Timer Stopped ..." );
   

}

void timer_init(void)
{
	

    nrfx_timer_config_t config = NRFX_TIMER_DEFAULT_CONFIG(NRFX_MHZ_TO_HZ(1));
	// config.frequency = NRF_TIMER_FREQ_1MHz;
    nrfx_err_t status = nrfx_timer_init(&timer_inst, &config, timer_handler);
    NRFX_ASSERT(status == NRFX_SUCCESS);
	
#if defined(__ZEPHYR__)
    #define TIMER_INST         NRFX_CONCAT_2(NRF_TIMER, TIMER_INST_IDX)
    #define TIMER_INST_HANDLER NRFX_CONCAT_3(nrfx_timer_, TIMER_INST_IDX, _irq_handler)
    IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(TIMER_INST), IRQ_PRIO_LOWEST, TIMER_INST_HANDLER, 0);
#endif

}

#endif

/*************************************************************************************************************/		  		  

#if CONFIG_BT_DIRECTED_ADVERTISING
static void bond_find(const struct bt_bond_info *info, void *user_data)
{
	int err;

	/* Filter already connected peers. */
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (conn_mode[i].conn) {
			const bt_addr_le_t *dst =
				bt_conn_get_dst(conn_mode[i].conn);

			if (!bt_addr_le_cmp(&info->addr, dst)) {
				return;
			}
		}
	}

	err = k_msgq_put(&bonds_queue, (void *) &info->addr, K_NO_WAIT);
	if (err) {
		printk("No space in the queue for the bond.\n");
	}
}
#endif




static void advertising_continue(void)
{
	struct bt_le_adv_param adv_param;

#if CONFIG_BT_DIRECTED_ADVERTISING
	bt_addr_le_t addr;

	if (!k_msgq_get(&bonds_queue, &addr, K_NO_WAIT)) {
		char addr_buf[BT_ADDR_LE_STR_LEN];

		adv_param = *BT_LE_ADV_CONN_DIR(&addr);
		adv_param.options |= BT_LE_ADV_OPT_DIR_ADDR_RPA;

		int err = bt_le_adv_start(&adv_param, NULL, 0, NULL, 0);

		if (err) {
			printk("Directed advertising failed to start\n");
			return;
		}

		bt_addr_le_to_str(&addr, addr_buf, BT_ADDR_LE_STR_LEN);
		printk("Direct advertising to %s started\n", addr_buf);
	} else
#endif
	{
		int err;

		adv_param = *BT_LE_ADV_CONN;
		adv_param.options |= BT_LE_ADV_OPT_ONE_TIME;
		err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad),
				  sd, ARRAY_SIZE(sd));
		if (err) {
			printk("Advertising failed to start (err %d)\n", err);
			return;
		}

		printk("Regular advertising started\n");
	}
}

static void advertising_start(void)
{
#if CONFIG_BT_DIRECTED_ADVERTISING
	k_msgq_purge(&bonds_queue);
	bt_foreach_bond(BT_ID_DEFAULT, bond_find, NULL);
#endif

	k_work_submit(&adv_work);
}


static void advertising_process(struct k_work *work)
{
	advertising_continue();
}

static void insert_conn_object(struct bt_conn *conn)
{
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (!conn_mode[i].conn) {
			conn_mode[i].conn = conn;
			conn_mode[i].in_boot_mode = false;

			return;
		}
	}

	printk("Connection object could not be inserted %p\n", conn);
}

/*
static bool is_conn_slot_free(void)
{
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (!conn_mode[i].conn) {
			return true;
		}
	}

	return false;
}
*/

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		if (err == BT_HCI_ERR_ADV_TIMEOUT) {
			LOG_ERR("Direct advertising to %s timed out\n", addr);
			k_work_submit(&adv_work);
		} else {
			printk("Failed to connect to %s (%u)\n", addr, err);
		}
		return;
	}

	LOG_INF("Connected %s", addr);

	err = bt_hids_connected(&hids_obj, conn);

	if (err) {
		LOG_ERR("Failed to notify HID service about connection");
		return;
	}

	insert_conn_object(conn);

	// Forward an event to the application 
		m_event.evt_type = APP_BT_EVT_CONNECTED;
		m_callback(&m_event);
		//LOG_WRN("Forward APP_BT_EVT_CONNECTED event");
/*
	if (is_conn_slot_free()) {
		advertising_start();
	}
*/	
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected from %s (reason %u)\n", addr, reason);

	err = bt_hids_disconnected(&hids_obj, conn);

	if (err) {
		printk("Failed to notify HID service about disconnection\n");
	}

	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (conn_mode[i].conn == conn) {
			conn_mode[i].conn = NULL;
			break;
		}
	}

	advertising_start();

	// Forward an event to the application 
	m_event.evt_type = APP_BT_EVT_DISCONNECTED;
	m_callback(&m_event);
}



BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected        = connected,
	.disconnected     = disconnected,
};



static void hids_pm_evt_handler(enum bt_hids_pm_evt evt,
				struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];
	size_t i;

	for (i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (conn_mode[i].conn == conn) {
			break;
		}
	}

	if (i >= CONFIG_BT_HIDS_MAX_CLIENT_COUNT) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	switch (evt) {
	case BT_HIDS_PM_EVT_BOOT_MODE_ENTERED:
		printk("Boot mode entered %s\n", addr);
		conn_mode[i].in_boot_mode = true;
		break;

	case BT_HIDS_PM_EVT_REPORT_MODE_ENTERED:
		printk("Report mode entered %s\n", addr);
		conn_mode[i].in_boot_mode = false;
		break;

	default:
		break;
	}
}


static void hid_init(void)
{
	int err;
	struct bt_hids_init_param hids_init_param = { 0 };
	struct bt_hids_inp_rep *hids_inp_rep;
	static const uint8_t mouse_movement_mask[ceiling_fraction(INPUT_REP_MOVEMENT_LEN, 8)] = {0};

	static const uint8_t report_map[] = {
		0x05, 0x01,     /* Usage Page (Generic Desktop) */
		0x09, 0x02,     /* Usage (Mouse) */

		0xA1, 0x01,     /* Collection (Application) */

		/* Report ID 1: Mouse buttons + scroll/pan */
		0x85, 0x01,       /* Report Id 1 */
		0x09, 0x01,       /* Usage (Pointer) */
		0xA1, 0x00,       /* Collection (Physical) */
		0x95, 0x05,       /* Report Count (3) */
		0x75, 0x01,       /* Report Size (1) */
		0x05, 0x09,       /* Usage Page (Buttons) */
		0x19, 0x01,       /* Usage Minimum (01) */
		0x29, 0x05,       /* Usage Maximum (05) */
		0x15, 0x00,       /* Logical Minimum (0) */
		0x25, 0x01,       /* Logical Maximum (1) */
		0x81, 0x02,       /* Input (Data, Variable, Absolute) */
		0x95, 0x01,       /* Report Count (1) */
		0x75, 0x03,       /* Report Size (3) */
		0x81, 0x01,       /* Input (Constant) for padding */
		0x75, 0x08,       /* Report Size (8) */
		0x95, 0x01,       /* Report Count (1) */
		0x05, 0x01,       /* Usage Page (Generic Desktop) */
		0x09, 0x38,       /* Usage (Wheel) */
		0x15, 0x81,       /* Logical Minimum (-127) */
		0x25, 0x7F,       /* Logical Maximum (127) */
		0x81, 0x06,       /* Input (Data, Variable, Relative) */
		0x05, 0x0C,       /* Usage Page (Consumer) */
		0x0A, 0x38, 0x02, /* Usage (AC Pan) */
		0x95, 0x01,       /* Report Count (1) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0xC0,             /* End Collection (Physical) */

		/* Report ID 2: Mouse motion */
		0x85, 0x02,       /* Report Id 2 */
		0x09, 0x01,       /* Usage (Pointer) */
		0xA1, 0x00,       /* Collection (Physical) */
		0x75, 0x0C,       /* Report Size (12) */
		0x95, 0x02,       /* Report Count (2) */
		0x05, 0x01,       /* Usage Page (Generic Desktop) */
		0x09, 0x30,       /* Usage (X) */
		0x09, 0x31,       /* Usage (Y) */
		0x16, 0x01, 0xF8, /* Logical maximum (2047) */
		0x26, 0xFF, 0x07, /* Logical minimum (-2047) */
		0x81, 0x06,       /* Input (Data, Variable, Relative) */
		0xC0,             /* End Collection (Physical) */
		0xC0,             /* End Collection (Application) */

		/* Report ID 3: Advanced buttons */
		0x05, 0x0C,       /* Usage Page (Consumer) */
		0x09, 0x01,       /* Usage (Consumer Control) */
		0xA1, 0x01,       /* Collection (Application) */
		0x85, 0x03,       /* Report Id (3) */
		0x15, 0x00,       /* Logical minimum (0) */
		0x25, 0x01,       /* Logical maximum (1) */
		0x75, 0x01,       /* Report Size (1) */
		0x95, 0x01,       /* Report Count (1) */

		0x09, 0xCD,       /* Usage (Play/Pause) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x83, 0x01, /* Usage (Consumer Control Configuration) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xB5,       /* Usage (Scan Next Track) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xB6,       /* Usage (Scan Previous Track) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */

		0x09, 0xEA,       /* Usage (Volume Down) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xE9,       /* Usage (Volume Up) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x25, 0x02, /* Usage (AC Forward) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x24, 0x02, /* Usage (AC Back) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0xC0              /* End Collection */
	};

	hids_init_param.rep_map.data = report_map;
	hids_init_param.rep_map.size = sizeof(report_map);

	hids_init_param.info.bcd_hid = BASE_USB_HID_SPEC_VERSION;
	hids_init_param.info.b_country_code = 0x00;
	hids_init_param.info.flags = (BT_HIDS_REMOTE_WAKE |
				      BT_HIDS_NORMALLY_CONNECTABLE);

	hids_inp_rep = &hids_init_param.inp_rep_group_init.reports[0];
	hids_inp_rep->size = INPUT_REP_BUTTONS_LEN;
	hids_inp_rep->id = INPUT_REP_REF_BUTTONS_ID;
	hids_init_param.inp_rep_group_init.cnt++;

	hids_inp_rep++;
	hids_inp_rep->size = INPUT_REP_MOVEMENT_LEN;
	hids_inp_rep->id = INPUT_REP_REF_MOVEMENT_ID;
	hids_inp_rep->rep_mask = mouse_movement_mask;
	hids_init_param.inp_rep_group_init.cnt++;

	hids_inp_rep++;
	hids_inp_rep->size = INPUT_REP_MEDIA_PLAYER_LEN;
	hids_inp_rep->id = INPUT_REP_REF_MPLAYER_ID;
	hids_init_param.inp_rep_group_init.cnt++;

	hids_init_param.is_mouse = true;
	hids_init_param.pm_evt_handler = hids_pm_evt_handler;

	err = bt_hids_init(&hids_obj, &hids_init_param);
	__ASSERT(err == 0, "HIDS initialization failed\n");
}

#if 0
static void mouse_movement_send(int16_t x_delta, int16_t y_delta)
{
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {

		if (!conn_mode[i].conn) {
			continue;
		}

		if (conn_mode[i].in_boot_mode) {
			x_delta = MAX(MIN(x_delta, SCHAR_MAX), SCHAR_MIN);
			y_delta = MAX(MIN(y_delta, SCHAR_MAX), SCHAR_MIN);

			bt_hids_boot_mouse_inp_rep_send(&hids_obj,
							     conn_mode[i].conn,
							     NULL,
							     (int8_t) x_delta,
							     (int8_t) y_delta,
							     NULL);
		} else {
			uint8_t x_buff[2];
			uint8_t y_buff[2];
			uint8_t buffer[INPUT_REP_MOVEMENT_LEN];

			int16_t x = MAX(MIN(x_delta, 0x07ff), -0x07ff);
			int16_t y = MAX(MIN(y_delta, 0x07ff), -0x07ff);

			/* Convert to little-endian. */
			sys_put_le16(x, x_buff);
			sys_put_le16(y, y_buff);

			/* Encode report. */
			BUILD_ASSERT(sizeof(buffer) == 3,
					 "Only 2 axis, 12-bit each, are supported");

			buffer[0] = x_buff[0];
			buffer[1] = (y_buff[0] << 4) | (x_buff[1] & 0x0f);
			buffer[2] = (y_buff[1] << 4) | (y_buff[0] >> 4);


			bt_hids_inp_rep_send(&hids_obj, conn_mode[i].conn,
						  INPUT_REP_MOVEMENT_INDEX,
						  buffer, sizeof(buffer), NULL);
		}
	}
}
#endif

static void hid_handler(struct k_work *work)
{
#if 0
	struct mouse_pos pos;

	k_msgq_get(&hids_queue, &pos, K_NO_WAIT);
	mouse_movement_send(pos.x_val, pos.y_val);
	/*while (!k_msgq_get(&hids_queue, &pos, K_NO_WAIT)) {
		mouse_movement_send(pos.x_val, pos.y_val);
	}*/
#else
	/** send audio monitor commander to TV*/

#endif
}


// void app_bt_move(bool mov)
// {
// 	struct mouse_pos pos;
	
// 	memset(&pos, 0, sizeof(struct mouse_pos));

// 	if(mov)
// 	{
// 		timer_start(REPORT_RATE);
// 	}
// 	else
// 	{
// 		timer_stop();
// 	}


// }


int app_bt_init(app_bt_callback_t callback)
{
	int err;
	
	LOG_INF("Starting Bluetooth Peripheral");

	m_callback = callback;

	// timer_init();		

	/* DIS initialized at system boot with SYS_INIT macro. */
	hid_init();

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}

	LOG_INF("Bluetooth initialized");

	k_work_init(&hids_work, hid_handler);
	k_work_init(&adv_work, advertising_process);

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}
	
	advertising_start();
	
	LOG_INF("Advertising successfully started");

	return 0;
}