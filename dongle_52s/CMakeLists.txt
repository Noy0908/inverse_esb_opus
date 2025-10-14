#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(NONE)

set(OPUS_LIBRARY_ROOT ${CMAKE_CURRENT_LIST_DIR}/lib CACHE PATH "Opus library root directory")
set(ENV{OPUS_LIBRARY_ROOT} $OPUS_LIBRARY_ROOT)

zephyr_link_libraries(-mfloat-abi=hard)
zephyr_link_libraries(-mfpu=fpv4-sp-d16 -ffunction-sections -fdata-sections -fno-strict-aliasing -mthumb -mabi=aapcs)

FILE(GLOB app_sources src/*.c)
# NORDIC SDK APP START
target_sources(app PRIVATE 
    ${app_sources}
    # lib/adpcm_lib/dvi_adpcm.c
    lib/inv_esb_lib/radio.c)
# NORDIC SDK APP END

zephyr_library_include_directories(lib/inv_esb_lib)
# zephyr_library_include_directories(lib/adpcm_lib)
zephyr_include_directories(lib/opus/opus_include/opus-1.2.1)
zephyr_include_directories(lib/opus/opus_configuration)
target_link_libraries(app PRIVATE ${OPUS_LIBRARY_ROOT}/opus/opus_library/opus_lib.a)