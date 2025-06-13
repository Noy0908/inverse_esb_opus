
#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <stdio.h>

#include "drv_flash.h"

LOG_MODULE_REGISTER(soc_flash, CONFIG_ESB_PRX_APP_LOG_LEVEL);

const struct device *flash_dev = TEST_PARTITION_DEVICE;


void soc_flash_init(void)
{
    if (!device_is_ready(flash_dev)) {
		printf("Flash device not ready\n");
		return ;
	}

	printf("\nTest 1: Flash erase page at 0x%x\n", TEST_PARTITION_OFFSET);
	printf("\nTest 1: Flash partition size = %d\n", TEST_PARTITION_SIZE);

	if (flash_erase(flash_dev, TEST_PARTITION_OFFSET, TEST_PARTITION_SIZE) != 0) {
		LOG_ERR("   Flash erase failed!\n");
	} 
    else 
    {
		LOG_DBG("   Flash erase succeeded!\n");
	}
}



int soc_flash_write(off_t offset, const void * data, size_t len)
{
    if (flash_write(flash_dev, offset, data, len) != 0) 
    {
        LOG_ERR("   Flash write failed!\n");
        return -1;
    }

    return 0;
}
	



