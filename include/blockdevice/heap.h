/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

/** \defgroup blockdevice_heap blockdevice_heap
 *  \ingroup blockdevice
 *  \brief Heap memory block device
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "blockdevice/blockdevice.h"

/*! \brief Create RAM heap memory block device
 * \ingroup blockdevice_heap
 *
 * Create a block device object that uses RAM heap memory.  The size of heap memory allocated to the block device is specified by size.
 *
 * \param size Size in bytes to be allocated to the block device.
 * \return Block device object. Returnes NULL in case of failure.
 * \retval NULL Failed to create block device object.
 */
blockdevice_t *blockdevice_heap_create(size_t size);

/*! \brief Release the heap memory device.
 * \ingroup blockdevice_heap
 *
 * \param device Block device object.
 */
void blockdevice_heap_free(blockdevice_t *device);

#ifdef __cplusplus
}
#endif
