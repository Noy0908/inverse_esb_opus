#ifndef __APP_BT_HID_H
#define __APP_BT_HID_H

typedef enum {APP_BT_EVT_CONNECTED, APP_BT_EVT_DISCONNECTED } app_bt_event_type_t;

typedef struct {
	app_bt_event_type_t evt_type;
	
} app_bt_event_t;

typedef void (*app_bt_callback_t)(app_bt_event_t *event);

int app_bt_init(app_bt_callback_t callback);

// void app_bt_move(bool mov);

#endif
