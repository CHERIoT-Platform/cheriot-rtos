// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstdint>
#include <cstring>
#include <debug.hh>
#include <thread.h>
#include <platform/sunburst/platform-gpio.hh>
#include <platform/sunburst/platform-pinmux.hh>
#include <platform/sunburst/platform-spi.hh>
#include "driver/MCP251XFD/ErrorsDef.h"
#include "driver/interface.hh"
#include "queue.h"
#include "timeout.hh"
#include "uart2.hh"

#define TIMESTAMP_TICK_us ( 25 ) // Tickstamp tick is 25us
#define TIMESTAMP_TICK(sysclk) ( ((sysclk) / 1000000) * TIMESTAMP_TICK_us )

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Main compartment">;

typedef enum CanEventType {
	CAN_EVENT_BUTTON_ON_OFF,
	CAN_EVENT_DRIVE_ON_ONLY,
	CAN_EVENT_DRIVE_OFF_ONLY
} CanEventType;
// A structure to read CAN events. This example is CAN2.0 only so no CAN FD is needed so the mask, on and off signals can all be the maximum of MCP251XFD::MCP251XFD_DLC_8BYTE.
typedef struct CanEvent {
	const char* name;
	uint32_t messageId; //!< Contain the message ID
	MCP251XFD::eMCP251XFD_DataLength dlc;	//!< Indicate the minimum bytes of payload data we can accept
	uint8_t mask[MCP251XFD::MCP251XFD_DLC_8BYTE];
	uint8_t on[MCP251XFD::MCP251XFD_DLC_8BYTE];
	uint8_t off[MCP251XFD::MCP251XFD_DLC_8BYTE];
	CanEventType typ;
	bool active;
} CanEvent;

#define EVENTS_LENGTH	(23)
CanEvent events[EVENTS_LENGTH] = {
	{
		.name = "Mute",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Volume Up",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Volume Down",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Scroll Volume +",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x12, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Scroll Volume -",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x12, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Previous",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Next",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Voice",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "View",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x23, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Star",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Scroll Up",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Scroll Down",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x06, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Right",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Phone",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "OK",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Nav",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Down",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Up",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Back",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Left",
		.messageId = 0x05BF,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_BUTTON_ON_OFF,
		.active = false
	},
	{
		.name = "Drive3",
		.messageId = 0x03BE,
		.dlc = MCP251XFD::MCP251XFD_DLC_8BYTE,
		.mask = {0x00, 0x00, 0x04, 0x00, 0xFF, 0x00, 0x00, 0x00},
		.on   = {0x00, 0x00, 0x04, 0x00, 0xC0, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_DRIVE_ON_ONLY,
		.active = false
	},
	{
		.name = "DriveOn!",
		.messageId = 0x03BE,
		.dlc = MCP251XFD::MCP251XFD_DLC_8BYTE,
		.mask = {0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_DRIVE_ON_ONLY,
		.active = false
	},
	{
		.name = "DriveOff",
		.messageId = 0x03C0,
		.dlc = MCP251XFD::MCP251XFD_DLC_4BYTE,
		.mask = {0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00},
		.on   = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.off  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.typ = CAN_EVENT_DRIVE_OFF_ONLY,
		.active = false
	}
};

uint32_t can1SclkResult = 0;
Spi_Config cfgCan1;
MCP251XFD::MCP251XFD_BitTimeStats can0Bittimestats;
uint32_t sysclkExt1;
MCP251XFD::MCP251XFD can1 = {
	.UserDriverData = NULL,	//!< Optional, can be used to store driver data or NULL
	.DriverConfig = MCP251XFD::MCP251XFD_DRIVER_NORMAL_USE,    //!< Driver configuration, by default it is MCP251XFD_DRIVER_NORMAL_USE. Configuration can be OR'ed
	.GPIOsOutLevel = 0,              //!< GPIOs pins output state (0 = set to '0' ; 1 = set to '1'). Used to speed up output change
	.SPI_ChipSelect = 0,				//!< This is the Chip Select index that will be set at the call of a transfer
	.InterfaceDevice = &cfgCan1,	//!< This is the pointer that will be in the first parameter of all interface call functions
	.SPIClockSpeed = 500000,				//!< SPI nominal clock speed (max is SYSCLK div by 2)
	.fnSPI_Init = MCP251XFD_InterfaceInit_Sonata,	//!< This function will be called at driver initialization to configure the interface driver
	.fnSPI_Transfer = MCP251XFD_InterfaceTransfer_Sonata,	//!< This function will be called at driver read/write data from/to the interface driver SPI
	.fnGetCurrentms = GetCurrentms_Sonata,	//!< This function will be called when the driver need to get current millisecond
	.fnComputeCRC16 = ComputeCRC16_Sonata	//!< This function will be called when a CRC16-CMS computation is needed (ie. in CRC mode or Safe Write). In normal mode, this can point to NULL
};

MCP251XFD::MCP251XFD_Config can1Conf =
{
	//--- Controller clocks ---
	.XtalFreq = 40000000UL, // 40Mhz                 //!< Component CLKIN Xtal/Resonator frequency (min 4MHz, max 40MHz). Set it to 0 if oscillator is used
	.OscFreq = 0,                               		//!< Component CLKIN oscillator frequency (min 2MHz, max 40MHz). Set it to 0 if Xtal/Resonator is used
	.SysclkConfig = MCP251XFD::MCP251XFD_SYSCLK_IS_CLKIN,       //!< Factor of frequency for the SYSCLK. SYSCLK = CLKIN x SysclkConfig where CLKIN is XtalFreq or OscFreq
	.ClkoPinConfig = MCP251XFD::MCP251XFD_CLKO_DivBy10,         //!< Configure the CLKO pin (SCLK div by 1, 2, 4, 10 or Start Of Frame)
	.SYSCLK_Result = &can1SclkResult,              //!< This is the SYSCLK of the component after configuration (can be NULL if the internal SYSCLK of the component do not have to be known)

	//--- CAN configuration ---
	.NominalBitrate = 500000, 	// 500 Kbit         //!< Speed of the Frame description and arbitration
	.DataBitrate = MCP251XFD_NO_CANFD,                         //!< Speed of all the data bytes of the frame (if CAN2.0 only mode, set to value MCP251XFD_NO_CANFD)
	.BitTimeStats = &can0Bittimestats,           	//!< Point to a Bit Time stat structure (set to NULL if no statistics are necessary)
	.Bandwidth = MCP251XFD::MCP251XFD_NO_DELAY,                 //!< Transmit Bandwidth Sharing, this is the delay between two consecutive transmissions (in arbitration bit times)
	//!< Set of CAN control flags to configure the CAN controller. Configuration can be OR'ed
	.ControlFlags = MCP251XFD::MCP251XFD_CAN_LISTEN_ONLY_MODE_ON_ERROR,

	//--- GPIOs and Interrupts pins ---
	.GPIO0PinMode = MCP251XFD::MCP251XFD_PIN_AS_GPIO0_IN,      //!< Startup INT0/GPIO0/XSTBY pins mode (INT0 => Interrupt for TX)
	.GPIO1PinMode = MCP251XFD::MCP251XFD_PIN_AS_GPIO1_IN,      //!< Startup INT1/GPIO1 pins mode (INT1 => Interrupt for RX)
	.INTsOutMode = MCP251XFD::MCP251XFD_PINS_PUSHPULL_OUT,      //!< Define the output type of all interrupt pins (INT, INT0 and INT1)
	.TXCANOutMode = MCP251XFD::MCP251XFD_PINS_PUSHPULL_OUT,     //!< Define the output type of the TXCAN pin

	//--- Interrupts ---
	//!< Set of system interrupt flags to enable. Configuration can be OR'ed
	.SysInterruptFlags = MCP251XFD::MCP251XFD_INT_NO_EVENT
};

#define MCP251XFD_EXT1_FIFO_COUNT	10

MCP251XFD::MCP251XFD_RAMInfos ext1TefRamInfos;
MCP251XFD::MCP251XFD_RAMInfos ext1TxqRamInfos;
MCP251XFD::MCP251XFD_RAMInfos ext1FifosRamInfos[MCP251XFD_EXT1_FIFO_COUNT - 2];
MCP251XFD::MCP251XFD_FIFO mcP251XfdExt1FifOlist[MCP251XFD_EXT1_FIFO_COUNT] =
{
	{ .Name = MCP251XFD::MCP251XFD_TEF, .Size = MCP251XFD::MCP251XFD_FIFO_10_MESSAGE_DEEP, .ControlFlags = MCP251XFD::MCP251XFD_FIFO_ADD_TIMESTAMP_ON_OBJ,
	.InterruptFlags = static_cast<MCP251XFD::eMCP251XFD_FIFOIntFlags>(MCP251XFD::MCP251XFD_FIFO_OVERFLOW_INT + MCP251XFD::MCP251XFD_FIFO_EVENT_FIFO_NOT_EMPTY_INT),
	.RAMInfos = &ext1TefRamInfos, },
	{ .Name = MCP251XFD::MCP251XFD_TXQ, .Size = MCP251XFD::MCP251XFD_FIFO_4_MESSAGE_DEEP, .Payload = MCP251XFD::MCP251XFD_PAYLOAD_64BYTE,
	.Attempts = MCP251XFD::MCP251XFD_THREE_ATTEMPTS, .Priority = MCP251XFD::MCP251XFD_MESSAGE_TX_PRIORITY16,
	.ControlFlags = MCP251XFD::MCP251XFD_FIFO_NO_RTR_RESPONSE,
	.InterruptFlags = static_cast<MCP251XFD::eMCP251XFD_FIFOIntFlags>(MCP251XFD::MCP251XFD_FIFO_TX_ATTEMPTS_EXHAUSTED_INT + MCP251XFD::MCP251XFD_FIFO_TRANSMIT_FIFO_NOT_FULL_INT),
	.RAMInfos = &ext1TxqRamInfos, },
	{ .Name = MCP251XFD::MCP251XFD_FIFO1, .Size = MCP251XFD::MCP251XFD_FIFO_4_MESSAGE_DEEP, .Payload = MCP251XFD::MCP251XFD_PAYLOAD_64BYTE,
	.Direction = MCP251XFD::MCP251XFD_RECEIVE_FIFO, .ControlFlags = MCP251XFD::MCP251XFD_FIFO_ADD_TIMESTAMP_ON_RX,
	.InterruptFlags = static_cast<MCP251XFD::eMCP251XFD_FIFOIntFlags>(MCP251XFD::MCP251XFD_FIFO_OVERFLOW_INT + MCP251XFD::MCP251XFD_FIFO_RECEIVE_FIFO_NOT_EMPTY_INT),
	.RAMInfos = &ext1FifosRamInfos[0], }, // SID: 0x000..0x1FF ; No EID
	{ .Name = MCP251XFD::MCP251XFD_FIFO2, .Size = MCP251XFD::MCP251XFD_FIFO_2_MESSAGE_DEEP, .Payload = MCP251XFD::MCP251XFD_PAYLOAD_64BYTE,
	.Direction = MCP251XFD::MCP251XFD_RECEIVE_FIFO, .ControlFlags = MCP251XFD::MCP251XFD_FIFO_ADD_TIMESTAMP_ON_RX,
	.InterruptFlags = static_cast<MCP251XFD::eMCP251XFD_FIFOIntFlags>(MCP251XFD::MCP251XFD_FIFO_OVERFLOW_INT + MCP251XFD::MCP251XFD_FIFO_RECEIVE_FIFO_NOT_EMPTY_INT),
	.RAMInfos = &ext1FifosRamInfos[1], }, // SID: 0x200..0x3FF ; No EID
	{ .Name = MCP251XFD::MCP251XFD_FIFO3, .Size = MCP251XFD::MCP251XFD_FIFO_4_MESSAGE_DEEP, .Payload = MCP251XFD::MCP251XFD_PAYLOAD_64BYTE,
	.Direction = MCP251XFD::MCP251XFD_RECEIVE_FIFO, .ControlFlags = MCP251XFD::MCP251XFD_FIFO_ADD_TIMESTAMP_ON_RX,
	.InterruptFlags = static_cast<MCP251XFD::eMCP251XFD_FIFOIntFlags>(MCP251XFD::MCP251XFD_FIFO_OVERFLOW_INT + MCP251XFD::MCP251XFD_FIFO_RECEIVE_FIFO_NOT_EMPTY_INT),
	.RAMInfos = &ext1FifosRamInfos[2], }, // SID: 0x400..0x5FF ; No EID
	{ .Name = MCP251XFD::MCP251XFD_FIFO4, .Size = MCP251XFD::MCP251XFD_FIFO_2_MESSAGE_DEEP, .Payload = MCP251XFD::MCP251XFD_PAYLOAD_64BYTE,
	.Direction = MCP251XFD::MCP251XFD_RECEIVE_FIFO, .ControlFlags = MCP251XFD::MCP251XFD_FIFO_ADD_TIMESTAMP_ON_RX,
	.InterruptFlags = static_cast<MCP251XFD::eMCP251XFD_FIFOIntFlags>(MCP251XFD::MCP251XFD_FIFO_OVERFLOW_INT + MCP251XFD::MCP251XFD_FIFO_RECEIVE_FIFO_NOT_EMPTY_INT),
	.RAMInfos = &ext1FifosRamInfos[3], }, // SID: 0x600..0x7FF ; No EID
	{ .Name = MCP251XFD::MCP251XFD_FIFO5, .Size = MCP251XFD::MCP251XFD_FIFO_4_MESSAGE_DEEP, .Payload = MCP251XFD::MCP251XFD_PAYLOAD_64BYTE,
	.Direction = MCP251XFD::MCP251XFD_TRANSMIT_FIFO, .Attempts = MCP251XFD::MCP251XFD_THREE_ATTEMPTS,
	.Priority = MCP251XFD::MCP251XFD_MESSAGE_TX_PRIORITY16, .ControlFlags = MCP251XFD::MCP251XFD_FIFO_NO_RTR_RESPONSE,
	.InterruptFlags = static_cast<MCP251XFD::eMCP251XFD_FIFOIntFlags>(MCP251XFD::MCP251XFD_FIFO_TX_ATTEMPTS_EXHAUSTED_INT + MCP251XFD::MCP251XFD_FIFO_TRANSMIT_FIFO_NOT_FULL_INT),
	.RAMInfos = &ext1FifosRamInfos[4], },
	{ .Name = MCP251XFD::MCP251XFD_FIFO6, .Size = MCP251XFD::MCP251XFD_FIFO_2_MESSAGE_DEEP, .Payload = MCP251XFD::MCP251XFD_PAYLOAD_64BYTE,
	.Direction = MCP251XFD::MCP251XFD_TRANSMIT_FIFO, .Attempts = MCP251XFD::MCP251XFD_THREE_ATTEMPTS,
	.Priority = MCP251XFD::MCP251XFD_MESSAGE_TX_PRIORITY16, .ControlFlags = MCP251XFD::MCP251XFD_FIFO_NO_RTR_RESPONSE,
	.InterruptFlags = static_cast<MCP251XFD::eMCP251XFD_FIFOIntFlags>(MCP251XFD::MCP251XFD_FIFO_TX_ATTEMPTS_EXHAUSTED_INT + MCP251XFD::MCP251XFD_FIFO_TRANSMIT_FIFO_NOT_FULL_INT),
	.RAMInfos = &ext1FifosRamInfos[5], },
	{ .Name = MCP251XFD::MCP251XFD_FIFO7, .Size = MCP251XFD::MCP251XFD_FIFO_2_MESSAGE_DEEP, .Payload = MCP251XFD::MCP251XFD_PAYLOAD_64BYTE,
	.Direction = MCP251XFD::MCP251XFD_TRANSMIT_FIFO, .Attempts = MCP251XFD::MCP251XFD_THREE_ATTEMPTS,
	.Priority = MCP251XFD::MCP251XFD_MESSAGE_TX_PRIORITY16, .ControlFlags = MCP251XFD::MCP251XFD_FIFO_NO_RTR_RESPONSE,
	.InterruptFlags = static_cast<MCP251XFD::eMCP251XFD_FIFOIntFlags>(MCP251XFD::MCP251XFD_FIFO_TX_ATTEMPTS_EXHAUSTED_INT + MCP251XFD::MCP251XFD_FIFO_TRANSMIT_FIFO_NOT_FULL_INT),
	.RAMInfos = &ext1FifosRamInfos[6], },
	{ .Name = MCP251XFD::MCP251XFD_FIFO8, .Size = MCP251XFD::MCP251XFD_FIFO_2_MESSAGE_DEEP, .Payload = MCP251XFD::MCP251XFD_PAYLOAD_64BYTE,
	.Direction = MCP251XFD::MCP251XFD_TRANSMIT_FIFO, .Attempts = MCP251XFD::MCP251XFD_THREE_ATTEMPTS,
	.Priority = MCP251XFD::MCP251XFD_MESSAGE_TX_PRIORITY16, .ControlFlags = MCP251XFD::MCP251XFD_FIFO_NO_RTR_RESPONSE,
	.InterruptFlags = static_cast<MCP251XFD::eMCP251XFD_FIFOIntFlags>(MCP251XFD::MCP251XFD_FIFO_TX_ATTEMPTS_EXHAUSTED_INT + MCP251XFD::MCP251XFD_FIFO_TRANSMIT_FIFO_NOT_FULL_INT),
	.RAMInfos = &ext1FifosRamInfos[7], },
};

#define MCP251XFD_EXT1_FILTER_COUNT 4
MCP251XFD::MCP251XFD_Filter mcp251xfdExt1FilterList[MCP251XFD_EXT1_FILTER_COUNT] =
{
	{ .Filter = MCP251XFD::MCP251XFD_FILTER0, .EnableFilter = true, .Match = MCP251XFD::MCP251XFD_MATCH_ONLY_SID, .PointTo = MCP251XFD::MCP251XFD_FIFO1, .AcceptanceID = 0x000, .AcceptanceMask = 0x600, }, // 0x000..0x1FF
	{ .Filter = MCP251XFD::MCP251XFD_FILTER1, .EnableFilter = true, .Match = MCP251XFD::MCP251XFD_MATCH_ONLY_SID, .PointTo = MCP251XFD::MCP251XFD_FIFO2, .AcceptanceID = 0x200, .AcceptanceMask = 0x600 }, // 0x200..0x3FF
	{ .Filter = MCP251XFD::MCP251XFD_FILTER2, .EnableFilter = true, .Match = MCP251XFD::MCP251XFD_MATCH_ONLY_SID, .PointTo = MCP251XFD::MCP251XFD_FIFO3, .AcceptanceID = 0x400, .AcceptanceMask = 0x600 }, // 0x400..0x5FF
	{ .Filter = MCP251XFD::MCP251XFD_FILTER3, .EnableFilter = true, .Match = MCP251XFD::MCP251XFD_MATCH_SID_EID, .PointTo = MCP251XFD::MCP251XFD_FIFO4, .AcceptanceID = MCP251XFD_ACCEPT_ALL_MESSAGES, .AcceptanceMask = MCP251XFD_ACCEPT_ALL_MESSAGES }, // Everything else
};

eERRORRESULT configure_mcp251xfd_on_can1() {
	eERRORRESULT result = ERR__NO_DEVICE_DETECTED;

	// Configure the CAN device
	Debug::log("GetSpiConfig()");
	result = GetSpiConfig(&cfgCan1, 1, SonataPinmux::PinSink::rph_g8, SonataPinmux::PinSink::ser0_tx, SonataPinmux::PinSink::ser0_tx, SonataPinmux::PinSink::rph_g11, SonataPinmux::PinSink::rph_g10, 1);
	if(result != ERR_OK) {
		Debug::log("ERROR! GetSpiConfig() failed with {}.", result);
	}

	Debug::log("Init_MCP251XFD()");
	thread_millisecond_wait(300);	// A short delay for clock to stablise (only need to be 3ms so 300ms should be more than enough)
	result = Init_MCP251XFD(&can1, &can1Conf);
	if(result != ERR_OK) {
		Debug::log("ERROR! Init_MCP251XFD() failed with {}.", result);
	}
	Debug::log("Init_MCP251XFD() done.");

	Debug::log("MCP251XFD_ConfigureTimeStamp()");
	Debug::log("can1SclkResult = {}", can1SclkResult);
	Debug::log("TIMESTAMP_TICK(can1SclkResult) = {}", TIMESTAMP_TICK(can1SclkResult));
	result = MCP251XFD_ConfigureTimeStamp(&can1, true, MCP251XFD::MCP251XFD_TS_CAN20_SOF_CANFD_SOF, TIMESTAMP_TICK(can1SclkResult), false);
	if(result != ERR_OK) {
		Debug::log("ERROR! MCP251XFD_ConfigureTimeStamp() failed with {}.", result);
	}
	Debug::log("MCP251XFD_ConfigureTimeStamp() done.");

	Debug::log("MCP251XFD_ConfigureFIFOList()");
	result = MCP251XFD_ConfigureFIFOList(&can1, mcP251XfdExt1FifOlist, MCP251XFD_EXT1_FIFO_COUNT);
	if(result != ERR_OK) {
		Debug::log("ERROR! MCP251XFD_ConfigureFIFOList() failed with {}.", result);
	}
	Debug::log("MCP251XFD_ConfigureFIFOList() done.");

	Debug::log("MCP251XFD_ConfigureFilterList()");
	result = MCP251XFD_ConfigureFilterList(&can1, MCP251XFD::MCP251XFD_D_NET_FILTER_DISABLE, &mcp251xfdExt1FilterList[0], MCP251XFD_EXT1_FILTER_COUNT);
	if(result != ERR_OK) {
		Debug::log("ERROR! MCP251XFD_ConfigureFilterList() failed with {}.", result);
	}
	Debug::log("MCP251XFD_ConfigureFilterList() done.");

	Debug::log("MCP251XFD_StartCAN20()");
	result = MCP251XFD_StartCAN20(&can1);
	if(result != ERR_OK) {
		Debug::log("ERROR! MCP251XFD_StartCAN20() failed with {}.", result);
	}
	Debug::log("MCP251XFD_StartCAN20() done.");

	// Debug::log("MCP251XFD_StartCANFD()");
	// result = MCP251XFD_StartCANFD(&can1);
	// if(result != ERR_OK) {
	// 	Debug::log("ERROR! MCP251XFD_StartCANFD() failed with {}.", result);
	// }
	// Debug::log("MCP251XFD_StartCANFD() done.");

	return result;
}

static uint32_t txMessageSeq = 0;

CHERI_SEALED(MessageQueue *) queue;

bool are_we_driving() {
	for(auto & __capability event : events) {
		if((event.typ == CAN_EVENT_DRIVE_ON_ONLY) && (event.active == true)) {
			return true;
		}
	}
	return false;
}

void set_not_driving() {
	for(auto & __capability event : events) {
		if(event.typ == CAN_EVENT_DRIVE_ON_ONLY) {
			event.active = false;
		}
	}
}

void notify_changes(uint32_t t, uint32_t v) {
	QueueMessage message = {t,v};
	int ret = blocking_forever<queue_send_sealed>(queue, &message);
	// Abort if the queue send errors.
	if(ret != 0) {
		Debug::log("Queue send failed {}", ret);
	}
}

void react_to_button(uint32_t v) {
	uint32_t t = 0;
	if(are_we_driving()) {
		t = 1;
	}
	notify_changes(t, v);
}

void interpret_can_message(MCP251XFD::MCP251XFD_CANMessage *mess) {
	// Go through each event and look for a match
	uint32_t id = 0;
	for(auto & __capability event : events) {
		// Check ID and DLC
		// Debug::log("event.messageId = {}, mess->MessageID == {}", event.messageId, mess->MessageID);
		if((event.messageId == mess->MessageID) && (event.dlc <= mess->DLC)) {
			// Debug::log("{}({})", __FUNCTION__, __LINE__);
			// Are we ON?
			if((event.typ == CAN_EVENT_BUTTON_ON_OFF) || (event.typ == CAN_EVENT_DRIVE_ON_ONLY)) {
				// Check if the signal is on.
				bool test = true;
				// Debug::log("Mask: {},{},{},{},{},{},{},{}", event.mask[0], event.mask[1], event.mask[2], event.mask[3], event.mask[4], event.mask[5], event.mask[6], event.mask[7]);
				// Debug::log("Data: {},{},{},{},{},{},{},{}", mess->PayloadData[0], mess->PayloadData[1], mess->PayloadData[2], mess->PayloadData[3], mess->PayloadData[4], mess->PayloadData[5], mess->PayloadData[6], mess->PayloadData[7]);
				// Debug::log("  On: {},{},{},{},{},{},{},{}", event.on[0], event.on[1], event.on[2], event.on[3], event.on[4], event.on[5], event.on[6], event.on[7]);
				for(uint8_t j = 0; j < event.dlc; j++) {
					if((event.mask[j] & mess->PayloadData[j]) != event.on[j]) {
						test = false;
						break;
					}
				}
				if((test == true) && (event.active == false)) {
					event.active = true;
					Debug::log("Event: {} ON", event.name);
					react_to_button(id);
				}
			}
			// Are we OFF?
			if((event.typ == CAN_EVENT_BUTTON_ON_OFF) || (event.typ == CAN_EVENT_DRIVE_OFF_ONLY)) {
				// Check if the signal is on.
				bool test = true;
				for(uint8_t j = 0; j < event.dlc; j++) {
					if((event.mask[j] & mess->PayloadData[j]) != event.off[j]) {
						test = false;
						break;
					}
				}
				if(event.typ == CAN_EVENT_DRIVE_OFF_ONLY) {
					if(test == true) {
						event.active = false;
						if(are_we_driving()) {
							Debug::log("Event: {} OFF", event.name);
							set_not_driving();	// Ensure that we are not driving.
							react_to_button(id);
						}
					}
				} else {
					if((test == true) && (event.active == true)) {
						event.active = false;
						Debug::log("Event: {} OFF", event.name);
						react_to_button(id);
					}
				}
			} 
		}
		id++;
	}
}

uint8_t mess366[MCP251XFD::MCP251XFD_DLC_8BYTE];

// Transmit a message to MCP251XFD device on EXT1
//=============================================================================
eERRORRESULT transmit_indication(SonataGpioBoard::JoystickDirection direction)
{
	eERRORRESULT ret = ERR_OK;
	if(direction == SonataGpioBoard::JoystickDirection::Left) {
		mess366[3] = 0x08;
	} else if(direction == SonataGpioBoard::JoystickDirection::Right) {
		mess366[3] = 0x10;
	} else if(direction == SonataGpioBoard::JoystickDirection::Pressed) {
		mess366[3] = 0x18;
	}
	MCP251XFD::eMCP251XFD_FIFOstatus fifoStatus = MCP251XFD::MCP251XFD_TX_FIFO_FULL;
	ret = MCP251XFD_GetFIFOStatus(&can1, MCP251XFD::MCP251XFD_TXQ, &fifoStatus); // First get FIFO2 status
	if (ret != ERR_OK) { 
		return ret;
	}
	if ((fifoStatus & MCP251XFD::MCP251XFD_TX_FIFO_NOT_FULL) > 0) // Second check FIFO not full
	{
		MCP251XFD::MCP251XFD_CANMessage tansmitMessage;
		//***** Fill the message as you want *****
		tansmitMessage.MessageID = 0x366;
		tansmitMessage.MessageSEQ = txMessageSeq;
		tansmitMessage.ControlFlags = MCP251XFD::MCP251XFD_STANDARD_MESSAGE_ID;
		tansmitMessage.DLC = MCP251XFD::MCP251XFD_DLC_8BYTE;
		tansmitMessage.PayloadData = &mess366[0];
		// Send message and flush
		ret = MCP251XFD_TransmitMessageToFIFO(&can1, &tansmitMessage, MCP251XFD::MCP251XFD_TXQ, true);
		txMessageSeq++;
	}
	return ret;
}


//=============================================================================
// Receive a message from MCP251XFD device on EXT1
//=============================================================================
eERRORRESULT receive_message_from_ext1_fifo(MCP251XFD::eMCP251XFD_FIFO fifo)
{
	eERRORRESULT ret = ERR_OK;
	MCP251XFD::eMCP251XFD_FIFOstatus fifoStatus = MCP251XFD::MCP251XFD_RX_FIFO_EMPTY;
	ret = MCP251XFD_GetFIFOStatus(&can1, fifo, &fifoStatus); // First get FIFO1 status
	if (ret != ERR_OK) { 
		Debug::log("ERROR! MCP251XFD_GetFIFOStatus() return {}", ret);
		return ret;
	}
	if ((fifoStatus & MCP251XFD::MCP251XFD_RX_FIFO_NOT_EMPTY) > 0) // Second check FIFO not empty
	{
		uint32_t messageTimeStamp = 0;
		uint8_t rxPayloadData[64]; // In this example, the FIFO1 have 64 bytes of payload
		MCP251XFD::MCP251XFD_CANMessage receivedMessage;
		receivedMessage.PayloadData = &rxPayloadData[0]; // Add receive payload data pointer to the message structure
		// that will be received
		ret = MCP251XFD_ReceiveMessageFromFIFO(&can1, &receivedMessage, MCP251XFD::MCP251XFD_PAYLOAD_64BYTE,
		&messageTimeStamp, fifo);
		if (ret == ERR_OK)
		{
			// Debug::log("Rx: ID: {}, FIFO: {}, dlc: {}, flg: {}, data: {} {} {} {} {} {} {} {} ", receivedMessage.MessageID, static_cast<int16_t>(fifo), receivedMessage.DLC, receivedMessage.ControlFlags, receivedMessage.PayloadData[0], receivedMessage.PayloadData[1], receivedMessage.PayloadData[2], receivedMessage.PayloadData[3], receivedMessage.PayloadData[4], receivedMessage.PayloadData[5], receivedMessage.PayloadData[6], receivedMessage.PayloadData[7]);
			if((receivedMessage.MessageID == 366) && (receivedMessage.DLC == MCP251XFD::MCP251XFD_DLC_8BYTE)) {
				// We keep a copy of this to use to sent the indicators message.
				memcpy(mess366, receivedMessage.PayloadData, MCP251XFD::MCP251XFD_DLC_8BYTE);
			} else {
				interpret_can_message(&receivedMessage);
			}
		}
	}
	return ret;
}

#define INDICATION_MAX_PERIOD	1000	// in ms the fastest that we can send a message.

/// Thread entry point.
void __cheri_compartment("main_comp") main_entry()
{
	eERRORRESULT result = ERR__NO_DEVICE_DETECTED;

	auto gpioBoard = MMIO_CAPABILITY(SonataGpioBoard, gpio_board);

	// Print welcome, along with the compartment's name to the default UART.
	Debug::log("Sonata VW Audi CAN Example");

	thread_millisecond_wait(3000);	// Wait a minute for the modem to finish it's set up routine (which doesn't take that long but I'm being cautious)
	Debug::log("Pass the queue to the modem driver.");
	// Allocate the queue for the UART
	int res = non_blocking<queue_create_sealed>(MALLOC_CAPABILITY, &queue, sizeof(QueueMessage), 16);
	if(res != 0) {
		Debug::log("ERROR! queue_create_sealed() failed! {}", res);
	}
	// Pass the queue handle to the consumer.
	res = set_queue(queue);	// The UART thread won't continue until this has been called.
	if(res != 0) {
		Debug::log("ERROR! set_queue() failed!");
	}
	Debug::log("Done.");
	thread_millisecond_wait(3000);	// Wait a minute for the modem to finish it's set up routine (which doesn't take that long but I'm being cautious)

	Debug::log("Configure MCP251XFD on CAN1");
	result = configure_mcp251xfd_on_can1();
	if(result != ERR_OK) {
		Debug::log("ERROR! ConfigureMCP251XFDonCAN1() failed with {}.", result);
	}

	Debug::log("Starting main loop.");
	uint8_t txCnt = 0;
	volatile uint32_t time;
	volatile uint32_t nextIndication = 0;
	while(true) {
		time = GetCurrentms_Sonata();
		// receive_message_from_ext1_fifo(MCP251XFD::MCP251XFD_FIFO1);
		receive_message_from_ext1_fifo(MCP251XFD::MCP251XFD_FIFO2);
		receive_message_from_ext1_fifo(MCP251XFD::MCP251XFD_FIFO3);
		// receive_message_from_ext1_fifo(MCP251XFD::MCP251XFD_FIFO4);
		
		auto jsv = gpioBoard->read_joystick();
		if(jsv.is_left() && (time >= nextIndication)) {
			Debug::log("Indicate Left");
			transmit_indication(SonataGpioBoard::JoystickDirection::Left);
			nextIndication = time + INDICATION_MAX_PERIOD;
		} else if(jsv.is_right() && (time >= nextIndication)) {
			Debug::log("Indicate Right");
			transmit_indication(SonataGpioBoard::JoystickDirection::Right);
			nextIndication = time + INDICATION_MAX_PERIOD;
		} else if(jsv.is_pressed() && (time >= nextIndication)) {
			Debug::log("Indicate Left & Right");
			transmit_indication(SonataGpioBoard::JoystickDirection::Pressed);
			nextIndication = time + INDICATION_MAX_PERIOD;
		}
	}
}
