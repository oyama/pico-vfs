/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

/** \defgroup blockdevice_flash blockdevice_flash
 *  \ingroup blockdevice
 *  \brief Raspberry Pi Pico on-board flash memory block device
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "blockdevice/blockdevice.h"

/*! \brief Create Raspberry Pi Pico On-board Flash block device
 * \ingroup blockdevice_flash
 *
 * Create a block device object that uses the Raspberry Pi Pico onboard flash memory. The start position of the flash memory to be allocated to the block device is specified by start and the length by length. start and length must be aligned to a flash sector of 4096 bytes.
 *
 * \param start Specifies the starting position of the flash memory to be allocated to the block device in bytes.
 * \param length Size in bytes to be allocated to the block device. If zero is specified, all remaining space is used.
 * \return Block device object. Returnes NULL in case of failure.
 * \retval NULL Failed to create block device object.
 */
blockdevice_t *blockdevice_flash_create(uint32_t start, size_t length);

/*! \brief Release the flash memory device.
 * \ingroup blockdevice_flash
 *
 * \param device Block device object.
 */
void blockdevice_flash_free(blockdevice_t *device);

#ifdef __cplusplus
}
#endif
