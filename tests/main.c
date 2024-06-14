#include <stdio.h>
#include <pico/stdlib.h>

#define COLOR_GREEN(format)  ("\e[32m" format "\e[0m")

extern void test_blockdevice(void);
extern void test_filesystem(void);
extern void test_vfs(void);
extern void test_standard(void);
extern void test_copy_between_different_filesystems(void);

int main(void) {
    stdio_init_all();

    printf("Start all tests\n");

    test_blockdevice();
    test_filesystem();
    test_vfs();
    test_standard();
    test_copy_between_different_filesystems();

    printf(COLOR_GREEN("All tests are ok\n"));
    while (1)
        tight_loop_contents();
}
