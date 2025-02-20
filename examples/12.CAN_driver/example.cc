// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <debug.hh>
#include <thread.h>
#include <platform-gpio.hh>
#include <platform-pinmux.hh>
#include <platform-spi.hh>
#include "driver/ErrorsDef.h"
#include "driver/Conf_MCP251XFD.h"
#include "driver/MCP251XFD.hh"
#include "driver/interface.hh"

#define TIMESTAMP_TICK_us ( MS_PER_TICK * 1000L ) // Tickrate in us (MS_PER_TICK * 1000)
#define TIMESTAMP_TICK(sysclk) ( ((sysclk) / 1000000) * TIMESTAMP_TICK_us )

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Main compartment">;

uint32_t can1_sclk_result = 0;
Spi_Config cfg_can1;
MCP251XFD_BitTimeStats can0_bittimestats;
uint32_t SYSCLK_Ext1;
MCP251XFD can1 = {
	.UserDriverData = NULL,	//!< Optional, can be used to store driver data or NULL
	.DriverConfig = MCP251XFD_DRIVER_NORMAL_USE,    //!< Driver configuration, by default it is MCP251XFD_DRIVER_NORMAL_USE. Configuration can be OR'ed
	.GPIOsOutLevel = 0,              //!< GPIOs pins output state (0 = set to '0' ; 1 = set to '1'). Used to speed up output change
	.SPI_ChipSelect = 0,				//!< This is the Chip Select index that will be set at the call of a transfer
	.InterfaceDevice = &cfg_can1,	//!< This is the pointer that will be in the first parameter of all interface call functions
	.SPIClockSpeed = 500000,				//!< SPI nominal clock speed (max is SYSCLK div by 2)
	.fnSPI_Init = MCP251XFD_InterfaceInit_Sonata,	//!< This function will be called at driver initialization to configure the interface driver
	.fnSPI_Transfer = MCP251XFD_InterfaceTransfer_Sonata,	//!< This function will be called at driver read/write data from/to the interface driver SPI
	.fnGetCurrentms = GetCurrentms_Sonata,	//!< This function will be called when the driver need to get current millisecond
	.fnComputeCRC16 = ComputeCRC16_Sonata	//!< This function will be called when a CRC16-CMS computation is needed (ie. in CRC mode or Safe Write). In normal mode, this can point to NULL
};

MCP251XFD_Config can_conf1 =
{
	//--- Controller clocks ---
	.XtalFreq = 40000000UL, // 40Mhz                 //!< Component CLKIN Xtal/Resonator frequency (min 4MHz, max 40MHz). Set it to 0 if oscillator is used
	.OscFreq = 0,                               		//!< Component CLKIN oscillator frequency (min 2MHz, max 40MHz). Set it to 0 if Xtal/Resonator is used
	.SysclkConfig = MCP251XFD_SYSCLK_IS_CLKIN,       //!< Factor of frequency for the SYSCLK. SYSCLK = CLKIN x SysclkConfig where CLKIN is XtalFreq or OscFreq
	.ClkoPinConfig = MCP251XFD_CLKO_DivBy10,         //!< Configure the CLKO pin (SCLK div by 1, 2, 4, 10 or Start Of Frame)
	.SYSCLK_Result = &can1_sclk_result,              //!< This is the SYSCLK of the component after configuration (can be NULL if the internal SYSCLK of the component do not have to be known)

	//--- CAN configuration ---
	.NominalBitrate = 500000, 	// 500 Kbit         //!< Speed of the Frame description and arbitration
	.DataBitrate = MCP251XFD_NO_CANFD,                         //!< Speed of all the data bytes of the frame (if CAN2.0 only mode, set to value MCP251XFD_NO_CANFD)
	.BitTimeStats = &can0_bittimestats,           	//!< Point to a Bit Time stat structure (set to NULL if no statistics are necessary)
	.Bandwidth = MCP251XFD_NO_DELAY,                 //!< Transmit Bandwidth Sharing, this is the delay between two consecutive transmissions (in arbitration bit times)
	//!< Set of CAN control flags to configure the CAN controller. Configuration can be OR'ed
	.ControlFlags = MCP251XFD_CAN_LISTEN_ONLY_MODE_ON_ERROR,

	//--- GPIOs and Interrupts pins ---
	.GPIO0PinMode = MCP251XFD_PIN_AS_GPIO0_IN,      //!< Startup INT0/GPIO0/XSTBY pins mode (INT0 => Interrupt for TX)
	.GPIO1PinMode = MCP251XFD_PIN_AS_GPIO1_IN,      //!< Startup INT1/GPIO1 pins mode (INT1 => Interrupt for RX)
	.INTsOutMode = MCP251XFD_PINS_PUSHPULL_OUT,      //!< Define the output type of all interrupt pins (INT, INT0 and INT1)
	.TXCANOutMode = MCP251XFD_PINS_PUSHPULL_OUT,     //!< Define the output type of the TXCAN pin

	//--- Interrupts ---
	//!< Set of system interrupt flags to enable. Configuration can be OR'ed
	.SysInterruptFlags = MCP251XFD_INT_NO_EVENT
};

#define MCP251XFD_EXT1_FIFO_COUNT	10

MCP251XFD_RAMInfos Ext1_TEF_RAMInfos;
MCP251XFD_RAMInfos Ext1_TXQ_RAMInfos;
MCP251XFD_RAMInfos Ext1_FIFOs_RAMInfos[MCP251XFD_EXT1_FIFO_COUNT - 2];
MCP251XFD_FIFO MCP251XFD_Ext1_FIFOlist[MCP251XFD_EXT1_FIFO_COUNT] =
{
 { .Name = MCP251XFD_TEF, .Size = MCP251XFD_FIFO_10_MESSAGE_DEEP, .ControlFlags = MCP251XFD_FIFO_ADD_TIMESTAMP_ON_OBJ,
 .InterruptFlags = static_cast<eMCP251XFD_FIFOIntFlags>(MCP251XFD_FIFO_OVERFLOW_INT + MCP251XFD_FIFO_EVENT_FIFO_NOT_EMPTY_INT),
 .RAMInfos = &Ext1_TEF_RAMInfos, },
 { .Name = MCP251XFD_TXQ, .Size = MCP251XFD_FIFO_4_MESSAGE_DEEP, .Payload = MCP251XFD_PAYLOAD_64BYTE,
 .Attempts = MCP251XFD_THREE_ATTEMPTS, .Priority = MCP251XFD_MESSAGE_TX_PRIORITY16,
 .ControlFlags = MCP251XFD_FIFO_NO_RTR_RESPONSE,
 .InterruptFlags = static_cast<eMCP251XFD_FIFOIntFlags>(MCP251XFD_FIFO_TX_ATTEMPTS_EXHAUSTED_INT + MCP251XFD_FIFO_TRANSMIT_FIFO_NOT_FULL_INT),
 .RAMInfos = &Ext1_TXQ_RAMInfos, },
 { .Name = MCP251XFD_FIFO1, .Size = MCP251XFD_FIFO_4_MESSAGE_DEEP, .Payload = MCP251XFD_PAYLOAD_64BYTE,
 .Direction = MCP251XFD_RECEIVE_FIFO, .ControlFlags = MCP251XFD_FIFO_ADD_TIMESTAMP_ON_RX,
 .InterruptFlags = static_cast<eMCP251XFD_FIFOIntFlags>(MCP251XFD_FIFO_OVERFLOW_INT + MCP251XFD_FIFO_RECEIVE_FIFO_NOT_EMPTY_INT),
 .RAMInfos = &Ext1_FIFOs_RAMInfos[0], }, // SID: 0x000..0x1FF ; No EID
 { .Name = MCP251XFD_FIFO2, .Size = MCP251XFD_FIFO_2_MESSAGE_DEEP, .Payload = MCP251XFD_PAYLOAD_64BYTE,
 .Direction = MCP251XFD_RECEIVE_FIFO, .ControlFlags = MCP251XFD_FIFO_ADD_TIMESTAMP_ON_RX,
 .InterruptFlags = static_cast<eMCP251XFD_FIFOIntFlags>(MCP251XFD_FIFO_OVERFLOW_INT + MCP251XFD_FIFO_RECEIVE_FIFO_NOT_EMPTY_INT),
 .RAMInfos = &Ext1_FIFOs_RAMInfos[1], }, // SID: 0x200..0x3FF ; No EID
 { .Name = MCP251XFD_FIFO3, .Size = MCP251XFD_FIFO_4_MESSAGE_DEEP, .Payload = MCP251XFD_PAYLOAD_64BYTE,
 .Direction = MCP251XFD_RECEIVE_FIFO, .ControlFlags = MCP251XFD_FIFO_ADD_TIMESTAMP_ON_RX,
 .InterruptFlags = static_cast<eMCP251XFD_FIFOIntFlags>(MCP251XFD_FIFO_OVERFLOW_INT + MCP251XFD_FIFO_RECEIVE_FIFO_NOT_EMPTY_INT),
 .RAMInfos = &Ext1_FIFOs_RAMInfos[2], }, // SID: 0x400..0x5FF ; No EID
 { .Name = MCP251XFD_FIFO4, .Size = MCP251XFD_FIFO_2_MESSAGE_DEEP, .Payload = MCP251XFD_PAYLOAD_64BYTE,
 .Direction = MCP251XFD_RECEIVE_FIFO, .ControlFlags = MCP251XFD_FIFO_ADD_TIMESTAMP_ON_RX,
 .InterruptFlags = static_cast<eMCP251XFD_FIFOIntFlags>(MCP251XFD_FIFO_OVERFLOW_INT + MCP251XFD_FIFO_RECEIVE_FIFO_NOT_EMPTY_INT),
 .RAMInfos = &Ext1_FIFOs_RAMInfos[3], }, // SID: 0x600..0x7FF ; No EID
 { .Name = MCP251XFD_FIFO5, .Size = MCP251XFD_FIFO_4_MESSAGE_DEEP, .Payload = MCP251XFD_PAYLOAD_64BYTE,
 .Direction = MCP251XFD_TRANSMIT_FIFO, .Attempts = MCP251XFD_THREE_ATTEMPTS,
 .Priority = MCP251XFD_MESSAGE_TX_PRIORITY16, .ControlFlags = MCP251XFD_FIFO_NO_RTR_RESPONSE,
 .InterruptFlags = static_cast<eMCP251XFD_FIFOIntFlags>(MCP251XFD_FIFO_TX_ATTEMPTS_EXHAUSTED_INT + MCP251XFD_FIFO_TRANSMIT_FIFO_NOT_FULL_INT),
 .RAMInfos = &Ext1_FIFOs_RAMInfos[4], },
 { .Name = MCP251XFD_FIFO6, .Size = MCP251XFD_FIFO_2_MESSAGE_DEEP, .Payload = MCP251XFD_PAYLOAD_64BYTE,
 .Direction = MCP251XFD_TRANSMIT_FIFO, .Attempts = MCP251XFD_THREE_ATTEMPTS,
 .Priority = MCP251XFD_MESSAGE_TX_PRIORITY16, .ControlFlags = MCP251XFD_FIFO_NO_RTR_RESPONSE,
 .InterruptFlags = static_cast<eMCP251XFD_FIFOIntFlags>(MCP251XFD_FIFO_TX_ATTEMPTS_EXHAUSTED_INT + MCP251XFD_FIFO_TRANSMIT_FIFO_NOT_FULL_INT),
 .RAMInfos = &Ext1_FIFOs_RAMInfos[5], },
 { .Name = MCP251XFD_FIFO7, .Size = MCP251XFD_FIFO_2_MESSAGE_DEEP, .Payload = MCP251XFD_PAYLOAD_64BYTE,
 .Direction = MCP251XFD_TRANSMIT_FIFO, .Attempts = MCP251XFD_THREE_ATTEMPTS,
 .Priority = MCP251XFD_MESSAGE_TX_PRIORITY16, .ControlFlags = MCP251XFD_FIFO_NO_RTR_RESPONSE,
 .InterruptFlags = static_cast<eMCP251XFD_FIFOIntFlags>(MCP251XFD_FIFO_TX_ATTEMPTS_EXHAUSTED_INT + MCP251XFD_FIFO_TRANSMIT_FIFO_NOT_FULL_INT),
 .RAMInfos = &Ext1_FIFOs_RAMInfos[6], },
 { .Name = MCP251XFD_FIFO8, .Size = MCP251XFD_FIFO_2_MESSAGE_DEEP, .Payload = MCP251XFD_PAYLOAD_64BYTE,
 .Direction = MCP251XFD_TRANSMIT_FIFO, .Attempts = MCP251XFD_THREE_ATTEMPTS,
 .Priority = MCP251XFD_MESSAGE_TX_PRIORITY16, .ControlFlags = MCP251XFD_FIFO_NO_RTR_RESPONSE,
 .InterruptFlags = static_cast<eMCP251XFD_FIFOIntFlags>(MCP251XFD_FIFO_TX_ATTEMPTS_EXHAUSTED_INT + MCP251XFD_FIFO_TRANSMIT_FIFO_NOT_FULL_INT),
 .RAMInfos = &Ext1_FIFOs_RAMInfos[7], },
};

#define MCP251XFD_EXT1_FILTER_COUNT 4
MCP251XFD_Filter MCP251XFD_Ext1_FilterList[MCP251XFD_EXT1_FILTER_COUNT] =
{
 { .Filter = MCP251XFD_FILTER0, .EnableFilter = false, .Match = MCP251XFD_MATCH_ONLY_SID, .PointTo = MCP251XFD_FIFO1, .AcceptanceID = 0x000, .AcceptanceMask = 0x600, }, // 0x000..0x1FF
 { .Filter = MCP251XFD_FILTER1, .EnableFilter = false, .Match = MCP251XFD_MATCH_ONLY_SID, .PointTo = MCP251XFD_FIFO2, .AcceptanceID = 0x200, .AcceptanceMask = 0x600 }, // 0x200..0x3FF
 { .Filter = MCP251XFD_FILTER2, .EnableFilter = false, .Match = MCP251XFD_MATCH_ONLY_SID, .PointTo = MCP251XFD_FIFO3, .AcceptanceID = 0x400, .AcceptanceMask = 0x600 }, // 0x400..0x5FF
 { .Filter = MCP251XFD_FILTER3, .EnableFilter = false, .Match = MCP251XFD_MATCH_ONLY_SID, .PointTo = MCP251XFD_FIFO4, .AcceptanceID = 0x600, .AcceptanceMask = 0x600 }, // 0x600..0x7FF
};

eERRORRESULT ConfigureMCP251XFDonCAN1() {
	eERRORRESULT result = ERR__NO_DEVICE_DETECTED;

	// Configure the CAN device
	Debug::log("GetSpiConfig()");
	result = GetSpiConfig(&cfg_can1, 1, SonataPinmux::OutputPin::rph_g8, SonataPinmux::OutputPin::ser0_tx, SonataPinmux::OutputPin::ser0_tx, SonataPinmux::OutputPin::rph_g11, SonataPinmux::OutputPin::rph_g10, 1);
	if(result != ERR_OK) {
		Debug::log("ERROR! GetSpiConfig() failed with {}.", result);
	}

	Debug::log("Init_MCP251XFD()");
	thread_millisecond_wait(300);	// A short delay for clock to stablise (only need to be 3ms so 300ms should be more than enough)
	result = Init_MCP251XFD(&can1, &can_conf1);
	if(result != ERR_OK) {
		Debug::log("ERROR! Init_MCP251XFD() failed with {}.", result);
	}
	Debug::log("Init_MCP251XFD() done.");

	//Debug::log("MCP251XFD_ConfigureTimeStamp()");
	//Debug::log("SYSCLK_Ext1 = {}", SYSCLK_Ext1);
	//Debug::log("TIMESTAMP_TICK(SYSCLK_Ext1) = {}", TIMESTAMP_TICK(SYSCLK_Ext1));
	//result = MCP251XFD_ConfigureTimeStamp(&can1, true, MCP251XFD_TS_CAN20_SOF_CANFD_SOF, TIMESTAMP_TICK(SYSCLK_Ext1), false);
	//if(result != ERR_OK) {
	//	Debug::log("ERROR! MCP251XFD_ConfigureTimeStamp() failed with {}.", result);
	//}
	//Debug::log("MCP251XFD_ConfigureTimeStamp() done.");

	Debug::log("MCP251XFD_ConfigureFIFOList()");
	result = MCP251XFD_ConfigureFIFOList(&can1, MCP251XFD_Ext1_FIFOlist, MCP251XFD_EXT1_FIFO_COUNT);
	if(result != ERR_OK) {
		Debug::log("ERROR! MCP251XFD_ConfigureFIFOList() failed with {}.", result);
	}
	Debug::log("MCP251XFD_ConfigureFIFOList() done.");

	Debug::log("MCP251XFD_ConfigureFilterList()");
	result = MCP251XFD_ConfigureFilterList(&can1, MCP251XFD_D_NET_FILTER_DISABLE, &MCP251XFD_Ext1_FilterList[0], MCP251XFD_EXT1_FILTER_COUNT);
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

/// Thread entry point.
void __cheri_compartment("main_comp") main_entry()
{
	eERRORRESULT result = ERR__NO_DEVICE_DETECTED;

	// Print welcome, along with the compartment's name to the default UART.
	Debug::log("Sonata MCP251XFD CAN Example");

	Debug::log("Configure MCP251XFD on CAN1");
	result = ConfigureMCP251XFDonCAN1();
	if(result != ERR_OK) {
		Debug::log("ERROR! ConfigureMCP251XFDonCAN1() failed with {}.", result);
	}

	while(true) {
		thread_millisecond_wait(1000);
	}
}
