/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/audio/dmic.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "drv_mic.h"
#include "dvi_adpcm.h"

LOG_MODULE_REGISTER(dmic_driver, CONFIG_ESB_BT_LOG_LEVEL);


/* Driver will allocate blocks from this slab to receive audio data into them.
 * Application, after getting a given block from the driver and processing its
 * data, needs to free that block.
 */

K_MEM_SLAB_DEFINE(mem_slab, MAX_BLOCK_SIZE, BLOCK_COUNT, 4);


static const struct gpio_dt_spec mic_power = GPIO_DT_SPEC_GET(DT_NODELABEL(mic_pwr), enable_gpios);

static const struct device * dmic_dev = DEVICE_DT_GET(DT_NODELABEL(dmic_dev));


static void mic_power_on(void)
{
    uint32_t ret;

	ret = gpio_pin_set(mic_power.port, mic_power.pin, 1);
	if (ret < 0) {
		LOG_ERR("Failed to power on the micphone: %d!", ret);
		return ;
	}
}


static void mic_power_off(void)
{
    uint32_t ret;

    ret = gpio_pin_set(mic_power.port, mic_power.pin, 0);
	if (ret < 0) {
		LOG_ERR("Failed to power off the micphone: %d!", ret);
		return ;
	}
}

// static dvi_adpcm_state_t    adpcm_state;
int drv_audio_init(void)
{
	int ret;
    
	if (!device_is_ready(dmic_dev)) {
		LOG_ERR("%s is not ready", dmic_dev->name);
		return -1;
	}

	if (!gpio_is_ready_dt(&mic_power)) {
		LOG_ERR("The micphone goio pin is not enable!!!!");
		return -1;
	}

	struct pcm_stream_cfg stream = {
		.pcm_width = SAMPLE_BIT_WIDTH,
		.mem_slab  = &mem_slab,
	};
	struct dmic_cfg cfg = {
		.io = {
			/* These fields can be used to limit the PDM clock
			 * configurations that the driver is allowed to use
			 * to those supported by the microphone.
			 */
			.min_pdm_clk_freq = 1000000,
			.max_pdm_clk_freq = 3500000,
			.min_pdm_clk_dc   = 40,
			.max_pdm_clk_dc   = 60,
		},
		.streams = &stream,
		.channel = {
			.req_num_streams = 1,
		},
	};

	cfg.channel.req_num_chan = 1;
	cfg.channel.req_chan_map_lo =
		dmic_build_channel_map(0, 0, PDM_CHAN_LEFT);
	cfg.streams[0].pcm_rate = MAX_SAMPLE_RATE;
	cfg.streams[0].block_size =
		BLOCK_SIZE(cfg.streams[0].pcm_rate, cfg.channel.req_num_chan);
	
	ret = dmic_configure(dmic_dev, &cfg);
	if (ret < 0) {
		LOG_ERR("Failed to configure the driver: %d", ret);
		return ret;
	}
	LOG_INF("PCM output rate: %u, channels: %u",
		cfg.streams[0].pcm_rate, cfg.channel.req_num_chan);

	// dvi_adpcm_init_state(&adpcm_state);			//for test

	return 0;
}



int drv_mic_start(void)
{
    int ret;

    LOG_INF("m_audio: Enabled\r\n");

    // if(m_audio_enabled == true)
    // {
    //     return NRF_SUCCESS;
    // }

    mic_power_on();

    // status = drv_audio_enable();
    // if (status == NRF_SUCCESS)
    // {
    //     m_audio_enabled = true;
    // }

	ret = dmic_trigger(dmic_dev, DMIC_TRIGGER_START);
	if (ret < 0) {
		LOG_ERR("START m_audio failed: %d", ret);
	}

    return ret;
}


int drv_mic_stop(void)
{
    int ret;

    LOG_INF("m_audio: Disabled\r\n");

    // if(m_audio_enabled == false)
    // {
    //     return NRF_SUCCESS;
    // }

    // status = drv_audio_disable();
    // if (status == NRF_SUCCESS)
    // {
    //     m_audio_enabled = false;
    // }

	ret = dmic_trigger(dmic_dev, DMIC_TRIGGER_STOP);
	if (ret < 0) {
		LOG_ERR("STOP m_audio failed: %d", ret);
	}

    mic_power_off();

    return ret;
}

uint32_t read_audio_data(void **buffer, int32_t timeout)
{
	uint32_t size = 0;
	int ret = 0;

	ret = dmic_read(dmic_dev, 0, buffer, &size, timeout);
	if (ret < 0) {
		LOG_ERR("PDM read failed: %d", ret);
		return ret;
	}

	// LOG_INF("Got pcm buffer %p of %u bytes", *buffer, size);
	// LOG_HEXDUMP_INF(*buffer,size,"PCM data");

	return size;
}


void free_audio_memory(void *buffer)
{
	k_mem_slab_free(&mem_slab, buffer);
}

/** just for test*/

int test_pdm_transfer(size_t block_count)
{
	int ret = 0;

	for (int i = 0; i < block_count; ++i) {
		void *buffer;
		uint32_t size;
		// int frame_size;
	    // uint8_t frame_buf[MAX_BLOCK_SIZE] = {0};

		ret = dmic_read(dmic_dev, 0, &buffer, &size, 1000);
		if (ret < 0) {
			LOG_ERR("%d - read failed: %d", i, ret);
			return ret;
		}

		LOG_INF("%d - got buffer %p of %u bytes", i, buffer, size);
		LOG_HEXDUMP_INF(buffer,200,"PCM data");

		// dvi_adpcm_encode(buffer, size, frame_buf, &frame_size,&adpcm_state, true);
		// LOG_INF("ADPCM buffer %u bytes", frame_size);
		// LOG_HEXDUMP_INF(frame_buf,frame_size,"ADPCM data");

		k_mem_slab_free(&mem_slab, buffer);
	}

	return ret;
}

