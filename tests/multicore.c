/*
 * NOTE: When operating Flash memory from core1, it must be run from RAM.
 */
#include <assert.h>
#include <hardware/clocks.h>
#include <hardware/flash.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "blockdevice/flash.h"
#include "blockdevice/heap.h"
#include "blockdevice/sd.h"
#include "filesystem/littlefs.h"
#include "filesystem/fat.h"
#include "filesystem/vfs.h"

#define COLOR_GREEN(format)  ("\e[32m" format "\e[0m")

#define TEST_FILE_SIZE      (320 * 1024)
#define FLASH_START_AT      (0.5 * 1024 * 1024)
#define FLASH_LENGTH_ALL    0


struct combination_map {
    blockdevice_t *device;
    filesystem_t *filesystem;
    const char *label;
};

#if defined(WITHOUT_BLOCKDEVICE_SD)
#define NUM_COMBINATION    2
#else
#define NUM_COMBINATION    4
#endif


static struct combination_map combination[NUM_COMBINATION] = {0};

static void init_filesystem_combination(void) {
    blockdevice_t *flash = blockdevice_flash_create(PICO_FLASH_SIZE_BYTES - PICO_FS_DEFAULT_SIZE,
                                                    FLASH_LENGTH_ALL);
#if !defined(WITHOUT_BLOCKDEVICE_SD)
    blockdevice_t *sd = blockdevice_sd_create(spi0,
                                              PICO_DEFAULT_SPI_TX_PIN,
                                              PICO_DEFAULT_SPI_RX_PIN,
                                              PICO_DEFAULT_SPI_SCK_PIN,
                                              PICO_DEFAULT_SPI_CSN_PIN,
                                              24 * MHZ,
                                              true);
#endif
    filesystem_t *fat = filesystem_fat_create();
    filesystem_t *littlefs = filesystem_littlefs_create(500, 16);

    combination[0] = (struct combination_map){
        .device = flash, .filesystem = littlefs, .label = "littlefs on Flash"
    };
    combination[1] = (struct combination_map){
        .device = flash, .filesystem = fat, .label = "FAT on Flash"
    };
#if !defined(WITHOUT_BLOCKDEVICE_SD)
    combination[2] = (struct combination_map){
        .device = sd, .filesystem = littlefs, .label = "littlefs on SD card"
    };
    combination[3] = (struct combination_map){
        .device = sd, .filesystem = fat, .label = "FAT on SD card"
    };
#endif
}

static void cleanup_combination(void) {
    filesystem_littlefs_free(combination[0].filesystem);
    filesystem_fat_free(combination[1].filesystem);
    blockdevice_flash_free(combination[0].device);
#if !defined(WITHOUT_BLOCKDEVICE_SD)
    blockdevice_sd_free(combination[2].device);
#endif
}

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

static void print_progress(const char *label, size_t current, size_t total) {
    int num_dots = (int)((double)current / total * (50 - strlen(label)));
    int num_spaces = (50 - strlen(label)) - num_dots;

    printf("\r%s ", label);
    for (int i = 0; i < num_dots; i++) {
        printf(".");
    }
    for (int i = 0; i < num_spaces; i++) {
        printf(" ");
    }
    printf(" %zu/%zu bytes", current, total);
    fflush(stdout);
}

static void __not_in_flash_func(test_write_read_two_files_core1)(void) {
    int fd = open("/core1", O_WRONLY|O_CREAT);

    uint8_t buffer[512] = {0};
    size_t remind = TEST_FILE_SIZE;
    unsigned seed = 1;
    while (remind > 0) {
        size_t chunk = remind % sizeof(buffer) ? remind % sizeof(buffer) : sizeof(buffer);
        for (size_t i = 0; i < (size_t)chunk; i++) {
            buffer[i] = rand_r(&seed) & 0xFF;
        }
        ssize_t write_size = write(fd, buffer, chunk);
        assert(write_size != -1);
        remind = remind - write_size;
    }
    int err = close(fd);
    assert(err == 0);

    fd = open("/core1", O_RDONLY);
    assert(fd != 0);

    seed = 1;
    remind = TEST_FILE_SIZE;
    while (remind > 0) {
        size_t chunk = remind % sizeof(buffer) ? remind % sizeof(buffer) : sizeof(buffer);
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

    multicore_fifo_push_blocking(1); // finish
    while (1)
        tight_loop_contents();

}

static void test_write_read_two_files(void) {
    const char *label = "Write then read";

    /* NOTE: When running with pico-sdk 1.5.1 and openocd, core1 needs to reset and sleep.
     *       https://forums.raspberrypi.com/viewtopic.php?t=349795
     */
    multicore_reset_core1();
    sleep_ms(100);
    multicore_launch_core1(test_write_read_two_files_core1);

    int fd = open("/core0", O_WRONLY|O_CREAT);
    uint8_t buffer[512] = {0};
    size_t remind = TEST_FILE_SIZE;
    unsigned seed = 0;
    while (remind > 0) {
        size_t chunk = remind % sizeof(buffer) ? remind % sizeof(buffer) : sizeof(buffer);
        for (size_t i = 0; i < (size_t)chunk; i++) {
            buffer[i] = rand_r(&seed) & 0xFF;
        }
        ssize_t write_size = write(fd, buffer, chunk);
        assert(write_size != -1);
        remind = remind - write_size;

        print_progress(label, TEST_FILE_SIZE - remind, TEST_FILE_SIZE * 2);
    }

    int err = close(fd);
    assert(err == 0);

    fd = open("/core0", O_RDONLY);
    assert(fd != 0);

    seed = 0;
    remind = TEST_FILE_SIZE;
    while (remind > 0) {
        size_t chunk = remind % sizeof(buffer) ? remind % sizeof(buffer) : sizeof(buffer);
        ssize_t read_size = read(fd, buffer, chunk);
        assert(read_size != -1);

        for (size_t i = 0; i < (size_t)read_size; i++) {
            volatile uint8_t r = rand_r(&seed) & 0xFF;
            assert(buffer[i] == r);
        }
        remind = remind - read_size;
        print_progress(label, TEST_FILE_SIZE * 2 - remind, TEST_FILE_SIZE * 2);
    }
    err = close(fd);
    assert(err == 0);

    uint32_t result = multicore_fifo_pop_blocking();
    assert(result == 1);

    printf(COLOR_GREEN(" ok\n"));
}


static void __not_in_flash_func(test_write_while_read_two_files_core1)(void) {
    int fd = open("/core1", O_WRONLY|O_CREAT);

    uint8_t buffer[512] = {0};
    size_t remind = TEST_FILE_SIZE;
    unsigned seed = 1;
    while (remind > 0) {
        size_t chunk = remind % sizeof(buffer) ? remind % sizeof(buffer) : sizeof(buffer);
        for (size_t i = 0; i < (size_t)chunk; i++) {
            buffer[i] = rand_r(&seed) & 0xFF;
        }
        ssize_t write_size = write(fd, buffer, chunk);
        assert(write_size != -1);
        remind = remind - write_size;
    }
    int err = close(fd);
    assert(err == 0);
    multicore_fifo_push_blocking(1); // write finish

    fd = open("/core1", O_RDONLY);
    assert(fd != 0);
    seed = 1;
    remind = TEST_FILE_SIZE;
    while (remind > 0) {
        size_t chunk = remind % sizeof(buffer) ? remind % sizeof(buffer) : sizeof(buffer);
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

    multicore_fifo_push_blocking(1); // read finish
    while (1)
        tight_loop_contents();
}


static void test_write_while_read_two_files(void) {
    const char *label = "Write while read";

    /* NOTE: When running with pico-sdk 1.5.1 and openocd, core1 needs to reset and sleep.
     *       https://forums.raspberrypi.com/viewtopic.php?t=349795
     */
    multicore_reset_core1();
    sleep_ms(100);
    multicore_launch_core1(test_write_while_read_two_files_core1);

    int fd = open("/core0", O_RDONLY);
    assert(fd != 0);
    uint8_t buffer[512] = {0};

    unsigned seed = 0;
    size_t remind = TEST_FILE_SIZE;
    while (remind > 0) {
        size_t chunk = remind % sizeof(buffer) ? remind % sizeof(buffer) : sizeof(buffer);
        ssize_t read_size = read(fd, buffer, chunk);
        assert(read_size != -1);

        for (size_t i = 0; i < (size_t)read_size; i++) {
            volatile uint8_t r = rand_r(&seed) & 0xFF;
            assert(buffer[i] == r);
        }
        remind = remind - read_size;
        print_progress(label, TEST_FILE_SIZE - remind, TEST_FILE_SIZE * 2);
    }
    int err = close(fd);
    assert(err == 0);

    // wait core1
    uint32_t result = multicore_fifo_pop_blocking();
    assert(result == 1);

    fd = open("/core0", O_WRONLY|O_CREAT);
    remind = TEST_FILE_SIZE;
    seed = 0;
    while (remind > 0) {
        size_t chunk = remind % sizeof(buffer) ? remind % sizeof(buffer) : sizeof(buffer);
        for (size_t i = 0; i < (size_t)chunk; i++) {
            buffer[i] = rand_r(&seed) & 0xFF;
        }
        ssize_t write_size = write(fd, buffer, chunk);
        assert(write_size != -1);
        remind = remind - write_size;
        print_progress(label, TEST_FILE_SIZE * 2 - remind, TEST_FILE_SIZE * 2);
    }
    err = close(fd);
    assert(err == 0);

    // wait core1
    result = multicore_fifo_pop_blocking();
    assert(result == 1);

    printf(COLOR_GREEN(" ok\n"));
}


static void setup(filesystem_t *fs, blockdevice_t *device) {
    int err = fs_format(fs, device);
    assert(err == 0);
    err = fs_mount("/", fs, device);
    assert(err == 0);
}

static void cleanup() {
    int err = fs_unmount("/");
    assert(err == 0);
}

int main(void) {
    stdio_init_all();
    printf("Start multicore tests\n");

    init_filesystem_combination();
    for (size_t i = 0; i < NUM_COMBINATION; i++) {
        struct combination_map *setting = &combination[i];
        printf("%s:\n", setting->label);

        setup(setting->filesystem, setting->device);

        test_write_read_two_files();
        test_write_while_read_two_files();

        cleanup();
    }
    cleanup_combination();

    printf(COLOR_GREEN("All tests are ok\n"));
    while (1)
        tight_loop_contents();
}
