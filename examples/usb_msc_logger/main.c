/* USB MSC Logger with two file systems
 *
 * Copyright 2024, Hiroyuki OYAMA. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <bsp/board.h>
#include <hardware/adc.h>
#include <hardware/gpio.h>
#include <hardware/structs/ioqspi.h>
#include <hardware/sync.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include <tusb.h>
#include "blockdevice/flash.h"
#include "filesystem/fat.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"

#define TEMPERATURE_UNITS 'C'
#define USBCTRL_REG    ((uint32_t *)(USBCTRL_REGS_BASE + 0x50))

bool filesystem_is_exported = false;
static bool enable_logging_task = false;
static blockdevice_t *flash1;
blockdevice_t *flash2;
static filesystem_t *littlefs;
static filesystem_t *fat;


static float read_onboard_temperature(const char unit) {
    const float conversionFactor = 3.3f / (1 << 12);

    float adc = (float)adc_read() * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;

    if (unit == 'C') {
        return tempC;
    } else if (unit == 'F') {
        return tempC * 9 / 5 + 32;
    }

    return -1.0f;
}

static bool __no_inline_not_in_flash_func(bootsel_button_get)(void) {
    const uint CS_PIN_INDEX = 1;
    uint32_t flags = save_and_disable_interrupts();
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    for (volatile int i = 0; i < 1000; ++i);
    bool button_state = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    restore_interrupts(flags);

    return button_state;
}

static bool filesystem_init(void) {
    flash1 = blockdevice_flash_create(1 * 1024 * 1024, 512 * 1024);
    flash2 = blockdevice_flash_create(1 * 1024 * 1024 + 512 * 1024, 0);

    littlefs = filesystem_littlefs_create(500, 16);
    fat = filesystem_fat_create();

    int err = fs_format(littlefs, flash1);
    if (err != 0) {
        printf("fs_format error=%d\n", err);
        return false;
    }
    err = fs_format(fat, flash2);
    if (err != 0) {
        printf("fs_format error=%d\n", err);
        return false;
    }

    err = fs_mount("/internal", littlefs, flash1);
    if (err != 0) {
        printf("fs_mount error=%d\n", err);
        return false;
    }
    return true;
}

static void export_filesystem(void) {
    printf("copy /internal to /export\n");

    int src = fs_open("/internal/TEMP.TXT", O_RDONLY);
    if (src < 0) {
        return;
    }

    int err = fs_mount("/export", fat, flash2);
    if (err != 0) {
        printf("fs_mount error=%d\n", err);
        return;
    }

    int dist = fs_open("/export/TEMP.TXT", O_WRONLY|O_CREAT);
    if (dist < 0) {
        printf("fs_open error=%d\n", dist);
        return;
    }
    char buffer[1024*64];
    while (true) {
        ssize_t read_size = fs_read(src, buffer, sizeof(buffer));
        if (read_size == 0)
            break;
        ssize_t write_size = fs_write(dist, buffer, read_size);
        if (write_size < 0) {
            printf("fs_write error=%d\n", write_size);
            break;
        }
    }
    fs_close(dist);
    fs_close(src);

    err = fs_unmount("/export");
    if (err != 0) {
        printf("fs_unmount error=%d\n", err);
        return;
    }
}

static void logging_task(void) {

    if (!enable_logging_task)
        return;

    static uint64_t last_measure = 0;
    uint64_t now = time_us_64();
    uint64_t duration = now - last_measure;
    if (duration < 1000000)
        return;

    float temperature = read_onboard_temperature(TEMPERATURE_UNITS);
    printf("temperature=%.1f\n", temperature);

    int fd = fs_open("/internal/TEMP.TXT", O_WRONLY|O_APPEND|O_CREAT);
    if (fd < 0) {
        printf("fs_open('/internal/TEMP.TXT') error=%d\n", fd);
    }
    char buffer[512] = {0};
    int length = sprintf(buffer, "temperature,%.1f\n", temperature);
    ssize_t write_size = fs_write(fd, buffer, length);
    if (write_size < 0) {
        printf("fs_file_write error=%d\n", write_size);
    }
    int err = fs_close(fd);
    if (err != 0) {
        printf("fs_close() error=%d\n", fd);
    }
    last_measure = now;
}

static void filesystem_management_task(void) {
    static bool last_status = false;

     bool button = bootsel_button_get();
     if (last_status != button) {
         if (button) {
             printf("USB disconnected\n");
             filesystem_is_exported = false;
             enable_logging_task = true;
         } else {
             enable_logging_task = false;
             printf("USB connected\n");
             export_filesystem();
             filesystem_is_exported = true;
         }
         last_status = button;
     }
}

int main(void) {
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
    board_init();
    tud_init(BOARD_TUD_RHPORT);
    stdio_init_all();
    if (!filesystem_init()) {
        return -1;
    }

    printf("Start USB MSC Logger with two file systems\n");
    while (true) {
         filesystem_management_task();
         if (enable_logging_task)
             logging_task();
         tud_task();
    }
}

