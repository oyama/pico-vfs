# Example of pico-vfs library

| App             | Description                                                             |
|-----------------|-------------------------------------------------------------------------|
| [hello](examples/hello)  | Hello filesystem world.                                                 |
| [fs\_inits](examples/fs_inits) | Examples of file system layout combinations.                      |
| [multicore\_logger](examples/multicore_logger) | Multi-core logger that saves 1kHz sampling data to an SD card |
| [elastic\_mqtt\_client](examples/elastic_mqtt_client) |Implements an MQTT client with a local queue to handle network disconnections seamlessly. |
| [usb\_logger](examples/usb_logger) | Data logger that mounts littlefs and FAT on flash memory and shares it with a PC via USB mass storage class.|
| [benchmark](examples/benchmark)| Data transfer tests with different block devices and different file system combinations.|

## Building sample code

The [pico-sdk](https://github.com/raspberrypi/pico-sdk) build environment is required to build the demonstration, see  [Getting started with Raspberry Pi Pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf) to prepare the toolchain for your platform. This project contains a _git submodule_. When cloning, the `--recursive` option must be given or a separate `git submodule update` must be performed.

Firmware can be built from the _CMake_ build directory of _pico-vfs_.

```bash
mkdir build; cd build/
PICO_SDK_PATH=/path/to/pico-sdk cmake ..
make hello fs_init_example multicore_logger usb_logger benchmark
```

The `elastic_mqtt_client` uses Wi-Fi, so `PICO_BOARD=pico_w` needs to be specified. Furthermore, `WIFI_SSID`, `WIFI_PASSWORD`, `MQTT_USER` and `MQTT_PASSWORD` must be specified in the environment variables when running cmake. This example uses [Adafruit IO](https://io.adafruit.com/)'s MQTT server.

```bash
PICO_SDK_PATH=/path/to/pico-sdk WIFI_SSID=SSID WIFI_PASSWORD=PASSWORD MQTT_USER=USER MQTT_PASSWORD=PASSWORD cmake .. -DPICO_BOARD=pico_w
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

