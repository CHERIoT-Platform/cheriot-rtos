# Basic SPI Tests

This will set up two SPI ports on the RPi header to query 2 MCP2518FD CAN controllers. This code won't fully drive them, instead it will just read one of the registers.

## Notes
* We will use the pinmux to enable the RPi ports to connect to the SPI modules and then we will activate the modules.
* The SPI driver ius supposed to be capable of driving the CE (Chip Enable) but that would prevent us from a write, then read operations (SPI is bi-directional). Also, we couldn't get the CE lines to work driectly from the SPI.
* The SPI driver has blocking read and blocking write functions already written.
* SPI transfers are inherently bi directional so, in the long term, we may require a blocking_transfer function, however, it may not be needed for this example.
* We will be using the Waveshare 2-CH CAN FD HAT for the purposes of testing. Details can be found here: https://www.waveshare.com/wiki/2-CH_CAN_FD_HAT
* SPI0 is connected to other devices. Only SPI1 and SPI2 are coinnected to the RPi header. So SPI1 is connected to RPi SPI0 and SPI2 is connected to RPi SPI1 (confused yet?)
* The SPI interrupts are currently not working but registers are reserved for them.
* If you uncomment SPI_TRANSFER_TEST it will use the blocking_transfer() function to perform a bidirectional transfer.

## SPI Naming
There are multiple names for the SPI signals and the documentation uses both in different places.
| Name 1 | Name 2 | Description
| ------ | ------ | ---------------------------------------------------------------------------------- |
|   CE   |   CS   | Chip Enable or Chip Select. Active low (pull down to activate).                    |
|  MOSI  |  COPI  | Chip Out, Peripheral In                                                            |
|  MISO  |  CIPI  | Chip In, Peripheral Out                                                            |
|  SCLK  |   SCK  | Serial Clock. Depending on the mode this could transfer on falling or rising edge. |

## SPI Pins Required

|  RPi Name | I/O | Header # | RPi GPIO | Pinmux Name                          | Pinmux Option  | Waveshare Name | Notes |
| --------- | --- | -------- | -------- | ------------------------------------ | -------------- | -------------- | --- |
| SPI0_CE_0 |  O  |    24    |     8    | SonataPinmux::OutputPin::rph_g8      | 1 (spi_1_cs_0) or 2 (gpio_0_ios_8) | CS_0           | Drive manually for now (option 2). |
| SPI0_COPI |  O  |    19    |    10    | SonataPinmux::OutputPin::rph_g10     | 1 (spi_1_copi) | MOSI_0         |     |
| SPI0_CIPO |  I  |    21    |     9    | SonataPinmux::BlockInput::spi_1_cipo | 1 (rph_g9)     | MISO_0         |     |
| SPI0_SCLK |  O  |    23    |    11    | SonataPinmux::OutputPin::rph_g11     | 1 (spi_1_sclk) | SCK_0          |     |
|           |  I  |    22    |    25    |                                      |                | INT_0          |     |
| SPI1_CE_0 |  O  |    12    |    18    | SonataPinmux::OutputPin::rph_g18     | 1 (spi_2_cs_0) or 2 (gpio_0_ios_18) | CS_1           | Drive manually for now (option 2). |
| SPI1_COPI |  O  |    38    |    20    | SonataPinmux::OutputPin::rph_g20     | 1 (spi_2_copi) | MOSI_1         |     |
| SPI1_CIPO |  I  |    35    |    19    | SonataPinmux::BlockInput::spi_2_cipo | 1 (rph_g19)    | MISO_1         |     |
| SPI1_SCLK |  O  |    40    |    21    | SonataPinmux::OutputPin::rph_g21     | 1 (spi_1_sclk) | SCK_1          |     |
|           |  I  |    18    |    24    |                                      |                | INT_1          |     |
Notes:
* It's interesting that the pinmux has entries for the Chip Enables but the SPI block doesn't seem capable of driving them yet.
* The interupt signals from the MCP251X chips may not be needed, but I've included them anyway, just in case.
