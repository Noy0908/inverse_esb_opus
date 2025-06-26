/**********************************
 *
 * Radio Config for 1 to 2 gaming 
 *
 **********************************/

#ifndef RADIO_CONFIG_H
#define RADIO_CONFIG_H


#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <nrf.h>

#define	NUM_OF_SUBEVTS	      1

#define  NUM_OF_PERIPHS	      2

// #define  TUNE_MODE            false


/**
   Debug Pins
*/
/*******************************************************************************************************************/

#if defined(CONFIG_SOC_SERIES_NRF53X)
#define PIN_CHANNEL_HOP       26 // indicate channel hopping when it is toggled
#define PIN_DATA_RX           25 // indicate data is received
#define PIN_DATA_TX            7 // indicate ack data is sent
#define PIN_DBG_01             6
#define PIN_DBG_02             5
#define PIN_DBG_03             4
#else
#define PIN_CHANNEL_HOP       31 // indicate channel hopping when it is toggled
#define PIN_DATA_RX           30 // indicate data is received
#define PIN_DATA_TX           29 // indicate ack data is sent
#define PIN_DBG_01            28
#define PIN_DBG_02             4
#define PIN_DBG_03             3
#endif

/**
   RADIO parameters
*/
/********************************************************************************************/

#define PHY_MODE	                              MODE_2_MBIT


#define RF_CHAN_TAB_SIZE			               8


#define RADIO_RTC_EVENT_TICKS		               4   // 2ms, 0.5ms per tick
			

#define CENTRAL_TIMER_SCAN_US		               1500 //1.5ms, 1 us per tick		
#define PERIPH_TIMER_SCAN_US		               2500 //2ms, 1 us per tick			

#define RTC_ADJ						               1


#define PERIPH_RTC_RX_OPERATE_PERIOD		      RADIO_RTC_EVENT_TICKS
#define PERIPH_RTC_RX_SEARCH_PERIOD			      PERIPH_RTC_RX_OPERATE_PERIOD* (RF_CHAN_TAB_SIZE +1)
#define PERIPH_RTC_RX_OPERATE_ADJ_PERIOD	      PERIPH_RTC_RX_OPERATE_PERIOD - RTC_ADJ


/** timeslot for one peripheral */
#define PERIPH_TIMER_TX_DELAY_PERIOD		      400  //us

#define CENTRAL_PKT_SIZE			               2



static const uint8_t  RF_CHANNEL_TAB[] =  {  6, 52, 76, 24, \
                                            44, 16, 64, 36 };


/*
static const uint8_t  RF_CHANNEL_TAB[] =  {  6, 6, 6, 6, \
                                             6, 6, 6, 6 };
*/                                         

#define	RADIO_ADDR_CONFIG				\
{							\
	.logic_addr_0		= 0x0741d066f7,		\
}



#define TEST_PKT_A   { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h' }

#define TEST_PKT_B	{ 'A', 'B', 'C', 'D', 'E', 'F', 'G','H'  } 


#endif //RADIO_H
