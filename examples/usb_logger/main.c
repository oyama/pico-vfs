/* USB MSC Logger with two file systems
 *
 * Copyright 2024, Hiroyuki OYAMA
 *
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
static bool enable_logging_task = true;
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
    if (err == -1) {
        printf("fs_format error: %s\n", strerror(errno));
        return false;
    }
    err = fs_format(fat, flash2);
    if (err == -1) {
        printf("fs_format error: %s\n", strerror(errno));
        return false;
    }

    err = fs_mount("/internal", littlefs, flash1);
    if (err == -1) {
        printf("fs_mount error: %s\n", strerror(errno));
        return false;
    }
    return true;
}

static void export_filesystem(void) {
    printf("copy /internal to /export\n");

    int src = open("/internal/TEMP.TXT", O_RDONLY);
    if (src == -1) {
        return;
    }

    int err = fs_mount("/export", fat, flash2);
    if (err == -1) {
        printf("fs_mount error: %s\n", strerror(errno));
        return;
    }

    int dist = open("/export/TEMP.TXT", O_WRONLY|O_CREAT);
    if (dist == -1) {
        printf("open error: %s\n", strerror(errno));
        return;
    }
    char buffer[1024*64];
    while (true) {
        ssize_t read_size = read(src, buffer, sizeof(buffer));
        if (read_size == 0)
            break;
        ssize_t write_size = write(dist, buffer, read_size);
        if (write_size == -1) {
            printf("write error: %s\n", strerror(errno));
            break;
        }
    }
    close(dist);
    close(src);

    err = fs_unmount("/export");
    if (err == -1) {
        printf("fs_unmount error: %s\n", strerror(errno));
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

    int fd = open("/internal/TEMP.TXT", O_WRONLY|O_APPEND|O_CREAT);
    if (fd == -1) {
        printf("open('/internal/TEMP.TXT') error: %s\n", strerror(errno));
    }
    char buffer[512] = {0};
    int length = sprintf(buffer, "temperature,%.1f\n", temperature);
    ssize_t write_size = write(fd, buffer, length);
    if (write_size == -1) {
        printf("write error: %s\n", strerror(errno));
    }
    int err = close(fd);
    if (err == -1) {
        printf("close error: %s\n", strerror(errno));
    }
    last_measure = now;
}

static bool is_usb_connected(void) {
    return tud_ready();
}

static void filesystem_management_task(void) {
    static bool last_status = false;

    bool usb = is_usb_connected();
    // Override USB connection status as long as the BOOTSEL button is pressed.
    if (bootsel_button_get())
        usb = false;

    if (last_status != usb) {
        if (usb) {
             enable_logging_task = false;
             printf("USB connected\n");
             export_filesystem();
             filesystem_is_exported = true;
         } else {
             printf("USB disconnected\n");
             filesystem_is_exported = false;
             enable_logging_task = true;
         }
         last_status = usb;
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
