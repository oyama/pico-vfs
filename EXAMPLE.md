# Example of pico-vfs library

| App             | Description                                                             |
|-----------------|-------------------------------------------------------------------------|
| [hello](hello)  | Hello filesystem world.                                                 |
| [fs\_inits](fs_inits) | Examples of file system layout combinations.                      |
| [benchmark](benchmark)| Data transfer tests with different block devices and different file system combinations.|
| [usb\_msc\_logger](usb_msc_logger) | Data logger that mounts littlefs and FAT on flash memory and shares it with a PC via USB mass storage class.|


Examples include benchmark test firmware for combinations of heterogeneous block devices and heterogeneous file systems, and logger firmware that splits the on-board flash memory in two and uses it with different file systems.

The [pico-sdk](https://github.com/raspberrypi/pico-sdk) build environment is required to build the demonstration, see  [Getting started with Raspberry Pi Pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf) to prepare the toolchain for your platform. This project contains a _git submodule_. When cloning, the `--recursive` option must be given or a separate `git submodule update` must be performed.

## Building sample code

Firmware can be built from the _CMake_ build directory of _pico-vfs_.

```bash
mkdir build; cd build/
PICO_SDK_PATH=/path/to/pico-sdk cmake ..
make hello fs_init_example benchmark logger
```
If no SD card device is connected, the `-DWITHOUT_BLOCKDEVICE_SD` option can be specified to skip the SD card manipulation procedure from the demo and unit tests.

### Circuit Diagram

If an SD card is to be used, a separate circuit must be connected via SPI. As an example, the schematic using the [Adafruit MicroSD card breakout board+](https://www.adafruit.com/product/254) is as follows

![adafruit-microsd](https://github.com/oyama/pico-vfs/assets/27072/b96e8493-4f3f-4d44-964d-8ada61745dff)

The spi and pin used in the block device argument can be customised. The following pins are used in the demonstration.

| Pin  | PCB pin | Usage    | description             |
|------|---------|----------|-------------------------|
| GP18 | 24      | SPI0 SCK | SPI clock               |
| GP19 | 25      | SPI0 TX  | SPI Master Out Slave In |
| GP16 | 21      | SPI0 RX  | SPI Master In Slave Out |
| GP17 | 22      | SPI0 CSn | SPI Chip select         |

