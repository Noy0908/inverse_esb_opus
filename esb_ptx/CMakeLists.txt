#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(NONE)

if (CONFIG_SOC_NRF5340_CPUAPP)
  message(FATAL_ERROR "This sample is not supported on the nRF53 application core")
endif()

# NORDIC SDK APP START
target_sources(app PRIVATE
  src/main.c
  src/sound_service.c
  src/drivers/drv_mic.c
  src/events/mic_work_event.c
  src/modules/app_bt_hid.c
  src/modules/app_timeslot.c
  src/modules/app_esb.c
  # ../common/circle_test/circle_test.c
  ../common/dvi_adpcm.c
)
# NORDIC SDK APP END
zephyr_library_include_directories(src/drivers/)
zephyr_library_include_directories(src/events/)
zephyr_library_include_directories(src/modules/)
zephyr_library_include_directories(../common)
# zephyr_library_include_directories(../common/circle_test)
