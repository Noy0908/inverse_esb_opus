#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef ESB_IPC_H
#define ESB_IPC_H


#define INVALID_MODE	0xFF
#define	CONFIG_CENTRAL		1
#define	CONFIG_PERIPH		2

typedef enum
{
	APP_IDLE,
	APP_CFG,
	APP_CFG2,
	APP_OPT
	
	
}app_state_t;



typedef enum
{
	RADIO_INITIALIZE_CMD = 1,
	RADIO_START_TX_POLL_CMD,
	RADIO_START_RX_SCAN_CMD,
	RADIO_PUT_PACKET_CMD

}cmd_t;


typedef enum
{
	RADIO_INITIALIZE_EVT = 1,
	RADIO_START_TX_POLL_EVT,
	RADIO_START_RX_SCAN_EVT,
	RADIO_CENTRAL_DATA_RCV_EVT,
	RADIO_PERIPH_DATA_SND_EVT,
	RADIO_PUT_PACKET_EVT

}evt_t;

typedef struct
{
    cmd_t 	data_hdr;
    uint8_t 	data_len;
    uint8_t 	data[96];
    
}ipc_cmd_msg_t;

typedef struct
{
    evt_t 	data_hdr;
    uint8_t 	data_len;
    uint8_t 	data[96];
    
}ipc_evt_msg_t;

#endif