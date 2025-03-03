# VW Audi CAN Example Example
This is a simple example of interfacing to a vehicle with CAN. It is designed to work on the VAG MQB and MLB platforms.

This code uses the MCP251XFD SPI to CAN driver based on [Emandhal's generic MCP251XFD driver](https://github.com/Emandhal/MCP251XFD). We have more examples of this CAN driver in other examples.

## What this code Does
This code opens the CAN interface as CAN2.0 at 500kbps. We look for sepcific CAN messages to extract certain events that we are interested in. We can also write to the CAN to flash the indicators/blinkers.

This can read the messages on the Comfort CAN that refer to the steering wheel buttons. It tells you which of the steering buttons are being pressed and when they are released. This information is output on the serial using the debug methods. In addtion, we can also detect driving by looking for the vehicle starting and also for a signal that the electronics have been switched off and the driver's door has been opened. This was etsted on a Manual 2020 Audi A3, so it may be different on Automatic Transmissions.

We also read the joystick on the Sonata device and do the following:
| Joystick Event | Action of Vehicle                      |
| -------------- | -----------------                      |
| Left           | Flash Left Indicator/Blinker on Dash   |
| Right          | Flash Right Indicator/Blinker on Dash  |
| Press          | Flash Both Indicators/Blinkers on Dash |
Note: The indicators/blinkers do not flash outside the vehicle - only on the dash. However, on some vehciles, you may see the daylight running lamps flicker.

### CAN Events Detected
* Steering Wheel Signals
  * Mute
  * Volume Up (push button volume controls found on VW)
  * Volume Down (push button volume controls found on VW)
  * Scrolls Volume + (volume scroll wheel as found on Audi)
  * Scrolls Volume - (volume scroll wheel as found on Audi)
  * Previous (Previous track/station)
  * Next (Next track/station)
  * Voice
  * View
  * Star
  * Up (left of steering on VW)
  * Down (left of steering on VW)
  * Scroll Up (scroll on left of steering on Audi)
  * Scroll Down (scroll on left of steering on Audi)
  * Right (page right on left of steering)
  * Left (page left on left of steering)
  * Phone
  * OK
  * Nav (not on all vehicles)
  * Back
* Driving Signals
  * DriveOn! (The engine has been started)
  * Drive3 (The engine has been started)
  * DriveOff (The electronics are off - normally occurs after the engine has been switched off and the driver's door opened).

## How Can We Try This?
You need a vehicle that uses the Volkswagen Audi Group (VAG) MQB or MLB platform.  If you have one of these vehicles, check that the steering featiures some of the listed buttons for it to work.

You need to find the Comfort CAN. A good place to look would be in one of the large vertical wiring looms found in the footwell on boith sides of the vehicle. This loom will have a branch that goes into the doors, so you may be able to find the CAN wires in the doors.
| CAN Signal | Wire Colour  |  Alternative Wire Colour |
| ---------- | ------------ | ------------------------ |
| CAN High   | Orange/Green | Plain Green              |
| CAN Low    | Orange/Brown | Plain Orange             |
Note: Orange/Green means an Orange wire with a Green tracer (stripe).

## Vehicles Supported
This is tested on a 2020 Audi A3 with a Manual Transmission but below is a list of other vehicles that use the same platform. The VAG MQB and MLB platforms are also used by other manaufacturers such as Skoda and Porsche but the further you get from VW and Audi and more they alter the codes so they may not work fully.
Vehicles that this should work on include:
### Audi
* A1/S1 2018 onwards
* A3/S3/RS3 2013 onwards
* A4/S4/RS4 2016 onwards
* A5/S5/RS5 2016 onwards
* A6/S6/RS6 2018 onwards

### Volkswagen
* Caddy 2019 
* Crafter 2017 onwards
* Golf 2014 to 2024
* Polo 2017 onwards
* Tiguan 2016 to 2024
* Transporter (T6.1) 2023 to 2023
* T-Roc 2017 onwards


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
We're going to run each CAN on a separate SPI to reduce the risk of accidental cross-talk.
|  RPi Name | I/O | Header # | RPi GPIO | Pinmux Name                          | Pinmux Option  | Waveshare Name | Notes |
| --------- | --- | -------- | -------- | ------------------------------------ | -------------- | -------------- | --- |
| SPI0_CE_0 |  O  |    24    |     8    | SonataPinmux::OutputPin::rph_g8      | 1 (spi_1_cs_0) | CS_0           |     |
| SPI0_COPI |  O  |    19    |    10    | SonataPinmux::OutputPin::rph_g10     | 1 (spi_1_copi) | MOSI_0         |     |
| SPI0_CIPO |  I  |    21    |     9    | SonataPinmux::BlockInput::spi_1_cipo | 1 (rph_g9)     | MISO_0         |     |
| SPI0_SCLK |  O  |    23    |    11    | SonataPinmux::OutputPin::rph_g11     | 1 (spi_1_sclk) | SCK_0          |     |
|           |  I  |    22    |    25    |                                      |                | INT_0          |     |
