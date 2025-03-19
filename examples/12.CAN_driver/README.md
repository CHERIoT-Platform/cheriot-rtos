# MCP251XFD Driver and Example

This code contains an implementation of an MCP251XFD SPI to CAN driver.
We are using:

[Emandhal's generic MCP251XFD driver](https://github.com/Emandhal/MCP251XFD) is fully featured and designed to run on any embedded platform. It claims to implement every feature of the MCP2518FD and MCP2517FD (including auto detection of the specific model type). There is fully documentation but we've found it diffficult to read in places. This is for little endian devices only, fortunately RISCV is little endian. This driver is alos designed to allow for multiple instances, with instance information being passed to each function as a struct.

## What this code Does
This code opens the CAN interface as CAN2.0 at 500kbps. It writes message ID 300 with some dynamic data on the TXQ. It has for filters configured to receive data:
1. FIFO 1: Read CAN2.0 packets with Standard ID (11-bit) in the range 0x000..0x1FF.
2. FIFO 2: Read CAN2.0 packets with Standard ID (11-bit) in the range 0x200..0x3FF.
3. FIFO 3: Read CAN2.0 packets with Standard ID (11-bit) in the range 0x400..0x5FF.
4. FIFO 4: Read all other CAN2.0 packets with Standard or Extended IDs.
Since a lower ID has higher priority, you could use this to read priority data first.
Data read back is displayed in a simple form on the serial line using the debug output.

We have configured FIFO 5 to FIFO 8 as transmit FIFOs but we are not using them in this example.

## Hardware Design
* We will be using the [Waveshare 2-CH CAN FD HAT](https://www.waveshare.com/wiki/2-CH_CAN_FD_HAT) for the purposes of testing.
* The Waveshare board uses a 40MHz crystal, which we need to know to configure the device.
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
| SPI0_CE_0 |  O  |    24    |     8    | SonataPinmux::OutputPin::rph_g8      | 1 (spi_1_cs_0) | CS_0           |     |
| SPI0_COPI |  O  |    19    |    10    | SonataPinmux::OutputPin::rph_g10     | 1 (spi_1_copi) | MOSI_0         |     |
| SPI0_CIPO |  I  |    21    |     9    | SonataPinmux::BlockInput::spi_1_cipo | 1 (rph_g9)     | MISO_0         |     |
| SPI0_SCLK |  O  |    23    |    11    | SonataPinmux::OutputPin::rph_g11     | 1 (spi_1_sclk) | SCK_0          |     |
|           |  I  |    22    |    25    |                                      |                | INT_0          |     |
| SPI1_CE_0 |  O  |    12    |    18    | SonataPinmux::OutputPin::rph_g18     | 1 (spi_2_cs_0) | CS_1           |     |
| SPI1_COPI |  O  |    38    |    20    | SonataPinmux::OutputPin::rph_g20     | 1 (spi_2_copi) | MOSI_1         |     |
| SPI1_CIPO |  I  |    35    |    19    | SonataPinmux::BlockInput::spi_2_cipo | 1 (rph_g19)    | MISO_1         |     |
| SPI1_SCLK |  O  |    40    |    21    | SonataPinmux::OutputPin::rph_g21     | 1 (spi_1_sclk) | SCK_1          |     |
|           |  I  |    18    |    24    |                                      |                | INT_1          |     |

## [Emandhal's MCP251XFD driver](https://github.com/Emandhal/MCP251XFD)

Notes on configuration
### General Driver Configuration
This done in a variety of files. Exmaples of these file can be found in sections 18 and 19 of the user manual on the Github page.

#### Conf_MCP251XFD.h
See section 18 of the manual for an example. Basically, we don't need to change it.
* Leave ```MCP251xFD_TRANS_BUF_SIZE``` as ```( 2+1+8+64+4+2 )```
* Leave ```CHECK_NULL_PARAM``` off for now.

#### Board specific drivers
We need to create the following board specific functions and pass pointers to these into each instance. We shouldn't be calling these functions directly, the driver will call them for us.
1. `uint32_t GetCurrentms(void)`
  This function will be called when the driver needs to get current milliseconds.
  Return the current run time in ms.
2. `uint16_t ComputeCRC16(const uint8_t* data, size_t size)`
  Where:
    * `*data` Is the pointed byte stream
    * `size` Is the size of the pointed byte stream
    * Returns the CRC computed
  This function will be called when a CRC16-CMS computation is needed (ie. in CRC mode or Safe Write). In normal
mode, this can point to NULL. We should use this though - I've noticed occasional issues with SPI transfers, using CRC will allow us to detect errors and try again. According to the MCP2518FD's datasheet, the CRC16-USB algorithm should be used.
3. `eERRORRESULT MCP251XFD_InterfaceInit(void *pIntDev, uint8_t chipSelect, const uint32_t sckFreq)`
  This function will be called at driver initialization to configure the interface driver SPI
  Where:
    * `*pIntDev` Is the MCP251XFD_Desc.InterfaceDevice of the device that call the interface initialization
    * `chipSelect` Is the Chip Select index to use for the SPI initialization
    * `sckFreq` Is the SCK frequency in Hz to set at the interface initialization
    * Returns an #eERRORRESULT value enum
4. `eERRORRESULT MCP251XFD_InterfaceTransfer(void *pIntDev, uint8_t chipSelect, uint8_t *txData, uint8_t *rxData,
size_t size)`
  This function will be called at driver read/write data from/to the interface driver SPI.
    * `*pIntDev` Is the MCP251XFD_Desc.InterfaceDevice of the device that call this function
    * `chipSelect` Is the Chip Select index to use for the SPI transfer
    * `*txData` Is the buffer to be transmit to through the SPI interface
    * `*rxData` Is the buffer to be received to through the SPI interface (can be NULL if it's just a send of
data)
    * `size` Is the size of data to be send and received trough SPI. txData and rxData shall be at least the
same size
    * Returns an #eERRORRESULT value enum

