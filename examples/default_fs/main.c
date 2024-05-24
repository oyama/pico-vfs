#include <pico/stdlib.h>
#include <stdio.h>
#include "filesystem/vfs.h"


int main(void) {
    stdio_init_all();
    fs_init();

    int fd = fs_open("/HELLO.TXT", O_WRONLY|O_CREAT);
    if (fd < 0)
        printf("fs_open error error=%d\n", fd);
    int err = fs_write(fd, "Hello World!\n", 12);
    if (err < 0)
        printf("fs_write error=%d\n", err);
    err = fs_close(fd);
    if (err != 0)
        printf("fs_close error=%d\n", err);
}
