#include <assert.h>
#include <pico/flash.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesystem/vfs.h"

#define COLOR_GREEN(format)  ("\e[32m" format "\e[0m")

#define TEST_FILE_SIZE      (512 * 1024)
#define FLASH_START_AT      (0.5 * 1024 * 1024)
#define FLASH_LENGTH_ALL    0

static uint8_t core1_buffer[1024*16];
static uint8_t core0_buffer[1024*16];

static size_t test_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int n = vprintf(format, args);
    va_end(args);

    printf(" ");
    for (size_t i = 0; i < 50 - (size_t)n; i++)
        printf(".");
    return (size_t)n;
}

static void write_read(const char *path, size_t size, uint8_t *buffer, size_t buf_size) {
    int fd = open(path, O_WRONLY|O_CREAT);
    size_t remind = size;
    unsigned seed = 0;
    while (remind > 0) {
        size_t chunk = remind % buf_size ? remind % buf_size : buf_size;
        for (size_t i = 0; i < (size_t)chunk; i++) {
            buffer[i] = rand_r(&seed) & 0xFF;
        }
        ssize_t write_size = write(fd, buffer, chunk);
        assert(write_size != -1);
        remind = remind - write_size;
    }

    int err = close(fd);
    assert(err == 0);

    fd = open(path, O_RDONLY);
    assert(fd != 0);
    seed = 0;
    remind = size;
    while (remind > 0) {
        size_t chunk = remind % buf_size ? remind % buf_size : buf_size;
        ssize_t read_size = read(fd, buffer, chunk);
        assert(read_size != -1);

        for (size_t i = 0; i < (size_t)read_size; i++) {
            volatile uint8_t r = rand_r(&seed) & 0xFF;
            assert(buffer[i] == r);
        }
        remind = remind - read_size;
    }
    err = close(fd);
    assert(err == 0);
}

static void test_sequential_write_read(void) {
    test_printf("/flash/core0 then /sd/core0");
    absolute_time_t start_at = get_absolute_time();

    write_read("/flash/core0", TEST_FILE_SIZE, core0_buffer, sizeof(core0_buffer));
    write_read("/sd/core0", TEST_FILE_SIZE * 5, core0_buffer, sizeof(core0_buffer));

    double duration = (double)absolute_time_diff_us(start_at, get_absolute_time()) / 1000 / 1000;
    printf(COLOR_GREEN("ok, %.1f seconds\n"), duration);
}

static void sd_card_write_read_task(void) {
    flash_safe_execute_core_init();
    write_read("/sd/core1", TEST_FILE_SIZE * 5, core1_buffer, sizeof(core1_buffer));

    multicore_fifo_push_blocking(1); // finish
    while (1)
        tight_loop_contents();
}

static void test_parallel_write_read(void) {
    test_printf("/flash/core0 with /sd/core1");

    /* NOTE: When running with pico-sdk 1.5.1 and openocd, core1 needs to reset and sleep.
     *       https://forums.raspberrypi.com/viewtopic.php?t=349795
     */
    multicore_reset_core1();
    sleep_ms(100);
    absolute_time_t start_at = get_absolute_time();
    multicore_launch_core1(sd_card_write_read_task);

    write_read("/flash/core0", TEST_FILE_SIZE, core0_buffer, sizeof(core0_buffer));
    int result = multicore_fifo_pop_blocking();
    assert(result == 1);

    double duration = (double)absolute_time_diff_us(start_at, get_absolute_time()) / 1000 / 1000;
    printf(COLOR_GREEN("ok, %.1f seconds\n"), duration);
}


int main(void) {
    stdio_init_all();
    printf("Start write and read tests:\n");
    if (!fs_init()) {
        printf("SD card device not found, skip\n");
    } else {
        test_sequential_write_read();
        remove("/flash/core0");
        remove("/sd/core0");
        test_parallel_write_read();
    }

    printf(COLOR_GREEN("All tests are ok\n"));
    while (1)
        tight_loop_contents();
}
