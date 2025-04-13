/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <string.h>
#include <ctype.h>
#include <bsp/board.h>
#include <tusb.h>
#include "blockdevice/flash.h"

#define ANSI_CYAN     "\e[36m"
#define ANSI_MAGENTA  "\e[35m"
#define ANSI_CLEAR    "\e[0m"

static bool ejected = false;
static bool usb_connected = false;

extern bool filesystem_is_exported;
extern blockdevice_t *flash2;


bool usb_connection_status(void) {
    return usb_connected;
}

void tud_mount_cb(void) {
    printf("\e[45mmount\e[0m\n");
    usb_connected = true;
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;

    printf("\e[45msuspend\e[0m\n");
    usb_connected = false;
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    (void) lun;

    const char vid[] = "TinyUSB";
    const char pid[] = "Mass Storage";
    const char rev[] = "1.0";

    memcpy(vendor_id  , vid, strlen(vid));
    memcpy(product_id , pid, strlen(pid));
    memcpy(product_rev, rev, strlen(rev));
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void) lun;
    if (!filesystem_is_exported) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }

    if (ejected) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }

    return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
    (void) lun;
    *block_count = flash2->size(flash2) /  flash2->erase_size;
    *block_size  = flash2->erase_size;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
    (void) lun;
    (void) power_condition;
    if ( load_eject ) {
        if (start) {
            // load disk storage
        } else {
            // unload disk storage
            ejected = true;
        }
    }
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    (void)lun;
    (void)offset;

    printf(ANSI_CYAN "USB read#%lu size=%lu\n" ANSI_CLEAR, lba, bufsize);
    int err = flash2->read(flash2, buffer, lba * flash2->erase_size, bufsize);
    if (err != 0) {
        printf("read error=%d\n", err);
    }
    return (int32_t) bufsize;
}

bool tud_msc_is_writable_cb (uint8_t lun) {
    (void) lun;
    return true;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    (void)lun;
    (void)offset;

    printf(ANSI_MAGENTA "USB write#%lu bufsize=%lu\n" ANSI_CLEAR, lba, bufsize);

    uint32_t block_size = flash2->erase_size;
    int err = flash2->erase(flash2, lba * block_size, bufsize);
    if (err != BD_ERROR_OK) {
        printf("erase error=%d\n", err);
    }
    err = flash2->program(flash2, buffer, lba * block_size, bufsize);
    if (err != BD_ERROR_OK) {
        printf("program error=%d\n", err);
    }

    return (int32_t)bufsize;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize) {
    (void)lun;

    void const *response = NULL;
    int32_t resplen = 0;
    bool in_xfer = true;

    switch (scsi_cmd[0]) {
    default:
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
        resplen = -1;
        break;
    }

    if (resplen > bufsize)
        resplen = bufsize;

    if (response && (resplen > 0)) {
        if(in_xfer) {
            memcpy(buffer, response, (size_t)resplen);
        } else {
            ; // SCSI output
        }
    }

    return (int32_t)resplen;
}
