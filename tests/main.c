#include <stdio.h>
#include <pico/stdlib.h>

#define COLOR_GREEN(format)  ("\e[32m" format "\e[0m")

extern void test_blockdevice_flash_filesystem_fat(void);
extern void test_blockdevice_sd_filesystem_fat(void);
extern void test_blockdevice_flash_filesystem_littlefs(void);

int main(void) {
    stdio_init_all();

    printf("Start all tests\n");

    test_blockdevice_flash_filesystem_fat();
    test_blockdevice_sd_filesystem_fat();
    test_blockdevice_flash_filesystem_littlefs();

    printf(COLOR_GREEN("All tests are ok\n"));
}
