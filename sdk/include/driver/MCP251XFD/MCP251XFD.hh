#pragma once
/*!*****************************************************************************
 * @file    MCP251XFD.h
 * @author  Fabien 'Emandhal' MAILLY
 * @version 1.0.6
 * @date    16/04/2023
 * @brief   MCP251XFD driver
 * @details
 * The MCP251XFD component is a CAN-bus controller supporting CAN2.0A, CAN2.0B
 * and CAN-FD with SPI interface
 * Follow datasheet MCP2517FD Rev.B (July 2019)
 *                  MCP2518FD Rev.B (Dec  2020)
 *                  MCP251863 Rev.A (Sept 2022) [Have a MCP2518FD inside]
 * Follow MCP25XXFD Family Reference Manual (DS20005678D)
 ******************************************************************************/
/* @page License
 *
 * Copyright (c) 2020-2023 Fabien MAILLY
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 *all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS,
 * IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
 * EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

/* Revision history:
 * 1.0.7    Add MCP251XFD_SetFIFOinterruptConfiguration(),
 *MCP251XFD_SetTEFinterruptConfiguration() and
 *MCP251XFD_SetTXQinterruptConfiguration() These are to change interrupt
 *configuration of a FIFO/TEF/TXQ at runtime [Thanks to xmurx] 1.0.6    Reduce
 *code size by merging all Data Reads and all Data Writes 1.0.5    Do a safer
 *timeout for functions Mark RegMCP251XFD_IOCON as deprecated following
 *MCP2517FD: DS80000792C (§6), MCP2518FD: DS80000789C (§5), MCP251863:
 *DS80000984A (§5) Change SPI max speed following MCP2517FD: DS80000792C (§5),
 *MCP2518FD: DS80000789C (§4), MCP251863: DS80000984A (§4) 1.0.4    Minor
 *changes in the code and documentation [Thanks to BombaMat] 1.0.3    Add
 *MCP251XFD_StartCANListenOnly() function Correct the
 *MCP251XFD_ReceiveMessageFromFIFO() function [Thanks to mikucukyilmaz] 1.0.2
 *MessageCtrlFlags is a set of instead of an enum, some reorganization of the
 *code 1.0.1    Simplify implementation of MCP251XFD_ConfigurePins(),
 *MCP251XFD_SetGPIOPinsDirection(), MCP251XFD_GetGPIOPinsInputLevel(), and
 *MCP251XFD_SetGPIOPinsOutputLevel() functions Correct
 *MCP251XFD_EnterSleepMode() function's description 1.0.0    Release version
 *****************************************************************************/

//=============================================================================
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
//-----------------------------------------------------------------------------
#include "Conf_MCP251XFD.h"
#include "ErrorsDef.h"
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
#define __MCP251XFD_PACKED__
#define MCP251XFD_PACKITEM __pragma(pack(push, 1))
#define MCP251XFD_UNPACKITEM __pragma(pack(pop))
#define MCP251XFD_PACKENUM(name, type) typedef enum name : type
#define MCP251XFD_UNPACKENUM(name) name

//! This macro is used to check the size of an object. If not, it will raise a
//! "divide by 0" error at compile time
#define MCP251XFD_CONTROL_ITEM_SIZE(item, size)                                \
	enum                                                                       \
	{                                                                          \
		item##_size_must_be_##size##_bytes =                                   \
		  1 / (int)(!!(sizeof(item) == size))                                  \
	}

//-----------------------------------------------------------------------------

#if MCP251XFD_TRANS_BUF_SIZE < 9
#	error MCP251XFD_TRANS_BUF_SIZE should not be < 9
#endif

//********************************************************************************************************************
// MCP251XFD limits definitions
//********************************************************************************************************************

// Frequencies and bitrate limits for MCP251XFD
#define MCP251XFD_XTALFREQ_MIN (4000000u)    //!< Min Xtal/Resonator frequency
#define MCP251XFD_XTALFREQ_MAX (40000000u)   //!< Max Xtal/Resonator frequency
#define MCP251XFD_OSCFREQ_MIN (2000000u)     //!< Min Oscillator frequency
#define MCP251XFD_OSCFREQ_MAX (40000000u)    //!< Max Oscillator frequency
#define MCP251XFD_SYSCLK_MIN (2000000u)      //!< Min SYSCLK frequency
#define MCP251XFD_SYSCLK_MAX (40000000u)     //!< Max SYSCLK frequency
#define MCP251XFD_CLKINPLL_MAX (40000000u)   //!< Max CLKIN+PLL frequency
#define MCP251XFD_NOMBITRATE_MIN (125000u)   //!< Min Nominal bitrate
#define MCP251XFD_NOMBITRATE_MAX (1000000u)  //!< Max Nominal bitrate
#define MCP251XFD_DATABITRATE_MIN (500000u)  //!< Min Data bitrate
#define MCP251XFD_DATABITRATE_MAX (8000000u) //!< Max Data bitrate
#define MCP251XFD_SPICLOCK_MAX                                                 \
	(17000000u) //!< Max SPI clock frequency for MCP251XFD (Ensure that FSCK is
	            //!< less than or equal to 0.85 * (FSYSCLK/2))

//-----------------------------------------------------------------------------

// Limits Bit Rate configuration range for MCP251XFD
#define MCP251XFD_tTXDtRXD_MAX                                                 \
	(255) //!< tTXD-RXD is the propagation delay of the transceiver, a maximum
	      //!< 255ns according to ISO 11898-1:2015
#define MCP251XFD_tBUS_CONV                                                    \
	(5) //!< TBUS is the delay on the CAN bus, which is approximately 5ns/m

#define MCP251XFD_NBRP_MIN (1)   //!< Min NBRP
#define MCP251XFD_NBRP_MAX (256) //!< Max NBRP
#define MCP251XFD_NSYNC (1) //!< NSYNC is 1 NTQ (Defined in ISO 11898-1:2015)
#define MCP251XFD_NTSEG1_MIN (2)   //!< Min NTSEG1
#define MCP251XFD_NTSEG1_MAX (256) //!< Max NTSEG1
#define MCP251XFD_NTSEG2_MIN (1)   //!< Min NTSEG2
#define MCP251XFD_NTSEG2_MAX (128) //!< Max NTSEG2
#define MCP251XFD_NSJW_MIN (1)     //!< Min NSJW
#define MCP251XFD_NSJW_MAX (128)   //!< Max NSJW
#define MCP251XFD_NTQBIT_MIN                                                   \
	(MCP251XFD_NSYNC + MCP251XFD_NTSEG1_MIN +                                  \
	 MCP251XFD_NTSEG2_MIN) //!< Min NTQ per Bit (1-bit SYNC + 1-bit PRSEG +
	                       //!< 1-bit PHSEG1 + 1-bit PHSEG2)
#define MCP251XFD_NTQBIT_MAX                                                   \
	(MCP251XFD_NTSEG1_MAX + MCP251XFD_NTSEG2_MAX +                             \
	 1) //!< Max NTQ per Bit (385-bits)

#define MCP251XFD_DBRP_MIN (1)   //!< Min DBRP
#define MCP251XFD_DBRP_MAX (256) //!< Max DBRP
#define MCP251XFD_DSYNC (1) //!< DSYNC is 1 NTQ (Defined in ISO 11898-1:2015)
#define MCP251XFD_DTSEG1_MIN (1)  //!< Min DTSEG1
#define MCP251XFD_DTSEG1_MAX (32) //!< Max DTSEG1
#define MCP251XFD_DTSEG2_MIN (1)  //!< Min DTSEG2
#define MCP251XFD_DTSEG2_MAX (16) //!< Max DTSEG2
#define MCP251XFD_DSJW_MIN (1)    //!< Min DSJW
#define MCP251XFD_DSJW_MAX (16)   //!< Max DSJW
#define MCP251XFD_DTQBIT_MIN                                                   \
	(MCP251XFD_NSYNC + MCP251XFD_NTSEG1_MIN +                                  \
	 MCP251XFD_NTSEG2_MIN) //!< Min DTQ per Bit (1-bit SYNC + 1-bit PRSEG +
	                       //!< 1-bit PHSEG1 + 1-bit PHSEG2)
#define MCP251XFD_DTQBIT_MAX                                                   \
	(MCP251XFD_NTSEG1_MAX + MCP251XFD_NTSEG2_MAX +                             \
	 1) //!< Max DTQ per Bit (49-bits)

#define MCP251XFD_TDCO_MIN (-64) //!< Min TDCO
#define MCP251XFD_TDCO_MAX (63)  //!< Max TDCO
#define MCP251XFD_TDCV_MIN (0)   //!< Min TDCV
#define MCP251XFD_TDCV_MAX (63)  //!< Max TDCV

//-----------------------------------------------------------------------------

// FIFO definitions
#define MCP251XFD_TEF_MAX (1)   //!< 1 TEF maximum
#define MCP251XFD_TXQ_MAX (1)   //!< 1 TXQ maximum
#define MCP251XFD_FIFO_MAX (31) //!< 31 FIFOs maximum
#define MCP251XFD_FIFO_CONF_MAX                                                \
	(MCP251XFD_TEF_MAX + MCP251XFD_TXQ_MAX +                                   \
	 MCP251XFD_FIFO_MAX) //!< Maximum 33 FIFO configurable
	                     //!< (TEF + TXQ + 31 FIFO)
#define MCP251XFD_TX_FIFO_MAX                                                  \
	(MCP251XFD_TXQ_MAX +                                                       \
	 MCP251XFD_FIFO_MAX) //!< Maximum 32 transmit FIFO (TXQ + 31 FIFO)
#define MCP251XFD_RX_FIFO_MAX                                                  \
	(MCP251XFD_TEF_MAX +                                                       \
	 MCP251XFD_FIFO_MAX) //!< Maximum 32 receive FIFO (TEF + 31 FIFO)

//-----------------------------------------------------------------------------

// Memory mapping definitions for MCP251XFD
#define MCP251XFD_CAN_CONTROLLER_SIZE (752u) //!< CAN controller size
#define MCP251XFD_RAM_SIZE (2048u)           //!< RAM size
#define MCP251XFD_CONTROLLER_SFR_SIZE (24u)  //!< Controller SFR size

#define MCP251XFD_CAN_CONTROLLER_ADDR                                          \
	(0x000u)                        //!< CAN Controller Memory base address
#define MCP251XFD_RAM_ADDR (0x400u) //!< RAM Memory base address
#define MCP251XFD_CONTROLLER_SFR_ADDR                                          \
	(0xE00u)                        //!< SFR Controller Memory base address
#define MCP251XFD_END_ADDR (0xFFFu) //!< Last possible address

//-----------------------------------------------------------------------------

// Safe Reset speed definition
#define MCP251XFD_DRIVER_SAFE_RESET_SPI_CLK                                    \
	(MCP251XFD_SYSCLK_MIN /                                                    \
	 2) //!< Set the SPI safe reset clock speed at 1MHz because SPI speed max is
	    //!< SYSCLK/2 and Xtal/Resonator/Oscillator frequency min is 2MHz

//-----------------------------------------------------------------------------

// SPI commands instructions (first 2 bytes are CAAA where C is 4-bits Command
// and A is 12-bits Address)
#define MCP251XFD_SPI_INSTRUCTION_RESET (0x00) //!< Reset instruction
#define MCP251XFD_SPI_INSTRUCTION_READ (0x03)  //!< Read instruction
#define MCP251XFD_SPI_INSTRUCTION_WRITE (0x02) //!< Write instruction
#define MCP251XFD_SPI_INSTRUCTION_WRITE_CRC                                    \
	(0x0A) //!< Write with CRC instruction
#define MCP251XFD_SPI_INSTRUCTION_READ_CRC (0x0B) //!< Read with CRC instruction
#define MCP251XFD_SPI_INSTRUCTION_SAFE_WRITE (0x0C) //!< Safe Write instruction

#define MCP251XFD_SPI_FIRST_BYTE(instruction, address)                         \
	(((instruction) << 4) |                                                    \
	 (((address) >> 8) & 0xF)) //!< Set first byte of SPI command
#define MCP251XFD_SPI_SECOND_BYTE(address)                                     \
	((address) & 0xFF) //!< Set next byte of SPI command
#define MCP251XFD_SPI_16BITS_WORD(instruction, address)                        \
	(((instruction) << 12) |                                                   \
	 ((address) &                                                              \
	  0xFFF)) //!< Set first and second byte of SPI command into a 16-bit word

#define MCP251XFD_DEV_ID_Pos 2
#define MCP251XFD_DEV_ID_Mask (0x1u << MCP251XFD_DEV_ID_Pos)
#define MCP251XFD_DEV_ID_SET(value)                                            \
	(((uint8_t)(value) << MCP251XFD_DEV_ID_Pos) &                              \
	 MCP251XFD_DEV_ID_Mask) //!< Set Device ID
#define MCP251XFD_DEV_ID_GET(value)                                            \
	(((uint8_t)(value) & MCP251XFD_DEV_ID_Mask) >>                             \
	 MCP251XFD_DEV_ID_Pos) //!< Get Device ID

#define MCP251XFD_SFR_OSC_PLLEN ((uint32_t)(0x1u << 0))  //!< PLL Enable
#define MCP251XFD_SFR_OSC_PLLDIS ((uint32_t)(0x0u << 0)) //!< PLL Disable
#define MCP251XFD_SFR_OSC_OSCDIS                                               \
	((uint32_t)(0x1u << 2)) //!< Clock (Oscillator) Disable, device is in sleep
	                        //!< mode
#define MCP251XFD_SFR_OSC_WAKEUP                                               \
	((uint32_t)(0x0u << 2)) //!< Device wake-up from sleep and put it in
	                        //!< Configuration mode
#define MCP251XFD_SFR_OSC_LPMEN                                                \
	((uint32_t)(0x1u                                                           \
	            << 3)) //!< Low Power Mode (LPM) Enable; Setting LPMEN doesn’t
	                   //!< actually put the device in LPM. It selects which
	                   //!< Sleep mode will be entered after requesting Sleep
	                   //!< mode using CiCON.REQOP. In order to wake up on RXCAN
	                   //!< activity, CiINT.WAKIE must be set
#define MCP251XFD_SFR_OSC_LPMDIS                                               \
	((uint32_t)(0x0u << 3)) //!< Low Power Mode (LPM) Disable; The device is in
	                        //!< sleep mode

#define MCP251XFD_SFR_OSC_SCLKDIV_Pos 4
#define MCP251XFD_SFR_OSC_SCLKDIV_Mask (0x1u << MCP251XFD_SFR_OSC_SCLKDIV_Pos)
#define MCP251XFD_SFR_OSC_SCLKDIV_SET(value)                                   \
	(((uint32_t)(value) << MCP251XFD_SFR_OSC_SCLKDIV_Pos) &                    \
	 MCP251XFD_SFR_OSC_SCLKDIV_Mask) //!< System Clock Divisor

#define MCP251XFD_SFR_OSC_CLKODIV_Pos 5
#define MCP251XFD_SFR_OSC_CLKODIV_Mask (0x3u << MCP251XFD_SFR_OSC_CLKODIV_Pos)
#define MCP251XFD_SFR_OSC_CLKODIV_SET(value)                                   \
	(((uint32_t)(value) << MCP251XFD_SFR_OSC_CLKODIV_Pos) &                    \
	 MCP251XFD_SFR_OSC_CLKODIV_Mask) //!< Clock Output Divisor
#define MCP251XFD_SFR_OSC_PLLRDY ((uint32_t)(0x1u << 8))  //!< PLL Ready
#define MCP251XFD_SFR_OSC_OSCRDY ((uint32_t)(0x1u << 10)) //!< Clock Ready
#define MCP251XFD_SFR_OSC_SCLKRDY                                              \
	((uint32_t)(0x1u << 12)) //!< Synchronized SCLKDIV

//*** Byte version access to Registers ***
#define MCP251XFD_SFR_OSC8_PLLEN ((uint8_t)(0x1u << 0))  //!< PLL Enable
#define MCP251XFD_SFR_OSC8_PLLDIS ((uint8_t)(0x0u << 0)) //!< PLL Disable
#define MCP251XFD_SFR_OSC8_OSCDIS                                              \
	((uint8_t)(0x1u                                                            \
	           << 2)) //!< Clock (Oscillator) Disable, device is in sleep mode
#define MCP251XFD_SFR_OSC8_WAKEUP                                              \
	((uint8_t)(0x0u << 2)) //!< Device wake-up from sleep and put it in
	                       //!< Configuration mode
#define MCP251XFD_SFR_OSC8_LPMEN                                               \
	((uint8_t)(0x1u                                                            \
	           << 3)) //!< Low Power Mode (LPM) Enable; Setting LPMEN doesn’t
	                  //!< actually put the device in LPM. It selects which
	                  //!< Sleep mode will be entered after requesting Sleep
	                  //!< mode using CiCON.REQOP. In order to wake up on RXCAN
	                  //!< activity, CiINT.WAKIE must be set
#define MCP251XFD_SFR_OSC8_LPMDIS                                              \
	((uint8_t)(0x0u << 3)) //!< Low Power Mode (LPM) Disable; The device is in
	                       //!< sleep mode
#define MCP251XFD_SFR_OSC8_SCLKDIV_Pos 4
#define MCP251XFD_SFR_OSC8_SCLKDIV_Mask (0x1u << MCP251XFD_SFR_OSC8_SCLKDIV_Pos)
#define MCP251XFD_SFR_OSC8_SCLKDIV_SET(value)                                  \
	(((uint8_t)(value) << MCP251XFD_SFR_OSC8_SCLKDIV_Pos) &                    \
	 MCP251XFD_SFR_OSC8_SCLKDIV_Mask) //!< System Clock Divisor
#define MCP251XFD_SFR_OSC8_CLKODIV_Pos 5
#define MCP251XFD_SFR_OSC8_CLKODIV_Mask (0x3u << MCP251XFD_SFR_OSC8_CLKODIV_Pos)
#define MCP251XFD_SFR_OSC8_CLKODIV_SET(value)                                  \
	(((uint8_t)(value) << MCP251XFD_SFR_OSC8_CLKODIV_Pos) &                    \
	 MCP251XFD_SFR_OSC8_CLKODIV_Mask) //!< Clock Output Divisor
#define MCP251XFD_SFR_OSC8_PLLRDY ((uint8_t)(0x1u << 0)) //!< PLL Ready
#define MCP251XFD_SFR_OSC8_OSCRDY ((uint8_t)(0x1u << 2)) //!< Clock Ready
#define MCP251XFD_SFR_OSC8_SCLKRDY                                             \
	((uint8_t)(0x1u << 4)) //!< Synchronized SCLKDIV
#define MCP251XFD_SFR_OSC8_CHECKFLAGS                                          \
	(MCP251XFD_SFR_OSC8_PLLRDY | MCP251XFD_SFR_OSC8_OSCRDY |                   \
	 MCP251XFD_SFR_OSC8_SCLKRDY) //!< Oscillator check flags

//-----------------------------------------------------------------------------

#define MCP251XFD_GPIO0_Mask (0b01) //!< Define the GPIO0 mask
#define MCP251XFD_GPIO1_Mask (0b10) //!< Define the GPIO1 mask

#define MCP251XFD_GPIO0_OUTPUT (0b00) //!< Define the GPIO0 as output
#define MCP251XFD_GPIO0_INPUT (0b01)  //!< Define the GPIO0 as input

#define MCP251XFD_GPIO1_OUTPUT (0b00) //!< Define the GPIO1 as output
#define MCP251XFD_GPIO1_INPUT (0b10)  //!< Define the GPIO1 as input

#define MCP251XFD_GPIO0_LOW (0b00)  //!< Define the GPIO0 low status
#define MCP251XFD_GPIO0_HIGH (0b01) //!< Define the GPIO0 high status

#define MCP251XFD_GPIO1_LOW (0b00)  //!< Define the GPIO1 low status
#define MCP251XFD_GPIO1_HIGH (0b10) //!< Define the GPIO1 high status

#define MCP251XFD_SFR_IOCON_GPIO0_INPUT                                        \
	((uint32_t)(0x1u << 0)) //!< GPIO0 Data Input Direction
#define MCP251XFD_SFR_IOCON_GPIO0_OUTPUT                                       \
	((uint32_t)(0x0u << 0)) //!< GPIO0 Data Output Direction
#define MCP251XFD_SFR_IOCON_GPIO1_INPUT                                        \
	((uint32_t)(0x1u << 1)) //!< GPIO1 Data Input Direction
#define MCP251XFD_SFR_IOCON_GPIO1_OUTPUT                                       \
	((uint32_t)(0x0u << 1)) //!< GPIO1 Data Output Direction
#define MCP251XFD_SFR_IOCON_XSTBYEN                                            \
	((uint32_t)(0x1u << 6)) //!< Enable Transceiver Standby Pin Control
#define MCP251XFD_SFR_IOCON_XSTBYDIS                                           \
	((uint32_t)(0x0u << 6)) //!< Disable Transceiver Standby Pin Control
#define MCP251XFD_SFR_IOCON_GPIO0_HIGH                                         \
	((uint32_t)(0x1u << 8)) //!< GPIO0 Latch Drive Pin High
#define MCP251XFD_SFR_IOCON_GPIO0_LOW                                          \
	((uint32_t)(0x0u << 8)) //!< GPIO0 Latch Drive Pin Low
#define MCP251XFD_SFR_IOCON_GPIO1_HIGH                                         \
	((uint32_t)(0x1u << 9)) //!< GPIO1 Latch Drive Pin High
#define MCP251XFD_SFR_IOCON_GPIO1_LOW                                          \
	((uint32_t)(0x0u << 9)) //!< GPIO1 Latch Drive Pin Low
#define MCP251XFD_SFR_IOCON_GPIO0_STATUS                                       \
	((uint32_t)(0x1u << 16)) //!< GPIO0 Status
#define MCP251XFD_SFR_IOCON_GPIO1_STATUS                                       \
	((uint32_t)(0x1u << 17)) //!< GPIO1 Status
#define MCP251XFD_SFR_IOCON_GPIO0_MODE                                         \
	((uint32_t)(0x1u << 24)) //!< GPIO0 GPIO Pin Mode
#define MCP251XFD_SFR_IOCON_GPIO0_INT0                                         \
	((uint32_t)(0x0u << 24)) //!< GPIO0 Interrupt Pin INT0, asserted when
	                         //!< CiINT.TXIF and TXIE are set
#define MCP251XFD_SFR_IOCON_GPIO1_MODE                                         \
	((uint32_t)(0x1u << 25)) //!< GPIO1 GPIO Pin Mode
#define MCP251XFD_SFR_IOCON_GPIO1_INT1                                         \
	((uint32_t)(0x0u << 25)) //!< GPIO1 Interrupt Pin INT1, asserted when
	                         //!< CiINT.RXIF and RXIE are set
#define MCP251XFD_SFR_IOCON_TXCANOD                                            \
	((uint32_t)(0x1u << 28)) //!< TXCAN Open Drain Mode
#define MCP251XFD_SFR_IOCON_SOF                                                \
	((uint32_t)(0x1u << 29)) //!< Start-Of-Frame signal
#define MCP251XFD_SFR_IOCON_INTOD                                              \
	((uint32_t)(0x1u << 30)) //!< Interrupt pins Open Drain Mode

//*** Byte version access to Registers ***
#define MCP251XFD_SFR_IOCON8_GPIO0_INPUT                                       \
	((uint8_t)(0x1u << 0)) //!< GPIO0 Data Input Direction
#define MCP251XFD_SFR_IOCON8_GPIO0_OUTPUT                                      \
	((uint8_t)(0x0u << 0)) //!< GPIO0 Data Output Direction
#define MCP251XFD_SFR_IOCON8_GPIO1_INPUT                                       \
	((uint8_t)(0x1u << 1)) //!< GPIO1 Data Input Direction
#define MCP251XFD_SFR_IOCON8_GPIO1_OUTPUT                                      \
	((uint8_t)(0x0u << 1)) //!< GPIO1 Data Output Direction
#define MCP251XFD_SFR_IOCON8_XSTBYEN                                           \
	((uint8_t)(0x1u << 6)) //!< Enable Transceiver Standby Pin Control
#define MCP251XFD_SFR_IOCON8_XSTBYDIS                                          \
	((uint8_t)(0x0u << 6)) //!< Disable Transceiver Standby Pin Control

#define MCP251XFD_SFR_IOCON8_GPIO0_HIGH                                        \
	((uint8_t)(0x1u << 0)) //!< GPIO0 Latch Drive Pin High
#define MCP251XFD_SFR_IOCON8_GPIO0_LOW                                         \
	((uint8_t)(0x0u << 0)) //!< GPIO0 Latch Drive Pin Low
#define MCP251XFD_SFR_IOCON8_GPIO1_HIGH                                        \
	((uint8_t)(0x1u << 1)) //!< GPIO1 Latch Drive Pin High
#define MCP251XFD_SFR_IOCON8_GPIO1_LOW                                         \
	((uint8_t)(0x0u << 1)) //!< GPIO1 Latch Drive Pin Low

#define MCP251XFD_SFR_IOCON8_GPIO0_STATUS                                      \
	((uint8_t)(0x1u << 0)) //!< GPIO0 Status
#define MCP251XFD_SFR_IOCON8_GPIO1_STATUS                                      \
	((uint8_t)(0x1u << 1)) //!< GPIO1 Status

#define MCP251XFD_SFR_IOCON8_GPIO0_MODE                                        \
	((uint8_t)(0x1u << 0)) //!< GPIO0 GPIO Pin Mode
#define MCP251XFD_SFR_IOCON8_GPIO0_INT0                                        \
	((uint8_t)(0x0u << 0)) //!< GPIO0 Interrupt Pin INT0, asserted when
	                       //!< CiINT.TXIF and TXIE are set
#define MCP251XFD_SFR_IOCON8_GPIO1_MODE                                        \
	((uint8_t)(0x1u << 1)) //!< GPIO1 GPIO Pin Mode
#define MCP251XFD_SFR_IOCON8_GPIO1_INT1                                        \
	((uint8_t)(0x0u << 1)) //!< GPIO1 Interrupt Pin INT1, asserted when
	                       //!< CiINT.RXIF and RXIE are set
#define MCP251XFD_SFR_IOCON8_TXCANOD                                           \
	((uint8_t)(0x1u << 4)) //!< TXCAN Open Drain Mode
#define MCP251XFD_SFR_IOCON8_SOF                                               \
	((uint8_t)(0x1u << 5)) //!< Start-Of-Frame signal
#define MCP251XFD_SFR_IOCON8_INTOD                                             \
	((uint8_t)(0x1u << 6)) //!< Interrupt pins Open Drain Mode

#define MCP251XFD_SFR_CRC_Pos 0
#define MCP251XFD_SFR_CRC_Mask (0xFFFFu << MCP251XFD_SFR_CRC_Pos)
#define MCP251XFD_SFR_CRC_SET(value)                                           \
	(((uint32_t)(value) << MCP251XFD_SFR_CRC_Pos) &                            \
	 MCP251XFD_SFR_CRC_Mask) //!< Cycle Redundancy Check from last CRC mismatch
#define MCP251XFD_SFR_CRC_CRCERRIF                                             \
	((uint32_t)(0x1u << 16)) //!< CRC Error Interrupt Flag
#define MCP251XFD_SFR_CRC_FERRIF                                               \
	((uint32_t)(0x1u << 17)) //!< CRC Command Format Error Interrupt Flag
#define MCP251XFD_SFR_CRC_CRCERRIE                                             \
	((uint32_t)(0x1u << 24)) //!< CRC Error Interrupt Enable
#define MCP251XFD_SFR_CRC_FERRIE                                               \
	((uint32_t)(0x1u << 25)) //!< CRC Command Format Error Interrupt Enable

//*** Byte version access to Registers ***
#define MCP251XFD_SFR_CRC16_Pos 0
#define MCP251XFD_SFR_CRC16_Mask (0xFFFFu << MCP251XFD_SFR_CRC16_Pos)
#define MCP251XFD_SFR_CRC16_SET(value)                                         \
	(((uint16_t)(value) << MCP251XFD_SFR_CRC16_Pos) &                          \
	 MCP251XFD_SFR_CRC16_Mask) //!< Cycle Redundancy Check from last CRC
	                           //!< mismatch
#define MCP251XFD_SFR_CRC8_CRCERRIF                                            \
	((uint8_t)(0x1u << 0)) //!< CRC Error Interrupt Flag
#define MCP251XFD_SFR_CRC8_FERRIF                                              \
	((uint8_t)(0x1u << 1)) //!< CRC Command Format Error Interrupt Flag
#define MCP251XFD_SFR_CRC8_CRCERRIE                                            \
	((uint8_t)(0x1u << 0)) //!< CRC Error Interrupt Enable
#define MCP251XFD_SFR_CRC8_CRCERRID                                            \
	((uint8_t)(0x0u << 0)) //!< CRC Error Interrupt Disable
#define MCP251XFD_SFR_CRC8_FERRIE                                              \
	((uint8_t)(0x1u << 1)) //!< CRC Command Format Error Interrupt Enable
#define MCP251XFD_SFR_CRC8_FERRID                                              \
	((uint8_t)(0x0u << 1)) //!< CRC Command Format Error Interrupt Disable

#define MCP251XFD_SFR_ECCCON_ECCEN ((uint32_t)(0x1u << 0))  //!< ECC Enable
#define MCP251XFD_SFR_ECCCON_ECCDIS ((uint32_t)(0x0u << 0)) //!< ECC Disable
#define MCP251XFD_SFR_ECCCON_SECIE                                             \
	((uint32_t)(0x1u << 1)) //!< Single Error Correction Interrupt Enable Flag
#define MCP251XFD_SFR_ECCCON_SECID                                             \
	((uint32_t)(0x0u << 1)) //!< Single Error Correction Interrupt Disable Flag
#define MCP251XFD_SFR_ECCCON_DEDIE                                             \
	((uint32_t)(0x1u << 2)) //!< Double Error Detection Interrupt Enable Flag
#define MCP251XFD_SFR_ECCCON_DEDID                                             \
	((uint32_t)(0x0u << 2)) //!< Double Error Detection Interrupt Disable Flag
#define MCP251XFD_SFR_ECCCON_PARITY_Pos 8
#define MCP251XFD_SFR_ECCCON_PARITY_Mask                                       \
	(0x3Fu << MCP251XFD_SFR_ECCCON_PARITY_Pos)
#define MCP251XFD_SFR_ECCCON_PARITY_GET(value)                                 \
	(((uint32_t)(value) & MCP251XFD_SFR_ECCCON_PARITY_Mask) >>                 \
	 MCP251XFD_SFR_ECCCON_PARITY_Pos) //!< Parity bits used during write to RAM
	                                  //!< when ECC is disabled
#define MCP251XFD_SFR_ECCCON_PARITY_SET(value)                                 \
	(((uint32_t)(value) << MCP251XFD_SFR_ECCCON_PARITY_Pos) &                  \
	 MCP251XFD_SFR_ECCCON_PARITY_Mask) //!< Get parity bits used during write to
	                                   //!< RAM when ECC is disabled

//*** Byte version access to Registers ***
#define MCP251XFD_SFR_ECCCON8_ECCEN ((uint8_t)(0x1u << 0))  //!< ECC Enable
#define MCP251XFD_SFR_ECCCON8_ECCDIS ((uint8_t)(0x0u << 0)) //!< ECC Disable
#define MCP251XFD_SFR_ECCCON8_SECIE                                            \
	((uint8_t)(0x1u << 1)) //!< Single Error Correction Interrupt Enable Flag
#define MCP251XFD_SFR_ECCCON8_SECID                                            \
	((uint8_t)(0x0u << 1)) //!< Single Error Correction Interrupt Disable Flag
#define MCP251XFD_SFR_ECCCON8_DEDIE                                            \
	((uint8_t)(0x1u << 2)) //!< Double Error Detection Interrupt Enable Flag
#define MCP251XFD_SFR_ECCCON8_DEDID                                            \
	((uint8_t)(0x0u << 2)) //!< Double Error Detection Interrupt Disable Flag
#define MCP251XFD_SFR_ECCCON8_PARITY_Pos 0
#define MCP251XFD_SFR_ECCCON8_PARITY_Mask                                      \
	(0x3Fu << MCP251XFD_SFR_ECCCON8_PARITY_Pos)
#define MCP251XFD_SFR_ECCCON8_PARITY_GET(value)                                \
	(((uint8_t)(value) & MCP251XFD_SFR_ECCCON8_PARITY_Mask) >>                 \
	 MCP251XFD_SFR_ECCCON8_PARITY_Pos) //!< Set parity bits used during write to
	                                   //!< RAM when ECC is disabled
#define MCP251XFD_SFR_ECCCON8_PARITY_SET(value)                                \
	(((uint8_t)(value) << MCP251XFD_SFR_ECCCON8_PARITY_Pos) &                  \
	 MCP251XFD_SFR_ECCCON8_PARITY_Mask) //!< Get parity bits used during write
	                                    //!< to RAM when ECC is disabled

#define MCP251XFD_SFR_ECCSTAT_SECIF                                            \
	((uint32_t)(0x1u << 1)) //!< Single Error Correction Interrupt Flag
#define MCP251XFD_SFR_ECCSTAT_DEDIF                                            \
	((uint32_t)(0x1u << 2)) //!< Double Error Detection Interrupt Flag
#define MCP251XFD_SFR_ECCSTAT_ERRADDR_Pos 16
#define MCP251XFD_SFR_ECCSTAT_ERRADDR_Mask                                     \
	(0xFFFu << MCP251XFD_SFR_ECCSTAT_ERRADDR_Pos)
#define MCP251XFD_SFR_ECCSTAT_ERRADDR_GET(value)                               \
	(((uint32_t)(value) & MCP251XFD_SFR_ECCSTAT_ERRADDR_Mask) >>               \
	 MCP251XFD_SFR_ECCSTAT_ERRADDR_Pos) //!< Get address where last ECC error
	                                    //!< occurred

//*** Byte version access to Registers ***
#define MCP251XFD_SFR_ECCSTAT8_SECIF                                           \
	((uint8_t)(0x1u << 1)) //!< Single Error Correction Interrupt Flag
#define MCP251XFD_SFR_ECCSTAT8_DEDIF                                           \
	((uint8_t)(0x1u << 2)) //!< Double Error Detection Interrupt Flag
#define MCP251XFD_SFR_ECCSTAT16_ERRADDR_Pos 0
#define MCP251XFD_SFR_ECCSTAT16_ERRADDR_Mask                                   \
	(0xFFFu << MCP251XFD_SFR_ECCSTAT16_ERRADDR_Pos)
#define MCP251XFD_SFR_ECCSTAT16_ERRADDR_GET(value)                             \
	(((uint16_t)(value) & MCP251XFD_SFR_ECCSTAT16_ERRADDR_Mask) >>             \
	 MCP251XFD_SFR_ECCSTAT16_ERRADDR_Pos) //!< Get address where last ECC error
	                                      //!< occurred

#define MCP251XFD_SFR_DEVID_REV_Pos 0
#define MCP251XFD_SFR_DEVID_REV_Mask (0xFu << MCP251XFD_SFR_DEVID_REV_Pos)
#define MCP251XFD_SFR_DEVID_REV_GET(value)                                     \
	(((uint32_t)(value) & MCP251XFD_SFR_DEVID_REV_Mask) >>                     \
	 MCP251XFD_SFR_DEVID_REV_Pos) //!< Get Silicon Revision
#define MCP251XFD_SFR_DEVID_ID_Pos 4
#define MCP251XFD_SFR_DEVID_ID_Mask (0xFu << MCP251XFD_SFR_DEVID_ID_Pos)
#define MCP251XFD_SFR_DEVID_ID_GET(value)                                      \
	(((uint32_t)(value) & MCP251XFD_SFR_DEVID_ID_Mask) >>                      \
	 MCP251XFD_SFR_DEVID_ID_Pos) //!< Get Device ID

//*** Byte version access to Registers ***
#define MCP251XFD_SFR_DEVID8_REV_Pos 0
#define MCP251XFD_SFR_DEVID8_REV_Mask (0xFu << MCP251XFD_SFR_DEVID8_REV_Pos)
#define MCP251XFD_SFR_DEVID8_REV_GET(value)                                    \
	(((uint32_t)(value) & MCP251XFD_SFR_DEVID8_REV_Mask) >>                    \
	 MCP251XFD_SFR_DEVID8_REV_Pos) //!< Get Silicon Revision
#define MCP251XFD_SFR_DEVID8_ID_Pos 4
#define MCP251XFD_SFR_DEVID8_ID_Mask (0xFu << MCP251XFD_SFR_DEVID8_ID_Pos)
#define MCP251XFD_SFR_DEVID8_ID_GET(value)                                     \
	(((uint32_t)(value) & MCP251XFD_SFR_DEVID8_ID_Mask) >>                     \
	 MCP251XFD_SFR_DEVID8_ID_Pos) //!< Get Device ID

#define MCP251XFD_CAN_CiCON_DNCNT_Pos 0
#define MCP251XFD_CAN_CiCON_DNCNT_Mask (0x1Fu << MCP251XFD_CAN_CiCON_DNCNT_Pos)
#define MCP251XFD_CAN_CiCON_DNCNT_SET(value)                                   \
	(((uint32_t)(value) << MCP251XFD_CAN_CiCON_DNCNT_Pos) &                    \
	 MCP251XFD_CAN_CiCON_DNCNT_Mask) //!< Set Device Net Filter Bit Number
#define MCP251XFD_CAN_CiCON_ISOCRCEN                                           \
	(0x1u << 5) //!< Enable ISO CRC in CAN FD Frames
#define MCP251XFD_CAN_CiCON_PXEDIS                                             \
	(0x1u << 6) //!< Protocol Exception Event Detection Disabled
#define MCP251XFD_CAN_CiCON_WAKFIL                                             \
	(0x1u << 8) //!< Enable CAN Bus Line Wake-up Filter

#define MCP251XFD_CAN_CiCON_WFT_Pos 9
#define MCP251XFD_CAN_CiCON_WFT_Mask (0x3u << MCP251XFD_CAN_CiCON_WFT_Pos)
#define MCP251XFD_CAN_CiCON_WFT_SET(value)                                     \
	(((uint32_t)(value) << MCP251XFD_CAN_CiCON_WFT_Pos) &                      \
	 MCP251XFD_CAN_CiCON_WFT_Mask) //!< Selectable Wake-up Filter Time
#define MCP251XFD_CAN_CiCON_BUSY (0x1u << 11)   //!< CAN Module is Busy
#define MCP251XFD_CAN_CiCON_BRSDIS (0x1u << 12) //!< Bit Rate Switching Disable
#define MCP251XFD_CAN_CiCON_RTXAT                                              \
	(0x1u << 16) //!< Restrict Retransmission Attempts
#define MCP251XFD_CAN_CiCON_ESIGM (0x1u << 17) //!< Transmit ESI in Gateway Mode
#define MCP251XFD_CAN_CiCON_SERR2LOM                                           \
	(0x1u << 18) //!< Transition to Listen Only Mode on System Error
#define MCP251XFD_CAN_CiCON_STEF (0x1u << 19)  //!< Store in Transmit Event FIFO
#define MCP251XFD_CAN_CiCON_TXQEN (0x1u << 20) //!< Enable Transmit Queue

#define MCP251XFD_CAN_CiCON_OPMOD_Pos 21
#define MCP251XFD_CAN_CiCON_OPMOD_Mask (0x7u << MCP251XFD_CAN_CiCON_OPMOD_Pos)
#define MCP251XFD_CAN_CiCON_OPMOD_GET(value)                                   \
	(((uint32_t)(value) & MCP251XFD_CAN_CiCON_OPMOD_Mask) >>                   \
	 MCP251XFD_CAN_CiCON_OPMOD_Pos) //!< Get Operation Mode Status
#define MCP251XFD_CAN_CiCON_REQOP_Pos 24
#define MCP251XFD_CAN_CiCON_REQOP_Mask (0x7u << MCP251XFD_CAN_CiCON_REQOP_Pos)
#define MCP251XFD_CAN_CiCON_REQOP_SET(value)                                   \
	(((uint32_t)(value) << MCP251XFD_CAN_CiCON_REQOP_Pos) &                    \
	 MCP251XFD_CAN_CiCON_REQOP_Mask) //!< Set Request Operation Mode
#define MCP251XFD_CAN_CiCON_ABAT                                               \
	(0x1u << 24) //!< Set Abort All Pending Transmissions

#define MCP251XFD_CAN_CiCON_TXBWS_Pos 28
#define MCP251XFD_CAN_CiCON_TXBWS_Mask (0xFu << MCP251XFD_CAN_CiCON_TXBWS_Pos)
#define MCP251XFD_CAN_CiCON_TXBWS_SET(value)                                   \
	(((uint32_t)(value) << MCP251XFD_CAN_CiCON_TXBWS_Pos) &                    \
	 MCP251XFD_CAN_CiCON_TXBWS_Mask) //!< Set Transmit Bandwidth Sharing

//*** Byte version access to Registers ***
#define MCP251XFD_CAN_CiCON8_DNCNT_Pos 0
#define MCP251XFD_CAN_CiCON8_DNCNT_Mask                                        \
	(0x1Fu << MCP251XFD_CAN_CiCON8_DNCNT_Pos)
#define MCP251XFD_CAN_CiCON8_DNCNT_SET(value)                                  \
	(((uint32_t)(value) << MCP251XFD_CAN_CiCON8_DNCNT_Pos) &                   \
	 MCP251XFD_CAN_CiCON8_DNCNT_Mask) //!< Set Device Net Filter Bit Number
#define MCP251XFD_CAN_CiCON8_ISOCRCEN                                          \
	(0x1u << 5) //!< Enable ISO CRC in CAN FD Frames
#define MCP251XFD_CAN_CiCON8_PXEDIS                                            \
	(0x1u << 6) //!< Protocol Exception Event Detection Disabled
#define MCP251XFD_CAN_CiCON8_WAKFIL                                            \
	(0x1u << 0) //!< Enable CAN Bus Line Wake-up Filter
#define MCP251XFD_CAN_CiCON8_WFT_Pos 1
#define MCP251XFD_CAN_CiCON8_WFT_Mask (0x3u << MCP251XFD_CAN_CiCON8_WFT_Pos)
#define MCP251XFD_CAN_CiCON8_WFT_SET(value)                                    \
	(((uint32_t)(value) << MCP251XFD_CAN_CiCON8_WFT_Pos) &                     \
	 MCP251XFD_CAN_CiCON8_WFT_Mask) //!< Selectable Wake-up Filter Time
#define MCP251XFD_CAN_CiCON8_BUSY (0x1u << 3)   //!< CAN Module is Busy
#define MCP251XFD_CAN_CiCON8_BRSDIS (0x1u << 4) //!< Bit Rate Switching Disable
#define MCP251XFD_CAN_CiCON8_RTXAT                                             \
	(0x1u << 0) //!< Restrict Retransmission Attempts
#define MCP251XFD_CAN_CiCON8_ESIGM (0x1u << 1) //!< Transmit ESI in Gateway Mode
#define MCP251XFD_CAN_CiCON8_SERR2LOM                                          \
	(0x1u << 2) //!< Transition to Listen Only Mode on System Error
#define MCP251XFD_CAN_CiCON8_STEF (0x1u << 3)  //!< Store in Transmit Event FIFO
#define MCP251XFD_CAN_CiCON8_TXQEN (0x1u << 4) //!< Enable Transmit Queue
#define MCP251XFD_CAN_CiCON8_OPMOD_Pos 5
#define MCP251XFD_CAN_CiCON8_OPMOD_Mask (0x7u << MCP251XFD_CAN_CiCON8_OPMOD_Pos)
#define MCP251XFD_CAN_CiCON8_OPMOD_GET(value)                                  \
	(eMCP251XFD_OperationMode)(                                                \
	  ((uint8_t)(value) & MCP251XFD_CAN_CiCON8_OPMOD_Mask) >>                  \
	  MCP251XFD_CAN_CiCON8_OPMOD_Pos) //!< Get Operation Mode Status
#define MCP251XFD_CAN_CiCON8_REQOP_Pos 0
#define MCP251XFD_CAN_CiCON8_REQOP_Mask (0x7u << MCP251XFD_CAN_CiCON8_REQOP_Pos)
#define MCP251XFD_CAN_CiCON8_REQOP_SET(value)                                  \
	(((uint8_t)(value) << MCP251XFD_CAN_CiCON8_REQOP_Pos) &                    \
	 MCP251XFD_CAN_CiCON8_REQOP_Mask) //!< Request Operation Mode
#define MCP251XFD_CAN_CiCON8_ABAT                                              \
	(0x1u << 3) //!< Abort All Pending Transmissions
#define MCP251XFD_CAN_CiCON8_TXBWS_Pos 4
#define MCP251XFD_CAN_CiCON8_TXBWS_Mask (0xFu << MCP251XFD_CAN_CiCON8_TXBWS_Pos)
#define MCP251XFD_CAN_CiCON8_TXBWS_SET(value)                                  \
	(((uint8_t)(value) << MCP251XFD_CAN_CiCON8_TXBWS_Pos) &                    \
	 MCP251XFD_CAN_CiCON8_TXBWS_Mask) //!< Set Transmit Bandwidth Sharing

//-----------------------------------------------------------------------------
#define MCP251XFD_CAN_CiNBTCFG_SJW_Pos 0
#define MCP251XFD_CAN_CiNBTCFG_SJW_Mask                                        \
	(0x7Fu << MCP251XFD_CAN_CiNBTCFG_SJW_Pos)
#define MCP251XFD_CAN_CiNBTCFG_SJW_SET(value)                                  \
	(((uint32_t)(value) << MCP251XFD_CAN_CiNBTCFG_SJW_Pos) &                   \
	 MCP251XFD_CAN_CiNBTCFG_SJW_Mask) //!< Synchronization Jump Width bits;
	                                  //!< Length is value x TQ
#define MCP251XFD_CAN_CiNBTCFG_TSEG2_Pos 8
#define MCP251XFD_CAN_CiNBTCFG_TSEG2_Mask                                      \
	(0x7Fu << MCP251XFD_CAN_CiNBTCFG_TSEG2_Pos)
#define MCP251XFD_CAN_CiNBTCFG_TSEG2_SET(value)                                \
	(((uint32_t)(value) << MCP251XFD_CAN_CiNBTCFG_TSEG2_Pos) &                 \
	 MCP251XFD_CAN_CiNBTCFG_TSEG2_Mask) //!< Time Segment 2 bits (Phase Segment
	                                    //!< 2); Length is value x TQ
#define MCP251XFD_CAN_CiNBTCFG_TSEG1_Pos 16
#define MCP251XFD_CAN_CiNBTCFG_TSEG1_Mask                                      \
	(0xFFu << MCP251XFD_CAN_CiNBTCFG_TSEG1_Pos)
#define MCP251XFD_CAN_CiNBTCFG_TSEG1_SET(value)                                \
	(((uint32_t)(value) << MCP251XFD_CAN_CiNBTCFG_TSEG1_Pos) &                 \
	 MCP251XFD_CAN_CiNBTCFG_TSEG1_Mask) //!< Time Segment 1 bits (Propagation
	                                    //!< Segment + Phase Segment 1); Length
	                                    //!< is value x TQ
#define MCP251XFD_CAN_CiNBTCFG_BRP_Pos 24
#define MCP251XFD_CAN_CiNBTCFG_BRP_Mask                                        \
	(0xFFu << MCP251XFD_CAN_CiNBTCFG_BRP_Pos)
#define MCP251XFD_CAN_CiNBTCFG_BRP_SET(value)                                  \
	(((uint32_t)(value) << MCP251XFD_CAN_CiNBTCFG_BRP_Pos) &                   \
	 MCP251XFD_CAN_CiNBTCFG_BRP_Mask) //!< Baud Rate Prescaler bits; TQ =
	                                  //!< value/Fsys

//-----------------------------------------------------------------------------
#define MCP251XFD_CAN_CiDBTCFG_SJW_Pos 0
#define MCP251XFD_CAN_CiDBTCFG_SJW_Mask (0xFu << MCP251XFD_CAN_CiDBTCFG_SJW_Pos)
#define MCP251XFD_CAN_CiDBTCFG_SJW_SET(value)                                  \
	(((uint32_t)(value) << MCP251XFD_CAN_CiDBTCFG_SJW_Pos) &                   \
	 MCP251XFD_CAN_CiDBTCFG_SJW_Mask) //!< Synchronization Jump Width bits;
	                                  //!< Length is value x TQ
#define MCP251XFD_CAN_CiDBTCFG_TSEG2_Pos 8
#define MCP251XFD_CAN_CiDBTCFG_TSEG2_Mask                                      \
	(0xFu << MCP251XFD_CAN_CiDBTCFG_TSEG2_Pos)
#define MCP251XFD_CAN_CiDBTCFG_TSEG2_SET(value)                                \
	(((uint32_t)(value) << MCP251XFD_CAN_CiDBTCFG_TSEG2_Pos) &                 \
	 MCP251XFD_CAN_CiDBTCFG_TSEG2_Mask) //!< Time Segment 2 bits (Phase Segment
	                                    //!< 2); Length is value x TQ
#define MCP251XFD_CAN_CiDBTCFG_TSEG1_Pos 16
#define MCP251XFD_CAN_CiDBTCFG_TSEG1_Mask                                      \
	(0x1Fu << MCP251XFD_CAN_CiDBTCFG_TSEG1_Pos)
#define MCP251XFD_CAN_CiDBTCFG_TSEG1_SET(value)                                \
	(((uint32_t)(value) << MCP251XFD_CAN_CiDBTCFG_TSEG1_Pos) &                 \
	 MCP251XFD_CAN_CiDBTCFG_TSEG1_Mask) //!< Time Segment 1 bits (Propagation
	                                    //!< Segment + Phase Segment 1); Length
	                                    //!< is value x TQ
#define MCP251XFD_CAN_CiDBTCFG_BRP_Pos 24
#define MCP251XFD_CAN_CiDBTCFG_BRP_Mask                                        \
	(0xFFu << MCP251XFD_CAN_CiDBTCFG_BRP_Pos)
#define MCP251XFD_CAN_CiDBTCFG_BRP_SET(value)                                  \
	(((uint32_t)(value) << MCP251XFD_CAN_CiDBTCFG_BRP_Pos) &                   \
	 MCP251XFD_CAN_CiDBTCFG_BRP_Mask) //!< Baud Rate Prescaler bits; TQ =
	                                  //!< value/Fsys

//-----------------------------------------------------------------------------
#define MCP251XFD_CAN_CiTDC_TDCV_Pos 0
#define MCP251XFD_CAN_CiTDC_TDCV_Mask (0x3Fu << MCP251XFD_CAN_CiTDC_TDCV_Pos)
#define MCP251XFD_CAN_CiTDC_TDCV_SET(value)                                    \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTDC_TDCV_Pos) &                     \
	 MCP251XFD_CAN_CiTDC_TDCV_Mask) //!< Transmitter Delay Compensation Value
	                                //!< bits; Secondary Sample Point (SSP);
	                                //!< Length is value x TSYSCLK
#define MCP251XFD_CAN_CiTDC_TDCO_Pos 8
#define MCP251XFD_CAN_CiTDC_TDCO_BitWidth 7
#define MCP251XFD_CAN_CiTDC_TDCO_Mask (0x7Fu << MCP251XFD_CAN_CiTDC_TDCO_Pos)
#define MCP251XFD_CAN_CiTDC_TDCO_SET(value)                                    \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTDC_TDCO_Pos) &                     \
	 MCP251XFD_CAN_CiTDC_TDCO_Mask) //!< Set the Transmitter Delay Compensation
	                                //!< Offset bits; Secondary Sample Point
	                                //!< (SSP). Two’s complement; offset can be
	                                //!< positive, zero, or negative; value x
	                                //!< TSYSCLK (min is –64 (0b1111111 x
	                                //!< TSYSCLK ; max is 63 (0b0111111) x
	                                //!< TSYSCLK)
#define MCP251XFD_CAN_CiTDC_TDCO_GET(value)                                    \
	((int8_t)(((((uint32_t)(value) >> MCP251XFD_CAN_CiTDC_TDCO_Pos) &          \
	            ((1 << MCP251XFD_CAN_CiTDC_TDCO_BitWidth) - 1)) ^              \
	           (1 << (MCP251XFD_CAN_CiTDC_TDCO_BitWidth - 1))) -               \
	          (1 << (MCP251XFD_CAN_CiTDC_TDCO_BitWidth -                       \
	                 1)))) //!< Get the Transmitter Delay Compensation Offset
	                       //!< bits; Secondary Sample Point (SSP). Two’s
	                       //!< complement; offset can be positive, zero, or
	                       //!< negative; value x TSYSCLK (min is –64 (0b1111111
	                       //!< x TSYSCLK ; max is 63 (0b0111111) x TSYSCLK)
#define MCP251XFD_CAN_CiTDC_TDCMOD_Pos 16
#define MCP251XFD_CAN_CiTDC_TDCMOD_Mask (0x3u << MCP251XFD_CAN_CiTDC_TDCMOD_Pos)
#define MCP251XFD_CAN_CiTDC_TDCMOD_SET(value)                                  \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTDC_TDCMOD_Pos) &                   \
	 MCP251XFD_CAN_CiTDC_TDCMOD_Mask) //!< Transmitter Delay Compensation Mode
	                                  //!< bits; Secondary Sample Point (SSP)
#define MCP251XFD_CAN_CiTDC_SID11EN                                            \
	(0x1u << 24) //!< Enable 12-Bit SID in CAN FD Base Format Messages
#define MCP251XFD_CAN_CiTDC_EDGFLTEN                                           \
	(0x1u << 25) //!< Enable Edge Filtering during Bus Integration state
#define MCP251XFD_CAN_CiTDC_EDGFLTDIS                                          \
	(0x0u << 25) //!< Disable Edge Filtering during Bus Integration state

//*** Byte version access to Registers ***
#define MCP251XFD_CAN_CiTDC8_TDCV_Pos 0
#define MCP251XFD_CAN_CiTDC8_TDCV_Mask (0x3Fu << MCP251XFD_CAN_CiTDC8_TDCV_Pos)
#define MCP251XFD_CAN_CiTDC8_TDCV_SET(value)                                   \
	(((uint8_t)(value) << MCP251XFD_CAN_CiTDC8_TDCV_Pos) &                     \
	 MCP251XFD_CAN_CiTDC8_TDCV_Mask) //!< Transmitter Delay Compensation Value
	                                 //!< bits; Secondary Sample Point (SSP);
	                                 //!< Length is value x TSYSCLK
#define MCP251XFD_CAN_CiTDC8_TDCO_Pos 0
#define MCP251XFD_CAN_CiTDC8_TDCO_BitWidth 7
#define MCP251XFD_CAN_CiTDC8_TDCO_Mask (0x7Fu << MCP251XFD_CAN_CiTDC8_TDCO_Pos)
#define MCP251XFD_CAN_CiTDC8_TDCO_SET(value)                                   \
	(((uint8_t)(value) << MCP251XFD_CAN_CiTDC8_TDCO_Pos) &                     \
	 MCP251XFD_CAN_CiTDC8_TDCO_Mask) //!< Set the Transmitter Delay Compensation
	                                 //!< Offset bits; Secondary Sample Point
	                                 //!< (SSP). Two’s complement; offset can be
	                                 //!< positive, zero, or negative; value x
	                                 //!< TSYSCLK (min is –64 (0b1111111 x
	                                 //!< TSYSCLK ; max is 63 (0b0111111) x
	                                 //!< TSYSCLK)
#define MCP251XFD_CAN_CiTDC8_TDCO_GET(value)                                   \
	((int8_t)(((((uint8_t)(value) >> MCP251XFD_CAN_CiTDC8_TDCO_Pos) &          \
	            ((1 << MCP251XFD_CAN_CiTDC8_TDCO_BitWidth) - 1)) ^             \
	           (1 << (MCP251XFD_CAN_CiTDC8_TDCO_BitWidth - 1))) -              \
	          (1 << (MCP251XFD_CAN_CiTDC8_TDCO_BitWidth -                      \
	                 1)))) //!< Get the Transmitter Delay Compensation Offset
	                       //!< bits; Secondary Sample Point (SSP). Two’s
	                       //!< complement; offset can be positive, zero, or
	                       //!< negative; value x TSYSCLK (min is –64 (0b1111111
	                       //!< x TSYSCLK ; max is 63 (0b0111111) x TSYSCLK)

#define MCP251XFD_CAN_CiTDC8_TDCMOD_Pos 0
#define MCP251XFD_CAN_CiTDC8_TDCMOD_Mask                                       \
	(0x3u << MCP251XFD_CAN_CiTDC8_TDCMOD_Pos)
#define MCP251XFD_CAN_CiTDC8_TDCMOD_SET(value)                                 \
	(((uint8_t)(value) << MCP251XFD_CAN_CiTDC8_TDCMOD_Pos) &                   \
	 MCP251XFD_CAN_CiTDC8_TDCMOD_Mask) //!< Transmitter Delay Compensation Mode
	                                   //!< bits; Secondary Sample Point (SSP)
#define MCP251XFD_CAN_CiTDC8_SID11EN                                           \
	(0x1u << 0) //!< Enable 12-Bit SID in CAN FD Base Format Messages
#define MCP251XFD_CAN_CiTDC8_EDGFLTEN                                          \
	(0x1u << 1) //!< Enable Edge Filtering during Bus Integration state
#define MCP251XFD_CAN_CiTDC8_EDGFLTDIS                                         \
	(0x0u << 1) //!< Disable Edge Filtering during Bus Integration state

//-----------------------------------------------------------------------------
#define MCP251XFD_CAN_CiTBC_Pos 0
#define MCP251XFD_CAN_CiTBC_Mask (0xFFFFFFFFu << MCP251XFD_CAN_CiTBC_Pos)
#define MCP251XFD_CAN_CiTBC_SET(value)                                         \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTBC_Pos) &                          \
	 MCP251XFD_CAN_CiTBC_Mask) //!< Time Base Counter. This is a free running
	                           //!< timer that increments every TBCPRE clocks
	                           //!< when TBCEN is set

//-----------------------------------------------------------------------------
#define MCP251XFD_CAN_CiTSCON_TBCPRE_MINVALUE (0x00u)
#define MCP251XFD_CAN_CiTSCON_TBCPRE_Pos 0
#define MCP251XFD_CAN_CiTSCON_TBCPRE_Bits 10
#define MCP251XFD_CAN_CiTSCON_TBCPRE_MAXVALUE                                  \
	((1 << MCP251XFD_CAN_CiTSCON_TBCPRE_Bits) - 1)
#define MCP251XFD_CAN_CiTSCON_TBCPRE_Mask                                      \
	(MCP251XFD_CAN_CiTSCON_TBCPRE_MAXVALUE << MCP251XFD_CAN_CiTSCON_TBCPRE_Pos)
#define MCP251XFD_CAN_CiTSCON_TBCPRE_SET(value)                                \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTSCON_TBCPRE_Pos) &                 \
	 MCP251XFD_CAN_CiTSCON_TBCPRE_Mask) //!< Time Base Counter Prescaler bits.
	                                    //!< TBC increments every 'value'
	                                    //!< clocks
#define MCP251XFD_CAN_CiTSCON_TSSP_Pos 16
#define MCP251XFD_CAN_CiTSCON_TSSP_Mask (0x3 << MCP251XFD_CAN_CiTSCON_TSSP_Pos)
#define MCP251XFD_CAN_CiTSCON_TSSP_SET(value)                                  \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTSCON_TSSP_Pos) &                   \
	 MCP251XFD_CAN_CiTSCON_TSSP_Mask) //!< Time Stamp sample point position
#define MCP251XFD_CAN_CiTSCON_TBCEN (0x1u << 16)  //!< Time Base Counter Enable
#define MCP251XFD_CAN_CiTSCON_TBCDIS (0x0u << 16) //!< Time Base Counter Disable
#define MCP251XFD_CAN_CiTSCON_TIMESTAMP_SOF                                    \
	(0x0u << 17) //!< Time Stamp at "beginning" of Frame: Classical Frame: at
	             //!< sample point of SOF, FD Frame: see TSRES bit
#define MCP251XFD_CAN_CiTSCON_TIMESTAMP_EOF                                    \
	(0x1u                                                                      \
	 << 17) //!< Time Stamp when frame is taken valid: RX no error until last
	        //!< but one bit of EOF, TX no error until the end of EOF
#define MCP251XFD_CAN_CiTSCON_TIMESTAMPFD_SOF                                  \
	(0x0u << 18) //!< Time Stamp res (FD Frames only) at sample point of SOF
#define MCP251XFD_CAN_CiTSCON_TIMESTAMPFD_FDF                                  \
	(0x1u << 18) //!< Time Stamp res (FD Frames only) at sample point of the bit
	             //!< following the FDF bit

//*** Byte version access to Registers ***
#define MCP251XFD_CAN_CiTSCON16_TBCPRE_Pos 0
#define MCP251XFD_CAN_CiTSCON16_TBCPRE_Mask                                    \
	(MCP251XFD_CAN_CiTSCON_TBCPRE_Bits << MCP251XFD_CAN_CiTSCON16_TBCPRE_Pos)
#define MCP251XFD_CAN_CiTSCON16_TBCPRE_SET(value)                              \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTSCON16_TBCPRE_Pos) &               \
	 MCP251XFD_CAN_CiTSCON16_TBCPRE_Mask) //!< Time Base Counter Prescaler bits.
	                                      //!< TBC increments every 'value'
	                                      //!< clocks
#define MCP251XFD_CAN_CiTSCON8_TSSP_Pos 0
#define MCP251XFD_CAN_CiTSCON8_TSSP_Mask                                       \
	(0x3 << MCP251XFD_CAN_CiTSCON8_TSSP_Pos)
#define MCP251XFD_CAN_CiTSCON8_TSSP_SET(value)                                 \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTSCON8_TSSP_Pos) &                  \
	 MCP251XFD_CAN_CiTSCON8_TSSP_Mask) //!< Time Stamp sample point position
#define MCP251XFD_CAN_CiTSCON8_TBCEN (0x1u << 0)  //!< Time Base Counter Enable
#define MCP251XFD_CAN_CiTSCON8_TBCDIS (0x0u << 0) //!< Time Base Counter Disable
#define MCP251XFD_CAN_CiTSCON8_TIMESTAMP_SOF                                   \
	(0x0u << 1) //!< Time Stamp at "beginning" of Frame: Classical Frame: at
	            //!< sample point of SOF, FD Frame: see TSRES bit
#define MCP251XFD_CAN_CiTSCON8_TIMESTAMP_EOF                                   \
	(0x1u << 1) //!< Time Stamp when frame is taken valid: RX no error until
	            //!< last but one bit of EOF, TX no error until the end of EOF
#define MCP251XFD_CAN_CiTSCON8_TIMESTAMPFD_SOF                                 \
	(0x0u << 2) //!< Time Stamp res (FD Frames only) at sample point of SOF
#define MCP251XFD_CAN_CiTSCON8_TIMESTAMPFD_FDF                                 \
	(0x1u << 2) //!< Time Stamp res (FD Frames only) at sample point of the bit
	            //!< following the FDF bit

//-----------------------------------------------------------------------------
#define MCP251XFD_CAN_CiVEC_ICODE_Pos 0
#define MCP251XFD_CAN_CiVEC_ICODE_Mask (0x7Fu << MCP251XFD_CAN_CiVEC_ICODE_Pos)
#define MCP251XFD_CAN_CiVEC_ICODE_GET(value)                                   \
	(((uint32_t)(value) & MCP251XFD_CAN_CiVEC_ICODE_Mask) >>                   \
	 MCP251XFD_CAN_CiVEC_ICODE_Pos) //!< Interrupt Flag Code. If multiple
	                                //!< interrupts are pending, the interrupt
	                                //!< with the highest number will be
	                                //!< indicated
#define MCP251XFD_CAN_CiVEC_FILHIT_Pos 8
#define MCP251XFD_CAN_CiVEC_FILHIT_Mask                                        \
	(0x1Fu << MCP251XFD_CAN_CiVEC_FILHIT_Pos)
#define MCP251XFD_CAN_CiVEC_FILHIT_GET(value)                                  \
	(((uint32_t)(value) & MCP251XFD_CAN_CiVEC_FILHIT_Mask) >>                  \
	 MCP251XFD_CAN_CiVEC_FILHIT_Pos) //!< Filter Hit Number. If multiple
	                                 //!< interrupts are pending, the interrupt
	                                 //!< with the highest number will be
	                                 //!< indicated
#define MCP251XFD_CAN_CiVEC_TXCODE_Pos 16
#define MCP251XFD_CAN_CiVEC_TXCODE_Mask                                        \
	(0x7Fu << MCP251XFD_CAN_CiVEC_TXCODE_Pos)
#define MCP251XFD_CAN_CiVEC_TXCODE_GET(value)                                  \
	(((uint32_t)(value) & MCP251XFD_CAN_CiVEC_TXCODE_Mask) >>                  \
	 MCP251XFD_CAN_CiVEC_TXCODE_Pos) //!< Transmit Interrupt Flag Code. If
	                                 //!< multiple interrupts are pending, the
	                                 //!< interrupt with the highest number will
	                                 //!< be indicated
#define MCP251XFD_CAN_CiVEC_RXCODE_Pos 24
#define MCP251XFD_CAN_CiVEC_RXCODE_Mask                                        \
	(0x7Fu << MCP251XFD_CAN_CiVEC_RXCODE_Pos)
#define MCP251XFD_CAN_CiVEC_RXCODE_GET(value)                                  \
	(((uint32_t)(value) & MCP251XFD_CAN_CiVEC_RXCODE_Mask) >>                  \
	 MCP251XFD_CAN_CiVEC_RXCODE_Pos) //!< Receive Interrupt Flag Code. If
	                                 //!< multiple interrupts are pending, the
	                                 //!< interrupt with the highest number will
	                                 //!< be indicated

//*** Byte version access to Registers ***
#define MCP251XFD_CAN_CiVEC8_ICODE_Pos 0
#define MCP251XFD_CAN_CiVEC8_ICODE_Mask                                        \
	(0x7Fu << MCP251XFD_CAN_CiVEC8_ICODE_Pos)
#define MCP251XFD_CAN_CiVEC8_ICODE_GET(value)                                  \
	(((uint32_t)(value) & MCP251XFD_CAN_CiVEC8_ICODE_Mask) >>                  \
	 MCP251XFD_CAN_CiVEC8_ICODE_Pos) //!< Interrupt Flag Code. If multiple
	                                 //!< interrupts are pending, the interrupt
	                                 //!< with the highest number will be
	                                 //!< indicated
#define MCP251XFD_CAN_CiVEC8_FILHIT_Pos 0
#define MCP251XFD_CAN_CiVEC8_FILHIT_Mask                                       \
	(0x1Fu << MCP251XFD_CAN_CiVEC8_FILHIT_Pos)
#define MCP251XFD_CAN_CiVEC8_FILHIT_GET(value)                                 \
	(((uint32_t)(value) & MCP251XFD_CAN_CiVEC8_FILHIT_Mask) >>                 \
	 MCP251XFD_CAN_CiVEC8_FILHIT_Pos) //!< Filter Hit Number. If multiple
	                                  //!< interrupts are pending, the interrupt
	                                  //!< with the highest number will be
	                                  //!< indicated
#define MCP251XFD_CAN_CiVEC8_TXCODE_Pos 0
#define MCP251XFD_CAN_CiVEC8_TXCODE_Mask                                       \
	(0x7Fu << MCP251XFD_CAN_CiVEC8_TXCODE_Pos)
#define MCP251XFD_CAN_CiVEC8_TXCODE_GET(value)                                 \
	(((uint32_t)(value) & MCP251XFD_CAN_CiVEC8_TXCODE_Mask) >>                 \
	 MCP251XFD_CAN_CiVEC8_TXCODE_Pos) //!< Transmit Interrupt Flag Code. If
	                                  //!< multiple interrupts are pending, the
	                                  //!< interrupt with the highest number
	                                  //!< will be indicated
#define MCP251XFD_CAN_CiVEC8_RXCODE_Pos 0
#define MCP251XFD_CAN_CiVEC8_RXCODE_Mask                                       \
	(0x7Fu << MCP251XFD_CAN_CiVEC8_RXCODE_Pos)
#define MCP251XFD_CAN_CiVEC8_RXCODE_GET(value)                                 \
	(((uint32_t)(value) & MCP251XFD_CAN_CiVEC8_RXCODE_Mask) >>                 \
	 MCP251XFD_CAN_CiVEC8_RXCODE_Pos) //!< Receive Interrupt Flag Code. If
	                                  //!< multiple interrupts are pending, the
	                                  //!< interrupt with the highest number
	                                  //!< will be indicated

//-----------------------------------------------------------------------------

#define MCP251XFD_CAN_CiINT_TXIF (0x1u << 0) //! Transmit FIFO Interrupt Flag
#define MCP251XFD_CAN_CiINT_RXIF (0x1u << 1) //! Receive FIFO Interrupt Flag
#define MCP251XFD_CAN_CiINT_TBCIF                                              \
	(0x1u << 2) //! Time Base Counter Overflow Interrupt Flag
#define MCP251XFD_CAN_CiINT_MODIF                                              \
	(0x1u << 3) //! Operation Mode Change Interrupt Flag
#define MCP251XFD_CAN_CiINT_TEFIF                                              \
	(0x1u << 4) //! Transmit Event FIFO Interrupt Flag
#define MCP251XFD_CAN_CiINT_ECCIF (0x1u << 8) //! ECC Error Interrupt Flag
#define MCP251XFD_CAN_CiINT_SPICRCIF                                           \
	(0x1u << 9) //! SPI CRC Error Interrupt Flag
#define MCP251XFD_CAN_CiINT_TXATIF                                             \
	(0x1u << 10) //! Transmit Attempt Interrupt Flag
#define MCP251XFD_CAN_CiINT_RXOVIF                                             \
	(0x1u << 11) //! Receive Object Overflow Interrupt Flag
#define MCP251XFD_CAN_CiINT_SERRIF (0x1u << 12) //! System Error Interrupt Flag
#define MCP251XFD_CAN_CiINT_CERRIF (0x1u << 13) //! CAN Bus Error Interrupt Flag
#define MCP251XFD_CAN_CiINT_WAKIF (0x1u << 14)  //! Bus Wake Up Interrupt Flag
#define MCP251XFD_CAN_CiINT_IVMIF                                              \
	(0x1u << 15)                              //! Invalid Message Interrupt Flag
#define MCP251XFD_CAN_CiINT_TXIE (0x1u << 16) //! Transmit FIFO Interrupt Enable
#define MCP251XFD_CAN_CiINT_RXIE (0x1u << 17) //! Receive FIFO Interrupt Enable
#define MCP251XFD_CAN_CiINT_TBCIE                                              \
	(0x1u << 18) //! Time Base Counter Interrupt Enable
#define MCP251XFD_CAN_CiINT_MODIE (0x1u << 19) //! Mode Change Interrupt Enable
#define MCP251XFD_CAN_CiINT_TEFIE                                              \
	(0x1u << 20) //! Transmit Event FIFO Interrupt Enable
#define MCP251XFD_CAN_CiINT_ECCIE (0x1u << 24) //! ECC Error Interrupt Enable
#define MCP251XFD_CAN_CiINT_SPICRCIE                                           \
	(0x1u << 25) //! SPI CRC Error Interrupt Enable
#define MCP251XFD_CAN_CiINT_TXATIE                                             \
	(0x1u << 26) //! Transmit Attempt Interrupt Enable
#define MCP251XFD_CAN_CiINT_RXOVIE                                             \
	(0x1u << 27) //! Receive FIFO Overflow Interrupt Enable
#define MCP251XFD_CAN_CiINT_SERRIE                                             \
	(0x1u << 28) //! System Error Interrupt Enable
#define MCP251XFD_CAN_CiINT_CERRIE                                             \
	(0x1u << 29) //! CAN Bus Error Interrupt Enable
#define MCP251XFD_CAN_CiINT_WAKIE (0x1u << 30) //! Bus Wake Up Interrupt Enable
#define MCP251XFD_CAN_CiINT_IVMIE                                              \
	(0x1u << 31) //! Invalid Message Interrupt Enable

//*** Byte version access to Registers ***
#define MCP251XFD_CAN_CiINT8_TXIF (0x1u << 0) //! Transmit FIFO Interrupt Flag
#define MCP251XFD_CAN_CiINT8_RXIF (0x1u << 1) //! Receive FIFO Interrupt Flag
#define MCP251XFD_CAN_CiINT8_TBCIF                                             \
	(0x1u << 2) //! Time Base Counter Overflow Interrupt Flag
#define MCP251XFD_CAN_CiINT8_MODIF                                             \
	(0x1u << 3) //! Operation Mode Change Interrupt Flag
#define MCP251XFD_CAN_CiINT8_TEFIF                                             \
	(0x1u << 4) //! Transmit Event FIFO Interrupt Flag
#define MCP251XFD_CAN_CiINT8_ECCIF (0x1u << 0) //! ECC Error Interrupt Flag
#define MCP251XFD_CAN_CiINT8_SPICRCIF                                          \
	(0x1u << 1) //! SPI CRC Error Interrupt Flag
#define MCP251XFD_CAN_CiINT8_TXATIF                                            \
	(0x1u << 2) //! Transmit Attempt Interrupt Flag
#define MCP251XFD_CAN_CiINT8_RXOVIF                                            \
	(0x1u << 3) //! Receive Object Overflow Interrupt Flag
#define MCP251XFD_CAN_CiINT8_SERRIF (0x1u << 4) //! System Error Interrupt Flag
#define MCP251XFD_CAN_CiINT8_CERRIF (0x1u << 5) //! CAN Bus Error Interrupt Flag
#define MCP251XFD_CAN_CiINT8_WAKIF (0x1u << 6)  //! Bus Wake Up Interrupt Flag
#define MCP251XFD_CAN_CiINT8_IVMIF                                             \
	(0x1u << 7)                               //! Invalid Message Interrupt Flag
#define MCP251XFD_CAN_CiINT8_TXIE (0x1u << 0) //! Transmit FIFO Interrupt Enable
#define MCP251XFD_CAN_CiINT8_RXIE (0x1u << 1) //! Receive FIFO Interrupt Enable
#define MCP251XFD_CAN_CiINT8_TBCIE                                             \
	(0x1u << 2) //! Time Base Counter Interrupt Enable
#define MCP251XFD_CAN_CiINT8_MODIE (0x1u << 3) //! Mode Change Interrupt Enable
#define MCP251XFD_CAN_CiINT8_TEFIE                                             \
	(0x1u << 4) //! Transmit Event FIFO Interrupt Enable
#define MCP251XFD_CAN_CiINT8_ECCIE (0x1u << 0) //! ECC Error Interrupt Enable
#define MCP251XFD_CAN_CiINT8_SPICRCIE                                          \
	(0x1u << 1) //! SPI CRC Error Interrupt Enable
#define MCP251XFD_CAN_CiINT8_TXATIE                                            \
	(0x1u << 2) //! Transmit Attempt Interrupt Enable
#define MCP251XFD_CAN_CiINT8_RXOVIE                                            \
	(0x1u << 3) //! Receive FIFO Overflow Interrupt Enable
#define MCP251XFD_CAN_CiINT8_SERRIE                                            \
	(0x1u << 4) //! System Error Interrupt Enable
#define MCP251XFD_CAN_CiINT8_CERRIE                                            \
	(0x1u << 5) //! CAN Bus Error Interrupt Enable
#define MCP251XFD_CAN_CiINT8_WAKIE (0x1u << 6) //! Bus Wake Up Interrupt Enable
#define MCP251XFD_CAN_CiINT8_IVMIE                                             \
	(0x1u << 7) //! Invalid Message Interrupt Enable

//*** 2-Byte version access to Registers ***
#define MCP251XFD_CAN_CiINT16_TXIF (0x1u << 0) //! Transmit FIFO Interrupt Flag
#define MCP251XFD_CAN_CiINT16_RXIF (0x1u << 1) //! Receive FIFO Interrupt Flag
#define MCP251XFD_CAN_CiINT16_TBCIF                                            \
	(0x1u << 2) //! Time Base Counter Overflow Interrupt Flag
#define MCP251XFD_CAN_CiINT16_MODIF                                            \
	(0x1u << 3) //! Operation Mode Change Interrupt Flag
#define MCP251XFD_CAN_CiINT16_TEFIF                                            \
	(0x1u << 4) //! Transmit Event FIFO Interrupt Flag
#define MCP251XFD_CAN_CiINT16_ECCIF (0x1u << 8) //! ECC Error Interrupt Flag
#define MCP251XFD_CAN_CiINT16_SPICRCIF                                         \
	(0x1u << 9) //! SPI CRC Error Interrupt Flag
#define MCP251XFD_CAN_CiINT16_TXATIF                                           \
	(0x1u << 10) //! Transmit Attempt Interrupt Flag
#define MCP251XFD_CAN_CiINT16_RXOVIF                                           \
	(0x1u << 11) //! Receive Object Overflow Interrupt Flag
#define MCP251XFD_CAN_CiINT16_SERRIF                                           \
	(0x1u << 12) //! System Error Interrupt Flag
#define MCP251XFD_CAN_CiINT16_CERRIF                                           \
	(0x1u << 13) //! CAN Bus Error Interrupt Flag
#define MCP251XFD_CAN_CiINT16_WAKIF (0x1u << 14) //! Bus Wake Up Interrupt Flag
#define MCP251XFD_CAN_CiINT16_IVMIF                                            \
	(0x1u << 15) //! Invalid Message Interrupt Flag
#define MCP251XFD_CAN_CiINT16_TXIE                                             \
	(0x1u << 0) //! Transmit FIFO Interrupt Enable
#define MCP251XFD_CAN_CiINT16_RXIE (0x1u << 1) //! Receive FIFO Interrupt Enable
#define MCP251XFD_CAN_CiINT16_TBCIE                                            \
	(0x1u << 2) //! Time Base Counter Interrupt Enable
#define MCP251XFD_CAN_CiINT16_MODIE (0x1u << 3) //! Mode Change Interrupt Enable
#define MCP251XFD_CAN_CiINT16_TEFIE                                            \
	(0x1u << 4) //! Transmit Event FIFO Interrupt Enable
#define MCP251XFD_CAN_CiINT16_ECCIE (0x1u << 8) //! ECC Error Interrupt Enable
#define MCP251XFD_CAN_CiINT16_SPICRCIE                                         \
	(0x1u << 9) //! SPI CRC Error Interrupt Enable
#define MCP251XFD_CAN_CiINT16_TXATIE                                           \
	(0x1u << 10) //! Transmit Attempt Interrupt Enable
#define MCP251XFD_CAN_CiINT16_RXOVIE                                           \
	(0x1u << 11) //! Receive FIFO Overflow Interrupt Enable
#define MCP251XFD_CAN_CiINT16_SERRIE                                           \
	(0x1u << 12) //! System Error Interrupt Enable
#define MCP251XFD_CAN_CiINT16_CERRIE                                           \
	(0x1u << 13) //! CAN Bus Error Interrupt Enable
#define MCP251XFD_CAN_CiINT16_WAKIE                                            \
	(0x1u << 14) //! Bus Wake Up Interrupt Enable
#define MCP251XFD_CAN_CiINT16_IVMIE                                            \
	(0x1u << 15) //! Invalid Message Interrupt Enable

#define MCP251XFD_CAN_INT_ALL_INT                                              \
	(MCP251XFD_CAN_CiINT16_TXIE | MCP251XFD_CAN_CiINT16_RXIE |                 \
	 MCP251XFD_CAN_CiINT16_TEFIE | MCP251XFD_CAN_CiINT16_TXATIE |              \
	 MCP251XFD_CAN_CiINT16_RXOVIE | MCP251XFD_CAN_CiINT16_TBCIE |              \
	 MCP251XFD_CAN_CiINT16_MODIE | MCP251XFD_CAN_CiINT16_ECCIE |               \
	 MCP251XFD_CAN_CiINT16_SPICRCIE | MCP251XFD_CAN_CiINT16_SERRIE |           \
	 MCP251XFD_CAN_CiINT16_CERRIE | MCP251XFD_CAN_CiINT16_WAKIE |              \
	 MCP251XFD_CAN_CiINT16_IVMIE)
#define MCP251XFD_CAN_INT_CLEARABLE_FLAGS                                      \
	(MCP251XFD_CAN_CiINT16_TBCIE | MCP251XFD_CAN_CiINT16_MODIE |               \
	 MCP251XFD_CAN_CiINT16_SERRIE | MCP251XFD_CAN_CiINT16_CERRIE |             \
	 MCP251XFD_CAN_CiINT16_WAKIE | MCP251XFD_CAN_CiINT16_IVMIE)

#define MCP251XFD_CAN_CiRXIF_RFIF1                                             \
	(0x1u << 1) //!< Receive FIFO  1 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF2                                             \
	(0x1u << 2) //!< Receive FIFO  2 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF3                                             \
	(0x1u << 3) //!< Receive FIFO  3 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF4                                             \
	(0x1u << 4) //!< Receive FIFO  4 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF5                                             \
	(0x1u << 5) //!< Receive FIFO  5 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF6                                             \
	(0x1u << 6) //!< Receive FIFO  6 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF7                                             \
	(0x1u << 7) //!< Receive FIFO  7 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF8                                             \
	(0x1u << 8) //!< Receive FIFO  8 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF9                                             \
	(0x1u << 9) //!< Receive FIFO  9 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF10                                            \
	(0x1u << 10) //!< Receive FIFO 10 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF11                                            \
	(0x1u << 11) //!< Receive FIFO 11 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF12                                            \
	(0x1u << 12) //!< Receive FIFO 12 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF13                                            \
	(0x1u << 13) //!< Receive FIFO 13 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF14                                            \
	(0x1u << 14) //!< Receive FIFO 14 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF15                                            \
	(0x1u << 15) //!< Receive FIFO 15 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF16                                            \
	(0x1u << 16) //!< Receive FIFO 16 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF17                                            \
	(0x1u << 17) //!< Receive FIFO 17 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF18                                            \
	(0x1u << 18) //!< Receive FIFO 18 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF19                                            \
	(0x1u << 19) //!< Receive FIFO 19 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF20                                            \
	(0x1u << 20) //!< Receive FIFO 20 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF21                                            \
	(0x1u << 21) //!< Receive FIFO 21 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF22                                            \
	(0x1u << 22) //!< Receive FIFO 22 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF23                                            \
	(0x1u << 23) //!< Receive FIFO 23 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF24                                            \
	(0x1u << 24) //!< Receive FIFO 24 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF25                                            \
	(0x1u << 25) //!< Receive FIFO 25 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF26                                            \
	(0x1u << 26) //!< Receive FIFO 26 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF27                                            \
	(0x1u << 27) //!< Receive FIFO 27 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF28                                            \
	(0x1u << 28) //!< Receive FIFO 28 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF29                                            \
	(0x1u << 29) //!< Receive FIFO 29 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF30                                            \
	(0x1u << 30) //!< Receive FIFO 30 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_RFIF31                                            \
	(0x1u << 31) //!< Receive FIFO 31 Interrupt Pending

//-----------------------------------------------------------------------------

#define MCP251XFD_CAN_CiRXOVIF_RFOVIF1                                         \
	(0x1u << 1) //!< Receive FIFO  1 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF2                                         \
	(0x1u << 2) //!< Receive FIFO  2 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF3                                         \
	(0x1u << 3) //!< Receive FIFO  3 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF4                                         \
	(0x1u << 4) //!< Receive FIFO  4 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF5                                         \
	(0x1u << 5) //!< Receive FIFO  5 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF6                                         \
	(0x1u << 6) //!< Receive FIFO  6 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF7                                         \
	(0x1u << 7) //!< Receive FIFO  7 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF8                                         \
	(0x1u << 8) //!< Receive FIFO  8 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF9                                         \
	(0x1u << 9) //!< Receive FIFO  9 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF10                                        \
	(0x1u << 10) //!< Receive FIFO 10 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF11                                        \
	(0x1u << 11) //!< Receive FIFO 11 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF12                                        \
	(0x1u << 12) //!< Receive FIFO 12 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF13                                        \
	(0x1u << 13) //!< Receive FIFO 13 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF14                                        \
	(0x1u << 14) //!< Receive FIFO 14 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF15                                        \
	(0x1u << 15) //!< Receive FIFO 15 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF16                                        \
	(0x1u << 16) //!< Receive FIFO 16 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF17                                        \
	(0x1u << 17) //!< Receive FIFO 17 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF18                                        \
	(0x1u << 18) //!< Receive FIFO 18 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF19                                        \
	(0x1u << 19) //!< Receive FIFO 19 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF20                                        \
	(0x1u << 20) //!< Receive FIFO 20 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF21                                        \
	(0x1u << 21) //!< Receive FIFO 21 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF22                                        \
	(0x1u << 22) //!< Receive FIFO 22 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF23                                        \
	(0x1u << 23) //!< Receive FIFO 23 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF24                                        \
	(0x1u << 24) //!< Receive FIFO 24 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF25                                        \
	(0x1u << 25) //!< Receive FIFO 25 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF26                                        \
	(0x1u << 26) //!< Receive FIFO 26 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF27                                        \
	(0x1u << 27) //!< Receive FIFO 27 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF28                                        \
	(0x1u << 28) //!< Receive FIFO 28 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF29                                        \
	(0x1u << 29) //!< Receive FIFO 29 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF30                                        \
	(0x1u << 30) //!< Receive FIFO 30 Overflow Interrupt Pending
#define MCP251XFD_CAN_CiRXOVIF_RFOVIF31                                        \
	(0x1u << 31) //!< Receive FIFO 31 Overflow Interrupt Pending

//-----------------------------------------------------------------------------
#define MCP251XFD_CAN_CiRXIF_TFIF0                                             \
	(0x1u << 0) //!< Transmit TXQ Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF1                                             \
	(0x1u << 1) //!< Transmit FIFO  1 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF2                                             \
	(0x1u << 2) //!< Transmit FIFO  2 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF3                                             \
	(0x1u << 3) //!< Transmit FIFO  3 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF4                                             \
	(0x1u << 4) //!< Transmit FIFO  4 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF5                                             \
	(0x1u << 5) //!< Transmit FIFO  5 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF6                                             \
	(0x1u << 6) //!< Transmit FIFO  6 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF7                                             \
	(0x1u << 7) //!< Transmit FIFO  7 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF8                                             \
	(0x1u << 8) //!< Transmit FIFO  8 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF9                                             \
	(0x1u << 9) //!< Transmit FIFO  9 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF10                                            \
	(0x1u << 10) //!< Transmit FIFO 10 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF11                                            \
	(0x1u << 11) //!< Transmit FIFO 11 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF12                                            \
	(0x1u << 12) //!< Transmit FIFO 12 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF13                                            \
	(0x1u << 13) //!< Transmit FIFO 13 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF14                                            \
	(0x1u << 14) //!< Transmit FIFO 14 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF15                                            \
	(0x1u << 15) //!< Transmit FIFO 15 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF16                                            \
	(0x1u << 16) //!< Transmit FIFO 16 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF17                                            \
	(0x1u << 17) //!< Transmit FIFO 17 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF18                                            \
	(0x1u << 18) //!< Transmit FIFO 18 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF19                                            \
	(0x1u << 19) //!< Transmit FIFO 19 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF20                                            \
	(0x1u << 20) //!< Transmit FIFO 20 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF21                                            \
	(0x1u << 21) //!< Transmit FIFO 21 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF22                                            \
	(0x1u << 22) //!< Transmit FIFO 22 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF23                                            \
	(0x1u << 23) //!< Transmit FIFO 23 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF24                                            \
	(0x1u << 24) //!< Transmit FIFO 24 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF25                                            \
	(0x1u << 25) //!< Transmit FIFO 25 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF26                                            \
	(0x1u << 26) //!< Transmit FIFO 26 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF27                                            \
	(0x1u << 27) //!< Transmit FIFO 27 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF28                                            \
	(0x1u << 28) //!< Transmit FIFO 28 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF29                                            \
	(0x1u << 29) //!< Transmit FIFO 29 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF30                                            \
	(0x1u << 30) //!< Transmit FIFO 30 Interrupt Pending
#define MCP251XFD_CAN_CiRXIF_TFIF31                                            \
	(0x1u << 31) //!< Transmit FIFO 31 Interrupt Pending

//-----------------------------------------------------------------------------

#define MCP251XFD_CAN_CiTXATIF_TFATIF0                                         \
	(0x1u << 0) //!< Transmit TXQ Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF1                                         \
	(0x1u << 1) //!< Transmit FIFO  1 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF2                                         \
	(0x1u << 2) //!< Transmit FIFO  2 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF3                                         \
	(0x1u << 3) //!< Transmit FIFO  3 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF4                                         \
	(0x1u << 4) //!< Transmit FIFO  4 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF5                                         \
	(0x1u << 5) //!< Transmit FIFO  5 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF6                                         \
	(0x1u << 6) //!< Transmit FIFO  6 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF7                                         \
	(0x1u << 7) //!< Transmit FIFO  7 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF8                                         \
	(0x1u << 8) //!< Transmit FIFO  8 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF9                                         \
	(0x1u << 9) //!< Transmit FIFO  9 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF10                                        \
	(0x1u << 10) //!< Transmit FIFO 10 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF11                                        \
	(0x1u << 11) //!< Transmit FIFO 11 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF12                                        \
	(0x1u << 12) //!< Transmit FIFO 12 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF13                                        \
	(0x1u << 13) //!< Transmit FIFO 13 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF14                                        \
	(0x1u << 14) //!< Transmit FIFO 14 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF15                                        \
	(0x1u << 15) //!< Transmit FIFO 15 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF16                                        \
	(0x1u << 16) //!< Transmit FIFO 16 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF17                                        \
	(0x1u << 17) //!< Transmit FIFO 17 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF18                                        \
	(0x1u << 18) //!< Transmit FIFO 18 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF19                                        \
	(0x1u << 19) //!< Transmit FIFO 19 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF20                                        \
	(0x1u << 20) //!< Transmit FIFO 20 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF21                                        \
	(0x1u << 21) //!< Transmit FIFO 21 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF22                                        \
	(0x1u << 22) //!< Transmit FIFO 22 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF23                                        \
	(0x1u << 23) //!< Transmit FIFO 23 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF24                                        \
	(0x1u << 24) //!< Transmit FIFO 24 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF25                                        \
	(0x1u << 25) //!< Transmit FIFO 25 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF26                                        \
	(0x1u << 26) //!< Transmit FIFO 26 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF27                                        \
	(0x1u << 27) //!< Transmit FIFO 27 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF28                                        \
	(0x1u << 28) //!< Transmit FIFO 28 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF29                                        \
	(0x1u << 29) //!< Transmit FIFO 29 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF30                                        \
	(0x1u << 30) //!< Transmit FIFO 30 Attempt Interrupt Pending
#define MCP251XFD_CAN_CiTXATIF_TFATIF31                                        \
	(0x1u << 31) //!< Transmit FIFO 31 Attempt Interrupt Pending

//-----------------------------------------------------------------------------

#define MCP251XFD_CAN_CiTXREQ_TXREQ0                                           \
	(0x1u << 0) //!< Transmit Queue Message Send Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ1                                           \
	(0x1u << 1) //!< Message Send FIFO  1 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ2                                           \
	(0x1u << 2) //!< Message Send FIFO  2 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ3                                           \
	(0x1u << 3) //!< Message Send FIFO  3 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ4                                           \
	(0x1u << 4) //!< Message Send FIFO  4 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ5                                           \
	(0x1u << 5) //!< Message Send FIFO  5 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ6                                           \
	(0x1u << 6) //!< Message Send FIFO  6 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ7                                           \
	(0x1u << 7) //!< Message Send FIFO  7 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ8                                           \
	(0x1u << 8) //!< Message Send FIFO  8 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ9                                           \
	(0x1u << 9) //!< Message Send FIFO  9 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ10                                          \
	(0x1u << 10) //!< Message Send FIFO 10 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ11                                          \
	(0x1u << 11) //!< Message Send FIFO 11 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ12                                          \
	(0x1u << 12) //!< Message Send FIFO 12 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ13                                          \
	(0x1u << 13) //!< Message Send FIFO 13 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ14                                          \
	(0x1u << 14) //!< Message Send FIFO 14 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ15                                          \
	(0x1u << 15) //!< Message Send FIFO 15 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ16                                          \
	(0x1u << 16) //!< Message Send FIFO 16 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ17                                          \
	(0x1u << 17) //!< Message Send FIFO 17 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ18                                          \
	(0x1u << 18) //!< Message Send FIFO 18 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ19                                          \
	(0x1u << 19) //!< Message Send FIFO 19 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ20                                          \
	(0x1u << 20) //!< Message Send FIFO 20 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ21                                          \
	(0x1u << 21) //!< Message Send FIFO 21 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ22                                          \
	(0x1u << 22) //!< Message Send FIFO 22 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ23                                          \
	(0x1u << 23) //!< Message Send FIFO 23 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ24                                          \
	(0x1u << 24) //!< Message Send FIFO 24 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ25                                          \
	(0x1u << 25) //!< Message Send FIFO 25 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ26                                          \
	(0x1u << 26) //!< Message Send FIFO 26 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ27                                          \
	(0x1u << 27) //!< Message Send FIFO 27 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ28                                          \
	(0x1u << 28) //!< Message Send FIFO 28 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ29                                          \
	(0x1u << 29) //!< Message Send FIFO 29 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ30                                          \
	(0x1u << 30) //!< Message Send FIFO 30 Request
#define MCP251XFD_CAN_CiTXREQ_TXREQ31                                          \
	(0x1u << 31) //!< Message Send FIFO 31 Request

//-----------------------------------------------------------------------------

#define MCP251XFD_CAN_CiTREC_REC_Pos 0
#define MCP251XFD_CAN_CiTREC_REC_Mask (0xFFu << MCP251XFD_CAN_CiTREC_REC_Pos)
#define MCP251XFD_CAN_CiTREC_REC_SET(value)                                    \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTREC_REC_Pos) &                     \
	 MCP251XFD_CAN_CiTREC_REC_Mask) //!< Receive Error Counter
#define MCP251XFD_CAN_CiTREC_TEC_Pos 8
#define MCP251XFD_CAN_CiTREC_TEC_Mask (0xFFu << MCP251XFD_CAN_CiTREC_TEC_Pos)
#define MCP251XFD_CAN_CiTREC_TEC_SET(value)                                    \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTREC_TEC_Pos) &                     \
	 MCP251XFD_CAN_CiTREC_TEC_Mask) //!< Transmit Error Counter
#define MCP251XFD_CAN_CiTREC_EWARN                                             \
	(0x1u << 16) //!< Transmitter or Receiver is in Error Warning State
#define MCP251XFD_CAN_CiTREC_RXWARN                                            \
	(0x1u << 17) //!< Receiver in Error Warning State (128 > REC > 95)
#define MCP251XFD_CAN_CiTREC_TXWARN                                            \
	(0x1u << 18) //!< Transmitter in Error Warning State (128 > TEC > 95)
#define MCP251XFD_CAN_CiTREC_RXBP                                              \
	(0x1u << 19) //!< Receiver in Error Passive State (REC > 127)
#define MCP251XFD_CAN_CiTREC_TXBP                                              \
	(0x1u << 20) //!< Transmitter in Error Passive State (TEC > 127)
#define MCP251XFD_CAN_CiTREC_TXBO                                              \
	(0x1u << 21) //!< Transmitter in Bus Off State (TEC > 255). In Configuration
	             //!< mode, TXBO is set, since the module is not on the bus

//*** Byte version access to Registers ***
#define MCP251XFD_CAN_CiTREC8_EWARN                                            \
	(0x1u << 0) //!< Transmitter or Receiver is in Error Warning State
#define MCP251XFD_CAN_CiTREC8_RXWARN                                           \
	(0x1u << 1) //!< Receiver in Error Warning State (128 > REC > 95)
#define MCP251XFD_CAN_CiTREC8_TXWARN                                           \
	(0x1u << 2) //!< Transmitter in Error Warning State (128 > TEC > 95)
#define MCP251XFD_CAN_CiTREC8_RXBP                                             \
	(0x1u << 3) //!< Receiver in Error Passive State (REC > 127)
#define MCP251XFD_CAN_CiTREC8_TXBP                                             \
	(0x1u << 4) //!< Transmitter in Error Passive State (TEC > 127)
#define MCP251XFD_CAN_CiTREC8_TXBO                                             \
	(0x1u << 5) //!< Transmitter in Bus Off State (TEC > 255). In Configuration
	            //!< mode, TXBO is set, since the module is not on the bus

#define MCP251XFD_CAN_CiTREC8_TX_ERROR                                         \
	(MCP251XFD_CAN_CiTREC8_EWARN | MCP251XFD_CAN_CiTREC8_TXWARN |              \
	 MCP251XFD_CAN_CiTREC8_TXBP | MCP251XFD_CAN_CiTREC8_TXBO)
#define MCP251XFD_CAN_CiTREC8_RX_ERROR                                         \
	(MCP251XFD_CAN_CiTREC8_EWARN | MCP251XFD_CAN_CiTREC8_RXWARN |              \
	 MCP251XFD_CAN_CiTREC8_RXBP)
#define MCP251XFD_CAN_CiTREC8_ALL_ERROR                                        \
	(MCP251XFD_CAN_CiTREC8_TX_ERROR | MCP251XFD_CAN_CiTREC8_RX_ERROR)

#define MCP251XFD_CAN_CiBDIAG0_NRERRCNT_Pos 0
#define MCP251XFD_CAN_CiBDIAG0_NRERRCNT_Mask                                   \
	(0xFFu << MCP251XFD_CAN_CiBDIAG0_NRERRCNT_Pos)
#define MCP251XFD_CAN_CiBDIAG0_NRERRCNT_GET(value)                             \
	(((uint32_t)(value) & MCP251XFD_CAN_CiBDIAG0_NRERRCNT_Mask) >>             \
	 MCP251XFD_CAN_CiBDIAG0_NRERRCNT_Pos) //!< Nominal Bit Rate Receive Error
	                                      //!< Counter
#define MCP251XFD_CAN_CiBDIAG0_NTERRCNT_Pos 8
#define MCP251XFD_CAN_CiBDIAG0_NTERRCNT_Mask                                   \
	(0xFFu << MCP251XFD_CAN_CiBDIAG0_NTERRCNT_Pos)
#define MCP251XFD_CAN_CiBDIAG0_NTERRCNT_GET(value)                             \
	(((uint32_t)(value) & MCP251XFD_CAN_CiBDIAG0_NTERRCNT_Mask) >>             \
	 MCP251XFD_CAN_CiBDIAG0_NTERRCNT_Pos) //!< Nominal Bit Rate Transmit Error
	                                      //!< Counter
#define MCP251XFD_CAN_CiBDIAG0_DRERRCNT_Pos 16
#define MCP251XFD_CAN_CiBDIAG0_DRERRCNT_Mask                                   \
	(0xFFu << MCP251XFD_CAN_CiBDIAG0_DRERRCNT_Pos)
#define MCP251XFD_CAN_CiBDIAG0_DRERRCNT_GET(value)                             \
	(((uint32_t)(value) & MCP251XFD_CAN_CiBDIAG0_DRERRCNT_Mask) >>             \
	 MCP251XFD_CAN_CiBDIAG0_DRERRCNT_Pos) //!< Data Bit Rate Receive Error
	                                      //!< Counter
#define MCP251XFD_CAN_CiBDIAG0_DTERRCNT_Pos 24
#define MCP251XFD_CAN_CiBDIAG0_DTERRCNT_Mask                                   \
	(0xFFu << MCP251XFD_CAN_CiBDIAG0_DTERRCNT_Pos)
#define MCP251XFD_CAN_CiBDIAG0_DTERRCNT_GET(value)                             \
	(((uint32_t)(value) & MCP251XFD_CAN_CiBDIAG0_DTERRCNT_Mask) >>             \
	 MCP251XFD_CAN_CiBDIAG0_DTERRCNT_Pos) //!< Data Bit Rate Transmit Error
	                                      //!< Counter

#define MCP251XFD_CAN_CiBDIAG1_EFMSGCNT_Pos 0
#define MCP251XFD_CAN_CiBDIAG1_EFMSGCNT_Mask                                   \
	(0xFFFFu << MCP251XFD_CAN_CiBDIAG1_EFMSGCNT_Pos)
#define MCP251XFD_CAN_CiBDIAG1_EFMSGCNT_GET(value)                             \
	(((uint32_t)(value) & MCP251XFD_CAN_CiBDIAG1_EFMSGCNT_Mask) >>             \
	 MCP251XFD_CAN_CiBDIAG1_EFMSGCNT_Pos) //!< Error Free Message Counter
#define MCP251XFD_CAN_CiBDIAG1_NBIT0ERR                                        \
	(0x1u                                                                      \
	 << 16) //!< Normal Bitrate: During the transmission of a message (or
	        //!< acknowledge bit, or active error flag, or overload flag), the
	        //!< device wanted to send a dominant level (data or identifier bit
	        //!< logical value ‘0’), but the monitored bus value was recessive
#define MCP251XFD_CAN_CiBDIAG1_NBIT1ERR                                        \
	(0x1u << 17) //!< Normal Bitrate: During the transmission of a message (with
	             //!< the exception of the arbitration field), the device wanted
	             //!< to send a recessive level (bit of logical value '1'), but
	             //!< the monitored bus value was dominant
#define MCP251XFD_CAN_CiBDIAG1_NACKERR                                         \
	(0x1u << 18) //!< Normal Bitrate: Transmitted message was not acknowledged
#define MCP251XFD_CAN_CiBDIAG1_NFORMERR                                        \
	(0x1u << 19) //!< Normal Bitrate: A fixed format part of a received frame
	             //!< has the wrong format
#define MCP251XFD_CAN_CiBDIAG1_NSTUFERR                                        \
	(0x1u << 20) //!< Normal Bitrate: More than 5 equal bits in a sequence have
	             //!< occurred in a part of a received message where this is not
	             //!< allowed
#define MCP251XFD_CAN_CiBDIAG1_NCRCERR                                         \
	(0x1u << 21) //!< Normal Bitrate: The CRC check sum of a received message
	             //!< was incorrect. The CRC of an incoming message does not
	             //!< match with the CRC calculated from the received data
#define MCP251XFD_CAN_CiBDIAG1_TXBOERR                                         \
	(0x1u << 23) //!< Device went to bus-off (and auto-recovered)
#define MCP251XFD_CAN_CiBDIAG1_DBIT0ERR                                        \
	(0x1u                                                                      \
	 << 24) //!< Data Bitrate: During the transmission of a message (or
	        //!< acknowledge bit, or active error flag, or overload flag), the
	        //!< device wanted to send a dominant level (data or identifier bit
	        //!< logical value ‘0’), but the monitored bus value was recessive
#define MCP251XFD_CAN_CiBDIAG1_DBIT1ERR                                        \
	(0x1u << 25) //!< Data Bitrate: During the transmission of a message (with
	             //!< the exception of the arbitration field), the device wanted
	             //!< to send a recessive level (bit of logical value '1'), but
	             //!< the monitored bus value was dominant
#define MCP251XFD_CAN_CiBDIAG1_DFORMERR                                        \
	(0x1u << 27) //!< Data Bitrate: A fixed format part of a received frame has
	             //!< the wrong format
#define MCP251XFD_CAN_CiBDIAG1_DSTUFERR                                        \
	(0x1u << 28) //!< Data Bitrate: More than 5 equal bits in a sequence have
	             //!< occurred in a part of a received message where this is not
	             //!< allowed
#define MCP251XFD_CAN_CiBDIAG1_DCRCERR                                         \
	(0x1u << 29) //!< Data Bitrate: The CRC check sum of a received message was
	             //!< incorrect. The CRC of an incoming message does not match
	             //!< with the CRC calculated from the received data
#define MCP251XFD_CAN_CiBDIAG1_ESI                                             \
	(0x1u << 30) //!< ESI flag of a received CAN FD message was set
#define MCP251XFD_CAN_CiBDIAG1_DLCMM                                           \
	(0x1u                                                                      \
	 << 31) //!< DLC Mismatch bit. During a transmission or reception, the
	        //!< specified DLC is larger than the PLSIZE of the FIFO element

//*** Byte version access to Registers ***
#define MCP251XFD_CAN_CiBDIAG18_NBIT0ERR                                       \
	(0x1u                                                                      \
	 << 0) //!< Normal Bitrate: During the transmission of a message (or
	       //!< acknowledge bit, or active error flag, or overload flag), the
	       //!< device wanted to send a dominant level (data or identifier bit
	       //!< logical value ‘0’), but the monitored bus value was recessive
#define MCP251XFD_CAN_CiBDIAG18_NBIT1ERR                                       \
	(0x1u << 1) //!< Normal Bitrate: During the transmission of a message (with
	            //!< the exception of the arbitration field), the device wanted
	            //!< to send a recessive level (bit of logical value '1'), but
	            //!< the monitored bus value was dominant
#define MCP251XFD_CAN_CiBDIAG18_NACKERR                                        \
	(0x1u << 2) //!< Normal Bitrate: Transmitted message was not acknowledged
#define MCP251XFD_CAN_CiBDIAG18_NFORMERR                                       \
	(0x1u << 3) //!< Normal Bitrate: A fixed format part of a received frame has
	            //!< the wrong format
#define MCP251XFD_CAN_CiBDIAG18_NSTUFERR                                       \
	(0x1u << 4) //!< Normal Bitrate: More than 5 equal bits in a sequence have
	            //!< occurred in a part of a received message where this is not
	            //!< allowed
#define MCP251XFD_CAN_CiBDIAG18_NCRCERR                                        \
	(0x1u << 5) //!< Normal Bitrate: The CRC check sum of a received message was
	            //!< incorrect. The CRC of an incoming message does not match
	            //!< with the CRC calculated from the received data
#define MCP251XFD_CAN_CiBDIAG18_TXBOERR                                        \
	(0x1u << 7) //!< Device went to bus-off (and auto-recovered)
#define MCP251XFD_CAN_CiBDIAG18_DBIT0ERR                                       \
	(0x1u                                                                      \
	 << 0) //!< Data Bitrate: During the transmission of a message (or
	       //!< acknowledge bit, or active error flag, or overload flag), the
	       //!< device wanted to send a dominant level (data or identifier bit
	       //!< logical value ‘0’), but the monitored bus value was recessive
#define MCP251XFD_CAN_CiBDIAG18_DBIT1ERR                                       \
	(0x1u << 1) //!< Data Bitrate: During the transmission of a message (with
	            //!< the exception of the arbitration field), the device wanted
	            //!< to send a recessive level (bit of logical value '1'), but
	            //!< the monitored bus value was dominant
#define MCP251XFD_CAN_CiBDIAG18_DFORMERR                                       \
	(0x1u << 3) //!< Data Bitrate: A fixed format part of a received frame has
	            //!< the wrong format
#define MCP251XFD_CAN_CiBDIAG18_DSTUFERR                                       \
	(0x1u                                                                      \
	 << 4) //!< Data Bitrate: More than 5 equal bits in a sequence have occurred
	       //!< in a part of a received message where this is not allowed
#define MCP251XFD_CAN_CiBDIAG18_DCRCERR                                        \
	(0x1u << 5) //!< Data Bitrate: The CRC check sum of a received message was
	            //!< incorrect. The CRC of an incoming message does not match
	            //!< with the CRC calculated from the received data
#define MCP251XFD_CAN_CiBDIAG18_ESI                                            \
	(0x1u << 6) //!< ESI flag of a received CAN FD message was set
#define MCP251XFD_CAN_CiBDIAG18_DLCMM                                          \
	(0x1u                                                                      \
	 << 7) //!< DLC Mismatch bit. During a transmission or reception, the
	       //!< specified DLC is larger than the PLSIZE of the FIFO element

//-----------------------------------------------------------------------------

#define MCP251XFD_CAN_CiTEFCON_TEFNEIE                                         \
	(0x1u << 0) //!< Transmit Event FIFO Not Empty Interrupt Enable
#define MCP251XFD_CAN_CiTEFCON_TEFHIE                                          \
	(0x1u << 1) //!< Transmit Event FIFO Half Full Interrupt Enable
#define MCP251XFD_CAN_CiTEFCON_TEFFIE                                          \
	(0x1u << 2) //!< Transmit Event FIFO Full Interrupt Enable
#define MCP251XFD_CAN_CiTEFCON_TEFOVIE                                         \
	(0x1u << 3) //!< Transmit Event FIFO Overflow Interrupt Enable
#define MCP251XFD_CAN_CiTEFCON_TEFTSEN                                         \
	(0x1u << 5) //!< Transmit Event FIFO Time Stamp Enable
#define MCP251XFD_CAN_CiTEFCON_UINC (0x1u << 8)    //!< Increment Tail
#define MCP251XFD_CAN_CiTEFCON_FRESET (0x1u << 10) //!< FIFO Reset

#define MCP251XFD_CAN_CiTEFCON_FSIZE_Pos 24
#define MCP251XFD_CAN_CiTEFCON_FSIZE_Mask                                      \
	(0x1Fu << MCP251XFD_CAN_CiTEFCON_FSIZE_Pos)
#define MCP251XFD_CAN_CiTEFCON_FSIZE_SET(value)                                \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTEFCON_FSIZE_Pos) &                 \
	 MCP251XFD_CAN_CiTEFCON_FSIZE_Mask) //!< FIFO Size

//*** Byte version access to Registers ***
#define MCP251XFD_CAN_CiTEFCON8_TEFNEIE                                        \
	(0x1u << 0) //!< Transmit Event FIFO Not Empty Interrupt Enable
#define MCP251XFD_CAN_CiTEFCON8_TEFHIE                                         \
	(0x1u << 1) //!< Transmit Event FIFO Half Full Interrupt Enable
#define MCP251XFD_CAN_CiTEFCON8_TEFFIE                                         \
	(0x1u << 2) //!< Transmit Event FIFO Full Interrupt Enable
#define MCP251XFD_CAN_CiTEFCON8_TEFOVIE                                        \
	(0x1u << 3) //!< Transmit Event FIFO Overflow Interrupt Enable
#define MCP251XFD_CAN_CiTEFCON8_TEFTSEN                                        \
	(0x1u << 5) //!< Transmit Event FIFO Time Stamp Enable
#define MCP251XFD_CAN_CiTEFCON8_UINC (0x1u << 0)   //!< Increment Tail
#define MCP251XFD_CAN_CiTEFCON8_FRESET (0x1u << 2) //!< FIFO Reset
#define MCP251XFD_CAN_CiTEFCON8_FSIZE_Pos 0
#define MCP251XFD_CAN_CiTEFCON8_FSIZE_Mask                                     \
	(0x1Fu << MCP251XFD_CAN_CiTEFCON8_FSIZE_Pos)
#define MCP251XFD_CAN_CiTEFCON8_FSIZE_SET(value)                               \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTEFCON8_FSIZE_Pos) &                \
	 MCP251XFD_CAN_CiTEFCON8_FSIZE_Mask) //!< FIFO Size

#define MCP251XFD_CAN_CiTEFSTA_TEFNEIF                                         \
	(0x1u << 0) //!< Transmit Event FIFO Not Empty Interrupt Flag
#define MCP251XFD_CAN_CiTEFSTA_TEFHIF                                          \
	(0x1u << 1) //!< Transmit Event FIFO Half Full Interrupt Flag
#define MCP251XFD_CAN_CiTEFSTA_TEFFIF                                          \
	(0x1u << 2) //!< Transmit Event FIFO Full Interrupt Flag
#define MCP251XFD_CAN_CiTEFSTA_TEFOVIF                                         \
	(0x1u << 3) //!< Transmit Event FIFO Overflow Interrupt Flag

//*** Byte version access to Registers ***
#define MCP251XFD_CAN_CiTEFSTA8_TEFNEIF                                        \
	(0x1u << 0) //!< Transmit Event FIFO Not Empty Interrupt Flag
#define MCP251XFD_CAN_CiTEFSTA8_TEFHIF                                         \
	(0x1u << 1) //!< Transmit Event FIFO Half Full Interrupt Flag
#define MCP251XFD_CAN_CiTEFSTA8_TEFFIF                                         \
	(0x1u << 2) //!< Transmit Event FIFO Full Interrupt Flag
#define MCP251XFD_CAN_CiTEFSTA8_TEFOVIF                                        \
	(0x1u << 3) //!< Transmit Event FIFO Overflow Interrupt Flag

#define MCP251XFD_CAN_CiTEFSTA8_ALL_EVENTS                                     \
	(MCP251XFD_CAN_CiTEFSTA8_TEFNEIF | MCP251XFD_CAN_CiTEFSTA8_TEFHIF |        \
	 MCP251XFD_CAN_CiTEFSTA8_TEFFIF | MCP251XFD_CAN_CiTEFSTA8_TEFOVIF)

#define MCP251XFD_CAN_CiTEFUA_Pos 0
#define MCP251XFD_CAN_CiTEFUA_Mask (0xFFFFFFFFu << MCP251XFD_CAN_CiTEFUA_Pos)
#define MCP251XFD_CAN_CiTEFUA_SET(value)                                       \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTEFUA_Pos) &                        \
	 MCP251XFD_CAN_CiTEFUA_Mask) //!< Transmit Event FIFO User Address

#define MCP251XFD_CAN_CiTXQCON_TXQNIE                                          \
	(0x1u << 0) //!< Transmit Queue Not Full Interrupt Enable
#define MCP251XFD_CAN_CiTXQCON_TXQEIE                                          \
	(0x1u << 2) //!< Transmit Queue Empty Interrupt Enable
#define MCP251XFD_CAN_CiTXQCON_TXATIE                                          \
	(0x1u << 4) //!< Transmit Attempts Exhausted Interrupt Enable
#define MCP251XFD_CAN_CiTXQCON_TXEN (0x1u << 7)    //!< TX Enable
#define MCP251XFD_CAN_CiTXQCON_UINC (0x1u << 8)    //!< Increment Head
#define MCP251XFD_CAN_CiTXQCON_TXREQ (0x1u << 9)   //!< Message Send Request
#define MCP251XFD_CAN_CiTXQCON_FRESET (0x1u << 10) //!< FIFO Reset

#define MCP251XFD_CAN_CiTXQCON_TXPRI_Pos 16
#define MCP251XFD_CAN_CiTXQCON_TXPRI_Mask                                      \
	(0x1Fu << MCP251XFD_CAN_CiTXQCON_TXPRI_Pos)
#define MCP251XFD_CAN_CiTXQCON_TXPRI_SET(value)                                \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTXQCON_TXPRI_Pos) &                 \
	 MCP251XFD_CAN_CiTXQCON_TXPRI_Mask) //!< Message transmit priority

#define MCP251XFD_CAN_CiTXQCON_TXAT_Pos 21
#define MCP251XFD_CAN_CiTXQCON_TXAT_Mask                                       \
	(0x3u << MCP251XFD_CAN_CiTXQCON_TXAT_Pos)
#define MCP251XFD_CAN_CiTXQCON_TXAT_SET(value)                                 \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTXQCON_TXAT_Pos) &                  \
	 MCP251XFD_CAN_CiTXQCON_TXAT_Mask) //!< Retransmission Attempts
#define MCP251XFD_CAN_CiTXQCON_FSIZE_Pos 24
#define MCP251XFD_CAN_CiTXQCON_FSIZE_Mask                                      \
	(0x1Fu << MCP251XFD_CAN_CiTXQCON_FSIZE_Pos)
#define MCP251XFD_CAN_CiTXQCON_FSIZE_SET(value)                                \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTXQCON_FSIZE_Pos) &                 \
	 MCP251XFD_CAN_CiTXQCON_FSIZE_Mask) //!< FIFO Size

#define MCP251XFD_CAN_CiTXQCON_PLSIZE_Pos 29
#define MCP251XFD_CAN_CiTXQCON_PLSIZE_Mask                                     \
	(0x7u << MCP251XFD_CAN_CiTXQCON_PLSIZE_Pos)
#define MCP251XFD_CAN_CiTXQCON_PLSIZE_SET(value)                               \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTXQCON_PLSIZE_Pos) &                \
	 MCP251XFD_CAN_CiTXQCON_PLSIZE_Mask) //!< Payload Size

//*** Byte version access to Registers ***
#define MCP251XFD_CAN_CiTXQCON8_TXQNIE                                         \
	(0x1u << 0) //!< Transmit Queue Not Full Interrupt Enable
#define MCP251XFD_CAN_CiTXQCON8_TXQEIE                                         \
	(0x1u << 2) //!< Transmit Queue Empty Interrupt Enable
#define MCP251XFD_CAN_CiTXQCON8_TXATIE                                         \
	(0x1u << 4) //!< Transmit Attempts Exhausted Interrupt Enable
#define MCP251XFD_CAN_CiTXQCON8_TXEN (0x1u << 7)   //!< TX Enable
#define MCP251XFD_CAN_CiTXQCON8_UINC (0x1u << 0)   //!< Increment Head
#define MCP251XFD_CAN_CiTXQCON8_TXREQ (0x1u << 1)  //!< Message Send Request
#define MCP251XFD_CAN_CiTXQCON8_FRESET (0x1u << 2) //!< FIFO Reset
#define MCP251XFD_CAN_CiTXQCON8_TXPRI_Pos 0
#define MCP251XFD_CAN_CiTXQCON8_TXPRI_Mask                                     \
	(0x1Fu << MCP251XFD_CAN_CiTXQCON8_TXPRI_Pos)
#define MCP251XFD_CAN_CiTXQCON8_TXPRI_SET(value)                               \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTXQCON8_TXPRI_Pos) &                \
	 MCP251XFD_CAN_CiTXQCON8_TXPRI_Mask) //!< Message transmit priority
#define MCP251XFD_CAN_CiTXQCON8_TXAT_Pos 5
#define MCP251XFD_CAN_CiTXQCON8_TXAT_Mask                                      \
	(0x3u << MCP251XFD_CAN_CiTXQCON8_TXAT_Pos)
#define MCP251XFD_CAN_CiTXQCON8_TXAT_SET(value)                                \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTXQCON8_TXAT_Pos) &                 \
	 MCP251XFD_CAN_CiTXQCON8_TXAT_Mask) //!< Retransmission Attempts
#define MCP251XFD_CAN_CiTXQCON8_FSIZE_Pos 0
#define MCP251XFD_CAN_CiTXQCON8_FSIZE_Mask                                     \
	(0x1Fu << MCP251XFD_CAN_CiTXQCON8_FSIZE_Pos)
#define MCP251XFD_CAN_CiTXQCON8_FSIZE_SET(value)                               \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTXQCON8_FSIZE_Pos) &                \
	 MCP251XFD_CAN_CiTXQCON8_FSIZE_Mask) //!< FIFO Size
#define MCP251XFD_CAN_CiTXQCON8_PLSIZE_Pos 5
#define MCP251XFD_CAN_CiTXQCON8_PLSIZE_Mask                                    \
	(0x7u << MCP251XFD_CAN_CiTXQCON8_PLSIZE_Pos)
#define MCP251XFD_CAN_CiTXQCON8_PLSIZE_SET(value)                              \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTXQCON8_PLSIZE_Pos) &               \
	 MCP251XFD_CAN_CiTXQCON8_PLSIZE_Mask) //!< Payload Size

#define MCP251XFD_CAN_CiTXQSTA_TXQNIF                                          \
	(0x1u << 0) //!< Transmit Queue Not Full Interrupt Flag
#define MCP251XFD_CAN_CiTXQSTA_TXQEIF                                          \
	(0x1u << 2) //!< Transmit Queue Empty Interrupt Flag
#define MCP251XFD_CAN_CiTXQSTA_TXATIF                                          \
	(0x1u << 4) //!< Transmit Attempts Exhausted Interrupt Pending
#define MCP251XFD_CAN_CiTXQSTA_TXERR                                           \
	(0x1u << 5) //!< Error Detected During Transmission
#define MCP251XFD_CAN_CiTXQSTA_TXLARB                                          \
	(0x1u << 6) //!< Message Lost Arbitration Status
#define MCP251XFD_CAN_CiTXQSTA_TXABT (0x1u << 7) //!< Message Aborted Status
#define MCP251XFD_CAN_CiTXQSTA_TXQCI_Pos 8
#define MCP251XFD_CAN_CiTXQSTA_TXQCI_Mask                                      \
	(0x1Fu << MCP251XFD_CAN_CiTXQSTA_TXQCI_Pos)
#define MCP251XFD_CAN_CiTXQSTA_TXQCI_SET(value)                                \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTXQSTA_TXQCI_Pos) &                 \
	 MCP251XFD_CAN_CiTXQSTA_TXQCI_Mask) //!< Transmit Queue Message Index

//*** Byte version access to Registers ***
#define MCP251XFD_CAN_CiTXQSTA8_TXQNIF                                         \
	(0x1u << 0) //!< Transmit Queue Not Full Interrupt Flag
#define MCP251XFD_CAN_CiTXQSTA8_TXQEIF                                         \
	(0x1u << 2) //!< Transmit Queue Empty Interrupt Flag
#define MCP251XFD_CAN_CiTXQSTA8_TXATIF                                         \
	(0x1u << 4) //!< Transmit Attempts Exhausted Interrupt Pending
#define MCP251XFD_CAN_CiTXQSTA8_TXERR                                          \
	(0x1u << 5) //!< Error Detected During Transmission
#define MCP251XFD_CAN_CiTXQSTA8_TXLARB                                         \
	(0x1u << 6) //!< Message Lost Arbitration Status
#define MCP251XFD_CAN_CiTXQSTA8_TXABT (0x1u << 7) //!< Message Aborted Status
#define MCP251XFD_CAN_CiTXQSTA8_TXQCI_Pos 0
#define MCP251XFD_CAN_CiTXQSTA8_TXQCI_Mask                                     \
	(0x1Fu << MCP251XFD_CAN_CiTXQSTA8_TXQCI_Pos)
#define MCP251XFD_CAN_CiTXQSTA8_TXQCI_SET(value)                               \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTXQSTA8_TXQCI_Pos) &                \
	 MCP251XFD_CAN_CiTXQSTA8_TXQCI_Mask) //!< Transmit Queue Message Index

#define MCP251XFD_CAN_CiTXQSTA8_ALL_EVENTS                                     \
	(MCP251XFD_CAN_CiTXQSTA8_TXQNIF | MCP251XFD_CAN_CiTXQSTA8_TXQEIF |         \
	 MCP251XFD_CAN_CiTXQSTA8_TXATIF | MCP251XFD_CAN_CiTXQSTA8_TXERR |          \
	 MCP251XFD_CAN_CiTXQSTA8_TXLARB | MCP251XFD_CAN_CiTXQSTA8_TXABT)

#define MCP251XFD_CAN_CiTXQUA_Pos 0
#define MCP251XFD_CAN_CiTXQUA_Mask (0xFFFFFFFFu << MCP251XFD_CAN_CiTXQUA_Pos)
#define MCP251XFD_CAN_CiTXQUA_SET(value)                                       \
	(((uint32_t)(value) << MCP251XFD_CAN_CiTXQUA_Pos) &                        \
	 MCP251XFD_CAN_CiTXQUA_Mask) //!< Transmit Event FIFO User Address

#define MCP251XFD_CAN_CiFIFOCONm_TFNRFNIE                                      \
	(0x1u << 0) //!< Transmit/Receive FIFO Not Full/Not Empty Interrupt Enable
#define MCP251XFD_CAN_CiFIFOCONm_TFHRFHIE                                      \
	(0x1u << 1) //!< Transmit/Receive FIFO Half Empty/Half Full Interrupt Enable
#define MCP251XFD_CAN_CiFIFOCONm_TFERFFIE                                      \
	(0x1u << 2) //!< Transmit/Receive FIFO Empty/Full Interrupt Enable
#define MCP251XFD_CAN_CiFIFOCONm_RXOVIE                                        \
	(0x1u << 3) //!< Overflow Interrupt Enable
#define MCP251XFD_CAN_CiFIFOCONm_TXATIE                                        \
	(0x1u << 4) //!< Transmit Attempts Exhausted Interrupt Enable
#define MCP251XFD_CAN_CiFIFOCONm_RXTSEN                                        \
	(0x1u << 5) //!< Received Message Time Stamp Enable
#define MCP251XFD_CAN_CiFIFOCONm_RTREN (0x1u << 6) //!< Auto RTR Enable

#define MCP251XFD_CAN_CiFIFOCONm_TXEN (0x1u << 7)    //!< TX/RX FIFO Selection
#define MCP251XFD_CAN_CiFIFOCONm_UINC (0x1u << 8)    //!< Increment Head/Tail
#define MCP251XFD_CAN_CiFIFOCONm_TXREQ (0x1u << 9)   //!< Message Send Request
#define MCP251XFD_CAN_CiFIFOCONm_FRESET (0x1u << 10) //!< FIFO Reset
#define MCP251XFD_CAN_CiFIFOCONm_TXPRI_Pos 16
#define MCP251XFD_CAN_CiFIFOCONm_TXPRI_Mask                                    \
	(0x1Fu << MCP251XFD_CAN_CiFIFOCONm_TXPRI_Pos)
#define MCP251XFD_CAN_CiFIFOCONm_TXPRI_SET(value)                              \
	(((uint32_t)(value) << MCP251XFD_CAN_CiFIFOCONm_TXPRI_Pos) &               \
	 MCP251XFD_CAN_CiFIFOCONm_TXPRI_Mask) //!< Message transmit priority
#define MCP251XFD_CAN_CiFIFOCONm_TXAT_Pos 21
#define MCP251XFD_CAN_CiFIFOCONm_TXAT_Mask                                     \
	(0x3u << MCP251XFD_CAN_CiFIFOCONm_TXAT_Pos)
#define MCP251XFD_CAN_CiFIFOCONm_TXAT_SET(value)                               \
	(((uint32_t)(value) << MCP251XFD_CAN_CiFIFOCONm_TXAT_Pos) &                \
	 MCP251XFD_CAN_CiFIFOCONm_TXAT_Mask) //!< Retransmission Attempts
#define MCP251XFD_CAN_CiFIFOCONm_FSIZE_Pos 24
#define MCP251XFD_CAN_CiFIFOCONm_FSIZE_Mask                                    \
	(0x1Fu << MCP251XFD_CAN_CiFIFOCONm_FSIZE_Pos)
#define MCP251XFD_CAN_CiFIFOCONm_FSIZE_SET(value)                              \
	(((uint32_t)(value) << MCP251XFD_CAN_CiFIFOCONm_FSIZE_Pos) &               \
	 MCP251XFD_CAN_CiFIFOCONm_FSIZE_Mask) //!< FIFO Size
#define MCP251XFD_CAN_CiFIFOCONm_PLSIZE_Pos 29
#define MCP251XFD_CAN_CiFIFOCONm_PLSIZE_Mask                                   \
	(0x7u << MCP251XFD_CAN_CiFIFOCONm_PLSIZE_Pos)
#define MCP251XFD_CAN_CiFIFOCONm_PLSIZE_SET(value)                             \
	(((uint32_t)(value) << MCP251XFD_CAN_CiFIFOCONm_PLSIZE_Pos) &              \
	 MCP251XFD_CAN_CiFIFOCONm_PLSIZE_Mask) //!< Payload Size

//*** Byte version access to Registers ***
#define MCP251XFD_CAN_CiFIFOCONm8_TFNRFNIE                                     \
	(0x1u << 0) //!< Transmit/Receive FIFO Not Full/Not Empty Interrupt Enable
#define MCP251XFD_CAN_CiFIFOCONm8_TFHRFHIE                                     \
	(0x1u << 1) //!< Transmit/Receive FIFO Half Empty/Half Full Interrupt Enable
#define MCP251XFD_CAN_CiFIFOCONm8_TFERFFIE                                     \
	(0x1u << 2) //!< Transmit/Receive FIFO Empty/Full Interrupt Enable
#define MCP251XFD_CAN_CiFIFOCONm8_RXOVIE                                       \
	(0x1u << 3) //!< Overflow Interrupt Enable
#define MCP251XFD_CAN_CiFIFOCONm8_TXATIE                                       \
	(0x1u << 4) //!< Transmit Attempts Exhausted Interrupt Enable
#define MCP251XFD_CAN_CiFIFOCONm8_RXTSEN                                       \
	(0x1u << 5) //!< Received Message Time Stamp Enable
#define MCP251XFD_CAN_CiFIFOCONm8_RTREN (0x1u << 6)  //!< Auto RTR Enable
#define MCP251XFD_CAN_CiFIFOCONm8_TXEN (0x1u << 7)   //!< TX/RX FIFO Selection
#define MCP251XFD_CAN_CiFIFOCONm8_UINC (0x1u << 0)   //!< Increment Head/Tail
#define MCP251XFD_CAN_CiFIFOCONm8_TXREQ (0x1u << 1)  //!< Message Send Request
#define MCP251XFD_CAN_CiFIFOCONm8_FRESET (0x1u << 2) //!< FIFO Reset
#define MCP251XFD_CAN_CiFIFOCONm8_TXPRI_Pos 0
#define MCP251XFD_CAN_CiFIFOCONm8_TXPRI_Mask                                   \
	(0x1Fu << MCP251XFD_CAN_CiFIFOCONm8_TXPRI_Pos)
#define MCP251XFD_CAN_CiFIFOCONm8_TXPRI_SET(value)                             \
	(((uint32_t)(value) << MCP251XFD_CAN_CiFIFOCONm8_TXPRI_Pos) &              \
	 MCP251XFD_CAN_CiFIFOCONm8_TXPRI_Mask) //!< Message transmit priority
#define MCP251XFD_CAN_CiFIFOCONm8_TXAT_Pos 5
#define MCP251XFD_CAN_CiFIFOCONm8_TXAT_Mask                                    \
	(0x3u << MCP251XFD_CAN_CiFIFOCONm8_TXAT_Pos)
#define MCP251XFD_CAN_CiFIFOCONm8_TXAT_SET(value)                              \
	(((uint32_t)(value) << MCP251XFD_CAN_CiFIFOCONm8_TXAT_Pos) &               \
	 MCP251XFD_CAN_CiFIFOCONm8_TXAT_Mask) //!< Retransmission Attempts
#define MCP251XFD_CAN_CiFIFOCONm8_FSIZE_Pos 0
#define MCP251XFD_CAN_CiFIFOCONm8_FSIZE_Mask                                   \
	(0x1Fu << MCP251XFD_CAN_CiFIFOCONm8_FSIZE_Pos)
#define MCP251XFD_CAN_CiFIFOCONm8_FSIZE_SET(value)                             \
	(((uint32_t)(value) << MCP251XFD_CAN_CiFIFOCONm8_FSIZE_Pos) &              \
	 MCP251XFD_CAN_CiFIFOCONm8_FSIZE_Mask) //!< FIFO Size
#define MCP251XFD_CAN_CiFIFOCONm8_PLSIZE_Pos 5
#define MCP251XFD_CAN_CiFIFOCONm8_PLSIZE_Mask                                  \
	(0x7u << MCP251XFD_CAN_CiFIFOCONm8_PLSIZE_Pos)
#define MCP251XFD_CAN_CiFIFOCONm8_PLSIZE_SET(value)                            \
	(((uint32_t)(value) << MCP251XFD_CAN_CiFIFOCONm8_PLSIZE_Pos) &             \
	 MCP251XFD_CAN_CiFIFOCONm8_PLSIZE_Mask) //!< Payload Size

#define MCP251XFD_CAN_CiFIFOCONm8_INT_Mask                                     \
	(MCP251XFD_CAN_CiFIFOCONm8_TFNRFNIE | MCP251XFD_CAN_CiFIFOCONm8_TFHRFHIE | \
	 MCP251XFD_CAN_CiFIFOCONm8_TFERFFIE | MCP251XFD_CAN_CiFIFOCONm8_RXOVIE |   \
	 MCP251XFD_CAN_CiFIFOCONm8_TXATIE) //!< Interrupt flags mask

#define MCP251XFD_CAN_CiFIFOSTAm_TFNRFNIF                                      \
	(0x1u << 0) //!< Transmit/Receive FIFO Not Full/Not Empty Interrupt Flag
#define MCP251XFD_CAN_CiFIFOSTAm_TFHRFHIF                                      \
	(0x1u << 1) //!< Transmit/Receive FIFO Half Empty/Half Full Interrupt Flag
#define MCP251XFD_CAN_CiFIFOSTAm_TFERFFIF                                      \
	(0x1u << 2) //!< Transmit/Receive FIFO Empty/Full Interrupt Flag
#define MCP251XFD_CAN_CiFIFOSTAm_RXOVIF                                        \
	(0x1u << 3) //!< Receive FIFO Overflow Interrupt Flag
#define MCP251XFD_CAN_CiFIFOSTAm_TXATIF                                        \
	(0x1u << 4) //!< Transmit Attempts Exhausted Interrupt Pending
#define MCP251XFD_CAN_CiFIFOSTAm_TXERR                                         \
	(0x1u << 5) //!< Error Detected During Transmission
#define MCP251XFD_CAN_CiFIFOSTAm_TXLARB                                        \
	(0x1u << 6) //!< Message Lost Arbitration Status
#define MCP251XFD_CAN_CiFIFOSTAm_TXABT (0x1u << 7) //!< Message Aborted Status
#define MCP251XFD_CAN_CiFIFOSTAm_FIFOCI_Pos 8
#define MCP251XFD_CAN_CiFIFOSTAm_FIFOCI_Mask                                   \
	(0x1Fu << MCP251XFD_CAN_CiFIFOSTAm_FIFOCI_Pos)
#define MCP251XFD_CAN_CiFIFOSTAm_FIFOCI_SET(value)                             \
	(((uint32_t)(value) << MCP251XFD_CAN_CiFIFOSTAm_FIFOCI_Pos) &              \
	 MCP251XFD_CAN_CiFIFOSTAm_FIFOCI_Mask) //!< FIFO Message Index

//*** Byte version access to Registers ***
#define MCP251XFD_CAN_CiFIFOSTAm8_TFNRFNIF                                     \
	(0x1u << 0) //!< Transmit/Receive FIFO Not Full/Not Empty Interrupt Flag
#define MCP251XFD_CAN_CiFIFOSTAm8_TFHRFHIF                                     \
	(0x1u << 1) //!< Transmit/Receive FIFO Half Empty/Half Full Interrupt Flag
#define MCP251XFD_CAN_CiFIFOSTAm8_TFERFFIF                                     \
	(0x1u << 2) //!< Transmit/Receive FIFO Empty/Full Interrupt Flag
#define MCP251XFD_CAN_CiFIFOSTAm8_RXOVIF                                       \
	(0x1u << 3) //!< Receive FIFO Overflow Interrupt Flag
#define MCP251XFD_CAN_CiFIFOSTAm8_TXATIF                                       \
	(0x1u << 4) //!< Transmit Attempts Exhausted Interrupt Pending
#define MCP251XFD_CAN_CiFIFOSTAm8_TXERR                                        \
	(0x1u << 5) //!< Error Detected During Transmission
#define MCP251XFD_CAN_CiFIFOSTAm8_TXLARB                                       \
	(0x1u << 6) //!< Message Lost Arbitration Status
#define MCP251XFD_CAN_CiFIFOSTAm8_TXABT (0x1u << 7) //!< Message Aborted Status
#define MCP251XFD_CAN_CiFIFOSTAm8_FIFOCI_Pos 0
#define MCP251XFD_CAN_CiFIFOSTAm8_FIFOCI_Mask                                  \
	(0x1Fu << MCP251XFD_CAN_CiFIFOSTAm8_FIFOCI_Pos)
#define MCP251XFD_CAN_CiFIFOSTAm8_FIFOCI_SET(value)                            \
	(((uint32_t)(value) << MCP251XFD_CAN_CiFIFOSTAm8_FIFOCI_Pos) &             \
	 MCP251XFD_CAN_CiFIFOSTAm8_FIFOCI_Mask) //!< FIFO Message Index

#define MCP251XFD_CAN_CiFIFOSTAm8_TX_FIFO                                      \
	(MCP251XFD_CAN_CiFIFOSTAm8_TFNRFNIF | MCP251XFD_CAN_CiFIFOSTAm8_TFHRFHIF | \
	 MCP251XFD_CAN_CiFIFOSTAm8_TFERFFIF | MCP251XFD_CAN_CiFIFOSTAm8_TXATIF |   \
	 MCP251XFD_CAN_CiFIFOSTAm8_TXERR | MCP251XFD_CAN_CiFIFOSTAm8_TXLARB |      \
	 MCP251XFD_CAN_CiFIFOSTAm8_TXABT)
#define MCP251XFD_CAN_CiFIFOSTAm8_RX_FIFO                                      \
	(MCP251XFD_CAN_CiFIFOSTAm8_TFNRFNIF | MCP251XFD_CAN_CiFIFOSTAm8_TFHRFHIF | \
	 MCP251XFD_CAN_CiFIFOSTAm8_TFERFFIF | MCP251XFD_CAN_CiFIFOSTAm8_RXOVIF)

#define MCP251XFD_CAN_CiFIFOUAm_Pos 0
#define MCP251XFD_CAN_CiFIFOUAm_Mask                                           \
	(0xFFFFFFFFu << MCP251XFD_CAN_CiFIFOUAm_Pos)
#define MCP251XFD_CAN_CiFIFOUAm_GET(value)                                     \
	(((uint32_t)(value) & MCP251XFD_CAN_CiFIFOUAm_Mask)                        \
	 << MCP251XFD_CAN_CiFIFOUAm_Pos) //!< FIFO User Address

//-----------------------------------------------------------------------------

#define MCP251XFD_FIFO_REG_SIZE                                                \
	(sizeof(MCP251XFD_CiFIFOCONm_Register) +                                   \
	 sizeof(MCP251XFD_CiFIFOSTAm_Register) +                                   \
	 sizeof(MCP251XFD_CiFIFOUAm_Register)) //!< FIFO registers size is the size
	                                       //!< of bytes between FIFO Control
	                                       //!< and Status Registers

#define MCP251XFD_CAN_CiFLTCONm_FBP_Pos 0
#define MCP251XFD_CAN_CiFLTCONm_FBP_Mask                                       \
	(0x1Fu << MCP251XFD_CAN_CiFLTCONm_FBP_Pos)
#define MCP251XFD_CAN_CiFLTCONm_FBP_SET(value)                                 \
	(((uint8_t)(value) << MCP251XFD_CAN_CiFLTCONm_FBP_Pos) &                   \
	 MCP251XFD_CAN_CiFLTCONm_FBP_Mask) //!< Pointer to FIFO when Filter hits
#define MCP251XFD_CAN_CiFLTCONm_ENABLE                                         \
	(0x1u << 7) //!< Enable Filter to Accept Messages
#define MCP251XFD_CAN_CiFLTCONm_DISABLE                                        \
	(0x0u << 7) //!< Disable Filter to Accept Messages

//*** Byte version access to Registers ***
#define MCP251XFD_CAN_CiFLTCONm8_FBP_Pos 0
#define MCP251XFD_CAN_CiFLTCONm8_FBP_Mask                                      \
	(0x1Fu << MCP251XFD_CAN_CiFLTCONm8_FBP_Pos)
#define MCP251XFD_CAN_CiFLTCONm8_FBP_SET(value)                                \
	(((uint8_t)(value) << MCP251XFD_CAN_CiFLTCONm8_FBP_Pos) &                  \
	 MCP251XFD_CAN_CiFLTCONm8_FBP_Mask) //!< Pointer to FIFO when Filter hits
#define MCP251XFD_CAN_CiFLTCONm8_ENABLE                                        \
	(0x1u << 7) //!< Enable Filter to Accept Messages
#define MCP251XFD_CAN_CiFLTCONm8_DISABLE                                       \
	(0x0u << 7) //!< Disable Filter to Accept Messages

#define MCP251XFD_CAN_CiFLTOBJm_SID_Pos 0
#define MCP251XFD_CAN_CiFLTOBJm_SID_Mask                                       \
	(0x7FFu << MCP251XFD_CAN_CiFLTOBJm_SID_Pos)
#define MCP251XFD_CAN_CiFLTOBJm_SID_SET(value)                                 \
	(((uint32_t)(value) << MCP251XFD_CAN_CiFLTOBJm_SID_Pos) &                  \
	 MCP251XFD_CAN_CiFLTOBJm_SID_Mask) //!< Standard Identifier filter
#define MCP251XFD_CAN_CiFLTOBJm_EID_Pos 11
#define MCP251XFD_CAN_CiFLTOBJm_EID_Mask                                       \
	(0x3FFFFu << MCP251XFD_CAN_CiFLTOBJm_EID_Pos)
#define MCP251XFD_CAN_CiFLTOBJm_EID_SET(value)                                 \
	(((uint32_t)(value) << MCP251XFD_CAN_CiFLTOBJm_EID_Pos) &                  \
	 MCP251XFD_CAN_CiFLTOBJm_EID_Mask) //!< Extended Identifier filter
#define MCP251XFD_CAN_CiFLTOBJm_SID11                                          \
	(0x1u << 29) //!< Standard Identifier filter in FD mode
#define MCP251XFD_CAN_CiFLTOBJm_EXIDE                                          \
	(0x1u << 30) //!< Extended Identifier Enable

#define MCP251XFD_SID_Size 11
#define MCP251XFD_SID_Mask ((1 << MCP251XFD_SID_Size) - 1)
#define MCP251XFD_EID_Size 18
#define MCP251XFD_EID_Mask ((1 << MCP251XFD_EID_Size) - 1)

#define MCP251XFD_CAN_CiMASKm_MSID_Pos 0
#define MCP251XFD_CAN_CiMASKm_MSID_Mask                                        \
	(0x7FFu << MCP251XFD_CAN_CiMASKm_MSID_Pos)
#define MCP251XFD_CAN_CiMASKm_MSID_SET(value)                                  \
	(((uint32_t)(value) << MCP251XFD_CAN_CiMASKm_MSID_Pos) &                   \
	 MCP251XFD_CAN_CiMASKm_MSID_Mask) //!< Standard Identifier Mask
#define MCP251XFD_CAN_CiMASKm_MEID_Pos 11
#define MCP251XFD_CAN_CiMASKm_MEID_Mask                                        \
	(0x3FFFFu << MCP251XFD_CAN_CiMASKm_MEID_Pos)
#define MCP251XFD_CAN_CiMASKm_MEID_SET(value)                                  \
	(((uint32_t)(value) << MCP251XFD_CAN_CiMASKm_MEID_Pos) &                   \
	 MCP251XFD_CAN_CiMASKm_MEID_Mask) //!< Extended Identifier Mask
#define MCP251XFD_CAN_CiMASKm_MSID11                                           \
	(0x1u << 29) //!< Standard Identifier Mask in FD mode
#define MCP251XFD_CAN_CiMASKm_MIDE (0x1u << 30) //!< Extended Identifier Enable

//-----------------------------------------------------------------------------

#define MCP251XFD_FILTER_REG_SIZE                                              \
	(sizeof(MCP251XFD_CiFLTOBJm_Register) +                                    \
	 sizeof(MCP251XFD_CiMASKm_Register)) //!< Filter registers size is the size
	                                     //!< of bytes between FIFO Object and
	                                     //!< Mask Registers

#define MCP251XFD_CAN_MSGT0_SID_Pos 0
#define MCP251XFD_CAN_MSGT0_SID_Mask (0x7FFu << MCP251XFD_CAN_MSGT0_SID_Pos)
#define MCP251XFD_CAN_MSGT0_SID_SET(value)                                     \
	(((uint32_t)(value) << MCP251XFD_CAN_MSGT0_SID_Pos) &                      \
	 MCP251XFD_CAN_MSGT0_SID_Mask) //!< Set Standard Identifier
#define MCP251XFD_CAN_MSGT0_EID_Pos 11
#define MCP251XFD_CAN_MSGT0_EID_Mask (0x3FFFFu << MCP251XFD_CAN_MSGT0_EID_Pos)
#define MCP251XFD_CAN_MSGT0_EID_SET(value)                                     \
	(((uint32_t)(value) << MCP251XFD_CAN_MSGT0_EID_Pos) &                      \
	 MCP251XFD_CAN_MSGT0_EID_Mask) //!< Set Extended Identifier
#define MCP251XFD_CAN_MSGT0_SID11                                              \
	(0x1u                                                                      \
	 << 29) //!< In FD mode the standard ID can be extended to 12 bit using r1

#define MCP251XFD_CAN_MSGT1_DLC_Pos 0
#define MCP251XFD_CAN_MSGT1_DLC_Mask (0xFu << MCP251XFD_CAN_MSGT1_DLC_Pos)
#define MCP251XFD_CAN_MSGT1_DLC_SET(value)                                     \
	(((uint32_t)(value) << MCP251XFD_CAN_MSGT1_DLC_Pos) &                      \
	 MCP251XFD_CAN_MSGT1_DLC_Mask)          //!< Set Data Length Code
#define MCP251XFD_CAN_MSGT1_IDE (0x1u << 4) //!< Identifier Extension Flag
#define MCP251XFD_CAN_MSGT1_RTR (0x1u << 5) //!< Remote Transmission Request
#define MCP251XFD_CAN_MSGT1_BRS (0x1u << 6) //!< Bit Rate Switch
#define MCP251XFD_CAN_MSGT1_FDF (0x1u << 7) //!< FD Frame
#define MCP251XFD_CAN_MSGT1_ESI (0x1u << 8) //!< Error Status Indicator
#define MCP251XFD_CAN_MSGT1_SEQ_Pos 9
#define MCP251XFD_CAN_MSGT1_SEQ_Mask (0x7FFFFFu << MCP251XFD_CAN_MSGT1_SEQ_Pos)
#define MCP251XFD_CAN_MSGT1_SEQ_SET(value)                                     \
	(((uint32_t)(value) << MCP251XFD_CAN_MSGT1_SEQ_Pos) &                      \
	 MCP251XFD_CAN_MSGT1_SEQ_Mask) //!< Set sequence to keep track of
	                               //!< transmitted messages in Transmit Event
	                               //!< FIFO

#define MCP2517FD_SEQUENCE_MAX ((1 << 23) - 1)
#define MCP2518FD_SEQUENCE_MAX ((1 << 7) - 1)

//-----------------------------------------------------------------------------

#define MCP251XFD_CAN_MSG_T0 0
#define MCP251XFD_CAN_MSG_T1 1

#define MCP251XFD_CAN_TX_MESSAGE_SIZE_MAX                                      \
	(sizeof(MCP251XFD_CAN_TX_Message) + MCP251XFD_PAYLOAD_MAX)

//-----------------------------------------------------------------------------

#define MCP251XFD_CAN_MSG_TE0 0
#define MCP251XFD_CAN_MSG_TE1 1

#define MCP251XFD_CAN_TX_EVENTOBJECT_SIZE                                      \
	(sizeof(MCP251XFD_CAN_TX_Message_Identifier) +                             \
	 sizeof(MCP251XFD_CAN_TX_Message_Control))

//-----------------------------------------------------------------------------

#define MCP251XFD_CAN_MSGR0_SID_Pos 0
#define MCP251XFD_CAN_MSGR0_SID_Mask (0x7FFu << MCP251XFD_CAN_MSGR0_SID_Pos)
#define MCP251XFD_CAN_MSGR0_SID_GET(value)                                     \
	(((uint32_t)(value) & MCP251XFD_CAN_MSGR0_SID_Mask)                        \
	 << MCP251XFD_CAN_MSGR0_SID_Pos) //!< Get Standard Identifier
#define MCP251XFD_CAN_MSGR0_EID_Pos 11
#define MCP251XFD_CAN_MSGR0_EID_Mask (0x3FFFFu << MCP251XFD_CAN_MSGR0_EID_Pos)
#define MCP251XFD_CAN_MSGR0_EID_GET(value)                                     \
	(((uint32_t)(value) & MCP251XFD_CAN_MSGR0_EID_Mask)                        \
	 << MCP251XFD_CAN_MSGR0_EID_Pos) //!< Get Extended Identifier
#define MCP251XFD_CAN_MSGR0_SID11                                              \
	(0x1u                                                                      \
	 << 29) //!< In FD mode the standard ID can be extended to 12 bit using r1

#define MCP251XFD_CAN_MSGR1_DLC_Pos 0
#define MCP251XFD_CAN_MSGR1_DLC_Mask (0xFu << MCP251XFD_CAN_MSGR1_DLC_Pos)
#define MCP251XFD_CAN_MSGR1_DLC_GET(value)                                     \
	(((uint32_t)(value) & MCP251XFD_CAN_MSGR1_DLC_Mask) >>                     \
	 MCP251XFD_CAN_MSGR1_DLC_Pos)           //!< Get Data Length Code
#define MCP251XFD_CAN_MSGR1_IDE (0x1u << 4) //!< Identifier Extension Flag
#define MCP251XFD_CAN_MSGR1_RTR (0x1u << 5) //!< Remote Transmission Request
#define MCP251XFD_CAN_MSGR1_BRS (0x1u << 6) //!< Bit Rate Switch
#define MCP251XFD_CAN_MSGR1_FDF (0x1u << 7) //!< FD Frame
#define MCP251XFD_CAN_MSGR1_ESI (0x1u << 8) //!< Error Status Indicator
#define MCP251XFD_CAN_MSGR1_FILTHIT_Pos 11
#define MCP251XFD_CAN_MSGR1_FILTHIT_Mask                                       \
	(0x1Fu << MCP251XFD_CAN_MSGR1_FILTHIT_Pos)
#define MCP251XFD_CAN_MSGR1_FILTHIT_GET(value)                                 \
	(((uint32_t)(value) & MCP251XFD_CAN_MSGR1_FILTHIT_Mask) >>                 \
	 MCP251XFD_CAN_MSGR1_FILTHIT_Pos) //!< Get Filter Hit, number of filter that
	                                  //!< matched

//-----------------------------------------------------------------------------

#define MCP251XFD_CAN_MSG_R0 0
#define MCP251XFD_CAN_MSG_R1 1

#define MCP251XFD_CAN_RX_MESSAGE_SIZE_MAX                                      \
	(sizeof(MCP251XFD_CAN_RX_Message) + MCP251XFD_PAYLOAD_MAX)

//-----------------------------------------------------------------------------

#define MCP251XFD_FIFO_MIN_SIZE                                                \
	(sizeof(MCP251XFD_CAN_TX_Message) + MCP251XFD_PAYLOAD_MIN)

//-----------------------------------------------------------------------------

#define MCP251XFD_DEV_PS_Pos 0
#define MCP251XFD_DEV_PS_Mask (0x3u << MCP251XFD_DEV_PS_Pos)
#define MCP251XFD_DEV_PS_SET(value)                                            \
	(((uint32_t)(value) << MCP251XFD_DEV_PS_Pos) &                             \
	 MCP251XFD_DEV_PS_Mask) //!< Set Device Power State
#define MCP251XFD_DEV_PS_GET(value)                                            \
	(eMCP251XFD_PowerStates)(((uint32_t)(value) & MCP251XFD_DEV_PS_Mask) >>    \
	                         MCP251XFD_DEV_PS_Pos) //!< Get Device Power State

namespace MCP251XFD
{
	//********************************************************************************************************************
	// MCP251XFD's RAM definitions
	//********************************************************************************************************************

	//! MCP251XFD RAM FIFO Informations structure
	typedef struct MCP251XFD_RAMInfos
	{
		uint16_t ByteInFIFO; //!< Total number of bytes that FIFO takes in RAM
		uint16_t RAMStartAddress; //!< RAM Start Address of the FIFO
		uint8_t  ByteInObject;    //!< How many bytes in an object of the FIFO
	} MCP251XFD_RAMInfos;

	//-----------------------------------------------------------------------------

	//! int32_t to 2-uint16_t to 4-uint8_t conversion
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__
	{
		uint32_t Uint32;
		uint16_t Uint16[sizeof(uint32_t) / sizeof(uint16_t)];
		uint8_t  Bytes[sizeof(uint32_t) / sizeof(uint8_t)];
	} MCP251XFD_uint32t_Conv;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_uint32t_Conv, 4);

	//! int16_t to 2-uint8_t conversion
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__
	{
		uint16_t Uint16;
		uint8_t  Bytes[sizeof(uint16_t) / sizeof(uint8_t)];
	} MCP251XFD_uint16t_Conv;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_uint16t_Conv, 2);

	//-----------------------------------------------------------------------------

	//********************************************************************************************************************
	// MCP251XFD's driver definitions
	//********************************************************************************************************************

	//! Driver configuration enum
	typedef enum
	{
		MCP251XFD_DRIVER_NORMAL_USE =
		  0x00, //!< Use the driver with no special verifications, just settings
		        //!< verifications (usually the fastest mode)
		MCP251XFD_DRIVER_SAFE_RESET =
		  0x01, //!< Set Configuration mode first and next send a Reset command
		        //!< with a SPI clock at 1MHz max (MCP251XFD_SYSCLK_MIN div by
		        //!< 2)
		MCP251XFD_DRIVER_ENABLE_ECC =
		  0x02, //!< Enable the ECC just before the RAM initialization and
		        //!< activate ECCCON_SECIE and ECCCON_DEDIE interrupt flags
		MCP251XFD_DRIVER_INIT_CHECK_RAM =
		  0x04, //!< Check RAM at initialization by writing some data and
		        //!< checking them on all the RAM range (slower at
		        //!< initialization, take a long time)
		MCP251XFD_DRIVER_INIT_SET_RAM_AT_0 =
		  0x08, //!< Set all bytes of the RAM to 0x00 (slower at initialization)
		MCP251XFD_DRIVER_CLEAR_BUFFER_BEFORE_READ =
		  0x10, //!< This send 0x00 byte while reading SPI interface, mainly for
		        //!< cybersecurity purpose (little bit slower)
		MCP251XFD_DRIVER_USE_READ_WRITE_CRC =
		  0x20, //!< Use CRC with all commands and data going to and from the
		        //!< controller (add 3 more bytes to each transaction, 2 for CRC
		        //!< + 1 for length)
		MCP251XFD_DRIVER_USE_SAFE_WRITE =
		  0x40, //!< Each SFR write or memory write is sent one at a time
		        //!< (slower but send only the 2 bytes for CRC)
	} eMCP251XFD_DriverConfig;

	typedef eMCP251XFD_DriverConfig
	  setMCP251XFD_DriverConfig; //! Set of Driver configuration (can be OR'ed)

	//-----------------------------------------------------------------------------

	//! List of supported devices
	typedef enum
	{
		MCP2517FD = 0x0,         //!< MCP2517FD supported
		MCP2518FD = 0x1,         //!< MCP2518FD/MCP251863 supported
		eMPC251XFD_DEVICE_COUNT, // Device count of this enum, keep last
	} eMCP251XFD_Devices;

	const char *const MCP251XFD_DevicesNames[eMPC251XFD_DEVICE_COUNT] = {
	  "MCP2517FD",
	  "MCP2518FD", // Same for MCP251863
	};

	//-----------------------------------------------------------------------------

	//********************************************************************************************************************
	// MCP251XFD Register list
	//********************************************************************************************************************

	//! MCP251XFD registers list
	typedef enum
	{
		// CAN-FD Controller Module Registers
		RegMCP251XFD_CiCON = 0x000, //!< CAN Control Register
		RegMCP251XFD_CiNBTCFG =
		  0x004, //!< Nominal BitTime Configuration Register
		RegMCP251XFD_CiNBTCFG_SJW =
		  RegMCP251XFD_CiNBTCFG + 0, //!< Nominal BitTime Configuration Register
		                             //!< - Synchronization Jump Width
		RegMCP251XFD_CiNBTCFG_TSEG2 =
		  RegMCP251XFD_CiNBTCFG + 1, //!< Nominal BitTime Configuration Register
		                             //!< - Time Segment 2 (Phase Segment 2)
		RegMCP251XFD_CiNBTCFG_TSEG1 =
		  RegMCP251XFD_CiNBTCFG +
		  2, //!< Nominal BitTime Configuration Register - Time Segment 1
		     //!< (Propagation Segment + Phase Segment 1)
		RegMCP251XFD_CiNBTCFG_BRP =
		  RegMCP251XFD_CiNBTCFG +
		  3, //!< Nominal BitTime Configuration Register - Baud Rate Prescaler
		RegMCP251XFD_CiDBTCFG = 0x008, //!< Data BitTime Configuration Register
		RegMCP251XFD_CiDBTCFG_SJW =
		  RegMCP251XFD_CiDBTCFG + 0, //!< Data BitTime Configuration Register -
		                             //!< Synchronization Jump Width
		RegMCP251XFD_CiDBTCFG_TSEG2 =
		  RegMCP251XFD_CiDBTCFG + 1, //!< Data BitTime Configuration Register -
		                             //!< Time Segment 2 (Phase Segment 2)
		RegMCP251XFD_CiDBTCFG_TSEG1 =
		  RegMCP251XFD_CiDBTCFG +
		  2, //!< Data BitTime Configuration Register - Time Segment 1
		     //!< (Propagation Segment + Phase Segment 1)
		RegMCP251XFD_CiDBTCFG_BRP =
		  RegMCP251XFD_CiDBTCFG +
		  3, //!< Data BitTime Configuration Register - Baud Rate Prescaler
		RegMCP251XFD_CiTDC = 0x00C, //!< Transmitter Delay Compensation Register
		RegMCP251XFD_CiTDC_TDCV =
		  RegMCP251XFD_CiTDC + 0, //!< Transmitter Delay Compensation Register -
		                          //!< Transmitter Delay Compensation Value
		RegMCP251XFD_CiTDC_TDCO =
		  RegMCP251XFD_CiTDC + 1, //!< Transmitter Delay Compensation Register -
		                          //!< Transmitter Delay Compensation Offset
		RegMCP251XFD_CiTDC_TDCMOD =
		  RegMCP251XFD_CiTDC + 2, //!< Transmitter Delay Compensation Register -
		                          //!< Transmitter Delay Compensation Mode
		RegMCP251XFD_CiTDC_CONFIG =
		  RegMCP251XFD_CiTDC +
		  3, //!< Transmitter Delay Compensation Register - CAN-FD configuration
		RegMCP251XFD_CiTBC   = 0x010, //!< Time Base Counter Register
		RegMCP251XFD_CiTSCON = 0x014, //!< Time Stamp Control Register
		RegMCP251XFD_CiTSCON_TBCPRE =
		  RegMCP251XFD_CiTSCON +
		  0, //!< Time Stamp Control Register - Time Base Counter Prescaler
		RegMCP251XFD_CiTSCON_CONFIG =
		  RegMCP251XFD_CiTSCON +
		  2, //!< Time Stamp Control Register - Time Base Counter Configuration
		RegMCP251XFD_CiVEC = 0x018, //!< Interrupt Code Register
		RegMCP251XFD_CiVEC_ICODE =
		  RegMCP251XFD_CiVEC +
		  0, //!< Interrupt Code Register - Interrupt Flag Code
		RegMCP251XFD_CiVEC_FILHIT =
		  RegMCP251XFD_CiVEC +
		  1, //!< Interrupt Code Register - Filter Hit Number
		RegMCP251XFD_CiVEC_TXCODE =
		  RegMCP251XFD_CiVEC +
		  2, //!< Interrupt Code Register - Transmit Interrupt Flag Code
		RegMCP251XFD_CiVEC_RXCODE =
		  RegMCP251XFD_CiVEC +
		  3, //!< Interrupt Code Register - Receive Interrupt Flag Code
		RegMCP251XFD_CiINT = 0x01C, //!< Interrupt Register
		RegMCP251XFD_CiINT_FLAG =
		  RegMCP251XFD_CiINT + 0, //!< Interrupt Register - Interrupts flags
		RegMCP251XFD_CiINT_CONFIG =
		  RegMCP251XFD_CiINT + 2,    //!< Interrupt Register - Interrupts enable
		RegMCP251XFD_CiRXIF = 0x020, //!< Receive Interrupt Status Register
		RegMCP251XFD_CiTXIF =
		  0x024, //!< Receive Overflow Interrupt Status Register
		RegMCP251XFD_CiRXOVIF = 0x028, //!< Transmit Interrupt Status Register
		RegMCP251XFD_CiTXATIF =
		  0x02C, //!< Transmit Attempt Interrupt Status Register
		RegMCP251XFD_CiTXREQ = 0x030, //!< Transmit Request Register
		RegMCP251XFD_CiTREC  = 0x034, //!< Transmit/Receive Error Count Register
		RegMCP251XFD_CiTREC_REC =
		  RegMCP251XFD_CiTREC +
		  0, //!< Transmit/Receive Error Count Register - Receive Error Counter
		RegMCP251XFD_CiTREC_TEC =
		  RegMCP251XFD_CiTREC +
		  1, //!< Transmit/Receive Error Count Register - Transmit Error Counter
		RegMCP251XFD_CiTREC_STATUS =
		  RegMCP251XFD_CiTREC +
		  2, //!< Transmit/Receive Error Count Register - Error Status
		RegMCP251XFD_CiBDIAG0 = 0x038, //!< Bus Diagnostic Register 0
		RegMCP251XFD_CiBDIAG0_NRERRCNT =
		  RegMCP251XFD_CiBDIAG0 + 0, //!< Bus Diagnostic Register 0 - Nominal
		                             //!< Bit Rate Receive Error Counter
		RegMCP251XFD_CiBDIAG0_NTERRCNT =
		  RegMCP251XFD_CiBDIAG0 + 1, //!< Bus Diagnostic Register 0 - Nominal
		                             //!< Bit Rate Transmit Error Counter
		RegMCP251XFD_CiBDIAG0_DRERRCNT =
		  RegMCP251XFD_CiBDIAG0 + 2, //!< Bus Diagnostic Register 0 - Data Bit
		                             //!< Rate Receive Error Counter
		RegMCP251XFD_CiBDIAG0_DTERRCNT =
		  RegMCP251XFD_CiBDIAG0 + 3,   //!< Bus Diagnostic Register 0 - Data Bit
		                               //!< Rate Transmit Error Counter
		RegMCP251XFD_CiBDIAG1 = 0x03C, //!< Bus Diagnostic Register 1
		RegMCP251XFD_CiBDIAG1_EFMSGCNT =
		  RegMCP251XFD_CiBDIAG1 +
		  0, //!< Bus Diagnostic Register 1 - Error Free Message Counter
		RegMCP251XFD_CiTEFCON = 0x040, //!< Transmit Event FIFO Control Register
		RegMCP251XFD_CiTEFCON_CONFIG =
		  RegMCP251XFD_CiTEFCON +
		  0, //!< Transmit Event FIFO Control Register - Interrupt configuration
		RegMCP251XFD_CiTEFCON_CONTROL =
		  RegMCP251XFD_CiTEFCON +
		  1, //!< Transmit Event FIFO Control Register - TEF Control
		RegMCP251XFD_CiTEFSTA = 0x044, //!< Transmit Event FIFO Status Register
		RegMCP251XFD_CiTEFSTA_FLAGS =
		  RegMCP251XFD_CiTEFSTA +
		  0, //!< Transmit Event FIFO Status Register - Flags
		RegMCP251XFD_CiTEFUA =
		  0x048, //!< Transmit Event FIFO User Address Register
		RegMCP251XFD_Reserved4C = 0x04C, //!< Reserved Register
		                                 // Transmit Queue Registers
		RegMCP251XFD_CiTXQCON = 0x050,   //!< Transmit Queue Control Register
		RegMCP251XFD_CiTXQCON_CONFIG =
		  RegMCP251XFD_CiTXQCON +
		  0, //!< Transmit Queue Control Register - Interrupt configuration
		RegMCP251XFD_CiTXQCON_CONTROL =
		  RegMCP251XFD_CiTXQCON +
		  1, //!< Transmit Queue Control Register - TXQ Control
		RegMCP251XFD_CiTXQSTA = 0x054, //!< Transmit Queue Status Register
		RegMCP251XFD_CiTXQSTA_FLAGS =
		  RegMCP251XFD_CiTXQSTA + 0, //!< Transmit Queue Status Register - Flags
		RegMCP251XFD_CiTXQSTA_TXQCI =
		  RegMCP251XFD_CiTXQSTA +
		  1, //!< Transmit Queue Status Register - Transmit Queue Message Index
		RegMCP251XFD_CiTXQUA = 0x058, //!< Transmit Queue User Address Register
		                              // FIFOs Registers
		RegMCP251XFD_CiFIFOCONm =
		  0x05C, //!< FIFO Control Register m, (m = 1 to 31)
		RegMCP251XFD_CiFIFOCONm_CONFIG =
		  RegMCP251XFD_CiFIFOCONm + 0, //!< FIFO Control Register m, (m = 1 to
		                               //!< 31) - Interrupt configuration
		RegMCP251XFD_CiFIFOCONm_CONTROL =
		  RegMCP251XFD_CiFIFOCONm +
		  1, //!< FIFO Control Register m, (m = 1 to 31) - FIFO Control
		RegMCP251XFD_CiFIFOSTAm =
		  0x060, //!< FIFO Status Register m, (m = 1 to 31)
		RegMCP251XFD_CiFIFOSTAm_FLAGS =
		  RegMCP251XFD_CiFIFOSTAm +
		  0, //!< FIFO Status Register m, (m = 1 to 31) - Flags
		RegMCP251XFD_CiFIFOSTAm_FIFOCI =
		  RegMCP251XFD_CiFIFOSTAm +
		  1, //!< FIFO Status Register m, (m = 1 to 31) - FIFO Message Index
		RegMCP251XFD_CiFIFOUAm =
		  0x064, //!< FIFO User Address Register m, (m = 1 to 31)
		RegMCP251XFD_CiFIFOCON1  = 0x05C, //!< FIFO Control Register 1
		RegMCP251XFD_CiFIFOSTA1  = 0x060, //!< FIFO Status Register 1
		RegMCP251XFD_CiFIFOUA1   = 0x064, //!< FIFO User Address Register 1
		RegMCP251XFD_CiFIFOCON2  = 0x068, //!< FIFO Control Register 2
		RegMCP251XFD_CiFIFOSTA2  = 0x06C, //!< FIFO Status Register 2
		RegMCP251XFD_CiFIFOUA2   = 0x070, //!< FIFO User Address Register 2
		RegMCP251XFD_CiFIFOCON3  = 0x074, //!< FIFO Control Register 3
		RegMCP251XFD_CiFIFOSTA3  = 0x078, //!< FIFO Status Register 3
		RegMCP251XFD_CiFIFOUA3   = 0x07C, //!< FIFO User Address Register 3
		RegMCP251XFD_CiFIFOCON4  = 0x080, //!< FIFO Control Register 4
		RegMCP251XFD_CiFIFOSTA4  = 0x084, //!< FIFO Status Register 4
		RegMCP251XFD_CiFIFOUA4   = 0x088, //!< FIFO User Address Register 4
		RegMCP251XFD_CiFIFOCON5  = 0x08C, //!< FIFO Control Register 5
		RegMCP251XFD_CiFIFOSTA5  = 0x090, //!< FIFO Status Register 5
		RegMCP251XFD_CiFIFOUA5   = 0x094, //!< FIFO User Address Register 5
		RegMCP251XFD_CiFIFOCON6  = 0x098, //!< FIFO Control Register 6
		RegMCP251XFD_CiFIFOSTA6  = 0x09C, //!< FIFO Status Register 6
		RegMCP251XFD_CiFIFOUA6   = 0x0A0, //!< FIFO User Address Register 6
		RegMCP251XFD_CiFIFOCON7  = 0x0A4, //!< FIFO Control Register 7
		RegMCP251XFD_CiFIFOSTA7  = 0x0A8, //!< FIFO Status Register 7
		RegMCP251XFD_CiFIFOUA7   = 0x0AC, //!< FIFO User Address Register 7
		RegMCP251XFD_CiFIFOCON8  = 0x0B0, //!< FIFO Control Register 8
		RegMCP251XFD_CiFIFOSTA8  = 0x0B4, //!< FIFO Status Register 8
		RegMCP251XFD_CiFIFOUA8   = 0x0B8, //!< FIFO User Address Register 8
		RegMCP251XFD_CiFIFOCON9  = 0x0BC, //!< FIFO Control Register 9
		RegMCP251XFD_CiFIFOSTA9  = 0x0C0, //!< FIFO Status Register 9
		RegMCP251XFD_CiFIFOUA9   = 0x0C4, //!< FIFO User Address Register 9
		RegMCP251XFD_CiFIFOCON10 = 0x0C8, //!< FIFO Control Register 10
		RegMCP251XFD_CiFIFOSTA10 = 0x0CC, //!< FIFO Status Register 10
		RegMCP251XFD_CiFIFOUA10  = 0x0D0, //!< FIFO User Address Register 10
		RegMCP251XFD_CiFIFOCON11 = 0x0D4, //!< FIFO Control Register 11
		RegMCP251XFD_CiFIFOSTA11 = 0x0D8, //!< FIFO Status Register 11
		RegMCP251XFD_CiFIFOUA11  = 0x0DC, //!< FIFO User Address Register 11
		RegMCP251XFD_CiFIFOCON12 = 0x0E0, //!< FIFO Control Register 12
		RegMCP251XFD_CiFIFOSTA12 = 0x0E4, //!< FIFO Status Register 12
		RegMCP251XFD_CiFIFOUA12  = 0x0E8, //!< FIFO User Address Register 12
		RegMCP251XFD_CiFIFOCON13 = 0x0EC, //!< FIFO Control Register 13
		RegMCP251XFD_CiFIFOSTA13 = 0x0F0, //!< FIFO Status Register 13
		RegMCP251XFD_CiFIFOUA13  = 0x0F4, //!< FIFO User Address Register 13
		RegMCP251XFD_CiFIFOCON14 = 0x0F8, //!< FIFO Control Register 14
		RegMCP251XFD_CiFIFOSTA14 = 0x0FC, //!< FIFO Status Register 14
		RegMCP251XFD_CiFIFOUA14  = 0x100, //!< FIFO User Address Register 14
		RegMCP251XFD_CiFIFOCON15 = 0x104, //!< FIFO Control Register 15
		RegMCP251XFD_CiFIFOSTA15 = 0x108, //!< FIFO Status Register 15
		RegMCP251XFD_CiFIFOUA15  = 0x10C, //!< FIFO User Address Register 15
		RegMCP251XFD_CiFIFOCON16 = 0x110, //!< FIFO Control Register 16
		RegMCP251XFD_CiFIFOSTA16 = 0x114, //!< FIFO Status Register 16
		RegMCP251XFD_CiFIFOUA16  = 0x118, //!< FIFO User Address Register 16
		RegMCP251XFD_CiFIFOCON17 = 0x11C, //!< FIFO Control Register 17
		RegMCP251XFD_CiFIFOSTA17 = 0x120, //!< FIFO Status Register 17
		RegMCP251XFD_CiFIFOUA17  = 0x124, //!< FIFO User Address Register 17
		RegMCP251XFD_CiFIFOCON18 = 0x128, //!< FIFO Control Register 18
		RegMCP251XFD_CiFIFOSTA18 = 0x12C, //!< FIFO Status Register 18
		RegMCP251XFD_CiFIFOUA18  = 0x130, //!< FIFO User Address Register 18
		RegMCP251XFD_CiFIFOCON19 = 0x134, //!< FIFO Control Register 19
		RegMCP251XFD_CiFIFOSTA19 = 0x138, //!< FIFO Status Register 19
		RegMCP251XFD_CiFIFOUA19  = 0x13C, //!< FIFO User Address Register 19
		RegMCP251XFD_CiFIFOCON20 = 0x140, //!< FIFO Control Register 20
		RegMCP251XFD_CiFIFOSTA20 = 0x144, //!< FIFO Status Register 20
		RegMCP251XFD_CiFIFOUA20  = 0x148, //!< FIFO User Address Register 20
		RegMCP251XFD_CiFIFOCON21 = 0x14C, //!< FIFO Control Register 21
		RegMCP251XFD_CiFIFOSTA21 = 0x150, //!< FIFO Status Register 21
		RegMCP251XFD_CiFIFOUA21  = 0x154, //!< FIFO User Address Register 21
		RegMCP251XFD_CiFIFOCON22 = 0x158, //!< FIFO Control Register 22
		RegMCP251XFD_CiFIFOSTA22 = 0x15C, //!< FIFO Status Register 22
		RegMCP251XFD_CiFIFOUA22  = 0x160, //!< FIFO User Address Register 22
		RegMCP251XFD_CiFIFOCON23 = 0x164, //!< FIFO Control Register 23
		RegMCP251XFD_CiFIFOSTA23 = 0x168, //!< FIFO Status Register 23
		RegMCP251XFD_CiFIFOUA23  = 0x16C, //!< FIFO User Address Register 23
		RegMCP251XFD_CiFIFOCON24 = 0x170, //!< FIFO Control Register 24
		RegMCP251XFD_CiFIFOSTA24 = 0x174, //!< FIFO Status Register 24
		RegMCP251XFD_CiFIFOUA24  = 0x178, //!< FIFO User Address Register 24
		RegMCP251XFD_CiFIFOCON25 = 0x17C, //!< FIFO Control Register 25
		RegMCP251XFD_CiFIFOSTA25 = 0x180, //!< FIFO Status Register 25
		RegMCP251XFD_CiFIFOUA25  = 0x184, //!< FIFO User Address Register 25
		RegMCP251XFD_CiFIFOCON26 = 0x188, //!< FIFO Control Register 26
		RegMCP251XFD_CiFIFOSTA26 = 0x18C, //!< FIFO Status Register 26
		RegMCP251XFD_CiFIFOUA26  = 0x190, //!< FIFO User Address Register 26
		RegMCP251XFD_CiFIFOCON27 = 0x194, //!< FIFO Control Register 27
		RegMCP251XFD_CiFIFOSTA27 = 0x198, //!< FIFO Status Register 27
		RegMCP251XFD_CiFIFOUA27  = 0x19C, //!< FIFO User Address Register 27
		RegMCP251XFD_CiFIFOCON28 = 0x1A0, //!< FIFO Control Register 28
		RegMCP251XFD_CiFIFOSTA28 = 0x1A4, //!< FIFO Status Register 28
		RegMCP251XFD_CiFIFOUA28  = 0x1A8, //!< FIFO User Address Register 28
		RegMCP251XFD_CiFIFOCON29 = 0x1AC, //!< FIFO Control Register 29
		RegMCP251XFD_CiFIFOSTA29 = 0x1B0, //!< FIFO Status Register 29
		RegMCP251XFD_CiFIFOUA29  = 0x1B4, //!< FIFO User Address Register 29
		RegMCP251XFD_CiFIFOCON30 = 0x1B8, //!< FIFO Control Register 30
		RegMCP251XFD_CiFIFOSTA30 = 0x1BC, //!< FIFO Status Register 30
		RegMCP251XFD_CiFIFOUA30  = 0x1C0, //!< FIFO User Address Register 30
		RegMCP251XFD_CiFIFOCON31 = 0x1C4, //!< FIFO Control Register 31
		RegMCP251XFD_CiFIFOSTA31 = 0x1C8, //!< FIFO Status Register 31
		RegMCP251XFD_CiFIFOUA31  = 0x1CC, //!< FIFO User Address Register 31
		                                  // Filters Registers
		RegMCP251XFD_CiFLTCONm =
		  0x1D0, //!< Filter Control Register m, (m = 0 to 31)
		RegMCP251XFD_CiFLTCON0 = 0x1D0, //!< Filter  0 to  3 Control Register
		RegMCP251XFD_CiFLTCON0_FILTER0 =
		  RegMCP251XFD_CiFLTCON0 +
		  0, //!< Filter  0 to  3 Control Register - Filter 0
		RegMCP251XFD_CiFLTCON0_FILTER1 =
		  RegMCP251XFD_CiFLTCON0 +
		  1, //!< Filter  0 to  3 Control Register - Filter 1
		RegMCP251XFD_CiFLTCON0_FILTER2 =
		  RegMCP251XFD_CiFLTCON0 +
		  2, //!< Filter  0 to  3 Control Register - Filter 2
		RegMCP251XFD_CiFLTCON0_FILTER3 =
		  RegMCP251XFD_CiFLTCON0 +
		  3, //!< Filter  0 to  3 Control Register - Filter 3
		RegMCP251XFD_CiFLTCON1 = 0x1D4, //!< Filter  4 to  7 Control Register
		RegMCP251XFD_CiFLTCON1_FILTER4 =
		  RegMCP251XFD_CiFLTCON1 +
		  0, //!< Filter  4 to  7 Control Register - Filter 4
		RegMCP251XFD_CiFLTCON1_FILTER5 =
		  RegMCP251XFD_CiFLTCON1 +
		  1, //!< Filter  4 to  7 Control Register - Filter 5
		RegMCP251XFD_CiFLTCON1_FILTER6 =
		  RegMCP251XFD_CiFLTCON1 +
		  2, //!< Filter  4 to  7 Control Register - Filter 6
		RegMCP251XFD_CiFLTCON1_FILTER7 =
		  RegMCP251XFD_CiFLTCON1 +
		  3, //!< Filter  4 to  7 Control Register - Filter 7
		RegMCP251XFD_CiFLTCON2 = 0x1D8, //!< Filter  8 to 11 Control Register
		RegMCP251XFD_CiFLTCON2_FILTER8 =
		  RegMCP251XFD_CiFLTCON2 +
		  0, //!< Filter  8 to 11 Control Register - Filter 8
		RegMCP251XFD_CiFLTCON2_FILTER9 =
		  RegMCP251XFD_CiFLTCON2 +
		  1, //!< Filter  8 to 11 Control Register - Filter 9
		RegMCP251XFD_CiFLTCON2_FILTER10 =
		  RegMCP251XFD_CiFLTCON2 +
		  2, //!< Filter  8 to 11 Control Register - Filter 10
		RegMCP251XFD_CiFLTCON2_FILTER11 =
		  RegMCP251XFD_CiFLTCON2 +
		  3, //!< Filter  8 to 11 Control Register - Filter 11
		RegMCP251XFD_CiFLTCON3 = 0x1DC, //!< Filter 12 to 15 Control Register
		RegMCP251XFD_CiFLTCON3_FILTER12 =
		  RegMCP251XFD_CiFLTCON3 +
		  0, //!< Filter 12 to 15 Control Register - Filter 12
		RegMCP251XFD_CiFLTCON3_FILTER13 =
		  RegMCP251XFD_CiFLTCON3 +
		  1, //!< Filter 12 to 15 Control Register - Filter 13
		RegMCP251XFD_CiFLTCON3_FILTER14 =
		  RegMCP251XFD_CiFLTCON3 +
		  2, //!< Filter 12 to 15 Control Register - Filter 14
		RegMCP251XFD_CiFLTCON3_FILTER15 =
		  RegMCP251XFD_CiFLTCON3 +
		  3, //!< Filter 12 to 15 Control Register - Filter 15
		RegMCP251XFD_CiFLTCON4 = 0x1E0, //!< Filter 16 to 19 Control Register
		RegMCP251XFD_CiFLTCON4_FILTER16 =
		  RegMCP251XFD_CiFLTCON4 +
		  0, //!< Filter 16 to 19 Control Register - Filter 16
		RegMCP251XFD_CiFLTCON4_FILTER17 =
		  RegMCP251XFD_CiFLTCON4 +
		  1, //!< Filter 16 to 19 Control Register - Filter 17
		RegMCP251XFD_CiFLTCON4_FILTER18 =
		  RegMCP251XFD_CiFLTCON4 +
		  2, //!< Filter 16 to 19 Control Register - Filter 18
		RegMCP251XFD_CiFLTCON4_FILTER19 =
		  RegMCP251XFD_CiFLTCON4 +
		  3, //!< Filter 16 to 19 Control Register - Filter 19
		RegMCP251XFD_CiFLTCON5 = 0x1E4, //!< Filter 20 to 23 Control Register
		RegMCP251XFD_CiFLTCON5_FILTER20 =
		  RegMCP251XFD_CiFLTCON5 +
		  0, //!< Filter 20 to 23 Control Register - Filter 20
		RegMCP251XFD_CiFLTCON5_FILTER21 =
		  RegMCP251XFD_CiFLTCON5 +
		  1, //!< Filter 20 to 23 Control Register - Filter 21
		RegMCP251XFD_CiFLTCON5_FILTER22 =
		  RegMCP251XFD_CiFLTCON5 +
		  2, //!< Filter 20 to 23 Control Register - Filter 22
		RegMCP251XFD_CiFLTCON5_FILTER23 =
		  RegMCP251XFD_CiFLTCON5 +
		  3, //!< Filter 20 to 23 Control Register - Filter 23
		RegMCP251XFD_CiFLTCON6 = 0x1E8, //!< Filter 24 to 27 Control Register
		RegMCP251XFD_CiFLTCON6_FILTER24 =
		  RegMCP251XFD_CiFLTCON6 +
		  0, //!< Filter 24 to 27 Control Register - Filter 24
		RegMCP251XFD_CiFLTCON6_FILTER25 =
		  RegMCP251XFD_CiFLTCON6 +
		  1, //!< Filter 24 to 27 Control Register - Filter 25
		RegMCP251XFD_CiFLTCON6_FILTER26 =
		  RegMCP251XFD_CiFLTCON6 +
		  2, //!< Filter 24 to 27 Control Register - Filter 26
		RegMCP251XFD_CiFLTCON6_FILTER27 =
		  RegMCP251XFD_CiFLTCON6 +
		  3, //!< Filter 24 to 27 Control Register - Filter 27
		RegMCP251XFD_CiFLTCON7 = 0x1EC, //!< Filter 28 to 31 Control Register
		RegMCP251XFD_CiFLTCON7_FILTER28 =
		  RegMCP251XFD_CiFLTCON7 +
		  0, //!< Filter 28 to 31 Control Register - Filter 28
		RegMCP251XFD_CiFLTCON7_FILTER29 =
		  RegMCP251XFD_CiFLTCON7 +
		  1, //!< Filter 28 to 31 Control Register - Filter 29
		RegMCP251XFD_CiFLTCON7_FILTER30 =
		  RegMCP251XFD_CiFLTCON7 +
		  2, //!< Filter 28 to 31 Control Register - Filter 30
		RegMCP251XFD_CiFLTCON7_FILTER31 =
		  RegMCP251XFD_CiFLTCON7 +
		  3, //!< Filter 28 to 31 Control Register - Filter 31
		RegMCP251XFD_CiFLTOBJm =
		  0x1F0, //!< Filter Object Register m, (m = 0 to 31)
		RegMCP251XFD_CiMASKm = 0x1F4, //!< Filter Mask Register m, (m = 0 to 31)
		RegMCP251XFD_CiFLTOBJ0  = 0x1F0, //!< Filter Object Register 0
		RegMCP251XFD_CiMASK0    = 0x1F4, //!< Filter Mask Register 0
		RegMCP251XFD_CiFLTOBJ1  = 0x1F8, //!< Filter Object Register 1
		RegMCP251XFD_CiMASK1    = 0x1FC, //!< Filter Mask Register 1
		RegMCP251XFD_CiFLTOBJ2  = 0x200, //!< Filter Object Register 2
		RegMCP251XFD_CiMASK2    = 0x204, //!< Filter Mask Register 2
		RegMCP251XFD_CiFLTOBJ3  = 0x208, //!< Filter Object Register 3
		RegMCP251XFD_CiMASK3    = 0x20C, //!< Filter Mask Register 3
		RegMCP251XFD_CiFLTOBJ4  = 0x210, //!< Filter Object Register 4
		RegMCP251XFD_CiMASK4    = 0x214, //!< Filter Mask Register 4
		RegMCP251XFD_CiFLTOBJ5  = 0x218, //!< Filter Object Register 5
		RegMCP251XFD_CiMASK5    = 0x21C, //!< Filter Mask Register 5
		RegMCP251XFD_CiFLTOBJ6  = 0x220, //!< Filter Object Register 6
		RegMCP251XFD_CiMASK6    = 0x224, //!< Filter Mask Register 6
		RegMCP251XFD_CiFLTOBJ7  = 0x228, //!< Filter Object Register 7
		RegMCP251XFD_CiMASK7    = 0x22C, //!< Filter Mask Register 7
		RegMCP251XFD_CiFLTOBJ8  = 0x230, //!< Filter Object Register 8
		RegMCP251XFD_CiMASK8    = 0x234, //!< Filter Mask Register 8
		RegMCP251XFD_CiFLTOBJ9  = 0x238, //!< Filter Object Register 9
		RegMCP251XFD_CiMASK9    = 0x23C, //!< Filter Mask Register 9
		RegMCP251XFD_CiFLTOBJ10 = 0x240, //!< Filter Object Register 10
		RegMCP251XFD_CiMASK10   = 0x244, //!< Filter Mask Register 10
		RegMCP251XFD_CiFLTOBJ11 = 0x248, //!< Filter Object Register 11
		RegMCP251XFD_CiMASK11   = 0x24C, //!< Filter Mask Register 11
		RegMCP251XFD_CiFLTOBJ12 = 0x250, //!< Filter Object Register 12
		RegMCP251XFD_CiMASK12   = 0x254, //!< Filter Mask Register 12
		RegMCP251XFD_CiFLTOBJ13 = 0x258, //!< Filter Object Register 13
		RegMCP251XFD_CiMASK13   = 0x25C, //!< Filter Mask Register 13
		RegMCP251XFD_CiFLTOBJ14 = 0x260, //!< Filter Object Register 14
		RegMCP251XFD_CiMASK14   = 0x264, //!< Filter Mask Register 14
		RegMCP251XFD_CiFLTOBJ15 = 0x268, //!< Filter Object Register 15
		RegMCP251XFD_CiMASK15   = 0x26C, //!< Filter Mask Register 15
		RegMCP251XFD_CiFLTOBJ16 = 0x270, //!< Filter Object Register 16
		RegMCP251XFD_CiMASK16   = 0x274, //!< Filter Mask Register 16
		RegMCP251XFD_CiFLTOBJ17 = 0x278, //!< Filter Object Register 17
		RegMCP251XFD_CiMASK17   = 0x27C, //!< Filter Mask Register 17
		RegMCP251XFD_CiFLTOBJ18 = 0x280, //!< Filter Object Register 18
		RegMCP251XFD_CiMASK18   = 0x284, //!< Filter Mask Register 18
		RegMCP251XFD_CiFLTOBJ19 = 0x288, //!< Filter Object Register 19
		RegMCP251XFD_CiMASK19   = 0x28C, //!< Filter Mask Register 19
		RegMCP251XFD_CiFLTOBJ20 = 0x290, //!< Filter Object Register 20
		RegMCP251XFD_CiMASK20   = 0x294, //!< Filter Mask Register 20
		RegMCP251XFD_CiFLTOBJ21 = 0x298, //!< Filter Object Register 21
		RegMCP251XFD_CiMASK21   = 0x29C, //!< Filter Mask Register 21
		RegMCP251XFD_CiFLTOBJ22 = 0x2A0, //!< Filter Object Register 22
		RegMCP251XFD_CiMASK22   = 0x2A4, //!< Filter Mask Register 22
		RegMCP251XFD_CiFLTOBJ23 = 0x2A8, //!< Filter Object Register 23
		RegMCP251XFD_CiMASK23   = 0x2AC, //!< Filter Mask Register 23
		RegMCP251XFD_CiFLTOBJ24 = 0x2B0, //!< Filter Object Register 24
		RegMCP251XFD_CiMASK24   = 0x2B4, //!< Filter Mask Register 24
		RegMCP251XFD_CiFLTOBJ25 = 0x2B8, //!< Filter Object Register 25
		RegMCP251XFD_CiMASK25   = 0x2BC, //!< Filter Mask Register 25
		RegMCP251XFD_CiFLTOBJ26 = 0x2C0, //!< Filter Object Register 26
		RegMCP251XFD_CiMASK26   = 0x2C4, //!< Filter Mask Register 26
		RegMCP251XFD_CiFLTOBJ27 = 0x2C8, //!< Filter Object Register 27
		RegMCP251XFD_CiMASK27   = 0x2CC, //!< Filter Mask Register 27
		RegMCP251XFD_CiFLTOBJ28 = 0x2D0, //!< Filter Object Register 28
		RegMCP251XFD_CiMASK28   = 0x2D4, //!< Filter Mask Register 28
		RegMCP251XFD_CiFLTOBJ29 = 0x2D8, //!< Filter Object Register 29
		RegMCP251XFD_CiMASK29   = 0x2DC, //!< Filter Mask Register 29
		RegMCP251XFD_CiFLTOBJ30 = 0x2E0, //!< Filter Object Register 30
		RegMCP251XFD_CiMASK30   = 0x2E4, //!< Filter Mask Register 30
		RegMCP251XFD_CiFLTOBJ31 = 0x2E8, //!< Filter Object Register 31
		RegMCP251XFD_CiMASK31   = 0x2EC, //!< Filter Mask Register 31

		// MCP251XFD Specific Registers
		RegMCP251XFD_OSC = 0xE00, //!< Oscillator Control Register
		RegMCP251XFD_OSC_CONFIG =
		  RegMCP251XFD_OSC +
		  0, //!< Oscillator Control Register - Configuration register
		RegMCP251XFD_OSC_CHECK =
		  RegMCP251XFD_OSC + 1, //!< Oscillator Control Register - Check
		                        //!< frequency configuration register
#if defined(_MSC_VER) ||                                                       \
  (defined(__cplusplus) && (__cplusplus >= 201103L /*C++11*/)) ||              \
  (!defined(__cplusplus))
		RegMCP251XFD_IOCON =
		  0xE04, //!< Input/Output Control Register
		         //!< @deprecated Use RegMCP251XFD_IOCON_x subregisters with
		         //!< MCP251XFD_ReadSFR8() and MCP251XFD_WriteSFR8(). Follows
		         //!< datasheets errata for: Writing multiple bytes to the IOCON
		         //!< register using one SPI WRITE instruction may overwrite
		         //!< LAT0 and LAT1
#endif
		RegMCP251XFD_IOCON_DIRECTION =
		  0xE04 + 0, //!< Input/Output Control Register - Pins direction
		RegMCP251XFD_IOCON_OUTLEVEL =
		  0xE04 + 1, //!< Input/Output Control Register - Pin output level
		RegMCP251XFD_IOCON_INLEVEL =
		  0xE04 + 2, //!< Input/Output Control Register - Pin input level
		RegMCP251XFD_IOCON_PINMODE =
		  0xE04 + 3,              //!< Input/Output Control Register - Pin mode
		RegMCP251XFD_CRC = 0xE08, //!< CRC Register
		RegMCP251XFD_CRC_CRC =
		  RegMCP251XFD_CRC + 0, //!< CRC Register - Last CRC mismatch
		RegMCP251XFD_CRC_FLAGS =
		  RegMCP251XFD_CRC + 2, //!< CRC Register - Status flags
		RegMCP251XFD_CRC_CONFIG =
		  RegMCP251XFD_CRC + 3,      //!< CRC Register - Interrupts enable
		RegMCP251XFD_ECCCON = 0xE0C, //!< ECC Control Register
		RegMCP251XFD_ECCCON_ENABLE =
		  RegMCP251XFD_ECCCON +
		  0, //!< ECC Control Register - Interrupt and ECC enable
		RegMCP251XFD_ECCCON_PARITY =
		  RegMCP251XFD_ECCCON +
		  1, //!< ECC Control Register - Fixed parity value
		RegMCP251XFD_ECCSTAT = 0xE10, //!< ECC Status Register
		RegMCP251XFD_ECCSTAT_FLAGS =
		  RegMCP251XFD_ECCSTAT + 0, //!< ECC Status Register - Status flags
		RegMCP251XFD_ECCSTAT_ERRADDR =
		  RegMCP251XFD_ECCSTAT + 2, //!< ECC Status Register - ECC error address
		RegMCP251XFD_DEVID = 0xE14, //!< Device ID Register
	} eMCP251XFD_Registers;

	//********************************************************************************************************************
	// MCP251XFD Specific Controller Registers
	//********************************************************************************************************************

	//! Oscillator Control Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_OSC_Register
	{
		uint32_t OSC;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t PLLEN : 1;  //!<  0   - PLL Enable (This bit can only be
			                     //!<  modified in Configuration mode): 1 =
			                     //!<  System Clock from 10x PLL ; 0 = System
			                     //!<  Clock comes directly from XTAL oscillator
			uint32_t : 1;        //!<  1
			uint32_t OSCDIS : 1; //!<  2   - Clock (Oscillator) Disable: 1 =
			                     //!<  Clock disabled, the device is in Sleep
			                     //!<  mode ; 0 = Enable Clock (Clearing while
			                     //!<  in Sleep mode will wake-up the device and
			                     //!<  put it back in Configuration mode)
			uint32_t
			  LPMEN : 1; //!<  3   - (MCP2518FD only) Low Power Mode (LPM)
			             //!<  Enable: 1 = When in LPM, the device will stop the
			             //!<  clock and power down the majority of the chip.
			             //!<  Register and RAM values will be lost. The device
			             //!<  will wake-up due to asserting nCS, or due to
			             //!<  RXCAN activity ; 0 = When in Sleep mode, the
			             //!<  device will stop the clock, and retain it’s
			             //!<  register and RAM values. It will wake-up due to
			             //!<  clearing the OSCDIS bit, or due to RXCAN activity
			uint32_t
			  SCLKDIV : 1; //!<  4   - System Clock Divisor (This bit can only
			               //!<  be modified in Configuration mode): 1 = SCLK is
			               //!<  divided by 2 ; 0 = SCLK is divided by 1
			uint32_t CLKODIV : 2; //!<  5-6 - Clock Output Divisor: 11 = CLKO is
			                      //!<  divided by 10 ; 10 = CLKO is divided by
			                      //!<  4 ; 01 = CLKO is divided by 2 ; 00 =
			                      //!<  CLKO is divided by 1
			uint32_t : 1;         //!<  7
			uint32_t PLLRDY : 1; //!<  8   - PLL Ready: 1 = PLL Locked ; 0 = PLL
			                     //!<  not ready
			uint32_t : 1;        //!<  9
			uint32_t OSCRDY : 1; //!< 10   - Clock Ready: 1 = Clock is running
			                     //!< and stable ; 0 = Clock not ready or off
			uint32_t : 1;        //!< 11
			uint32_t SCLKRDY : 1; //!< 12   - Synchronized SCLKDIV bit: 1 =
			                      //!< SCLKDIV 1 ; 0 = SCLKDIV 0
			uint32_t : 19;        //!< 13-31
		} Bits;
	} MCP251XFD_OSC_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_OSC_Register, 4);

	//! System Clock Divisor for the OSC.SCLKDIV register
	typedef enum
	{
		MCP251XFD_SCLK_DivBy1 = 0b0, //!< System Clock Divisor by 1 (default)
		MCP251XFD_SCLK_DivBy2 = 0b1, //!< System Clock Divisor by 2
	} eMCP251XFD_SCLKDIV;

	//! Clock Output Divisor for the OSC.CLKODIV register
	typedef enum
	{
		MCP251XFD_CLKO_DivBy1 = 0b000, //!< Clock Output Divisor by 1
		MCP251XFD_CLKO_DivBy2 = 0b001, //!< Clock Output Divisor by 2
		MCP251XFD_CLKO_DivBy4 = 0b010, //!< Clock Output Divisor by 4
		MCP251XFD_CLKO_DivBy10 =
		  0b011,                    //!< Clock Output Divisor by 10 (default)
		MCP251XFD_CLKO_SOF = 0b111, //!< CLKO pin output Start Of Frame (Not
		                            //!< configured in the OSC.CLKODIV register)
	} eMCP251XFD_CLKODIV;

	//! Xtal/Oscillator (CLKIN) multiplier/divisor to SYSCLK
	typedef enum
	{
		MCP251XFD_SYSCLK_IS_CLKIN, //!< SYSCLK is CLKIN (no PLL,
		                           //!< eMCP251XFD_SCLKDIV.SCLK_DivBy1). For
		                           //!< CLKIN at 20MHz or 40MHz
		MCP251XFD_SYSCLK_IS_CLKIN_DIV_2, //!< SYSCLK is CLKIN divide by 2 (no
		                                 //!< PLL,
		                                 //!< eMCP251XFD_SCLKDIV.SCLK_DivBy2).
		                                 //!< For CLKIN at 20MHz or 40MHz
		MCP251XFD_SYSCLK_IS_CLKIN_MUL_5, //!< SYSCLK is CLKIN multiply by 5 (PLL
		                                 //!< enable,
		                                 //!< eMCP251XFD_SCLKDIV.SCLK_DivBy2).
		                                 //!< For CLKIN at 4MHz
		MCP251XFD_SYSCLK_IS_CLKIN_MUL_10, //!< SYSCLK is CLKIN multiply by 10
		                                  //!< (PLL enable,
		                                  //!< eMCP251XFD_SCLKDIV.SCLK_DivBy1).
		                                  //!< For CLKIN at 2MHz or 4MHz
	} eMCP251XFD_CLKINtoSYSCLK;

	//-----------------------------------------------------------------------------

	//! Input/Output Control Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_IOCON_Register
	{
		uint32_t IOCON;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t
			  TRIS0 : 1; //!<  0 - GPIO0 Data Direction. If PM0 = '0', TRIS0
			             //!<  will be ignored and the pin will be an output:
			             //!<  '1' = Input Pin ; '0' = Output Pin
			uint32_t
			  TRIS1 : 1;  //!<  1 - GPIO1 Data Direction. If PM1 = '0', TRIS1
			              //!<  will be ignored and the pin will be an output:
			              //!<  '1' = Input Pin ; '0' = Output Pin
			uint32_t : 4; //!<  2-5
			uint32_t XSTBYEN : 1; //!<  6 - Enable Transceiver Standby Pin
			                      //!<  Control: '1' = XSTBY control enabled ;
			                      //!<  '0' = XSTBY control disabled
			uint32_t : 1;         //!<  7
			uint32_t LAT0 : 1; //!<  8 - GPIO0 Latch: '1' = Drive Pin High ; '0'
			                   //!<  = Drive Pin Low
			uint32_t LAT1 : 1; //!<  9 - GPIO1 Latch: '1' = Drive Pin High ; '0'
			                   //!<  = Drive Pin Low
			uint32_t : 6;      //!< 10-15
			uint32_t GPIO0 : 1; //!< 16 - GPIO0 Status: '1' = VGPIO0 > VIH ; '0'
			                    //!< = VGPIO0 < VIL
			uint32_t GPIO1 : 1; //!< 17 - GPIO1 Status: '1' = VGPIO0 > VIH ; '0'
			                    //!< = VGPIO0 < VIL
			uint32_t : 6;       //!< 18-23
			uint32_t PM0 : 1;   //!< 24 - GPIO Pin Mode: '1' = Pin is used as
			                    //!< GPIO0 ; '0' = Interrupt Pin INT0, asserted
			                    //!< when CiINT.TXIF and TXIE are set
			uint32_t PM1 : 1;   //!< 25 - GPIO Pin Mode: '1' = Pin is used as
			                    //!< GPIO1 ; '0' = Interrupt Pin INT1, asserted
			                    //!< when CiINT.TXIF and TXIE are set
			uint32_t : 2;       //!< 26-27
			uint32_t TXCANOD : 1; //!< 28 - TXCAN Open Drain Mode: '1' = Open
			                      //!< Drain Output ; '0' = Push/Pull Output
			uint32_t SOF : 1;     //!< 29 - Start-Of-Frame signal: SOF signal on
			                      //!< CLKO pin ; Clock on CLKO pin
			uint32_t INTOD : 1; //!< 30 - Interrupt pins Open Drain Mode: '1' =
			                    //!< Open Drain Output ; '0' = Push/Pull Output
			uint32_t : 1;       //!< 31
		} Bits;
	} MCP251XFD_IOCON_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_IOCON_Register, 4);

	//! INT0/GPIO0/XSTBY configuration for the IOCON register
	typedef enum
	{
		MCP251XFD_PIN_AS_INT0_TX =
		  0b00, //!< INT0/GPIO0/XSTBY pin as TX Interrupt output (active low)
		MCP251XFD_PIN_AS_GPIO0_IN =
		  0b01, //!< INT0/GPIO0/XSTBY pin as GPIO input
		MCP251XFD_PIN_AS_GPIO0_OUT =
		  0b10, //!< INT0/GPIO0/XSTBY pin as GPIO output
		MCP251XFD_PIN_AS_XSTBY =
		  0b11, //!< INT0/GPIO0/XSTBY pin as Transceiver Standby output
	} eMCP251XFD_GPIO0Mode;

	//! INT1/GPIO1 configuration for the IOCON register
	typedef enum
	{
		MCP251XFD_PIN_AS_INT1_RX =
		  0b00, //!< INT1/GPIO1 pin as RX Interrupt output (active low)
		MCP251XFD_PIN_AS_GPIO1_IN  = 0b01, //!< INT1/GPIO1 pin as GPIO input
		MCP251XFD_PIN_AS_GPIO1_OUT = 0b10, //!< INT1/GPIO1 pin as GPIO output
	} eMCP251XFD_GPIO1Mode;

	//! Output configuration for the IOCON.INTOD and the IOCON.TXCANOD register
	typedef enum
	{
		MCP251XFD_PINS_PUSHPULL_OUT  = 0b00, //!< Pin with Push/Pull output
		MCP251XFD_PINS_OPENDRAIN_OUT = 0b01, //!< Pin with Open Drain output
	} eMCP251XFD_OutMode;

	//-----------------------------------------------------------------------------

	//! CRC Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CRC_Register
	{
		uint32_t CRCreg;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t CRCbits : 16; //!<  0-15 - Cycle Redundancy Check from last
			                       //!<  CRC mismatch
			uint32_t CRCERRIF : 1; //!< 16    - CRC Error Interrupt Flag: '1' =
			                       //!< CRC mismatch occurred ; '0' = No CRC
			                       //!< error has occurred
			uint32_t
			  FERRIF : 1; //!< 17    - CRC Command Format Error Interrupt Flag:
			              //!< '1' = Number of Bytes mismatch during “SPI with
			              //!< CRC” command occurred ; '0' = No SPI CRC command
			              //!< format error occurred
			uint32_t : 6;          //!< 18-23
			uint32_t CRCERRIE : 1; //!< 24    - CRC Error Interrupt Enable
			uint32_t
			  FERRIE : 1; //!< 25    - CRC Command Format Error Interrupt Enable
			uint32_t : 6; //!< 26-31
		} Bits;
	} MCP251XFD_CRC_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CRC_Register, 4);

	//! CRC Events
	typedef enum
	{
		MCP251XFD_CRC_NO_EVENT     = 0x00, //!< No CRC events
		MCP251XFD_CRC_CRCERR_EVENT = 0x01, //!< CRC Error Interrupt event
		MCP251XFD_CRC_FORMERR_EVENT =
		  0x02, //!< CRC Command Format Error Interrupt event
		MCP251XFD_CRC_ALL_EVENTS  = 0x03, //!< All CRC interrupts events
		MCP251XFD_CRC_EVENTS_MASK = 0x03, //!< CRC events mask
	} eMCP251XFD_CRCEvents;

	typedef eMCP251XFD_CRCEvents
	  setMCP251XFD_CRCEvents; //! Set of CRC Events (can be OR'ed)

	//-----------------------------------------------------------------------------

	//! ECC Control Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_ECCCON_Register
	{
		uint32_t ECCCON;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t ECCEN : 1; //!<  0    - ECC Enable: '1' = ECC enabled ; '0'
			                    //!<  = ECC disabled
			uint32_t SECIE : 1; //!<  1    - Single Error Correction Interrupt
			                    //!<  Enable Flag
			uint32_t DEDIE : 1; //!<  2    - Double Error Detection Interrupt
			                    //!<  Enable Flag
			uint32_t : 5;       //!<  3- 7
			uint32_t PARITY : 7; //!<  8-14 - Parity bits used during write to
			                     //!<  RAM when ECC is disabled
			uint32_t : 17;       //!< 15-31
		} Bits;
	} MCP251XFD_ECCCON_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_ECCCON_Register, 4);

	//-----------------------------------------------------------------------------

	//! ECC Status Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_ECCSTAT_Register
	{
		uint32_t ECCSTAT;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t : 1;       //!<  0
			uint32_t SECIF : 1; //!<  1    - Single Error Correction Interrupt
			                    //!<  Flag: '1' = Single Error was corrected ;
			                    //!<  '0' = No Single Error occurred
			uint32_t DEDIF : 1; //!<  2    - Double Error Detection Interrupt
			                    //!<  Flag: '1' = Double Error was detected ;
			                    //!<  '0' = No Double Error Detection occurred
			uint32_t : 13;      //!<  3-15
			uint32_t
			  ERRADDR : 12; //!< 16-27 - Address where last ECC error occurred
			uint32_t : 4;   //!< 28-31
		} Bits;
	} MCP251XFD_ECCSTAT_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_ECCSTAT_Register, 4);

	//! ECC Events
	typedef enum
	{
		MCP251XFD_ECC_NO_EVENT = 0x00, //!< No ECC events
		MCP251XFD_ECC_SEC_EVENT =
		  0x02, //!< ECC Single Error Correction Interrupt
		MCP251XFD_ECC_DED_EVENT =
		  0x04, //!< ECC Double Error Detection Interrupt
		MCP251XFD_ECC_ALL_EVENTS  = 0x06, //!< All ECC interrupts events
		MCP251XFD_ECC_EVENTS_MASK = 0x06, //!< ECC events mask
	} eMCP251XFD_ECCEvents;

	typedef eMCP251XFD_ECCEvents
	  setMCP251XFD_ECCEvents; //! Set of ECC Events (can be OR'ed)

	//-----------------------------------------------------------------------------

	//! Device ID Register (MCP2518FD only)
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_DEVID_Register
	{
		uint32_t DEVID;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t REV : 4; //!< 0- 3 - Silicon Revision
			uint32_t ID : 4;  //!< 4- 7 - Device ID
			uint32_t : 24;    //!< 8-31
		} Bits;
	} MCP251XFD_DEVID_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_DEVID_Register, 4);

	//-----------------------------------------------------------------------------

	//********************************************************************************************************************
	// MCP251XFD CAN Controller Registers
	//********************************************************************************************************************

	//! CAN Control Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiCON_Register
	{
		uint32_t CiCON;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t DNCNT : 5; //!<  0- 4 - Device Net Filter Bit Number bits
			uint32_t
			  ISOCRCEN : 1; //!<  5    - Enable ISO CRC in CAN FD Frames bit:
			                //!<  '1' = Include Stuff Bit Count in CRC Field and
			                //!<  use Non-Zero CRC Initialization Vector
			                //!<  according to ISO 11898-1:2015 ; '0' = Do NOT
			                //!<  include Stuff Bit Count in CRC Field and use
			                //!<  CRC Initialization Vector with all zeros
			uint32_t
			  PXEDIS : 1; //!<  6    - Protocol Exception Event Detection
			              //!<  Disabled bit: '1' = Protocol Exception is
			              //!<  treated as a Form Error ; '0' = If a Protocol
			              //!<  Exception is detected, the CAN FD Controller
			              //!<  Module will enter Bus Integrating state
			uint32_t : 1; //!<  7
			uint32_t
			  WAKFIL : 1; //!<  8    - Enable CAN Bus Line Wake-up Filter bit:
			              //!<  '1' = Use CAN bus line filter for wake-up ; '0'
			              //!<  = CAN bus line filter is not used for wake-up
			uint32_t WFT : 2;  //!<  9-10 - Selectable Wake-up Filter Time bits
			uint32_t BUSY : 1; //!< 11    - CAN Module is Busy bit: '1' = The
			                   //!< CAN module is transmitting or receiving a
			                   //!< message ; '0' = The CAN module is inactive
			uint32_t BRSDIS : 1; //!< 12    - Bit Rate Switching Disable bit:
			                     //!< '1' = Bit Rate Switching is Disabled,
			                     //!< regardless of BRS in the Transmit Message
			                     //!< Object ; '0' = Bit Rate Switching depends
			                     //!< on BRS in the Transmit Message Object
			uint32_t : 3;        //!< 13-15
			uint32_t RTXAT : 1;  //!< 16    - Restrict Retransmission Attempts
			                     //!< bit: '1' = Restricted retransmission
			                     //!< attempts, CiFIFOCONm.TXAT is used ; '0' =
			                     //!< Unlimited number of retransmission
			                     //!< attempts, CiFIFOCONm.TXAT will be ignored
			uint32_t
			  ESIGM : 1; //!< 17    - Transmit ESI in Gateway Mode bit: '1' =
			             //!< ESI is transmitted recessive when ESI of message
			             //!< is high or CAN controller error passive ; '0' =
			             //!< ESI reflects error status of CAN controller
			uint32_t SERR2LOM : 1; //!< 18    - Transition to Listen Only Mode
			                       //!< on System Error bit: '1' = Transition to
			                       //!< Listen Only Mode ; '0' = Transition to
			                       //!< Restricted Operation Mode
			uint32_t STEF : 1;  //!< 19    - Store in Transmit Event FIFO bit:
			                    //!< '1' = Saves transmitted messages in TEF and
			                    //!< reserves space in RAM ; '0' = Don’t save
			                    //!< transmitted messages in TEF
			uint32_t TXQEN : 1; //!< 20    - Enable Transmit Queue bit: '1' =
			                    //!< Enables TXQ and reserves space in RAM ; '0'
			                    //!< = Don’t reserve space in RAM for TXQ
			uint32_t OPMOD : 3; //!< 21-23 - Operation Mode Status bits
			uint32_t REQOP : 3; //!< 24-26 - Request Operation Mode bits
			uint32_t ABAT : 1; //!< 27    - Abort All Pending Transmissions bit:
			                   //!< '1' = Signal all transmit FIFOs to abort
			                   //!< transmission ; '0' = Module will clear this
			                   //!< bit when all transmissions were aborted
			uint32_t TXBWS : 4; //!< 28-31 - Transmit Bandwidth Sharing bits
		} Bits;
	} MCP251XFD_CiCON_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiCON_Register, 4);

	/*! @enum eMCP251XFD_DNETFilter
	 * @brief Device Net Filter Bit Number bits for the CiCON.DNCNT register
	 * If DNCNT is greater than 0 and the received message has DLC = 0,
	 * indicating no data payload, the filter comparison will terminate with the
	 * identifier. If DNCNT is greater than 8 and the received message has DLC =
	 * 1, indicating a payload of one data byte, the filter comparison will
	 * terminate with the 8th bit of the data. If DNCNT is greater than 16 and
	 * the received message has DLC = 2, indicating a payload of two data bytes,
	 * the filter comparison will terminate with the 16th bit of the data. If
	 * DNCNT is greater than 18, indicating that the user selected a number of
	 * bits greater than the total number of EID bits, the filter comparison
	 * will terminate with the 18th bit of the data.
	 */
	typedef enum
	{
		MCP251XFD_D_NET_FILTER_DISABLE = 0b00000, //!< Do not compare data bytes

		MCP251XFD_D_NET_FILTER_1Bit =
		  0b00001, //!< Compare up to data byte 0 bit 7 with EID0 (Data Byte
		           //!< 0[7] = EID[0])
		MCP251XFD_D_NET_FILTER_2Bits =
		  0b00010, //!< Compare up to data byte 0 bit 6 with EID1 (Data Byte
		           //!< 0[7:6] = EID[0:1])
		MCP251XFD_D_NET_FILTER_3Bits =
		  0b00011, //!< Compare up to data byte 0 bit 5 with EID2 (Data Byte
		           //!< 0[7:5] = EID[0:2])
		MCP251XFD_D_NET_FILTER_4Bits =
		  0b00100, //!< Compare up to data byte 0 bit 4 with EID3 (Data Byte
		           //!< 0[7:4] = EID[0:3])
		MCP251XFD_D_NET_FILTER_5Bits =
		  0b00101, //!< Compare up to data byte 0 bit 3 with EID4 (Data Byte
		           //!< 0[7:3] = EID[0:4])
		MCP251XFD_D_NET_FILTER_6Bits =
		  0b00110, //!< Compare up to data byte 0 bit 2 with EID5 (Data Byte
		           //!< 0[7:2] = EID[0:5])
		MCP251XFD_D_NET_FILTER_7Bits =
		  0b00111, //!< Compare up to data byte 0 bit 1 with EID6 (Data Byte
		           //!< 0[7:1] = EID[0:6])
		MCP251XFD_D_NET_FILTER_8Bits =
		  0b01000, //!< Compare up to data byte 0 bit 0 with EID7 (Data Byte
		           //!< 0[7:0] = EID[0:7])

		MCP251XFD_D_NET_FILTER_9Bits =
		  0b01001, //!< Compare up to data byte 1 bit 7 with EID8 (Data Byte
		           //!< 0[7:0] and Data Byte 1[7] = EID[0:8])
		MCP251XFD_D_NET_FILTER_10Bits =
		  0b01010, //!< Compare up to data byte 1 bit 6 with EID9 (Data Byte
		           //!< 0[7:0] and Data Byte 1[7:6] = EID[0:9])
		MCP251XFD_D_NET_FILTER_11Bits =
		  0b01011, //!< Compare up to data byte 1 bit 5 with EID10 (Data Byte
		           //!< 0[7:0] and Data Byte 1[7:5] = EID[0:10])
		MCP251XFD_D_NET_FILTER_12Bits =
		  0b01100, //!< Compare up to data byte 1 bit 4 with EID11 (Data Byte
		           //!< 0[7:0] and Data Byte 1[7:4] = EID[0:11])
		MCP251XFD_D_NET_FILTER_13Bits =
		  0b01101, //!< Compare up to data byte 1 bit 3 with EID12 (Data Byte
		           //!< 0[7:0] and Data Byte 1[7:3] = EID[0:12])
		MCP251XFD_D_NET_FILTER_14Bits =
		  0b01110, //!< Compare up to data byte 1 bit 2 with EID13 (Data Byte
		           //!< 0[7:0] and Data Byte 1[7:2] = EID[0:13])
		MCP251XFD_D_NET_FILTER_15Bits =
		  0b01111, //!< Compare up to data byte 1 bit 1 with EID14 (Data Byte
		           //!< 0[7:0] and Data Byte 1[7:1] = EID[0:14])
		MCP251XFD_D_NET_FILTER_16Bits =
		  0b10000, //!< Compare up to data byte 1 bit 0 with EID15 (Data Byte
		           //!< 0[7:0] and Data Byte 1[7:0] = EID[0:15])

		MCP251XFD_D_NET_FILTER_17Bits =
		  0b10001, //!< Compare up to data byte 2 bit 7 with EID16 (Data Byte
		           //!< 0[7:0] and Data Byte 1[7:0] and Byte 2[7] = EID[0:16])
		MCP251XFD_D_NET_FILTER_18Bits =
		  0b10010, //!< Compare up to data byte 2 bit 6 with EID17 (Data Byte
		           //!< 0[7:0] and Data Byte 1[7:0] and Byte 2[7:6] = EID[0:17])
	} eMCP251XFD_DNETFilter;

	/*! @enum eMCP251XFD_WakeUpFilter
	 * @brief Wake-up Filter Time bits for the CiCON.WFT register
	 * Pulse on RXCAN shorter than the minimum TFILTER time will be ignored;
	 * pulses longer than the maximum TFILTER time will wake-up the device
	 */
	typedef enum
	{
		MCP251XFD_T00FILTER_60ns =
		  0b000, //!< Filter 00: Time min =  40ns (MCP2517FD) /  50ns
		         //!< (MCP2518FD) ; max =  75ns (MCP2517FD) / 100ns (MCP2518FD)
		MCP251XFD_T01FILTER_100ns =
		  0b001, //!< Filter 01: Time min =  70ns (MCP2517FD) /  80ns
		         //!< (MCP2518FD) ; max = 120ns (MCP2517FD) / 140ns (MCP2518FD)
		MCP251XFD_T10FILTER_170ns =
		  0b010, //!< Filter 10: Time min = 125ns (MCP2517FD) / 130ns
		         //!< (MCP2518FD) ; max = 215ns (MCP2517FD) / 220ns (MCP2518FD)
		MCP251XFD_T11FILTER_300ns =
		  0b011, //!< Filter 11: Time min = 225ns (MCP2517FD) / 225ns
		         //!< (MCP2518FD) ; max = 390ns (MCP2517FD) / 390ns (MCP2518FD)
		MCP251XFD_NO_FILTER = 0b111, //!< Do not use a filter for wake-up
	} eMCP251XFD_WakeUpFilter;

	//! CAN Controller Operation Modes for the CiCON.OPMOD and CiCON.REQOP
	//! registers
	typedef enum
	{
		MCP251XFD_NORMAL_CANFD_MODE =
		  0b000, //!< Set Normal CAN FD mode; supports mixing of CAN FD and
		         //!< Classic CAN 2.0 frames
		MCP251XFD_SLEEP_MODE = 0b001, //!< Set Sleep mode
		MCP251XFD_INTERNAL_LOOPBACK_MODE =
		  0b010,                              //!< Set Internal Loopback mode
		MCP251XFD_LISTEN_ONLY_MODE   = 0b011, //!< Set Listen Only mode
		MCP251XFD_CONFIGURATION_MODE = 0b100, //!< Set Configuration mode
		MCP251XFD_EXTERNAL_LOOPBACK_MODE =
		  0b101, //!< Set External Loopback mode
		MCP251XFD_NORMAL_CAN20_MODE =
		  0b110, //!< Set Normal CAN 2.0 mode; possible error frames on CAN FD
		         //!< frames
		MCP251XFD_RESTRICTED_OPERATION_MODE =
		  0b111, //!< Set Restricted Operation mode
	} eMCP251XFD_OperationMode;

	/*! CAN Controller Transmit Bandwidth Sharing bits for the CiCON.TXBWS
	 * register Delay between two consecutive transmissions (in arbitration bit
	 * times)
	 */
	typedef enum
	{
		MCP251XFD_NO_DELAY         = 0b0000, //!< No delay (default)
		MCP251XFD_DELAY_2BIT_TIMES = 0b0001, //!< Delay 2 arbitration bit times
		MCP251XFD_DELAY_4BIT_TIMES = 0b0010, //!< Delay 4 arbitration bit times
		MCP251XFD_DELAY_8BIT_TIMES = 0b0011, //!< Delay 8 arbitration bit times
		MCP251XFD_DELAY_16BIT_TIMES =
		  0b0100, //!< Delay 16 arbitration bit times
		MCP251XFD_DELAY_32BIT_TIMES =
		  0b0101, //!< Delay 32 arbitration bit times
		MCP251XFD_DELAY_64BIT_TIMES =
		  0b0110, //!< Delay 64 arbitration bit times
		MCP251XFD_DELAY_128BIT_TIMES =
		  0b0111, //!< Delay 128 arbitration bit times
		MCP251XFD_DELAY_256BIT_TIMES =
		  0b1000, //!< Delay 256 arbitration bit times
		MCP251XFD_DELAY_512BIT_TIMES =
		  0b1001, //!< Delay 512 arbitration bit times
		MCP251XFD_DELAY_1024BIT_TIMES =
		  0b1010, //!< Delay 1024 arbitration bit times
		MCP251XFD_DELAY_2048BIT_TIMES =
		  0b1011, //!< Delay 2048 arbitration bit times
		MCP251XFD_DELAY_4096BIT_TIMES =
		  0b1100, //!< Delay 4096 arbitration bit times
	} eMCP251XFD_Bandwidth;

	//! Nominal Bit Time Configuration Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiNBTCFG_Register
	{
		uint32_t CiNBTCFG;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t SJW : 7;   //!<  0- 6 - Synchronization Jump Width bits;
			                    //!<  Length is value x TQ
			uint32_t : 1;       //!<  7
			uint32_t TSEG2 : 7; //!<  8-14 - Time Segment 2 bits (Phase Segment
			                    //!<  2); Length is value x TQ
			uint32_t : 1;       //!< 15
			uint32_t
			  TSEG1 : 8; //!< 16-23 - Time Segment 1 bits (Propagation Segment +
			             //!< Phase Segment 1); Length is value x TQ
			uint32_t
			  BRP : 8; //!< 24-31 - Baud Rate Prescaler bits; TQ = value/Fsys
		} Bits;
	} MCP251XFD_CiNBTCFG_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiNBTCFG_Register, 4);

	//! Data Bit Time Configuration Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiDBTCFG_Register
	{
		uint32_t CiDBTCFG;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t SJW : 4;   //!<  0- 3 - Synchronization Jump Width bits;
			                    //!<  Length is value x TQ
			uint32_t : 4;       //!<  7
			uint32_t TSEG2 : 4; //!<  8-11 - Time Segment 2 bits (Phase Segment
			                    //!<  2); Length is value x TQ
			uint32_t : 4;       //!< 15
			uint32_t
			  TSEG1 : 5; //!< 16-20 - Time Segment 1 bits (Propagation Segment +
			             //!< Phase Segment 1); Length is value x TQ
			uint32_t : 3; //!< 21-23
			uint32_t
			  BRP : 8; //!< 24-31 - Baud Rate Prescaler bits; TQ = value/Fsys
		} Bits;
	} MCP251XFD_CiDBTCFG_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiDBTCFG_Register, 4);

	//! Transmitter Delay Compensation Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiTDC_Register
	{
		uint32_t CiTDC;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t TDCV : 6; //!<  0- 5 - Transmitter Delay Compensation Value
			                   //!<  bits; Secondary Sample Point (SSP) Length
			                   //!<  is value x TSYSCLK
			uint32_t : 2;      //!<  6-7
			uint32_t
			  TDCO : 7; //!<  8-14 - Transmitter Delay Compensation Offset bits;
			            //!<  Secondary Sample Point (SSP). Two’s complement;
			            //!<  offset can be positive, zero, or negative; value x
			            //!<  TSYSCLK (min is –64 (0b1111111 x TSYSCLK ; max is
			            //!<  63 (0b0111111) x TSYSCLK)
			uint32_t : 1;        //!< 15
			uint32_t TDCMOD : 2; //!< 16-17 - Transmitter Delay Compensation
			                     //!< Mode bits; Secondary Sample Point (SSP)
			uint32_t : 6;        //!< 18-23
			uint32_t
			  SID11EN : 1; //!< 24    - Enable 12-Bit SID in CAN FD Base Format
			               //!< Messages bit: '1' = RRS is used as SID11 in CAN
			               //!< FD base format messages: SID<11:0> = {SID<10:0>,
			               //!< SID11} ; '0' = Don’t use RRS; SID<10:0>
			               //!< according to ISO 11898-1:2015
			uint32_t
			  EDGFLTEN : 1; //!< 25    - Enable Edge Filtering during Bus
			                //!< Integration state bit: '1' = Edge Filtering
			                //!< enabled, according to ISO 11898-1:2015 ; '0' =
			                //!< Edge Filtering disabled
			uint32_t : 6;   //!< 26-31
		} Bits;
	} MCP251XFD_CiTDC_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiTDC_Register, 4);

	//! Transmitter Delay Compensation Mode bits; Secondary Sample Point (SSP)
	//! for the CiTDC.TDCMOD
	typedef enum
	{
		MCP251XFD_TDC_DISABLED = 0b000, //!< TDC Disabled
		MCP251XFD_MANUAL_MODE =
		  0b001, //!< Manual; Don’t measure, use TDCV + TDCO from register
		MCP251XFD_AUTO_MODE = 0b010, //!< Auto; measure delay and add TDCO
	} eMCP251XFD_TDCMode;

	//! Bit Time statistics structure for CAN speed
	typedef struct MCP251XFD_BitTimeStats
	{
		uint32_t NominalBitrate; //!< This is the actual nominal bitrate
		uint32_t DataBitrate;    //!< This is the actual data bitrate
		uint32_t MaxBusLength; //!< This is the maximum bus length according to
		                       //!< parameters
		uint32_t NSamplePoint; //!< Nominal Sample Point. Should be as close as
		                       //!< possible to 80%
		uint32_t DSamplePoint; //!< Data Sample Point. Should be as close as
		                       //!< possible to 80%
		uint32_t
		  OscTolC1; //!< Condition 1 for the maximum tolerance of the oscillator
		            //!< (Equation 3-12 of MCP25XXFD Family Reference Manual)
		uint32_t
		  OscTolC2; //!< Condition 2 for the maximum tolerance of the oscillator
		            //!< (Equation 3-13 of MCP25XXFD Family Reference Manual)
		uint32_t
		  OscTolC3; //!< Condition 3 for the maximum tolerance of the oscillator
		            //!< (Equation 3-14 of MCP25XXFD Family Reference Manual)
		uint32_t
		  OscTolC4; //!< Condition 4 for the maximum tolerance of the oscillator
		            //!< (Equation 3-15 of MCP25XXFD Family Reference Manual)
		uint32_t
		  OscTolC5; //!< Condition 5 for the maximum tolerance of the oscillator
		            //!< (Equation 3-16 of MCP25XXFD Family Reference Manual)
		uint32_t OscTolerance; //!< Oscillator Tolerance, minimum of conditions
		                       //!< 1-5 (Equation 3-11 of MCP25XXFD Family
		                       //!< Reference Manual)
	} MCP251XFD_BitTimeStats;

	//! Bit Time Configuration structure for CAN speed
	typedef struct MCP251XFD_BitTimeConfig
	{
		//--- Nominal Bit Times ---
		uint32_t NBRP;   //!< Baud Rate Prescaler bits; TQ = value/Fsys
		uint32_t NTSEG1; //!< Time Segment 1 bits (Propagation Segment + Phase
		                 //!< Segment 1); Length is value x TQ
		uint32_t NTSEG2; //!< Time Segment 2 bits (Phase Segment 2); Length is
		                 //!< value x TQ
		uint32_t
		  NSJW; //!< Synchronization Jump Width bits; Length is value x TQ

		//--- Data Bit Times ---
		uint32_t DBRP;   //!< Baud Rate Prescaler bits; TQ = value/Fsys
		uint32_t DTSEG1; //!< Time Segment 1 bits (Propagation Segment + Phase
		                 //!< Segment 1); Length is value x TQ
		uint32_t DTSEG2; //!< Time Segment 2 bits (Phase Segment 2); Length is
		                 //!< value x TQ
		uint32_t
		  DSJW; //!< Synchronization Jump Width bits; Length is value x TQ

		//--- Transmitter Delay Compensation ---
		eMCP251XFD_TDCMode TDCMOD; //!< Transmitter Delay Compensation Mode;
		                           //!< Secondary Sample Point (SSP)
		int32_t
		  TDCO; //!< Transmitter Delay Compensation Offset; Secondary Sample
		        //!< Point (SSP). Two’s complement; offset can be positive,
		        //!< zero, or negative (used as positive only here)
		uint32_t TDCV;    //!< Transmitter Delay Compensation Value; Secondary
		                  //!< Sample Point (SSP)
		bool EDGE_FILTER; //!< Enable Edge Filtering during Bus Integration
		                  //!< state. In case of use of
		                  //!< MCP251XFD_CalculateBitTimeConfiguration() with
		                  //!< CAN-FD this parameter will be set to 'true'

		//--- Result Statistics ---
		MCP251XFD_BitTimeStats *Stats; //!< Point to a stat structure (set to
		                               //!< NULL if no statistics are necessary)
	} MCP251XFD_BitTimeConfig;

	//-----------------------------------------------------------------------------

	//! Time Base Counter Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiTBC_Register
	{
		uint32_t
		  CiTBC; //!< Time Base Counter. This is a free running timer that
		         //!< increments every TBCPRE clocks when TBCEN is set
		uint8_t Bytes[sizeof(uint32_t)];
	} MCP251XFD_CiTBC_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiTBC_Register, 4);

	//! Time Stamp Control Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiTSCON_Register
	{
		uint32_t CiTSCON;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t TBCPRE : 10; //!<  0- 9 - Time Base Counter Prescaler bits.
			                      //!<  TBC increments every 'value' clocks
			uint32_t : 6;         //!< 10-15
			uint32_t TBCEN : 1; //!< 16    - Time Base Counter Enable bit: '1' =
			                    //!< Enable TBC ; '0' = Stop and reset TBC
			uint32_t
			  TSEOF : 1; //!< 17    - Time Stamp EOF bit: '1' = Time Stamp when
			             //!< frame is taken valid: RX no error until last but
			             //!< one bit of EOF, TX no error until the end of EOF ;
			             //!< '0' = Time Stamp at "beginning" of Frame:
			             //!< Classical Frame: at sample point of SOF, FD Frame:
			             //!< see TSRES bit
			uint32_t
			  TSRES : 1; //!< 18    - Time Stamp res bit (FD Frames only): '1' =
			             //!< at sample point of the bit following the FDF bit ;
			             //!< '0' = at sample point of SOF
			uint32_t : 13; //!< 19-31
		} Bits;
	} MCP251XFD_CiTSCON_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiTSCON_Register, 4);

	//! TimeStamp sample point
	typedef enum
	{
		MCP251XFD_TS_CAN20_SOF = 0b00, //!< Time Stamp at "beginning" of Frame:
		                               //!< CAN2.0 at sample point of SOF
		MCP251XFD_TS_CAN20_SOF_CANFD_SOF =
		  0b00, //!< Time Stamp at "beginning" of Frame: CAN2.0 at sample point
		        //!< of SOF & CAN-FD at sample point of SOF
		MCP251XFD_TS_CAN20_SOF_CANFD_FDF =
		  0b10, //!< Time Stamp at "beginning" of Frame: CAN2.0 at sample point
		        //!< of SOF & CAN-FD at sample point of the bit following the
		        //!< FDF bit
		MCP251XFD_TS_CAN20_EOF = 0b01, //!< Time Stamp at "end-on-frame" of
		                               //!< Frame: CAN2.0 at sample point of EOF
		MCP251XFD_TS_CAN20_EOF_CANFD_EOF =
		  0b01, //!< Time Stamp at "end-on-frame" of Frame: CAN2.0 at sample
		        //!< point of EOF & CAN-FD at sample point of EOF
	} eMCP251XFD_SamplePoint;

	//! Interrupt Code Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiVEC_Register
	{
		uint32_t CiVEC;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t
			  ICODE : 7;  //!<  0- 6 - Interrupt Flag Code bits. If multiple
			              //!<  interrupts are pending, the interrupt with the
			              //!<  highest number will be indicated.
			uint32_t : 1; //!<  7
			uint32_t FILHIT : 5; //!<  8-12 - Filter Hit Number bits
			uint32_t : 3;        //!< 13-15
			uint32_t TXCODE : 7; //!< 16-22 - Transmit Interrupt Flag Code bits
			uint32_t : 1;        //!< 23
			uint32_t RXCODE : 7; //!< 24-30 - Receive Interrupt Flag Code bits
			uint32_t : 1;        //!< 31
		} Bits;
	} MCP251XFD_CiVEC_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiVEC_Register, 4);

	//! Interrupt Flag Code bits for the CiVEC.ICODE
	typedef enum
	{
		MCP251XFD_TXQ_INTERRUPT =
		  0b0000000, //!< TXQ Interrupt (TFIF<0> set). If for RXCODE, FIFO 0
		             //!< can’t receive so this flag code is reserved
		MCP251XFD_FIFO1_INTERRUPT =
		  0b0000001, //!< FIFO  1 Interrupt (TFIF< 1> or RFIF< 1> set)
		MCP251XFD_FIFO2_INTERRUPT =
		  0b0000010, //!< FIFO  2 Interrupt (TFIF< 2> or RFIF< 2> set)
		MCP251XFD_FIFO3_INTERRUPT =
		  0b0000011, //!< FIFO  3 Interrupt (TFIF< 3> or RFIF< 3> set)
		MCP251XFD_FIFO4_INTERRUPT =
		  0b0000100, //!< FIFO  4 Interrupt (TFIF< 4> or RFIF< 4> set)
		MCP251XFD_FIFO5_INTERRUPT =
		  0b0000101, //!< FIFO  5 Interrupt (TFIF< 5> or RFIF< 5> set)
		MCP251XFD_FIFO6_INTERRUPT =
		  0b0000110, //!< FIFO  6 Interrupt (TFIF< 6> or RFIF< 6> set)
		MCP251XFD_FIFO7_INTERRUPT =
		  0b0000111, //!< FIFO  7 Interrupt (TFIF< 7> or RFIF< 7> set)
		MCP251XFD_FIFO8_INTERRUPT =
		  0b0001000, //!< FIFO  8 Interrupt (TFIF< 8> or RFIF< 8> set)
		MCP251XFD_FIFO9_INTERRUPT =
		  0b0001001, //!< FIFO  9 Interrupt (TFIF< 9> or RFIF< 9> set)
		MCP251XFD_FIFO10_INTERRUPT =
		  0b0001010, //!< FIFO 10 Interrupt (TFIF<10> or RFIF<10> set)
		MCP251XFD_FIFO11_INTERRUPT =
		  0b0001011, //!< FIFO 11 Interrupt (TFIF<11> or RFIF<11> set)
		MCP251XFD_FIFO12_INTERRUPT =
		  0b0001100, //!< FIFO 12 Interrupt (TFIF<12> or RFIF<12> set)
		MCP251XFD_FIFO13_INTERRUPT =
		  0b0001101, //!< FIFO 13 Interrupt (TFIF<13> or RFIF<13> set)
		MCP251XFD_FIFO14_INTERRUPT =
		  0b0001110, //!< FIFO 14 Interrupt (TFIF<14> or RFIF<14> set)
		MCP251XFD_FIFO15_INTERRUPT =
		  0b0001111, //!< FIFO 15 Interrupt (TFIF<15> or RFIF<15> set)
		MCP251XFD_FIFO16_INTERRUPT =
		  0b0010000, //!< FIFO 16 Interrupt (TFIF<16> or RFIF<16> set)
		MCP251XFD_FIFO17_INTERRUPT =
		  0b0010001, //!< FIFO 17 Interrupt (TFIF<17> or RFIF<17> set)
		MCP251XFD_FIFO18_INTERRUPT =
		  0b0010010, //!< FIFO 18 Interrupt (TFIF<18> or RFIF<18> set)
		MCP251XFD_FIFO19_INTERRUPT =
		  0b0010011, //!< FIFO 19 Interrupt (TFIF<19> or RFIF<19> set)
		MCP251XFD_FIFO20_INTERRUPT =
		  0b0010100, //!< FIFO 20 Interrupt (TFIF<20> or RFIF<20> set)
		MCP251XFD_FIFO21_INTERRUPT =
		  0b0010101, //!< FIFO 21 Interrupt (TFIF<21> or RFIF<21> set)
		MCP251XFD_FIFO22_INTERRUPT =
		  0b0010110, //!< FIFO 22 Interrupt (TFIF<22> or RFIF<22> set)
		MCP251XFD_FIFO23_INTERRUPT =
		  0b0010111, //!< FIFO 23 Interrupt (TFIF<23> or RFIF<23> set)
		MCP251XFD_FIFO24_INTERRUPT =
		  0b0011000, //!< FIFO 24 Interrupt (TFIF<24> or RFIF<24> set)
		MCP251XFD_FIFO25_INTERRUPT =
		  0b0011001, //!< FIFO 25 Interrupt (TFIF<25> or RFIF<25> set)
		MCP251XFD_FIFO26_INTERRUPT =
		  0b0011010, //!< FIFO 26 Interrupt (TFIF<26> or RFIF<26> set)
		MCP251XFD_FIFO27_INTERRUPT =
		  0b0011011, //!< FIFO 27 Interrupt (TFIF<27> or RFIF<27> set)
		MCP251XFD_FIFO28_INTERRUPT =
		  0b0011100, //!< FIFO 28 Interrupt (TFIF<28> or RFIF<28> set)
		MCP251XFD_FIFO29_INTERRUPT =
		  0b0011101, //!< FIFO 29 Interrupt (TFIF<29> or RFIF<29> set)
		MCP251XFD_FIFO30_INTERRUPT =
		  0b0011110, //!< FIFO 30 Interrupt (TFIF<30> or RFIF<30> set)
		MCP251XFD_FIFO31_INTERRUPT =
		  0b0011111, //!< FIFO 31 Interrupt (TFIF<31> or RFIF<31> set)
		MCP251XFD_NO_INTERRUPT    = 0b1000000, //!< No interrupt
		MCP251XFD_ERROR_INTERRUPT = 0b1000001, //!< Error Interrupt (CERRIF/IE)
		MCP251XFD_WAKEUP_INTERRUPT =
		  0b1000010, //!< Wake-up interrupt (WAKIF/WAKIE)
		MCP251XFD_RECEIVE_FIFO_OVF =
		  0b1000011, //!< Receive FIFO Overflow Interrupt (any bit in CiRXOVIF
		             //!< set)
		MCP251XFD_ADDRESS_ERROR_INTERRUPT =
		  0b1000100, //!< Address Error Interrupt (illegal FIFO address
		             //!< presented to system) (SERRIF/IE)
		MCP251XFD_RXTX_MAB_OVF_UVF =
		  0b1000101, //!< RX/TX MAB Overflow/Underflow (RX: message received
		             //!< before previous message was saved to memory; TX: can't
		             //!< feed TX MAB fast enough to transmit consistent data)
		             //!< (SERRIF/IE)
		MCP251XFD_TBC_OVF_INTERRUPT = 0b1000110, //!< TBC Overflow (TBCIF/IE)
		MCP251XFD_OPMODE_CHANGE_OCCURED =
		  0b1000111, //!< Operation Mode Change Occurred (MODIF/IE)
		MCP251XFD_INVALID_MESSAGE_OCCURED =
		  0b1001000, //!< Invalid Message Occurred (IVMIF/IE)
		MCP251XFD_TRANSMIT_EVENT_FIFO =
		  0b1001001, //!< Transmit Event FIFO Interrupt (any bit in CiTEFIF set)
		MCP251XFD_TRANSMIT_ATTEMPT =
		  0b1001010, //!< Transmit Attempt Interrupt (any bit in CiTXATIF set)
	} eMCP251XFD_InterruptFlagCode;

	//! Interrupt Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiINT_Register
	{
		uint32_t CiINT;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t TXIF : 1; //!<  0    - Transmit FIFO Interrupt Flag bit:
			                   //!<  '1' = Transmit FIFO interrupt pending ; '0'
			                   //!<  = No transmit FIFO interrupts pending
			uint32_t RXIF : 1; //!<  1    - Receive FIFO Interrupt Flag bit: '1'
			                   //!<  = Receive FIFO interrupt pending ; '0' = No
			                   //!<  receive FIFO interrupts pending
			uint32_t TBCIF : 1; //!<  2    - Time Base Counter Overflow
			                    //!<  Interrupt Flag bit (set by hardware and
			                    //!<  cleared by application): '1' = TBC has
			                    //!<  overflowed ; '0' = TBC didn’t overflow
			uint32_t
			  MODIF : 1; //!<  3    - Operation Mode Change Interrupt Flag bit
			             //!<  (set by hardware and cleared by application): '1'
			             //!<  = Operation mode change occurred (OPMOD has
			             //!<  changed) ; '0' = No mode change occurred
			uint32_t TEFIF : 1; //!<  4    - Transmit Event FIFO Interrupt Flag
			                    //!<  bit: '1' = TEF interrupt pending ; '0' =
			                    //!<  No TEF interrupts pe
			uint32_t : 3;       //!<  5- 7
			uint32_t ECCIF : 1; //!<  8    - ECC Error Interrupt Flag bit
			uint32_t SPICRCIF : 1; //!<  9    - SPI CRC Error Interrupt Flag bit
			uint32_t
			  TXATIF : 1; //!< 10    - Transmit Attempt Interrupt Flag bit
			uint32_t
			  RXOVIF : 1; //!< 11    - Receive Object Overflow Interrupt Flag
			              //!< bit: '1' = Receive FIFO overflow occurred ; '0' =
			              //!< No receive FIFO overflow has occurred
			uint32_t SERRIF : 1; //!< 12    - System Error Interrupt Flag bit
			                     //!< (set by hardware and cleared by
			                     //!< application): '1' = A system error
			                     //!< occurred ; '0' = No system error occurred
			uint32_t
			  CERRIF : 1; //!< 13    - CAN Bus Error Interrupt Flag bit (set by
			              //!< hardware and cleared by application)
			uint32_t WAKIF : 1; //!< 14    - Bus Wake Up Interrupt Flag bit (set
			                    //!< by hardware and cleared by application)
			uint32_t
			  IVMIF : 1; //!< 15    - Invalid Message Interrupt Flag bit (set by
			             //!< hardware and cleared by application)
			uint32_t TXIE : 1; //!< 16    - Transmit FIFO Interrupt Enable bit
			uint32_t RXIE : 1; //!< 17    - Receive FIFO Interrupt Enable bit
			uint32_t
			  TBCIE : 1; //!< 18    - Time Base Counter Interrupt Enable bit
			uint32_t MODIE : 1; //!< 19    - Mode Change Interrupt Enable bit
			uint32_t
			  TEFIE : 1;  //!< 20    - Transmit Event FIFO Interrupt Enable bit
			uint32_t : 3; //!< 21-23
			uint32_t ECCIE : 1; //!< 24    - ECC Error Interrupt Enable bit
			uint32_t
			  SPICRCIE : 1; //!< 25    - SPI CRC Error Interrupt Enable bit
			uint32_t
			  TXATIE : 1; //!< 26    - Transmit Attempt Interrupt Enable bit
			uint32_t RXOVIE : 1; //!< 27    - Receive FIFO Overflow Interrupt
			                     //!< Enable bit
			uint32_t SERRIE : 1; //!< 28    - System Error Interrupt Enable bit
			uint32_t CERRIE : 1; //!< 29    - CAN Bus Error Interrupt Enable bit
			uint32_t WAKIE : 1;  //!< 30    - Bus Wake Up Interrupt Enable bit
			uint32_t
			  IVMIE : 1; //!< 31    - Invalid Message Interrupt Enable bit
		} Bits;
	} MCP251XFD_CiINT_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiINT_Register, 4);

	//! Interrupt Events, can be OR'ed.
	typedef enum
	{
		// TEF, TXQ and FIFO interrupts
		MCP251XFD_INT_NO_EVENT = 0x0000, //!< No interrupt events
		MCP251XFD_INT_TX_EVENT =
		  MCP251XFD_CAN_CiINT16_TXIE, //!< Transmit events. Equivalent to INT0
		MCP251XFD_INT_RX_EVENT =
		  MCP251XFD_CAN_CiINT16_RXIE, //!< Receive events. Equivalent to INT1
		MCP251XFD_INT_TEF_EVENT =
		  MCP251XFD_CAN_CiINT16_TEFIE, //!< TEF events. Clearable in specific
		                               //!< FIFO
		MCP251XFD_INT_TX_ATTEMPTS_EVENT =
		  MCP251XFD_CAN_CiINT16_TXATIE, //!< Transmit attempts events. Clearable
		                                //!< in specific FIFO
		MCP251XFD_INT_RX_OVERFLOW_EVENT =
		  MCP251XFD_CAN_CiINT16_RXOVIE, //!< Receive overflow events. Clearable
		                                //!< in specific FIFO
		                                // System interrupts
		MCP251XFD_INT_TIME_BASE_COUNTER_EVENT =
		  MCP251XFD_CAN_CiINT16_TBCIE, //!< Time base counter events. Clearable
		                               //!< in CiINT
		MCP251XFD_INT_OPERATION_MODE_CHANGE_EVENT =
		  MCP251XFD_CAN_CiINT16_MODIE, //!< Operation mode change events.
		                               //!< Clearable in CiINT
		MCP251XFD_INT_RAM_ECC_EVENT =
		  MCP251XFD_CAN_CiINT16_ECCIE, //!< ECC RAM events. Clearable in ECCSTA
		MCP251XFD_INT_SPI_CRC_EVENT =
		  MCP251XFD_CAN_CiINT16_SPICRCIE, //!< SPI CRC events. Clearable in CRC
		MCP251XFD_INT_SYSTEM_ERROR_EVENT =
		  MCP251XFD_CAN_CiINT16_SERRIE, //!< System error events. Clearable in
		                                //!< CiINT
		MCP251XFD_INT_BUS_ERROR_EVENT =
		  MCP251XFD_CAN_CiINT16_CERRIE, //!< Bus error events. Clearable in
		                                //!< CiINT
		MCP251XFD_INT_BUS_WAKEUP_EVENT =
		  MCP251XFD_CAN_CiINT16_WAKIE, //!< Bus wakeup events, only when
		                               //!< sleeping. Clearable in CiINT
		MCP251XFD_INT_RX_INVALID_MESSAGE_EVENT =
		  MCP251XFD_CAN_CiINT16_IVMIE, //!< Invalid receipt message events.
		                               //!< Clearable in CiINT

		MCP251XFD_INT_ENABLE_ALL_EVENTS =
		  MCP251XFD_CAN_INT_ALL_INT, //!< Enable all events
		MCP251XFD_INT_EVENTS_STATUS_FLAGS_MASK =
		  MCP251XFD_CAN_INT_ALL_INT, //!< Events flags mask
		MCP251XFD_INT_CLEARABLE_FLAGS_MASK =
		  MCP251XFD_CAN_INT_CLEARABLE_FLAGS, //!< Clearable in CiINT
	} eMCP251XFD_InterruptEvents;

	typedef eMCP251XFD_InterruptEvents
	  setMCP251XFD_InterruptEvents; //! Set of Interrupt Events (can be OR'ed)

	//-----------------------------------------------------------------------------

	//! Receive Interrupt Status Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiRXIF_Register
	{
		uint32_t CiRXIF;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t : 1;       //!<  0
			uint32_t RFIF1 : 1; //!<  1 - Receive FIFO  1 Interrupt Pending bit:
			                    //!<  '1' = Interrupt is pending ; '0' =
			                    //!<  Interrupt not pending
			uint32_t RFIF2 : 1; //!<  2 - Receive FIFO  2 Interrupt Pending bit:
			                    //!<  '1' = Interrupt is pending ; '0' =
			                    //!<  Interrupt not pending
			uint32_t RFIF3 : 1; //!<  3 - Receive FIFO  3 Interrupt Pending bit:
			                    //!<  '1' = Interrupt is pending ; '0' =
			                    //!<  Interrupt not pending
			uint32_t RFIF4 : 1; //!<  4 - Receive FIFO  4 Interrupt Pending bit:
			                    //!<  '1' = Interrupt is pending ; '0' =
			                    //!<  Interrupt not pending
			uint32_t RFIF5 : 1; //!<  5 - Receive FIFO  5 Interrupt Pending bit:
			                    //!<  '1' = Interrupt is pending ; '0' =
			                    //!<  Interrupt not pending
			uint32_t RFIF6 : 1; //!<  6 - Receive FIFO  6 Interrupt Pending bit:
			                    //!<  '1' = Interrupt is pending ; '0' =
			                    //!<  Interrupt not pending
			uint32_t RFIF7 : 1; //!<  7 - Receive FIFO  7 Interrupt Pending bit:
			                    //!<  '1' = Interrupt is pending ; '0' =
			                    //!<  Interrupt not pending
			uint32_t RFIF8 : 1; //!<  8 - Receive FIFO  8 Interrupt Pending bit:
			                    //!<  '1' = Interrupt is pending ; '0' =
			                    //!<  Interrupt not pending
			uint32_t RFIF9 : 1; //!<  9 - Receive FIFO  9 Interrupt Pending bit:
			                    //!<  '1' = Interrupt is pending ; '0' =
			                    //!<  Interrupt not pending
			uint32_t RFIF10 : 1; //!< 10 - Receive FIFO 10 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF11 : 1; //!< 11 - Receive FIFO 11 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF12 : 1; //!< 12 - Receive FIFO 12 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF13 : 1; //!< 13 - Receive FIFO 13 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF14 : 1; //!< 14 - Receive FIFO 14 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF15 : 1; //!< 15 - Receive FIFO 15 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF16 : 1; //!< 16 - Receive FIFO 16 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF17 : 1; //!< 17 - Receive FIFO 17 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF18 : 1; //!< 18 - Receive FIFO 18 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF19 : 1; //!< 19 - Receive FIFO 19 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF20 : 1; //!< 20 - Receive FIFO 20 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF21 : 1; //!< 21 - Receive FIFO 21 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF22 : 1; //!< 22 - Receive FIFO 22 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF23 : 1; //!< 23 - Receive FIFO 23 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF24 : 1; //!< 24 - Receive FIFO 24 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF25 : 1; //!< 25 - Receive FIFO 25 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF26 : 1; //!< 26 - Receive FIFO 26 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF27 : 1; //!< 27 - Receive FIFO 27 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF28 : 1; //!< 28 - Receive FIFO 28 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF29 : 1; //!< 29 - Receive FIFO 29 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF30 : 1; //!< 30 - Receive FIFO 30 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t RFIF31 : 1; //!< 31 - Receive FIFO 31 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
		} Bits;
	} MCP251XFD_CiRXIF_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiRXIF_Register, 4);

	//! Receive Overflow Interrupt Status Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiRXOVIF_Register
	{
		uint32_t CiRXOVIF;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t : 1;          //!<  0
			uint32_t RFOVIF1 : 1;  //!<  1 - Receive FIFO  1 Overflow Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t RFOVIF2 : 1;  //!<  2 - Receive FIFO  2 Overflow Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t RFOVIF3 : 1;  //!<  3 - Receive FIFO  3 Overflow Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t RFOVIF4 : 1;  //!<  4 - Receive FIFO  4 Overflow Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t RFOVIF5 : 1;  //!<  5 - Receive FIFO  5 Overflow Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t RFOVIF6 : 1;  //!<  6 - Receive FIFO  6 Overflow Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t RFOVIF7 : 1;  //!<  7 - Receive FIFO  7 Overflow Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t RFOVIF8 : 1;  //!<  8 - Receive FIFO  8 Overflow Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t RFOVIF9 : 1;  //!<  9 - Receive FIFO  9 Overflow Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t RFOVIF10 : 1; //!< 10 - Receive FIFO 10 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF11 : 1; //!< 11 - Receive FIFO 11 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF12 : 1; //!< 12 - Receive FIFO 12 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF13 : 1; //!< 13 - Receive FIFO 13 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF14 : 1; //!< 14 - Receive FIFO 14 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF15 : 1; //!< 15 - Receive FIFO 15 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF16 : 1; //!< 16 - Receive FIFO 16 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF17 : 1; //!< 17 - Receive FIFO 17 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF18 : 1; //!< 18 - Receive FIFO 18 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF19 : 1; //!< 19 - Receive FIFO 19 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF20 : 1; //!< 20 - Receive FIFO 20 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF21 : 1; //!< 21 - Receive FIFO 21 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF22 : 1; //!< 22 - Receive FIFO 22 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF23 : 1; //!< 23 - Receive FIFO 23 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF24 : 1; //!< 24 - Receive FIFO 24 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF25 : 1; //!< 25 - Receive FIFO 25 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF26 : 1; //!< 26 - Receive FIFO 26 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF27 : 1; //!< 27 - Receive FIFO 27 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF28 : 1; //!< 28 - Receive FIFO 28 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF29 : 1; //!< 29 - Receive FIFO 29 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF30 : 1; //!< 30 - Receive FIFO 30 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t RFOVIF31 : 1; //!< 31 - Receive FIFO 31 Overflow Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
		} Bits;
	} MCP251XFD_CiRXOVIF_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiRXOVIF_Register, 4);

	//! Transmit Interrupt Status Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiTXIF_Register
	{
		uint32_t CiTXIF;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t TFIF0 : 1;  //!<  0 - Transmit TXQ Interrupt Pending bit:
			                     //!<  '1' = Interrupt is pending ; '0' =
			                     //!<  Interrupt not pending
			uint32_t TFIF1 : 1;  //!<  1 - Transmit FIFO  1 Interrupt Pending
			                     //!<  bit: '1' = Interrupt is pending ; '0' =
			                     //!<  Interrupt not pending
			uint32_t TFIF2 : 1;  //!<  2 - Transmit FIFO  2 Interrupt Pending
			                     //!<  bit: '1' = Interrupt is pending ; '0' =
			                     //!<  Interrupt not pending
			uint32_t TFIF3 : 1;  //!<  3 - Transmit FIFO  3 Interrupt Pending
			                     //!<  bit: '1' = Interrupt is pending ; '0' =
			                     //!<  Interrupt not pending
			uint32_t TFIF4 : 1;  //!<  4 - Transmit FIFO  4 Interrupt Pending
			                     //!<  bit: '1' = Interrupt is pending ; '0' =
			                     //!<  Interrupt not pending
			uint32_t TFIF5 : 1;  //!<  5 - Transmit FIFO  5 Interrupt Pending
			                     //!<  bit: '1' = Interrupt is pending ; '0' =
			                     //!<  Interrupt not pending
			uint32_t TFIF6 : 1;  //!<  6 - Transmit FIFO  6 Interrupt Pending
			                     //!<  bit: '1' = Interrupt is pending ; '0' =
			                     //!<  Interrupt not pending
			uint32_t TFIF7 : 1;  //!<  7 - Transmit FIFO  7 Interrupt Pending
			                     //!<  bit: '1' = Interrupt is pending ; '0' =
			                     //!<  Interrupt not pending
			uint32_t TFIF8 : 1;  //!<  8 - Transmit FIFO  8 Interrupt Pending
			                     //!<  bit: '1' = Interrupt is pending ; '0' =
			                     //!<  Interrupt not pending
			uint32_t TFIF9 : 1;  //!<  9 - Transmit FIFO  9 Interrupt Pending
			                     //!<  bit: '1' = Interrupt is pending ; '0' =
			                     //!<  Interrupt not pending
			uint32_t TFIF10 : 1; //!< 10 - Transmit FIFO 10 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF11 : 1; //!< 11 - Transmit FIFO 11 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF12 : 1; //!< 12 - Transmit FIFO 12 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF13 : 1; //!< 13 - Transmit FIFO 13 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF14 : 1; //!< 14 - Transmit FIFO 14 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF15 : 1; //!< 15 - Transmit FIFO 15 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF16 : 1; //!< 16 - Transmit FIFO 16 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF17 : 1; //!< 17 - Transmit FIFO 17 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF18 : 1; //!< 18 - Transmit FIFO 18 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF19 : 1; //!< 19 - Transmit FIFO 19 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF20 : 1; //!< 20 - Transmit FIFO 20 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF21 : 1; //!< 21 - Transmit FIFO 21 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF22 : 1; //!< 22 - Transmit FIFO 22 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF23 : 1; //!< 23 - Transmit FIFO 23 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF24 : 1; //!< 24 - Transmit FIFO 24 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF25 : 1; //!< 25 - Transmit FIFO 25 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF26 : 1; //!< 26 - Transmit FIFO 26 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF27 : 1; //!< 27 - Transmit FIFO 27 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF28 : 1; //!< 28 - Transmit FIFO 28 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF29 : 1; //!< 29 - Transmit FIFO 29 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF30 : 1; //!< 30 - Transmit FIFO 30 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
			uint32_t TFIF31 : 1; //!< 31 - Transmit FIFO 31 Interrupt Pending
			                     //!< bit: '1' = Interrupt is pending ; '0' =
			                     //!< Interrupt not pending
		} Bits;
	} MCP251XFD_CiTXIF_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiTXIF_Register, 4);

	//! Transmit Attempt Interrupt Status Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiTXATIF_Register
	{
		uint32_t CiTXATIF;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t TFATIF0 : 1;  //!<  0 - Transmit TXQ Attempt Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t TFATIF1 : 1;  //!<  1 - Transmit FIFO  1 Attempt Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t TFATIF2 : 1;  //!<  2 - Transmit FIFO  2 Attempt Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t TFATIF3 : 1;  //!<  3 - Transmit FIFO  3 Attempt Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t TFATIF4 : 1;  //!<  4 - Transmit FIFO  4 Attempt Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t TFATIF5 : 1;  //!<  5 - Transmit FIFO  5 Attempt Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t TFATIF6 : 1;  //!<  6 - Transmit FIFO  6 Attempt Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t TFATIF7 : 1;  //!<  7 - Transmit FIFO  7 Attempt Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t TFATIF8 : 1;  //!<  8 - Transmit FIFO  8 Attempt Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t TFATIF9 : 1;  //!<  9 - Transmit FIFO  9 Attempt Interrupt
			                       //!<  Pending bit: '1' = Interrupt is pending
			                       //!<  ; '0' = Interrupt not pending
			uint32_t TFATIF10 : 1; //!< 10 - Transmit FIFO 10 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF11 : 1; //!< 11 - Transmit FIFO 11 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF12 : 1; //!< 12 - Transmit FIFO 12 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF13 : 1; //!< 13 - Transmit FIFO 13 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF14 : 1; //!< 14 - Transmit FIFO 14 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF15 : 1; //!< 15 - Transmit FIFO 15 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF16 : 1; //!< 16 - Transmit FIFO 16 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF17 : 1; //!< 17 - Transmit FIFO 17 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF18 : 1; //!< 18 - Transmit FIFO 18 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF19 : 1; //!< 19 - Transmit FIFO 19 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF20 : 1; //!< 20 - Transmit FIFO 20 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF21 : 1; //!< 21 - Transmit FIFO 21 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF22 : 1; //!< 22 - Transmit FIFO 22 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF23 : 1; //!< 23 - Transmit FIFO 23 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF24 : 1; //!< 24 - Transmit FIFO 24 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF25 : 1; //!< 25 - Transmit FIFO 25 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF26 : 1; //!< 26 - Transmit FIFO 26 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF27 : 1; //!< 27 - Transmit FIFO 27 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF28 : 1; //!< 28 - Transmit FIFO 28 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF29 : 1; //!< 29 - Transmit FIFO 29 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF30 : 1; //!< 30 - Transmit FIFO 30 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
			uint32_t TFATIF31 : 1; //!< 31 - Transmit FIFO 31 Attempt Interrupt
			                       //!< Pending bit: '1' = Interrupt is pending
			                       //!< ; '0' = Interrupt not pending
		} Bits;
	} MCP251XFD_CiTXATIF_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiTXATIF_Register, 4);

	//! Receive Interrupt Status for the CiRXIF, CiRXOVIF, CiTXIF and CiTXATIF
	//! registers. Can be or'ed
	typedef enum
	{
		MCP251XFD_INTERRUPT_ON_TXQ =
		  0x00000001, //!< Interrupt is pending on TXQ (TFIF<0> or TFATIF<0>
		              //!< set)
		MCP251XFD_INTERRUPT_ON_FIFO1 =
		  0x00000002, //!< Interrupt is pending on FIFO  1 (TFIF< 1> or TFATIF<
		              //!< 1> or RFIF< 1> or RFOVIF< 1> set)
		MCP251XFD_INTERRUPT_ON_FIFO2 =
		  0x00000004, //!< Interrupt is pending on FIFO  2 (TFIF< 2> or TFATIF<
		              //!< 2> or RFIF< 2> or RFOVIF< 2> set)
		MCP251XFD_INTERRUPT_ON_FIFO3 =
		  0x00000008, //!< Interrupt is pending on FIFO  3 (TFIF< 3> or TFATIF<
		              //!< 3> or RFIF< 3> or RFOVIF< 3> set)
		MCP251XFD_INTERRUPT_ON_FIFO4 =
		  0x00000010, //!< Interrupt is pending on FIFO  4 (TFIF< 4> or TFATIF<
		              //!< 4> or RFIF< 4> or RFOVIF< 4> set)
		MCP251XFD_INTERRUPT_ON_FIFO5 =
		  0x00000020, //!< Interrupt is pending on FIFO  5 (TFIF< 5> or TFATIF<
		              //!< 5> or RFIF< 5> or RFOVIF< 5> set)
		MCP251XFD_INTERRUPT_ON_FIFO6 =
		  0x00000040, //!< Interrupt is pending on FIFO  6 (TFIF< 6> or TFATIF<
		              //!< 6> or RFIF< 6> or RFOVIF< 6> set)
		MCP251XFD_INTERRUPT_ON_FIFO7 =
		  0x00000080, //!< Interrupt is pending on FIFO  7 (TFIF< 7> or TFATIF<
		              //!< 7> or RFIF< 7> or RFOVIF< 7> set)
		MCP251XFD_INTERRUPT_ON_FIFO8 =
		  0x00000100, //!< Interrupt is pending on FIFO  8 (TFIF< 8> or TFATIF<
		              //!< 8> or RFIF< 8> or RFOVIF< 8> set)
		MCP251XFD_INTERRUPT_ON_FIFO9 =
		  0x00000200, //!< Interrupt is pending on FIFO  9 (TFIF< 9> or TFATIF<
		              //!< 9> or RFIF< 9> or RFOVIF< 9> set)
		MCP251XFD_INTERRUPT_ON_FIFO10 =
		  0x00000400, //!< Interrupt is pending on FIFO 10 (TFIF<10> or
		              //!< TFATIF<10> or RFIF<10> or RFOVIF<10> set)
		MCP251XFD_INTERRUPT_ON_FIFO11 =
		  0x00000800, //!< Interrupt is pending on FIFO 11 (TFIF<11> or
		              //!< TFATIF<11> or RFIF<11> or RFOVIF<11> set)
		MCP251XFD_INTERRUPT_ON_FIFO12 =
		  0x00001000, //!< Interrupt is pending on FIFO 12 (TFIF<12> or
		              //!< TFATIF<12> or RFIF<12> or RFOVIF<12> set)
		MCP251XFD_INTERRUPT_ON_FIFO13 =
		  0x00002000, //!< Interrupt is pending on FIFO 13 (TFIF<13> or
		              //!< TFATIF<13> or RFIF<13> or RFOVIF<13> set)
		MCP251XFD_INTERRUPT_ON_FIFO14 =
		  0x00004000, //!< Interrupt is pending on FIFO 14 (TFIF<14> or
		              //!< TFATIF<14> or RFIF<14> or RFOVIF<14> set)
		MCP251XFD_INTERRUPT_ON_FIFO15 =
		  0x00008000, //!< Interrupt is pending on FIFO 15 (TFIF<15> or
		              //!< TFATIF<15> or RFIF<15> or RFOVIF<15> set)
		MCP251XFD_INTERRUPT_ON_FIFO16 =
		  0x00010000, //!< Interrupt is pending on FIFO 16 (TFIF<16> or
		              //!< TFATIF<16> or RFIF<16> or RFOVIF<16> set)
		MCP251XFD_INTERRUPT_ON_FIFO17 =
		  0x00020000, //!< Interrupt is pending on FIFO 17 (TFIF<17> or
		              //!< TFATIF<17> or RFIF<17> or RFOVIF<17> set)
		MCP251XFD_INTERRUPT_ON_FIFO18 =
		  0x00040000, //!< Interrupt is pending on FIFO 18 (TFIF<18> or
		              //!< TFATIF<18> or RFIF<18> or RFOVIF<18> set)
		MCP251XFD_INTERRUPT_ON_FIFO19 =
		  0x00080000, //!< Interrupt is pending on FIFO 19 (TFIF<19> or
		              //!< TFATIF<19> or RFIF<19> or RFOVIF<19> set)
		MCP251XFD_INTERRUPT_ON_FIFO20 =
		  0x00100000, //!< Interrupt is pending on FIFO 20 (TFIF<20> or
		              //!< TFATIF<20> or RFIF<20> or RFOVIF<20> set)
		MCP251XFD_INTERRUPT_ON_FIFO21 =
		  0x00200000, //!< Interrupt is pending on FIFO 21 (TFIF<21> or
		              //!< TFATIF<21> or RFIF<21> or RFOVIF<21> set)
		MCP251XFD_INTERRUPT_ON_FIFO22 =
		  0x00400000, //!< Interrupt is pending on FIFO 22 (TFIF<22> or
		              //!< TFATIF<22> or RFIF<22> or RFOVIF<22> set)
		MCP251XFD_INTERRUPT_ON_FIFO23 =
		  0x00800000, //!< Interrupt is pending on FIFO 23 (TFIF<23> or
		              //!< TFATIF<23> or RFIF<23> or RFOVIF<23> set)
		MCP251XFD_INTERRUPT_ON_FIFO24 =
		  0x01000000, //!< Interrupt is pending on FIFO 24 (TFIF<24> or
		              //!< TFATIF<24> or RFIF<24> or RFOVIF<24> set)
		MCP251XFD_INTERRUPT_ON_FIFO25 =
		  0x02000000, //!< Interrupt is pending on FIFO 25 (TFIF<25> or
		              //!< TFATIF<25> or RFIF<25> or RFOVIF<25> set)
		MCP251XFD_INTERRUPT_ON_FIFO26 =
		  0x04000000, //!< Interrupt is pending on FIFO 26 (TFIF<26> or
		              //!< TFATIF<26> or RFIF<26> or RFOVIF<26> set)
		MCP251XFD_INTERRUPT_ON_FIFO27 =
		  0x08000000, //!< Interrupt is pending on FIFO 27 (TFIF<27> or
		              //!< TFATIF<27> or RFIF<27> or RFOVIF<27> set)
		MCP251XFD_INTERRUPT_ON_FIFO28 =
		  0x10000000, //!< Interrupt is pending on FIFO 28 (TFIF<28> or
		              //!< TFATIF<28> or RFIF<28> or RFOVIF<28> set)
		MCP251XFD_INTERRUPT_ON_FIFO29 =
		  0x20000000, //!< Interrupt is pending on FIFO 29 (TFIF<29> or
		              //!< TFATIF<29> or RFIF<29> or RFOVIF<29> set)
		MCP251XFD_INTERRUPT_ON_FIFO30 =
		  0x40000000, //!< Interrupt is pending on FIFO 30 (TFIF<30> or
		              //!< TFATIF<30> or RFIF<30> or RFOVIF<30> set)
		MCP251XFD_INTERRUPT_ON_FIFO31 =
		  0x80000000, //!< Interrupt is pending on FIFO 31 (TFIF<31> or
		              //!< TFATIF<31> or RFIF<31> or RFOVIF<31> set)
	} eMCP251XFD_InterruptOnFIFO;

	typedef eMCP251XFD_InterruptOnFIFO
	  setMCP251XFD_InterruptOnFIFO; //! Set of Receive Interrupt Status (can be
	                                //! OR'ed)

	//-----------------------------------------------------------------------------

	/*! Transmit Request Register
	 * Bits can NOT be used for aborting a transmission
	 */
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiTXREQ_Register
	{
		uint32_t CiTXREQ;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t
			  TXREQ0 : 1; //!<  0 - Transmit Queue Message Send Request bit:
			              //!<  Setting this bit to '1' requests sending a
			              //!<  message ; The bit will automatically clear when
			              //!<  the message(s) queued in the object is (are)
			              //!<  successfully sent
			uint32_t
			  TXREQ1 : 1; //!<  1 - Message Send FIFO  1 Request bit (TXEN=1):
			              //!<  Setting this bit to '1' requests sending a
			              //!<  message ; The bit will automatically clear when
			              //!<  the message(s) queued in the object is (are)
			              //!<  successfully sent
			uint32_t
			  TXREQ2 : 1; //!<  2 - Message Send FIFO  2 Request bit (TXEN=1):
			              //!<  Setting this bit to '1' requests sending a
			              //!<  message ; The bit will automatically clear when
			              //!<  the message(s) queued in the object is (are)
			              //!<  successfully sent
			uint32_t
			  TXREQ3 : 1; //!<  3 - Message Send FIFO  3 Request bit (TXEN=1):
			              //!<  Setting this bit to '1' requests sending a
			              //!<  message ; The bit will automatically clear when
			              //!<  the message(s) queued in the object is (are)
			              //!<  successfully sent
			uint32_t
			  TXREQ4 : 1; //!<  4 - Message Send FIFO  4 Request bit (TXEN=1):
			              //!<  Setting this bit to '1' requests sending a
			              //!<  message ; The bit will automatically clear when
			              //!<  the message(s) queued in the object is (are)
			              //!<  successfully sent
			uint32_t
			  TXREQ5 : 1; //!<  5 - Message Send FIFO  5 Request bit (TXEN=1):
			              //!<  Setting this bit to '1' requests sending a
			              //!<  message ; The bit will automatically clear when
			              //!<  the message(s) queued in the object is (are)
			              //!<  successfully sent
			uint32_t
			  TXREQ6 : 1; //!<  6 - Message Send FIFO  6 Request bit (TXEN=1):
			              //!<  Setting this bit to '1' requests sending a
			              //!<  message ; The bit will automatically clear when
			              //!<  the message(s) queued in the object is (are)
			              //!<  successfully sent
			uint32_t
			  TXREQ7 : 1; //!<  7 - Message Send FIFO  7 Request bit (TXEN=1):
			              //!<  Setting this bit to '1' requests sending a
			              //!<  message ; The bit will automatically clear when
			              //!<  the message(s) queued in the object is (are)
			              //!<  successfully sent
			uint32_t
			  TXREQ8 : 1; //!<  8 - Message Send FIFO  8 Request bit (TXEN=1):
			              //!<  Setting this bit to '1' requests sending a
			              //!<  message ; The bit will automatically clear when
			              //!<  the message(s) queued in the object is (are)
			              //!<  successfully sent
			uint32_t
			  TXREQ9 : 1; //!<  9 - Message Send FIFO  9 Request bit (TXEN=1):
			              //!<  Setting this bit to '1' requests sending a
			              //!<  message ; The bit will automatically clear when
			              //!<  the message(s) queued in the object is (are)
			              //!<  successfully sent
			uint32_t
			  TXREQ10 : 1; //!< 10 - Message Send FIFO 10 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ11 : 1; //!< 11 - Message Send FIFO 11 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ12 : 1; //!< 12 - Message Send FIFO 12 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ13 : 1; //!< 13 - Message Send FIFO 13 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ14 : 1; //!< 14 - Message Send FIFO 14 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ15 : 1; //!< 15 - Message Send FIFO 15 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ16 : 1; //!< 16 - Message Send FIFO 16 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ17 : 1; //!< 17 - Message Send FIFO 17 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ18 : 1; //!< 18 - Message Send FIFO 18 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ19 : 1; //!< 19 - Message Send FIFO 19 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ20 : 1; //!< 20 - Message Send FIFO 20 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ21 : 1; //!< 21 - Message Send FIFO 21 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ22 : 1; //!< 22 - Message Send FIFO 22 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ23 : 1; //!< 23 - Message Send FIFO 23 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ24 : 1; //!< 24 - Message Send FIFO 24 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ25 : 1; //!< 25 - Message Send FIFO 25 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ26 : 1; //!< 26 - Message Send FIFO 26 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ27 : 1; //!< 27 - Message Send FIFO 27 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ28 : 1; //!< 28 - Message Send FIFO 28 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ29 : 1; //!< 29 - Message Send FIFO 29 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ30 : 1; //!< 30 - Message Send FIFO 30 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
			uint32_t
			  TXREQ31 : 1; //!< 31 - Message Send FIFO 31 Request bit (TXEN=1):
			               //!< Setting this bit to '1' requests sending a
			               //!< message ; The bit will automatically clear when
			               //!< the message(s) queued in the object is (are)
			               //!< successfully sent
		} Bits;
	} MCP251XFD_CiTXREQ_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiTXREQ_Register, 4);

	//! Transmit/Receive error count Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiTREC_Register
	{
		uint32_t CiTREC;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t REC : 8;   //!<  0- 7 - Receive Error Counter bits
			uint32_t TEC : 8;   //!<  8-15 - Transmit Error Counter bits
			uint32_t EWARN : 1; //!< 16    - Transmitter or Receiver is in Error
			                    //!< Warning State bit
			uint32_t RXWARN : 1; //!< 17    - Receiver in Error Warning State
			                     //!< bit (128 > REC > 95)
			uint32_t TXWARN : 1; //!< 18    - Transmitter in Error Warning State
			                     //!< bit (128 > TEC > 95)
			uint32_t RXBP : 1; //!< 19    - Receiver in Error Passive State bit
			                   //!< (REC > 127)
			uint32_t TXBP : 1; //!< 20    - Transmitter in Error Passive State
			                   //!< bit (TEC > 127)
			uint32_t TXBO : 1; //!< 21    - Transmitter in Bus Off State bit
			                   //!< (TEC > 255). In Configuration mode, TXBO is
			                   //!< set, since the module is not on the bus
			uint32_t : 10;     //!< 22-31
		} Bits;
	} MCP251XFD_CiTREC_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiTREC_Register, 4);

	//! Transmit and Receive Error status
	typedef enum
	{
		MCP251XFD_ERROR_STATUS_FLAGS_MASK =
		  MCP251XFD_CAN_CiTREC8_ALL_ERROR, //!< Error Status Flags Mask
		MCP251XFD_TX_RX_WARNING_STATE =
		  MCP251XFD_CAN_CiTREC8_EWARN, //!< Transmitter or Receiver is in Error
		                               //!< Warning State

		// Transmit Error status
		MCP251XFD_TX_NO_ERROR = 0x00, //!< No Transmit Error
		MCP251XFD_TX_WARNING_STATE =
		  MCP251XFD_CAN_CiTREC8_TXWARN, //!< Transmitter in Error Warning State
		MCP251XFD_TX_BUS_PASSIVE_STATE =
		  MCP251XFD_CAN_CiTREC8_TXBP, //!< Transmitter in Error Passive State
		MCP251XFD_TX_BUS_OFF_STATE =
		  MCP251XFD_CAN_CiTREC8_TXBO, //!< Transmitter in Bus Off State
		MCP251XFD_TX_ERROR_MASK =
		  MCP251XFD_CAN_CiTREC8_TX_ERROR, //!< Transmitter Error Mask

		// Receive Error status
		MCP251XFD_RX_NO_ERROR = 0x00, //!< No Receive Error
		MCP251XFD_RX_WARNING_STATE =
		  MCP251XFD_CAN_CiTREC8_RXWARN, //!< Receiver in Error Warning State
		MCP251XFD_RX_BUS_PASSIVE_STATE =
		  MCP251XFD_CAN_CiTREC8_RXBP, //!< Receiver in Error Passive State
		MCP251XFD_RX_ERROR_MASK =
		  MCP251XFD_CAN_CiTREC8_RX_ERROR, //!< Receiver Error Mask
	} eMCP251XFD_TXRXErrorStatus;

	//-----------------------------------------------------------------------------

	//! Bus Diagnostic Register 0
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiBDIAG0_Register
	{
		uint32_t CiBDIAG0;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint8_t NominalBitRateReceiveErrorCount; //!< Nominal Bit Rate
			                                         //!< Receive Error Counter
			uint8_t
			  NominalBitRateTransmitErrorCount;   //!< Nominal Bit Rate Transmit
			                                      //!< Error Counter
			uint8_t DataBitRateReceiveErrorCount; //!< Data Bit Rate Receive
			                                      //!< Error Counter
			uint8_t DataBitRateTransmitErrorCount; //!< Data Bit Rate Transmit
			                                       //!< Error Counter
		} Reg;
		struct
		{
			uint32_t NRERRCNT : 8; //!<  0- 7 - Nominal Bit Rate Receive Error
			                       //!<  Counter bits
			uint32_t NTERRCNT : 8; //!<  8-15 - Nominal Bit Rate Transmit Error
			                       //!<  Counter bits
			uint32_t DRERRCNT : 8; //!< 16-23 - Data Bit Rate Receive Error
			                       //!< Counter bits
			uint32_t DTERRCNT : 8; //!< 24-31 - Data Bit Rate Transmit Error
			                       //!< Counter bits
		} Bits;
	} MCP251XFD_CiBDIAG0_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiBDIAG0_Register, 4);

	//-----------------------------------------------------------------------------

	//! Transmit and Receive Error status
	MCP251XFD_PACKENUM(eMCP251XFD_DiagStatus, uint16_t){
	  MCP251XFD_DIAG_MASK = 0xFBBF,

	  // Nominal bitrate diag
	  MCP251XFD_DIAG_NBIT0_ERR =
	    0x0001, //!< Normal Bitrate: During the transmission of a message (or
	            //!< acknowledge bit, or active error flag, or overload flag),
	            //!< the device wanted to send a dominant level (data or
	            //!< identifier bit logical value ‘0’), but the monitored bus
	            //!< value was recessive
	  MCP251XFD_DIAG_NBIT1_ERR =
	    0x0002, //!< Normal Bitrate: During the transmission of a message (with
	            //!< the exception of the arbitration field), the device wanted
	            //!< to send a recessive level (bit of logical value '1'), but
	            //!< the monitored bus value was dominant
	  MCP251XFD_DIAG_NACK_ERR =
	    0x0004, //!< Normal Bitrate: Transmitted message was not acknowledged
	  MCP251XFD_DIAG_NFORM_ERR =
	    0x0008, //!< Normal Bitrate: A fixed format part of a received frame has
	            //!< the wrong format
	  MCP251XFD_DIAG_NSTUFF_ERR =
	    0x0010, //!< Normal Bitrate: More than 5 equal bits in a sequence have
	            //!< occurred in a part of a received message where this is not
	            //!< allowed
	  MCP251XFD_DIAG_NCRC_ERR =
	    0x0020, //!< Normal Bitrate: The CRC check sum of a received message was
	            //!< incorrect. The CRC of an incoming message does not match
	            //!< with the CRC calculated from the received data
	  MCP251XFD_DIAG_TXBO_ERR =
	    0x0080, //!< Device went to bus-off (and auto-recovered)
	  // Data bitrate diag
	  MCP251XFD_DIAG_DBIT0_ERR =
	    0x0100, //!< Data Bitrate: During the transmission of a message (or
	            //!< acknowledge bit, or active error flag, or overload flag),
	            //!< the device wanted to send a dominant level (data or
	            //!< identifier bit logical value ‘0’), but the monitored bus
	            //!< value was recessive
	  MCP251XFD_DIAG_DBIT1_ERR =
	    0x0200, //!< Data Bitrate: During the transmission of a message (with
	            //!< the exception of the arbitration field), the device wanted
	            //!< to send a recessive level (bit of logical value '1'), but
	            //!< the monitored bus value was dominant
	  MCP251XFD_DIAG_DFORM_ERR =
	    0x0800, //!< Data Bitrate: A fixed format part of a received frame has
	            //!< the wrong format
	  MCP251XFD_DIAG_DSTUFF_ERR =
	    0x1000, //!< Data Bitrate: More than 5 equal bits in a sequence have
	            //!< occurred in a part of a received message where this is not
	            //!< allowed
	  MCP251XFD_DIAG_DCRC_ERR =
	    0x2000, //!< Data Bitrate: The CRC check sum of a received message was
	            //!< incorrect. The CRC of an incoming message does not match
	            //!< with the CRC calculated from the received data
	  MCP251XFD_DIAG_ESI_SET =
	    0x4000, //!< ESI flag of a received CAN FD message was set
	  MCP251XFD_DIAG_DLC_MISMATCH =
	    0x8000, //!< DLC Mismatch bit. During a transmission or reception, the
	            //!< specified DLC is larger than the PLSIZE of the FIFO element
	} MCP251XFD_UNPACKENUM(eMCP251XFD_DiagStatus);
	MCP251XFD_CONTROL_ITEM_SIZE(eMCP251XFD_DiagStatus, 2);

	//-----------------------------------------------------------------------------

	//! Bus Diagnostics Register 1
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiBDIAG1_Register
	{
		uint32_t CiBDIAG1;
		uint16_t Uint16[sizeof(uint32_t) / sizeof(uint16_t)];
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint16_t ErrorFreeCounter;   //!< Error Free Message Counter
			eMCP251XFD_DiagStatus Flags; //!< Transmit and Receive Error status
		} Reg;
		struct
		{
			uint32_t EFMSGCNT : 16; //!<  0-15 - Error Free Message Counter bits
			uint32_t
			  NBIT0ERR : 1; //!< 16    - Normal Bitrate: During the transmission
			                //!< of a message (or acknowledge bit, or active
			                //!< error flag, or overload flag), the device
			                //!< wanted to send a dominant level (data or
			                //!< identifier bit logical value ‘0’), but the
			                //!< monitored bus value was recessive
			uint32_t
			  NBIT1ERR : 1; //!< 17    - Normal Bitrate: During the transmission
			                //!< of a message (with the exception of the
			                //!< arbitration field), the device wanted to send a
			                //!< recessive level (bit of logical value '1'), but
			                //!< the monitored bus value was dominant
			uint32_t NACKERR : 1; //!< 18    - Normal Bitrate: Transmitted
			                      //!< message was not acknowledged
			uint32_t
			  NFORMERR : 1; //!< 19    - Normal Bitrate: A fixed format part of
			                //!< a received frame has the wrong format
			uint32_t
			  NSTUFERR : 1; //!< 20    - Normal Bitrate: More than 5 equal bits
			                //!< in a sequence have occurred in a part of a
			                //!< received message where this is not allowed
			uint32_t
			  NCRCERR : 1; //!< 21    - Normal Bitrate: The CRC check sum of a
			               //!< received message was incorrect. The CRC of an
			               //!< incoming message does not match with the CRC
			               //!< calculated from the received data
			uint32_t : 1;  //!< 22
			uint32_t TXBOERR : 1; //!< 23    - Device went to bus-off (and
			                      //!< auto-recovered)
			uint32_t
			  DBIT0ERR : 1; //!< 24    - Data Bitrate: During the transmission
			                //!< of a message (or acknowledge bit, or active
			                //!< error flag, or overload flag), the device
			                //!< wanted to send a dominant level (data or
			                //!< identifier bit logical value ‘0’), but the
			                //!< monitored bus value was recessive
			uint32_t
			  DBIT1ERR : 1; //!< 25    - Data Bitrate: During the transmission
			                //!< of a message (with the exception of the
			                //!< arbitration field), the device wanted to send a
			                //!< recessive level (bit of logical value '1'), but
			                //!< the monitored bus value was dominant
			uint32_t : 1;   //!< 26
			uint32_t
			  DFORMERR : 1; //!< 27    - Data Bitrate: A fixed format part of a
			                //!< received frame has the wrong format
			uint32_t
			  DSTUFERR : 1; //!< 28    - Data Bitrate: More than 5 equal bits in
			                //!< a sequence have occurred in a part of a
			                //!< received message where this is not allowed
			uint32_t
			  DCRCERR : 1;    //!< 29    - Data Bitrate: The CRC check sum of a
			                  //!< received message was incorrect. The CRC of an
			                  //!< incoming message does not match with the CRC
			                  //!< calculated from the received data
			uint32_t ESI : 1; //!< 30    - ESI flag of a received CAN FD message
			                  //!< was set
			uint32_t
			  DLCMM : 1; //!< 31    - DLC Mismatch bit. During a transmission or
			             //!< reception, the specified DLC is larger than the
			             //!< PLSIZE of the FIFO element
		} Bits;
	} MCP251XFD_CiBDIAG1_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiBDIAG1_Register, 4);

	//! Available FIFO list
	typedef enum
	{
		MCP251XFD_TEF    = -1, //!< TEF - Transmit Event FIFO
		MCP251XFD_TXQ    = 0,  //!< TXQ - Transmit Queue
		MCP251XFD_FIFO1  = 1,  //!< FIFO  1
		MCP251XFD_FIFO2  = 2,  //!< FIFO  2
		MCP251XFD_FIFO3  = 3,  //!< FIFO  3
		MCP251XFD_FIFO4  = 4,  //!< FIFO  4
		MCP251XFD_FIFO5  = 5,  //!< FIFO  5
		MCP251XFD_FIFO6  = 6,  //!< FIFO  6
		MCP251XFD_FIFO7  = 7,  //!< FIFO  7
		MCP251XFD_FIFO8  = 8,  //!< FIFO  8
		MCP251XFD_FIFO9  = 9,  //!< FIFO  9
		MCP251XFD_FIFO10 = 10, //!< FIFO 10
		MCP251XFD_FIFO11 = 11, //!< FIFO 11
		MCP251XFD_FIFO12 = 12, //!< FIFO 12
		MCP251XFD_FIFO13 = 13, //!< FIFO 13
		MCP251XFD_FIFO14 = 14, //!< FIFO 14
		MCP251XFD_FIFO15 = 15, //!< FIFO 15
		MCP251XFD_FIFO16 = 16, //!< FIFO 16
		MCP251XFD_FIFO17 = 17, //!< FIFO 17
		MCP251XFD_FIFO18 = 18, //!< FIFO 18
		MCP251XFD_FIFO19 = 19, //!< FIFO 19
		MCP251XFD_FIFO20 = 20, //!< FIFO 20
		MCP251XFD_FIFO21 = 21, //!< FIFO 21
		MCP251XFD_FIFO22 = 22, //!< FIFO 22
		MCP251XFD_FIFO23 = 23, //!< FIFO 23
		MCP251XFD_FIFO24 = 24, //!< FIFO 24
		MCP251XFD_FIFO25 = 25, //!< FIFO 25
		MCP251XFD_FIFO26 = 26, //!< FIFO 26
		MCP251XFD_FIFO27 = 27, //!< FIFO 27
		MCP251XFD_FIFO28 = 28, //!< FIFO 28
		MCP251XFD_FIFO29 = 29, //!< FIFO 29
		MCP251XFD_FIFO30 = 30, //!< FIFO 30
		MCP251XFD_FIFO31 = 31, //!< FIFO 31
		MCP251XFD_FIFO_COUNT,  // Keep it last
		MCP251XFD_NO_FIFO =
		  MCP251XFD_FIFO_COUNT, //!< No FIFO code for FIFO status functions
	} eMCP251XFD_FIFO;

	//-----------------------------------------------------------------------------

	//! Transmit Event FIFO Control Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiTEFCON_Register
	{
		uint32_t CiTEFCON;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t TEFNEIE : 1; //!<  0    - Transmit Event FIFO Not Empty
			                      //!<  Interrupt Enable bit: '1' = Interrupt
			                      //!<  enabled for FIFO not empty ; '0' =
			                      //!<  Interrupt disabled for FIFO not empty
			uint32_t TEFHIE : 1;  //!<  1    - Transmit Event FIFO Half Full
			                      //!<  Interrupt Enable bit: '1' = Interrupt
			                      //!<  enabled for FIFO half full ; '0' =
			                      //!<  Interrupt disabled for FIFO half full
			uint32_t
			  TEFFIE : 1; //!<  2    - Transmit Event FIFO Full Interrupt Enable
			              //!<  bit: '1' = Interrupt enabled for FIFO full ; '0'
			              //!<  = Interrupt disabled for FIFO full
			uint32_t TEFOVIE : 1; //!<  3    - Transmit Event FIFO Overflow
			                      //!<  Interrupt Enable bit: '1' = Interrupt
			                      //!<  enabled for overflow event ; '0' =
			                      //!<  Interrupt disabled for overflow event
			uint32_t : 1;         //!<  4
			uint32_t
			  TEFTSEN : 1; //!<  5    - Transmit Event FIFO Time Stamp Enable
			               //!<  bit (This bits can only be modified in
			               //!<  Configuration mode): '1' = Time Stamp objects
			               //!<  in TEF ; '0' = Don’t Time Stamp objects in TEF
			uint32_t : 2; //!<  6- 7
			uint32_t
			  UINC : 1;   //!<  8    - Increment Tail bit. When this bit is set,
			              //!<  the FIFO tail will increment by a single message
			uint32_t : 1; //!<  9
			uint32_t
			  FRESET : 1;  //!< 10    - FIFO Reset bit: '1' = FIFO will be reset
			               //!< when bit is set, cleared by hardware when FIFO
			               //!< was reset. The user should wait for this bit to
			               //!< clear before taking any action ; '0' = No effect
			uint32_t : 13; //!< 11-23
			uint32_t FSIZE : 5; //!< 24-28 - FIFO Size bits
			uint32_t : 3;       //!< 29-31
		} Bits;
	} MCP251XFD_CiTEFCON_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiTEFCON_Register, 4);

	//! FIFO Size for the CiTEFCON.FSIZE, CiTXQCON.FSIZE and CiFIFOCONm.FSIZE
	typedef enum
	{
		MCP251XFD_FIFO_1_MESSAGE_DEEP  = 0b000000, //!< FIFO is  1 Message deep
		MCP251XFD_FIFO_2_MESSAGE_DEEP  = 0b000001, //!< FIFO is  2 Message deep
		MCP251XFD_FIFO_3_MESSAGE_DEEP  = 0b000010, //!< FIFO is  3 Message deep
		MCP251XFD_FIFO_4_MESSAGE_DEEP  = 0b000011, //!< FIFO is  4 Message deep
		MCP251XFD_FIFO_5_MESSAGE_DEEP  = 0b000100, //!< FIFO is  5 Message deep
		MCP251XFD_FIFO_6_MESSAGE_DEEP  = 0b000101, //!< FIFO is  6 Message deep
		MCP251XFD_FIFO_7_MESSAGE_DEEP  = 0b000110, //!< FIFO is  7 Message deep
		MCP251XFD_FIFO_8_MESSAGE_DEEP  = 0b000111, //!< FIFO is  8 Message deep
		MCP251XFD_FIFO_9_MESSAGE_DEEP  = 0b001000, //!< FIFO is  9 Message deep
		MCP251XFD_FIFO_10_MESSAGE_DEEP = 0b001001, //!< FIFO is 10 Message deep
		MCP251XFD_FIFO_11_MESSAGE_DEEP = 0b001010, //!< FIFO is 11 Message deep
		MCP251XFD_FIFO_12_MESSAGE_DEEP = 0b001011, //!< FIFO is 12 Message deep
		MCP251XFD_FIFO_13_MESSAGE_DEEP = 0b001100, //!< FIFO is 13 Message deep
		MCP251XFD_FIFO_14_MESSAGE_DEEP = 0b001101, //!< FIFO is 14 Message deep
		MCP251XFD_FIFO_15_MESSAGE_DEEP = 0b001110, //!< FIFO is 15 Message deep
		MCP251XFD_FIFO_16_MESSAGE_DEEP = 0b001111, //!< FIFO is 16 Message deep
		MCP251XFD_FIFO_17_MESSAGE_DEEP = 0b010000, //!< FIFO is 17 Message deep
		MCP251XFD_FIFO_18_MESSAGE_DEEP = 0b010001, //!< FIFO is 18 Message deep
		MCP251XFD_FIFO_19_MESSAGE_DEEP = 0b010010, //!< FIFO is 19 Message deep
		MCP251XFD_FIFO_20_MESSAGE_DEEP = 0b010011, //!< FIFO is 20 Message deep
		MCP251XFD_FIFO_21_MESSAGE_DEEP = 0b010100, //!< FIFO is 21 Message deep
		MCP251XFD_FIFO_22_MESSAGE_DEEP = 0b010101, //!< FIFO is 22 Message deep
		MCP251XFD_FIFO_23_MESSAGE_DEEP = 0b010110, //!< FIFO is 23 Message deep
		MCP251XFD_FIFO_24_MESSAGE_DEEP = 0b010111, //!< FIFO is 24 Message deep
		MCP251XFD_FIFO_25_MESSAGE_DEEP = 0b011000, //!< FIFO is 25 Message deep
		MCP251XFD_FIFO_26_MESSAGE_DEEP = 0b011001, //!< FIFO is 26 Message deep
		MCP251XFD_FIFO_27_MESSAGE_DEEP = 0b011010, //!< FIFO is 27 Message deep
		MCP251XFD_FIFO_28_MESSAGE_DEEP = 0b011011, //!< FIFO is 28 Message deep
		MCP251XFD_FIFO_29_MESSAGE_DEEP = 0b011100, //!< FIFO is 29 Message deep
		MCP251XFD_FIFO_30_MESSAGE_DEEP = 0b011101, //!< FIFO is 30 Message deep
		MCP251XFD_FIFO_31_MESSAGE_DEEP = 0b011110, //!< FIFO is 31 Message deep
		MCP251XFD_FIFO_32_MESSAGE_DEEP = 0b011111, //!< FIFO is 32 Message deep
	} eMCP251XFD_MessageDeep;

	//-----------------------------------------------------------------------------

	//! Transmit Event FIFO Status Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiTEFSTA_Register
	{
		uint32_t CiTEFSTA;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t
			  TEFNEIF : 1; //!<  0    - Transmit Event FIFO Not Empty Interrupt
			               //!<  Flag bit (This bit is read only and reflects
			               //!<  the status of the FIFO): '1' = FIFO is not
			               //!<  empty, contains at least one message ; '0' =
			               //!<  FIFO is empty
			uint32_t
			  TEFHIF : 1; //!<  1    - Transmit Event FIFO Half Full Interrupt
			              //!<  Flag bit (This bit is read only and reflects the
			              //!<  status of the FIFO): '1' = FIFO is ≥ half full ;
			              //!<  '0' = FIFO is < half full
			uint32_t TEFFIF : 1; //!<  2    - Transmit Event FIFO Full Interrupt
			                     //!<  Flag bit (This bit is read only and
			                     //!<  reflects the status of the FIFO): '1' =
			                     //!<  FIFO is full ; '0' = FIFO is not full
			uint32_t
			  TEFOVIF : 1; //!<  3    - Transmit Event FIFO Overflow Interrupt
			               //!<  Flag bit: '1' = Overflow event has occurred
			               //!<  ;'0' = No overflow event occurred
			uint32_t : 28; //!<  4-31
		} Bits;
	} MCP251XFD_CiTEFSTA_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiTEFSTA_Register, 4);

	//! Transmit Event FIFO status
	typedef enum
	{
		MCP251XFD_TEF_FIFO_EMPTY = 0x00, //!< TEF FIFO empty
		MCP251XFD_TEF_FIFO_NOT_EMPTY =
		  MCP251XFD_CAN_CiTEFSTA8_TEFNEIF, //!< TEF FIFO not empty
		MCP251XFD_TEF_FIFO_HALF_FULL =
		  MCP251XFD_CAN_CiTEFSTA8_TEFHIF, //!< TEF FIFO half full
		MCP251XFD_TEF_FIFO_FULL =
		  MCP251XFD_CAN_CiTEFSTA8_TEFFIF, //!< TEF FIFO full
		MCP251XFD_TEF_FIFO_OVERFLOW =
		  MCP251XFD_CAN_CiTEFSTA8_TEFOVIF, //!< TEF overflow
		MCP251XFD_TEF_FIFO_STATUS_MASK =
		  MCP251XFD_CAN_CiTEFSTA8_ALL_EVENTS, //!< TEF status mask
	} eMCP251XFD_TEFstatus;

	typedef eMCP251XFD_TEFstatus
	  setMCP251XFD_TEFstatus; //! Set of Transmit Event FIFO status (can be
	                          //! OR'ed)

	//-----------------------------------------------------------------------------

	/*! Time Base Counter Register
	 * This register is not guaranteed to read correctly in Configuration mode
	 * and should only be accessed when the module is not in Configuration mode
	 */
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiTEFUA_Register
	{
		uint32_t CiTEFUA; //!< Transmit Event FIFO User Address bits. A read of
		                  //!< this register will return the address where the
		                  //!< next object is to be read (FIFO tail)
		uint8_t Bytes[sizeof(uint32_t)];
	} MCP251XFD_CiTEFUA_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiTEFUA_Register, 4);

	//-----------------------------------------------------------------------------

	//! Transmit Queue Control Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiTXQCON_Register
	{
		uint32_t CiTXQCON;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t
			  TXQNIE : 1; //!<  0    - Transmit Queue Not Full Interrupt Enable
			              //!<  bit: '1' = Interrupt enabled for TXQ not full ;
			              //!<  '0' = Interrupt disabled for TXQ not full
			uint32_t : 1; //!<  1
			uint32_t
			  TXQEIE : 1; //!<  2    - Transmit Queue Empty Interrupt Enable
			              //!<  bit: '1' = Interrupt enabled for TXQ empty ; '0'
			              //!<  = Interrupt disabled for TXQ empty
			uint32_t : 1; //!<  3
			uint32_t TXATIE : 1; //!<  4    - Transmit Attempts Exhausted
			                     //!<  Interrupt Enable bit: '1' = Enable
			                     //!<  interrupt ; '0' = Disable interrupt
			uint32_t : 2;        //!<  5- 6
			uint32_t TXEN : 1;   //!<  7    - TX Enable: '1' = Transmit Message
			                     //!<  Queue. This bit always reads as '1'.
			uint32_t
			  UINC : 1; //!<  8    - Increment Head bit: When this bit is set,
			            //!<  the FIFO head will increment by a single message
			uint32_t
			  TXREQ : 1; //!<  9    - Message Send Request bit (This bit is
			             //!<  updated when a message completes (or aborts) or
			             //!<  when the FIFO is reset): '1' = Requests sending a
			             //!<  message; the bit will automatically clear when
			             //!<  all the messages queued in the TXQ are
			             //!<  successfully sent ; '0' = Clearing the bit to '0'
			             //!<  while set ('1') will request a message abort.
			uint32_t
			  FRESET : 1; //!< 10    - FIFO Reset bit (FRESET is set while in
			              //!< Configuration mode and is automatically cleared
			              //!< in Normal mode): '1' = FIFO will be reset when
			              //!< bit is set; cleared by hardware when FIFO was
			              //!< reset. User should wait until this bit is clear
			              //!< before taking any action ; '0' = No effect
			uint32_t : 5; //!< 11-15
			uint32_t TXPRI : 5; //!< 16-20 - Message Transmit Priority bits
			uint32_t TXAT : 2;  //!< 21-22 - Retransmission Attempts bits. This
			                    //!< feature is enabled when CiCON.RTXAT is set.
			uint32_t : 1;       //!< 23
			uint32_t FSIZE : 5; //!< 24-28 - FIFO Size
			uint32_t PLSIZE : 3; //!< 29-31 - Payload Size
		} Bits;
	} MCP251XFD_CiTXQCON_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiTXQCON_Register, 4);

	//! Message Transmit Priority for the CiTXQCON.TXPRI
	typedef enum
	{
		MCP251XFD_MESSAGE_TX_PRIORITY1 =
		  0b00000, //!< Message transmit priority  1 (Lowest)
		MCP251XFD_MESSAGE_TX_PRIORITY2 =
		  0b00001, //!< Message transmit priority  2
		MCP251XFD_MESSAGE_TX_PRIORITY3 =
		  0b00010, //!< Message transmit priority  3
		MCP251XFD_MESSAGE_TX_PRIORITY4 =
		  0b00011, //!< Message transmit priority  4
		MCP251XFD_MESSAGE_TX_PRIORITY5 =
		  0b00100, //!< Message transmit priority  5
		MCP251XFD_MESSAGE_TX_PRIORITY6 =
		  0b00101, //!< Message transmit priority  6
		MCP251XFD_MESSAGE_TX_PRIORITY7 =
		  0b00110, //!< Message transmit priority  7
		MCP251XFD_MESSAGE_TX_PRIORITY8 =
		  0b00111, //!< Message transmit priority  8
		MCP251XFD_MESSAGE_TX_PRIORITY9 =
		  0b01000, //!< Message transmit priority  9
		MCP251XFD_MESSAGE_TX_PRIORITY10 =
		  0b01001, //!< Message transmit priority 10
		MCP251XFD_MESSAGE_TX_PRIORITY11 =
		  0b01010, //!< Message transmit priority 11
		MCP251XFD_MESSAGE_TX_PRIORITY12 =
		  0b01011, //!< Message transmit priority 12
		MCP251XFD_MESSAGE_TX_PRIORITY13 =
		  0b01100, //!< Message transmit priority 13
		MCP251XFD_MESSAGE_TX_PRIORITY14 =
		  0b01101, //!< Message transmit priority 14
		MCP251XFD_MESSAGE_TX_PRIORITY15 =
		  0b01110, //!< Message transmit priority 15
		MCP251XFD_MESSAGE_TX_PRIORITY16 =
		  0b01111, //!< Message transmit priority 16
		MCP251XFD_MESSAGE_TX_PRIORITY17 =
		  0b10000, //!< Message transmit priority 17
		MCP251XFD_MESSAGE_TX_PRIORITY18 =
		  0b10001, //!< Message transmit priority 18
		MCP251XFD_MESSAGE_TX_PRIORITY19 =
		  0b10010, //!< Message transmit priority 19
		MCP251XFD_MESSAGE_TX_PRIORITY20 =
		  0b10011, //!< Message transmit priority 20
		MCP251XFD_MESSAGE_TX_PRIORITY21 =
		  0b10100, //!< Message transmit priority 21
		MCP251XFD_MESSAGE_TX_PRIORITY22 =
		  0b10101, //!< Message transmit priority 22
		MCP251XFD_MESSAGE_TX_PRIORITY23 =
		  0b10110, //!< Message transmit priority 23
		MCP251XFD_MESSAGE_TX_PRIORITY24 =
		  0b10111, //!< Message transmit priority 24
		MCP251XFD_MESSAGE_TX_PRIORITY25 =
		  0b11000, //!< Message transmit priority 25
		MCP251XFD_MESSAGE_TX_PRIORITY26 =
		  0b11001, //!< Message transmit priority 26
		MCP251XFD_MESSAGE_TX_PRIORITY27 =
		  0b11010, //!< Message transmit priority 27
		MCP251XFD_MESSAGE_TX_PRIORITY28 =
		  0b11011, //!< Message transmit priority 28
		MCP251XFD_MESSAGE_TX_PRIORITY29 =
		  0b11100, //!< Message transmit priority 29
		MCP251XFD_MESSAGE_TX_PRIORITY30 =
		  0b11101, //!< Message transmit priority 30
		MCP251XFD_MESSAGE_TX_PRIORITY31 =
		  0b11110, //!< Message transmit priority 31
		MCP251XFD_MESSAGE_TX_PRIORITY32 =
		  0b11111, //!< Message transmit priority 32 (Highest)
	} eMCP251XFD_Priority;

	//! Retransmission Attempts for the CiTXQCON.TXAT and CiFIFOCONm.TXAT
	typedef enum
	{
		MCP251XFD_DISABLE_ATTEMPS = 0b00, //!< Disable retransmission attempts
		MCP251XFD_THREE_ATTEMPTS  = 0b01, //!< Three retransmission attempts
		MCP251XFD_UNLIMITED_ATTEMPTS =
		  0b10, //!< Unlimited number of retransmission attempts
	} eMCP251XFD_Attempts;

	//! Payload Size for the CiTXQCON.PLSIZE and CiFIFOCONm.PLSIZE
	typedef enum
	{
		MCP251XFD_PAYLOAD_8BYTE  = 0b000, //!< Payload  8 data bytes
		MCP251XFD_PAYLOAD_12BYTE = 0b001, //!< Payload 12 data bytes
		MCP251XFD_PAYLOAD_16BYTE = 0b010, //!< Payload 16 data bytes
		MCP251XFD_PAYLOAD_20BYTE = 0b011, //!< Payload 20 data bytes
		MCP251XFD_PAYLOAD_24BYTE = 0b100, //!< Payload 24 data bytes
		MCP251XFD_PAYLOAD_32BYTE = 0b101, //!< Payload 32 data bytes
		MCP251XFD_PAYLOAD_48BYTE = 0b110, //!< Payload 48 data bytes
		MCP251XFD_PAYLOAD_64BYTE = 0b111, //!< Payload 64 data bytes
		MCP251XFD_PAYLOAD_COUNT,          // Keep last
	} eMCP251XFD_PayloadSize;

	//-----------------------------------------------------------------------------

	//! Transmit Queue Status Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiTXQSTA_Register
	{
		uint32_t CiTXQSTA;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t
			  TXQNIF : 1; //!<  0    - Transmit Queue Not Full Interrupt Flag
			              //!<  bit: '1' = TXQ is not full ; '0' = TXQ is full
			uint32_t : 1; //!<  1
			uint32_t
			  TXQEIF : 1; //!<  2    - Transmit Queue Empty Interrupt Flag bit:
			              //!<  '1' = TXQ is empty ; '0' = TXQ is not empty, at
			              //!<  least 1 message queued to be transmitted
			uint32_t : 1; //!<  3
			uint32_t TXATIF : 1; //!<  4    - Transmit Attempts Exhausted
			                     //!<  Interrupt Pending bit: '1' = Interrupt
			                     //!<  pending ; '0' = Interrupt Not pending
			uint32_t
			  TXERR : 1; //!<  5    - Error Detected During Transmission bit.
			             //!<  This bit is cleared when TXREQ is set or by
			             //!<  writing a 0 using the SPI. This bit is updated
			             //!<  when a message completes (or aborts) or when the
			             //!<  TXQ is reset: '1' = A bus error occurred while
			             //!<  the message was being sent ; '0' = A bus error
			             //!<  did not occur while the message was being sent
			uint32_t
			  TXLARB : 1; //!<  6    - Message Lost Arbitration Status bit. This
			              //!<  bit is cleared when TXREQ is set or by writing a
			              //!<  0 using the SPI. This bit is updated when a
			              //!<  message completes (or aborts) or when the TXQ is
			              //!<  reset: '1' = Message lost arbitration while
			              //!<  being sent ; '0' = Message did not loose
			              //!<  arbitration while being sent
			uint32_t
			  TXABT : 1; //!<  7    - Message Aborted Status bit. This bit is
			             //!<  cleared when TXREQ is set or by writing a 0 using
			             //!<  the SPI. This bit is updated when a message
			             //!<  completes (or aborts) or when the TXQ is reset:
			             //!<  '1' = Message was aborted ; '0' = Message
			             //!<  completed successfully
			uint32_t
			  TXQCI : 5; //!<  8-12 - Transmit Queue Message Index bits.
			             //!<  TXQCI<4:0> gives a zero-indexed value to the
			             //!<  message in the TXQ. If the TXQ is 4 messages deep
			             //!<  (FSIZE=5) TXQCI will take on a value of 0 to 3
			             //!<  depending on the state of the TXQ: A read of this
			             //!<  register will return an index to the message that
			             //!<  the FIFO will next attempt to transmit
			uint32_t : 19; //!< 13-31
		} Bits;
	} MCP251XFD_CiTXQSTA_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiTXQSTA_Register, 4);

	//! Transmit Queue status
	typedef enum
	{
		MCP251XFD_TXQ_FULL = 0x00, //!< TXQ full
		MCP251XFD_TXQ_NOT_FULL =
		  MCP251XFD_CAN_CiTXQSTA8_TXQNIF,                     //!< TXQ not full
		MCP251XFD_TXQ_EMPTY = MCP251XFD_CAN_CiTXQSTA8_TXQEIF, //!< TXQ empty
		MCP251XFD_TXQ_ATTEMPTS_EXHAUSTED =
		  MCP251XFD_CAN_CiTXQSTA8_TXATIF, //!< TXQ attempts exhausted
		MCP251XFD_TXQ_BUS_ERROR =
		  MCP251XFD_CAN_CiTXQSTA8_TXERR, //!< TXQ bus error
		MCP251XFD_TXQ_ARBITRATION_LOST =
		  MCP251XFD_CAN_CiTXQSTA8_TXLARB, //!< TXQ arbitration lost
		MCP251XFD_TXQ_ABORTED = MCP251XFD_CAN_CiTXQSTA8_TXABT, //!< TXQ aborted
		MCP251XFD_TXQ_STATUS_MASK =
		  MCP251XFD_CAN_CiTXQSTA8_ALL_EVENTS, //!< TXQ status mask
	} eMCP251XFD_TXQstatus;

	typedef eMCP251XFD_TXQstatus
	  setMCP251XFD_TXQstatus; //! Set of Transmit Queue status (can be OR'ed)

	//-----------------------------------------------------------------------------

	/*! Transmit Queue User Address Register
	 * This register is not guaranteed to read correctly in Configuration mode
	 * and should only be accessed when the module is not in Configuration mode
	 */
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiTXQUA_Register
	{
		uint32_t CiTXQUA; //!< TXQ User Address bits. A read of this register
		                  //!< will return the address where the next message is
		                  //!< to be written (TXQ head)
		uint8_t Bytes[sizeof(uint32_t)];
	} MCP251XFD_CiTXQUA_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiTXQUA_Register, 4);

	//-----------------------------------------------------------------------------

	//! FIFO Control Register m, (m = 1 to 31)
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiFIFOCONm_Register
	{
		uint32_t CiFIFOCONm;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t
			  TFNRFNIE : 1; //!<  0    - Transmit/Receive FIFO Not Full/Not
			                //!<  Empty Interrupt Enable bit. If TXEN=1 (FIFO
			                //!<  configured as a Transmit FIFO), transmit FIFO
			                //!<  Not Full Interrupt Enable: '1' = Interrupt
			                //!<  enabled for FIFO not full ; '0' = Interrupt
			                //!<  disabled for FIFO not full. If TXEN=0 (FIFO
			                //!<  configured as a Receive FIFO), receive FIFO
			                //!<  Not Empty Interrupt Enable: '1' = Interrupt
			                //!<  enabled for FIFO not empty ; '0' = Interrupt
			                //!<  disabled for FIFO not empty
			uint32_t
			  TFHRFHIE : 1; //!<  1    - Transmit/Receive FIFO Half Empty/Half
			                //!<  Full Interrupt Enable bit. If TXEN=1 (FIFO
			                //!<  configured as a Transmit FIFO), transmit FIFO
			                //!<  Half Empty Interrupt Enable: '1' = Interrupt
			                //!<  enabled for FIFO half empty ; '0' = Interrupt
			                //!<  disabled for FIFO half empty. If TXEN = 0
			                //!<  (FIFO configured as a Receive FIFO), receive
			                //!<  FIFO Half Full Interrupt Enable: '1' =
			                //!<  Interrupt enabled for FIFO half full ; '0' =
			                //!<  Interrupt disabled for FIFO half full
			uint32_t
			  TFERFFIE : 1; //!<  2    - Transmit/Receive FIFO Empty/Full
			                //!<  Interrupt Enable bit. If TXEN=1 (FIFO
			                //!<  configured as a Transmit FIFO), transmit FIFO
			                //!<  Empty Interrupt Enable: '1' = Interrupt
			                //!<  enabled for FIFO empty ; '0' = Interrupt
			                //!<  disabled for FIFO empty. If TXEN=0 (FIFO
			                //!<  configured as a Receive FIFO), receive FIFO
			                //!<  Full Interrupt Enable: '1' = Interrupt enabled
			                //!<  for FIFO full ; '0' = Interrupt disabled for
			                //!<  FIFO full
			uint32_t
			  RXOVIE : 1; //!<  3    - Overflow Interrupt Enable bit: '1' =
			              //!<  Interrupt enabled for overflow event ; '0' =
			              //!<  Interrupt disabled for overflow event
			uint32_t TXATIE : 1; //!<  4    - Transmit Attempts Exhausted
			                     //!<  Interrupt Enable bit: '1' = Enable
			                     //!<  interrupt ; '0' = Disable interrupt
			uint32_t RXTSEN : 1; //!<  5    - Received Message Time Stamp Enable
			                     //!<  bit (This bit can only be modified in
			                     //!<  Configuration mode): '1' = Capture time
			                     //!<  stamp in received message object in RAM ;
			                     //!<  '0' = Don’t capture time stamp
			uint32_t RTREN : 1;  //!<  6    - Auto RTR Enable bit: '1' = When a
			                    //!<  remote transmit is received, TXREQ will be
			                    //!<  set ; '0' = When a remote transmit is
			                    //!<  received, TXREQ will be unaffected
			uint32_t
			  TXEN : 1; //!<  7    - TX/RX FIFO Selection bit (This bit can only
			            //!<  be modified in Configuration mode): '1' = Transmit
			            //!<  FIFO ; '0' = Receive FIFO
			uint32_t
			  UINC : 1; //!<  8    - Increment Head/Tail bit. If TXEN=1 (FIFO
			            //!<  configured as a Transmit FIFO), when this bit is
			            //!<  set, the FIFO head will increment by a single
			            //!<  message. If TXEN=0 (FIFO configured as a Receive
			            //!<  FIFO), when this bit is set, the FIFO tail will
			            //!<  increment by a single message
			uint32_t
			  TXREQ : 1; //!<  9    - Message Send Request bit. This bit is
			             //!<  updated when a message completes (or aborts) or
			             //!<  when the FIFO is reset. If TXEN=0 (FIFO
			             //!<  configured as a Receive FIFO), this bit has no
			             //!<  effect. If TXEN=1 (FIFO configured as a Transmit
			             //!<  FIFO): '1' = Requests sending a message; the bit
			             //!<  will automatically clear when all the messages
			             //!<  queued in the FIFO are successfully sent. '0' =
			             //!<  Clearing the bit to '0' while set ('1') will
			             //!<  request a message abort
			uint32_t
			  FRESET : 1; //!< 10    - FIFO Reset bit. FRESET is set while in
			              //!< Configuration mode and is automatically cleared
			              //!< in Normal mode: '1' = FIFO will be reset when bit
			              //!< is set; cleared by hardware when FIFO was reset.
			              //!< User should wait until this bit is clear before
			              //!< taking any action ; '0' = No effect
			uint32_t : 5; //!< 11-15
			uint32_t TXPRI : 5;  //!< 16-20 - Message Transmit Priority bits
			uint32_t TXAT : 2;   //!< 21-22 - Retransmission Attempts bits
			uint32_t : 1;        //!< 23
			uint32_t FSIZE : 5;  //!< 24-28 - FIFO Size bits
			uint32_t PLSIZE : 3; //!< 29-31 - Payload Size bits
		} Bits;
	} MCP251XFD_CiFIFOCONm_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiFIFOCONm_Register, 4);

	//! FIFO Direction for the CiFIFOCONm.TXEN
	typedef enum
	{
		MCP251XFD_RECEIVE_FIFO  = 0b0, //!< Receive FIFO
		MCP251XFD_TRANSMIT_FIFO = 0b1, //!< Transmit FIFO
	} eMCP251XFD_SelTXRX;

	//-----------------------------------------------------------------------------

	//! FIFO Status Register m, (m = 1 to 31)
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiFIFOSTAm_Register
	{
		uint32_t CiFIFOSTAm;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t
			  TFNRFNIF : 1; //!<  0    - Transmit/Receive FIFO Not Full/Not
			                //!<  Empty Interrupt Flag bit. If TXEN=1 (FIFO is
			                //!<  configured as a Transmit FIFO), transmit FIFO
			                //!<  Not Full Interrupt Flag: '1' = FIFO is not
			                //!<  full ; '0' = FIFO is full. If TXEN=0 (FIFO is
			                //!<  configured as a Receive FIFO), receive FIFO
			                //!<  Not Empty Interrupt Flag: '1' = FIFO is not
			                //!<  empty, contains at least one message ; '0' =
			                //!<  FIFO is empty
			uint32_t
			  TFHRFHIF : 1; //!<  1    - Transmit/Receive FIFO Half Empty/Half
			                //!<  Full Interrupt Flag bit. If TXEN=1 (FIFO is
			                //!<  configured as a Transmit FIFO), transmit FIFO
			                //!<  Half Empty Interrupt Flag: '1' = FIFO is ≤
			                //!<  half full ; '0' = FIFO is > half full. If
			                //!<  TXEN=0 (FIFO is configured as a Receive FIFO),
			                //!<  receive FIFO Half Full Interrupt Flag: '1' =
			                //!<  FIFO is ≥ half full ; '0' = FIFO is < half
			                //!<  full
			uint32_t
			  TFERFFIF : 1; //!<  2    - Transmit/Receive FIFO Empty/Full
			                //!<  Interrupt Flag bit. If TXEN=1 (FIFO is
			                //!<  configured as a Transmit FIFO), transmit FIFO
			                //!<  Empty Interrupt Flag: '1' = FIFO is empty ;
			                //!<  '0' = FIFO is not empty; at least one message
			                //!<  queued to be transmitted. If TXEN=0 (FIFO is
			                //!<  configured as a Receive FIFO), receive FIFO
			                //!<  Full Interrupt Flag: '1' = FIFO is full ; '0'
			                //!<  = FIFO is not full
			uint32_t
			  RXOVIF : 1; //!<  3    - Receive FIFO Overflow Interrupt Flag bit.
			              //!<  If TXEN=1 (FIFO is configured as a Transmit
			              //!<  FIFO), unused, Read as '0'. If TXEN=0 (FIFO is
			              //!<  configured as a Receive FIFO): '1' = Overflow
			              //!<  event has occurred ; '0' = No overflow event has
			              //!<  occurred
			uint32_t
			  TXATIF : 1; //!<  4    - Transmit Attempts Exhausted Interrupt
			              //!<  Pending bit. If TXEN=1 (FIFO is configured as a
			              //!<  Transmit FIFO): '1' = Interrupt pending ; '0' =
			              //!<  Interrupt not pending. If TXEN=0 (FIFO is
			              //!<  configured as a Receive FIFO), read as '0'
			uint32_t
			  TXERR : 1; //!<  5    - Error Detected During Transmission bit.
			             //!<  This bit is cleared when TXREQ is set or by
			             //!<  writing a 0 using the SPI. This bit is updated
			             //!<  when a message completes (or aborts) or when the
			             //!<  FIFO is reset: '1' = A bus error occurred while
			             //!<  the message was being sent ; '0' = A bus error
			             //!<  did not occur while the message was being sent
			uint32_t
			  TXLARB : 1; //!<  6    - Message Lost Arbitration Status bit. This
			              //!<  bit is cleared when TXREQ is set or by writing a
			              //!<  0 using the SPI. This bit is updated when a
			              //!<  message completes (or aborts) or when the FIFO
			              //!<  is reset: '1' = Message lost arbitration while
			              //!<  being sent ; '0' = Message did not lose
			              //!<  arbitration while being sent
			uint32_t
			  TXABT : 1; //!<  7    - Message Aborted Status bit. This bit is
			             //!<  cleared when TXREQ is set or by writing a 0 using
			             //!<  the SPI. This bit is updated when a message
			             //!<  completes (or aborts) or when the FIFO is reset:
			             //!<  '1' = Message was aborted ; '0' = Message
			             //!<  completed successfully
			uint32_t
			  FIFOCI : 5; //!<  8    - FIFO Message Index bits (FIFOCI<4:0>
			              //!<  gives a zero-indexed value to the message in the
			              //!<  FIFO. If the FIFO is 4 messages deep (FSIZE=5)
			              //!<  FIFOCI will take on a value of 0 to 3 depending
			              //!<  on the state of the FIFO). If TXEN=1 (FIFO is
			              //!<  configured as a Transmit FIFO): A read of this
			              //!<  bit field will return an index to the message
			              //!<  that the FIFO will next attempt to transmit. If
			              //!<  TXEN=0 (FIFO is configured as a Receive FIFO): A
			              //!<  read of this bit field will return an index to
			              //!<  the message that the FIFO will use to save the
			              //!<  next message
			uint32_t : 19; //!<  9-31
		} Bits;
	} MCP251XFD_CiFIFOSTAm_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiFIFOSTAm_Register, 4);

	//! Transmit and Receive FIFO status
	typedef enum
	{
		MCP251XFD_FIFO_CLEARABLE_STATUS_FLAGS = 0xF8,

		// Transmit FIFO status
		MCP251XFD_TX_FIFO_FULL = 0x00, //!< Transmit FIFO full
		MCP251XFD_TX_FIFO_NOT_FULL =
		  MCP251XFD_CAN_CiFIFOSTAm8_TFNRFNIF, //!< Transmit FIFO not full
		MCP251XFD_TX_FIFO_HALF_EMPTY =
		  MCP251XFD_CAN_CiFIFOSTAm8_TFHRFHIF, //!< Transmit FIFO half empty
		MCP251XFD_TX_FIFO_EMPTY =
		  MCP251XFD_CAN_CiFIFOSTAm8_TFERFFIF, //!< Transmit FIFO empty
		MCP251XFD_TX_FIFO_ATTEMPTS_EXHAUSTED =
		  MCP251XFD_CAN_CiFIFOSTAm8_TXATIF, //!< Transmit FIFO attempts
		                                    //!< exhausted
		MCP251XFD_TX_FIFO_BUS_ERROR =
		  MCP251XFD_CAN_CiFIFOSTAm8_TXERR, //!< Transmit bus error
		MCP251XFD_TX_FIFO_ARBITRATION_LOST =
		  MCP251XFD_CAN_CiFIFOSTAm8_TXLARB, //!< Transmit arbitration lost
		MCP251XFD_TX_FIFO_ABORTED =
		  MCP251XFD_CAN_CiFIFOSTAm8_TXABT, //!< Transmit aborted
		MCP251XFD_TX_FIFO_STATUS_MASK =
		  MCP251XFD_CAN_CiFIFOSTAm8_TX_FIFO, //!< Transmit FIFO status mask

		// Receive FIFO status
		MCP251XFD_RX_FIFO_EMPTY = 0x00, //!< Receive FIFO empty
		MCP251XFD_RX_FIFO_NOT_EMPTY =
		  MCP251XFD_CAN_CiFIFOSTAm8_TFNRFNIF, //!< Receive FIFO not empty
		MCP251XFD_RX_FIFO_HALF_FULL =
		  MCP251XFD_CAN_CiFIFOSTAm8_TFHRFHIF, //!< Receive FIFO half full
		MCP251XFD_RX_FIFO_FULL =
		  MCP251XFD_CAN_CiFIFOSTAm8_TFERFFIF, //!< Receive FIFO full
		MCP251XFD_RX_FIFO_OVERFLOW =
		  MCP251XFD_CAN_CiFIFOSTAm8_RXOVIF, //!< Receive overflow
		MCP251XFD_RX_FIFO_STATUS_MASK =
		  MCP251XFD_CAN_CiFIFOSTAm8_RX_FIFO, //!< Receive FIFO status mask
	} eMCP251XFD_FIFOstatus;

	typedef eMCP251XFD_FIFOstatus
	  setMCP251XFD_FIFOstatus; //! Set of Transmit and Receive FIFO status (can
	                           //! be OR'ed)

	//-----------------------------------------------------------------------------

	/*! FIFO User Address Register m, (m = 1 to 31)
	 * This register is not guaranteed to read correctly in Configuration mode
	 * and should only be accessed when the module is not in Configuration mode
	 */
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiFIFOUAm_Register
	{
		uint32_t CiFIFOUAm; //!< FIFO User Address bits. If TXEN=1 (FIFO is
		                    //!< configured as a Transmit FIFO): A read of this
		                    //!< register will return the address where the next
		                    //!< message is to be written (FIFO head). If TXEN=0
		                    //!< (FIFO is configured as a Receive FIFO): A read
		                    //!< of this register will return the address where
		                    //!< the next message is to be read (FIFO tail)
		uint8_t Bytes[sizeof(uint32_t)];
	} MCP251XFD_CiFIFOUAm_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiFIFOUAm_Register, 4);

	//-----------------------------------------------------------------------------

	//********************************************************************************************************************
	// MCP251XFD CAN Filters Objects
	//********************************************************************************************************************

	//! Available Filter list
	typedef enum
	{
		MCP251XFD_FILTER0  = 0,  //!< Filter  0
		MCP251XFD_FILTER1  = 1,  //!< Filter  1
		MCP251XFD_FILTER2  = 2,  //!< Filter  2
		MCP251XFD_FILTER3  = 3,  //!< Filter  3
		MCP251XFD_FILTER4  = 4,  //!< Filter  4
		MCP251XFD_FILTER5  = 5,  //!< Filter  5
		MCP251XFD_FILTER6  = 6,  //!< Filter  6
		MCP251XFD_FILTER7  = 7,  //!< Filter  7
		MCP251XFD_FILTER8  = 8,  //!< Filter  8
		MCP251XFD_FILTER9  = 9,  //!< Filter  9
		MCP251XFD_FILTER10 = 10, //!< Filter 10
		MCP251XFD_FILTER11 = 11, //!< Filter 11
		MCP251XFD_FILTER12 = 12, //!< Filter 12
		MCP251XFD_FILTER13 = 13, //!< Filter 13
		MCP251XFD_FILTER14 = 14, //!< Filter 14
		MCP251XFD_FILTER15 = 15, //!< Filter 15
		MCP251XFD_FILTER16 = 16, //!< Filter 16
		MCP251XFD_FILTER17 = 17, //!< Filter 17
		MCP251XFD_FILTER18 = 18, //!< Filter 18
		MCP251XFD_FILTER19 = 19, //!< Filter 19
		MCP251XFD_FILTER20 = 20, //!< Filter 20
		MCP251XFD_FILTER21 = 21, //!< Filter 21
		MCP251XFD_FILTER22 = 22, //!< Filter 22
		MCP251XFD_FILTER23 = 23, //!< Filter 23
		MCP251XFD_FILTER24 = 24, //!< Filter 24
		MCP251XFD_FILTER25 = 25, //!< Filter 25
		MCP251XFD_FILTER26 = 26, //!< Filter 26
		MCP251XFD_FILTER27 = 27, //!< Filter 27
		MCP251XFD_FILTER28 = 28, //!< Filter 28
		MCP251XFD_FILTER29 = 29, //!< Filter 29
		MCP251XFD_FILTER30 = 30, //!< Filter 30
		MCP251XFD_FILTER31 = 31, //!< Filter 31
		MCP251XFD_FILTER_COUNT,  // Keep it last
	} eMCP251XFD_Filter;

	//-----------------------------------------------------------------------------

	//! Filter Control Register m, (m = 0 to 31)
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiFLTCONm_Register
	{
		uint8_t CiFLTCONm;
		struct
		{
			uint8_t FBP : 5; //!<  0-4 - Pointer to FIFO when Filter hits bits.
			                 //!<  This bit can only be modified if the
			                 //!<  corresponding filter is disabled (FLTEN=0)
			uint8_t : 2;     //!<  5-6
			uint8_t
			  FLTEN : 1; //!<  7   - Enable Filter to Accept Messages bit: '1' =
			             //!<  Filter is enabled ; '0' = Filter is disabled
		} Bits;
	} MCP251XFD_CiFLTCONm_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiFLTCONm_Register, 1);

	//-----------------------------------------------------------------------------

	//! Filter Object Register m, (m = 0 to 31)
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiFLTOBJm_Register
	{
		uint32_t CiFLTOBJm;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t SID : 11; //!<  0-10 - Standard Identifier filter bits
			uint32_t EID : 18; //!< 11-28 - Extended Identifier filter bits. In
			                   //!< DeviceNet mode, these are the filter bits
			                   //!< for the first 18 data bits
			uint32_t
			  SID11 : 1; //!< 29    - Standard Identifier filter in FD mode, the
			             //!< standard ID can be extended to 12 bit using r1
			uint32_t EXIDE : 1; //!< 30    - Extended Identifier Enable bit. If
			                    //!< CiMASKm.MIDE=1: '1' = Match only messages
			                    //!< with extended identifier ; '0' = Match only
			                    //!< messages with standard identifier
			uint32_t : 1;       //!< 31
		} Bits;
	} MCP251XFD_CiFLTOBJm_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiFLTOBJm_Register, 4);

	//-----------------------------------------------------------------------------

	//! Mask Register m, (m = 0 to 31)
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CiMASKm_Register
	{
		uint32_t CiMASKm;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t MSID : 11; //!<  0-10 - Standard Identifier Mask bits
			uint32_t MEID : 18; //!< 11-28 - Extended Identifier Mask bits. In
			                    //!< DeviceNet mode, these are the mask bits for
			                    //!< the first 18 data bits
			uint32_t
			  MSID11 : 1; //!< 29    - Standard Identifier Mask in FD mode, the
			              //!< standard ID can be extended to 12 bit using r1
			uint32_t
			  MIDE : 1; //!< 30    - Identifier Receive mode bit: '1' = Match
			            //!< only message types (standard or extended ID) that
			            //!< correspond to CiFLTOBJm.EXIDE bit in filter ; '0' =
			            //!< Match both standard and extended message frames if
			            //!< filters match
			uint32_t : 1; //!< 31
		} Bits;
	} MCP251XFD_CiMASKm_Register;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CiMASKm_Register, 4);

	//-----------------------------------------------------------------------------

	//********************************************************************************************************************
	// MCP251XFD CAN Messages Objects
	//********************************************************************************************************************

	//! Control flags of CAN message
	typedef enum
	{
		MCP251XFD_NO_MESSAGE_CTRL_FLAGS = 0x00, //!< No Message Control Flags
		MCP251XFD_CAN20_FRAME =
		  0x00, //!< Indicate that the frame is a CAN2.0A/B
		MCP251XFD_CANFD_FRAME = 0x01, //!< Indicate that the frame is a CAN-FD
		MCP251XFD_NO_SWITCH_BITRATE =
		  0x00, //!< The data bitrate is not switched (only CAN-FD frame)
		MCP251XFD_SWITCH_BITRATE =
		  0x02, //!< The data bitrate is switched (only CAN-FD frame)
		MCP251XFD_REMOTE_TRANSMISSION_REQUEST =
		  0x04, //!< The frame is a Remote Transmission Request; not used in CAN
		        //!< FD
		MCP251XFD_STANDARD_MESSAGE_ID =
		  0x00, //!< Clear the Identifier Extension Flag that set the standard
		        //!< ID format
		MCP251XFD_EXTENDED_MESSAGE_ID =
		  0x08, //!< Set the Identifier Extension Flag that set the extended ID
		        //!< format
		MCP251XFD_TRANSMIT_ERROR_PASSIVE =
		  0x10, //!< Error Status Indicator: In CAN to CAN gateway mode
		        //!< (CiCON.ESIGM=1), the transmitted ESI flag is a "logical OR"
		        //!< of T1.ESI and error passive state of the CAN controller; In
		        //!< normal mode ESI indicates the error status
	} eMCP251XFD_MessageCtrlFlags;

	typedef eMCP251XFD_MessageCtrlFlags
	  setMCP251XFD_MessageCtrlFlags; //! Set of Control flags of CAN message
	                                 //! (can be OR'ed)

	//-----------------------------------------------------------------------------

	//! Data Length Size for the CAN message
	typedef enum
	{
		MCP251XFD_DLC_0BYTE  = 0b0000, //!< The DLC is  0 data byte
		MCP251XFD_DLC_1BYTE  = 0b0001, //!< The DLC is  1 data byte
		MCP251XFD_DLC_2BYTE  = 0b0010, //!< The DLC is  2 data bytes
		MCP251XFD_DLC_3BYTE  = 0b0011, //!< The DLC is  3 data bytes
		MCP251XFD_DLC_4BYTE  = 0b0100, //!< The DLC is  4 data bytes
		MCP251XFD_DLC_5BYTE  = 0b0101, //!< The DLC is  5 data bytes
		MCP251XFD_DLC_6BYTE  = 0b0110, //!< The DLC is  6 data bytes
		MCP251XFD_DLC_7BYTE  = 0b0111, //!< The DLC is  7 data bytes
		MCP251XFD_DLC_8BYTE  = 0b1000, //!< The DLC is  8 data bytes
		MCP251XFD_DLC_12BYTE = 0b1001, //!< The DLC is 12 data bytes
		MCP251XFD_DLC_16BYTE = 0b1010, //!< The DLC is 16 data bytes
		MCP251XFD_DLC_20BYTE = 0b1011, //!< The DLC is 20 data bytes
		MCP251XFD_DLC_24BYTE = 0b1100, //!< The DLC is 24 data bytes
		MCP251XFD_DLC_32BYTE = 0b1101, //!< The DLC is 32 data bytes
		MCP251XFD_DLC_48BYTE = 0b1110, //!< The DLC is 48 data bytes
		MCP251XFD_DLC_64BYTE = 0b1111, //!< The DLC is 64 data bytes
		MCP251XFD_DLC_COUNT,           // Keep last
		MCP251XFD_PAYLOAD_MIN = 8,
		MCP251XFD_PAYLOAD_MAX = 64,
	} eMCP251XFD_DataLength;

	//-----------------------------------------------------------------------------

	//! MCP251XFD CAN message configuration structure
	typedef struct MCP251XFD_CANMessage
	{
		uint32_t MessageID; //!< Contain the message ID to send
		uint32_t
		  MessageSEQ; //!< This is the context of the CAN message. This sequence
		              //!< will be copied in the TEF to trace the message sent
		setMCP251XFD_MessageCtrlFlags
		  ControlFlags; //!< Contain the CAN controls flags
		eMCP251XFD_DataLength
		  DLC; //!< Indicate how many bytes in the payload data will be sent or
		       //!< how many bytes in the payload data is received
		uint8_t *PayloadData; //!< Pointer to the payload data that will be
		                      //!< sent. PayloadData array should be at least
		                      //!< the same size as indicate by the DLC
	} MCP251XFD_CANMessage;

	//-----------------------------------------------------------------------------

	//! CAN Transmit Message Identifier (T0)
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CAN_TX_Message_Identifier
	{
		uint32_t T0;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t SID : 11;  //!<  0-10 - Standard Identifier
			uint32_t EID : 18;  //!< 11-28 - Extended Identifier
			uint32_t SID11 : 1; //!< 29    - In FD mode the standard ID can be
			                    //!< extended to 12 bit using RRS
			uint32_t : 2;       //!< 30-31
		};
	} MCP251XFD_CAN_TX_Message_Identifier;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CAN_TX_Message_Identifier, 4);

	//-----------------------------------------------------------------------------

	//! CAN Transmit Message Control Field (T1)
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CAN_TX_Message_Control
	{
		uint32_t T1;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t DLC : 4; //!< 0- 3 - Data Length Code
			uint32_t
			  IDE : 1; //!< 4    - Identifier Extension Flag; distinguishes
			           //!< between base and extended format
			uint32_t RTR : 1; //!< 5    - Remote Transmission Request; not used
			                  //!< in CAN-FD
			uint32_t BRS : 1; //!< 6    - Bit Rate Switch; selects if data bit
			                  //!< rate is switched
			uint32_t FDF : 1; //!< 7    - FD Frame; distinguishes between CAN
			                  //!< and CAN-FD formats
			uint32_t
			  ESI : 1; //!< 8    - Error Status Indicator. In CAN to CAN gateway
			           //!< mode (CiCON.ESIGM=1), the transmitted ESI flag is a
			           //!< "logical OR" of T1.ESI and error passive state of
			           //!< the CAN controller. In normal mode ESI indicates the
			           //!< error status: '1' = Transmitting node is error
			           //!< passive ; '0' = Transmitting node is error active
			uint32_t
			  SEQ : 23; //!< 9-31 - Sequence to keep track of transmitted
			            //!< messages in Transmit Event FIFO (Only bit <6:0> for
			            //!< the MCP2517X. Bits <22:7> should be at '0')
		};
	} MCP251XFD_CAN_TX_Message_Control;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CAN_TX_Message_Control, 4);

	//-----------------------------------------------------------------------------

	//! Transmit Message Object Register (TXQ and TX FIFO)
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CAN_TX_Message
	{
		uint32_t Word[2];
		uint8_t  Bytes[8];
		struct
		{
			MCP251XFD_CAN_TX_Message_Identifier
			  T0; //!< CAN Transmit Message Identifier (T0)
			MCP251XFD_CAN_TX_Message_Control
			  T1; //!< CAN Transmit Message Control Field (T1)
		};
	} MCP251XFD_CAN_TX_Message;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CAN_TX_Message, 8);

	//-----------------------------------------------------------------------------

	//! Transmit Event Object Register (TEF)
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CAN_TX_EventObject
	{
		uint32_t Word[2];
		uint8_t  Bytes[8];
		struct
		{
			MCP251XFD_CAN_TX_Message_Identifier
			  TE0; //!< CAN Transmit Event Object Identifier (TE0)
			MCP251XFD_CAN_TX_Message_Control
			  TE1; //!< CAN Transmit Event Object Control Field (TE1)
			uint32_t
			  TimeStamp; //!< Transmit Message Time Stamp. TE2 (TXMSGTS) only
			             //!< exists in objects where CiTEFCON.TEFTSEN is set
		};
	} MCP251XFD_CAN_TX_EventObject;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CAN_TX_EventObject, 12);

	//-----------------------------------------------------------------------------

	//! CAN Receive Message Identifier (R0)
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CAN_RX_Message_Identifier
	{
		uint32_t R0;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t SID : 11;  //!<  0-10 - Standard Identifier
			uint32_t EID : 18;  //!< 11-28 - Extended Identifier
			uint32_t SID11 : 1; //!< 29    - In FD mode the standard ID can be
			                    //!< extended to 12 bit using RRS
			uint32_t : 2;       //!< 30-31
		};
	} MCP251XFD_CAN_RX_Message_Identifier;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CAN_RX_Message_Identifier, 4);

	//-----------------------------------------------------------------------------

	//! CAN Receive Message Control Field (R1)
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CAN_RX_Message_Control
	{
		uint32_t R1;
		uint8_t  Bytes[sizeof(uint32_t)];
		struct
		{
			uint32_t DLC : 4; //!<  0- 3 - Data Length Code
			uint32_t
			  IDE : 1; //!<  4    - Identifier Extension Flag; distinguishes
			           //!<  between base and extended format
			uint32_t RTR : 1; //!<  5    - Remote Transmission Request; not used
			                  //!<  in CAN-FD
			uint32_t BRS : 1; //!<  6    - Bit Rate Switch; selects if data bit
			                  //!<  rate is switched
			uint32_t FDF : 1; //!<  7    - FD Frame; distinguishes between CAN
			                  //!<  and CAN-FD formats
			uint32_t ESI : 1; //!<  8    - Error Status Indicator: '1' =
			                  //!<  Transmitting node is error passive ; '0' =
			                  //!<  Transmitting node is error active
			uint32_t : 2;     //!<  9-10
			uint32_t FILTHIT : 5; //!< 11-15 - Filter Hit, number of filters
			                      //!< that matched
			uint32_t : 16;        //!< 16-31
		};
	} MCP251XFD_CAN_RX_Message_Control;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CAN_RX_Message_Control, 4);

	//! Receive Message Object Register
	MCP251XFD_PACKITEM
	typedef union __MCP251XFD_PACKED__ MCP251XFD_CAN_RX_Message
	{
		uint32_t Word[2];
		uint8_t  Bytes[8];
		struct
		{
			MCP251XFD_CAN_RX_Message_Identifier
			  R0; //!< CAN Receive Message Identifier (R0)
			MCP251XFD_CAN_RX_Message_Control
			  R1; //!< CAN Receive Message Control Field (R1)
			uint32_t
			  TimeStamp; //!< Transmit Message Time Stamp. R2 (RXMSGTS) only
			             //!< exits in objects where CiFIFOCONm.RXTSEN is set
		};
	} MCP251XFD_CAN_RX_Message;
	MCP251XFD_UNPACKITEM;
	MCP251XFD_CONTROL_ITEM_SIZE(MCP251XFD_CAN_RX_Message, 12);

	//-----------------------------------------------------------------------------

	//********************************************************************************************************************
	// MCP251XFD Driver API
	//********************************************************************************************************************

	//! Device power states
	typedef enum
	{
		MCP251XFD_DEVICE_SLEEP_NOT_CONFIGURED =
		  0x0, //!< Device sleep mode is not configured so the device is in
		       //!< normal power state
		MCP251XFD_DEVICE_NORMAL_POWER_STATE =
		  0x1,                              //!< Device is in normal power state
		MCP251XFD_DEVICE_SLEEP_STATE = 0x2, //!< Device is in sleep power state
		MCP251XFD_DEVICE_LOWPOWER_SLEEP_STATE =
		  0x3, //!< Device is in low-power sleep power state
	} eMCP251XFD_PowerStates;

	//-----------------------------------------------------------------------------

	typedef struct MCP251XFD
	  MCP251XFD; //! Typedef of MCP251XFD device object structure
	typedef uint8_t
	  TMCP251XFDDriverInternal; //! Alias for Driver Internal data flags

	//-----------------------------------------------------------------------------

	/*! @brief Function for interface driver initialization of the MCP251XFD
	 *
	 * This function will be called at driver initialization to configure the
	 * interface driver
	 * @param[in] *pIntDev Is the MCP251XFD.InterfaceDevice of the device that
	 * call the interface initialization
	 * @param[in] chipSelect Is the Chip Select index to use for the SPI
	 * initialization
	 * @param[in] sckFreq Is the SCK frequency in Hz to set at the interface
	 * initialization
	 * @return Returns an #eERRORRESULT value enum
	 */
	typedef eERRORRESULT (*MCP251XFD_SPIInit_Func)(void          *pIntDev,
	                                               uint8_t        chipSelect,
	                                               const uint32_t sckFreq);

	/*! @brief Function for interface transfer of the MCP251XFD
	 *
	 * This function will be called at driver read/write data from/to the
	 * interface driver
	 * @param[in] *pIntDev Is the MCP251XFD.InterfaceDevice of the device that
	 * call the data transfer
	 * @param[in] chipSelect Is the Chip Select index to use for the SPI
	 * transfer
	 * @param[in] *txData Is the data to send through the interface
	 * @param[out] *rxData Is where the data received through the interface will
	 * be stored. This parameter can be nulled by the driver if no received data
	 * is expected
	 * @param[in] size Is the size of the data to send and receive through the
	 * interface
	 * @return Returns an #eERRORRESULT value enum
	 */
	typedef eERRORRESULT (*MCP251XFD_SPITransfer_Func)(void    *pIntDev,
	                                                   uint8_t  chipSelect,
	                                                   uint8_t *txData,
	                                                   uint8_t *rxData,
	                                                   size_t   size);

	/*! @brief Function that gives the current millisecond of the system to the
	 * driver
	 *
	 * This function will be called when the driver needs to get current
	 * millisecond
	 * @return Returns the current millisecond of the system
	 */
	typedef uint32_t (*GetCurrentms_Func)(void);

	/*! @brief Function that compute CRC16-CMS for the driver
	 *
	 * This function will be called when a CRC16-CMS computation is needed (ie.
	 * in CRC mode or Safe Write). In normal mode, this can point to NULL.
	 * @param[in] *data Is the byte steam of data to compute
	 * @param[in] size Is the size of the byte stream
	 * @return Returns the result of the CRC16-CMS computation
	 */
	typedef uint16_t (*ComputeCRC16_Func)(const uint8_t *data, size_t size);

	//-----------------------------------------------------------------------------

	//! MCP251XFD device object structure
	struct MCP251XFD
	{
		void *UserDriverData; //!< Optional, can be used to store driver data or
		                      //!< NULL

		//--- Driver configuration ---
		setMCP251XFD_DriverConfig
		  DriverConfig; //!< Driver configuration, by default it is
		                //!< MCP251XFD_DRIVER_NORMAL_USE. Configuration can be
		                //!< OR'ed
		TMCP251XFDDriverInternal
		  InternalConfig; //!< DO NOT USE OR CHANGE THIS VALUE, IT'S THE
		                  //!< INTERNAL DRIVER CONFIGURATION

		//--- GPIO configuration ---
		uint8_t
		  GPIOsOutLevel; //!< GPIOs pins output state (0 = set to '0' ; 1 = set
		                 //!< to '1'). Used to speed up output change

		//--- Interface driver call functions ---
		uint8_t SPI_ChipSelect; //!< This is the Chip Select index that will be
		                        //!< set at the call of a transfer
		void
		  *InterfaceDevice; //!< This is the pointer that will be in the first
		                    //!< parameter of all interface call functions
		uint32_t
		  SPIClockSpeed; //!< SPI nominal clock speed (max is SYSCLK div by 2)
		MCP251XFD_SPIInit_Func
		  fnSPI_Init; //!< This function will be called at driver initialization
		              //!< to configure the interface driver
		MCP251XFD_SPITransfer_Func
		  fnSPI_Transfer; //!< This function will be called at driver read/write
		                  //!< data from/to the interface driver SPI

		//--- Time call function ---
		GetCurrentms_Func
		  fnGetCurrentms; //!< This function will be called when the driver need
		                  //!< to get current millisecond

		//--- CRC16-CMS call function ---
		ComputeCRC16_Func
		  fnComputeCRC16; //!< This function will be called when a CRC16-CMS
		                  //!< computation is needed (ie. in CRC mode or Safe
		                  //!< Write). In normal mode, this can point to NULL
	};

	//-----------------------------------------------------------------------------

#define MCP251XFD_NO_CANFD                                                     \
	(0) //!< This value specify that the driver will not calculate CAN-FD
	    //!< bitrate
#define MCP251XFD_CANFD_ENABLED                                                \
	(0x80) //!< This value is used inside the driver (MCP251XFD.InternalConfig)
	       //!< to indicate if the CANFD is configured

	//! CAN control configuration flags
	typedef enum
	{
		MCP251XFD_CAN_RESTRICTED_MODE_ON_ERROR =
		  0x00, //!< Transition to Restricted Operation Mode on system error
		MCP251XFD_CAN_LISTEN_ONLY_MODE_ON_ERROR =
		  0x01, //!< Transition to Listen Only Mode on system error
		MCP251XFD_CAN_ESI_REFLECTS_ERROR_STATUS =
		  0x00, //!< ESI reflects error status of CAN controller
		MCP251XFD_CAN_GATEWAY_MODE_ESI_RECESSIVE =
		  0x02, //!< Transmit ESI in Gateway Mode, ESI is transmitted recessive
		        //!< when ESI of message is high or CAN controller error passive
		MCP251XFD_CAN_UNLIMITED_RETRANS_ATTEMPTS =
		  0x00, //!< Unlimited number of retransmission attempts,
		        //!< MCP251XFD_FIFO.Attempts (CiFIFOCONm.TXAT) will be ignored
		MCP251XFD_CAN_RESTRICTED_RETRANS_ATTEMPTS =
		  0x04, //!< Restricted retransmission attempts, MCP251XFD_FIFO.Attempts
		        //!< (CiFIFOCONm.TXAT) is used
		MCP251XFD_CANFD_BITRATE_SWITCHING_ENABLE =
		  0x00, //!< Bit Rate Switching is Enabled, Bit Rate Switching depends
		        //!< on BRS in the Transmit Message Object
		MCP251XFD_CANFD_BITRATE_SWITCHING_DISABLE =
		  0x08, //!< Bit Rate Switching is Disabled, regardless of BRS in the
		        //!< Transmit Message Object
		MCP251XFD_CAN_PROTOCOL_EXCEPT_ENTER_INTEGRA =
		  0x00, //!< If a Protocol Exception is detected, the CAN FD Controller
		        //!< Module will enter Bus Integrating state. A recessive "res
		        //!< bit" following a recessive FDF bit is called a Protocol
		        //!< Exception
		MCP251XFD_CAN_PROTOCOL_EXCEPT_AS_FORM_ERROR =
		  0x10, //!< Protocol Exception is treated as a Form Error. A recessive
		        //!< "res bit" following a recessive FDF bit is called a
		        //!< Protocol Exception
		MCP251XFD_CANFD_USE_NONISO_CRC =
		  0x00, //!< Do NOT include Stuff Bit Count in CRC Field and use CRC
		        //!< Initialization Vector with all zeros
		MCP251XFD_CANFD_USE_ISO_CRC =
		  0x20, //!< Include Stuff Bit Count in CRC Field and use Non-Zero CRC
		        //!< Initialization Vector according to ISO 11898-1:2015
		MCP251XFD_CANFD_DONT_USE_RRS_BIT_AS_SID11 =
		  0x00, //!< Don’t use RRS; SID<10:0> according to ISO 11898-1:2015
		MCP251XFD_CANFD_USE_RRS_BIT_AS_SID11 =
		  0x40, //!< RRS is used as SID11 in CAN FD base format messages:
		        //!< SID<11:0> = {SID<10:0>, SID11}
	} eMCP251XFD_CANCtrlFlags;

	typedef eMCP251XFD_CANCtrlFlags
	  setMCP251XFD_CANCtrlFlags; //! Set of CAN control configuration flags (can
	                             //! be OR'ed)

	//! MCP251XFD Controller and CAN configuration structure
	typedef struct MCP251XFD_Config
	{
		//--- Controller clocks ---
		uint32_t
		  XtalFreq; //!< Component CLKIN Xtal/Resonator frequency (min 4MHz, max
		            //!< 40MHz). Set it to 0 if oscillator is used
		uint32_t OscFreq; //!< Component CLKIN oscillator frequency (min 2MHz,
		                  //!< max 40MHz). Set it to 0 if Xtal/Resonator is used
		eMCP251XFD_CLKINtoSYSCLK
		  SysclkConfig; //!< Factor of frequency for the SYSCLK. SYSCLK = CLKIN
		                //!< x SysclkConfig where CLKIN is XtalFreq or OscFreq
		eMCP251XFD_CLKODIV ClkoPinConfig; //!< Configure the CLKO pin (SCLK div
		                                  //!< by 1, 2, 4, 10 or Start Of Frame)
		uint32_t
		  *SYSCLK_Result; //!< This is the SYSCLK of the component after
		                  //!< configuration (can be NULL if the internal SYSCLK
		                  //!< of the component do not have to be known)

		//--- CAN configuration ---
		uint32_t
		  NominalBitrate; //!< Speed of the Frame description and arbitration
		uint32_t
		  DataBitrate; //!< Speed of all the data bytes of the frame (if CAN2.0
		               //!< only mode, set to value MCP251XFD_NO_CANFD)
		MCP251XFD_BitTimeStats
		  *BitTimeStats; //!< Point to a Bit Time stat structure (set to NULL if
		                 //!< no statistics are necessary)
		eMCP251XFD_Bandwidth
		  Bandwidth; //!< Transmit Bandwidth Sharing, this is the delay between
		             //!< two consecutive transmissions (in arbitration bit
		             //!< times)
		setMCP251XFD_CANCtrlFlags
		  ControlFlags; //!< Set of CAN control flags to configure the CAN
		                //!< controller. Configuration can be OR'ed

		//--- GPIOs and Interrupts pins ---
		eMCP251XFD_GPIO0Mode GPIO0PinMode; //!< Startup INT0/GPIO0/XSTBY pins
		                                   //!< mode (INT0 => Interrupt for TX)
		eMCP251XFD_GPIO1Mode GPIO1PinMode; //!< Startup INT1/GPIO1 pins mode
		                                   //!< (INT1 => Interrupt for RX)
		eMCP251XFD_OutMode INTsOutMode;    //!< Define the output type of all
		                                //!< interrupt pins (INT, INT0 and INT1)
		eMCP251XFD_OutMode
		  TXCANOutMode; //!< Define the output type of the TXCAN pin

		//--- Interrupts ---
		setMCP251XFD_InterruptEvents
		  SysInterruptFlags; //!< Set of system interrupt flags to enable.
		                     //!< Configuration can be OR'ed
	} MCP251XFD_Config;

	//-----------------------------------------------------------------------------

	//! MCP251XFD FIFO configuration flags
	typedef enum
	{
		MCP251XFD_FIFO_NO_CONTROL_FLAGS = 0x00, //!< Set no control flags
		MCP251XFD_FIFO_NO_RTR_RESPONSE =
		  0x00, //!< When a remote transmit is received, Transmit Request
		        //!< (TXREQ) of the FIFO will be unaffected
		MCP251XFD_FIFO_AUTO_RTR_RESPONSE =
		  0x40, //!< When a remote transmit is received, Transmit Request
		        //!< (TXREQ) of the FIFO will be set
		MCP251XFD_FIFO_NO_TIMESTAMP_ON_RX = 0x00, //!< Do not capture time stamp
		MCP251XFD_FIFO_ADD_TIMESTAMP_ON_RX =
		  0x20, //!< Capture time stamp in received message object in RAM
		MCP251XFD_FIFO_ADD_TIMESTAMP_ON_OBJ =
		  0x20, //!< Capture time stamp in objects in TEF
	} eMCP251XFD_FIFOCtrlFlags;

	//! MCP251XFD FIFO interruption flags
	typedef enum
	{
		MCP251XFD_FIFO_NO_INTERRUPT_FLAGS = 0x00, //!< Set no interrupt flags
		MCP251XFD_FIFO_TX_ATTEMPTS_EXHAUSTED_INT =
		  0x10, //!< Transmit Attempts Exhausted Interrupt Enable
		MCP251XFD_FIFO_OVERFLOW_INT =
		  0x08, //!< Overflow Interrupt Enable (Not available on TXQ (FIFO0))
		MCP251XFD_FIFO_TRANSMIT_FIFO_EMPTY_INT =
		  0x04, //!< Transmit FIFO Empty Interrupt Enable
		MCP251XFD_FIFO_TRANSMIT_FIFO_HALF_EMPTY_INT =
		  0x02, //!< Transmit FIFO Half Empty Interrupt Enable (Not available on
		        //!< TXQ (FIFO0))
		MCP251XFD_FIFO_TRANSMIT_FIFO_NOT_FULL_INT =
		  0x01, //!< Transmit FIFO Not Full Interrupt Enable
		MCP251XFD_FIFO_RECEIVE_FIFO_FULL_INT =
		  0x04, //!< Receive FIFO Full Interrupt Enable
		MCP251XFD_FIFO_RECEIVE_FIFO_HALF_FULL_INT =
		  0x02, //!< Receive FIFO Half Full Interrupt Enable
		MCP251XFD_FIFO_RECEIVE_FIFO_NOT_EMPTY_INT =
		  0x01, //!< Receive FIFO Not Empty Interrupt Enable
		MCP251XFD_FIFO_EVENT_FIFO_FULL_INT =
		  0x04, //!< Transmit Event FIFO Full Interrupt Enable
		MCP251XFD_FIFO_EVENT_FIFO_HALF_FULL_INT =
		  0x02, //!< Transmit Event FIFO Half Full Interrupt Enable
		MCP251XFD_FIFO_EVENT_FIFO_NOT_EMPTY_INT =
		  0x01, //!< Transmit Event FIFO Not Empty Interrupt Enable
		MCP251XFD_FIFO_ALL_INTERRUPTS_FLAGS = 0x1F, //!< Set all interrupt flags
	} eMCP251XFD_FIFOIntFlags;

	//! MCP251XFD FIFO configuration structure
	typedef struct MCP251XFD_FIFO
	{
		eMCP251XFD_FIFO Name; //!< FIFO name (TXQ, FIFO1..31 or TEF)

		//--- FIFO Size ---
		eMCP251XFD_MessageDeep Size;    //!< FIFO Message size deep (1 to 32)
		eMCP251XFD_PayloadSize Payload; //!< Message Payload Size (8, 12, 16,
		                                //!< 20, 24, 32, 48 or 64 bytes)

		//--- Configuration ---
		eMCP251XFD_SelTXRX  Direction; //!< TX/RX FIFO Selection
		eMCP251XFD_Attempts Attempts; //!< Retransmission Attempts. This feature
		                              //!< is enabled when CiCON.RTXAT is set
		eMCP251XFD_Priority
		  Priority; //!< Message Transmit Priority ('0x00' = Lowest Message
		            //!< Priority and '0x1F' = Highest Message Priority)
		eMCP251XFD_FIFOCtrlFlags
		  ControlFlags; //!< FIFO control flags to configure the FIFO
		eMCP251XFD_FIFOIntFlags
		  InterruptFlags; //!< FIFO interrupt flags to configure interrupts of
		                  //!< the FIFO

		//--- FIFO RAM Infos ---
		MCP251XFD_RAMInfos
		  *RAMInfos; //!< RAM Informations of the FIFO (Can be NULL)
	} MCP251XFD_FIFO;

	//-----------------------------------------------------------------------------

	//! Filter match type
	typedef enum
	{
		MCP251XFD_MATCH_ONLY_SID =
		  0x0, //!< Match only messages with standard identifier (+SID11 in FD
		       //!< mode if configured)
		MCP251XFD_MATCH_ONLY_EID =
		  0x1, //!< Match only messages with extended identifier
		MCP251XFD_MATCH_SID_EID =
		  0x2, //!< Match both standard and extended message frames
	} eMCP251XFD_FilterMatch;

#define MCP251XFD_ACCEPT_ALL_MESSAGES                                          \
	(0x00000000u) //!< Indicate that the filter will accept all messages

	//! MCP251XFD Filter configuration structure
	typedef struct MCP251XFD_Filter
	{
		//--- Configuration ---
		eMCP251XFD_Filter Filter;       //!< Filter to configure
		bool              EnableFilter; //!< Enable the filter
		eMCP251XFD_FilterMatch
		  Match; //!< Filter match type of the frame (SID and/or EID)
		eMCP251XFD_FIFO PointTo; //!< Message matching filter is stored in
		                         //!< pointed FIFO name (FIFO1..31)

		//--- Message Filter ---
		uint32_t AcceptanceID; //!< Message Filter Acceptance SID+(SID11 in FD
		                       //!< mode)+EID
		uint32_t
		  AcceptanceMask; //!< Message Filter Mask SID+(SID11 in FD mode)+EID
		                  //!< (corresponding bits to AcceptanceID: '1': bit to
		                  //!< filter ; '0' bit that do not care)
		bool ExtendedID;  //!< The message filter is an extended ID
	} MCP251XFD_Filter;

	//********************************************************************************************************************

	//=============================================================================
	// Prototypes
	//=============================================================================
	// eERRORRESULT __MCP251XFD_TestRAM(MCP251XFD *pComp);
	eERRORRESULT
	MCP251XFD_WriteRAM32(MCP251XFD *pComp, uint16_t address, uint32_t data);
	eERRORRESULT
	MCP251XFD_ReadRAM32(MCP251XFD *pComp, uint16_t address, uint32_t *data);
	eERRORRESULT MCP251XFD_ResetDevice(MCP251XFD *pComp);
	eERRORRESULT
	MCP251XFD_WriteSFR8(MCP251XFD *pComp, uint16_t address, const uint8_t data);
	eERRORRESULT
	MCP251XFD_ReadSFR8(MCP251XFD *pComp, uint16_t address, uint8_t *data);
	eERRORRESULT MCP251XFD_ConfigureCRC(MCP251XFD             *pComp,
	                                    setMCP251XFD_CRCEvents interrupts);
	eERRORRESULT MCP251XFD_ConfigureECC(MCP251XFD             *pComp,
	                                    bool                   enableECC,
	                                    setMCP251XFD_ECCEvents interrupts,
	                                    uint8_t fixedParityValue);
	eERRORRESULT MCP251XFD_InitRAM(MCP251XFD *pComp);
	eERRORRESULT MCP251XFD_SetGPIOPinsOutputLevel(MCP251XFD *pComp,
	                                              uint8_t    pinsLevel,
	                                              uint8_t    pinsChangeMask);
	eERRORRESULT MCP251XFD_ConfigurePins(MCP251XFD           *pComp,
	                                     eMCP251XFD_GPIO0Mode GPIO0PinMode,
	                                     eMCP251XFD_GPIO1Mode GPIO1PinMode,
	                                     eMCP251XFD_OutMode   INTOutMode,
	                                     eMCP251XFD_OutMode   TXCANOutMode,
	                                     bool                 CLKOasSOF);
	eERRORRESULT MCP251XFD_CalculateBitTimeConfiguration(
	  const uint32_t           fsysclk,
	  const uint32_t           desiredNominalBitrate,
	  const uint32_t           desiredDataBitrate,
	  MCP251XFD_BitTimeConfig *pConf);
	eERRORRESULT
	MCP251XFD_SetBitTimeConfiguration(MCP251XFD               *pComp,
	                                  MCP251XFD_BitTimeConfig *pConf,
	                                  bool                     can20only);
	eERRORRESULT
	MCP251XFD_ConfigureCANController(MCP251XFD                *pComp,
	                                 setMCP251XFD_CANCtrlFlags flags,
	                                 eMCP251XFD_Bandwidth      bandwidth);
	eERRORRESULT
	MCP251XFD_ConfigureInterrupt(MCP251XFD                   *pComp,
	                             setMCP251XFD_InterruptEvents interruptsFlags);
	eERRORRESULT
	MCP251XFD_UpdateFIFO(MCP251XFD *pComp, eMCP251XFD_FIFO name, bool andFlush);
	uint32_t MCP251XFD_MessageIDtoObjectMessageIdentifier(uint32_t messageID,
	                                                      bool     extended,
	                                                      bool     UseSID11);
	uint8_t  MCP251XFD_DLCToByte(eMCP251XFD_DataLength dlc, bool isCANFD);
	eERRORRESULT MCP251XFD_GetNextMessageAddressFIFO(MCP251XFD      *pComp,
	                                                 eMCP251XFD_FIFO name,
	                                                 uint32_t *nextAddress,
	                                                 uint8_t  *nextIndex);
	uint8_t      MCP251XFD_PayloadToByte(eMCP251XFD_PayloadSize payload);
	uint32_t
	MCP251XFD_ObjectMessageIdentifierToMessageID(uint32_t objectMessageID,
	                                             bool     extended,
	                                             bool     UseSID11);
	eERRORRESULT
	MCP251XFD_CalculateBitrateStatistics(const uint32_t           fsysclk,
	                                     MCP251XFD_BitTimeConfig *pConf,
	                                     bool                     can20only);
	eERRORRESULT
	             MCP251XFD_WaitOperationModeChange(MCP251XFD               *pComp,
	                                               eMCP251XFD_OperationMode askedMode);
	eERRORRESULT MCP251XFD_ClearInterruptEvents(
	  MCP251XFD                   *pComp,
	  setMCP251XFD_InterruptEvents interruptsFlags);
	eERRORRESULT MCP251XFD_ClearFIFOConfiguration(MCP251XFD      *pComp,
	                                              eMCP251XFD_FIFO name);
	eERRORRESULT MCP251XFD_DisableFilter(MCP251XFD        *pComp,
	                                     eMCP251XFD_Filter name);

	// Test all RAM address of the MCP251XFD for the driver flag
	// DRIVER_INIT_CHECK_RAM
	eERRORRESULT __MCP251XFD_TestRAM(MCP251XFD *pComp)
	{
		eERRORRESULT Error;
		uint32_t     Result = 0;
		for (uint16_t Address = MCP251XFD_RAM_ADDR;
		     Address < (MCP251XFD_RAM_ADDR + MCP251XFD_RAM_SIZE);
		     Address += 4)
		{
			Error = MCP251XFD_WriteRAM32(
			  pComp, Address, 0x55555555); // Write 0x55555555 at address
			if (Error != ERR_NONE)
				return Error; // If there is an error while writing the RAM
				              // address then return the error
			Error = MCP251XFD_ReadRAM32(
			  pComp, Address, &Result); // Read again the data
			if (Error != ERR_NONE)
				return Error; // If there is an error while reading the RAM
				              // address then return the error
			if (Result != 0x55555555)
				return ERR__RAM_TEST_FAIL; // If data read is not 0x55555555
				                           // then return an error

			Error = MCP251XFD_WriteRAM32(
			  pComp, Address, 0xAAAAAAAA); // Write 0xAAAAAAAA at address
			if (Error != ERR_NONE)
				return Error; // If there is an error while writing the RAM
				              // address then return the error
			Error = MCP251XFD_ReadRAM32(
			  pComp, Address, &Result); // Read again the data
			if (Error != ERR_NONE)
				return Error; // If there is an error while reading the RAM
				              // address then return the error
			if (Result != 0xAAAAAAAA)
				return ERR__RAM_TEST_FAIL; // If data read is not 0xAAAAAAAA
				                           // then return an error
		}
		return ERR_NONE;
	}
//-----------------------------------------------------------------------------
#define MCP251XFD_USE_SID11                                                    \
	((pComp->InternalConfig &                                                  \
	  (MCP251XFD_CANFD_USE_RRS_BIT_AS_SID11 | MCP251XFD_CANFD_ENABLED)) ==     \
	 (MCP251XFD_CANFD_USE_RRS_BIT_AS_SID11 | MCP251XFD_CANFD_ENABLED))
#define MCP251XFD_TIME_DIFF(begin, end)                                        \
	(((end) >= (begin))                                                        \
	   ? ((end) - (begin))                                                     \
	   : (UINT32_MAX -                                                         \
	      ((begin) - (end)-1))) // Works only if time difference is strictly
	                            // inferior to (UINT32_MAX/2) and call often
	//-----------------------------------------------------------------------------

	/*! @brief MCP251XFD device initialization
	 *
	 * This function initializes the MCP251XFD driver and call the
	 * initialization of the interface driver (SPI). It checks parameters and
	 * perform a RESET Next this function configures the MCP251XFD Controller
	 * and the CAN controller
	 * @warning This function must be used after component power-on otherwise
	 * the reset function call can fail when the component is used with
	 * DRIVER_SAFE_RESET
	 * @param[in] *pComp Is the pointed structure of the device to be
	 * initialized
	 * @param[in] *pConf Is the pointed structure of the device configuration
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT Init_MCP251XFD(MCP251XFD *pComp, const MCP251XFD_Config *pConf)
	{
#ifdef CHECK_NULL_PARAM
		if ((pComp == NULL) || (pConf == NULL))
			return ERR__PARAMETER_ERROR;
		if (pComp->fnSPI_Init == NULL)
			return ERR__PARAMETER_ERROR;
		if (pComp->fnGetCurrentms == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		eERRORRESULT Error;
		uint32_t     Result;
		pComp->InternalConfig = 0;

		//--- Check configuration ---------------------------------
		if ((pConf->XtalFreq != 0) &&
		    (pConf->XtalFreq < MCP251XFD_XTALFREQ_MIN))
			return ERR__FREQUENCY_ERROR; // The device crystal should not be <
			                             // 4MHz
		if ((pConf->XtalFreq != 0) &&
		    (pConf->XtalFreq > MCP251XFD_XTALFREQ_MAX))
			return ERR__FREQUENCY_ERROR; // The device crystal should not be >
			                             // 40MHz
		if ((pConf->OscFreq != 0) && (pConf->OscFreq < MCP251XFD_OSCFREQ_MIN))
			return ERR__FREQUENCY_ERROR; // The device oscillator should not be
			                             // <  2MHz
		if ((pConf->OscFreq != 0) && (pConf->OscFreq > MCP251XFD_OSCFREQ_MAX))
			return ERR__FREQUENCY_ERROR; // The device oscillator should not be
			                             // > 40MHz
		uint32_t CompFreq = 0;
		if (pConf->XtalFreq != 0)
			CompFreq = pConf->XtalFreq;
		else
			CompFreq = pConf->OscFreq; // Select the component frequency
		if (CompFreq == 0)
			return ERR__CONFIGURATION; // Both XtalFreq and OscFreq are
			                           // configured to 0

		//--- Configure SPI Interface -----------------------------
		if (pComp->SPIClockSpeed > MCP251XFD_SPICLOCK_MAX)
			return ERR__SPI_FREQUENCY_ERROR; // The SPI clock should not be
			                                 // superior to the SPI clock max
		if ((pComp->DriverConfig & MCP251XFD_DRIVER_SAFE_RESET) == 0)
		{
			Error = pComp->fnSPI_Init(
			  pComp->InterfaceDevice,
			  pComp->SPI_ChipSelect,
			  pComp
			    ->SPIClockSpeed); // Initialize the SPI interface only in case
			                      // of no safe reset (the interface will be
			                      // initialized in the reset at a safe speed)
			if (Error != ERR_OK)
				return Error; // If there is an error while calling fnSPI_Init()
				              // then return the error
		}

		//--- Reset -----------------------------------------------
		Error = MCP251XFD_ResetDevice(pComp); // Reset the device
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ResetDevice() then return the error
		pComp->InternalConfig = MCP251XFD_DEV_PS_SET(
		  MCP251XFD_DEVICE_SLEEP_NOT_CONFIGURED); // Device is in normal power
		                                          // state, sleep is not yet
		                                          // configured

		//--- Test SPI connection ---------------------------------
		Error =
		  MCP251XFD_WriteRAM32(pComp,
		                       (MCP251XFD_RAM_ADDR + MCP251XFD_RAM_SIZE - 4),
		                       0xAA55AA55); // Write 0xAA55AA55 at address
		if (Error != ERR_NONE)
			return Error; // If there is an error while writing the RAM address
			              // then return the error
		Error =
		  MCP251XFD_ReadRAM32(pComp,
		                      (MCP251XFD_RAM_ADDR + MCP251XFD_RAM_SIZE - 4),
		                      &Result); // Read again the data

		if ((Error == ERR__CRC_ERROR) || (Result != 0xAA55AA55))
			return ERR__NO_DEVICE_DETECTED; // If CRC mismatch or data read is
			                                // not 0xAA55AA55 then no device is
			                                // detected
		if (Error != ERR_NONE)
			return Error; // If there is an error while reading the RAM address
			              // then return the error

		//--- Configure component clock ---------------------------
		uint8_t Config = MCP251XFD_SFR_OSC_WAKEUP |
		                 MCP251XFD_SFR_OSC8_SCLKDIV_SET(MCP251XFD_SCLK_DivBy1) |
		                 MCP251XFD_SFR_OSC8_PLLDIS;
		if ((pConf->SysclkConfig == MCP251XFD_SYSCLK_IS_CLKIN_MUL_5) ||
		    (pConf->SysclkConfig == MCP251XFD_SYSCLK_IS_CLKIN_MUL_10))
		{
			Config |= MCP251XFD_SFR_OSC8_PLLEN;
			CompFreq *= 10;
		} // Activate the 10x PLL from Xtal/Resonator frequency @4MHz or from
		  // oscillator frequency @2MHz
		if (CompFreq > MCP251XFD_CLKINPLL_MAX)
			return ERR__FREQUENCY_ERROR; // If component clock (CLKIN+PLL) is
			                             // too high then return an error
		if ((pConf->SysclkConfig == MCP251XFD_SYSCLK_IS_CLKIN_DIV_2) ||
		    (pConf->SysclkConfig == MCP251XFD_SYSCLK_IS_CLKIN_MUL_5))
		{
			Config |= MCP251XFD_SFR_OSC8_SCLKDIV_SET(MCP251XFD_SCLK_DivBy2);
			CompFreq /= 2;
		} // Configure CLKIN+PLL divisor from Xtal/Resonator and PLL or
		  // oscillator frequency
		if (pConf->SYSCLK_Result != NULL)
			*pConf->SYSCLK_Result =
			  CompFreq; // Save the internal SYSCLK if needed
		if (CompFreq > MCP251XFD_SYSCLK_MAX)
			return ERR__FREQUENCY_ERROR; // If component clock (SYSCLK) is too
			                             // high then return an error
		if (pConf->ClkoPinConfig != MCP251XFD_CLKO_SOF)
			Config |= MCP251XFD_SFR_OSC8_CLKODIV_SET(
			  pConf->ClkoPinConfig); // Configure the CLKO pin (CLKIN+PLL div by
			                         // 1, 2, 4, 10 or Start Of Frame)
		Config |=
		  MCP251XFD_SFR_OSC8_LPMEN; // Set now the Low Power Mode for further
		                            // check of which module MCP251XFD it is
		Error = MCP251XFD_WriteSFR8(
		  pComp,
		  RegMCP251XFD_OSC_CONFIG,
		  Config); // Write the Oscillator Register configuration
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_WriteSFR8() then return the error

		//--- Check clocks stabilization --------------------------
		uint8_t CheckVal =
		  ((uint8_t)(Config)&MCP251XFD_SFR_OSC8_CHECKFLAGS) |
		  MCP251XFD_SFR_OSC8_OSCRDY; // Check if PLL Locked (if enabled), OSC
		                             // clock is running and stable, and SCLKDIV
		                             // is synchronized (if divided by 2)
		uint32_t StartTime = pComp->fnGetCurrentms(); // Start the timeout
		while (true)
		{
			Error = MCP251XFD_ReadSFR8(
			  pComp,
			  RegMCP251XFD_OSC_CHECK,
			  &Config); // Read current OSC register mode with
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ReadSFR8() then return the error
			if ((Config & MCP251XFD_SFR_OSC8_CHECKFLAGS) == CheckVal)
				break; // Check if the controller's clocks are ready
			if (MCP251XFD_TIME_DIFF(StartTime, pComp->fnGetCurrentms()) > 4)
				return ERR__DEVICE_TIMEOUT; // Wait at least 3ms (see TOSCSTAB
				                            // in Table 7-3 from datasheet
				                            // Electrical Specifications) + 1ms
				                            // because GetCurrentms can be 1
				                            // cycle before the new ms. If
				                            // timeout then return the error
		}

		//--- Set desired SPI clock speed -------------------------
		if (pComp->SPIClockSpeed > (((CompFreq >> 1) * 85) / 100))
			return ERR__SPI_FREQUENCY_ERROR; // Ensure that FSCK is less than or
			                                 // equal to 0.85*(FSYSCLK/2).
			                                 // Follows datasheets errata for:
			                                 // The SPI can write corrupted data
			                                 // to the RAM at fast SPI speeds
		if ((pComp->DriverConfig & MCP251XFD_DRIVER_SAFE_RESET) > 0)
		{
			Error = pComp->fnSPI_Init(
			  pComp->InterfaceDevice,
			  pComp->SPI_ChipSelect,
			  pComp->SPIClockSpeed); // Set the SPI speed clock to desired clock
			                         // speed
			if (Error != ERR_OK)
				return Error; // If there is an error while changing SPI
				              // interface speed then return the error
		}

		//--- Configure CRC Interrupts ----------------------------
		if ((pComp->DriverConfig & MCP251XFD_DRIVER_USE_READ_WRITE_CRC) >
		    0) // If there is a DRIVER_USE_READ_WRITE_CRC flag then
		{
			Error = MCP251XFD_ConfigureCRC(
			  pComp, MCP251XFD_CRC_ALL_EVENTS); // Configure the CRC and all
			                                    // interrupts related to CRC
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ConfigureCRC() then return the error
		}

		//--- Check which MCP251XFD it is ------------------------- // Since the
		// DEVID register return the same value for MCP2517FD and MCP2518FD,
		// this driver use the OSC.LPMEN to check which one it is
		Error = MCP251XFD_ReadSFR8(pComp,
		                           RegMCP251XFD_OSC_CONFIG,
		                           &Config); // Read current OSC config mode
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		pComp->InternalConfig |=
		  MCP251XFD_DEV_ID_SET((Config & MCP251XFD_SFR_OSC8_LPMEN) > 0
		                         ? MCP2518FD
		                         : MCP2517FD); // Set which one it is to the
		                                       // internal config of the driver
		Config &= ~MCP251XFD_SFR_OSC8_LPMEN;
		Error = MCP251XFD_WriteSFR8(
		  pComp,
		  RegMCP251XFD_OSC_CONFIG,
		  Config); // Write the OSC config mode with the LPM bit cleared
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_WriteSFR8() then return the error

		//--- Test SPI connection and RAM Test --------------------
		if ((pComp->DriverConfig & MCP251XFD_DRIVER_INIT_CHECK_RAM) >
		    0) // If there is a DRIVER_INIT_CHECK_RAM flag then
		{
			Error = __MCP251XFD_TestRAM(
			  pComp); // Check the all the RAM only of the device
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // __MCP251XFD_TestRAM() then return the error
		}
		else // Else check only SPI interface
		{
			Error = MCP251XFD_WriteRAM32(
			  pComp,
			  (MCP251XFD_RAM_ADDR + MCP251XFD_RAM_SIZE - 4),
			  0xAA55AA55); // Write 0xAA55AA55 at address
			if (Error != ERR_NONE)
				return Error; // If there is an error while writing the RAM
				              // address then return the error
			Error =
			  MCP251XFD_ReadRAM32(pComp,
			                      (MCP251XFD_RAM_ADDR + MCP251XFD_RAM_SIZE - 4),
			                      &Result); // Read again the data
			if (Error != ERR_NONE)
				return Error; // If there is an error while reading the RAM
				              // address then return the error
			if (Result != 0xAA55AA55)
				return ERR__RAM_TEST_FAIL; // If data read is not 0xAA55AA55
				                           // then return an error
		}

		//--- Configure RAM ECC -----------------------------------
		if ((pComp->DriverConfig & MCP251XFD_DRIVER_ENABLE_ECC) >
		    0) // If there is a DRIVER_ENABLE_ECC flag then
		{
			Error =
			  MCP251XFD_ConfigureECC(pComp,
			                         true,
			                         MCP251XFD_ECC_ALL_EVENTS,
			                         0x55); // Configure the ECC and enable all
			                                // interrupts related to ECC
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ConfigureECC() then return the error
		}

		//--- Initialize RAM if configured ------------------------
		if ((pComp->DriverConfig & MCP251XFD_DRIVER_INIT_SET_RAM_AT_0) >
		    0) // If there is a DRIVER_INIT_SET_RAM_AT_0 flag then
		{
			Error = MCP251XFD_InitRAM(
			  pComp); // Initialize all RAM addresses with 0x00000000
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_InitRAM() then return the error
		}

		//--- Initialize Int pins or GPIOs ------------------------
		Error = MCP251XFD_SetGPIOPinsOutputLevel(
		  pComp,
		  pComp->GPIOsOutLevel,
		  MCP251XFD_GPIO0_Mask |
		    MCP251XFD_GPIO1_Mask); // Set GPIO pins output level before change
		                           // to mode GPIO. This is to get directly the
		                           // good output level when (if) pins will be
		                           // in output mode
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ConfigurePins() then return the error
		Error = MCP251XFD_ConfigurePins(
		  pComp,
		  pConf->GPIO0PinMode,
		  pConf->GPIO1PinMode,
		  pConf->INTsOutMode,
		  pConf->TXCANOutMode,
		  (pConf->ClkoPinConfig == MCP251XFD_CLKO_SOF)); // Configure pins
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ConfigurePins() then return the error

		//--- Set Nominal and Data bitrate ------------------------
		MCP251XFD_BitTimeConfig ConfBitTime;
		ConfBitTime.Stats = pConf->BitTimeStats;
		Error             = MCP251XFD_CalculateBitTimeConfiguration(
          CompFreq,
          pConf->NominalBitrate,
          pConf->DataBitrate,
          &ConfBitTime); // Calculate Bit Time
		if (Error != ERR_OK)
			return Error; // If there is an error while calling
			              // MCP251XFD_CalculateBitTimeConfiguration() then
			              // return the error
		Error = MCP251XFD_SetBitTimeConfiguration(
		  pComp,
		  &ConfBitTime,
		  pConf->DataBitrate ==
		    MCP251XFD_NO_CANFD); // Set Bit Time configuration to registers
		if (Error != ERR_OK)
			return Error; // If there is an error while calling
			              // MCP251XFD_SetBitTimeConfiguration() then return the
			              // error

		//--- CAN configuration -----------------------------------
		Error = MCP251XFD_WriteSFR8(
		  pComp,
		  RegMCP251XFD_CiCON + 2,
		  0x00); // Disable TEF and TXQ configuration in the RegMCP251XFD_CiCON
		         // register (Third byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_WriteSFR8() then return the error
		Error = MCP251XFD_ConfigureCANController(
		  pComp,
		  pConf->ControlFlags,
		  pConf->Bandwidth); // Configure the CAN control
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ConfigureCANControl() then return the
			              // error

		//--- System interrupt enable -----------------------------
		Error = MCP251XFD_ConfigureInterrupt(
		  pComp,
		  (setMCP251XFD_InterruptEvents)
		    pConf->SysInterruptFlags); // Configure interrupts to enable
		return Error;
	}

	/*! @brief RAM initialization of the MCP251XFD
	 *
	 * This function initialize all the RAM addresses with 0x00000000
	 * @param[in] *pComp Is the pointed structure of the device where the RAM
	 * will be initialize
	 * @return Returns an #eERRORRESULT value enum
	 */
	//=============================================================================
	// [STATIC] RAM initialization of the MCP251XFD device
	//=============================================================================
	eERRORRESULT MCP251XFD_InitRAM(MCP251XFD *pComp)
	{
		eERRORRESULT Error;

		for (uint16_t Address = MCP251XFD_RAM_ADDR;
		     Address < (MCP251XFD_RAM_ADDR + MCP251XFD_RAM_SIZE);
		     Address += 4)
		{
			Error = MCP251XFD_WriteRAM32(
			  pComp, Address, 0x00000000); // Write 0x00000000 at address
			if (Error != ERR_NONE)
				return Error; // If there is an error while writing the RAM
				              // address then return the error
		}
		return ERR_NONE;
	}

	//********************************************************************************************************************

	/*! @brief Get actual device of the MCP251XFD
	 *
	 * @param[in] *pComp Is the pointed structure of the device
	 * @param[out] *device Is the device found (MCP2517FD or MCP2518FD)
	 * @param[out] *deviceId Is the returned device ID (This parameter can be
	 * NULL if not needed)
	 * @param[out] *deviceRev Is the returned device Revision (This parameter
	 * can be NULL if not needed)
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_GetDeviceID(MCP251XFD          *pComp,
	                                   eMCP251XFD_Devices *device,
	                                   uint8_t            *deviceId,
	                                   uint8_t            *deviceRev)
	{
#ifdef CHECK_NULL_PARAM
		if ((pComp == NULL) || (device == NULL))
			return ERR__PARAMETER_ERROR;
#endif
		*device = (eMCP251XFD_Devices)MCP251XFD_DEV_ID_GET(
		  pComp
		    ->InternalConfig); // Get device found when initializing the module
		eERRORRESULT Error;

		if ((deviceId != NULL) || (deviceRev != NULL))
		{
			uint8_t Value;
			Error = MCP251XFD_ReadSFR8(
			  pComp,
			  RegMCP251XFD_DEVID,
			  &Value); // Read value of the DEVID register (First byte only)
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ReadSFR8() then return the error
			if (deviceId != NULL)
				*deviceId = MCP251XFD_SFR_DEVID8_ID_GET(Value); // Get Device ID
			if (deviceRev != NULL)
				*deviceRev =
				  MCP251XFD_SFR_DEVID8_REV_GET(Value); // Get Device Revision
		}
		return ERR_NONE;
	}

	//********************************************************************************************************************

	/*! @brief Read data from the MCP251XFD
	 *
	 * Read data from the MCP251XFD. In case of data reading data from the RAM,
	 * the size should be modulo 4
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] address Is the address where data will be read in the
	 * MCP251XFD (address will be incremented automatically)
	 * @param[out] *data Is where the data will be stored
	 * @param[in] size Is the size of the data array to read
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_ReadData(MCP251XFD *pComp,
	                                uint16_t   address,
	                                uint8_t   *data,
	                                uint16_t   size)
	{
#ifdef CHECK_NULL_PARAM
		if ((pComp == NULL) || (data == NULL))
			return ERR__PARAMETER_ERROR;
		if (pComp->fnSPI_Transfer == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		const bool UseCRC =
		  ((pComp->DriverConfig & MCP251XFD_DRIVER_USE_READ_WRITE_CRC) > 0);
		const bool InRAM =
		  (address >= MCP251XFD_RAM_ADDR) &&
		  (address < (MCP251XFD_RAM_ADDR +
		              MCP251XFD_RAM_SIZE)); // True if address is in the RAM
		                                    // region ; False if address is in
		                                    // Controller SFR or CAN SFR region
		if (address > MCP251XFD_END_ADDR)
			return ERR__PARAMETER_ERROR;
		uint8_t      Buffer[MCP251XFD_TRANS_BUF_SIZE];
		eERRORRESULT Error;

		//--- Define the Increment value ---
		uint16_t Increment =
		  MCP251XFD_TRANS_BUF_SIZE -
		  (UseCRC ? 5 : 2);  // If use CRC for read, Buffer size minus 2 for
		                     // Command, minus 1 for length, minus 2 for CRC,
		                     // else just 2 for command
		if (UseCRC && InRAM) // In RAM region and use CRC for read?
		{
			if ((size & 0b11) != 0)
				return ERR__DATA_MODULO; // size should be a multiple of 4 in
				                         // case of Write CRC or Safe Write
			Increment &=
			  0xFFFC; // If in RAM region then the increment should be the
			          // nearest less than or equal value multiple by 4
		} // Here Increment cannot be 0 because MCP251XFD_TRANS_BUF_SIZE is
		  // compiler protected to be not less than 9

		//--- Create all clusters of data and send them ---
		uint8_t *pBuf;
		size_t   BufRemain = 0;
		size_t   ByteCount;
		while (size > 0)
		{
			const uint16_t Addr = MCP251XFD_SPI_16BITS_WORD(
			  (UseCRC ? MCP251XFD_SPI_INSTRUCTION_READ_CRC
			          : MCP251XFD_SPI_INSTRUCTION_READ),
			  address);
			//--- Compose SPI command ---
			pBuf  = &Buffer[0];
			*pBuf = ((Addr >> 8) & 0xFF); // Set first byte of SPI command
			++pBuf;
			*pBuf = Addr & 0xFF; // Set next byte of SPI command
			++pBuf;
			ByteCount = (size > Increment ? Increment
			                              : size); // Define byte count to send

			//--- If needed, set 0x00 byte while reading data on SPI interface
			// else send garbage data ---
			if ((pComp->DriverConfig &
			     MCP251XFD_DRIVER_CLEAR_BUFFER_BEFORE_READ) > 0)
			{
				const size_t BuffUsedSize =
				  ByteCount + (UseCRC
				                 ? 2 + 1 - 2
				                 : 2); // Here 2 for Command + 1 for Length - 2
				                       // for CRC (CRC on the SPI send is not
				                       // checked by the controller thus will be
				                       // set to 0 too), else just 2 for command
				for (size_t z = 2; z < BuffUsedSize; ++z)
					Buffer[z] = 0x00; // Set to 0
			}

			// Set length of data
			uint8_t LenData = 0;
			if (UseCRC && InRAM)
				LenData = (ByteCount >> 2) &
				          0xFF; // If use CRC for read and in RAM, set how many
				                // data word that is requested
			else
				LenData =
				  ByteCount & 0xFF; // Set how many data byte that is requested
			if (UseCRC)
			{
				*pBuf = LenData;
				++pBuf;
			} // Add Len in the frame if use CRC

			//--- Now send the data through SPI interface ---
			const size_t ByteToReadCount =
			  ByteCount + (UseCRC ? (2 + 1 + 2)
			                      : 2); // In case of use CRC for read, here are
			                            // 2 bytes for Command + 1 for Length +
			                            // 2 for CRC, else just 2 for command
			Error = pComp->fnSPI_Transfer(
			  pComp->InterfaceDevice,
			  pComp->SPI_ChipSelect,
			  &Buffer[0],
			  &Buffer[0],
			  ByteToReadCount); // Transfer the data in the buffer
			if (Error != ERR_NONE)
				return Error; // If there is an error while transferring data
				              // then return the error

			//--- Copy buffer to data ---
			BufRemain =
			  ByteCount; // Set how many data that will fit in the buffer
			while ((BufRemain > 0) && (size > 0))
			{
				*data = *pBuf; // Copy data
				++pBuf;
				++data;
				--BufRemain;
				--size;
				++address;
			}

			//--- Check CRC ---
			if (UseCRC)
			{
				// Replace Head data of the Tx buffer into the Rx buffer
				Buffer[0] = (uint8_t)((Addr >> 8) & 0xFF);
				Buffer[1] = (uint8_t)((Addr >> 0) & 0xFF);
				Buffer[2] = LenData;

				// Compute CRC and compare to the one in the buffer
#ifdef CHECK_NULL_PARAM
				if (pComp->fnComputeCRC16 == NULL)
					return ERR__PARAMETER_ERROR; // If the CRC function is not
					                             // present, raise an error
#endif
				uint16_t CRC = pComp->fnComputeCRC16(
				  &Buffer[0],
				  (ByteCount + 2 + 1)); // Compute CRC on the Buffer data (2 for
				                        // Command + 1 for Length)
				uint16_t BufCRC = *pBuf
				                  << 8; // Get CRC MSB on the next buffer byte
				++pBuf;
				BufCRC |= *pBuf; // Get CRC LSB on the next buffer byte
				if (CRC != BufCRC)
					return ERR__CRC_ERROR; // If CRC mismatch, then raise an
					                       // error
			}
		}

		return ERR_NONE;
	}

	/*! @brief Read a byte data from an SFR register of the MCP251XFD
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] address Is the SFR address where data will be read in the
	 * MCP251XFD
	 * @param[out] *data Is where the data will be stored
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_ReadSFR8(MCP251XFD *pComp, uint16_t address, uint8_t *data)
	{
		return MCP251XFD_ReadData(pComp, address, data, 1);
	}

	/*! @brief Read a 2-bytes data from an SFR address of the MCP251XFD
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] address Is the address where data will be read in the
	 * MCP251XFD
	 * @param[out] *data Is where the data will be stored
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_ReadSFR16(MCP251XFD *pComp, uint16_t address, uint16_t *data)
	{
		if (data == NULL)
			return ERR__PARAMETER_ERROR;
		MCP251XFD_uint16t_Conv Tmp;
		eERRORRESULT           Error =
		  MCP251XFD_ReadData(pComp, address, &Tmp.Bytes[0], 2);
		*data = Tmp.Uint16;
		return Error;
	}

	/*! @brief Read a word data (4 bytes) from an SFR address of the MCP251XFD
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] address Is the address where data will be read in the
	 * MCP251XFD
	 * @param[out] *data Is where the data will be stored
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_ReadSFR32(MCP251XFD *pComp, uint16_t address, uint32_t *data)
	{
		if (data == NULL)
			return ERR__PARAMETER_ERROR;
		MCP251XFD_uint32t_Conv Tmp;
		eERRORRESULT           Error =
		  MCP251XFD_ReadData(pComp, address, &Tmp.Bytes[0], 4);
		*data = Tmp.Uint32;
		return Error;
	}

	/*! @brief Read a word data (4 bytes) from a RAM address of the MCP251XFD
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] address Is the address where data will be read in the
	 * MCP251XFD
	 * @param[out] *data Is where the data will be stored
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_ReadRAM32(MCP251XFD *pComp, uint16_t address, uint32_t *data)
	{
		if (data == NULL)
			return ERR__PARAMETER_ERROR;
		MCP251XFD_uint32t_Conv Tmp;
		eERRORRESULT           Error =
		  MCP251XFD_ReadData(pComp, address, &Tmp.Bytes[0], 4);
		*data = Tmp.Uint32;
		return Error;
	}

	/*! @brief Write data to the MCP251XFD
	 *
	 * Write data to the MCP251XFD. In case of data writing data to the RAM, the
	 * size should be modulo 4
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] address Is the address where data will be written in the
	 * MCP251XFD (address will be incremented automatically)
	 * @param[in] *data Is the data array to write
	 * @param[in] size Is the size of the data array to write
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_WriteData(MCP251XFD     *pComp,
	                                 uint16_t       address,
	                                 const uint8_t *data,
	                                 uint16_t       size)
	{
#ifdef CHECK_NULL_PARAM
		if ((pComp == NULL) || (data == NULL))
			return ERR__PARAMETER_ERROR;
		if (pComp->fnSPI_Transfer == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		const bool UseCRC =
		  ((pComp->DriverConfig & (MCP251XFD_DRIVER_USE_READ_WRITE_CRC |
		                           MCP251XFD_DRIVER_USE_SAFE_WRITE)) > 0);
		const bool UseSafe =
		  ((pComp->DriverConfig & MCP251XFD_DRIVER_USE_SAFE_WRITE) > 0);
		const bool InRAM =
		  (address >= MCP251XFD_RAM_ADDR) &&
		  (address < (MCP251XFD_RAM_ADDR +
		              MCP251XFD_RAM_SIZE)); // True if address is in the RAM
		                                    // region ; False if address is in
		                                    // Controller SFR or CAN SFR region
		if (address > MCP251XFD_END_ADDR)
			return ERR__PARAMETER_ERROR;
		uint8_t      Buffer[MCP251XFD_TRANS_BUF_SIZE];
		uint32_t     Increment;
		eERRORRESULT Error;

		//--- Define the Increment value ---
		uint32_t Instruction = (UseCRC ? MCP251XFD_SPI_INSTRUCTION_WRITE_CRC
		                               : MCP251XFD_SPI_INSTRUCTION_WRITE);
		if (UseSafe == false)
		{
			Increment =
			  MCP251XFD_TRANS_BUF_SIZE -
			  (UseCRC ? 5 : 2); // If use CRC for writeBuffer size minus 2 for
			                    // Command, minus 1 for length, minus 2 for CRC,
			                    // else just 2 for command
			if (UseCRC && InRAM) // In RAM region and use CRC for write
			{
				if ((size & 0b11) != 0)
					return ERR__DATA_MODULO; // size should be a multiple of 4
					                         // in case of Write CRC or Safe
					                         // Write
				Increment &=
				  0xFFFC; // If in RAM region then the increment should be the
				          // nearest less than or equal value multiple by 4
			} // Here Increment cannot be 0 because MCP251XFD_TRANS_BUF_SIZE is
			  // compiler protected to be not less than 9
		}
		else
		{
			Instruction =
			  MCP251XFD_SPI_INSTRUCTION_SAFE_WRITE; // Set safe write
			                                        // instruction
			if (InRAM)
			{
				if ((size & 0b11) != 0)
					return ERR__DATA_MODULO; // size should be a multiple of 4
					                         // in case of Write CRC or Safe
					                         // Write
				Increment = 4; // If use safe write and in RAM then the
				               // increment is 4 bytes
			}
			else
				Increment = 1; // If use safe write and in SFR then the
				               // increment is 1 byte
		}

		//--- Create all clusters of data and send them ---
		uint8_t *pBuf;
		size_t   BufRemain = 0;
		size_t   ByteCount = 0;
		while (size > 0)
		{
			const uint16_t Addr =
			  MCP251XFD_SPI_16BITS_WORD(Instruction, address);
			//--- Compose SPI command ---
			pBuf  = &Buffer[0];
			*pBuf = ((Addr >> 8) & 0xFF); // Set first byte of SPI command
			++pBuf;
			*pBuf = Addr & 0xFF; // Set next byte of SPI command
			++pBuf;

			//--- Set length of data ---
			ByteCount = (size > Increment ? Increment
			                              : size); // Define byte count to send
			if (UseCRC &&
			    (UseSafe ==
			     false)) // Add Len in the frame if use CRC but not use safe
			{
				if (InRAM)
					*pBuf = (ByteCount >> 2) &
					        0xFF; // If use CRC for write and in RAM, set how
					              // many data word that is requested
				else
					*pBuf = ByteCount &
					        0xFF; // Set how many data byte that is requested
				++pBuf;
			}

			//--- Copy data to buffer ---
			BufRemain =
			  Increment; // Set how many data that will fit in the buffer
			while ((BufRemain > 0) && (size > 0))
			{
				*pBuf = *data; // Copy data
				++pBuf;
				++data;
				--BufRemain;
				--size;
				++address;
			}

			//--- Compute CRC and add to the buffer ---
			if (UseCRC)
			{
#ifdef CHECK_NULL_PARAM
				if (pComp->fnComputeCRC16 == NULL)
					return ERR__PARAMETER_ERROR; // If the CRC function is not
					                             // present, raise an error
#endif
				const size_t ByteToComputeCount =
				  ByteCount + (UseSafe ? 2 : (2 + 1));
				uint16_t FrameCRC = pComp->fnComputeCRC16(
				  &Buffer[0],
				  ByteToComputeCount); // Compute CRC on the Buffer data (2 for
				                       // Command + 1 for Length for not safe)
				*pBuf =
				  (FrameCRC >> 8) & 0xFF; // Put CRC MSB on the next buffer byte
				++pBuf;
				*pBuf = FrameCRC & 0xFF; // Put CRC LSB on the next buffer byte
			}

			//--- Now send the data through SPI interface ---
			const size_t ByteToWriteCount =
			  ByteCount +
			  (UseSafe
			     ? (2 + 2)
			     : (UseCRC ? (2 + 1 + 2)
			               : 2)); // In case of use CRC for write here are 2
			                      // bytes for Command + 1 for Length for not
			                      // safe + 2 for CRC, else just 2 for command
			Error = pComp->fnSPI_Transfer(
			  pComp->InterfaceDevice,
			  pComp->SPI_ChipSelect,
			  &Buffer[0],
			  NULL,
			  ByteToWriteCount); // Transfer the data in the buffer (2 for
			                     // Command + 1 for Length + 2 for CRC)
			if (Error != ERR_NONE)
				return Error; // If there is an error while transferring data
				              // then return the error
		}

		return ERR_NONE;
	}

	/*! @brief Write a byte data to an SFR register of the MCP251XFD
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] address Is the address where data will be written in the
	 * MCP251XFD
	 * @param[in] data Is the data to write
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_WriteSFR8(MCP251XFD *pComp, uint16_t address, const uint8_t data)
	{
		return MCP251XFD_WriteData(pComp, address, &data, 1);
	}

	/*! @brief Write a 2-bytes data to an SFR register of the MCP251XFD
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] address Is the address where data will be written in the
	 * MCP251XFD
	 * @param[in] data Is the data to write
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_WriteSFR16(MCP251XFD     *pComp,
	                                         uint16_t       address,
	                                         const uint16_t data)
	{
		MCP251XFD_uint16t_Conv Tmp;
		Tmp.Uint16 = data;
		eERRORRESULT Error =
		  MCP251XFD_WriteData(pComp, address, &Tmp.Bytes[0], 2);
		return Error;
	}

	/*! @brief Write a word data (4 bytes) to an SFR register of the MCP251XFD
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] address Is the address where data will be written in the
	 * MCP251XFD
	 * @param[in] data Is the data to write
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_WriteSFR32(MCP251XFD     *pComp,
	                                         uint16_t       address,
	                                         const uint32_t data)
	{
		MCP251XFD_uint32t_Conv Tmp;
		Tmp.Uint32 = data;
		eERRORRESULT Error =
		  MCP251XFD_WriteData(pComp, address, &Tmp.Bytes[0], 4);
		return Error;
	}

	/*! @brief Write a word data (4 bytes) to a RAM register of the MCP251XFD
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] address Is the address where data will be written in the
	 * MCP251XFD
	 * @param[in] data Is the data to write
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_WriteRAM32(MCP251XFD *pComp, uint16_t address, uint32_t data)
	{
		MCP251XFD_uint32t_Conv Tmp;
		Tmp.Uint32 = data;
		eERRORRESULT Error =
		  MCP251XFD_WriteData(pComp, address, &Tmp.Bytes[0], 4);
		return Error;
	}

	//********************************************************************************************************************

	/*! @brief Transmit a message object (with data) to a FIFO of the MCP251XFD
	 *
	 * Transmit the message to the specified FIFO. This function uses the
	 * specific format of the component (T0, T1, Ti). This function gets the
	 * next address where to put the message object on, send it and update the
	 * head pointer
	 * @warning This function does not check if the FIFO have a room for the
	 * message or if the FIFO is actually a transmit FIFO or the actual state of
	 * the FIFO
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] *messageObjectToSend Is the message object to send with all
	 * its data
	 * @param[in] objectSize Is the size of the message object (with its data).
	 * This value needs to be modulo 4
	 * @param[in] toFIFO Is the name of the FIFO to fill
	 * @param[in] andFlush Indicate if the FIFO will be flush to the CAN bus
	 * right after this message
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_TransmitMessageObjectToFIFO(MCP251XFD      *pComp,
	                                      uint8_t        *messageObjectToSend,
	                                      uint8_t         objectSize,
	                                      eMCP251XFD_FIFO toFIFO,
	                                      bool            andFlush)
	{
#ifdef CHECK_NULL_PARAM
		if ((pComp == NULL) || (messageObjectToSend == NULL))
			return ERR__PARAMETER_ERROR;
#endif
		if (toFIFO == MCP251XFD_TEF)
			return ERR__PARAMETER_ERROR;
		if ((objectSize & 0x3) > 0)
			return ERR__BYTE_COUNT_MODULO_4;
		eERRORRESULT Error;

		//--- Get address where to write the frame ---
		uint32_t NextAddress = 0;
		Error                = MCP251XFD_GetNextMessageAddressFIFO(
          pComp, toFIFO, &NextAddress, NULL); // Get next message address
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_GetNextMessageAddressFIFO() then return
			              // the error
		NextAddress += MCP251XFD_RAM_ADDR;

		//--- Write data to RAM ---
		Error = MCP251XFD_WriteData(pComp,
		                            NextAddress,
		                            &messageObjectToSend[0],
		                            objectSize); // Write data to RAM address
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_WriteData() then return the error

		//--- Update FIFO and flush if asked ---
		return MCP251XFD_UpdateFIFO(pComp, toFIFO, andFlush);
	}

	/*! @brief Transmit a message object (with data) to the TXQ of the MCP251XFD
	 *
	 * Transmit the message to the TXQ. This function uses the specific format
	 * of the component (T0, T1, Ti). This function gets the next address where
	 * to put the message object on, send it and update the head pointer
	 * @warning This function do not not check if the TXQ have a room for the
	 * message or the actual state of the TXQ
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] *messageObjectToSend Is the message object to send with all
	 * its data
	 * @param[in] objectSize Is the size of the message object (with its data).
	 * This value need to be modulo 4
	 * @param[in] andFlush Indicate if the TXQ will be flush to the CAN bus
	 * right after this message
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_TransmitMessageObjectToTXQ(MCP251XFD *pComp,
	                                     uint8_t   *messageObjectToSend,
	                                     uint8_t    objectSize,
	                                     bool       andFlush)
	{
		return MCP251XFD_TransmitMessageObjectToFIFO(
		  pComp, messageObjectToSend, objectSize, MCP251XFD_TXQ, andFlush);
	}

	/*! @brief Transmit a message to a FIFO of the MCP251XFD
	 *
	 * Transmit the message to the specified FIFO
	 * This function gets the next address where to put the message object on,
	 * send it and update the head pointer
	 * @warning This function does not check if the FIFO have a room for the
	 * message or if the FIFO is actually a transmit FIFO or the actual state of
	 * the FIFO
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] *messageToSend Is the message to send with all the data
	 * attached with
	 * @param[in] toFIFO Is the name of the FIFO to fill
	 * @param[in] andFlush Indicate if the FIFO will be flush to the CAN bus
	 * right after this message
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_TransmitMessageToFIFO(MCP251XFD            *pComp,
	                                MCP251XFD_CANMessage *messageToSend,
	                                eMCP251XFD_FIFO       toFIFO,
	                                bool                  andFlush)
	{
#ifdef CHECK_NULL_PARAM
		if ((pComp == NULL) || (messageToSend == NULL))
			return ERR__PARAMETER_ERROR;
#endif
		if (toFIFO == MCP251XFD_TEF)
			return ERR__PARAMETER_ERROR;
		uint8_t                   Buffer[MCP251XFD_CAN_TX_MESSAGE_SIZE_MAX];
		MCP251XFD_CAN_TX_Message *Message =
		  (MCP251XFD_CAN_TX_Message *)Buffer; // The first 8 bytes represent the
		                                      // MCP251XFD_CAN_TX_Message struct

		//--- Fill message ID (T0) ---
		bool Extended =
		  ((messageToSend->ControlFlags & MCP251XFD_EXTENDED_MESSAGE_ID) > 0);
		bool CANFDframe =
		  ((messageToSend->ControlFlags & MCP251XFD_CANFD_FRAME) > 0);
		Message->T0.T0 = MCP251XFD_MessageIDtoObjectMessageIdentifier(
		  messageToSend->MessageID,
		  Extended,
		  MCP251XFD_USE_SID11 && CANFDframe);

		//--- Fill message controls (T1) ---
		Message->T1.T1  = 0; // Initialize message Controls to 0
		Message->T1.SEQ = messageToSend->MessageSEQ; // Set message Sequence
		if (CANFDframe)
			Message->T1.FDF = 1;
		if ((messageToSend->ControlFlags & MCP251XFD_SWITCH_BITRATE) > 0)
			Message->T1.BRS = 1;
		if ((messageToSend->ControlFlags &
		     MCP251XFD_REMOTE_TRANSMISSION_REQUEST) > 0)
			Message->T1.RTR = 1;
		if ((messageToSend->ControlFlags & MCP251XFD_EXTENDED_MESSAGE_ID) > 0)
			Message->T1.IDE = 1;
		if ((messageToSend->ControlFlags & MCP251XFD_TRANSMIT_ERROR_PASSIVE) >
		    0)
			Message->T1.ESI = 1;
		Message->T1.DLC = (uint8_t)messageToSend->DLC;

		//--- Fill payload data ---
		if ((messageToSend->DLC != MCP251XFD_DLC_0BYTE) &&
		    (messageToSend->PayloadData == NULL))
			return ERR__NO_DATA_AVAILABLE;
		uint8_t BytesDLC = MCP251XFD_DLCToByte(messageToSend->DLC, CANFDframe);
		if (messageToSend->PayloadData != NULL)
		{
			uint8_t *pBuff = &Buffer[sizeof(
			  MCP251XFD_CAN_TX_Message)]; // Next bytes of the Buffer is for
			                              // payload
			uint8_t *pData =
			  &messageToSend
			     ->PayloadData[0]; // Select the first byte of payload data
			uint8_t BytesToCopy =
			  BytesDLC; // Get how many byte in the payload data will be copied
			while (BytesToCopy-- > 0)
				*pBuff++ = *pData++;  // Copy data
			if ((BytesDLC & 0x3) > 0) // Not modulo 4?
				for (uint8_t z = 0; z < (4 - (BytesDLC & 0x3)); z++)
					*pBuff++ = 0; // Fill with 0
		}

		//--- Send data ---
		uint8_t BytesToSend = (sizeof(MCP251XFD_CAN_TX_Message) + BytesDLC);
		if ((BytesToSend & 0x3) != 0)
			BytesToSend =
			  (BytesToSend & 0xFC) + 4; // Adjust to the upper modulo 4 bytes
			                            // (mandatory for RAM access)
		return MCP251XFD_TransmitMessageObjectToFIFO(
		  pComp, &Buffer[0], BytesToSend, toFIFO, andFlush);
	}

	/*! @brief Transmit a message to the TXQ of the MCP251XFD
	 *
	 * Transmit the message to the TXQ
	 * This function gets the next address where to put the message object on,
	 * send it and update the head pointer
	 * @warning This function does not check if the TXQ have a room for the
	 * message or the actual state of the TXQ
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] *messageToSend Is the message to send with all the data
	 * attached with
	 * @param[in] andFlush Indicate if the TXQ will be flush to the CAN bus
	 * right after this message
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_TransmitMessageToTXQ(MCP251XFD            *pComp,
	                               MCP251XFD_CANMessage *messageToSend,
	                               bool                  andFlush)
	{
		return MCP251XFD_TransmitMessageToFIFO(
		  pComp, messageToSend, MCP251XFD_TXQ, andFlush);
	}

	/*! @brief Receive a message object (with data) to the FIFO of the MCP251XFD
	 *
	 * Receive the message from the specified FIFO. This function uses the
	 * specific format of the component (R0, R1, (R2,) Ri). This function gets
	 * the next address where to get the message object from, get it and update
	 * the tail pointer
	 * @warning This function does not check if the FIFO have a message pending
	 * or if the FIFO is actually a receive FIFO or the actual state of the FIFO
	 * or if the TimeStamp is set or not
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *messageObjectGet Is the message object retrieve with all its
	 * data
	 * @param[in] objectSize Is the size of the message object (with its data).
	 * This value needs to be modulo 4
	 * @param[in] fromFIFO Is the name of the FIFO to extract
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_ReceiveMessageObjectFromFIFO(MCP251XFD      *pComp,
	                                       uint8_t        *messageObjectGet,
	                                       uint8_t         objectSize,
	                                       eMCP251XFD_FIFO fromFIFO)
	{
#ifdef CHECK_NULL_PARAM
		if ((pComp == NULL) || (messageObjectGet == NULL))
			return ERR__PARAMETER_ERROR;
#endif
		if (fromFIFO == MCP251XFD_TXQ)
			return ERR__PARAMETER_ERROR;
		if ((objectSize & 0x3) > 0)
			return ERR__BYTE_COUNT_MODULO_4;
		eERRORRESULT Error;

		//--- Get address where to write the frame ---
		uint32_t NextAddress = 0;
		Error                = MCP251XFD_GetNextMessageAddressFIFO(
          pComp, fromFIFO, &NextAddress, NULL); // Get next message address
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_GetNextMessageAddressFIFO() then return
			              // the error
		NextAddress += MCP251XFD_RAM_ADDR; // Add RAM offset address

		//--- Read data from RAM ---
		Error = MCP251XFD_ReadData(pComp,
		                           NextAddress,
		                           &messageObjectGet[0],
		                           objectSize); // Read data to RAM address
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadData() then return the error

		//--- Update FIFO ---
		return MCP251XFD_UpdateFIFO(
		  pComp, fromFIFO, false); // Can't flush a receive FIFO
	}

	/*! @brief Receive a message object (with data) to the TEF of the MCP251XFD
	 *
	 * Receive a message from the TEF. This function uses the specific format of
	 * the component (TE0, TE1 (,TE2)). This function gets the next address
	 * where to get the message object from, get it and update the tail pointer
	 * @warning This function does not check if the TEF have a message pending
	 * or the actual state of the TEF or if the TimeStamp is set or not
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *messageObjectGet Is the message object retrieve with all its
	 * data
	 * @param[in] objectSize Is the size of the message object (with its data).
	 * This value needs to be modulo 4
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_ReceiveMessageObjectFromTEF(MCP251XFD *pComp,
	                                      uint8_t   *messageObjectGet,
	                                      uint8_t    objectSize)
	{
		return MCP251XFD_ReceiveMessageObjectFromFIFO(
		  pComp, messageObjectGet, objectSize, MCP251XFD_TEF);
	}

	/*! @brief Receive a message from a FIFO of the MCP251XFD
	 *
	 * Receive a message from the specified FIFO
	 * This function gets the next address where to get the message object from,
	 * get it and update the tail pointer
	 * @warning This function does not check if the FIFO have a message pending
	 * or if the FIFO is actually a receive FIFO or the actual state of the FIFO
	 * or if the TimeStamp is set or not
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *messageGet Is the message retrieve with all the data
	 * attached with
	 * @param[in] payloadSize Indicate the payload of the FIFO (8, 12, 16, 20,
	 * 24, 32, 48 or 64)
	 * @param[out] *timeStamp Is the returned TimeStamp of the message (can be
	 * set to NULL if the TimeStamp is not set in this FIFO)
	 * @param[in] fromFIFO Is the name of the FIFO to extract
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_ReceiveMessageFromFIFO(MCP251XFD             *pComp,
	                                 MCP251XFD_CANMessage  *messageGet,
	                                 eMCP251XFD_PayloadSize payloadSize,
	                                 uint32_t              *timeStamp,
	                                 eMCP251XFD_FIFO        fromFIFO)
	{
#ifdef CHECK_NULL_PARAM
		if ((pComp == NULL) || (messageGet == NULL))
			return ERR__PARAMETER_ERROR;
#endif
		if (fromFIFO == MCP251XFD_TXQ)
			return ERR__PARAMETER_ERROR;
		uint8_t                   Buffer[MCP251XFD_CAN_RX_MESSAGE_SIZE_MAX];
		MCP251XFD_CAN_RX_Message *Message =
		  (MCP251XFD_CAN_RX_Message *)Buffer; // The first 8 bytes represent the
		                                      // MCP251XFD_CAN_RX_Message struct
		eERRORRESULT Error;

		//--- Get data ---
		uint8_t BytesPayload = MCP251XFD_PayloadToByte(payloadSize);
		uint8_t BytesToGet = (sizeof(MCP251XFD_CAN_RX_Message) + BytesPayload);
		if (fromFIFO == MCP251XFD_TEF)
			BytesToGet =
			  sizeof(MCP251XFD_CAN_TX_EventObject); // In case of TEF, there is
			                                        // not data
		if (timeStamp == NULL)
			BytesToGet -=
			  sizeof(uint32_t); // Time Stamp not needed = 4 bytes less
		if ((BytesToGet & 0x3) != 0)
			BytesToGet =
			  (BytesToGet & 0xFC) + 4; // Adjust to the upper modulo 4 bytes
			                           // (mandatory for RAM access)
		Error = MCP251XFD_ReceiveMessageObjectFromFIFO(
		  pComp, &Buffer[0], BytesToGet, fromFIFO); // Read bytes from RAM
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReceiveMessageObjectFromFIFO() then
			              // return the error

		//--- Extract message ID (R0) ---
		bool Extended         = (Message->R1.IDE == 1);
		bool CANFDframe       = (Message->R1.FDF == 1);
		messageGet->MessageID = MCP251XFD_ObjectMessageIdentifierToMessageID(
		  Message->R0.R0,
		  Extended,
		  MCP251XFD_USE_SID11 && CANFDframe); // Extract SID/EID from R0

		//--- Extract message controls (R1) ---
		messageGet->ControlFlags = MCP251XFD_NO_MESSAGE_CTRL_FLAGS;
		messageGet->MessageSEQ   = 0u;
		if (fromFIFO == MCP251XFD_TEF)
			messageGet->MessageSEQ =
			  ((MCP251XFD_CAN_TX_EventObject *)Buffer)
			    ->TE1.SEQ; // If it is a TEF, extract the Sequence by casting
			               // Buffer into a TEF object and get SEQ in TE1
		if (CANFDframe)
			messageGet->ControlFlags =
			  (setMCP251XFD_MessageCtrlFlags)(messageGet->ControlFlags +
			                                  MCP251XFD_CANFD_FRAME);
		if (Message->R1.BRS == 1)
			messageGet->ControlFlags =
			  (setMCP251XFD_MessageCtrlFlags)(messageGet->ControlFlags +
			                                  MCP251XFD_SWITCH_BITRATE);
		if (Message->R1.RTR == 1)
			messageGet->ControlFlags =
			  (setMCP251XFD_MessageCtrlFlags)(messageGet->ControlFlags +
			                                  MCP251XFD_REMOTE_TRANSMISSION_REQUEST);
		if (Extended)
			messageGet->ControlFlags =
			  (setMCP251XFD_MessageCtrlFlags)(messageGet->ControlFlags +
			                                  MCP251XFD_EXTENDED_MESSAGE_ID);
		if (Message->R1.ESI == 1)
			messageGet->ControlFlags =
			  (setMCP251XFD_MessageCtrlFlags)(messageGet->ControlFlags +
			                                  MCP251XFD_TRANSMIT_ERROR_PASSIVE);
		messageGet->DLC = (eMCP251XFD_DataLength)Message->R1.DLC;

		//--- Extract TimeStamp ---
		uint8_t *pBuff =
		  &Buffer[sizeof(MCP251XFD_CAN_RX_Message_Identifier) +
		          sizeof(MCP251XFD_CAN_RX_Message_Control)]; // Next bytes of
		                                                     // the Buffer is
		                                                     // for timestamp
		                                                     // and/or payload
		if (timeStamp != NULL)
		{
			*timeStamp = Message->TimeStamp;
			pBuff +=
			  sizeof(uint32_t); // If TimeStamp extracted, update the pBuff
		}

		//--- Extract payload data ---
		if (fromFIFO != MCP251XFD_TEF)
		{
			if ((messageGet->DLC != MCP251XFD_DLC_0BYTE) &&
			    (messageGet->PayloadData == NULL))
				return ERR__NO_DATA_AVAILABLE;
			if (messageGet->PayloadData != NULL)
			{
				uint8_t *pData =
				  &messageGet
				     ->PayloadData[0]; // Select the first byte of payload data
				uint8_t BytesDLC = MCP251XFD_DLCToByte(
				  messageGet->DLC,
				  CANFDframe); // Get how many byte need to be extract from the
				               // message to correspond to its DLC
				if (BytesPayload < BytesDLC)
					BytesDLC = BytesPayload; // Get the least between
					                         // BytesPayload and BytesDLC
				while (BytesDLC-- > 0)
					*pData++ = *pBuff++; // Copy data
			}
		}

		return ERR_NONE;
	}

	/*! @brief Receive a message from the TEF of the MCP251XFD
	 *
	 * Receive a message from the TEF
	 * This function gets the next address where to get the message object from,
	 * get it and update the tail pointer
	 * @warning This function does not check if the TEF have a message pending
	 * or the actual state of the TEF or if the TimeStamp is set or not
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *messageGet Is the message retrieve with all the data
	 * attached with
	 * @param[out] *timeStamp Is the returned TimeStamp of the message (can be
	 * set to NULL if the TimeStamp is not set in the TEF)
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_ReceiveMessageFromTEF(MCP251XFD            *pComp,
	                                MCP251XFD_CANMessage *messageGet,
	                                uint32_t             *timeStamp)
	{
		return MCP251XFD_ReceiveMessageFromFIFO(
		  pComp,
		  messageGet,
		  MCP251XFD_PAYLOAD_8BYTE,
		  timeStamp,
		  MCP251XFD_TEF); // Here the payload will not be used inside the
		                  // function
	}

	//********************************************************************************************************************

	/*! @brief CRC Configuration of the MCP251XFD
	 *
	 * At initialization if the driver has the flag DRIVER_USE_READ_WRITE_CRC
	 * then all CRC interrupts are enabled.
	 * @param[in] *pComp Is the pointed structure of the device where the CRC
	 * will be configured
	 * @param[in] interrupts Corresponding bit to '1' enable the interrupt.
	 * Flags can be OR'ed
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_ConfigureCRC(MCP251XFD             *pComp,
	                                    setMCP251XFD_CRCEvents interrupts)
	{
		return MCP251XFD_WriteSFR8(pComp,
		                           RegMCP251XFD_CRC_CONFIG,
		                           interrupts); // Write configuration to the
		                                        // CRC register (last byte only)
	}

	/*! @brief Get CRC Status of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device where the CRC
	 * status will be obtained
	 * @param[out] *events Is the return value of current events flags of the
	 * CRC. Flags can be OR'ed
	 * @param[out] *lastCRCMismatch Is the Cycle Redundancy Check from last CRC
	 * mismatch. This parameter can be NULL if the last mismatch is not needed
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_GetCRCEvents(MCP251XFD              *pComp,
	                                    setMCP251XFD_CRCEvents *events,
	                                    uint16_t               *lastCRCMismatch)
	{
#ifdef CHECK_NULL_PARAM
		if (events == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		eERRORRESULT Error;

		Error = MCP251XFD_ReadSFR8(
		  pComp,
		  RegMCP251XFD_CRC_FLAGS,
		  (uint8_t *)
		    events); // Read status of the CRC register (third byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		*events =
		  (setMCP251XFD_CRCEvents)(*events &
		                           MCP251XFD_CRC_EVENTS_MASK); // Get CRC error
		                                                       // interrupt flag
		                                                       // status and CRC
		                                                       // command format
		                                                       // error
		                                                       // interrupt flag
		                                                       // status
		if (lastCRCMismatch != NULL)
		{
			Error = MCP251XFD_ReadSFR16(
			  pComp,
			  RegMCP251XFD_CRC_CRC,
			  lastCRCMismatch); // Read Address where last CRC error occurred
			                    // (first 2 bytes only)
		}
		return Error;
	}

	/*! @brief Clear CRC Status Flags of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device where the CRC
	 * status will be cleared
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_ClearCRCEvents(MCP251XFD *pComp)
	{
		return MCP251XFD_WriteSFR8(
		  pComp,
		  RegMCP251XFD_CRC_FLAGS,
		  MCP251XFD_CRC_NO_EVENT); // Write cleared status of the CRC register
		                           // (third byte only)
	}

	//********************************************************************************************************************

	/*! @brief ECC Configuration of the MCP251XFD device
	 *
	 * At initialization if the driver has the flag DRIVER_ENABLE_ECC then ECC
	 * is enable and all ECC interrupts are enable.
	 * @warning If the ECC is configured after use of the device without reset,
	 * ECC interrupts can occur if the RAM is not fully initialize (with
	 * MCP251XFD_InitRAM())
	 * @param[in] *pComp Is the pointed structure of the device where the ECC
	 * will be configured
	 * @param[in] enableECC Is at 'true' to enable ECC or 'false' to disable ECC
	 * @param[in] interrupts Corresponding bit to '1' enable the interrupt.
	 * Flags can be OR'ed
	 * @param[in] fixedParityValue Is the value of the parity bits used during
	 * write to RAM when ECC is disabled
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_ConfigureECC(MCP251XFD             *pComp,
	                                    bool                   enableECC,
	                                    setMCP251XFD_ECCEvents interrupts,
	                                    uint8_t                fixedParityValue)
	{
		uint8_t Config[2] = {MCP251XFD_SFR_ECCCON8_ECCDIS |
		                       MCP251XFD_SFR_ECCCON8_SECID |
		                       MCP251XFD_SFR_ECCCON8_DEDID,
		                     0};          // By default, disable all controls
		Config[0] |= (uint8_t)interrupts; // Single error correction and double
		                                  // error detection interrupts enable
		if (enableECC)
			Config[0] |= MCP251XFD_SFR_ECCCON8_ECCEN; // If ECC enable then add
			                                          // the ECC enable flag
		Config[1] = MCP251XFD_SFR_ECCCON8_PARITY_SET(
		  fixedParityValue); // Add the fixed parity used during write to RAM
		                     // when ECC is disabled
		return MCP251XFD_WriteData(
		  pComp,
		  RegMCP251XFD_ECCCON,
		  &Config[0],
		  2); // Write configuration to the ECC register (first 2 bytes only)
	}

	/*! @brief Get ECC Status Flags of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device where the ECC
	 * status will be obtained
	 * @param[out] *events Is the return value of current events flags of the
	 * ECC. Flags can be OR'ed
	 * @param[out] *lastErrorAddress Is the address where last ECC error
	 * occurred. This parameter can be NULL if the address is not needed
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_GetECCEvents(MCP251XFD              *pComp,
	                                    setMCP251XFD_ECCEvents *events,
	                                    uint16_t *lastErrorAddress)
	{
#ifdef CHECK_NULL_PARAM
		if (events == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		eERRORRESULT Error;

		Error = MCP251XFD_ReadSFR8(
		  pComp,
		  RegMCP251XFD_ECCSTAT_FLAGS,
		  (uint8_t *)
		    events); // Read status of the ECC register (first bytes only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		if (lastErrorAddress != NULL)
		{
			Error = MCP251XFD_ReadSFR16(
			  pComp,
			  RegMCP251XFD_ECCSTAT_ERRADDR,
			  lastErrorAddress); // Read Address where last ECC error occurred
			                     // (last 2 bytes only)
		}
		return Error;
	}

	/*! @brief Clear ECC Status Flags of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device where the ECC
	 * status will be cleared
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_ClearECCEvents(MCP251XFD *pComp)
	{
		return MCP251XFD_WriteSFR8(
		  pComp,
		  RegMCP251XFD_ECCSTAT_FLAGS,
		  MCP251XFD_CRC_NO_EVENT); // Write cleared status of the ECC register
		                           // (first byte only)
	}

	//********************************************************************************************************************

	/*! @brief Configure pins of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be configured
	 * @param[in] GPIO0PinMode Set the INT0/GPIO0/XSTBY pins mode
	 * @param[in] GPIO1PinMode Set the INT1/GPIO1 pins mode
	 * @param[in] INTOutMode Set the INTs (INT, INT0 and INT1) output pin mode
	 * @param[in] TXCANOutMode Set the TXCAN output pin mode
	 * @param[in] CLKOasSOF If 'true', then SOF signal is on CLKO pin else it's
	 * a clock on CLKO pin
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_ConfigurePins(MCP251XFD           *pComp,
	                                     eMCP251XFD_GPIO0Mode GPIO0PinMode,
	                                     eMCP251XFD_GPIO1Mode GPIO1PinMode,
	                                     eMCP251XFD_OutMode   INTOutMode,
	                                     eMCP251XFD_OutMode   TXCANOutMode,
	                                     bool                 CLKOasSOF)
	{
		eERRORRESULT Error;

		uint8_t Config =
		  MCP251XFD_SFR_IOCON8_GPIO0_INT0 |
		  MCP251XFD_SFR_IOCON8_GPIO1_INT1; // By default, disable all controls
		if ((GPIO0PinMode != MCP251XFD_PIN_AS_INT0_TX) &&
		    (GPIO0PinMode != MCP251XFD_PIN_AS_XSTBY))
			Config |=
			  MCP251XFD_SFR_IOCON8_GPIO0_MODE; // If the pin INT0/GPIO0/XSTBY is
			                                   // in GPIO mode then set GPIO
			                                   // mode
		if ((GPIO1PinMode != MCP251XFD_PIN_AS_INT1_RX))
			Config |=
			  MCP251XFD_SFR_IOCON8_GPIO1_MODE; // If the pin INT1/GPIO1 is in
			                                   // GPIO mode then set GPIO mode
		if (TXCANOutMode == MCP251XFD_PINS_OPENDRAIN_OUT)
			Config |= MCP251XFD_SFR_IOCON8_TXCANOD; // If the pin TXCAN mode is
			                                        // open drain mode then set
			                                        // open drain output
		if (CLKOasSOF)
			Config |=
			  MCP251XFD_SFR_IOCON8_SOF; // If the pin CLKO/SOF is in SOF mode
			                            // then set SOF signal on CLKO pin
		if (INTOutMode == MCP251XFD_PINS_OPENDRAIN_OUT)
			Config |= MCP251XFD_SFR_IOCON8_INTOD; // If all interrupt pins mode
			                                      // are open drain mode then
			                                      // set open drain output
		Error = MCP251XFD_WriteSFR8(
		  pComp,
		  RegMCP251XFD_IOCON_PINMODE,
		  Config); // Write configuration to the IOCON register (last byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_WriteSFR8() then return the error

		Config =
		  MCP251XFD_SFR_IOCON8_XSTBYDIS | MCP251XFD_SFR_IOCON8_GPIO0_OUTPUT |
		  MCP251XFD_SFR_IOCON8_GPIO1_OUTPUT; // By default, disable all controls
		if (GPIO0PinMode == MCP251XFD_PIN_AS_XSTBY)
			Config |=
			  MCP251XFD_SFR_IOCON8_XSTBYEN; // If the pin INT0/GPIO0/XSTBY is in
			                                // XSTBY mode then enable XSTBY mode
		if (GPIO0PinMode == MCP251XFD_PIN_AS_GPIO0_IN)
			Config |=
			  MCP251XFD_SFR_IOCON8_GPIO0_INPUT; // If the pin INT0/GPIO0/XSTBY
			                                    // is in GPIO input mode then
			                                    // set GPIO input mode
		if (GPIO1PinMode == MCP251XFD_PIN_AS_GPIO1_IN)
			Config |=
			  MCP251XFD_SFR_IOCON8_GPIO1_INPUT; // If the pin INT1/GPIO1 is in
			                                    // GPIO input mode then set GPIO
			                                    // input mode
		return MCP251XFD_WriteSFR8(pComp,
		                           RegMCP251XFD_IOCON_DIRECTION,
		                           Config); // Write configuration to the IOCON
		                                    // register (first byte only)
	}

	/*! @brief Set GPIO pins direction of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be configured
	 * @param[in] pinsDirection Set the IO pins direction, if bit is '1' then
	 * the corresponding GPIO is input else it's output
	 * @param[in] pinsChangeMask If the bit is set to '1', then the
	 * corresponding GPIO must be modified
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_SetGPIOPinsDirection(MCP251XFD *pComp,
	                                            uint8_t    pinsDirection,
	                                            uint8_t    pinsChangeMask)
	{
		eERRORRESULT Error;
		uint8_t      Config;

		pinsChangeMask &= 0x3;
		Error = MCP251XFD_ReadSFR8(pComp,
		                           RegMCP251XFD_IOCON_DIRECTION,
		                           &Config); // Read actual configuration of the
		                                     // IOCON register (first byte only)
		if (Error != ERR_OK)
			return Error;          // If there is an error while calling
			                       // MCP251XFD_ReadSFR8() then return the error
		Config &= ~pinsChangeMask; // Force change bits to 0
		Config |= (pinsDirection &
		           pinsChangeMask); // Apply new direction only on changed pins
		return MCP251XFD_WriteSFR8(pComp,
		                           RegMCP251XFD_IOCON_DIRECTION,
		                           Config); // Write new configuration to the
		                                    // IOCON register (first byte only)
	}

	/*! @brief Get GPIO pins input level of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be configured
	 * @param[out] *pinsState Return the actual level of all I/O pins. If bit is
	 * '1' then the corresponding GPIO is level high else it's level low
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_GetGPIOPinsInputLevel(MCP251XFD *pComp,
	                                             uint8_t   *pinsState)
	{
		return MCP251XFD_ReadSFR8(
		  pComp,
		  RegMCP251XFD_IOCON_INLEVEL,
		  pinsState); // Read actual state of the input pins in the IOCON
		              // register (third byte only)
	}

	/*! @brief Set GPIO pins output level of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be configured
	 * @param[in] pinsLevel Set the IO pins output level, if bit is '1' then the
	 * corresponding GPIO is level high else it's level low
	 * @param[in] pinsChangeMask If the bit is set to '1', then the
	 * corresponding GPIO must be modified
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_SetGPIOPinsOutputLevel(MCP251XFD *pComp,
	                                              uint8_t    pinsLevel,
	                                              uint8_t    pinsChangeMask)
	{
#ifdef CHECK_NULL_PARAM
		if (pComp == NULL)
			return ERR__PARAMETER_ERROR;
#endif

		pinsChangeMask &= 0x3;
		pComp->GPIOsOutLevel &= ~pinsChangeMask; // Force change bits to 0
		pComp->GPIOsOutLevel |=
		  (pinsLevel &
		   pinsChangeMask); // Apply new output level only on changed pins
		return MCP251XFD_WriteSFR8(
		  pComp,
		  RegMCP251XFD_IOCON_OUTLEVEL,
		  pComp->GPIOsOutLevel); // Write new configuration to the IOCON
		                         // register (Second byte only)
	}

	//-----------------------------------------------------------------------------

	/*! @brief Calculate Bit Time for CAN2.0 or CAN-FD Configuration for the
	 * MCP251XFD device
	 *
	 * Calculate the best Bit Time configuration following desired bitrates for
	 * CAN-FD This function call automatically the
	 * MCP251XFD_CalculateBitrateStatistics() function
	 * @param[in] fsysclk Is the SYSCLK of the device
	 * @param[in] desiredNominalBitrate Is the desired Nominal Bitrate of the
	 * CAN-FD configuration
	 * @param[in] desiredDataBitrate Is the desired Data Bitrate of the CAN-FD
	 * configuration (If not CAN-FD set it to 0 or MCP251XFD_NO_CANFD)
	 * @param[out] *pConf Is the pointed structure of the Bit Time configuration
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_CalculateBitTimeConfiguration(
	  const uint32_t           fsysclk,
	  const uint32_t           desiredNominalBitrate,
	  const uint32_t           desiredDataBitrate,
	  MCP251XFD_BitTimeConfig *pConf)
	{
#ifdef CHECK_NULL_PARAM
		if (pConf == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		//--- Check values ----------------------------------------
		if (fsysclk < MCP251XFD_SYSCLK_MIN)
			return ERR__PARAMETER_ERROR;
		if (fsysclk > MCP251XFD_SYSCLK_MAX)
			return ERR__PARAMETER_ERROR;
		if (desiredNominalBitrate < MCP251XFD_NOMBITRATE_MIN)
			return ERR__BAUDRATE_ERROR;
		if (desiredNominalBitrate > MCP251XFD_NOMBITRATE_MAX)
			return ERR__BAUDRATE_ERROR;
		if (desiredDataBitrate != MCP251XFD_NO_CANFD)
			if (desiredDataBitrate < MCP251XFD_DATABITRATE_MIN)
				return ERR__BAUDRATE_ERROR;
		if (desiredDataBitrate > MCP251XFD_DATABITRATE_MAX)
			return ERR__BAUDRATE_ERROR;

		//--- Declaration -----------------------------------------
		uint32_t ErrorTQ, ErrorNTQ, ErrorDTQ, DTQbits = 0;
		uint32_t BestBRP     = MCP251XFD_NBRP_MAX,
		         BestNTQbits = MCP251XFD_NTQBIT_MAX,
		         BestDTQbits = MCP251XFD_DTQBIT_MAX;

		//--- Calculate Nominal & Data Bit Time parameter ---------
		uint32_t MinErrorBR = UINT32_MAX;
		uint32_t BRP =
		  MCP251XFD_NBRP_MAX; // Select the worst BRP value. Here all value from
		                      // max to min will be tested to get the best tuple
		                      // of NBRP and DBRP, identical TQ in both phases
		                      // prevents quantization errors during bit rate
		                      // switching
		while (--BRP >= MCP251XFD_NBRP_MIN)
		{
			uint32_t NTQbits = fsysclk / desiredNominalBitrate /
			                   BRP; // Calculate the NTQbits according to BRP
			                        // and the desired Nominal Bitrate
			if ((NTQbits < MCP251XFD_NTQBIT_MIN) ||
			    (NTQbits > MCP251XFD_NTQBIT_MAX))
				continue; // This TQbits count is not possible with this BRP,
				          // then do the next BRP value
			if (desiredDataBitrate != MCP251XFD_NO_CANFD)
			{
				DTQbits = fsysclk / desiredDataBitrate /
				          BRP; // Calculate the DTQbits according to BRP and the
				               // desired Data Bitrate
				if ((DTQbits < MCP251XFD_DTQBIT_MIN) ||
				    (DTQbits > MCP251XFD_DTQBIT_MAX))
					continue; // This TQbits count is not possible with this
					          // BRP, then do the next BRP value
			}

			// NTQ & DTQ bits count
			ErrorNTQ = (fsysclk - (desiredNominalBitrate * NTQbits *
			                       BRP)); // Calculate NTQ error
			if (desiredDataBitrate != MCP251XFD_NO_CANFD)
			{
				if (ErrorNTQ == 0)
					ErrorNTQ = 1; // Adjust NTQ error
				ErrorDTQ = (fsysclk - (desiredDataBitrate * DTQbits * BRP));
				if (ErrorDTQ == 0)
					ErrorDTQ = 1; // Calculate DTQ error
				ErrorTQ = (ErrorNTQ * ErrorDTQ);
			}
			else
				ErrorTQ = ErrorNTQ;
			if (ErrorTQ <= MinErrorBR) // If better error then
			{
				MinErrorBR  = ErrorTQ;
				BestBRP     = BRP;
				BestNTQbits = NTQbits;
				BestDTQbits = DTQbits;
			} // Save best parameters

			// NTQ+1 & DTQ bits count
			if (NTQbits < MCP251XFD_NTQBIT_MAX)
			{
				ErrorNTQ = ((desiredNominalBitrate * (NTQbits + 1) * BRP) -
				            fsysclk); // Calculate NTQ error with NTQbits+1
				if (desiredDataBitrate != MCP251XFD_NO_CANFD)
				{
					if (ErrorNTQ == 0)
						ErrorNTQ = 1; // Adjust NTQ error
					ErrorDTQ = (fsysclk - (desiredDataBitrate * DTQbits * BRP));
					if (ErrorDTQ == 0)
						ErrorDTQ = 1; // Calculate DTQ error
					ErrorTQ = (ErrorNTQ * ErrorDTQ);
				}
				else
					ErrorTQ = ErrorNTQ;
				if (ErrorTQ <= MinErrorBR) // If better error then
				{
					MinErrorBR  = ErrorTQ;
					BestBRP     = BRP;
					BestNTQbits = NTQbits + 1;
					BestDTQbits = DTQbits;
				} // Save best parameters
			}

			// NTQ+1 & DTQ or DTQ+1 bits count
			if (desiredDataBitrate != MCP251XFD_NO_CANFD)
			{
				if (DTQbits < MCP251XFD_DTQBIT_MAX)
				{
					ErrorNTQ =
					  (fsysclk - (desiredNominalBitrate * NTQbits * BRP));
					if (ErrorNTQ == 0)
						ErrorNTQ = 1; // Calculate NTQ error
					ErrorDTQ =
					  ((desiredDataBitrate * (DTQbits + 1) * BRP) - fsysclk);
					if (ErrorDTQ == 0)
						ErrorDTQ = 1; // Calculate DTQ error with DTQbits+1
					ErrorTQ = (ErrorNTQ * ErrorDTQ);
					if (ErrorTQ <= MinErrorBR) // If better error then
					{
						MinErrorBR  = ErrorTQ;
						BestBRP     = BRP;
						BestNTQbits = NTQbits;
						BestDTQbits = DTQbits + 1;
					} // Save best parameters
				}
				if ((NTQbits < MCP251XFD_NTQBIT_MAX) &&
				    (DTQbits < MCP251XFD_DTQBIT_MAX))
				{
					ErrorNTQ =
					  ((desiredNominalBitrate * (NTQbits + 1) * BRP) - fsysclk);
					if (ErrorNTQ == 0)
						ErrorNTQ = 1; // Calculate NTQ error with NTQbits+1
					ErrorDTQ =
					  ((desiredDataBitrate * (DTQbits + 1) * BRP) - fsysclk);
					if (ErrorDTQ == 0)
						ErrorDTQ = 1; // Calculate DTQ error with DTQbits+1
					ErrorTQ = (ErrorNTQ * ErrorDTQ);
					if (ErrorTQ <= MinErrorBR) // If better error then
					{
						MinErrorBR  = ErrorTQ;
						BestBRP     = BRP;
						BestNTQbits = NTQbits + 1;
						BestDTQbits = DTQbits + 1;
					} // Save best parameters
				}
			}
		}
		if (MinErrorBR == UINT32_MAX)
			return ERR__BITTIME_ERROR; // Impossible to find a good BRP

		//--- Calculate Nominal segments --------------------------
		pConf->NBRP =
		  BestBRP - 1; // ** Save the best NBRP in the configuration **
		uint32_t NTSEG2 =
		  BestNTQbits /
		  5; // The Nominal Sample Point must be close to 80% (5x20%) of NTQ per
		     // bits so NTSEG2 should be 20% of NTQbits
		if ((BestNTQbits % 5) > 2)
			NTSEG2++; // To be as close as possible to 80%
		if (NTSEG2 < MCP251XFD_NTSEG2_MIN)
			NTSEG2 = MCP251XFD_NTSEG2_MIN; // Correct NTSEG2 if < 1
		if (NTSEG2 > MCP251XFD_NTSEG2_MAX)
			NTSEG2 = MCP251XFD_NTSEG2_MAX; // Correct NTSEG2 if > 128
		pConf->NTSEG2 =
		  NTSEG2 - 1; // ** Save the NTSEG2 in the configuration **
		uint32_t NTSEG1 =
		  BestNTQbits - NTSEG2 -
		  MCP251XFD_NSYNC; // NTSEG1  = NTQbits - NTSEG2 - 1 (NSYNC)
		if (NTSEG1 < MCP251XFD_NTSEG1_MIN)
			NTSEG1 = MCP251XFD_NTSEG1_MIN; // Correct NTSEG1 if < 1
		if (NTSEG1 > MCP251XFD_NTSEG1_MAX)
			NTSEG1 = MCP251XFD_NTSEG1_MAX; // Correct NTSEG1 if > 256
		pConf->NTSEG1 =
		  NTSEG1 - 1; // ** Save the NTSEG1 in the configuration **
		uint32_t NSJW =
		  NTSEG2; // Normally NSJW = NTSEG2, maximizing NSJW lessens the
		          // requirement for the oscillator tolerance
		if (NTSEG1 < NTSEG2)
			NSJW = NTSEG1; // But NSJW = min(NPHSEG1, NPHSEG2)
		if (NSJW < MCP251XFD_NSJW_MIN)
			NSJW = MCP251XFD_NSJW_MIN; // Correct NSJW if < 1
		if (NSJW > MCP251XFD_NSJW_MAX)
			NSJW = MCP251XFD_NSJW_MAX; // Correct NSJW if > 128
		pConf->NSJW = NSJW - 1; // ** Save the NSJW in the configuration **

		//--- Calculate Data segments -----------------------------
		if (desiredDataBitrate != MCP251XFD_NO_CANFD)
		{
			pConf->DBRP =
			  BestBRP - 1; // ** Save the best DBRP in the configuration **
			uint32_t DTSEG2 =
			  BestDTQbits /
			  5; // The Data Sample Point must be close to 80% (5x20%) of DTQ
			     // per bits so DTSEG2 should be 20% of DTQbits
			if ((BestDTQbits % 5) > 2)
				DTSEG2++; // To be as close as possible to 80%
			if (DTSEG2 < MCP251XFD_NTSEG2_MIN)
				DTSEG2 = MCP251XFD_NTSEG2_MIN; // Correct DTSEG2 if < 1
			if (DTSEG2 > MCP251XFD_NTSEG2_MAX)
				DTSEG2 = MCP251XFD_NTSEG2_MAX; // Correct DTSEG2 if > 16
			pConf->DTSEG2 =
			  DTSEG2 - 1; // ** Save the DTSEG2 in the configuration **
			uint32_t DTSEG1 =
			  BestDTQbits - DTSEG2 -
			  MCP251XFD_DSYNC; // DTSEG1  = DTQbits - DTSEG2 - 1 (DSYNC)
			if (DTSEG1 < MCP251XFD_NTSEG1_MIN)
				DTSEG1 = MCP251XFD_NTSEG1_MIN; // Correct DTSEG1 if < 1
			if (DTSEG1 > MCP251XFD_NTSEG1_MAX)
				DTSEG1 = MCP251XFD_NTSEG1_MAX; // Correct DTSEG1 if > 32
			pConf->DTSEG1 =
			  DTSEG1 - 1; // ** Save the DTSEG1 in the configuration **
			uint32_t DSJW =
			  DTSEG2; // Normally DSJW = DTSEG2, maximizing DSJW lessens the
			          // requirement for the oscillator tolerance
			if (DTSEG1 < DTSEG2)
				DSJW = DTSEG1; // But DSJW = min(DPHSEG1, DPHSEG2)
			if (DSJW < MCP251XFD_DSJW_MIN)
				DSJW = MCP251XFD_DSJW_MIN; // Correct DSJW if < 1
			if (DSJW > MCP251XFD_DSJW_MAX)
				DSJW = MCP251XFD_DSJW_MAX; // Correct DSJW if > 128
			pConf->DSJW = DSJW - 1; // ** Save the DSJW in the configuration **

			//--- Calculate Transmitter Delay Compensation ----------
			if (desiredDataBitrate >=
			    1000000) // Enable Automatic TDC for DBR of 1Mbps and Higher
				pConf->TDCMOD =
				  MCP251XFD_AUTO_MODE; // ** Set Automatic TDC measurement
				                       // compensations for transmitter delay
				                       // variations
			else
				pConf->TDCMOD =
				  MCP251XFD_MANUAL_MODE; // ** Set Manual; Don�t measure, use
				                         // TDCV + TDCO from register
			const uint32_t SSP =
			  BestBRP *
			  DTSEG1; // In order to set the SSP to 80%, SSP = TDCO + TDCV
			          // (Equation 3-10 of MCP25XXFD Family Reference Manual).
			          // SSP is set to DBRP * (DPRSEG + DPHSEG1) = DBRP * DTSEG1
			uint32_t TDCO = SSP;
			if (TDCO > MCP251XFD_TDCO_MAX)
				TDCO = MCP251XFD_TDCO_MAX; // Correct TDCO if > 63
			pConf->TDCO = TDCO; // ** Save the TDCO in the configuration **
			uint32_t TDCV =
			  SSP -
			  TDCO; // TDCV is the remaining of SSP: TDCV = SSP - TDCO (Equation
			        // 3-10 of MCP25XXFD Family Reference Manual)
			if (TDCV > MCP251XFD_TDCV_MAX)
				TDCV = MCP251XFD_TDCV_MAX; // Correct TDCV if > 63
			pConf->TDCV = TDCV; // ** Save the TDCV in the configuration **
			pConf->EDGE_FILTER = true; // ** Edge Filtering enabled, according
			                           // to ISO 11898-1:2015 **
		}
		else
		{
			pConf->DBRP   = 0x0;  // ** Set the DBRP in the configuration **
			pConf->DTSEG2 = 0x3;  // ** Set the DTSEG2 in the configuration **
			pConf->DTSEG1 = 0x0E; // ** Set the DTSEG1 in the configuration **
			pConf->DSJW   = 0x3;  // ** Set the DSJW in the configuration **
			pConf->TDCMOD =
			  MCP251XFD_AUTO_MODE; // ** Set Automatic TDC measurement
			                       // compensations for transmitter delay
			                       // variations
			pConf->TDCO = 0x10;    // ** Set the TDCO in the configuration **
			pConf->TDCV = 0x00;    // ** Set the TDCV in the configuration **
		}

		eERRORRESULT Error = ERR_OK;
		if (pConf->Stats != NULL)
			Error = MCP251XFD_CalculateBitrateStatistics(
			  fsysclk,
			  pConf,
			  desiredDataBitrate ==
			    MCP251XFD_NO_CANFD); // If statistics are necessary, then
			                         // calculate them
		return Error;                // If there is an error while calling
		              // MCP251XFD_CalculateBitrateStatistics() then return the
		              // error
	}

	/*! @brief Calculate Bitrate Statistics of a Bit Time configuration
	 *
	 * Calculate bus length, sample points, bitrates and oscillator tolerance
	 * following BitTime Configuration
	 * @param[in] fsysclk Is the SYSCLK of the device
	 * @param[in,out] *pConf Is the pointed structure of the Bit Time
	 * configuration
	 * @param[in] can20only Indicate that parameters for Data Bitrate are not
	 * calculated if true
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_CalculateBitrateStatistics(const uint32_t           fsysclk,
	                                     MCP251XFD_BitTimeConfig *pConf,
	                                     bool                     can20only)
	{
#ifdef CHECK_NULL_PARAM
		if (pConf == NULL)
			return ERR__PARAMETER_ERROR;
		if (pConf->Stats == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		//--- Check values ----------------------------------------
		if (fsysclk < MCP251XFD_SYSCLK_MIN)
			return ERR__PARAMETER_ERROR;
		if (fsysclk > MCP251XFD_SYSCLK_MAX)
			return ERR__PARAMETER_ERROR;

		//--- Declaration -----------------------------------------
		uint32_t DTQbits = 0;

		//--- Calculate bus length & Nominal Sample Point ---------
		const uint32_t NTQ =
		  (((pConf->NBRP + 1) * 1000000) /
		   (fsysclk / 1000)); // Nominal Time Quanta = 1/FSYSCLK multiply by
		                      // 1000000000 to get ns (Equation 3-3 of MCP25XXFD
		                      // Family Reference Manual)
		const uint32_t NPRSEG =
		  (pConf->NTSEG1 + 1) -
		  (pConf->NTSEG2 + 1); // Here PHSEG2 (NTSEG2) should be equal to PHSEG1
		                       // so NPRSEG = NTSEG1 - NTSEG2 (Figure 3-2 of
		                       // MCP25XXFD Family Reference Manual)
		pConf->Stats->MaxBusLength =
		  (uint32_t)(((NTQ * NPRSEG) - (2 * MCP251XFD_tTXDtRXD_MAX)) /
		             (2 *
		              MCP251XFD_tBUS_CONV)); // Formula is (2x(tTXD�RXD +
		                                     // (5*BusLen))/NTQ = NPRSEG =>
		                                     // BusLen =
		                                     // ((NTQ*NPRESG)-(2*tTXD))/(2*5) in
		                                     // meter (Equation 3-9 of MCP25XXFD
		                                     // Family Reference Manual)
		const uint32_t NTQbits =
		  (MCP251XFD_NSYNC + (pConf->NTSEG1 + 1) +
		   (pConf->NTSEG2 +
		    1)); // NTQ per bits = NSYNC + NTSEG1 + NTSEG2 (Equation 3-5 of
		         // MCP25XXFD Family Reference Manual)
		uint32_t SamplePoint = ((MCP251XFD_NSYNC + (pConf->NTSEG1 + 1)) * 100) /
		                       NTQbits; // Calculate actual nominal sample point
		pConf->Stats->NSamplePoint =
		  (uint32_t)(SamplePoint *
		             100); // ** Save actual Nominal sample point with 2 digits
		                   // after the decimal point (divide by 100 to get
		                   // percentage)
		pConf->Stats->NominalBitrate =
		  (fsysclk / (pConf->NBRP + 1) /
		   NTQbits); // ** Save actual Nominal Bitrate

		//--- Calculate Data Sample Point -------------------------
		if (can20only == false)
		{
			DTQbits = (MCP251XFD_DSYNC + (pConf->DTSEG1 + 1) +
			           (pConf->DTSEG2 +
			            1)); // DTQ per bits = DSYNC + DTSEG1 + DTSEG2 (Equation
			                 // 3-6 of MCP25XXFD Family Reference Manual)
			SamplePoint = ((MCP251XFD_DSYNC + (pConf->DTSEG1 + 1)) * 100) /
			              DTQbits; // Calculate actual data sample point
			// pConf->Stats->DSamplePoint = (uint32_t)(SamplePoint * 100.0f); //
			// ** Save actual Data sample point with 2 digits after the decimal
			// point (divide by 100 to get percentage)
			//  We don't have floats yet!
			pConf->Stats->DSamplePoint =
			  (uint32_t)(SamplePoint *
			             100); // ** Save actual Data sample point with 2 digits
			                   // after the decimal point (divide by 100 to get
			                   // percentage)
			pConf->Stats->DataBitrate =
			  (fsysclk / (pConf->DBRP + 1) /
			   DTQbits); // ** Save actual Data Bitrate
		}
		else
		{
			pConf->Stats->DSamplePoint = 0; // ** Set actual Data sample point
			pConf->Stats->DataBitrate  = 0; // ** Set actual Data Bitrate
		}

		//--- Calculate oscillator tolerance ----------------------
		const uint32_t NPHSEG1 = (pConf->NTSEG1 + 1) - NPRSEG; // Get NPHSEG1
		const uint32_t MinNPHSEG =
		  (NPHSEG1 <= (pConf->NTSEG2 + 1)
		     ? NPHSEG1
		     : (pConf->NTSEG2 + 1)); // Get min(NPHSEG1, NPHSEG2)
		pConf->Stats->OscTolC1 =
		  (((pConf->NSJW + 1) * 10000) /
		   (2 * 10 *
		    NTQbits)); // Condition 1 for the maximum tolerance of the
		               // oscillator with 2 digits after the decimal point
		               // (Equation 3-12 of MCP25XXFD Family Reference Manual)
		pConf->Stats->OscTolerance = pConf->Stats->OscTolC1;
		pConf->Stats->OscTolC2 =
		  ((MinNPHSEG * 10000) /
		   (2 * (13 * NTQbits -
		         (pConf->NTSEG2 +
		          1)))); // Condition 2 for the maximum tolerance of the
		                 // oscillator with 2 digits after the decimal point
		                 // (Equation 3-13 of MCP25XXFD Family Reference Manual)
		pConf->Stats->OscTolerance =
		  (pConf->Stats->OscTolC2 < pConf->Stats->OscTolerance
		     ? pConf->Stats->OscTolC2
		     : pConf->Stats
		         ->OscTolerance); // Oscillator Tolerance, minimum of conditions
		                          // 1-5 (Equation 3-11 of MCP25XXFD Family
		                          // Reference Manual)
		if (can20only)
		{
			pConf->Stats->OscTolC3 = 0;
			pConf->Stats->OscTolC4 = 0;
			pConf->Stats->OscTolC5 = 0;
		}
		else
		{
			pConf->Stats->OscTolC3 =
			  (((pConf->DSJW + 1) * 10000) /
			   (2 * 10 * DTQbits)); // Condition 3 for the maximum tolerance of
			                        // the oscillator with 2 digits after the
			                        // decimal point (Equation 3-14 of MCP25XXFD
			                        // Family Reference Manual)
			pConf->Stats->OscTolerance =
			  (pConf->Stats->OscTolC3 < pConf->Stats->OscTolerance
			     ? pConf->Stats->OscTolC3
			     : pConf->Stats
			         ->OscTolerance); // Oscillator Tolerance, minimum of
			                          // conditions 1-5 (Equation 3-11 of
			                          // MCP25XXFD Family Reference Manual)
			const uint32_t NBRP = (pConf->NBRP + 1), DBRP = (pConf->DBRP + 1);
			pConf->Stats->OscTolC4 =
			  ((MinNPHSEG * 10000) /
			   (2 * ((((6 * DTQbits - (pConf->DTSEG2 + 1)) * DBRP) / NBRP) +
			         (7 * NTQbits)))); // Condition 4 for the maximum tolerance
			                           // of the oscillator with 2 digits after
			                           // the decimal point (Equation 3-15 of
			                           // MCP25XXFD Family Reference Manual)
			pConf->Stats->OscTolerance =
			  (pConf->Stats->OscTolC4 < pConf->Stats->OscTolerance
			     ? pConf->Stats->OscTolC4
			     : pConf->Stats
			         ->OscTolerance); // Oscillator Tolerance, minimum of
			                          // conditions 1-5 (Equation 3-11 of
			                          // MCP25XXFD Family Reference Manual)
			const int32_t
			  NBRP_DBRP = ((NBRP * 10000) / DBRP),
			  MaxBRP    = ((NBRP_DBRP - 10000) > 0
			                 ? (NBRP_DBRP - 10000)
			                 : 0); // NBRP/DBRP and max(0,(NBRP/DBRP-1)). The use
			                    // of 10000 is to set 2 digits on the C5 result
			pConf->Stats->OscTolC5 =
			  ((((pConf->DSJW + 1) * 10000) - MaxBRP) /
			   (2 *
			    (((2 * NTQbits - (pConf->NTSEG2 + 1)) * NBRP) / DBRP +
			     (pConf->DTSEG2 + 1) +
			     4 * DTQbits))); // Condition 5 for the maximum tolerance of the
			                     // oscillator with 2 digits after the decimal
			                     // point (Equation 3-16 of MCP25XXFD Family
			                     // Reference Manual) [WARNING: An error seems
			                     // to be present in the original formula]
			pConf->Stats->OscTolerance =
			  (pConf->Stats->OscTolC5 < pConf->Stats->OscTolerance
			     ? pConf->Stats->OscTolC5
			     : pConf->Stats
			         ->OscTolerance); // Oscillator Tolerance, minimum of
			                          // conditions 1-5 (Equation 3-11 of
			                          // MCP25XXFD Family Reference Manual)
		}
		return ERR_NONE;
	}

	/*! @brief Set Bit Time Configuration to the MCP251XFD device
	 *
	 * Set the Nominal and Data Bit Time to registers
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] *pConf Is the pointed structure of the Bit Time configuration
	 * @param[in] can20only Indicate that parameters for Data Bitrate are not
	 * set to registers (CiDBTCFG and CiTDC)
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_SetBitTimeConfiguration(MCP251XFD               *pComp,
	                                  MCP251XFD_BitTimeConfig *pConf,
	                                  bool                     can20only)
	{
#ifdef CHECK_NULL_PARAM
		if (pConf == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		eERRORRESULT Error;

		//--- Write Nominal Bit Time configuration ----------------
		MCP251XFD_CiNBTCFG_Register NConfig;
		NConfig.CiNBTCFG =
		  MCP251XFD_CAN_CiNBTCFG_BRP_SET(pConf->NBRP) |
		  MCP251XFD_CAN_CiNBTCFG_TSEG1_SET(
		    pConf->NTSEG1) // Set Nominal Bit Time configuration
		  | MCP251XFD_CAN_CiNBTCFG_TSEG2_SET(pConf->NTSEG2) |
		  MCP251XFD_CAN_CiNBTCFG_SJW_SET(pConf->NSJW);
		Error = MCP251XFD_WriteData(
		  pComp,
		  RegMCP251XFD_CiNBTCFG,
		  &NConfig.Bytes[0],
		  sizeof(MCP251XFD_CiNBTCFG_Register)); // Write configuration to the
		                                        // CiNBTCFG register
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_WriteData() then return the error

		if (!can20only)
		{
			//--- Write Data Bit Time configuration -----------------
			MCP251XFD_CiDBTCFG_Register DConfig;
			DConfig.CiDBTCFG =
			  MCP251XFD_CAN_CiDBTCFG_BRP_SET(pConf->DBRP) |
			  MCP251XFD_CAN_CiDBTCFG_TSEG1_SET(
			    pConf->DTSEG1) // Set Data Bit Time configuration
			  | MCP251XFD_CAN_CiDBTCFG_TSEG2_SET(pConf->DTSEG2) |
			  MCP251XFD_CAN_CiDBTCFG_SJW_SET(pConf->DSJW);
			Error = MCP251XFD_WriteData(
			  pComp,
			  RegMCP251XFD_CiDBTCFG,
			  &DConfig.Bytes[0],
			  sizeof(MCP251XFD_CiDBTCFG_Register)); // Write configuration to
			                                        // the CiDBTCFG register
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_WriteData() then return the error

			//--- Write Data Bit Time configuration -----------------
			MCP251XFD_CiTDC_Register TConfig;
			TConfig.CiTDC = MCP251XFD_CAN_CiTDC_TDCO_SET(pConf->TDCO) |
			                MCP251XFD_CAN_CiTDC_TDCV_SET(
			                  pConf->TDCV) // Set Data Bit Time configuration
			                | MCP251XFD_CAN_CiTDC_TDCMOD_SET(pConf->TDCMOD) |
			                MCP251XFD_CAN_CiTDC_EDGFLTDIS;
			if (pConf->EDGE_FILTER)
				TConfig.CiTDC |=
				  MCP251XFD_CAN_CiTDC_EDGFLTEN; // Enable Edge Filter if asked
			Error = MCP251XFD_WriteData(
			  pComp,
			  RegMCP251XFD_CiTDC,
			  &TConfig.Bytes[0],
			  sizeof(MCP251XFD_CiTDC_Register)); // Write configuration to the
			                                     // CiDBTCFG register
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_WriteData()
			pComp->InternalConfig |=
			  MCP251XFD_CANFD_ENABLED; // CAN-FD is enable if Data Bitrate is
			                           // set
		}
		else
			pComp->InternalConfig &= ~MCP251XFD_CANFD_ENABLED; // Set no CAN-FD

		return ERR_NONE;
	}

	//********************************************************************************************************************

	/*! @brief Abort all pending transmissions of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_AbortAllTransmissions(MCP251XFD *pComp)
	{
		eERRORRESULT Error;
		uint8_t      Config;

		Error = MCP251XFD_ReadSFR8(pComp,
		                           RegMCP251XFD_CiCON + 3,
		                           &Config); // Read actual configuration of the
		                                     // CiCON register (Last byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		Config |= MCP251XFD_CAN_CiCON8_ABAT; // Add ABAT flag
		return MCP251XFD_WriteSFR8(
		  pComp,
		  RegMCP251XFD_CiCON + 3,
		  Config); // Write the new configuration to the CiCON register (Last
		           // byte only)
	}

	/*! @brief Get actual operation mode of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *actualMode Is where the result of the actual mode will be
	 * saved
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_GetActualOperationMode(MCP251XFD                *pComp,
	                                 eMCP251XFD_OperationMode *actualMode)
	{
		eERRORRESULT Error;
		uint8_t      Config;

		Error = MCP251XFD_ReadSFR8(pComp,
		                           RegMCP251XFD_CiCON + 2,
		                           &Config); // Read actual configuration of the
		                                     // CiCON register (Third byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		*actualMode = MCP251XFD_CAN_CiCON8_OPMOD_GET(Config); // Get actual mode
		return ERR_NONE;
	}

	/*! @brief Request operation mode change of the MCP251XFD device
	 *
	 * In case of wait operation change, the function calls
	 * MCP251XFD_WaitOperationModeChange()
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] newMode Is the new operational mode to set
	 * @param[in] waitOperationChange Set to 'true' if the function must wait
	 * for the actual operation mode change (wait up to 7ms)
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_RequestOperationMode(MCP251XFD               *pComp,
	                               eMCP251XFD_OperationMode newMode,
	                               bool                     waitOperationChange)
	{
#ifdef CHECK_NULL_PARAM
		if (pComp == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		eERRORRESULT Error;
		uint8_t      Config;

		if (((pComp->InternalConfig & MCP251XFD_CANFD_ENABLED) == 0) &&
		    (newMode == MCP251XFD_NORMAL_CANFD_MODE))
			return ERR__CONFIGURATION; // Can't change to CAN-FD mode if the
			                           // bitrate is not configured for
		Error = MCP251XFD_ReadSFR8(pComp,
		                           RegMCP251XFD_CiCON + 3,
		                           &Config); // Read actual configuration of the
		                                     // CiCON register (Last byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		Config &= ~MCP251XFD_CAN_CiCON8_REQOP_Mask; // Clear request mode bits
		Config |= MCP251XFD_CAN_CiCON8_REQOP_SET(
		  newMode);                          // Set request operation mode bits
		Config |= MCP251XFD_CAN_CiCON8_ABAT; // Need to stop all transmissions
		                                     // before changing configuration
		Error =
		  MCP251XFD_WriteSFR8(pComp,
		                      RegMCP251XFD_CiCON + 3,
		                      Config); // Write the new configuration to the
		                               // CiCON register (Last byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_WriteSFR8() then return the error

		if (waitOperationChange)
		{
			Error = MCP251XFD_WaitOperationModeChange(
			  pComp, newMode); // Wait for operation mode change
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_WaitOperationModeChange() then return
				              // the error
			Error = MCP251XFD_ClearInterruptEvents(
			  pComp,
			  MCP251XFD_INT_OPERATION_MODE_CHANGE_EVENT); // Automatically clear
			                                              // the Operation Mode
			                                              // Change Flag
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ClearInterruptEvents() then return
				              // the error
		}
		pComp->InternalConfig &= ~MCP251XFD_DEV_PS_Mask;
		pComp->InternalConfig |= MCP251XFD_DEV_PS_SET(
		  MCP251XFD_DEVICE_NORMAL_POWER_STATE); // Set normal power state even
		                                        // if the operation mode is
		                                        // sleep, this value will be
		                                        // changed to the good value by
		                                        // the function
		                                        // MCP251XFD_EnterSleepMode()
		return ERR_NONE;
	}

	/*! @brief Wait for operation mode change of the MCP251XFD device
	 *
	 * The function can wait up to 7ms. After this time, if the device doesn't
	 * change its operation mode, the function returns an ERR_DEVICETIMEOUT
	 * error
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] askedMode Is the mode asked after a call of
	 * MCP251XFD_RequestOperationMode()
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_WaitOperationModeChange(MCP251XFD               *pComp,
	                                  eMCP251XFD_OperationMode askedMode)
	{
#ifdef CHECK_NULL_PARAM
		if (pComp == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		eERRORRESULT Error;
		uint8_t      Config;

		uint32_t StartTime = pComp->fnGetCurrentms(); // Start the timeout
		while (true)
		{
			Error = MCP251XFD_ReadSFR8(
			  pComp,
			  RegMCP251XFD_CiCON + 2,
			  &Config); // Read current configuration mode with the current
			            // driver configuration
			if (Error != ERR_NONE)
				return Error; // If there is an error while reading the register
				              // then return the error
			if (MCP251XFD_CAN_CiCON8_OPMOD_GET(Config) == askedMode)
				break; // Check if the controller is in configuration mode
			if (MCP251XFD_TIME_DIFF(StartTime, pComp->fnGetCurrentms()) >
			    7) // Wait at least 7ms because the longest message is 731 bit
			       // long and the minimum bitrate is 125kbit/s that mean 5,8ms
			       // + 2x 6bytes @ 1Mbit/s over SPI that mean 96µs = ~6ms + 1ms
			       // because GetCurrentms can be 1 cycle before the new ms
				return ERR__DEVICE_TIMEOUT; // Timeout? return the error
		}
		return ERR_NONE;
	}

	/*! @brief Start the MCP251XFD device in CAN2.0 mode
	 *
	 * This function asks for a mode change to CAN2.0 but do not wait for its
	 * actual change because normally the device is in configuration mode and
	 * the change to CAN2.0 mode will be instantaneous
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_StartCAN20(MCP251XFD *pComp)
	{
		return MCP251XFD_RequestOperationMode(
		  pComp, MCP251XFD_NORMAL_CAN20_MODE, false);
	}

	/*! @brief Start the MCP251XFD device in CAN-FD mode
	 *
	 * This function asks for a mode change to CAN-FD but do not wait for its
	 * actual change because normally the device is in configuration mode and
	 * the change to CAN-FD mode will be instantaneous
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_StartCANFD(MCP251XFD *pComp)
	{
		return MCP251XFD_RequestOperationMode(
		  pComp, MCP251XFD_NORMAL_CANFD_MODE, false);
	}

	/*! @brief Start the MCP251XFD device in CAN Listen-Only mode
	 *
	 * This function asks for a mode change to CAN Listen-Only but do not wait
	 * for its actual change because normally the device is in configuration
	 * mode and the change to CAN Listen-Only mode will be instantaneous
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_StartCANListenOnly(MCP251XFD *pComp)
	{
		return MCP251XFD_RequestOperationMode(
		  pComp, MCP251XFD_LISTEN_ONLY_MODE, false);
	}

	/*! @brief Configure CAN Controller of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] flags Is all the flags for the configuration
	 * @param[in] bandwidth Is the Delay between two consecutive transmissions
	 * (in arbitration bit times)
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_ConfigureCANController(MCP251XFD                *pComp,
	                                 setMCP251XFD_CANCtrlFlags flags,
	                                 eMCP251XFD_Bandwidth      bandwidth)
	{
		eERRORRESULT Error;
		uint32_t     Config;

		Error =
		  MCP251XFD_ReadSFR32(pComp,
		                      RegMCP251XFD_CiCON,
		                      &Config); // Read actual configuration of the
		                                // CiTDC register (Last byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR32() then return the error
		if (MCP251XFD_CAN_CiCON_OPMOD_GET(Config) !=
		    MCP251XFD_CONFIGURATION_MODE)
			return ERR__NEED_CONFIG_MODE; // Device must be in Configuration
			                              // Mode to perform the configuration

		Config &=
		  ~(MCP251XFD_CAN_CiCON_TXBWS_Mask | MCP251XFD_CAN_CiCON_REQOP_Mask |
		    MCP251XFD_CAN_CiCON_ABAT // Clear by default all flags that can be
		                             // changed
		    | MCP251XFD_CAN_CiCON_OPMOD_Mask | MCP251XFD_CAN_CiCON_SERR2LOM |
		    MCP251XFD_CAN_CiCON_ESIGM | MCP251XFD_CAN_CiCON_RTXAT |
		    MCP251XFD_CAN_CiCON_BRSDIS | MCP251XFD_CAN_CiCON_PXEDIS |
		    MCP251XFD_CAN_CiCON_ISOCRCEN);
		Config |= MCP251XFD_CAN_CiCON_REQOP_SET(
		  MCP251XFD_CONFIGURATION_MODE); // Stay in configuration mode because
		                                 // these change are done in
		                                 // configuration mode only
		Config |= MCP251XFD_CAN_CiCON_TXBWS_SET(
		  bandwidth); // Set the delay between two consecutive transmissions (in
		              // arbitration bit times)
		if ((flags & MCP251XFD_CAN_LISTEN_ONLY_MODE_ON_ERROR) > 0)
			Config |= MCP251XFD_CAN_CiCON_SERR2LOM; // Set transition to Listen
			                                        // Only Mode on system error
		if ((flags & MCP251XFD_CAN_GATEWAY_MODE_ESI_RECESSIVE) > 0)
			Config |=
			  MCP251XFD_CAN_CiCON_ESIGM; // Set transmit ESI in Gateway Mode,
			                             // ESI is transmitted recessive when
			                             // ESI of message is high or CAN
			                             // controller error passive
		if ((flags & MCP251XFD_CAN_RESTRICTED_RETRANS_ATTEMPTS) > 0)
			Config |=
			  MCP251XFD_CAN_CiCON_RTXAT; // Set restricted retransmission
			                             // attempts, CiFIFOCONm.TXAT is used
		if ((flags & MCP251XFD_CANFD_BITRATE_SWITCHING_DISABLE) > 0)
			Config |=
			  MCP251XFD_CAN_CiCON_BRSDIS; // Set Bit Rate Switching is Disabled,
			                              // regardless of BRS in the Transmit
			                              // Message Object
		if ((flags & MCP251XFD_CAN_PROTOCOL_EXCEPT_AS_FORM_ERROR) > 0)
			Config |=
			  MCP251XFD_CAN_CiCON_PXEDIS; // Set Protocol Exception is treated
			                              // as a Form Error. A recessive "res
			                              // bit" following a recessive FDF bit
			                              // is called a Protocol Exception
		if ((flags & MCP251XFD_CANFD_USE_ISO_CRC) > 0)
			Config |=
			  MCP251XFD_CAN_CiCON_ISOCRCEN; // Set Include Stuff Bit Count in
			                                // CRC Field and use Non-Zero CRC
			                                // Initialization Vector according
			                                // to ISO 11898-1:2015
		Error = MCP251XFD_WriteSFR32(
		  pComp, RegMCP251XFD_CiCON, Config); // Write new configuration to the
		                                      // CiTDC register (Last byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_WriteSFR8() then return the error

		uint8_t TConfig;
		Error =
		  MCP251XFD_ReadSFR8(pComp,
		                     RegMCP251XFD_CiTDC_CONFIG,
		                     &TConfig); // Read actual configuration of the
		                                // CiTDC register (Last byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		TConfig &= ~MCP251XFD_CAN_CiTDC8_SID11EN; // Clear the flag
		if ((flags & MCP251XFD_CANFD_USE_RRS_BIT_AS_SID11) > 0) // Use SID11?
		{
			TConfig |= MCP251XFD_CAN_CiTDC8_SID11EN; // Add use SID11 flag
			pComp->InternalConfig |=
			  MCP251XFD_CANFD_USE_RRS_BIT_AS_SID11; // Add use SID11 to the
			                                        // internal configuration
		}
		else
			pComp->InternalConfig &=
			  ~MCP251XFD_CANFD_USE_RRS_BIT_AS_SID11; // Else clear use SID11 to
			                                         // the internal
			                                         // configuration
		return MCP251XFD_WriteSFR8(pComp,
		                           RegMCP251XFD_CiTDC_CONFIG,
		                           TConfig); // Write new configuration to the
		                                     // CiTDC register (Last byte only)
	}

	//********************************************************************************************************************

	/*! @brief Sleep mode configuration of the MCP251XFD device
	 *
	 * Sleep mode is a low-power mode, where register and RAM contents are
	 * preserved and the clock is switched off. LPM is an Ultra-Low Power mode,
	 * where the majority of the chip is powered down. Only the logic required
	 * for wake-up is powered
	 * @warning Exiting LPM is similar to a POR. The CAN FD Controller module
	 * will transition to Configuration mode. All registers will be reset and
	 * RAM data will be lost. The device must be reconfigured
	 * @param[in] *pComp Is the pointed structure of the device where the sleep
	 * will be configured
	 * @param[in] useLowPowerMode Is at 'true' to use the low power mode if
	 * available or 'false' to use the simpler sleep
	 * @param[in] wakeUpFilter Indicate which filter to use for wake-up due to
	 * CAN bus activity. This feature can be used to protect the module from
	 * wake-up due to short glitches on the RXCAN pin.
	 * @param[in] interruptBusWakeUp Is at 'true' to enable bus wake-up
	 * interrupt or 'false' to disable the interrupt
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_ConfigureSleepMode(MCP251XFD              *pComp,
	                             bool                    useLowPowerMode,
	                             eMCP251XFD_WakeUpFilter wakeUpFilter,
	                             bool                    interruptBusWakeUp)
	{
#ifdef CHECK_NULL_PARAM
		if (pComp == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		if (useLowPowerMode &&
		    (MCP251XFD_DEV_ID_GET(pComp->InternalConfig) == MCP2517FD))
			return ERR__NOT_SUPPORTED; // If the device is MCP2517FD then it
			                           // does not support Low Power Mode
		eERRORRESULT Error;

		uint8_t Config;
		Error = MCP251XFD_ReadSFR8(
		  pComp,
		  RegMCP251XFD_OSC_CONFIG,
		  &Config); // Read the Oscillator Register configuration
		if (Error != ERR_NONE)
			return Error;    // If there is an error while reading the SFR then
			                 // return the error
		if (useLowPowerMode) // If the device support the Low Power Mode
		{
			Config |= MCP251XFD_SFR_OSC8_LPMEN; // Set OSC.LPMEN bit
			pComp->InternalConfig |=
			  MCP251XFD_SFR_OSC8_LPMEN; // Set the LPM in the internal config
		}
		else
		{
			Config &= ~MCP251XFD_SFR_OSC8_LPMEN; // Clear OSC.LPMEN bit
			pComp->InternalConfig &=
			  ~MCP251XFD_SFR_OSC8_LPMEN; // Clear the LPM in the internal config
		}
		Error = MCP251XFD_WriteSFR8(
		  pComp,
		  RegMCP251XFD_OSC_CONFIG,
		  Config); // Write the Oscillator Register configuration
		if (Error != ERR_NONE)
			return Error; // If there is an error while writing the SFR then
			              // return the error

		Error = MCP251XFD_ReadSFR8(
		  pComp,
		  RegMCP251XFD_CiCON + 1,
		  &Config); // Read actual flags configuration of the RegMCP251XFD_CiCON
		            // register (Second byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		Config &=
		  ~(MCP251XFD_CAN_CiCON8_WFT_Mask |
		    MCP251XFD_CAN_CiCON8_WAKFIL); // Clear actual filter configuration
		if (wakeUpFilter != MCP251XFD_NO_FILTER)
			Config |= MCP251XFD_CAN_CiCON8_WFT_SET(wakeUpFilter) |
			          MCP251XFD_CAN_CiCON8_WAKFIL; // Enable wake-up filter
		Error = MCP251XFD_WriteSFR8(
		  pComp,
		  RegMCP251XFD_CiCON + 1,
		  Config); // Write new flags configuration of the RegMCP251XFD_CiCON
		           // register (Second byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while writing the SFR then
			              // return the error

		Error = MCP251XFD_ReadSFR8(
		  pComp,
		  RegMCP251XFD_CiINT_CONFIG + 1,
		  &Config); // Read actual flags configuration of the RegMCP251XFD_CiINT
		            // register (Last byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		if (interruptBusWakeUp)
			Config |=
			  MCP251XFD_CAN_CiINT8_WAKIE; // Enable the Bus Wake Up Interrupt
		else
			Config &=
			  ~MCP251XFD_CAN_CiINT8_WAKIE; // Disable the Bus Wake Up Interrupt
		return MCP251XFD_WriteSFR8(
		  pComp,
		  RegMCP251XFD_CiINT_CONFIG + 1,
		  Config); // Write new flags configuration of the RegMCP251XFD_CiINT
		           // register (Last byte only)
	}

	/*! @brief Enter the MCP251XFD device in sleep mode
	 *
	 * This function puts the device in sleep mode
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_EnterSleepMode(MCP251XFD *pComp)
	{
#ifdef CHECK_NULL_PARAM
		if (pComp == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		eERRORRESULT Error;

		eMCP251XFD_PowerStates LastPS =
		  MCP251XFD_DEV_PS_GET(pComp->InternalConfig); // Get last power state
		if (LastPS == MCP251XFD_DEVICE_SLEEP_NOT_CONFIGURED)
			return ERR__CONFIGURATION; // No configuration available to enter
			                           // sleep mode
		if (LastPS != MCP251XFD_DEVICE_NORMAL_POWER_STATE)
			return ERR__ALREADY_IN_SLEEP; // Device already in sleep mode
		Error = MCP251XFD_RequestOperationMode(
		  pComp, MCP251XFD_SLEEP_MODE, false); // Set Sleep mode
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_RequestOperationMode() then return the
			              // error
		pComp->InternalConfig &= ~MCP251XFD_DEV_PS_Mask;
		if ((pComp->InternalConfig & MCP251XFD_SFR_OSC8_LPMEN) > 0)
			pComp->InternalConfig |= MCP251XFD_DEV_PS_SET(
			  MCP251XFD_DEVICE_LOWPOWER_SLEEP_STATE); // If Low Power Mode then
			                                          // the device will be in
			                                          // low power mode
		else
			pComp->InternalConfig |= MCP251XFD_DEV_PS_SET(
			  MCP251XFD_DEVICE_SLEEP_STATE); // Else the device will be in sleep
			                                 // mode
		return ERR_NONE;
	}

	/*! @brief Verify if the MCP251XFD device is in sleep mode
	 *
	 * This function verifies if the device is in sleep mode by checking the
	 * OSC.OSCDIS
	 * @warning In low power mode, it's impossible to check if the device is in
	 * sleep mode or not without wake it up because a simple asserting of the
	 * SPI CS will exit the LPM
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *isInSleepMode Indicate if the device is in sleep mode
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_IsDeviceInSleepMode(MCP251XFD *pComp,
	                                           bool      *isInSleepMode)
	{
#ifdef CHECK_NULL_PARAM
		if (pComp == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		eERRORRESULT Error;

		eMCP251XFD_PowerStates LastPS =
		  MCP251XFD_DEV_PS_GET(pComp->InternalConfig); // Get last power state
		if (LastPS == MCP251XFD_DEVICE_SLEEP_NOT_CONFIGURED)
			return ERR__CONFIGURATION; // No configuration available to enter
			                           // sleep mode
		*isInSleepMode = true;
		if (LastPS == MCP251XFD_DEVICE_LOWPOWER_SLEEP_STATE)
			return ERR__NOT_SUPPORTED; // Here if the device is in
			                           // DEVICE_LOWPOWER_SLEEP_STATE, a simple
			                           // assert of SPI CS exit the LPM then
			                           // this function is not supported
		uint8_t Config;
		Error = MCP251XFD_ReadSFR8(
		  pComp,
		  RegMCP251XFD_OSC_CONFIG,
		  &Config); // Read the Oscillator Register configuration
		if (Error != ERR_NONE)
			return Error; // If there is an error while reading the SFR then
			              // return the error
		*isInSleepMode = ((Config & MCP251XFD_SFR_OSC8_OSCDIS) >
		                  0); // Return the actual state of the sleep mode
		if (*isInSleepMode == false)
		{
			pComp->InternalConfig &= ~MCP251XFD_DEV_PS_Mask;
			pComp->InternalConfig |= MCP251XFD_DEV_PS_SET(
			  MCP251XFD_DEVICE_NORMAL_POWER_STATE); // If the function return is
			                                        // not in sleep mode then
			                                        // refresh the internal
			                                        // state of the device
		}
		return ERR_NONE;
	}

	/*! @brief Manually wake up the MCP251XFD device
	 *
	 * After a wake-up from sleep, the device will be in configuration mode.
	 * After a wake-up from low power sleep, the device is at the same state as
	 * a Power On Reset, the device must be reconfigured
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *fromstate Is the power mode state before the wake up (Can be
	 * NULL if not necessary to know)
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_WakeUp(MCP251XFD              *pComp,
	                              eMCP251XFD_PowerStates *fromState)
	{
#ifdef CHECK_NULL_PARAM
		if (pComp == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		eERRORRESULT Error;

		eMCP251XFD_PowerStates LastPS =
		  MCP251XFD_DEV_PS_GET(pComp->InternalConfig); // Get last power state
		if (LastPS == MCP251XFD_DEVICE_SLEEP_NOT_CONFIGURED)
			return ERR__CONFIGURATION; // No configuration available to wake up

		uint8_t Config;
		Error = MCP251XFD_ReadSFR8(
		  pComp,
		  RegMCP251XFD_OSC_CONFIG,
		  &Config); // Read the Oscillator Register configuration (Here if the
		            // device is in DEVICE_LOWPOWER_SLEEP_STATE, the reset
		            // already happened because a simple assert of SPI CS exit
		            // the LPM)
		if (Error != ERR_NONE)
			return Error; // If there is an error while reading the SFR then
			              // return the error

		if (LastPS !=
		    MCP251XFD_DEVICE_LOWPOWER_SLEEP_STATE) // In Sleep mode or a false
		                                           // normal state, there is
		                                           // more to do
		{
			Config &= ~MCP251XFD_SFR_OSC8_OSCDIS; // Clear OSC.OSCDIS bit
			Error = MCP251XFD_WriteSFR8(
			  pComp,
			  RegMCP251XFD_OSC_CONFIG,
			  Config); // Write the Oscillator Register configuration
			if (Error != ERR_NONE)
				return Error; // If there is an error while writing the SFR then
				              // return the error
		}
		if (fromState != NULL)
			*fromState = LastPS; // Return the previous sleep mode
		pComp->InternalConfig &= ~MCP251XFD_DEV_PS_Mask;
		pComp->InternalConfig |= MCP251XFD_DEV_PS_SET(
		  MCP251XFD_DEVICE_NORMAL_POWER_STATE); // Set normal power state
		return ERR_NONE;
	}

	/*! @brief Retrieve from which state mode the MCP251XFD device get a bus
	 * wake up from
	 *
	 * Use this function when the wake up interrupt occur (wake up from bus) and
	 * it's not from a manual wake up with the function MCP251XFD_WakeUp(). This
	 * function will indicate from which sleep mode (normal of low power) the
	 * wake up occurs
	 * @warning If you call this function, the driver will understand that the
	 * device is awake without verifying and configure itself as well
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @return Return the sleep mode state before wake up
	 */
	eMCP251XFD_PowerStates MCP251XFD_BusWakeUpFromState(MCP251XFD *pComp)
	{
#ifdef CHECK_NULL_PARAM
		if (pComp == NULL)
			return MCP251XFD_DEVICE_SLEEP_NOT_CONFIGURED;
#endif
		eMCP251XFD_PowerStates PowerState =
		  MCP251XFD_DEV_PS_GET(pComp->InternalConfig);
		pComp->InternalConfig &= ~MCP251XFD_DEV_PS_Mask;
		pComp->InternalConfig |= MCP251XFD_DEV_PS_SET(
		  MCP251XFD_DEVICE_NORMAL_POWER_STATE); // Set normal power state
		return PowerState;                      // Return last power state
	}

	//********************************************************************************************************************

	/*! @brief Configure the Time Stamp of frames in the MCP251XFD device
	 *
	 * This function configures the 32-bit free-running counter of the Time
	 * Stamp
	 * @param[in] *pComp Is the pointed structure of the device where the time
	 * stamp will be configured
	 * @param[in] enableTS Is at 'true' to enable Time Stamp or 'false' to
	 * disable Time Stamp
	 * @param[in] samplePoint Is an enumerator that indicate where the Time
	 * Stamp sample point is at
	 * @param[in] prescaler Is the prescaler of the Time Stamp counter (time in
	 * µs is: 1/SYSCLK/TBCPRE)
	 * @param[in] interruptBaseCounter Is at 'true' to enable time base counter
	 * interrupt or 'false' to disable the interrupt. A rollover of the TBC will
	 * generate an interrupt if interruptBaseCounter is set
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_ConfigureTimeStamp(MCP251XFD             *pComp,
	                             bool                   enableTS,
	                             eMCP251XFD_SamplePoint samplePoint,
	                             uint16_t               prescaler,
	                             bool                   interruptBaseCounter)
	{
		if (prescaler < (MCP251XFD_CAN_CiTSCON_TBCPRE_MINVALUE + 1))
			return ERR__PARAMETER_ERROR;
		if (prescaler > (MCP251XFD_CAN_CiTSCON_TBCPRE_MAXVALUE + 1))
			return ERR__PARAMETER_ERROR;
		eERRORRESULT               Error;
		MCP251XFD_CiTSCON_Register Config;

		//--- Write Time Stamp configuration ----------------------
		Config.CiTSCON =
		  MCP251XFD_CAN_CiTSCON_TBCDIS; // Initialize the register
		if (enableTS)
		{
			Config.CiTSCON |= MCP251XFD_CAN_CiTSCON_TBCEN; // Add Enable TS flag
			Config.CiTSCON |= MCP251XFD_CAN_CiTSCON_TSSP_SET(
			  samplePoint); // Set sample point position
			Config.CiTSCON |= MCP251XFD_CAN_CiTSCON_TBCPRE_SET(
			  prescaler - 1); // Set prescaler (time in µs is: 1/SYSCLK/TBCPRE)
		}
		Error = MCP251XFD_WriteData(pComp,
		                            RegMCP251XFD_CiTSCON,
		                            &Config.Bytes[0],
		                            3); // Write new configuration to the CiTDC
		                                // register (First 3-bytes only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_WriteData() then return the error

		uint8_t Flags;
		Error =
		  MCP251XFD_ReadSFR8(pComp,
		                     RegMCP251XFD_CiINT_CONFIG,
		                     &Flags); // Read actual flags configuration of the
		                              // CiINT register (Third byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		if (interruptBaseCounter)
			Flags |= MCP251XFD_CAN_CiINT8_TBCIE; // Add Time Base Counter
			                                     // Interrupt Enable flag
		else
			Flags &=
			  ~MCP251XFD_CAN_CiINT8_TBCIE; // Else clear the interrupt flag
		return MCP251XFD_WriteSFR8(
		  pComp,
		  RegMCP251XFD_CiINT_CONFIG,
		  Flags); // Write the new flags configuration to the CiINT register
		          // (Third byte only)
	}

	/*! @brief Set the Time Stamp counter the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] value Is the value to set into the counter
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_SetTimeStamp(MCP251XFD *pComp, uint32_t value)
	{
		return MCP251XFD_WriteSFR32(
		  pComp,
		  RegMCP251XFD_CiTBC,
		  value); // Write the new value to the CiTBC register
	}

	/*! @brief Get the Time Stamp counter the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *value Is the value to get from the counter
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_GetTimeStamp(MCP251XFD *pComp, uint32_t *value)
	{
		return MCP251XFD_ReadSFR32(
		  pComp,
		  RegMCP251XFD_CiTBC,
		  value); // Read the value to the CiTBC register
	}

	//********************************************************************************************************************

	/*! @brief Configure TEF of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] enableTEF Indicate if the TEF must be activated or not
	 * @param[in] *confTEF Is the configuration structure of the TEF
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_ConfigureTEF(MCP251XFD      *pComp,
	                                    bool            enableTEF,
	                                    MCP251XFD_FIFO *confTEF)
	{
		eERRORRESULT Error;

		//--- Enable/Disable TEF ---
		uint8_t CiCONflags;
		Error = MCP251XFD_ReadSFR8(
		  pComp,
		  RegMCP251XFD_CiCON + 2,
		  &CiCONflags); // Read actual flags configuration of the
		                // RegMCP251XFD_CiCON register (Third byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		if (MCP251XFD_CAN_CiCON8_OPMOD_GET(CiCONflags) !=
		    MCP251XFD_CONFIGURATION_MODE)
			return ERR__NEED_CONFIG_MODE; // Device must be in Configuration
			                              // Mode to perform the configuration
		if (enableTEF)
			CiCONflags |=
			  MCP251XFD_CAN_CiCON8_STEF; // Add Enable Transmit Event FIFO flag
		else
			CiCONflags &= ~MCP251XFD_CAN_CiCON8_STEF; // Else Disable Transmit
			                                          // Event FIFO flag
		Error = MCP251XFD_WriteSFR8(
		  pComp,
		  RegMCP251XFD_CiCON + 2,
		  CiCONflags); // Write the new flags configuration to the
		               // RegMCP251XFD_CiCON register (Third byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_WriteSFR8() then return the error

		//--- Configure TEF --
		if (enableTEF)
		{
#ifdef CHECK_NULL_PARAM
			if (confTEF == NULL)
				return ERR__PARAMETER_ERROR;
#endif
			if (confTEF->Name != MCP251XFD_TEF)
				return ERR__PARAMETER_ERROR;
			MCP251XFD_CiTEFCON_Register Reg;
			uint8_t                     Size =
			  MCP251XFD_CAN_TX_EVENTOBJECT_SIZE; // By default 1 element is
			                                     // 2x4-bytes in RAM
			Reg.CiTEFCON =
			  MCP251XFD_CAN_CiTEFCON_FSIZE_SET(confTEF->Size); // Add FIFO Size
			if ((confTEF->ControlFlags & MCP251XFD_FIFO_ADD_TIMESTAMP_ON_OBJ) >
			    0) // Transmit Event FIFO Time Stamp Enable
			{
				Reg.CiTEFCON |= MCP251XFD_CAN_CiTEFCON_TEFTSEN;
				Size += 4;
			}
			if ((confTEF->InterruptFlags & MCP251XFD_FIFO_OVERFLOW_INT) > 0)
				Reg.CiTEFCON |=
				  MCP251XFD_CAN_CiTEFCON_TEFOVIE; // Add Transmit Event FIFO
				                                  // Overflow Interrupt Enable
			if ((confTEF->InterruptFlags & MCP251XFD_FIFO_EVENT_FIFO_FULL_INT) >
			    0)
				Reg.CiTEFCON |=
				  MCP251XFD_CAN_CiTEFCON_TEFFIE; // Add Transmit Event FIFO Full
				                                 // Interrupt Enable
			if ((confTEF->InterruptFlags &
			     MCP251XFD_FIFO_EVENT_FIFO_HALF_FULL_INT) > 0)
				Reg.CiTEFCON |=
				  MCP251XFD_CAN_CiTEFCON_TEFHIE; // Add Transmit Event FIFO Half
				                                 // Full Interrupt Enable
			if ((confTEF->InterruptFlags &
			     MCP251XFD_FIFO_EVENT_FIFO_NOT_EMPTY_INT) > 0)
				Reg.CiTEFCON |=
				  MCP251XFD_CAN_CiTEFCON_TEFNEIE; // Add Transmit Event FIFO Not
				                                  // Empty Interrupt Enable

			Error = MCP251XFD_WriteData(
			  pComp,
			  RegMCP251XFD_CiTEFCON,
			  &Reg.Bytes[0],
			  sizeof(MCP251XFD_CiTEFCON_Register)); // Write TEF configuration
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_WriteData() then return the error

			if (confTEF->RAMInfos != NULL)
			{
				confTEF->RAMInfos->ByteInObject =
				  Size; // Set size of 1 object in the TEF
				confTEF->RAMInfos->ByteInFIFO =
				  (Size * ((uint8_t)confTEF->Size +
				           1)); // Total size of the TEF in RAM is 1 element
				                // size x Element Count
				confTEF->RAMInfos->RAMStartAddress =
				  MCP251XFD_RAM_ADDR; // Set the start address of the TEF. Here
				                      // the TEF is always the first in RAM
			}
		}

		return ERR_NONE;
	}

	/*! @brief Configure TXQ of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] enableTXQ Indicate if the TXQ must be activated or not
	 * @param[in] *confTXQ Is the configuration structure of the TXQ
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_ConfigureTXQ(MCP251XFD      *pComp,
	                                    bool            enableTXQ,
	                                    MCP251XFD_FIFO *confTXQ)
	{
		eERRORRESULT Error;

		//--- Enable/Disable TXQ ---
		uint8_t CiCONflags;
		Error = MCP251XFD_ReadSFR8(
		  pComp,
		  RegMCP251XFD_CiCON + 2,
		  &CiCONflags); // Read actual flags configuration of the
		                // RegMCP251XFD_CiCON register (Third byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		if (MCP251XFD_CAN_CiCON8_OPMOD_GET(CiCONflags) !=
		    MCP251XFD_CONFIGURATION_MODE)
			return ERR__NEED_CONFIG_MODE; // Device must be in Configuration
			                              // Mode to perform the configuration
		if (enableTXQ)
			CiCONflags |=
			  MCP251XFD_CAN_CiCON8_TXQEN; // Add Enable Transmit Queue flag
		else
			CiCONflags &=
			  ~MCP251XFD_CAN_CiCON8_TXQEN; // Else Disable Transmit Queue flag
		Error = MCP251XFD_WriteSFR8(
		  pComp,
		  RegMCP251XFD_CiCON + 2,
		  CiCONflags); // Write the new flags configuration to the
		               // RegMCP251XFD_CiCON register (Third byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_WriteSFR8() then return the error

		//--- Configure TXQ --
		if (enableTXQ)
		{
#ifdef CHECK_NULL_PARAM
			if (confTXQ == NULL)
				return ERR__PARAMETER_ERROR;
#endif
			if (confTXQ->Name != MCP251XFD_TXQ)
				return ERR__PARAMETER_ERROR;
			uint8_t Size =
			  sizeof(MCP251XFD_CAN_TX_Message); // By default 1 element is
			                                    // 2x4-bytes in RAM
			MCP251XFD_CiTXQCON_Register Reg;
			Reg.CiTXQCON = MCP251XFD_CAN_CiTXQCON_PLSIZE_SET(
			  confTXQ->Payload); // Add Payload Size
			Size += MCP251XFD_PayloadToByte(
			  confTXQ->Payload); // Add Payload size to Size of message
			Reg.CiTXQCON |=
			  MCP251XFD_CAN_CiTXQCON_FSIZE_SET(confTXQ->Size); // Add FIFO Size
			Reg.CiTXQCON |= MCP251XFD_CAN_CiTXQCON_TXAT_SET(
			  confTXQ->Attempts); // Add Retransmission Attempts
			Reg.CiTXQCON |= MCP251XFD_CAN_CiTXQCON_TXPRI_SET(
			  confTXQ->Priority); // Add Message transmit priority
			if ((confTXQ->InterruptFlags &
			     MCP251XFD_FIFO_TX_ATTEMPTS_EXHAUSTED_INT) > 0)
				Reg.CiTXQCON |=
				  MCP251XFD_CAN_CiTXQCON_TXATIE; // Add Transmit Attempts
				                                 // Exhausted Interrupt Enable
			if ((confTXQ->InterruptFlags &
			     MCP251XFD_FIFO_TRANSMIT_FIFO_EMPTY_INT) > 0)
				Reg.CiTXQCON |=
				  MCP251XFD_CAN_CiTXQCON_TXQEIE; // Add Transmit FIFO Empty
				                                 // Interrupt Enable
			if ((confTXQ->InterruptFlags &
			     MCP251XFD_FIFO_TRANSMIT_FIFO_NOT_FULL_INT) > 0)
				Reg.CiTXQCON |=
				  MCP251XFD_CAN_CiTXQCON_TXQNIE; // Add Transmit FIFO Not Full
				                                 // Interrupt Enable

			Error = MCP251XFD_WriteData(
			  pComp,
			  RegMCP251XFD_CiTXQCON,
			  &Reg.Bytes[0],
			  sizeof(MCP251XFD_CiTXQCON_Register)); // Write TXQ configuration
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_WriteData() then return the error

			if (confTXQ->RAMInfos != NULL)
			{
				confTXQ->RAMInfos->ByteInObject =
				  Size; // Set size of 1 object in the TXQ
				confTXQ->RAMInfos->ByteInFIFO =
				  (Size * ((uint8_t)confTXQ->Size +
				           1)); // Total size of the TXQ in RAM is 1 element
				                // size x Element Count
				confTXQ->RAMInfos->RAMStartAddress =
				  0; // Can't know the start address of the TXQ info here
			}
			if ((Size * ((uint8_t)confTXQ->Size + 1u)) > MCP251XFD_RAM_SIZE)
				return ERR__OUT_OF_MEMORY;
		}

		return ERR_NONE;
	}

	/*! @brief Configure a FIFO of the MCP251XFD device
	 *
	 * FIFO are enabled by configuring the FIFO and, in case of receive FIFO, a
	 * filter point to the FIFO
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] *confFIFO Is the configuration structure of the FIFO
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_ConfigureFIFO(MCP251XFD      *pComp,
	                                     MCP251XFD_FIFO *confFIFO)
	{
#ifdef CHECK_NULL_PARAM
		if (confFIFO == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		if ((confFIFO->Name == MCP251XFD_TEF) ||
		    (confFIFO->Name == MCP251XFD_TXQ) ||
		    (confFIFO->Name >= MCP251XFD_FIFO_COUNT))
			return ERR__PARAMETER_ERROR;
		eERRORRESULT Error;

		//--- Device in Configuration Mode ---
		eMCP251XFD_OperationMode OpMode;
		Error = MCP251XFD_GetActualOperationMode(
		  pComp, &OpMode); // Get actual Operational Mode
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_GetActualOperationMode() then return the
			              // error
		if (OpMode != MCP251XFD_CONFIGURATION_MODE)
			return ERR__NEED_CONFIG_MODE; // Device must be in Configuration
			                              // Mode to perform the configuration

		//--- Configure FIFO --
		MCP251XFD_CiFIFOCONm_Register Reg;
		uint8_t                       Size = sizeof(
          MCP251XFD_CAN_TX_Message); // By default 1 element is 2x4-bytes in RAM
		Reg.CiFIFOCONm = MCP251XFD_CAN_CiFIFOCONm_PLSIZE_SET(
		  confFIFO->Payload); // Add Payload Size
		Size += MCP251XFD_PayloadToByte(
		  confFIFO->Payload); // Add Payload size to Size of message
		Reg.CiFIFOCONm |=
		  MCP251XFD_CAN_CiFIFOCONm_FSIZE_SET(confFIFO->Size); // Add FIFO Size
		Reg.CiFIFOCONm |= MCP251XFD_CAN_CiFIFOCONm_TXAT_SET(
		  confFIFO->Attempts); // Add Retransmission Attempts
		Reg.CiFIFOCONm |= MCP251XFD_CAN_CiFIFOCONm_TXPRI_SET(
		  confFIFO->Priority); // Add Message transmit priority
		if (confFIFO->Direction == MCP251XFD_TRANSMIT_FIFO)
		{
			Reg.CiFIFOCONm |=
			  MCP251XFD_CAN_CiFIFOCONm_TXEN; // Transmit Event FIFO Time Stamp
			                                 // Enable
			if ((confFIFO->InterruptFlags &
			     MCP251XFD_FIFO_TX_ATTEMPTS_EXHAUSTED_INT) > 0)
				Reg.CiFIFOCONm |=
				  MCP251XFD_CAN_CiFIFOCONm_TXATIE; // Add Transmit Attempts
				                                   // Exhausted Interrupt Enable
			if ((confFIFO->ControlFlags & MCP251XFD_FIFO_AUTO_RTR_RESPONSE) > 0)
				Reg.CiFIFOCONm |=
				  MCP251XFD_CAN_CiFIFOCONm_RTREN; // When a remote transmit is
				                                  // received, Transmit Request
				                                  // (TXREQ) of the FIFO will be
				                                  // set
			if ((confFIFO->InterruptFlags &
			     MCP251XFD_FIFO_TRANSMIT_FIFO_EMPTY_INT) > 0)
				Reg.CiFIFOCONm |=
				  MCP251XFD_CAN_CiFIFOCONm_TFERFFIE; // Add Transmit FIFO Empty
				                                     // Interrupt Enable
			if ((confFIFO->InterruptFlags &
			     MCP251XFD_FIFO_TRANSMIT_FIFO_HALF_EMPTY_INT) > 0)
				Reg.CiFIFOCONm |=
				  MCP251XFD_CAN_CiFIFOCONm_TFHRFHIE; // Add Transmit FIFO Half
				                                     // Empty Interrupt Enable
			if ((confFIFO->InterruptFlags &
			     MCP251XFD_FIFO_TRANSMIT_FIFO_NOT_FULL_INT) > 0)
				Reg.CiFIFOCONm |=
				  MCP251XFD_CAN_CiFIFOCONm_TFNRFNIE; // Add Transmit FIFO Not
				                                     // Full Interrupt Enable
		}
		else
		{
			if ((confFIFO->ControlFlags & MCP251XFD_FIFO_ADD_TIMESTAMP_ON_RX) >
			    0) // Receive FIFO Time Stamp Enable
			{
				Reg.CiFIFOCONm |= MCP251XFD_CAN_CiFIFOCONm_RXTSEN;
				Size += 4;
			}
			if ((confFIFO->InterruptFlags & MCP251XFD_FIFO_OVERFLOW_INT) > 0)
				Reg.CiFIFOCONm |=
				  MCP251XFD_CAN_CiFIFOCONm_RXOVIE; // Add Transmit Attempts
				                                   // Exhausted Interrupt Enable
			if ((confFIFO->InterruptFlags &
			     MCP251XFD_FIFO_RECEIVE_FIFO_FULL_INT) > 0)
				Reg.CiFIFOCONm |=
				  MCP251XFD_CAN_CiFIFOCONm_TFERFFIE; // Add Receive FIFO Full
				                                     // Interrupt Enable
			if ((confFIFO->InterruptFlags &
			     MCP251XFD_FIFO_RECEIVE_FIFO_HALF_FULL_INT) > 0)
				Reg.CiFIFOCONm |=
				  MCP251XFD_CAN_CiFIFOCONm_TFHRFHIE; // Add Receive FIFO Half
				                                     // Full Interrupt Enable
			if ((confFIFO->InterruptFlags &
			     MCP251XFD_FIFO_RECEIVE_FIFO_NOT_EMPTY_INT) > 0)
				Reg.CiFIFOCONm |=
				  MCP251XFD_CAN_CiFIFOCONm_TFNRFNIE; // Add Receive FIFO Not
				                                     // Empty Interrupt Enable
		}

		uint16_t Address =
		  RegMCP251XFD_CiFIFOCONm +
		  (MCP251XFD_FIFO_REG_SIZE *
		   ((uint16_t)confFIFO->Name - 1u)); // Select the address of the FIFO
		Error = MCP251XFD_WriteData(
		  pComp,
		  Address,
		  &Reg.Bytes[0],
		  sizeof(MCP251XFD_CiFIFOCONm_Register)); // Write FIFO configuration
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_WriteData() then return the error

		if (confFIFO->RAMInfos != NULL)
		{
			confFIFO->RAMInfos->ByteInObject =
			  Size; // Set size of 1 object in the FIFO
			confFIFO->RAMInfos->ByteInFIFO =
			  (Size * ((uint8_t)confFIFO->Size +
			           1)); // Total size of the FIFO in RAM is 1 element size x
			                // Element Count
			confFIFO->RAMInfos->RAMStartAddress =
			  0; // Can't know the start address of the FIFO info here
		}
		if ((Size * ((uint8_t)confFIFO->Size + 1u)) > MCP251XFD_RAM_SIZE)
			return ERR__OUT_OF_MEMORY;

		return ERR_NONE;
	}

	/*! @brief Configure a FIFO list of the MCP251XFD device
	 *
	 * This function configures a set of FIFO at once. All FIFO, TEF or TXQ that
	 * are not in the list are either disabled or cleared
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] *listFIFO Is the list of FIFO to configure
	 * @param[in] count Is the count of FIFO in the list
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_ConfigureFIFOList(MCP251XFD      *pComp,
	                                         MCP251XFD_FIFO *listFIFO,
	                                         size_t          count)
	{
#ifdef CHECK_NULL_PARAM
		if (listFIFO == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		if (count == 0)
			return ERR_NONE;
		if (count > MCP251XFD_FIFO_CONF_MAX)
			return ERR__OUT_OF_RANGE;
		eERRORRESULT       Error;
		uint16_t           TotalSize = 0;
		MCP251XFD_RAMInfos TmpRAMInfos;
		bool               UseTmpRAMInfos = false;

		//--- First: Configure TEF if any ---
		uint8_t TEFcount = 0; // The TEF always start at address
		                      // MCP251XFD_RAM_ADDR so it must be first
		for (size_t zTEF = 0; zTEF < count; zTEF++)
		{
			if (listFIFO[zTEF].Name == MCP251XFD_TEF)
			{
				TEFcount++;
				if (TEFcount > MCP251XFD_TEF_MAX)
					return ERR__TOO_MANY_TEF;

				//--- Configure TEF ---
				UseTmpRAMInfos = (listFIFO[zTEF].RAMInfos ==
				                  NULL); // Is there a RAMInfos structure
				                         // attached to the configuration?
				if (UseTmpRAMInfos)
					listFIFO[zTEF].RAMInfos =
					  &TmpRAMInfos; // If not RAMInfos structure attached, then
					                // set a temporary one
				Error = MCP251XFD_ConfigureTEF(
				  pComp, true, &listFIFO[zTEF]); // Configure the TEF
				if (Error != ERR_NONE)
					return Error; // If there is an error while calling
					              // MCP251XFD_ConfigureTEF() then return the
					              // error
				TotalSize +=
				  listFIFO[zTEF]
				    .RAMInfos->ByteInFIFO; // Add TEF size to the total
				if (UseTmpRAMInfos)
					listFIFO[zTEF].RAMInfos =
					  NULL; // If not RAMInfos structure attached, then unset
					        // the temporary one
			}
		}
		if (TEFcount == 0)
		{
			Error =
			  MCP251XFD_ConfigureTEF(pComp, false, NULL); // Deactivate the TEF
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ConfigureTEF() then return the error
		}

		//--- Second: Configure TXQ if any ---
		uint8_t TXQcount = 0; // The TXQ always follow the TEF (if no TEF then
		                      // it's the first) in RAM
		for (size_t zTXQ = 0; zTXQ < count; zTXQ++)
		{
			if (listFIFO[zTXQ].Name == MCP251XFD_TXQ)
			{
				TXQcount++;
				if (TXQcount > MCP251XFD_TXQ_MAX)
					return ERR__TOO_MANY_TXQ;

				//--- Configure TXQ --
				UseTmpRAMInfos = (listFIFO[zTXQ].RAMInfos ==
				                  NULL); // Is there a RAMInfos structure
				                         // attached to the configuration?
				if (UseTmpRAMInfos)
					listFIFO[zTXQ].RAMInfos =
					  &TmpRAMInfos; // If not RAMInfos structure attached, then
					                // set a temporary one
				Error = MCP251XFD_ConfigureTXQ(
				  pComp, true, &listFIFO[zTXQ]); // Configure the TXQ
				if (Error != ERR_NONE)
					return Error; // If there is an error while calling
					              // MCP251XFD_ConfigureTXQ() then return the
					              // error
				listFIFO[zTXQ].RAMInfos->RAMStartAddress =
				  MCP251XFD_RAM_ADDR + TotalSize; // Set start address
				TotalSize +=
				  listFIFO[zTXQ]
				    .RAMInfos->ByteInFIFO; // Add TXQ size to the total
				if (UseTmpRAMInfos)
					listFIFO[zTXQ].RAMInfos =
					  NULL; // If not RAMInfos structure attached, then unset
					        // the temporary one
			}
		}
		if (TXQcount == 0)
		{
			Error =
			  MCP251XFD_ConfigureTXQ(pComp, false, NULL); // Deactivate the TxQ
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ConfigureTEF() then return the error
		}

		//--- Third: Configure FIFOs if any ---
		int32_t LastFIFO = 0;
		for (int32_t zFIFO = 1; zFIFO < MCP251XFD_FIFO_COUNT; zFIFO++)
		{
			for (size_t z = 0; z < count; z++)
			{
				if (listFIFO[z].Name == zFIFO)
				{
					for (int32_t clearFIFO = LastFIFO + 1; clearFIFO < zFIFO;
					     clearFIFO++) // For each FIFO not listed between 2 FIFO
					{
						Error = MCP251XFD_ClearFIFOConfiguration(
						  pComp,
						  (eMCP251XFD_FIFO)zFIFO); // Clear FIFO configuration
						if (Error != ERR_NONE)
							return Error; // If there is an error while calling
							              // MCP251XFD_ClearFIFOConfiguration()
							              // then return the error
						TotalSize +=
						  MCP251XFD_FIFO_MIN_SIZE; // Add min FIFO size. A FIFO
						                           // cannot be completely
						                           // disable so, it takes the
						                           // minimum possible size
					}
					LastFIFO = zFIFO;
					//--- Configure FIFO --
					UseTmpRAMInfos = (listFIFO[z].RAMInfos ==
					                  NULL); // Is there a RAMInfos structure
					                         // attached to the configuration?
					if (UseTmpRAMInfos)
						listFIFO[z].RAMInfos =
						  &TmpRAMInfos; // If not RAMInfos structure attached,
						                // then set a temporary one
					Error = MCP251XFD_ConfigureFIFO(
					  pComp, &listFIFO[z]); // Configure the FIFO
					if (Error != ERR_NONE)
						return Error; // If there is an error while calling
						              // MCP251XFD_ConfigureFIFO() then return
						              // the error
					listFIFO[z].RAMInfos->RAMStartAddress =
					  MCP251XFD_RAM_ADDR + TotalSize; // Set start address
					TotalSize +=
					  listFIFO[z]
					    .RAMInfos->ByteInFIFO; // Add FIFO size to the total
					if (UseTmpRAMInfos)
						listFIFO[z].RAMInfos =
						  NULL; // If not RAMInfos structure attached, then
						        // unset the temporary one
				}
			}
		}

		//--- Check RAM used ---
		if (TotalSize > MCP251XFD_RAM_SIZE)
			return ERR__OUT_OF_MEMORY;

		return ERR_NONE;
	}

	/*! @brief Reset a FIFO of the MCP251XFD device
	 *
	 * FIFO will be reset. The function will wait until the reset is effective.
	 * In Configuration Mode, the FIFO is automatically reset
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] name Is the name of the FIFO to be reset
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_ResetFIFO(MCP251XFD *pComp, eMCP251XFD_FIFO name)
	{
		if (name >= MCP251XFD_FIFO_COUNT)
			return ERR__PARAMETER_ERROR;
		eERRORRESULT Error;

		//--- Device in Configuration Mode ---
		eMCP251XFD_OperationMode OpMode;
		Error = MCP251XFD_GetActualOperationMode(
		  pComp, &OpMode); // Get actual Operational Mode
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_GetActualOperationMode() then return the
			              // error
		if (OpMode == MCP251XFD_CONFIGURATION_MODE)
			return ERR_NONE; // Device in Configuration Mode automatically reset
			                 // the FIFO

		//--- Set Reset of the FIFO ---
		uint16_t Address =
		  RegMCP251XFD_CiFIFOCONm_CONTROL +
		  (MCP251XFD_FIFO_REG_SIZE *
		   ((uint16_t)name - 1u)); // Select the address of the FIFO
		if (name == MCP251XFD_TEF)
			Address = RegMCP251XFD_CiTEFCON_CONTROL; // If it's the TEF then
			                                         // select its address
		if (name == MCP251XFD_TXQ)
			Address = RegMCP251XFD_CiTXQCON_CONTROL; // If it's the TXQ then
			                                         // select its address
		Error = MCP251XFD_WriteSFR8(
		  pComp,
		  Address,
		  MCP251XFD_CAN_CiFIFOCONm8_FRESET); // Write FIFO configuration (Second
		                                     // byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_WriteSFR8() then return the error

		//--- Now wait the reset to be effective ---
		uint8_t  Config    = 0;
		uint32_t StartTime = pComp->fnGetCurrentms(); // Start the timeout
		while (true)
		{
			Error = MCP251XFD_ReadSFR8(
			  pComp,
			  Address,
			  &Config); // Read current FIFO configuration (Second byte only)
			if (Error != ERR_NONE)
				return Error; // If there is an error while reading the register
				              // then return the error
			if ((Config & MCP251XFD_CAN_CiFIFOCONm8_FRESET) == 0)
				break; // Check if the FIFO was reset
			if (MCP251XFD_TIME_DIFF(StartTime, pComp->fnGetCurrentms()) > 3)
				return ERR__DEVICE_TIMEOUT; // Wait at least 3ms. If timeout
				                            // then return the error
		}
		return ERR_NONE;
	}

	/*! @brief Reset the TEF of the MCP251XFD device
	 *
	 * TEF will be reset. The function will wait until the reset is effective.
	 * In Configuration Mode, the TEF is automatically reset
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_ResetTEF(MCP251XFD *pComp)
	{
		return MCP251XFD_ResetFIFO(pComp, MCP251XFD_TEF);
	}

	/*! @brief Reset the TXQ of the MCP251XFD device
	 *
	 * TXQ will be reset. The function will wait until the reset is effective.
	 * In Configuration Mode, the TXQ is automatically reset
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_ResetTXQ(MCP251XFD *pComp)
	{
		return MCP251XFD_ResetFIFO(pComp, MCP251XFD_TXQ);
	}

	/*! @brief Update (and flush) a FIFO of the MCP251XFD device
	 *
	 * Increment the Head/Tail of the FIFO. If flush too, a message send request
	 * is ask
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] name Is the name of the FIFO to be update (and flush)
	 * @param[in] andFlush Indicate if the FIFO must be flush too
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_UpdateFIFO(MCP251XFD *pComp, eMCP251XFD_FIFO name, bool andFlush)
	{
		if (name >= MCP251XFD_FIFO_COUNT)
			return ERR__PARAMETER_ERROR;

		//--- Set address of the FIFO ---
		uint16_t Address =
		  RegMCP251XFD_CiFIFOCONm_CONTROL +
		  (MCP251XFD_FIFO_REG_SIZE *
		   ((uint16_t)name - 1u)); // Select the address of the FIFO
		if (name == MCP251XFD_TEF)
			Address = RegMCP251XFD_CiTEFCON_CONTROL; // If it's the TEF then
			                                         // select its address
		if (name == MCP251XFD_TXQ)
			Address = RegMCP251XFD_CiTXQCON_CONTROL; // If it's the TXQ then
			                                         // select its address

		//--- Set update (and flush if ask) ---
		uint8_t Config = MCP251XFD_CAN_CiFIFOCONm8_UINC; // Set update
		if (andFlush)
			Config |= MCP251XFD_CAN_CiFIFOCONm8_TXREQ; // Add flush if ask
		return MCP251XFD_WriteSFR8(
		  pComp,
		  Address,
		  Config); // Write FIFO update (and flush) (Second byte only)
	}

	/*! @brief Update the TEF of the MCP251XFD device
	 *
	 * Increment the Head/Tail of the TEF
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_UpdateTEF(MCP251XFD *pComp)
	{
		return MCP251XFD_UpdateFIFO(pComp, MCP251XFD_TEF, false);
	}

	/*! @brief Update (and flush) the TXQ of the MCP251XFD device
	 *
	 * Increment the Head/Tail of the TXQ. If flush too, a message send request
	 * is ask
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] andFlush Indicate if the TXQ must be flush too
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_UpdateTXQ(MCP251XFD *pComp, bool andFlush)
	{
		return MCP251XFD_UpdateFIFO(pComp, MCP251XFD_TXQ, andFlush);
	}

	/*! @brief Flush a FIFO of the MCP251XFD device
	 *
	 * A message send request is ask to the FIFO
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] name Is the name of the FIFO to be flush
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_FlushFIFO(MCP251XFD *pComp, eMCP251XFD_FIFO name)
	{
		if (name == MCP251XFD_TEF)
			return ERR__NOT_AVAILABLE; // In the TEF no flush is possible
		if (name >= MCP251XFD_FIFO_COUNT)
			return ERR__PARAMETER_ERROR;

		//--- Set the flush ---
		uint16_t Address = RegMCP251XFD_CiTXREQ +
		                   ((uint16_t)name >>
		                    3); // Select the good address of the TXREQ register
		return MCP251XFD_WriteSFR8(
		  pComp,
		  Address,
		  ((uint8_t)name & 0xFF)); // Write FIFO flush by selecting the good bit
	}

	/*! @brief Flush a TXQ of the MCP251XFD device
	 *
	 * A message send request is ask to the TXQ
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_FlushTXQ(MCP251XFD *pComp)
	{
		return MCP251XFD_FlushFIFO(pComp, MCP251XFD_TXQ);
	}

	/*! @brief Flush all FIFOs (+TXQ) of the MCP251XFD device
	 *
	 * Flush all TXQ and all transmit FIFOs
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_FlushAllFIFO(MCP251XFD *pComp)
	{
		return MCP251XFD_WriteSFR32(pComp, RegMCP251XFD_CiTXREQ, 0xFFFFFFFF);
	}

	/*! @brief Get status of a FIFO of the MCP251XFD device
	 *
	 * Get messages status and some interrupt flags related to this FIFO (First
	 * byte of CiFIFOSTAm)
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] name Is the name of the FIFO where status flags will be got
	 * @param[out] *statusFlags Is the return value of status flags
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_GetFIFOStatus(MCP251XFD               *pComp,
	                                     eMCP251XFD_FIFO          name,
	                                     setMCP251XFD_FIFOstatus *statusFlags)
	{
#ifdef CHECK_NULL_PARAM
		if (statusFlags == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		if (name >= MCP251XFD_FIFO_COUNT)
			return ERR__PARAMETER_ERROR;

		//--- Set address of the FIFO ---
		uint16_t Address =
		  RegMCP251XFD_CiFIFOSTAm_FLAGS +
		  (MCP251XFD_FIFO_REG_SIZE *
		   ((uint16_t)name - 1u)); // Select the address of the FIFO
		if (name == MCP251XFD_TEF)
			Address = RegMCP251XFD_CiTEFSTA_FLAGS; // If it's the TEF then
			                                       // select its address
		if (name == MCP251XFD_TXQ)
			Address = RegMCP251XFD_CiTXQSTA_FLAGS; // If it's the TXQ then
			                                       // select its address

		//--- Get FIFO status ---
		return MCP251XFD_ReadSFR8(
		  pComp,
		  Address,
		  (uint8_t *)statusFlags); // Read FIFO status (First byte only)
	}

	/*! @brief Get status of a TEF of the MCP251XFD device
	 *
	 * Get messages status and some interrupt flags related to this TEF (First
	 * byte of CiTEFSTA)
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *statusFlags Is the return value of status flags
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_GetTEFStatus(MCP251XFD              *pComp,
	                       setMCP251XFD_TEFstatus *statusFlags)
	{
		eERRORRESULT Error = MCP251XFD_GetFIFOStatus(
		  pComp, MCP251XFD_TEF, (setMCP251XFD_FIFOstatus *)statusFlags);
		*statusFlags = (setMCP251XFD_TEFstatus)((*statusFlags) &
		                                        MCP251XFD_TEF_FIFO_STATUS_MASK);
		return Error;
	}

	/*! @brief Get status of a TXQ of the MCP251XFD device
	 *
	 * Get messages status and some interrupt flags related to this TXQ (First
	 * byte of CiTXQSTA)
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *statusFlags Is the return value of status flags
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_GetTXQStatus(MCP251XFD              *pComp,
	                       setMCP251XFD_TXQstatus *statusFlags)
	{
		eERRORRESULT Error = MCP251XFD_GetFIFOStatus(
		  pComp, MCP251XFD_TXQ, (setMCP251XFD_FIFOstatus *)statusFlags);
		*statusFlags =
		  (setMCP251XFD_TXQstatus)((*statusFlags) & MCP251XFD_TXQ_STATUS_MASK);
		return Error;
	}

	/*! @brief Get next message address and/or index of a FIFO of the MCP251XFD
	 * device
	 *
	 * If it's a transmit FIFO then a read of this will return the address
	 * and/or index where the next message is to be written (FIFO head) If it's
	 * a receive FIFO then a read of this will return the address and/or index
	 * where the next message is to be read (FIFO tail)
	 * @warning This register is not guaranteed to read correctly in
	 * Configuration mode and should only be accessed when the module is not in
	 * Configuration mode
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] name Is the name of the FIFO
	 * @param[out] *nextAddress Is the next user address of the FIFO. This
	 * parameter can be NULL if not needed
	 * @param[out] *nextIndex Is the next user index of the FIFO. This parameter
	 * can be NULL if not needed
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_GetNextMessageAddressFIFO(MCP251XFD      *pComp,
	                                                 eMCP251XFD_FIFO name,
	                                                 uint32_t *nextAddress,
	                                                 uint8_t  *nextIndex)
	{
		if (name >= MCP251XFD_FIFO_COUNT)
			return ERR__PARAMETER_ERROR;
		eERRORRESULT Error;
		uint16_t     Address;

		//--- Get next message address ---
		if (nextAddress != NULL)
		{
			//--- Set address of the FIFO ---
			Address = RegMCP251XFD_CiFIFOUAm +
			          (MCP251XFD_FIFO_REG_SIZE *
			           ((uint16_t)name - 1u)); // Select the address of the FIFO
			if (name == MCP251XFD_TEF)
				Address = RegMCP251XFD_CiTEFUA; // If it's the TEF then select
				                                // its address
			if (name == MCP251XFD_TXQ)
				Address = RegMCP251XFD_CiTXQUA; // If it's the TXQ then select
				                                // its address
			//--- Get FIFO status ---
			Error = MCP251XFD_ReadSFR32(
			  pComp, Address, nextAddress); // Read FIFO user address
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ReadSFR32() then return the error
		}

		//--- Get next message index ---
		if (nextIndex != NULL)
		{
			if (name == MCP251XFD_TEF)
				return ERR__NOT_AVAILABLE;
			//--- Set address of the FIFO ---
			Address = RegMCP251XFD_CiFIFOSTAm_FIFOCI +
			          (MCP251XFD_FIFO_REG_SIZE *
			           ((uint16_t)name - 1u)); // Select the address of the FIFO
			if (name == MCP251XFD_TXQ)
				Address = RegMCP251XFD_CiTXQSTA_TXQCI; // If it's the TXQ then
				                                       // select its address

			//--- Get FIFO status ---
			Error = MCP251XFD_ReadSFR8(
			  pComp, Address, nextIndex); // Read FIFO status (Second byte only)
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ReadSFR8() then return the error
			*nextIndex &= MCP251XFD_CAN_CiFIFOSTAm8_FIFOCI_Mask;
		}
		return ERR_NONE;
	}

	/*! @brief Get next message address of a TEF of the MCP251XFD device
	 *
	 * A read of this register will return the address where the next object is
	 * to be read (FIFO tail) This register is not guaranteed to read correctly
	 * in Configuration mode and should only be accessed when the module is not
	 * in Configuration mode
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *nextAddress Is the next user address of the TEF. This
	 * parameter can be NULL if not needed
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_GetNextMessageAddressTEF(MCP251XFD *pComp, uint32_t *nextAddress)
	{
		return MCP251XFD_GetNextMessageAddressFIFO(
		  pComp, MCP251XFD_TEF, nextAddress, NULL);
	}

	/*! @brief Get next message address and/or index of a TXQ of the MCP251XFD
	 * device
	 *
	 * A read of this register will return the address where the next message is
	 * to be written (TXQ head) This register is not guaranteed to read
	 * correctly in Configuration mode and should only be accessed when the
	 * module is not in Configuration mode
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *nextAddress Is the next user address of the TXQ. This
	 * parameter can be NULL if not needed
	 * @param[out] *nextIndex Is the next user index of the TXQ. This parameter
	 * can be NULL if not needed
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_GetNextMessageAddressTXQ(MCP251XFD *pComp,
	                                   uint32_t  *nextAddress,
	                                   uint8_t   *nextIndex)
	{
		return MCP251XFD_GetNextMessageAddressFIFO(
		  pComp, MCP251XFD_TXQ, nextAddress, nextIndex);
	}

	/*! @brief Clear the FIFO configuration of the MCP251XFD device
	 *
	 * Clearing FIFO configuration do not disable totally it, all filter that
	 * point to it must be disabled too.
	 * @warning Never clear a FIFO in the middle of the FIFO list or it will
	 * completely destroy messages in RAM after this FIFO
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] name Is the name of the FIFO where the configuration will be
	 * cleared
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_ClearFIFOConfiguration(MCP251XFD      *pComp,
	                                              eMCP251XFD_FIFO name)
	{
		eERRORRESULT Error = MCP251XFD_ResetFIFO(pComp, name); // Reset the FIFO
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ResetFIFO() then return the error
#ifndef __cplusplus
		MCP251XFD_FIFO ClearConf = {
		  .Name           = name,
		  .Size           = MCP251XFD_FIFO_1_MESSAGE_DEEP,
		  .Payload        = MCP251XFD_PAYLOAD_8BYTE,
		  .Direction      = MCP251XFD_RECEIVE_FIFO,
		  .Attempts       = MCP251XFD_UNLIMITED_ATTEMPTS,
		  .Priority       = MCP251XFD_MESSAGE_TX_PRIORITY1,
		  .ControlFlags   = MCP251XFD_FIFO_NO_CONTROL_FLAGS,
		  .InterruptFlags = MCP251XFD_FIFO_NO_INTERRUPT_FLAGS,
		  .RAMInfos       = NULL,
		};
#else
		MCP251XFD_FIFO ClearConf = {
		  name,
		  MCP251XFD_FIFO_1_MESSAGE_DEEP,
		  MCP251XFD_PAYLOAD_8BYTE,
		  MCP251XFD_RECEIVE_FIFO,
		  MCP251XFD_UNLIMITED_ATTEMPTS,
		  MCP251XFD_MESSAGE_TX_PRIORITY1,
		  MCP251XFD_FIFO_NO_CONTROL_FLAGS,
		  MCP251XFD_FIFO_NO_INTERRUPT_FLAGS,
		  NULL,
		};
#endif // !__cplusplus
		return MCP251XFD_ConfigureFIFO(pComp,
		                               &ClearConf); // Clear the configuration
	}

	/*! @brief Set a FIFO interrupt configuration of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] name Is the name of the FIFO where the configuration will be
	 * changed
	 * @param[in] interruptFlags Is the interrupt flags to change (mentionned
	 * flags will be set and the others will be cleared)
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_SetFIFOinterruptConfiguration(
	  MCP251XFD              *pComp,
	  eMCP251XFD_FIFO         name,
	  eMCP251XFD_FIFOIntFlags interruptFlags)
	{
		eERRORRESULT Error;
		uint8_t      Config;
		if (name >= MCP251XFD_FIFO_COUNT)
			return ERR__PARAMETER_ERROR;

		//--- Set address of the FIFO ---
		uint16_t Address =
		  RegMCP251XFD_CiFIFOCONm_CONFIG +
		  (MCP251XFD_FIFO_REG_SIZE *
		   ((uint16_t)name - 1u)); // Select the address of the FIFO
		if (name == MCP251XFD_TEF)
			Address = RegMCP251XFD_CiTEFCON_CONFIG; // If it's the TEF then
			                                        // select its address
		if (name == MCP251XFD_TXQ)
			Address = RegMCP251XFD_CiTXQCON_CONFIG; // If it's the TXQ then
			                                        // select its address

		//--- Read the FIFO's register ---
		interruptFlags = static_cast<eMCP251XFD_FIFOIntFlags>(
		  interruptFlags & MCP251XFD_FIFO_ALL_INTERRUPTS_FLAGS);
		Error = MCP251XFD_ReadSFR8(
		  pComp,
		  Address,
		  &Config); // Read actual configuration of the FIFO's register
		if (Error != ERR_OK)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error

		//--- Set interrupts flags ---
		Config &=
		  ~MCP251XFD_CAN_CiFIFOCONm8_INT_Mask; // Force interrupts bits to 0
		if (((interruptFlags & MCP251XFD_FIFO_TRANSMIT_FIFO_NOT_FULL_INT) >
		     0) ||
		    ((interruptFlags & MCP251XFD_FIFO_RECEIVE_FIFO_NOT_EMPTY_INT) > 0))
			Config |=
			  MCP251XFD_CAN_CiFIFOCONm8_TFNRFNIE; // Transmit/Receive FIFO Not
			                                      // Full/Not Empty Interrupt
			                                      // Enable
		if (((interruptFlags & MCP251XFD_FIFO_TRANSMIT_FIFO_HALF_EMPTY_INT) >
		     0) ||
		    ((interruptFlags & MCP251XFD_FIFO_RECEIVE_FIFO_HALF_FULL_INT) > 0))
			Config |=
			  MCP251XFD_CAN_CiFIFOCONm8_TFHRFHIE; // Transmit/Receive FIFO Half
			                                      // Empty/Half Full Interrupt
			                                      // Enable
		if (((interruptFlags & MCP251XFD_FIFO_TRANSMIT_FIFO_EMPTY_INT) > 0) ||
		    ((interruptFlags & MCP251XFD_FIFO_RECEIVE_FIFO_FULL_INT) > 0))
			Config |=
			  MCP251XFD_CAN_CiFIFOCONm8_TFERFFIE; // Transmit/Receive FIFO
			                                      // Empty/Full Interrupt Enable
		if (((interruptFlags & MCP251XFD_FIFO_OVERFLOW_INT) > 0))
			Config |=
			  MCP251XFD_CAN_CiFIFOCONm8_RXOVIE; // Overflow Interrupt Enable
		if (((interruptFlags & MCP251XFD_FIFO_TX_ATTEMPTS_EXHAUSTED_INT) > 0))
			Config |=
			  MCP251XFD_CAN_CiFIFOCONm8_TXATIE; // Transmit Attempts Exhausted
			                                    // Interrupt Enable
		return MCP251XFD_WriteSFR8(
		  pComp,
		  Address,
		  Config); // Write new configuration to the FIFO's register
	}

	/*! @brief Set a TEF interrupt configuration of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] interruptFlags Is the interrupt flags to change (mentionned
	 * flags will be set and the others will be cleared)
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_SetTEFinterruptConfiguration(
	  MCP251XFD              *pComp,
	  eMCP251XFD_FIFOIntFlags interruptFlags)
	{
		return MCP251XFD_SetFIFOinterruptConfiguration(
		  pComp, MCP251XFD_TEF, interruptFlags);
	}

	/*! @brief Set a TXQ interrupt configuration of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] interruptFlags Is the interrupt flags to change (mentionned
	 * flags will be set and the others will be cleared)
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_SetTXQinterruptConfiguration(
	  MCP251XFD              *pComp,
	  eMCP251XFD_FIFOIntFlags interruptFlags)
	{
		return MCP251XFD_SetFIFOinterruptConfiguration(
		  pComp, MCP251XFD_TXQ, interruptFlags);
	}

	//**********************************************************************************************************************************************************

	/*! @brief Configure interrupt of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] interruptsFlags Is the set of events where interrupts will be
	 * enabled. Flags can be OR'ed
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_ConfigureInterrupt(MCP251XFD                   *pComp,
	                             setMCP251XFD_InterruptEvents interruptsFlags)
	{
		eERRORRESULT Error;
		Error = MCP251XFD_ClearInterruptEvents(
		  pComp,
		  MCP251XFD_INT_CLEARABLE_FLAGS_MASK); // Clear all clearable interrupts
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR16() then return the error
		return MCP251XFD_WriteSFR16(
		  pComp,
		  RegMCP251XFD_CiINT_CONFIG,
		  interruptsFlags); // Write new interrupt configuration (The 2 MSB
		                    // bytes)
	}

	/*! @brief Get interrupt events of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *interruptsFlags Is the return value of interrupt events.
	 * Flags are OR'ed
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_GetInterruptEvents(MCP251XFD                    *pComp,
	                             setMCP251XFD_InterruptEvents *interruptsFlags)
	{
		return MCP251XFD_ReadSFR16(
		  pComp,
		  RegMCP251XFD_CiINT_FLAG,
		  (uint16_t *)
		    interruptsFlags); // Read interrupt flags (The 2 LSB bytes)
	}

	/*! @brief Get the current interrupt event of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *currentEvent Is the return value of the current interrupt
	 * event
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_GetCurrentInterruptEvent(
	  MCP251XFD                    *pComp,
	  eMCP251XFD_InterruptFlagCode *currentEvent)
	{
		return MCP251XFD_ReadSFR8(
		  pComp,
		  RegMCP251XFD_CiVEC_ICODE,
		  (uint8_t *)
		    currentEvent); // Read current interrupt event (First byte only)
	}

	/*! @brief Clear interrupt events of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] interruptsFlags Is the set of events where interrupts will be
	 * cleared. Flags can be OR'ed
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_ClearInterruptEvents(MCP251XFD                   *pComp,
	                               setMCP251XFD_InterruptEvents interruptsFlags)
	{
		if ((interruptsFlags & MCP251XFD_INT_CLEARABLE_FLAGS_MASK) == 0)
			return ERR_NONE;
		uint16_t     Interrupts = 0;
		eERRORRESULT Error      = MCP251XFD_ReadSFR16(
          pComp,
          RegMCP251XFD_CiINT_FLAG,
          &Interrupts); // Read interrupt flags (The 2 LSB bytes)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR16() then return the error
		Interrupts &= ~((uint16_t)interruptsFlags); // Clear selected flags
		return MCP251XFD_WriteSFR16(
		  pComp,
		  RegMCP251XFD_CiINT_FLAG,
		  Interrupts); // Write flags cleared (The 2 LSB bytes)
	}

	/*! @brief Get current receive FIFO name and status that generate an
	 * interrupt (if any)
	 *
	 * This function can be called to check if there is a receive interrupt. It
	 * gives the name and the status of the FIFO. If more than one object has an
	 * interrupt pending, the interrupt or FIFO with the highest number will
	 * show up. Once the interrupt with the highest priority is cleared, the
	 * next highest priority interrupt will show up
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *name Is the returned name of the FIFO that generate an
	 * interrupt
	 * @param[out] *flags Is the return value of status flags of the FIFO (can
	 * be NULL if it's not needed). Flags can be OR'ed
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_GetCurrentReceiveFIFONameAndStatusInterrupt(
	  MCP251XFD               *pComp,
	  eMCP251XFD_FIFO         *name,
	  setMCP251XFD_FIFOstatus *flags)
	{
#ifdef CHECK_NULL_PARAM
		if (name == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		eERRORRESULT Error;

		//--- Get current interrupt code ---
		*name          = MCP251XFD_NO_FIFO;
		uint8_t RxCode = 0;
		Error          = MCP251XFD_ReadSFR8(
          pComp,
          RegMCP251XFD_CiVEC_RXCODE,
          &RxCode); // Read Interrupt code register (Last byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		RxCode = MCP251XFD_CAN_CiVEC8_RXCODE_GET(RxCode); // Extract RXCODE
		if (RxCode < MCP251XFD_FIFO1)
			return ERR__UNKNOWN_ELEMENT; // FIFO0 is reserved so not possible
		if ((RxCode > MCP251XFD_FIFO31) && (RxCode != MCP251XFD_NO_INTERRUPT))
			return ERR__UNKNOWN_ELEMENT; // Only FIFO1 to 31 and no Interrupt
			                             // code possible so the rest is not
			                             // possible
		if (RxCode == MCP251XFD_NO_INTERRUPT)
			return ERR_NONE;             // No interrupt? Good
		*name = (eMCP251XFD_FIFO)RxCode; // Save the current FIFO name interrupt

		//--- Get status flags of the FIFO ---
		if ((*name != MCP251XFD_NO_FIFO) && (flags != NULL))
			return MCP251XFD_GetFIFOStatus(
			  pComp, *name, flags); // Get status flags of the FIFO
		return ERR_NONE;
	}

	/*! @brief Get current receive FIFO name that generate an interrupt (if any)
	 *
	 * This function can be called to check if there is a receive interrupt. It
	 * gives the name of the FIFO. If more than one object has an interrupt
	 * pending, the interrupt or FIFO with the highest number will show up. Once
	 * the interrupt with the highest priority is cleared, the next highest
	 * priority interrupt will show up
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *name Is the returned name of the FIFO that generate an
	 * interrupt
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_GetCurrentReceiveFIFONameInterrupt(MCP251XFD       *pComp,
	                                             eMCP251XFD_FIFO *name)
	{
		return MCP251XFD_GetCurrentReceiveFIFONameAndStatusInterrupt(
		  pComp, name, NULL);
	}

	/*! @brief Get current transmit FIFO name and status that generate an
	 * interrupt (if any)
	 *
	 * This function can be called to check if there is a transmit interrupt. It
	 * gives the name and the status of the FIFO. If more than one object has an
	 * interrupt pending, the interrupt or FIFO with the highest number will
	 * show up. Once the interrupt with the highest priority is cleared, the
	 * next highest priority interrupt will show up
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *name Is the returned name of the FIFO that generate an
	 * interrupt
	 * @param[out] *flags Is the return value of status flags of the FIFO (can
	 * be NULL if it's not needed). Flags can be OR'ed
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_GetCurrentTransmitFIFONameAndStatusInterrupt(
	  MCP251XFD               *pComp,
	  eMCP251XFD_FIFO         *name,
	  setMCP251XFD_FIFOstatus *flags)
	{
#ifdef CHECK_NULL_PARAM
		if (name == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		eERRORRESULT Error;

		//--- Get current interrupt code ---
		*name          = MCP251XFD_NO_FIFO;
		uint8_t TxCode = 0;
		Error          = MCP251XFD_ReadSFR8(
          pComp,
          RegMCP251XFD_CiVEC_TXCODE,
          &TxCode); // Read Interrupt code register (Third byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		TxCode = MCP251XFD_CAN_CiVEC8_TXCODE_GET(TxCode); // Extract TXCODE
		if ((TxCode > MCP251XFD_FIFO31) && (TxCode != MCP251XFD_NO_INTERRUPT))
			return ERR__UNKNOWN_ELEMENT; // Only FIFO0 (TXQ) to 31 and no
			                             // Interrupt code possible so the rest
			                             // is not possible
		if (TxCode == MCP251XFD_NO_INTERRUPT)
			return ERR_NONE;             // No interrupt? Good
		*name = (eMCP251XFD_FIFO)TxCode; // Save the current FIFO name interrupt

		//--- Get status flags of the FIFO ---
		if ((*name != MCP251XFD_NO_FIFO) && (flags != NULL))
			return MCP251XFD_GetFIFOStatus(
			  pComp, *name, flags); // Get status flags of the FIFO
		return ERR_NONE;
	}

	/*! @brief Get current transmit FIFO name that generate an interrupt (if
	 * any)
	 *
	 * This function can be called to check if there is a transmit interrupt. It
	 * gives the name of the FIFO. If more than one object has an interrupt
	 * pending, the interrupt or FIFO with the highest number will show up. Once
	 * the interrupt with the highest priority is cleared, the next highest
	 * priority interrupt will show up
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *name Is the returned name of the FIFO that generate an
	 * interrupt
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_GetCurrentTransmitFIFONameInterrupt(MCP251XFD       *pComp,
	                                              eMCP251XFD_FIFO *name)
	{
		return MCP251XFD_GetCurrentTransmitFIFONameAndStatusInterrupt(
		  pComp, name, NULL);
	}

	/*! @brief Clear selected FIFO events of the MCP251XFD device
	 *
	 * Clear the events in parameter to the FIFO selected
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] name Is the name of the FIFO where events will be cleared
	 * @param[in] events Are the set of events to clear
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_ClearFIFOEvents(MCP251XFD      *pComp,
	                                       eMCP251XFD_FIFO name,
	                                       uint8_t         events)
	{
		if (name >= MCP251XFD_FIFO_COUNT)
			return ERR__PARAMETER_ERROR;
		if ((events & MCP251XFD_FIFO_CLEARABLE_STATUS_FLAGS) == 0)
			return ERR_NONE; // Threat only the clearable ones
		eERRORRESULT Error;

		uint16_t Address =
		  RegMCP251XFD_CiFIFOSTAm_FLAGS +
		  (MCP251XFD_FIFO_REG_SIZE *
		   ((uint16_t)name - 1u)); // Select the address of the FIFO
		if (name == MCP251XFD_TEF)
			Address = RegMCP251XFD_CiTEFSTA_FLAGS; // If it's the TEF then
			                                       // select its address
		if (name == MCP251XFD_TXQ)
			Address = RegMCP251XFD_CiTXQSTA_FLAGS; // If it's the TXQ then
			                                       // select its address

		//--- Get FIFO status ---
		uint8_t Status;
		Error =
		  MCP251XFD_ReadSFR8(pComp, Address, &Status); // Read FIFO status flags
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error

		//--- Clear selected flags ---
		if (((events & MCP251XFD_TEF_FIFO_OVERFLOW) > 0) ||
		    ((events & MCP251XFD_RX_FIFO_OVERFLOW) > 0))
			Status &= ~MCP251XFD_CAN_CiFIFOSTAm8_RXOVIF; // Receive & TEF: Clear
			                                             // Overflow event
		if (((events & MCP251XFD_TXQ_ATTEMPTS_EXHAUSTED) > 0) ||
		    ((events & MCP251XFD_TX_FIFO_ATTEMPTS_EXHAUSTED) > 0))
			Status &=
			  ~MCP251XFD_CAN_CiFIFOSTAm8_TXATIF; // Transmit & TXQ: Clear
			                                     // Attempts Exhausted event
		if (((events & MCP251XFD_TXQ_BUS_ERROR) > 0) ||
		    ((events & MCP251XFD_TX_FIFO_BUS_ERROR) > 0))
			Status &= ~MCP251XFD_CAN_CiFIFOSTAm8_TXERR; // Transmit & TXQ: Clear
			                                            // Error Detected During
			                                            // Transmission event
		if (((events & MCP251XFD_TXQ_ARBITRATION_LOST) > 0) ||
		    ((events & MCP251XFD_TX_FIFO_ARBITRATION_LOST) > 0))
			Status &= ~MCP251XFD_CAN_CiFIFOSTAm8_TXLARB; // Transmit & TXQ:
			                                             // Clear Message Lost
			                                             // Arbitration event
		if (((events & MCP251XFD_TXQ_STATUS_MASK) > 0) ||
		    ((events & MCP251XFD_TX_FIFO_STATUS_MASK) > 0))
			Status &= ~MCP251XFD_CAN_CiFIFOSTAm8_TXABT; // Transmit & TXQ: Clear
			                                            // Message Aborted event
		return MCP251XFD_WriteSFR8(
		  pComp, Address, Status); // Write new FIFO status flags
	}

	/*! @brief Clear TEF overflow event of the MCP251XFD device
	 *
	 * Clear the overflow event of the TEF
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_ClearTEFOverflowEvent(MCP251XFD *pComp)
	{
		return MCP251XFD_ClearFIFOEvents(
		  pComp, MCP251XFD_TEF, MCP251XFD_TEF_FIFO_OVERFLOW);
	}

	/*! @brief Clear FIFO overflow event of the MCP251XFD device
	 *
	 * Clear the overflow event of the FIFO selected
	 * @warning This function does not check if it's a receive FIFO
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] name Is the name of the FIFO where event will be cleared
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_ClearFIFOOverflowEvent(MCP251XFD      *pComp,
	                                                     eMCP251XFD_FIFO name)
	{
		return MCP251XFD_ClearFIFOEvents(
		  pComp, name, MCP251XFD_RX_FIFO_OVERFLOW);
	}

	/*! @brief Clear FIFO attempts event of the MCP251XFD device
	 *
	 * Clear the attempt event of the FIFO selected
	 * @warning This function does not check if it's a transmit FIFO
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] name Is the name of the FIFO where event will be cleared
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_ClearFIFOAttemptsEvent(MCP251XFD      *pComp,
	                                                     eMCP251XFD_FIFO name)
	{
		return MCP251XFD_ClearFIFOEvents(
		  pComp, name, MCP251XFD_TX_FIFO_ATTEMPTS_EXHAUSTED);
	}

	/*! @brief Clear TXQ attempts event of the MCP251XFD device
	 *
	 * Clear the attempt event of the TXQ selected
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_ClearTXQAttemptsEvent(MCP251XFD *pComp)
	{
		return MCP251XFD_ClearFIFOEvents(
		  pComp, MCP251XFD_TXQ, MCP251XFD_TXQ_ATTEMPTS_EXHAUSTED);
	}

	/*! @brief Get the receive interrupt pending status of all FIFO
	 *
	 * Get the status of receive pending interrupt and overflow pending
	 * interrupt of all FIFOs at once, OR'ed in one variable
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *interruptPending Is the return of the receive pending
	 * interrupt of all FIFOs (This parameter can be NULL)
	 * @param[out] *overflowStatus Is the return of the receive overflow pending
	 * interrupt of all FIFOs (This parameter can be NULL)
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_GetReceiveInterruptStatusOfAllFIFO(
	  MCP251XFD                    *pComp,
	  setMCP251XFD_InterruptOnFIFO *interruptPending,
	  setMCP251XFD_InterruptOnFIFO *overflowStatus)
	{
		eERRORRESULT Error;
		if (interruptPending != NULL)
		{
			Error = MCP251XFD_ReadSFR32(
			  pComp,
			  RegMCP251XFD_CiRXIF,
			  (uint32_t *)
			    interruptPending); // Read the Receive interrupt status register
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ReadSFR32() then return the error
		}
		if (overflowStatus != NULL)
		{
			Error = MCP251XFD_ReadSFR32(
			  pComp,
			  RegMCP251XFD_CiRXOVIF,
			  (uint32_t *)overflowStatus); // Read the Receive overflow
			                               // interrupt status register
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ReadSFR32() then return the error
		}
		return ERR_NONE;
	}

	/*! @brief Get the receive interrupt pending status of all FIFO
	 *
	 * Get the status of receive overflow interrupt of all FIFOs at once, OR'ed
	 * in one variable
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *interruptPending Is the return of the receive pending
	 * interrupt of all FIFOs
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_GetReceivePendingInterruptStatusOfAllFIFO(
	  MCP251XFD                    *pComp,
	  setMCP251XFD_InterruptOnFIFO *interruptPending)
	{
		if (interruptPending == NULL)
			return ERR__PARAMETER_ERROR;
		return MCP251XFD_GetReceiveInterruptStatusOfAllFIFO(
		  pComp, interruptPending, NULL);
	}

	/*! @brief Get the receive overflow interrupt pending status of all FIFO
	 *
	 * Get the status of overflow pending interrupt of all FIFOs at once, OR'ed
	 * in one variable
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *overflowStatus Is the return of the receive overflow pending
	 * interrupt of all FIFOs
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_GetReceiveOverflowInterruptStatusOfAllFIFO(
	  MCP251XFD                    *pComp,
	  setMCP251XFD_InterruptOnFIFO *overflowStatus)
	{
		if (overflowStatus == NULL)
			return ERR__PARAMETER_ERROR;
		return MCP251XFD_GetReceiveInterruptStatusOfAllFIFO(
		  pComp, NULL, overflowStatus);
	}

	/*! @brief Get the transmit interrupt pending status of all FIFO
	 *
	 * Get the status of transmit pending interrupt and attempt exhaust pending
	 * interrupt of all FIFOs at once, OR'ed in one variable
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *interruptPending Is the return of the transmit pending
	 * interrupt of all FIFOs (This parameter can be NULL)
	 * @param[out] *attemptStatus Is the return of the transmit attempt exhaust
	 * pending interrupt of all FIFOs (This parameter can be NULL)
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_GetTransmitInterruptStatusOfAllFIFO(
	  MCP251XFD                    *pComp,
	  setMCP251XFD_InterruptOnFIFO *interruptPending,
	  setMCP251XFD_InterruptOnFIFO *attemptStatus)
	{
		eERRORRESULT Error;
		if (interruptPending != NULL)
		{
			Error = MCP251XFD_ReadSFR32(
			  pComp,
			  RegMCP251XFD_CiTXIF,
			  (uint32_t *)
			    interruptPending); // Read the Receive interrupt status register
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ReadSFR32() then return the error
		}
		if (attemptStatus != NULL)
		{
			Error = MCP251XFD_ReadSFR32(
			  pComp,
			  RegMCP251XFD_CiTXATIF,
			  (uint32_t *)attemptStatus); // Read the Receive overflow interrupt
			                              // status register
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ReadSFR32() then return the error
		}
		return ERR_NONE;
	}

	/*! @brief Get the transmit interrupt pending status of all FIFO
	 *
	 * Get the status of transmit pending interrupt of all FIFOs at once, OR'ed
	 * in one variable
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *interruptPending Is the return of the transmit pending
	 * interrupt of all FIFOs
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_GetTransmitPendingInterruptStatusOfAllFIFO(
	  MCP251XFD                    *pComp,
	  setMCP251XFD_InterruptOnFIFO *interruptPending)
	{
		if (interruptPending == NULL)
			return ERR__PARAMETER_ERROR;
		return MCP251XFD_GetReceiveInterruptStatusOfAllFIFO(
		  pComp, interruptPending, NULL);
	}

	/*! @brief Get the transmit attempt exhaust interrupt pending status of all
	 * FIFO
	 *
	 * Get the status of attempt exhaust pending interrupt of all FIFOs at once,
	 * OR'ed in one variable
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[out] *attemptStatus Is the return of the transmit attempt exhaust
	 * pending interrupt of all FIFOs
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT MCP251XFD_GetTransmitAttemptInterruptStatusOfAllFIFO(
	  MCP251XFD                    *pComp,
	  setMCP251XFD_InterruptOnFIFO *attemptStatus)
	{
		if (attemptStatus == NULL)
			return ERR__PARAMETER_ERROR;
		return MCP251XFD_GetReceiveInterruptStatusOfAllFIFO(
		  pComp, NULL, attemptStatus);
	}

	//********************************************************************************************************************

	/*! @brief Configure the Device NET filter of the MCP251XFD device
	 *
	 * When a standard frame is received and the filter is configured for
	 * extended frames, the EID part of the Filter and Mask Object can be
	 * selected to filter on data bytes
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] filter Is the Device NET Filter to apply on all received
	 * frames and filters
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_ConfigureDeviceNetFilter(MCP251XFD            *pComp,
	                                   eMCP251XFD_DNETFilter filter)
	{
		eERRORRESULT Error;

		//--- Enable/Disable DNCNT ---
		uint8_t CiCONflags;
		Error = MCP251XFD_ReadSFR8(
		  pComp,
		  RegMCP251XFD_CiCON,
		  &CiCONflags); // Read actual flags configuration of the
		                // RegMCP251XFD_CiCON register (First byte only)
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		CiCONflags &= ~MCP251XFD_CAN_CiCON8_DNCNT_Mask; // Clear DNCNT config
		CiCONflags |= MCP251XFD_CAN_CiCON8_DNCNT_SET(
		  filter); // Set new filter configuration
		return MCP251XFD_WriteSFR8(
		  pComp,
		  RegMCP251XFD_CiCON,
		  CiCONflags); // Write the new flags configuration to the
		               // RegMCP251XFD_CiCON register (First byte only)
	}

	/*! @brief Configure a filter of the MCP251XFD device
	 *
	 * @warning This function does not check if the pointed FIFO is a receive
	 * FIFO
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] *confFilter Is the configuration structure of the Filter
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_ConfigureFilter(MCP251XFD        *pComp,
	                                       MCP251XFD_Filter *confFilter)
	{
#ifdef CHECK_NULL_PARAM
		if (confFilter == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		if ((confFilter->PointTo == MCP251XFD_TEF) ||
		    (confFilter->PointTo == MCP251XFD_TXQ) ||
		    (confFilter->PointTo >= MCP251XFD_FIFO_COUNT))
			return ERR__CONFIGURATION;
		eERRORRESULT Error;

		//--- Check if the filter is disabled ---
		MCP251XFD_CiFLTCONm_Register FilterConf;
		uint16_t                     AddrFLTCON =
		  static_cast<uint16_t>(RegMCP251XFD_CiFLTCONm) +
		  static_cast<uint16_t>(
		    confFilter->Filter); // Select the address of the FLTCON
		Error = MCP251XFD_ReadSFR8(
		  pComp,
		  AddrFLTCON,
		  &FilterConf.CiFLTCONm); // Read actual flags configuration of the
		                          // RegMCP251XFD_CiFLTCONm register
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		if ((FilterConf.CiFLTCONm & MCP251XFD_CAN_CiFLTCONm_ENABLE) > 0)
		{
			FilterConf.CiFLTCONm =
			  MCP251XFD_CAN_CiFLTCONm8_DISABLE; // Disable the filter
			Error = MCP251XFD_WriteSFR8(
			  pComp,
			  AddrFLTCON,
			  FilterConf.CiFLTCONm); // Write the new flags configuration to the
			                         // RegMCP251XFD_CiFLTCONm register
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_WriteSFR8() then return the error
		}

		if (confFilter->EnableFilter)
		{
			//--- Get the SID11 configuration ---
			bool UseSID11 = MCP251XFD_USE_SID11;

			//--- Check values ---
			if ((confFilter->AcceptanceID & confFilter->AcceptanceMask) !=
			    confFilter->AcceptanceID)
				return ERR__FILTER_CONSISTENCY;
			uint32_t MaxMask = (UseSID11 ? 1 : 0);
			switch (confFilter->Match)
			{
				case MCP251XFD_MATCH_ONLY_SID:
					MaxMask += MCP251XFD_SID_Size;
					break;
				case MCP251XFD_MATCH_ONLY_EID:
				case MCP251XFD_MATCH_SID_EID:
					MaxMask += MCP251XFD_EID_Size + MCP251XFD_SID_Size;
					break;
			}
			MaxMask = ~((1 << MaxMask) - 1);
			if ((confFilter->AcceptanceID & MaxMask) > 0)
				return ERR__FILTER_TOO_LARGE;
			if ((confFilter->AcceptanceMask & MaxMask) > 0)
				return ERR__FILTER_TOO_LARGE;

			//=== Fill Filter Object register ===
			MCP251XFD_CiFLTOBJm_Register FltObj;
			FltObj.CiFLTOBJm = MCP251XFD_MessageIDtoObjectMessageIdentifier(
			  confFilter->AcceptanceID,
			  (confFilter->Match != MCP251XFD_MATCH_ONLY_SID),
			  UseSID11);
			if (confFilter->Match == MCP251XFD_MATCH_ONLY_EID)
				FltObj.CiFLTOBJm |=
				  MCP251XFD_CAN_CiFLTOBJm_EXIDE; // If filter match only
				                                 // messages with extended
				                                 // identifier set the bit EXIDE
			//--- Send the filter object config ---
			uint16_t AddrFLTObj =
			  RegMCP251XFD_CiFLTOBJm +
			  ((uint16_t)confFilter->Filter *
			   MCP251XFD_FILTER_REG_SIZE); // Select the address of the CiFLTOBJ
			Error = MCP251XFD_WriteSFR32(
			  pComp,
			  AddrFLTObj,
			  FltObj.CiFLTOBJm); // Write the new flags configuration to the
			                     // RegMCP251XFD_CiFLTOBJm register
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_WriteSFR32() then return the error

			//=== Fill Filter Mask register ===
			MCP251XFD_CiMASKm_Register FltMask;
			FltMask.CiMASKm = MCP251XFD_MessageIDtoObjectMessageIdentifier(
			  confFilter->AcceptanceMask,
			  (confFilter->Match != MCP251XFD_MATCH_ONLY_SID),
			  UseSID11);
			if (confFilter->Match != MCP251XFD_MATCH_SID_EID)
				FltMask.CiMASKm |=
				  MCP251XFD_CAN_CiMASKm_MIDE; // If filter match only messages
				                              // with standard or extended
				                              // identifier set the bit MIDE
			//--- Send the filter mask config ---
			uint16_t AddrMask =
			  RegMCP251XFD_CiMASKm +
			  ((uint16_t)confFilter->Filter *
			   MCP251XFD_FILTER_REG_SIZE); // Select the address of the CiMASK
			Error = MCP251XFD_WriteSFR32(
			  pComp,
			  AddrMask,
			  FltMask.CiMASKm); // Write the new flags configuration to the
			                    // RegMCP251XFD_CiMASKm register
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_WriteSFR32() then return the error

			//=== Configure Filter control ===
			FilterConf.CiFLTCONm |=
			  MCP251XFD_CAN_CiFLTCONm8_ENABLE; // Enable filter
			FilterConf.CiFLTCONm |= MCP251XFD_CAN_CiFLTCONm8_FBP_SET(
			  confFilter->PointTo); // Set the Filter pointer to FIFO
			Error = MCP251XFD_WriteSFR8(
			  pComp,
			  AddrFLTCON,
			  FilterConf.CiFLTCONm); // Write the new flags configuration to the
			                         // RegMCP251XFD_CiFLTCONm register
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_WriteSFR8() then return the error
		}
		return ERR_NONE;
	}

	/*! @brief Configure a filter list and the DNCNT of the MCP251XFD device
	 *
	 * This function configures a set of Filters at once
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] filter Is the Device NET Filter to apply on all received
	 * frames, it call directly MCP251XFD_ConfigureDeviceNetFilter()
	 * @param[in] *listFilter Is the list of Filters to configure
	 * @param[in] count Is the count of Filters in the list
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_ConfigureFilterList(MCP251XFD            *pComp,
	                                           eMCP251XFD_DNETFilter filter,
	                                           MCP251XFD_Filter     *listFilter,
	                                           size_t                count)
	{
#ifdef CHECK_NULL_PARAM
		if (listFilter == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		if (count == 0)
			return ERR_NONE;
		if (count > MCP251XFD_FILTER_COUNT)
			return ERR__OUT_OF_RANGE;
		eERRORRESULT Error;

		//--- Configure the Device NET Filter ---
		Error = MCP251XFD_ConfigureDeviceNetFilter(
		  pComp, filter); // Configure the DNCNT
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ConfigureDeviceNetFilter() then return
			              // the error

		//--- Configure filters ---
		bool Modified = false;
		for (int32_t zFilter = 0; zFilter < MCP251XFD_FILTER_COUNT; zFilter++)
		{
			for (size_t z = 0; z < count; z++)
			{
				if (listFilter[z].Filter == zFilter)
				{
					Modified = true;
					Error    = MCP251XFD_ConfigureFilter(
                      pComp, &listFilter[z]); // Configure the filter
					if (Error != ERR_NONE)
						return Error; // If there is an error while calling
						              // MCP251XFD_ConfigureFilter() then return
						              // the error
				}
			}
			if (Modified == false) // Filter not listed
			{
				Error = MCP251XFD_DisableFilter(
				  pComp,
				  (eMCP251XFD_Filter)
				    zFilter); // Disable the Filter configuration
				if (Error != ERR_NONE)
					return Error; // If there is an error while calling
					              // MCP251XFD_DisableFilter() then return the
					              // error
			}
			Modified = false;
		}

		return ERR_NONE;
	}

	/*! @brief Disable a Filter of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be used
	 * @param[in] name Is the name of the Filter to disable
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_DisableFilter(MCP251XFD        *pComp,
	                                     eMCP251XFD_Filter name)
	{
		if (name >= MCP251XFD_FILTER_COUNT)
			return ERR__CONFIGURATION;
		eERRORRESULT                 Error;
		MCP251XFD_CiFLTCONm_Register FilterConf;

		uint16_t AddrFLTCON =
		  static_cast<uint16_t>(RegMCP251XFD_CiFLTCONm) +
		  static_cast<uint16_t>(name); // Select the address of the FLTCON
		Error = MCP251XFD_ReadSFR8(
		  pComp,
		  AddrFLTCON,
		  &FilterConf.CiFLTCONm); // Read actual flags configuration of the
		                          // RegMCP251XFD_CiFLTCONm register
		if (Error != ERR_NONE)
			return Error; // If there is an error while calling
			              // MCP251XFD_ReadSFR8() then return the error
		if ((FilterConf.CiFLTCONm & MCP251XFD_CAN_CiFLTCONm_ENABLE) > 0)
		{
			FilterConf.CiFLTCONm &=
			  ~MCP251XFD_CAN_CiFLTCONm8_ENABLE; // Disable the filter
			Error = MCP251XFD_WriteSFR8(
			  pComp,
			  AddrFLTCON,
			  FilterConf.CiFLTCONm); // Write the new flags configuration to the
			                         // RegMCP251XFD_CiFLTCONm register
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_WriteSFR8() then return the error
		}
		return ERR_NONE;
	}

	//********************************************************************************************************************

	/*! @brief Get transmit/receive error count and status of the MCP251XFD
	 * device
	 *
	 * @param[in] *pComp Is the pointed structure of the device use
	 * @param[out] *transmitErrorCount Is the result of the transmit error count
	 * (this parameter can be NULL)
	 * @param[out] *receiveErrorCount Is the result of the receive error count
	 * (this parameter can be NULL)
	 * @param[out] *status Is the return transmit/receive error status (this
	 * parameter can be NULL)
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_GetTransmitReceiveErrorCountAndStatus(
	  MCP251XFD                  *pComp,
	  uint8_t                    *transmitErrorCount,
	  uint8_t                    *receiveErrorCount,
	  eMCP251XFD_TXRXErrorStatus *status)
	{
		eERRORRESULT Error;
		if (transmitErrorCount != NULL)
		{
			Error = MCP251XFD_ReadSFR8(
			  pComp,
			  RegMCP251XFD_CiTREC_TEC,
			  transmitErrorCount); // Read the transmit error counter
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ReadSFR8() then return the error
		}
		if (receiveErrorCount != NULL)
		{
			Error = MCP251XFD_ReadSFR8(
			  pComp,
			  RegMCP251XFD_CiTREC_REC,
			  receiveErrorCount); // Read the receive error counter
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ReadSFR8() then return the error
		}
		if (status != NULL)
		{
			Error = MCP251XFD_ReadSFR8(
			  pComp,
			  RegMCP251XFD_CiTREC_STATUS,
			  (uint8_t *)status); // Read the transmit/receive error status
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ReadSFR8() then return the error
		}

		return ERR_NONE;
	}

	/*! @brief Get transmit error count of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device use
	 * @param[out] *transmitErrorCount Is the result of the transmit error count
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_GetTransmitErrorCount(MCP251XFD *pComp,
	                                uint8_t   *transmitErrorCount)
	{
		if (transmitErrorCount == NULL)
			return ERR__PARAMETER_ERROR;
		return MCP251XFD_GetTransmitReceiveErrorCountAndStatus(
		  pComp, transmitErrorCount, NULL, NULL);
	}

	/*! @brief Get receive error count of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device use
	 * @param[out] *receiveErrorCount Is the result of the receive error count
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_GetReceiveErrorCount(MCP251XFD *pComp, uint8_t *receiveErrorCount)
	{
		if (receiveErrorCount == NULL)
			return ERR__PARAMETER_ERROR;
		return MCP251XFD_GetTransmitReceiveErrorCountAndStatus(
		  pComp, NULL, receiveErrorCount, NULL);
	}

	/*! @brief Get transmit/receive error status of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device use
	 * @param[out] *status Is the return transmit/receive error status
	 * @return Returns an #eERRORRESULT value enum
	 */
	inline eERRORRESULT
	MCP251XFD_GetTransmitReceiveErrorStatus(MCP251XFD                  *pComp,
	                                        eMCP251XFD_TXRXErrorStatus *status)
	{
		if (status == NULL)
			return ERR__PARAMETER_ERROR;
		return MCP251XFD_GetTransmitReceiveErrorCountAndStatus(
		  pComp, NULL, NULL, status);
	}

	/*! @brief Get Bus diagnostic of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device use
	 * @param[out] *busDiagnostic0 Is the return value that contains separate
	 * error counters for receive/transmit and for nominal/data bit rates. The
	 * counters work differently than the counters in the CiTREC register. They
	 * are simply incremented by one on every error. They are never decremented
	 * (this parameter can be NULL)
	 * @param[out] *busDiagnostic1 Is the return value that keeps track of the
	 * kind of error that occurred since the last clearing of the register. The
	 * register also contains the error-free message counter (this parameter can
	 * be NULL)
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT
	MCP251XFD_GetBusDiagnostic(MCP251XFD                   *pComp,
	                           MCP251XFD_CiBDIAG0_Register *busDiagnostic0,
	                           MCP251XFD_CiBDIAG1_Register *busDiagnostic1)
	{
		eERRORRESULT Error;
		uint32_t     Data;
		if (busDiagnostic0 != NULL)
		{
			Error =
			  MCP251XFD_ReadSFR32(pComp,
			                      RegMCP251XFD_CiBDIAG0,
			                      &Data); // Read the transmit error counter
			busDiagnostic0->CiBDIAG0 = Data;
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ReadSFR32() then return the error
		}
		if (busDiagnostic1 != NULL)
		{
			Error =
			  MCP251XFD_ReadSFR32(pComp,
			                      RegMCP251XFD_CiBDIAG1,
			                      &Data); // Read the receive error counter
			busDiagnostic1->CiBDIAG1 = Data;
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_ReadSFR32() then return the error
		}
		return ERR_NONE;
	}

	/*! @brief Clear Bus diagnostic of the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device use
	 * @param[in] clearBusDiagnostic0 Set to 'true' to clear the bus diagnostic0
	 * @param[in] clearBusDiagnostic1 Set to 'true' to clear the bus diagnostic1
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_ClearBusDiagnostic(MCP251XFD *pComp,
	                                          bool       clearBusDiagnostic0,
	                                          bool       clearBusDiagnostic1)
	{
		eERRORRESULT Error;
		if (clearBusDiagnostic0)
		{
			Error = MCP251XFD_WriteSFR32(
			  pComp,
			  RegMCP251XFD_CiBDIAG0,
			  0x00000000); // Read the transmit error counter
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_WriteSFR32() then return the error
		}
		if (clearBusDiagnostic1)
		{
			Error = MCP251XFD_WriteSFR32(
			  pComp,
			  RegMCP251XFD_CiBDIAG1,
			  0x00000000); // Read the receive error counter
			if (Error != ERR_NONE)
				return Error; // If there is an error while calling
				              // MCP251XFD_WriteSFR32() then return the error
		}
		return ERR_NONE;
	}
	//********************************************************************************************************************

	/*! @brief Reset the MCP251XFD device
	 *
	 * @param[in] *pComp Is the pointed structure of the device to be reset
	 * @return Returns an #eERRORRESULT value enum
	 */
	eERRORRESULT MCP251XFD_ResetDevice(MCP251XFD *pComp)
	{
#ifdef CHECK_NULL_PARAM
		if (pComp == NULL)
			return ERR__PARAMETER_ERROR;
		if (pComp->fnSPI_Transfer == NULL)
			return ERR__PARAMETER_ERROR;
#endif
		eERRORRESULT Error;
		uint8_t      Buffer[2] = {(MCP251XFD_SPI_INSTRUCTION_RESET << 4), 0};

		//--- Do DRIVER_SAFE_RESET flag implementation before reset
		//---------------
		if ((pComp->DriverConfig & MCP251XFD_DRIVER_SAFE_RESET) > 0)
		{
#ifdef CHECK_NULL_PARAM
			if (pComp->fnSPI_Init == NULL)
				return ERR__PARAMETER_ERROR;
			if (pComp->fnGetCurrentms == NULL)
				return ERR__PARAMETER_ERROR;
#endif

			// Set SPI speed to 1MHz
			Error = pComp->fnSPI_Init(
			  pComp->InterfaceDevice,
			  pComp->SPI_ChipSelect,
			  MCP251XFD_DRIVER_SAFE_RESET_SPI_CLK); // Set the SPI speed clock
			                                        // to 1MHz (safe clock
			                                        // speed)
			if (Error != ERR_OK)
				return Error; // If there is an error while reading the register
				              // then return the error

			// Request configuration mode to avoid transmission error without
			// aborting them
			Error = MCP251XFD_RequestOperationMode(
			  pComp,
			  MCP251XFD_CONFIGURATION_MODE,
			  true); // Request a configuration mode and wait until device is
			         // this mode
			if (Error != ERR_OK)
				return Error; // If there is an error while calling
				              // MCP251XFD_RequestOperationMode() then return
				              // the error
		}

		//-- Perform Reset
		//--------------------------------------------------------
		return pComp->fnSPI_Transfer(pComp->InterfaceDevice,
		                             pComp->SPI_ChipSelect,
		                             &Buffer[0],
		                             NULL,
		                             2 /*bytes only*/); // Perform reset
	}

	//********************************************************************************************************************

	/*! @brief Message ID to Object Message Identifier
	 *
	 * @param[in] messageID Is the message ID to convert
	 * @param[in] extended Indicate if the message ID is extended or standard
	 * @param[in] UseSID11 Indicate if the message ID use the SID11
	 * @return Returns the Message ID
	 */
	uint32_t MCP251XFD_MessageIDtoObjectMessageIdentifier(uint32_t messageID,
	                                                      bool     extended,
	                                                      bool     UseSID11)
	{
		uint32_t ResultOMI = 0; // Initialize message ID to 0

		//--- Fill message ID (T0 or R0 or TE0) ---
		if (extended) // Message use extended ID?
		{
			ResultOMI =
			  ((messageID >> MCP251XFD_EID_Size) & MCP251XFD_SID_Mask) |
			  ((messageID & MCP251XFD_EID_Mask) << MCP251XFD_SID_Size);
			if (UseSID11)
				ResultOMI |= (messageID &
				              (1 << (MCP251XFD_EID_Size + MCP251XFD_SID_Size)));
		}
		else
		{
			ResultOMI = messageID & MCP251XFD_SID_Mask;
			if (UseSID11)
				ResultOMI |= ((messageID & (1 << MCP251XFD_SID_Size))
				              << (29 - MCP251XFD_SID_Size));
		}
		return ResultOMI;
	}

	/*! @brief Object Message Identifier to Message ID
	 *
	 * @param[in] objectMessageID Is the object message ID to convert
	 * @param[in] extended Indicate if the object message ID is extended or
	 * standard
	 * @param[in] UseSID11 Indicate if the object message ID use the SID11
	 * @return Returns the Object Message Identifier
	 */
	uint32_t
	MCP251XFD_ObjectMessageIdentifierToMessageID(uint32_t objectMessageID,
	                                             bool     extended,
	                                             bool     UseSID11)
	{
		uint32_t ResultID = 0; // Initialize message ID to 0

		//--- Extract object message ID (T0 or R0 or TE0) ---
		if (extended) // Message use extended ID?
		{
			ResultID =
			  ((objectMessageID >> MCP251XFD_SID_Size) & MCP251XFD_EID_Mask) |
			  ((objectMessageID & MCP251XFD_SID_Mask) << MCP251XFD_EID_Size);
			if (UseSID11)
				ResultID |= (objectMessageID &
				             (1 << (MCP251XFD_EID_Size + MCP251XFD_SID_Size)));
		}
		else
		{
			ResultID = objectMessageID & MCP251XFD_SID_Mask;
			if (UseSID11)
				ResultID |=
				  ((objectMessageID &
				    (1 << (MCP251XFD_EID_Size + MCP251XFD_SID_Size))) >>
				   (29 - MCP251XFD_SID_Size));
		}
		return ResultID;
	}

	//********************************************************************************************************************

	/*! @brief Payload to Byte Count
	 *
	 * @param[in] payload Is the enum of Message Payload Size (8, 12, 16, 20,
	 * 24, 32, 48 or 64 bytes)
	 * @return Returns the byte count
	 */
	uint8_t MCP251XFD_PayloadToByte(eMCP251XFD_PayloadSize payload)
	{
		//  const uint8_t PAYLOAD_TO_VALUE[PAYLOAD_COUNT] = {8, 12, 16, 20, 24,
		//  32, 48, 64}; if (payload < PAYLOAD_COUNT) return
		//  PAYLOAD_TO_VALUE[payload]; return 0;
		switch (payload)
		{
			case MCP251XFD_PAYLOAD_8BYTE:
				return 8; // Payload  8 data bytes
			case MCP251XFD_PAYLOAD_12BYTE:
				return 12; // Payload 12 data bytes
			case MCP251XFD_PAYLOAD_16BYTE:
				return 16; // Payload 16 data bytes
			case MCP251XFD_PAYLOAD_20BYTE:
				return 20; // Payload 20 data bytes
			case MCP251XFD_PAYLOAD_24BYTE:
				return 24; // Payload 24 data bytes
			case MCP251XFD_PAYLOAD_32BYTE:
				return 32; // Payload 32 data bytes
			case MCP251XFD_PAYLOAD_48BYTE:
				return 48; // Payload 48 data bytes
			case MCP251XFD_PAYLOAD_64BYTE:
				return 64; // Payload 64 data bytes
			default:
				return 0;
		}
		return 0;
	}

	/*! @brief Data Length Content to Byte Count
	 *
	 * @param[in] dlc Is the enum of Message DLC Size (0, 1, 2, 3, 4, 5, 6, 7,
	 * 8, 12, 16, 20, 24, 32, 48 or 64 bytes)
	 * @param[in] isCANFD Indicate if the DLC is from a CAN-FD frame or not
	 * @return Returns the byte count
	 */
	uint8_t MCP251XFD_DLCToByte(eMCP251XFD_DataLength dlc, bool isCANFD)
	{
		const uint8_t CAN20_DLC_TO_VALUE[MCP251XFD_DLC_COUNT] = {
		  0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 8, 8, 8, 8, 8, 8};
		const uint8_t CANFD_DLC_TO_VALUE[MCP251XFD_DLC_COUNT] = {
		  0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
		if ((dlc >= MCP251XFD_DLC_COUNT) || (dlc < (eMCP251XFD_DataLength)0))
			return 0;
		if (isCANFD)
			return CANFD_DLC_TO_VALUE[dlc & 0xF];
		return CAN20_DLC_TO_VALUE[dlc & 0xF];
	}

	//********************************************************************************************************************

} // namespace MCP251XFD