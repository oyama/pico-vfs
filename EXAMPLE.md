# Example of pico-vfs library

| App             | Description                                                             |
|-----------------|-------------------------------------------------------------------------|
| [hello](examples/hello)  | Hello filesystem world.                                                 |
| [fs\_inits](examples/fs_inits) | Examples of file system layout combinations.                      |
| [usb\_msc\_logger](examples/usb_msc_logger) | Data logger that mounts littlefs and FAT on flash memory and shares it with a PC via USB mass storage class.|
| [elastic\_mqtt\_client](examples/elastic_mqtt_client) |Implements an MQTT client with a local queue to handle network disconnections seamlessly. |
| [benchmark](examples/benchmark)| Data transfer tests with different block devices and different file system combinations.|

## Building sample code

The [pico-sdk](https://github.com/raspberrypi/pico-sdk) build environment is required to build the demonstration, see  [Getting started with Raspberry Pi Pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf) to prepare the toolchain for your platform. This project contains a _git submodule_. When cloning, the `--recursive` option must be given or a separate `git submodule update` must be performed.

Firmware can be built from the _CMake_ build directory of _pico-vfs_.

```bash
mkdir build; cd build/
PICO_SDK_PATH=/path/to/pico-sdk cmake ..
make hello fs_init_example logger benchmark
```

`PICO_BOARD=pico_w` must be specified for `elastic_mqtt_client` as it uses Wi-Fi. In addition, the Wi-Fi settings and the MQTT server to connect to must be changed in the source code.

```bash
PICO_SDK_PATH=/path/to/pico-sdk cmake .. -DPICO_BOARD=pico_w
make elastic_mqtt_client
```

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

