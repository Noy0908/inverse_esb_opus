#ifndef RADIO_H
#define RADIO_H

#include <stdbool.h>
#include <stdint.h>

#include "nrfx_timer.h"

#include "radio_config.h"

#define MAX_SUBEVTS		1
#define MAX_PERIPHS		2

#define MAX_CHANNEL_TAB		16

#define MAX_PAYLOAD_SIZE	64

#define MAX_PACKET_LENGTH	(ROUND_UP(MAX_PERIPHS, 8) / 8 + MAX_PAYLOAD_SIZE)

#define	RADIO_RTC			NRF_RTC0
#define RADIO_RTC_IRQn		RTC0_IRQn


typedef enum
{
	
 IDLE_STATE=0,
 CENTRAL_TX_STATE,
 CENTRAL_RX_STATE,
 PERIPH_TX_STATE,
 PERIPH_RX_STATE	

}radio_states_t;


typedef enum
{
    MODE_2_MBIT,
    MODE_1_MBIT,    
} radio_modes_t;

/**@brief Enhanced ShockBurst radio transmission power modes. */
typedef enum {
#if defined(CONFIG_NRF52840) 	
    RADIO_POWER_8DBM     = RADIO_TXPOWER_TXPOWER_Pos8dBm,  /**< 8 dBm radio transmit power.   */
#endif	
#if defined(CONFIG_SOC_SERIES_NRF53X)
    RADIO_TX_POWER_3DBM     = RADIO_TXPOWER_TXPOWER_0dBm,  /**< 3 dBm radio transmit power.   */
#else
    RADIO_TX_POWER_4DBM     = RADIO_TXPOWER_TXPOWER_Pos4dBm,  /**< 4 dBm radio transmit power.   */
    RADIO_TX_POWER_3DBM     = RADIO_TXPOWER_TXPOWER_Pos3dBm,  /**< 3 dBm radio transmit power.   */
#endif
    RADIO_TX_POWER_0DBM     = RADIO_TXPOWER_TXPOWER_0dBm,     /**< 0 dBm radio transmit power.   */
    RADIO_TX_POWER_NEG4DBM  = RADIO_TXPOWER_TXPOWER_Neg4dBm,  /**< -4 dBm radio transmit power.  */
    RADIO_TX_POWER_NEG8DBM  = RADIO_TXPOWER_TXPOWER_Neg8dBm,  /**< -8 dBm radio transmit power.  */
    RADIO_TX_POWER_NEG12DBM = RADIO_TXPOWER_TXPOWER_Neg12dBm, /**< -12 dBm radio transmit power. */
    RADIO_TX_POWER_NEG16DBM = RADIO_TXPOWER_TXPOWER_Neg16dBm, /**< -16 dBm radio transmit power. */
    RADIO_TX_POWER_NEG20DBM = RADIO_TXPOWER_TXPOWER_Neg20dBm, /**< -20 dBm radio transmit power. */
    RADIO_TX_POWER_NEG30DBM = RADIO_TXPOWER_TXPOWER_Neg30dBm, /**< -30 dBm radio transmit power. */
    RADIO_TX_POWER_NEG40DBM = RADIO_TXPOWER_TXPOWER_Neg40dBm  /**< -40 dBm radio transmit power. */
} radio_power_t;


typedef enum
{
    RADIO_EVENT_CENTRAL_POLL_END,   /**< Event triggered on Central have polled all subevts */
    RADIO_EVENT_CENTRAL_DATA_RCV,   /**< Event triggered on Central recieved data packet */
    RADIO_EVENT_PERIPH_POLL_RCV,    /**< Event triggered on Peripheral recieved poll packet.  */
    RADIO_EVENT_PERIPH_POLL_NOT_RCV,
    RADIO_EVENT_PERIPH_DATA_SND     /**< Event triggered on Peripheral sent data packet */

} radio_evt_id_t;


typedef enum
{
  RX_SEARCH,
  RX_OPERATE
}
rx_states_t;

typedef struct
{
	radio_evt_id_t		evt_id;                     //!< Enhanced ShockBurst event ID.
	uint8_t			chan_cnt;                  //!< channel count
#ifdef CONFIG_MULTIACK_CENTRAL
	uint16_t		subevt_num;
	uint8_t			periph_num;
#endif
	uint8_t			data_len;
} radio_evt_t;


typedef void (*event_callback_t ) (radio_evt_t const * p_event);

typedef struct
{
	uint64_t logic_addr_0;		/**< Logic address 0 encoded in little endian. */
} radio_address_t;

#define	RADIO_ADDR_DEFAULT				\
{							\
	.logic_addr_0		= 0xE7E7E7E7E7,		\
}

typedef struct
{
	uint16_t		num_subevts;
	uint8_t			num_periphs;
	uint16_t		dev_num;  /* FFFF being central,
					   * for peripherials,
					   * DEV_NUM = (SUBEVT_NUM << 8) + PERIPH_NUM
					   */
	radio_address_t		address;
	radio_power_t 		tx_power;
	radio_modes_t		mode;
	event_callback_t 	event_callback;
	uint8_t * 		tx_buf;
	uint8_t *		rx_buf;
	uint8_t			tx_length;
	uint16_t *		periph_cnt;   //peripheral counts, the last element being total count  

	const uint8_t *		channel_tab;
	size_t			channel_tab_size;
	uint32_t		scan_timer_val;
	uint32_t		periph_tx_timer_val;
	uint32_t		periph_rx_search_rtc_val;
	uint32_t		periph_rtc_tick_adj_val;
} radio_init_t;


int radio_setup(const radio_init_t *init);
int radio_set_dev_num(uint16_t dev_num);
void radio_start_poll(void);
void radio_start_receive(void);
void radio_stop(void);
uint8_t radio_get_poll_packet(void);

int8_t get_rssi(void);
bool get_crc(void);


#endif //RADIO_H
