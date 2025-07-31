#include <zephyr/kernel.h>
#include <zephyr/audio/dmic.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/logging/log.h>
#include "sound_service.h"
#include "mic_work_event.h"
#include "esb_handler.h"

LOG_MODULE_REGISTER(sound_service, LOG_LEVEL_INF);

#define SOUND_STACK_SIZE        20480
#define SOUND_PRIORITY          5

/* Milliseconds to wait for a block to be read. */
#define READ_TIMEOUT            1000

/** opus variables and functions */
#define OPUS_ENCODER_SIZE   7180


__ALIGN(4) static uint8_t m_opus_encoder[OPUS_ENCODER_SIZE];
static OpusEncoder * const m_opus_encoder_state = (OpusEncoder *)m_opus_encoder;

static uint8_t m_opus_complexity = CONFIG_OPUS_COMPLEXITY;
static int32_t m_opus_bitrate    = ((CONFIG_OPUS_BITRATE != 0) ? CONFIG_OPUS_BITRATE : OPUS_AUTO);
static bool m_opus_vbr        = ((CONFIG_OPUS_BITRATE == 0) || (CONFIG_OPUS_VBR_ENABLED != 0));
static uint8_t m_opus_channels   = CONFIG_OPUS_CHANNELS;


static void opus_encoder_configure(void)
{
	printk("opus_encoder_get_size() = %d\n", opus_encoder_get_size(m_opus_channels));
	__ASSERT_NO_MSG(opus_encoder_get_size(m_opus_channels) <= sizeof(m_opus_encoder));
	__ASSERT_NO_MSG(opus_encoder_init(m_opus_encoder_state, CONFIG_AUDIO_SAMPLING_FREQUENCY, m_opus_channels, OPUS_APPLICATION_RESTRICTED_LOWDELAY) == OPUS_OK);

	__ASSERT_NO_MSG(opus_encoder_ctl(m_opus_encoder_state, OPUS_SET_BITRATE(m_opus_bitrate))                      == OPUS_OK);
	__ASSERT_NO_MSG(opus_encoder_ctl(m_opus_encoder_state, OPUS_SET_VBR(m_opus_vbr))                              == OPUS_OK);
	__ASSERT_NO_MSG(opus_encoder_ctl(m_opus_encoder_state, OPUS_SET_VBR_CONSTRAINT((m_opus_bitrate != OPUS_AUTO)))== OPUS_OK);

	__ASSERT_NO_MSG(opus_encoder_ctl(m_opus_encoder_state, OPUS_SET_COMPLEXITY(m_opus_complexity))                == OPUS_OK);

	__ASSERT_NO_MSG(opus_encoder_ctl(m_opus_encoder_state, OPUS_SET_SIGNAL(OPUS_AUTO))                            == OPUS_OK);
	__ASSERT_NO_MSG(opus_encoder_ctl(m_opus_encoder_state, OPUS_SET_LSB_DEPTH(8))                                == OPUS_OK);
	__ASSERT_NO_MSG(opus_encoder_ctl(m_opus_encoder_state, OPUS_SET_DTX(0))                                       == OPUS_OK);
	__ASSERT_NO_MSG(opus_encoder_ctl(m_opus_encoder_state, OPUS_SET_INBAND_FEC(0))                                == OPUS_OK);
	__ASSERT_NO_MSG(opus_encoder_ctl(m_opus_encoder_state, OPUS_SET_PACKET_LOSS_PERC(0))                          == OPUS_OK);
}

/*********************************opus decoder*********************************************/
/******************************** opus decoder variables ******************************************/
#define OPUS_DECODER_SIZE   			9224
__ALIGN(4) static uint8_t m_opus_decoder[OPUS_DECODER_SIZE];
static OpusDecoder * const m_opus_decoder_state = (OpusDecoder *)m_opus_decoder;


static void opus_decoder_configure(void)
{
        printk("opus_decoder_get_size() = %d\n", opus_decoder_get_size(m_opus_channels));        
        __ASSERT_NO_MSG(opus_decoder_get_size(m_opus_channels) <= sizeof(m_opus_decoder));
        __ASSERT_NO_MSG(opus_decoder_init(m_opus_decoder_state, CONFIG_AUDIO_SAMPLING_FREQUENCY, m_opus_channels) == OPUS_OK);
}


static bool radio_is_up;


static void mic_data_handle(void *, void *, void *)
{
    void *buffer;
	uint32_t size;

	opus_encoder_configure();

	opus_decoder_configure();	//test

    LOG_INF("Sound service start, wait for PCM data......");

	/** suspend the thread until we received a start event*/
	k_thread_suspend(k_current_get());

    while(1)
    {
	#if 1
        int frame_size;
		uint8_t frame_buf[CONFIG_AUDIO_FRAME_SIZE_BYTES];
		/***** test *********************/
		int decompressed_frame_size;
		int16_t frame_buf_uncompressed[CONFIG_AUDIO_FRAME_SIZE_SAMPLES];
		/****************************** */
        size = read_audio_data(&buffer, READ_TIMEOUT);
        if(size)
        {	
			frame_size = opus_encode(
									m_opus_encoder_state,
									buffer,
									CONFIG_AUDIO_FRAME_SIZE_SAMPLES,
									frame_buf,
									CONFIG_AUDIO_FRAME_SIZE_BYTES
									);
			// if(frame_size != CONFIG_AUDIO_FRAME_SIZE_BYTES)
			// 	LOG_INF("%d", frame_size);
			LOG_INF("Packet send[%d], 0x%02x, 0x%02x, 0x%02x, 0x%02x  ", frame_size,			
				 frame_buf[0],frame_buf[1], frame_buf[2],frame_buf[3]);							
			
			// decompressed_frame_size =  opus_decode(m_opus_decoder_state, 
            //                             frame_buf, 
            //                             frame_size, 
            //                             frame_buf_uncompressed, 
            //                             CONFIG_AUDIO_FRAME_SIZE_SAMPLES, 0);
			// LOG_INF("%d", decompressed_frame_size);

			inv_esb_package_enqueue(frame_buf, frame_size);
	
            free_audio_memory(buffer);
		}
	#endif
    }
}



K_THREAD_DEFINE(sound_service, SOUND_STACK_SIZE,
                mic_data_handle, NULL, NULL, NULL,
                K_PRIO_PREEMPT(7), 0, 0);


extern void turn_on_off_led(uint8_t idx, bool onOff);
static bool mic_work_event_handler(const struct app_event_header *aeh)
{
	if (is_mic_work_event(aeh)) 
	{
		struct mic_work_event *event = cast_mic_work_event(aeh);
		if (event->type == MIC_STATUS_START) 
		{
			/** radio work imediately in case of loss audio packet */
			if(!radio_is_up)
			{
				inverse_esb_start();
				radio_is_up = true;
				LOG_INF("Radio start");
			}

            LOG_INF("Micphone start to work!");
			drv_mic_start();

			k_thread_resume(sound_service);
			
			turn_on_off_led(0, true);
		}
		else if(event->type == MIC_STATUS_STOP)
		{
            LOG_INF("Micphone stop to work!");
			drv_mic_stop();

			k_thread_suspend(sound_service);

            turn_on_off_led(0, false);
			/** radio work longer to  send the rest audio frame */
			if(radio_is_up)
			{
				radio_stop();
				radio_is_up = false;
				LOG_INF("Radio stop");
			}
		}

		return true;
	}
	else
	{
		return false;
	}
}


APP_EVENT_LISTENER(mic_work, mic_work_event_handler);
APP_EVENT_SUBSCRIBE(mic_work, mic_work_event);