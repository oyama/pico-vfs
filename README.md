# Thin virtual file system for Raspberry Pi Pico

Designed specifically for the Raspberry Pi Pico, `pico-vfs` is a virtual file system that allows users to efficiently and easily utilize different storage media such as onboard flash memory and SD cards using familiar POSIX and C standard file APIs. It streamlines file operations in embedded projects, providing a flexible and scalable file system.

## Key Features

- **POSIX and C Standard File API Support**: Users can perform basic POSIX file operations such as `open`, `read`, `write`, along with higher-level file functions like `fopen`, `fread`, `fwrite` enabling file operations in embedded projects with familiar APIs.
- **Virtual File System**: Multiple different file systems can be mounted into a single namespace, allowing programs to operate without needing to consider differences in storage media and file systems.
- **Pre-configured File System**: To reduce the time and complexity of initial setup, default file system configurations are included. Additionally, an `AUTO_INIT` feature is available to automatically initialize the file system before the execution of `main()`.
- **Modular Design**: pico-vfs adopts a multi-layer architecture, allowing components such as the block device abstraction layer and the file system abstraction layer to be freely exchanged and added.

## Examples of Use Cases

pico-vfs unlocks various applications for the Raspberry Pi Pico. Here's how it can be used:

- **Basic File Usage in Embedded Environments**: Manage configuration settings and record debug information using familiar POSIX-compliant file APIs.
- **Elastic MQTT Client**: Implements an MQTT client with a local queue to handle network disconnections seamlessly. [View Sample Code](EXAMPLE.md)
- **Complex UNIX Application Porting**: Port SQLite3 to the Pico using pico-vfs, enabling complex data management tasks on the device. [Visit pico-sqlite Project](https://github.com/oyama/pico-sqlite)

These are just a few examples. Utilize the flexibility and capabilities of pico-vfs to develop custom solutions tailored to your projects.

## Modular Design Architecture

```
+---------------------------------+
|         Application Code        |
+---------------------------------+
|       Virtual File System       | <<< POSIX File API Layer
+---------------------------------+
|     littlefs    |      FAT      | <<< File System Layer
+---------------------------------+
|   Flash Memory  |    SD Card    | <<< Block Device Layer
+---------------------------------+
|      Physical Storage Media     |
|      (Flash, SD card, Heap)     |
+---------------------------------+
```

pico-vfs employs an architecture designed to achieve efficient and flexible data management. The combination of file systems and block devices is not restricted, allowing for free reconfiguration:

- **Block Device Layer**: Abstracts differences in physical storage media such as SD cards and flash memory into a consistent interface, enabling developers to easily add or replace storage media.
- **File System Layer**: Integrates different file systems such as littlefs and FAT seamlessly, providing transparent data access to applications.
- **POSIX File API Layer**: Through the above layers, it offers a standard file API compliant with POSIX, used in desktop and server environments. This allows for immediate implementation of file operations like opening, reading, writing, and closing without the need for additional learning costs.

## Setup and Configuration

To add pico-vfs to your project, first place the pico-vfs source in your project directory using the following commands:

```bash
git clone https://github.com/oyama/pico-vfs.git
cd pico-vfs
git submodule update --init
```

Next, add the following lines to your project's `CMakeLists.txt` to include pico-vfs in the build process:

```CMakeLists.txt
add_subdirectory(pico-vfs)
```

Then, add the `pico_enable_filesystem` function to `CMakeLists.txt` to enable the file system:

```CMakeLists.txt
pico_enable_filesystem(${CMAKE_PROJECT_NAME})
```
This sets up your project to use the pico-vfs file system functionality. From your program, call the file system initialization function `fs_init()` to start using it:

```c
#include <pico/stdlib.h>
#include <stdio.h>
#include "filesystem/vfs.h"

int main(void) {
    stdio_init_all();
    fs_init();

    FILE *fp = fopen("HELLO.TXT", "w");
    fprintf(fp, "Hello World!\n");
    fclose(fp);
}
```
By default, 1.4MB of littlefs is mounted at /, and the Pico's onboard flash memory is used as a block device.

## Usage Guide

For detailed usage examples, refer to [EXAMPLE](EXAMPLE.md). For configuration examples involving various block devices and file systems, refer to [examples/fs\_inits](examples/fs_inits/). These include examples such as mounting a FAT file system on an SD card and littlefs on onboard flash memory into a single namespace.

Additionally, a list of POSIX standard file APIs verified to work with pico-vfs is available in [STANDARD](STANDARD.md). Refer to this document to check which APIs are available. For management APIs not included in the POSIX standard, such as file system formatting and mounting, refer to [API](API.md).

## Limitations

For detailed limitations, refer to [LIMITATION](LIMITATION.md). This document provides detailed explanations of potential limitations in specific scenarios and configurations.

## License

This project is licensed under the 3-Clause BSD License. For details, see the [LICENSE](LICENSE.md) file.

## Related Projects and Inspirations

There are multiple ways to add filesystems to the pico-sdk environment. Firstly, FatFs[^1] and littlefs[^2] are popular file system implementations. These filesystem implementations require writing drivers for the block devices used. They also each have their own Unix-like API, but with a distinctive dialect.

While there are several solutions that cover the problem of writing drivers for the Raspberry Pi Pico, the carlk3[^3] implementation is probably the most popular. Especially, it includes DMA support to reduce CPU load and support for even faster SDIO[^4]. This would be the first choice for projects using SD cards and the FAT file system with pico-sdk.

Among multi-filesystem implementations, Memotech-Bill[^5] implementation provides standard I/O support for pico-sdk using the Newlib[^6] hook. The littlefs file system for on-board flash and FatFs for SD cards can be operated as an integrated file system. It is an ambitious project that goes beyond files and integrates character devices such as TTYs and UARTs.

While referring to these existing projects, pico-vfs was developed with the aim of separating and making interchangeable the implementation of drivers and file systems for block devices. This provides functionality similar to that of MicroPython's VFS[^7]and ARM Mbed OS's Storage[^8].

## References

[^1]: [Generic FAT Filesystem Module](http://elm-chan.org/fsw/ff/)
[^2]: [littlefs](https://github.com/littlefs-project/littlefs)
[^3]: [C/C++ Library for SD Cards on the Pico](https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)
[^4]: [SDIO/iSDIO](https://www.sdcard.org/developers/sd-standard-overview/sdio-isdio/)
[^5]: [Standard File Input / Output for the Pico SDK](https://github.com/Memotech-Bill/pico-filesystem)
[^6]: [Newlib is a C library intended for use on embedded systems](https://www.sourceware.org/newlib/)
[^7]: [MicroPython Working with filesystems](https://docs.micropython.org/en/latest/reference/filesystem.html)
[^8]: [ARM Mbed OS - Data storage concepts](https://os.mbed.com/docs/mbed-os/v6.16/apis/data-storage-concepts.html)
