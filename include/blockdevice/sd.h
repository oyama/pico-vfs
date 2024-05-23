/*
 * Copyright 2024, Hiroyuki OYAMA. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <hardware/spi.h>
#include "blockdevice/blockdevice.h"


#define CONF_SD_INIT_FREQUENCY    (10 * 1000 * 1000)
#define CONF_SD_TRX_FREQUENCY     (24 * MHZ)

blockdevice_t *blockdevice_sd_create(spi_inst_t *spi_inst,
                                     uint8_t mosi,
                                     uint8_t miso,
                                     uint8_t sckl,
                                     uint8_t cs,
                                     uint32_t hz,
                                     bool enable_crc);
void blockdevice_sd_free(blockdevice_t *device);

#ifdef __cplusplus
}
#endif
