#include "blockdevice/sd.h"
#include "filesystem/fat.h"
#include "filesystem/vfs.h"
#include <hardware/clocks.h>
#include <assert.h>
#include <errno.h>
#include <pico/stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#define COLOR_GREEN(format)      ("\e[32m" format "\e[0m")
#define BUFFER_SIZE     (1024 * 64)
#define HUGE_FILE_SIZE  (1U * 1024U * 1024U * 1024U)

static void print_progress(const char *label, uint64_t current, uint64_t total) {
    int num_dots = (int)((double)current / total * (50 - strlen(label)));
    int num_spaces = (50 - strlen(label)) - num_dots;

    printf("\r%s ", label);
    for (int i = 0; i < num_dots; i++) {
        printf(".");
    }
    for (int i = 0; i < num_spaces; i++) {
        printf(" ");
    }
    printf(" %" PRIu64 "/%" PRIu64 " bytes", current, total);
}

bool fs_init(void) {
    blockdevice_t *sd = blockdevice_sd_create(spi0,
                                              PICO_DEFAULT_SPI_TX_PIN,
                                              PICO_DEFAULT_SPI_RX_PIN,
                                              PICO_DEFAULT_SPI_SCK_PIN,
                                              PICO_DEFAULT_SPI_CSN_PIN,
                                              24 * MHZ,
                                              false);
    filesystem_t *fat = filesystem_fat_create();

    printf("format / with FAT\n");
    int err = fs_format(fat, sd);
    if (err == -1) {
        printf("fs_format error: %s", strerror(errno));
        return false;
    }
    err = fs_mount("/", fat, sd);
    if (err == -1) {
        printf("fs_mount error: %s", strerror(errno));
        return false;
    }
    return true;
}

static uint32_t xor_rand(uint32_t *seed) {
    *seed ^= *seed << 13;
    *seed ^= *seed >> 17;
    *seed ^= *seed << 5;
    return *seed;
}

static uint32_t xor_rand_32bit(uint32_t *seed) {
    return xor_rand(seed);
}


static uint8_t buffer[BUFFER_SIZE] = {0};

static void huge_file_write(uint32_t seed) {
    const char *label = "Write";
    absolute_time_t start_at = get_absolute_time();
    char path[256] = {0};
    sprintf(path, "/huge.%lu", seed);
    int fd = open(path, O_WRONLY|O_CREAT);
    if (fd == -1) {
        printf("open error: %s\n", strerror(errno));
        return;
    }

    uint32_t counter = seed;
    xor_rand(&counter);
    uint64_t remind = HUGE_FILE_SIZE;
    while (remind > 0) {
        size_t chunk = remind % (uint64_t)sizeof(buffer) ? remind % sizeof(buffer) : sizeof(buffer);
        uint32_t *b = (uint32_t *)buffer;
        for (size_t j = 0; j < (chunk / sizeof(uint32_t)); j++) {
            b[j] = xor_rand_32bit(&counter);
        }

        ssize_t write_size = write(fd, buffer, chunk);
        if (write_size == -1) {
            printf("write: error: %s\n", strerror(errno));
            return;
        }
        remind = remind - write_size;
        print_progress(label, HUGE_FILE_SIZE - remind, HUGE_FILE_SIZE);
    }

    int err = close(fd);
    if (err == -1) {
        printf("close error: %s\n", strerror(errno));
        return;
    }

    double duration = (double)absolute_time_diff_us(start_at, get_absolute_time()) / 1000 / 1000;
    printf(" %.1f KB/s\n", (double)(HUGE_FILE_SIZE) / duration / 1024);
}

static void huge_file_read(uint32_t seed) {
    const char *label = "Read";
    absolute_time_t start_at = get_absolute_time();
    char path[256] = {0};
    sprintf(path, "/huge.%lu", seed);
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        printf("open error: %s\n", strerror(errno));
        return;
    }

    uint32_t counter = seed;
    xor_rand(&counter);
    uint64_t remind = HUGE_FILE_SIZE;
    while (remind > 0) {
        size_t chunk = remind % sizeof(buffer) ? remind % sizeof(buffer) : sizeof(buffer);
        ssize_t read_size = read(fd, buffer, chunk);
        if (read_size == -1) {
            printf("read error: %s\n", strerror(errno));
            return;
        }

       uint32_t *b = (uint32_t *)buffer;
        for (size_t j = 0; j < chunk / sizeof(uint32_t); j++) {
            volatile uint32_t v = xor_rand_32bit(&counter);
            if (b[j] != v) {
                printf("data mismatch\n");
                return;
            }
        }
        remind = remind - read_size;
        print_progress(label, HUGE_FILE_SIZE - remind, HUGE_FILE_SIZE);
    }

    int err = close(fd);
    if (err == -1) {
        printf("close error: %s\n", strerror(errno));
        return;
    }

    double duration = (double)absolute_time_diff_us(start_at, get_absolute_time()) / 1000 / 1000;
    printf(" %.1f KB/s\n", (double)(HUGE_FILE_SIZE) / duration / 1024);
}


int main(void) {
    stdio_init_all();
    fs_init();

    printf("10GB write/read test:\n");
    for (size_t i = 1; i <= 10; i++) {
        huge_file_write(i);
        huge_file_read(i);
    }

    printf(COLOR_GREEN("All tests ok\n"));

    while (1)
        tight_loop_contents();
}
