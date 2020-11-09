Air Quality Monitor
===================

This is an Air Quality Monitor which will display data such as the temperature,
pressure, and relative humidity on an screen using the [LittlevGL] graphics
framework and the [RIOT] operating system.

[LittlevGL]: https://lvgl.io/
[RIOT]: https://riot-os.org/

### Example board setup

The application works on any board that has an screen and some sensors attached to it.
However the recommended and most tested setup is with:

- The [ESP32-WROVER-KIT] board, which has a built-in ILI9341 screen.
- A [BME280] I2C Humidity/Pressure/Temperature sensor.
- A [GP2Y1010AU0F] Compact Optical Dust sensor.

[ESP32-WROVER-KIT]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/esp32/get-started-wrover-kit.html
[BME280]: https://www.bosch-sensortec.com/products/environmental-sensors/humidity-sensors-bme280/
[GP2Y1010AU0F]: https://www.sparkfun.com/datasheets/Sensors/gp2y1010au_e.pdf

The wiring of the BME280 sensor to the ESP32-WROVER-KIT is as follows:

| BME280 | ESP32-WROVER-KIT |
|--------|------------------|
| VCC    | Any 3.3V pin     |
| GND    | Any GND pin      |
| SCL    | IO27             |
| SDA    | IO26             |

Depending on the breakout board for a BME280 sensor you may need to change
the I2C address directly on the Makefile file.

The wiring of the GP2Y1010AU0F dust sensor to the ESP32-WROVER-KIT is as
follows:


| GP2Y1010AU0F | ESP32-WROVER-KIT |
|--------------|------------------|
| VCC          | Any 3.3V pin     |
| GND          | Any GND pin      |
| ILED         | IO22             |
| AOUT         | IO34             |

- :warning: the dust sensor normally uses 5V to operate, Waveshare breakout
boards can be plugged directly to a 3.3V supply.

### Compiling and flashing the application

To compile the project you may need to first install required tools, on the case
of an ESP32 board you can find instructions here:

- [RIOT ESP32 Toolchain](https://doc.riot-os.org/group__cpu__esp32.html#esp32_toolchain)

Then you may need to clone the RIOT-OS repository on a different path than this
project that you can reference through the `RIOTBASE` environment variable, example:

```bash
cd $HOME
git clone https://github.com/RIOT-OS/RIOT/ -b 2020.10-branch
export RIOTBASE=$HOME/RIOT
```

As the application works without modification on the `esp32-wrover-kit` board. To
build, flash and run the application for this board, just use:

```
make flash
```
