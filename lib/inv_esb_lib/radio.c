#include <zephyr/types.h>
#include <zephyr/irq.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/drivers/timer/nrf_grtc_timer.h>
#include <nrf.h>
#include <string.h>

#include "radio.h"
#include "radio_config.h"

#include <hal/nrf_radio.h>
// #include <hal/nrf_timer.h>
#include <hal/nrf_grtc.h>
// #include <helpers/nrfx_gppi.h>

// #if defined(DPPI_PRESENT)
// #include <nrfx_dppi.h>
// #else
// #include <nrfx_ppi.h>
// #endif
// #include <nrfx_timer.h>

LOG_MODULE_REGISTER(radio, CONFIG_APP_LOG_LEVEL);


/** 1 bit indicates one peripherals, calculate how many bytes is needed to indicate all the peripherals */
#define MAX_PERIPH_BIT_ARRAY_SIZE	(ROUND_UP(MAX_PERIPHS, 8) / 8)

#if defined(CONFIG_SOC_SERIES_NRF54HX) || defined(CONFIG_SOC_SERIES_NRF54LX)
#define RADIO_SHORTS_COMMON (RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_PHYEND_DISABLE_Msk)
#else
#define RADIO_SHORTS_COMMON		(RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_ADDRESS_RSSISTART_Msk |	RADIO_SHORTS_DISABLED_RSSISTOP_Msk)
#endif

#ifdef CONFIG_RADIO_PKT_CNT 
static uint16_t * m_periph_cnt; 
#endif

K_MSGQ_DEFINE(m_msgq_tx_payloads, sizeof(struct inv_esb_payload), 100, 4);


static uint8_t m_subevts;
static radio_states_t m_radio_state = IDLE_STATE;
static event_callback_t m_event_callback;
static uint8_t m_rf_chan_idx=0;
/* Byte 0 being S0 field, Byte 1 being length field, Byte 2 being S1 field,
 * rest of them contains the payload
 */
static uint8_t dma_buf[3 + MAX_PACKET_LENGTH + 1];
static volatile int8_t rssi = 0;
static volatile bool crc_ok = false;
static bool m_is_central;
static uint8_t m_dev_num;

#ifdef CONFIG_MULTIACK_PERIPH
static bool m_periph_is_poll_rcv = false;
static uint8_t loss_cnt = 0;
static bool is_rx_on;
static rx_states_t rx_state;
static uint8_t m_S;
static void timer_periph_event_handler(void);
#endif

#ifdef CONFIG_MULTIACK_CENTRAL
static uint8_t m_R[MAX_SUBEVTS][MAX_PERIPH_BIT_ARRAY_SIZE];
//32 bytes packet for poll packet
static uint8_t poll_packet[] = { 1,  2,  3,  4,  5,  6,  7,  8, 
								9, 10, 11, 12, 13, 14, 15, 16,
								17, 18, 19, 20, 21, 22, 23, 24,
								25, 26, 27, 28, 29, 30, 31, 32} ;                    /**< Packet to transmit. */

#endif


static int32_t channel1 = -1, channel2 = -1;
static uint32_t delay_value1 = 0, delay_value2 = 0;

#ifdef CONFIG_MULTIACK_DEBUG_GPIO
static const struct device *dbg_port= DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const uint8_t dbg_pins[] = {PIN_CHANNEL_HOP, PIN_DATA_RX, PIN_DATA_TX, PIN_DBG_01};

#endif

extern int leds_toggle(uint8_t idx);

// These function pointers are changed dynamically, depending on protocol configuration and state.
static void (*on_radio_rtc_interrupt)(void) = 0;
static void (*on_radio_disabled_interrupt)(void) = 0;



static uint64_t addr_fix(uint64_t total_address)
{
    /* Avoid addresses with four or more equal octets
    *  Avoid addresses with 9 or more consecutive 1s, or 9 or more consecutive 0s
    *  Avoid addresses with more than 7 consecutive toggles. This avoids having an 8-bit preamble reappearing 
	*  in the address 01010101 or 10101010. */

	int totalBits = 40;
	uint64_t correction_mask_4oct = 0x84422110;
	uint64_t bitmask_9bit = 0x1FF;
	uint64_t correction_mask_9bit = 0x40;
	uint64_t bitmask_8bit = 0xFF;
	uint64_t correction_mask_8bit = 0x20;

	/* Run the detection algorithm
	* Algo for 40 bits (5 bytes) base address
	* Theoretically it might be necessary to run through the loop multiple times, 
	* if applying one correction can break one of the other rules? */

	int loops;
	for(loops = 0; loops < 3; loops++)
	{
		int corrections_applied = 0;
		int i;
		int j;

		// Get the address byte components
		uint8_t octets[5];
		for(i = 0; i < sizeof(octets); i++)
		{
			octets[i] = (total_address >> (i*8));
		}
		// Check for 4 or more equal octets
		for(j = 0; j < 2; j++)
		{
			uint8_t diff_cnt = j;
			uint8_t diff_pos = sizeof(octets);

			// Find the number of octets different from the initial octet
			for(i = j + 1; i < sizeof(octets); i++)
			{
				if (octets[i] != octets[j])
				{
					diff_cnt++;
					diff_pos = i;
				}
			}

			// Correct the address if only 0 or 1 octet is found
			if (diff_cnt <= 1)
			{
				uint64_t rhs_bitmask = (1U << (diff_pos * 8)) - 1;
				uint64_t lhs_bitmask = ~rhs_bitmask;
				uint64_t rhs_xor = correction_mask_4oct & rhs_bitmask;
				uint64_t lhs_xor = correction_mask_4oct & lhs_bitmask;

				total_address ^= ((lhs_xor << 8) | rhs_xor);
				corrections_applied++;
				break;
			}
		}

		for(i = 0; i <= (totalBits - 9); i++)
		{
			// Check for 9 consecutive zeros
			if(((total_address >> i) & bitmask_9bit) == 0)
			{
				// Flip one bit to break the line of consecutive 0's
				total_address ^= (correction_mask_9bit << i);
				corrections_applied++;
			}
			// Check for 9 consecutive ones
			else if(((total_address >> i) & bitmask_9bit) == bitmask_9bit)
			{
				// Flip one bit to break the line of consecutive 1's
				total_address ^= (correction_mask_9bit << i);
				corrections_applied++;
			}
			// Check for more than 7 consecutive toggles
			else if(((total_address >> i) & bitmask_8bit) == 0x55 || ((total_address >> i) & bitmask_8bit) == 0xAA)
			{
				// FLip one bit to break the line of consecutive toggles
				total_address ^= (correction_mask_8bit << i);
				corrections_applied++;
			}
		}
		// Include the last bit which needs to be tested for the 8 bit mask
		i = totalBits - 8;
		if(((total_address >> i) & bitmask_8bit) == 0x55 || ((total_address >> i) & bitmask_8bit) == 0xAA)
		{
			// FLip one bit to break the line of consecutive toggles
			total_address ^= (correction_mask_8bit << i);
			corrections_applied++;
		}
		if(corrections_applied == 0) break;
	}

	return total_address;
}

static void base_addr_fill_in(void)
{
	radio_address_t m_radio_addr = RADIO_ADDR_CONFIG;
	uint64_t total_address;
	uint32_t d_subevts = 256U * m_subevts / NUM_OF_SUBEVTS;
	uint64_t addr_subevts = d_subevts ^ (d_subevts<<8) ^ (d_subevts<<16) ^
				(d_subevts<<24) ^ ((uint64_t)d_subevts<<32);
	uint8_t prefix0;

	total_address = addr_fix(m_radio_addr.logic_addr_0 ^ addr_subevts);

	NRF_RADIO->BASE0 = total_address;
	prefix0 = (total_address >> 32);

	NRF_RADIO->BASE1 = total_address;

	NRF_RADIO->PREFIX0 = ((total_address >> 24) & 0xFF00) | prefix0;
}


int8_t get_rssi()
{
    return rssi;
}

bool get_crc()
{
    return crc_ok;
}


static struct onoff_manager *clk_mgr;
static struct onoff_client clk_cli;

__INLINE static int hf_clock_start(void)
{
	int err;
	int res;

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

	// LOG_DBG("HF clock started");
	return 0;
}


__INLINE static void hf_clock_stop( void )
{	
	//Stop HF clock
    // int err;

	// if (!clk_mgr) {
	// 	return;
	// }
	// err = onoff_cancel_or_release(clk_mgr, &clk_cli);

	// clk_mgr = NULL;
}



#ifdef CONFIG_MULTIACK_DEBUG_GPIO
static void dbg_pins_init(void)
{
	int err;
	
	if (!device_is_ready(dbg_port)) 
	{
		LOG_ERR("Could not bind to debug port");
	}
	
	for (size_t i = 0; i < ARRAY_SIZE(dbg_pins); i++) 
	{
		err = gpio_pin_configure(dbg_port, dbg_pins[i], GPIO_OUTPUT);
		if (err) 
		{
			LOG_ERR("Unable to configure radio debug port");
			dbg_port = NULL;
		}
						
		gpio_pin_set(dbg_port, dbg_pins[i], 0); 
	}
}
#endif


/* GRTC timer compare interrupt handler */
static void grtc_interrupt_handler(int32_t id, uint64_t expire_time, void *user_data)
{
	if(id == channel1)
	{
	#ifdef CONFIG_MULTIACK_DEBUG_GPIO
		gpio_pin_toggle(dbg_port, PIN_CHANNEL_HOP);
	#endif   
		if(on_radio_rtc_interrupt)
			on_radio_rtc_interrupt();
	}
	else if(id == channel2)
	{
	#ifdef CONFIG_MULTIACK_CENTRAL
		if(m_radio_state == CENTRAL_RX_STATE)
		{
			NRF_RADIO->TASKS_DISABLE = 1;	//disable radio for receive timeout
		}
	#else
		timer_periph_event_handler();
	#endif
	}
	else
	{
		LOG_ERR("Invalid channel ID: %d", id);
		return;
	}
}

/**@brief Function for setting the RTC Capture Compare register 0, and enabling the corresponding
 *        event.
 *
 * @param[in] value   New value of Capture Compare register 0.
 */
__INLINE static int radio_grtc_compare0_set(uint32_t value_us)
{
	int err;
	uint64_t set_ticks = 0;

	if(channel1 <= 0) {
		/** Allocate GRTC capture/compare channel */
		channel1 = z_nrf_grtc_timer_chan_alloc();
		if (channel1 <= 0) {
			LOG_ERR("Failed to allocate GRTC channel, chan=%d\n", channel1);
		}
		LOG_INF("Allocated GRTC channel %d\n", channel1);
	}

	delay_value1 = value_us;

	set_ticks = z_nrf_grtc_timer_get_ticks(K_USEC(value_us));
	/** Set compare channel to given value */
	err = z_nrf_grtc_timer_set(channel1, set_ticks, grtc_interrupt_handler, NULL);
	if(err)
	{
		LOG_ERR("Failed to set GRTC timer, err=%d\n", err);
		return err;
	}

	return err;
}


/**
 * @brief Reset the GRTC timer compare channel.
 *
 * This function resets the GRTC timer compare channel by allocating a new channel
 * and setting the compare value. It should be called when the GRTC timer needs to
 * be reset with a new value.
 *
 * @param value_ms The time in milliseconds to set for the GRTC timer.
 * @return 0 on success, negative error code on failure.
 */
__INLINE static int radio_grtc_compare1_set(uint32_t value_us)
{
	int err;
	uint64_t set_ticks = 0;

	if(channel2 <= 0) {
		/** Allocate GRTC capture/compare channel */
		channel2 = z_nrf_grtc_timer_chan_alloc();
		if (channel2 <= 0) {
			LOG_ERR("Failed to allocate GRTC channel, chan=%d\n", channel2);
		}
		LOG_INF("Allocated GRTC channel %d\n", channel2);
	}

	delay_value2 = value_us;

	set_ticks = z_nrf_grtc_timer_get_ticks(K_USEC(value_us));
	/** Set compare channel to given value */
	err = z_nrf_grtc_timer_set(channel2, set_ticks, grtc_interrupt_handler, NULL);
	if(err)
	{
		LOG_ERR("Failed to set GRTC timer, err=%d\n", err);
		return err;
	}

	return err;
}


__INLINE static void radio_grtc_clear_count(void)
{
	// NRF_GRTC->TASKS_CLEAR =1;
}	


static void radio_set_tx_power(radio_power_t power)
{
    NRF_RADIO->TXPOWER = power;
}


 __INLINE static void radio_hop_channel(void)
{
	m_rf_chan_idx =(m_rf_chan_idx+1)% (sizeof(RF_CHANNEL_TAB));  	
	NRF_RADIO->FREQUENCY = RF_CHANNEL_TAB[m_rf_chan_idx];
}


__INLINE static void radio_disable(void)
{
	NRF_RADIO->EVENTS_DISABLED = 0;
	NRF_RADIO->TASKS_DISABLE = 1;
	while (NRF_RADIO->EVENTS_DISABLED == 0) {
		/* wait for register to settle */
	}	
}


#ifdef CONFIG_MULTIACK_CENTRAL

void increase_poll_index(void)
{
	poll_packet[0]++;
}


static void central_send_poll_packet(void)
{
	/** Update BASE0 and BASE1   **/
	base_addr_fill_in();

	memcpy(&dma_buf[3], m_R[m_subevts], PERIPH_BM_SIZE);

	memcpy(&dma_buf[3 + PERIPH_BM_SIZE], poll_packet, CENTRAL_PKT_SIZE);

	dma_buf[2] = 0;
	dma_buf[1] = PERIPH_BM_SIZE + CENTRAL_PKT_SIZE;
	dma_buf[0] = 0;

	//Start poll packet TX
	m_radio_state = CENTRAL_TX_STATE;
	NRF_RADIO->TASKS_TXEN = 1;
	
#ifdef CONFIG_MULTIACK_DEBUG_GPIO
	gpio_pin_set(dbg_port, PIN_DATA_TX, 1);
#endif

	static uint32_t timeCount = 0;
	if(0 == (timeCount++ % 100))
		leds_toggle(3);


#ifdef CONFIG_RADIO_PKT_CNT 	
	m_periph_cnt[NUM_OF_PERIPHS]++;
#endif     
}

static void rtc_central_event_handler(void)
{
	radio_grtc_clear_count();

	// //Start HF clock for radio and timer
	// hf_clock_start();
	if (m_radio_state != IDLE_STATE) {
		m_radio_state = IDLE_STATE;
		NRF_RADIO->TASKS_DISABLE = 1;
	} 

	NRF_RADIO->SHORTS       =  RADIO_SHORTS_COMMON;

	NRF_RADIO->INTENSET00     =  RADIO_INTENSET00_DISABLED_Msk; 

	radio_hop_channel();

	central_send_poll_packet();

	radio_grtc_compare0_set(RADIO_RTC_EVENT_TICKS);
}


static void on_central_disabled(void)
{
	if(m_radio_state == CENTRAL_TX_STATE)
	{
#ifdef CONFIG_MULTIACK_DEBUG_GPIO
		gpio_pin_set(dbg_port, PIN_DATA_TX, 0);
#endif
		//modify radio shorts without end -> disable
		NRF_RADIO->SHORTS = (1 << RADIO_SHORTS_READY_START_Pos);
	
		NRF_RADIO->INTENSET00     = RADIO_INTENSET00_PHYEND_Msk;
		//go to CENTRAL_RX_STATE
		NRF_RADIO->TASKS_RXEN = 1;	
		m_radio_state = CENTRAL_RX_STATE;
				
#ifdef CONFIG_MULTIACK_DEBUG_GPIO
		gpio_pin_set(dbg_port, PIN_DATA_RX, 1);
#endif
		// radio_timer_clear_start(CENTRAL_TIMER_SCAN_US);		
		radio_grtc_compare1_set(CENTRAL_TIMER_SCAN_US); 
	}
	else if (m_radio_state == CENTRAL_RX_STATE)
	{
		m_radio_state = IDLE_STATE;
		// radio_timer_stop();
		if( m_subevts >= (NUM_OF_SUBEVTS - 1) )
		{				
#ifdef CONFIG_MULTIACK_DEBUG_GPIO
			gpio_pin_set(dbg_port, PIN_DATA_RX, 0);
#endif
			m_subevts = 0;				
			hf_clock_stop();
			radio_evt_t event;
			//Callback to application
			event.evt_id = RADIO_EVENT_CENTRAL_POLL_END;
			m_event_callback(&event);
		}
		else
		{
			NRF_RADIO->SHORTS =  RADIO_SHORTS_COMMON;  //re=enable RADIOI END -> DISABLE
	#ifdef CONFIG_MULTIACK_DEBUG_GPIO
			gpio_pin_set(dbg_port, PIN_DATA_RX, 0);
	#endif
			m_subevts = (m_subevts +1) % NUM_OF_SUBEVTS;
			central_send_poll_packet();
		}
	}						
}	

static void on_central_end(void)
{
	uint8_t i;
	if (m_radio_state == CENTRAL_RX_STATE)
	{
		if(NRF_RADIO->CRCSTATUS & (RADIO_CRCSTATUS_CRCSTATUS_CRCOk << RADIO_CRCSTATUS_CRCSTATUS_Pos))
		{
			int err;
			uint8_t rx_pid;
			uint16_t subevts_no = m_subevts;
			uint8_t periph_no = dma_buf[0];
			uint8_t len = dma_buf[1];
			uint8_t pid = dma_buf[2];

			NRF_RADIO->TASKS_START =1;  //restart rx

			if (len > MAX_PAYLOAD_SIZE) 
			{
				err = -EMSGSIZE;
			} 
			else if (periph_no && periph_no <= NUM_OF_PERIPHS) 
			{
				i = periph_no - 1;
				uint8_t bit_pos = (i & 0x7);
				uint8_t pos = (i >> 3);

				rx_pid = (m_R[subevts_no][pos] >> bit_pos) & 0x1;

				if (rx_pid == (pid & 0x1)) 
				{
					m_R[subevts_no][pos] ^= BIT(bit_pos);

					//Callback to application
					radio_evt_t event;
					event.evt_id = RADIO_EVENT_CENTRAL_DATA_RCV;
					event.chan_cnt = m_rf_chan_idx;
					event.subevt_num = subevts_no;
					event.periph_num = periph_no;
					memcpy(event.data, &dma_buf[3], len);
					event.data_len = len;
					// LOG_INF("Rec %d from %d: %d", event.data_len, event.periph_num, event.data[0]);
					m_event_callback(&event);

				#ifdef CONFIG_MULTIACK_DEBUG_GPIO
					gpio_pin_toggle(dbg_port, PIN_DBG_01);
				#endif
				} 
				else 
				{
					err = -EPROTO;
				}
			} 
			else 
			{
				err = -EINVAL;
			}

		#ifdef CONFIG_RADIO_PKT_CNT 
			 m_periph_cnt[periph_no-1]++;
		#endif	 
		}
	}
}


void radio_start_poll(void)
{
	radio_grtc_compare0_set(RADIO_RTC_EVENT_TICKS);
}

#endif


#ifdef CONFIG_MULTIACK_PERIPH

static void inv_esb_start_tx(const struct inv_esb_payload *payload)
{
	//Set dma_buf to data
	if (payload->length <= MAX_PACKET_LENGTH + 1) {
		memcpy(&dma_buf[3], payload->data, payload->length);
	}

	dma_buf[2] = m_S;
	dma_buf[1] = payload->length;
	dma_buf[0] = m_dev_num;		//periph_num
 
	//Set TX state in advance
	m_radio_state = PERIPH_TX_STATE;

	radio_grtc_compare1_set(PERIPH_TIMER_TX_DELAY_PERIOD * m_dev_num);
}


static void send_packet_from_tx_msgq(void)
{
	static struct inv_esb_payload tx_payload;

	if (k_msgq_peek(&m_msgq_tx_payloads, &tx_payload) == 0) 
	{
		inv_esb_start_tx(&tx_payload);
	}
}

void delete_tx_item_from_queue(void)
{
	struct inv_esb_payload tmp_payload;
	if (k_msgq_get(&m_msgq_tx_payloads, &tmp_payload, K_NO_WAIT)) 
	{
		// LOG_ERR("Failed to delete payload from msgq");
	}
}


int inv_esb_package_enqueue(uint32_t idx, uint8_t *buf, uint32_t length)
{
	int ret = 0;
	static struct inv_esb_payload tx_payload;
	if (length > MAX_PAYLOAD_SIZE) {
		LOG_ERR("Payload length %d exceeds maximum %d", length, MAX_PAYLOAD_SIZE);
		return -EMSGSIZE;
	}

	tx_payload.data[0] = idx & 0xFF;  // Set the first byte as the index
	tx_payload.data[1] = (idx >> 8) & 0xFF; // Set the second byte as the index high byte
	tx_payload.data[2] = (idx >> 16) & 0xFF; // Set the third byte as the index high byte
	tx_payload.data[3] = (idx >> 24) & 0xFF; // Set the fourth byte as the index high byte

	memcpy(&tx_payload.data[4], buf, length);
	tx_payload.length = length + 4;
	ret = k_msgq_put(&m_msgq_tx_payloads, &tx_payload, K_NO_WAIT);
	if (ret)  {
		// LOG_INF("Audio message queue is full");
		return -ENOMEM;
	}
	return ret;
}


static void rtc_periph_event_handler(void)
{	
	if (m_radio_state != IDLE_STATE) {
		m_radio_state = IDLE_STATE;
		NRF_RADIO->TASKS_DISABLE = 1;
	} 

	if (rx_state== RX_OPERATE)
	{
		if ( loss_cnt == sizeof(RF_CHANNEL_TAB))
		{
			rx_state = RX_SEARCH;
			is_rx_on = false;				//make sure that the receiver continuously on 
			radio_grtc_clear_count();
			radio_grtc_compare0_set(PERIPH_RTC_RX_SEARCH_PERIOD);
		}
		else
		{
			radio_grtc_clear_count();
			radio_grtc_compare0_set(PERIPH_RTC_RX_OPERATE_PERIOD);
			loss_cnt++;
		}  
		
		// hf_clock_start();
		radio_hop_channel();
		NRF_RADIO->TASKS_RXEN  = 1;
	
#ifdef CONFIG_MULTIACK_DEBUG_GPIO
		gpio_pin_set(dbg_port, PIN_DATA_RX, 1);
#endif
		
		// radio_grtc_compare1_set(PERIPH_TIMER_SCAN_US);
		m_radio_state = PERIPH_RX_STATE;				
   }
   else if (rx_state== RX_SEARCH)
   {
	   radio_grtc_clear_count();   
	   radio_grtc_compare0_set(PERIPH_RTC_RX_SEARCH_PERIOD);
	   //For power saving, turn off scan for RX_SEARCH_PERIOD
		if(is_rx_on)
		{
			is_rx_on = false;		
			radio_disable();
					
#ifdef CONFIG_MULTIACK_DEBUG_GPIO
			gpio_pin_set(dbg_port, PIN_DATA_RX, 0);
#endif
			hf_clock_stop();	
			m_radio_state = IDLE_STATE;		
		}
		else
		{
			is_rx_on = true;	
			// hf_clock_start();
			radio_hop_channel();			
			NRF_RADIO->TASKS_RXEN =1;

#ifdef CONFIG_MULTIACK_DEBUG_GPIO
			gpio_pin_set(dbg_port, PIN_DATA_RX, 1);
#endif
			m_radio_state = PERIPH_RX_STATE;		
		}   
   }
   else
   {
		LOG_ERR("m_radio_state = UNKNOWN_STATE !!");   
   }
}

static void timer_periph_event_handler(void)
{	
	/** start radio TX */
	if(m_radio_state == PERIPH_TX_STATE)
	{
		NRF_RADIO->TASKS_TXEN = 1;
	#ifdef CONFIG_MULTIACK_DEBUG_GPIO
		gpio_pin_set(dbg_port, PIN_DATA_TX, 1);
	#endif
	}
	else if (m_radio_state == PERIPH_RX_STATE) 
	{
		if(rx_state== RX_OPERATE)
		{
			NRF_RADIO->TASKS_DISABLE = 1;
		}
	}
}


static bool peripheral_handle_poll_packet(void)
{
	/** notice application that you have received the poll packet */
	radio_evt_t	m_radio_event;
	uint8_t len = dma_buf[1];
	if (len > PERIPH_BM_SIZE) {
		len -= PERIPH_BM_SIZE;
	} else {
		len = 0;
	}

	if(m_dev_num!=0)
	{
		// Mask out the flow control bit
		uint8_t i = m_dev_num - 1;
		uint8_t bit_pos = (i & 0x7);
		uint8_t pos = (i >> 3);
		uint8_t pid = (dma_buf[3 + pos] >> bit_pos) & 0x01;
		/** last response packet has been received by central */
		if (pid != m_S) 
		{
			m_S ^= 0x1;
			m_radio_event.data[0] = 1; // Set the first byte to indicate that previous response is sent
		}
	}	

	/** notice application that you have received the poll packet */
	m_radio_event.evt_id = RADIO_EVENT_PERIPH_POLL_RCV;
	m_radio_event.chan_cnt = m_rf_chan_idx;
	memcpy(&m_radio_event.data[1], &dma_buf[3 + PERIPH_BM_SIZE], len);
	m_radio_event.data_len = len + 1;
	m_event_callback(&m_radio_event);

	return true;
}


static void on_periph_disabled(void)
{
	if(m_radio_state == PERIPH_RX_STATE)
	{	
#ifdef CONFIG_MULTIACK_DEBUG_GPIO
		gpio_pin_set(dbg_port, PIN_DATA_RX, 0);  //Stop RX 
#endif
		m_radio_state = IDLE_STATE;
	
		/** Radio generate irq after packet received finished */	
		if ((NRF_RADIO->CRCSTATUS & (RADIO_CRCSTATUS_CRCSTATUS_CRCOk << RADIO_CRCSTATUS_CRCSTATUS_Pos)) &&
		    !dma_buf[0])  // check also it is the poll packet that sent by central but not the other peripherals
		{
#ifdef CONFIG_MULTIACK_DEBUG_GPIO
			gpio_pin_set(dbg_port, PIN_DBG_01, 1);
#endif
			//poll packet received	
			m_periph_is_poll_rcv = true;
			rssi = -NRF_RADIO->RSSISAMPLE;
			loss_cnt = 0;   
			// LOG_INF("Poll packet received, RSSI: %d", rssi);
			peripheral_handle_poll_packet();
			                   
#ifdef CONFIG_MULTIACK_DEBUG_GPIO
			gpio_pin_set(dbg_port, PIN_DBG_01, 0);
#endif
			// ready to send data back to central
			if (rx_state == RX_SEARCH)
			{
				rx_state = RX_OPERATE;
			}

			//Reload PERIPH_RTC_RX_OPERATE_ADJ_PERIOD to RADIO RTC
			radio_grtc_compare0_set(PERIPH_RTC_RX_OPERATE_ADJ_PERIOD);

			/** start transmit packet from message queue */
			send_packet_from_tx_msgq();
		}
	}
	else if (m_radio_state == PERIPH_TX_STATE)
	{
#ifdef CONFIG_MULTIACK_DEBUG_GPIO
			gpio_pin_set(dbg_port, PIN_DATA_TX, 0);
#endif	

			//Stops HFCLK;
			hf_clock_stop();
			m_radio_state = IDLE_STATE;
	}		
}	

void radio_start_receive(void)
{	
	radio_grtc_compare0_set(PERIPH_RTC_RX_SEARCH_PERIOD); 
	// hf_clock_start();   	   
	is_rx_on = true;
	rx_state = RX_SEARCH;
	m_radio_state =  PERIPH_RX_STATE;

	NRF_RADIO->EVENTS_DISABLED = 0;
	NRF_RADIO->SHORTS = RADIO_SHORTS_COMMON;
	NRF_RADIO->INTENSET00 = RADIO_INTENSET00_DISABLED_Msk;
	NRF_RADIO->TASKS_RXEN = 1;

}


/**
 * @brief Set the device number for the radio.
 *
 * @param dev_num Device number to set.
 * @return 0 on success, negative error code on failure.
 */
int radio_set_dev_num(uint16_t dev_num)
{
	int err;
	uint8_t subevts = (dev_num >> 8);
	uint8_t dev_no = dev_num & 0xFF;

	if (subevts >= NUM_OF_SUBEVTS ||
	    !dev_no || dev_no > NUM_OF_PERIPHS) {
		err = -EINVAL;
	} else if (m_radio_state != IDLE_STATE) {
		err = -EBUSY;
	} else if (m_is_central) {
		err = -EPERM;
	} else {
		m_subevts = subevts;
		m_dev_num = dev_no;
		base_addr_fill_in();
		err = 0;
	}

	return err;
}

#endif




void radio_stop(void)
{
	//Stop RTC
	// radio_rtc_stop_clear();

	//Stop radio
	NRF_RADIO->SHORTS = 0;
	NRF_RADIO->INTENCLR00 = 0xFFFFFFFF;
	NRF_RADIO->EVENTS_DISABLED = 0;
	NRF_RADIO->TASKS_DISABLE = 1;
	while (NRF_RADIO->EVENTS_DISABLED == 0) {
		/* wait for register to settle */
	}

	m_radio_state = IDLE_STATE;
}
	
/**
 * @brief Handler for radio interrupt events.
 */
void radio_irq_handler(void)
{
	if (nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_PHYEND) &&
	    (NRF_RADIO->INTENSET00 & RADIO_INTENSET00_PHYEND_Msk)) {
		nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_PHYEND);

	#ifdef CONFIG_MULTIACK_CENTRAL
		if (m_is_central) {
			on_central_end();
		}
	#endif
	}

    if (NRF_RADIO->EVENTS_DISABLED && (NRF_RADIO->INTENSET00 & RADIO_INTENSET00_DISABLED_Msk))
    {		
        NRF_RADIO->EVENTS_DISABLED = 0;	
		if(on_radio_disabled_interrupt)
			on_radio_disabled_interrupt();
    }
}


ISR_DIRECT_DECLARE(RADIO_0_IRQHandler)
{
	radio_irq_handler();
	return 1;
}


int radio_setup(const radio_init_t *init)
{
	uint8_t lf_length = (MAX_PACKET_LENGTH>63)?8:6;
	uint8_t s1_length = 1;
	
#ifdef CONFIG_MULTIACK_CENTRAL
	for (uint16_t i = 0; i < NUM_OF_SUBEVTS; i++) {
		memset(m_R[i], 0, PERIPH_BM_SIZE);
	}
#endif

#ifdef CONFIG_MULTIACK_PERIPH
	m_S = 0;
#endif

    m_event_callback = init->event_callback;
    m_is_central = (init->dev_num ==0xFFFF);
	m_subevts = (m_is_central)? 0: (init->dev_num >> 8);
	m_dev_num = init->dev_num & 0xFF;

	if (!m_is_central &&
	    unlikely(m_subevts >= NUM_OF_SUBEVTS ||
		     m_dev_num > NUM_OF_PERIPHS)) {
		return -EINVAL;
	}

#ifdef CONFIG_RADIO_PKT_CNT 
	m_periph_cnt = init->periph_cnt;
#endif	
	hf_clock_start();   //<-- for testing only

	if (m_is_central) {
#ifdef CONFIG_MULTIACK_CENTRAL
		on_radio_rtc_interrupt = rtc_central_event_handler;
		on_radio_disabled_interrupt = on_central_disabled;
#else
		return -ENOTSUP;
#endif
	} else {
#ifdef CONFIG_MULTIACK_PERIPH
		on_radio_rtc_interrupt = rtc_periph_event_handler;
		on_radio_disabled_interrupt = on_periph_disabled;
#else
		return -ENOTSUP;
#endif
	}

	/**  Set fast ramp */
	NRF_RADIO->TIMING = 1;   

    /* Should trigger tasks_softreset for nRF54HX after header fix */
#if defined(CONFIG_SOC_SERIES_NRF54LX)
	NRF_RADIO->TASKS_SOFTRESET = 1;
#elif !defined(CONFIG_SOC_SERIES_NRF54HX)
	NRF_RADIO->POWER = 1;
#endif
	
    NRF_RADIO->PCNF0        =   (((1UL) << RADIO_PCNF0_S0LEN_Pos                               ) & RADIO_PCNF0_S0LEN_Msk)
                              | (((s1_length) << RADIO_PCNF0_S1LEN_Pos                               ) & RADIO_PCNF0_S1LEN_Msk)
                              | (((lf_length) << RADIO_PCNF0_LFLEN_Pos                               ) & RADIO_PCNF0_LFLEN_Msk);

    NRF_RADIO->PCNF1        =   (((RADIO_PCNF1_ENDIAN_Big )        << RADIO_PCNF1_ENDIAN_Pos    ) & RADIO_PCNF1_ENDIAN_Msk)
                              | (((4UL)                            << RADIO_PCNF1_BALEN_Pos     ) & RADIO_PCNF1_BALEN_Msk)
                              | (((0UL)                            << RADIO_PCNF1_STATLEN_Pos   ) & RADIO_PCNF1_STATLEN_Msk)
                              | ((((uint32_t) MAX_PACKET_LENGTH)   << RADIO_PCNF1_MAXLEN_Pos    ) & RADIO_PCNF1_MAXLEN_Msk)
                              | ((RADIO_PCNF1_WHITEEN_Disabled     << RADIO_PCNF1_WHITEEN_Pos   ) & RADIO_PCNF1_WHITEEN_Msk);
   
	NRF_RADIO->CRCCNF       =   (((RADIO_CRCCNF_SKIPADDR_Include)    << RADIO_CRCCNF_SKIPADDR_Pos ) & RADIO_CRCCNF_SKIPADDR_Msk)
								| (((RADIO_CRCCNF_LEN_Two)           << RADIO_CRCCNF_LEN_Pos      ) & RADIO_CRCCNF_LEN_Msk);
	NRF_RADIO->CRCINIT 			= 0xFFFFUL;
	NRF_RADIO->CRCPOLY      = 0x11021UL;
	NRF_RADIO->SHORTS       = RADIO_SHORTS_COMMON;
    
    switch(init->mode)
    {	
        case MODE_1_MBIT:
            NRF_RADIO->MODE = (RADIO_MODE_MODE_Ble_1Mbit << RADIO_MODE_MODE_Pos) & RADIO_MODE_MODE_Msk;
#if defined(CONFIG_SOC_SERIES_NRF53X) //errata 117
			*((volatile uint32_t *)0x41008588) = *((volatile uint32_t *)0x01FF0080);
#endif
            break;
        case MODE_2_MBIT:
            NRF_RADIO->MODE = (RADIO_MODE_MODE_Ble_2Mbit << RADIO_MODE_MODE_Pos       ) & RADIO_MODE_MODE_Msk;
#if defined(CONFIG_SOC_SERIES_NRF53X)	//errata 117
			*((volatile uint32_t *)0x41008588) = *((volatile uint32_t *)0x01FF0084);
#endif
            break;
        default:
            break;
    }

	//Radio interrupt settings
	IRQ_DIRECT_CONNECT( RADIO_0_IRQn, 2 , RADIO_0_IRQHandler, 0);
	irq_enable(RADIO_0_IRQn);					
	NVIC_ClearPendingIRQ(RADIO_0_IRQn);
	NVIC_SetPriority(RADIO_0_IRQn, 2);
	NVIC_EnableIRQ(RADIO_0_IRQn);     
	NRF_RADIO->INTENSET00 = RADIO_INTENSET00_DISABLED_Msk;         
	base_addr_fill_in();
    NRF_RADIO->PACKETPTR    = (uint32_t)dma_buf;

	radio_set_tx_power(RADIO_TX_POWER_0DBM);

	NRF_RADIO->TXADDRESS	= 0;		//to transmit at pipe 0
	NRF_RADIO->RXADDRESSES	= 0x01;		//turn on pipe 0 only to receive

	if (m_rf_chan_idx >= sizeof(RF_CHANNEL_TAB)) {
		m_rf_chan_idx = 0;
	}

    NRF_RADIO->FREQUENCY    = RF_CHANNEL_TAB[m_rf_chan_idx];  
    memset(dma_buf, 0, sizeof(dma_buf));

#ifdef CONFIG_MULTIACK_DEBUG_GPIO
	dbg_pins_init();
#endif
	return 0;
}


