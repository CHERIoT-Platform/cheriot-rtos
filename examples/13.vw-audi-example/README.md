# VW Audi CAN Example Example

This code uses the MCP251XFD SPI to CAN driver based on [Emandhal's generic MCP251XFD driver](https://github.com/Emandhal/MCP251XFD). We have more examples of this CAN driver in other examples.

## What this code Does
This code opens the CAN interface as CAN2.0 at 500kbps. We look for sepcific CAN messages to extract certain events that we are interested in.

## Hardware Design
* We will be using the [Waveshare 2-CH CAN FD HAT](https://www.waveshare.com/wiki/2-CH_CAN_FD_HAT) for the purposes of testing.
* The Waveshare board uses a 40MHz crystal, which we need to know to cofigure the device.
* This device has two MCP2518FD SPI to CAN ICs.
* Each device is on a separate SPI bus.
* You can also use the, on-board, links (soldering required) to place them both onto one SPI bus with different CS lines.
* It is also possible to stack these devices with two ICs on each SPI giving a maximum of four CAN buses on one RPi header. Pretty cool.
* Waveshare give examples of use for Raspberry Pi (using a built-in driver from Raspberry Pi OS) and Arduino.
* Our target SPI clock rate is 1Mbps.

## Notes
* We use the pinmux to enable the RPi ports to connect to the SPI modules and then we will activate the modules.
* SPI0 is connected to other devices. Only SPI1 and SPI2 are connected to the RPi header. So SPI1 is connected to RPi SPI0 and SPI2 is connected to RPi SPI1 (confused yet?)
* The SPI interrupts weren't working when we first wrote this example, although they do now work so we could update.

## SPI Pins Required
We're going to run each CAN ona separate SPI to reduce the risk of accidental cross-talk.
|  RPi Name | I/O | Header # | RPi GPIO | Pinmux Name                          | Pinmux Option  | Waveshare Name | Notes |
| --------- | --- | -------- | -------- | ------------------------------------ | -------------- | -------------- | --- |
| SPI0_CE_0 |  O  |    24    |     8    | SonataPinmux::OutputPin::rph_g8      | 1 (spi_1_cs_0) | CS_0           | Drive manually for now. |
| SPI0_COPI |  O  |    19    |    10    | SonataPinmux::OutputPin::rph_g10     | 1 (spi_1_copi) | MOSI_0         |     |
| SPI0_CIPO |  I  |    21    |     9    | SonataPinmux::BlockInput::spi_1_cipo | 1 (rph_g9)     | MISO_0         |     |
| SPI0_SCLK |  O  |    23    |    11    | SonataPinmux::OutputPin::rph_g11     | 1 (spi_1_sclk) | SCK_0          |     |
|           |  I  |    22    |    25    |                                      |                | INT_0          |     |
| SPI1_CE_0 |  O  |    12    |    18    | SonataPinmux::OutputPin::rph_g18     | 1 (spi_2_cs_0) | CS_1           | Drive manually for now. |
| SPI1_COPI |  O  |    38    |    20    | SonataPinmux::OutputPin::rph_g20     | 1 (spi_2_copi) | MOSI_1         |     |
| SPI1_CIPO |  I  |    35    |    19    | SonataPinmux::BlockInput::spi_2_cipo | 1 (rph_g19)    | MISO_1         |     |
| SPI1_SCLK |  O  |    40    |    21    | SonataPinmux::OutputPin::rph_g21     | 1 (spi_1_sclk) | SCK_1          |     |
|           |  I  |    18    |    24    |                                      |                | INT_1          |     |
