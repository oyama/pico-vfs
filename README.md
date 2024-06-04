# Thin virtual file system for Raspberry Pi Pico

__pico-vfs__ seamlessly integrates POSIX file APIs on the Raspberry Pi Pico, allowing users to interact with the file system using a familiar, standardized interface. pico-vfs offers the flexibility to freely combine various __block devices__ and __file systems__ to configure an integrated file system, and is easy to set up from the start.

## Key Features

- **POSIX API Support**: Utilize standard POSIX file operations such as `open`, `read`, `write`, and `close` to achieve intuitive file handling.
- **Modular Design**: Easily exchange underlying block devices and file systems without modifying application code, providing high flexibility.
- **Pre-configured File System**: Includes a default file system setup, reducing initial setup time and complexity.
- **Customizability**: Use the `pico_enable_filesystem` function in your project's CMake setup to configure and activate different file systems and block devices as needed.

pico-vfs is designed with resource-constrained environments in mind, making it easy to customize and readily adaptable to specific project needs.

## Quick Start Guide

To use pico-vfs, add the file system enable instructions to your project's `CMakeLists.txt`:

```CMakeLists.txt
pico_enable_filesystem(${CMAKE_PROJECT_NAME})
```

Simply call the file system initialization function `fs_init()` from your program:

```c
#include <pico/stdlib.h>
#include <stdio.h>
#include "filesystem/vfs.h"

int main(void) {
    stdio_init_all();
    fs_init();

    FILE *fp = fopen("/HELLO.TXT", "w");
    fprintf(fp, "Hello World!\n");
    fclose(fp);
}
```

By default, 1.4MB of littlefs is mounted at `/`, and the Pico's onboard flash memory is used as a block device.

If you want to customize the storage size, specify the required storage size in `pico_enable_filesystem`:

```CMakeLists.txt
pico_enable_filesystem(${CMAKE_PROJECT_NAME} FS_SIZE 1441792)
```
pico-vfs can be further customized. The sample program included in [examples/default\_fs/my\_fs\_init.c](examples/default_fs/my_fs_init.c) demonstrates mounting a FAT file system using an SD card as a block device and littlefs on the onboard flash memory in a single namespace.

## Architecture overview

To provide a flexible and lightweight file system framework for embedded systems, pico-vfs consists of three layers: a __block device__ layer that abstracts storage devices, a __file system__ layer that abstracts file systems, and a __virtual file system__ layer that links these to the POSIX API.

### Block device layer

A block device is an object implementing `blockdevice_t`, which includes callback functions and variables tailored to the block device, allowing different block devices to be operated with a consistent interface.

- [src/blockdevice/flash.c](src/blockdevice/flash.c): Raspberry Pi Pico on-board flash memory block device
- [src/blockdevice/sd.c](src/blockdevice/sd.c): SPI-connected SD or MMC card block device
- [src/blockdevice/heap.c](src/blockdevice/heap.c): Heap memory block device

### File system layer

The file system is an object implementing `filesystem_t`, which contains callback functions and variables based on the file system, enabling different file systems to be operated with a consistent interface.

- [src/filesystem/fat.c](src/filesystem/fat.c): FAT file system with FatFs[^1]
- [src/filesystem/littlefs.c](src/filesystem/littlefs.c): littlefs[^2] filesystem

### VFS layer

Block devices and file systems are integrated into the POSIX file API by the VFS layer. Users can perform operations like `open()`, `read()`, and `write()` as usual, and also use higher-level functions such as `fopen()`, `fprintf()`, and `fgets()`. The available POSIX and C standard file APIs are listed in [STANDARD.md](STANDARD.md). See [API.md](API.md) for file system management APIs not defined in POSIX.

- [src/filesystem/vfs.c](src/filesystem/vfs.c): Virtual file system layer

Furthermore, individual block devices and file systems are available as separate INTERFACE libraries, enabling the file system to be configured with minimal footprint.

## Examples

Examples include benchmark test firmware for combinations of heterogeneous block devices and heterogeneous file systems, and logger firmware that splits the on-board flash memory in two and uses it with different file systems.

The pico-sdk[^3] build environment is required to build the demonstration, see  _Getting started with Raspberry Pi Pico_[^4] to prepare the toolchain for your platform. This project contains a _git submodule_. When cloning, the `--recursive` option must be given or a separate `git submodule update` must be performed.

```bash
git clone --recursive https://github.com/oyama/pico-vfs.git
cd pico-vfs
mkdir build; cd build
PICO_SDK_FETCH_FROM_GIT=1 cmake ..
make benchmark logger default_fs tests
```
The above examples specify the environment variable `PICO_SDK_FETCH_FROM_GIT` to download the pico-sdk from GitHub. If you want to specify a locally deployed pico-sdk, you should set it with the `PICO_SDK_PATH` environment variable.
The build generates the firmware `examples/benchmark/benchmark.uf2`, `examples/usb_msc_logger/logger.uf2` and `examples/default_fs/default_fs.uf2`. Both can be installed by simply dragging and dropping them onto a Raspberry Pi Pico running in BOOTSEL mode.

If no SD card device is connected, the `WITHOUT_BLOCKDEVICE_SD` option can be specified to skip the SD card manipulation procedure from the demo and unit tests.

```bash
PICO_SDK_FETCH_FROM_GIT=1 cmake .. -DWITHOUT_BLOCKDEVICE_SD=YES
```

### Circuit Diagram

If an SD card is to be used, a separate circuit must be connected via SPI. As an example, the schematic using the Adafruit MicroSD card breakout board+[^5] is as follows

![adafruit-microsd](https://github.com/oyama/pico-vfs/assets/27072/b96e8493-4f3f-4d44-964d-8ada61745dff)

The spi and pin used in the block device argument can be customised. The following pins are used in the demonstration.

| Pin  | PCB pin | Usage    | description             |
|------|---------|----------|-------------------------|
| GP18 | 24      | SPI0 SCK | SPI clock               |
| GP19 | 25      | SPI0 TX  | SPI Master Out Slave In |
| GP16 | 21      | SPI0 RX  | SPI Master In Slave Out |
| GP17 | 22      | SPI0 CSn | SPI Chip select         |

## Integration into project

Add the pico-vfs repository to your project by `git submodule` or simply `git clone`. Since pico-vfs contains submodules, recommended with `git submodule update --init --recursive`:

```bash
git submodule add https://github.com/oyama/pico-vfs.git
git submodule update --init --recursive
```

Add `add_subdirectory` to `CMakeLists.txt` so that they can be built together:
```CMakeLists.txt
add_subdirectory(pico-vfs)

pico_enable_filesystem(${CMAKE_PROJECT_NAME})
```

In this example, a minimal number of components are configured by default.

For instance, a configuration where the onboard flash memory is set up with littlefs and combined with the FAT file system on an SD card is as follows:

```CMakeLists.txt
pico_enable_filesystem(${CMAKE_PROJECT_NAME} FS_INIT my_fs_init.c)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  blockdevice_flash
  blockdevice_sd
  filesystem_littlefs
  filesystem_fat
  filesystem_vfs
```
You can add the `fs_init()` function to your project to freely layout the file system.

```my_fs_init.c
#include <hardware/clocks.h>
#include <hardware/flash.h>
#include "blockdevice/flash.h"
#include "blockdevice/sd.h"
#include "filesystem/fat.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"

bool fs_init(void) {
    blockdevice_t *flash = blockdevice_flash_create(PICO_FLASH_SIZE_BYTES - DEFAULT_FS_SIZE, 0);
    blockdevice_t *sd = blockdevice_sd_create(spi0,
                                              PICO_DEFAULT_SPI_TX_PIN,
                                              PICO_DEFAULT_SPI_RX_PIN,
                                              PICO_DEFAULT_SPI_SCK_PIN,
                                              PICO_DEFAULT_SPI_CSN_PIN,
                                              10 * MHZ,
                                              false);
    filesystem_t *lfs = filesystem_littlefs_create(500, 16);
    filesystem_t *fat = filesystem_fat_create();

    fs_mount("/", lfs, flash);
    fs_mount("/sd", fat, sd);
    return true;
}
```

Of course, a more bare-metal use, where only block devices are utilized, is also possible.

## Related Projects and Inspirations

There are multiple ways to add filesystems to the pico-sdk environment. Firstly, FatFs[^1] and littlefs[^2] are popular file system implementations. These filesystem implementations require writing drivers for the block devices used. They also each have their own Unix-like API, but with a distinctive dialect.

While there are several solutions that cover the problem of writing drivers for the Raspberry Pi Pico, the carlk3[^6] implementation is probably the most popular. Especially, it includes DMA support to reduce CPU load and support for even faster SDIO[^7]. This would be the first choice for projects using SD cards and the FAT file system with pico-sdk.

Among multi-filesystem implementations, Memotech-Bill[^8] implementation provides standard I/O support for pico-sdk using the newlib[^9] hook. The littlefs file system for on-board flash and FatFs for SD cards can be operated as an integrated file system. It is an ambitious project that goes beyond files and integrates character devices such as TTYs and UARTs.

While referring to these existing projects, _pico-vfs_ aims to make the implementation of drivers and file systems for block devices separate and interchangeable, similar to MicroPython's VFS[^10] and ARM Mbed OS's Storage[^11].

## References

[^1]: [Generic FAT Filesystem Module](http://elm-chan.org/fsw/ff/)
[^2]: [littlefs](https://github.com/littlefs-project/littlefs)
[^3]: [pico-sdk](https://github.com/raspberrypi/pico-sdk)
[^4]: [Getting started with Raspberry Pi Pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf)
[^5]: [adafruit MicroSD card breakout board+](https://www.adafruit.com/product/254)
[^6]: [C/C++ Library for SD Cards on the Pico](https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)
[^7]: [SDIO/iSDIO](https://www.sdcard.org/developers/sd-standard-overview/sdio-isdio/)
[^8]: [Standard File Input / Output for the Pico SDK](https://github.com/Memotech-Bill/pico-filesystem)
[^9]: [Newlib is a C library intended for use on embedded systems](https://www.sourceware.org/newlib/)
[^10]: [MicroPython Working with filesystems](https://docs.micropython.org/en/latest/reference/filesystem.html)
[^11]: [ARM Mbed OS - Data storage concepts](https://os.mbed.com/docs/mbed-os/v6.16/apis/data-storage-concepts.html)
