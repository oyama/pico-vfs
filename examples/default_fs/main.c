#include <errno.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include "filesystem/vfs.h"


int main(void) {
    stdio_init_all();
    fs_init();

    // Write file by file descriptor
    int fd = open("/HELLO.TXT", O_WRONLY|O_CREAT);
    if (fd < 0)
        printf("open error=%d\n", fd);
    int err = write(fd, "Hello World!\n", 12);
    if (err < 0)
        printf("write error=%d\n", err);
    err = close(fd);
    if (err != 0)
        printf("close error=%d\n", err);

    // Read file by stream
    FILE *fp = fopen("/HELLO.TXT", "r");
    if (fp == NULL)
        printf("fopen error=%d\n", errno);

    char buffer[512];
    size_t read_size = fread(buffer, sizeof(char), sizeof(buffer), fp);
    if (read_size == 0)
        printf("fread error=%d\n", errno);
    err = fclose(fp);
    if (err != 0)
        printf("fclose error=%d\n", err);

    printf("/HELLO.TXT: %s\n", buffer);
}
