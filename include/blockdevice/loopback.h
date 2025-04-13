/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

/** \defgroup blockdevice_loopback blockdevice_loopback
 *  \ingroup blockdevice
 *  \brief Loopback disk image file block device
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "blockdevice/blockdevice.h"

/*! \brief Create loopback block device
 * \ingroup blockdevice_loopback
 *
 * Create a loopback device object that uses a disk image file. Specify the file path allocated to the block device, as well as the maximum size capacity and block size block_size.
 *
 * \param path Disk image file path.
 * \param capacity Maximum device size bytes.
 * \param block_size Block size byte.
 * \return Block device object. Returnes NULL in case of failure.
 * \retval NULL Failed to create block device object.
 */
blockdevice_t *blockdevice_loopback_create(const char *path, size_t capacity, size_t block_size);

/*! \brief Release the loopback device.
 * \ingroup blockdevice_loopback
 *
 * \param device Block device object.
 */
void blockdevice_loopback_free(blockdevice_t *device);

#ifdef __cplusplus
}
#endif
