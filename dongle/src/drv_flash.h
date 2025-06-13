#ifndef __DRV_FLASH_H__
#define __DRV_FLASH_H__

// #include <zephyr/kernel.h>
// #include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>

#define TEST_PARTITION	storage_partition

#define TEST_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(TEST_PARTITION)
#define TEST_PARTITION_SIZE		FIXED_PARTITION_SIZE(TEST_PARTITION)
#define TEST_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(TEST_PARTITION)



void soc_flash_init(void);

int soc_flash_write(off_t offset, const void * data, size_t len);


#endif