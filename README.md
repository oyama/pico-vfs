# Thin virtual file system for Raspberry Pi Pico

__pico-vfs__ adds a Unix-like, thin virtual file system (VFS) layer to the Raspberry Pi Pico. Any __block device__ and __file system__ can be combined into a single virtual file system, starting from root `/`.

## Quick Start Guide

The pico-vfs consists of a _block device_, a _file system_ and a _VFS layer_ that brings these together.
Users are free to combine the storage device's block devices and the file systems deployed on them to configure the best file system for their embedded project.

```c
// Block Device creation: Create a block device for the onboard flash memory.
#define  FLASH_START_AT  (0.5 * 1024 * 1024)
blockdevice_t *flash = blockdevice_flash_create(FLASH_START_AT, 0);

// Block Device creation: Create a block device for an SD card connected via SPI.
blockdevice_t *sd = blockdevice_sd_create(spi0,
                                          PICO_DEFAULT_SPI_RX_PIN,
                                          PICO_DEFAULT_SPI_TX_PIN,
                                          PICO_DEFAULT_SPI_SCK_PIN,
                                          PICO_DEFAULT_SPI_CSN_PIN,
                                          24 * MHZ,
                                          false);

// File System creation: Create a FAT file system instance.
filesystem_t *fat = filesystem_fat_create();

// File System creation: Create a LittleFS file system instance with specific parameters.
filesystem_t *littlefs = filesystem_littlefs_create(500, 16);

// Mounting: Mount the LittleFS on the onboard flash memory at root directory.
fs_mount("/", littlefs, flash);

// Mounting: Mount the FAT file system on the SD card at '/sd'.
fs_mount("/sd", fat, sd);

// File Operations: Open a file '/sd/README.TXT' on the SD card for writing. Create if not exists.
int fd = fs_open("/sd/README.TXT", O_WRONLY|O_CREAT);

char buffer[] = "Hello World!";
fs_write(fd, buffer, sizeof(buffer));

fs_close(fd);
```
In addition, the individual block devices and file systems are available as separate INTERFACE libraries, so that the file system can be configured with the minimum required footprint.

## Block device

A block device is a struct implementing `blockdevice_t`. The data type contains callback functions and variables depending on the block device, and allowing different block devices to be operated with a consistent interface.

- [src/blockdevice/flash.c](src/blockdevice/flash.c): Raspberry Pi Pico on-board flash memory block device
- [src/blockdevice/sd.c](src/blockdevice/sd.c): SPI-connected SD or MMC card block device

## File system

The file system is a struct implementation `filesystem_t`. This data types contains callback functions and variables depending on the file system, and allows different file systems to be operated with a consistent interface.

- [src/filesystem/fat.c](src/filesystem/fat.c): FAT file system with FatFs[^1]
- [src/filesystem/littlefs.c](src/filesystem/littlefs.c): littlefs[^2] filesystem

## VFS

Block devices and file systems are integrated into a UNIX-like API by VFS layer. Users can `fs_format()` or `fs_mount()` a combination of block devices and file systems, which can then be `fs_open()` for further `fs_read()` and `fs_write()`.

- [include/filesystem/vfs.h](include/filesystem/vfs.h): Virtual file system layer

## Demonstration

For demonstration purposes, benchmark test firmware for combined block devices and file systems is included.
The pico-sdk[^3] build environment is required to build the demonstration, see  _Getting started with Raspberry Pi Pico_[^4] to prepare the toolchain for your platform. This project contains a _git submodule_. When cloning, the `--recursive` option must be given or a separate `git submodule update` must be performed.

```bash
git clone --recursive https://github.com/oyama/pico-vfs.git
cd pico-vfs
mkdir build; cd build
PICO_SDK_FETCH_FROM_GIT=1 cmake ..
make benchmark
```
The above examples specify the environment variable `PICO_SDK_FETCH_FROM_GIT` to download the pico-sdk from GitHub. If you want to specify a locally deployed pico-sdk, you should set it with the `PICO_SDK_PATH` environment variable.
Once built, the firmware `benchmark.uf2` will be generated. Simply drag and drop it onto your device to install.

If no SD card device is connected, the `WITHOUT_BLOCKDEVICE_SD` option can be specified to skip the SD card manipulation procedure from the demo and unit tests.

```bash
PICO_SDK_FETCH_FROM_GIT=1 cmake .. -DWITHOUT_BLOCKDEVICE_SD=YES
make benchmark
```

### Circuit Diagram

If an SD card is to be used, a separate circuit must be connected via SPI. As an example, the schematic using the Adafruit MicroSD card breakout board+[^5] is as follows

![adafruit-microsd](https://github.com/oyama/pico-vfs/assets/27072/b96e8493-4f3f-4d44-964d-8ada61745dff)

The spi and pin used in the block device argument can be customised. The following pins are used in the demonstration.

| pin | SPI      | description             |
|-----|----------|-------------------------|
| 24  | SPI0 SCK | SPI clock               |
| 25  | SPI0 TX  | SPI Master Out Slave In |
| 21  | SPI0 RX  | SPI Master In Slave Out |
| 22  | SPI0 CSn | SPI Chip select         |

## Integration into project

The pico-vfs components are defined as _INTERFACE_ libraries. You can build by adding pico-vfs to your project's `CMakeLists.txt` and specifying the libraries of the components you want to introduce.

```CMakeLists.txt
add_subdirectory(pico-vfs)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  blockdevice_sd
  blockdevice_flash
  filesystem_fat
  filesystem_littlefs
  filesystem_vfs
)
```

In this example, all components are registered, but if, for example, the block device is only on-board `flash` and the file system is only `littlefs`, the following works by registering only the components required.

```CMakeLists.txt
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  blockdevice_flash
  filesystem_littlefs
  filesystem_vfs
)
```

If desired, the VFS layer can be removed and only the file system layer can be used on bare metal.

```CMakeLists.txt
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  blockdevice_flash
  filesystem_littlefs
)
```

## Related Projects and Inspirations

There are multiple ways to add filesystems to the pico-sdk environment. Firstly, FatFs[^1] and littlefs[^2] are popular file system implementations. These filesystem implementations require writing drivers for the block devices used. They also each have their own Unix-like API, but with a distinctive dialect.
While there are several solutions that cover the problem of writing drivers for the Raspberry Pi Pico, the carlk3[^6] implementation is probably the most popular.
Especially, it includes DMA support to reduce CPU load and support for even faster SDIO[^7]. This would be the first choice for projects using SD cards and the FAT file system with pico-sdk.
Among multi-filesystem implementations, Memotech-Bill[^8] implementation provides standard I/O support for pico-sdk using the newlib[^9] hook. The littlefs file system for on-board flash and FatFs for SD cards can be operated as an integrated file system. It is an ambitious project that goes beyond files and integrates character devices such as TTYs and UARTs.

While referring to these existing projects, _pico-vfs_ aims to make the implementation of drivers and file systems for block devices separate and interchangeable, similar to MicroPython's VFS[^10] and ARM Mbed OS's Storage[^11].

![block-diagram](https://github.com/oyama/pico-vfs/assets/27072/ddc89f54-b011-40d2-b150-22495a4529c1)

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
