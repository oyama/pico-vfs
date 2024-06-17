#include <stdio.h>
#include <pico/stdlib.h>

#define COLOR_GREEN(format)  ("\e[32m" format "\e[0m")

extern void test_blockdevice(void);
extern void test_filesystem(void);
extern void test_benchmark(void);

int main(void) {
    stdio_init_all();

    printf("Start all tests\n");

    test_blockdevice();
    test_filesystem();
    test_benchmark();

    printf(COLOR_GREEN("All tests are ok\n"));
}
