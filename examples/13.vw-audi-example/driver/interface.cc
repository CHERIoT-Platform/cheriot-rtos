#include <cdefs.h>
#include <stdint.h>
#include <thread.h>

#include <platform/sunburst/platform-gpio.hh>
#include <platform/sunburst/platform-pinmux.hh>
#include <platform/sunburst/platform-spi.hh>
#include <driver/MCP251XFD/ErrorsDef.h>
#include <driver/MCP251XFD/crc/CRC16_CMS.hh>
#include <driver/MCP251XFD/MCP251XFD.hh>
#include "interface.hh"


// The SPI clock calculation
// The settings is the length of a half period of the SPI clock, measured in system clock cycles reduced by 1.
// The system clock is 40000000Hz (40MHz - 25ns)
// The code examples use a 1MHz SPI clock. Which gives us a clock period of 1E-06 or 1uS.
// Therefore, we want to try to set the half period to 500ns. So our setting, s, will be:
// s = (500 / 25ns) - 1 = 20 - 1 = 19

#define SPI_CLOCK_SPEED_SETTING	(19)
#define CLEAR_BIT(REG, BIT) (REG = REG & (~(1U << (BIT))))
#define SET_BIT(REG, BIT)   (REG = REG | (1U << (BIT)))

using Debug = ConditionalDebug<true, "Interface.cc">;

eERRORRESULT GetSpiConfig(Spi_Config_t* cfg, uint8_t spiNum, SonataPinmux::PinSink cs0, SonataPinmux::PinSink cs1, SonataPinmux::PinSink cs2, SonataPinmux::PinSink sclk, SonataPinmux::PinSink copi, uint8_t cipo) {
    if(spiNum == 1) {
        cfg->spi = MMIO_CAPABILITY(SonataSpi::Generic<>, spi1);    // Access to the SPI1 module
    } else if(spiNum == 2) {
        cfg->spi = MMIO_CAPABILITY(SonataSpi::Generic<>, spi2);    // Access to the SPI2 module
    } else {
        return ERR__SPI_PARAMETER_ERROR;
    }
    if(spiNum == 1) {
        // SCLK
        if((sclk == SonataPinmux::PinSink::rph_g11) || (sclk == SonataPinmux::PinSink::ah_tmpio13)) {
            cfg->sclk_sel = 1;
        } else if(sclk == SonataPinmux::PinSink::pmod0_4) {
            cfg->sclk_sel = 2;
        } else {
            return ERR__SPI_PARAMETER_ERROR;
        }
        // COPI
        if((copi == SonataPinmux::PinSink::rph_g10) || (copi == SonataPinmux::PinSink::ah_tmpio11)) {
            cfg->copi_sel = 1;
        } else if(copi == SonataPinmux::PinSink::pmod0_2) {
            cfg->copi_sel = 2;
        } else {
            return ERR__SPI_PARAMETER_ERROR;
        }
        // CIPO
        if((cipo < 1) || (cipo > 3)) {
            return ERR__SPI_PARAMETER_ERROR;
        }
        cfg->cipo_sel = SonataPinmux::BlockSink::spi_1_cipo;
        // CS0
        if(cs0 == SonataPinmux::PinSink::rph_g8) {
            cfg->cs0_sel = 1;
        } else if(cs0 == SonataPinmux::PinSink::pmod0_1) {
            cfg->cs0_sel = 2;
        } else if(cs0 == SonataPinmux::PinSink::ser0_tx) {
            cfg->cs0_sel = 0; // Not Used
        } else {
            return ERR__SPI_PARAMETER_ERROR;
        }
        // CS1
        if(cs1 == SonataPinmux::PinSink::rph_g7) {
            cfg->cs1_sel = 1;
        } else if(cs1 == SonataPinmux::PinSink::pmod0_9) {
            cfg->cs1_sel = 2;
        } else if(cs1 == SonataPinmux::PinSink::ser0_tx) {
            cfg->cs1_sel = 0; // Not Used
        } else {
            return ERR__SPI_PARAMETER_ERROR;
        }
        // CS2
        if(cs2 == SonataPinmux::PinSink::ser0_tx) {
            cfg->cs2_sel = 0; // Not Used
        } else {
            return ERR__SPI_PARAMETER_ERROR;
        }
    } else if(spiNum == 2) {
        // SCLK
        if((sclk == SonataPinmux::PinSink::rph_g21) || (sclk == SonataPinmux::PinSink::mb2)) {
            cfg->sclk_sel = 1;
        } else if(sclk == SonataPinmux::PinSink::pmod1_4) {
            cfg->sclk_sel = 2;
        } else {
            return ERR__SPI_PARAMETER_ERROR;
        }
        // COPI
        if((copi == SonataPinmux::PinSink::rph_g20) || (copi == SonataPinmux::PinSink::mb4)) {
            cfg->copi_sel = 1;
        } else if(copi == SonataPinmux::PinSink::pmod1_2) {
            cfg->copi_sel = 2;
        } else {
            return ERR__SPI_PARAMETER_ERROR;
        }
        // CIPO
        if((cipo < 1) || (cipo > 3)) {
            return ERR__SPI_PARAMETER_ERROR;
        }
        cfg->cipo_sel = SonataPinmux::BlockSink::spi_2_cipo;
        // CS0
        if(cs0 == SonataPinmux::PinSink::rph_g18) {
            cfg->cs0_sel = 1;
        } else if(cs0 == SonataPinmux::PinSink::pmod1_1) {
            cfg->cs0_sel = 2;
        } else if(cs0 == SonataPinmux::PinSink::ser0_tx) {
            cfg->cs0_sel = 0; // Not Used
        } else {
            return ERR__SPI_PARAMETER_ERROR;
        }
        // CS1
        if(cs1 == SonataPinmux::PinSink::rph_g17) {
            cfg->cs1_sel = 1;
        } else if(cs1 == SonataPinmux::PinSink::pmod1_9) {
            cfg->cs1_sel = 2;
        } else if(cs1 == SonataPinmux::PinSink::ser0_tx) {
            cfg->cs1_sel = 0; // Not Used
        } else {
            return ERR__SPI_PARAMETER_ERROR;
        }
        // CS2
        if(cs1 == SonataPinmux::PinSink::rph_g16) {
            cfg->cs1_sel = 1;
        } else if(cs1 == SonataPinmux::PinSink::pmod1_10) {
            cfg->cs1_sel = 2;
        } else if(cs1 == SonataPinmux::PinSink::ser0_tx) {
            cfg->cs1_sel = 0; // Not Used
        } else {
            return ERR__SPI_PARAMETER_ERROR;
        }
    } else {
        return ERR__SPI_PARAMETER_ERROR;
    }
    cfg->spi_num = spiNum;
    cfg->cs0 = cs0;
    cfg->cs1 = cs1;
    cfg->sclk = sclk;
    cfg->copi = copi;
    cfg->cipo = cipo;
    return ERR_OK;
}

//=============================================================================
// MCP251XFD_X get millisecond
//=============================================================================
uint32_t GetCurrentms_Sonata(void)
{
    // Sonata
    // Written - 2025-01-10
    // Tested - 
    static const uint32_t CyclesPerMillisecond = CPU_TIMER_HZ / 1'000;
    uint64_t cycles = rdcycle64();  // Hidden in riscvreg.h and included through thread.h
    uint32_t msCount = static_cast<uint32_t>(cycles / CyclesPerMillisecond);    // Driver is not bothered by it wrapping (apprently).
    return msCount;
}

//=============================================================================
// MCP251XFD_X compute CRC16-CMS
//=============================================================================
uint16_t ComputeCRC16_Sonata(const uint8_t* data, size_t size)
{
    // Sonata
    // From the MCP2518FD datasheet: 
    // The MCP2518FD device uses the following generator polynomial: CRC-16/USB (0x8005). CRC-16 detects
    // all single and double-bit errors, all errors with an odd number of bits, all burst errors of length 16 or less, and
    // most errors for longer bursts. This allows an excellent detection of SPI communication errors that can happen in the system, and heavily reduces the risk of
    // miscommunication, even under noisy environments.
    // The best way to implement this will be with a prepared crc_table.
    // 
    // Written - 2025-02-03
    // Tested - 
    return ComputeCRC16CMS(data, size);
}

//*******************************************************************************************************************
//=============================================================================
// MCP251XFD SPI driver interface configuration for the Sonata
//=============================================================================
eERRORRESULT MCP251XFD_InterfaceInit_Sonata(void *pIntDev, uint8_t chipSelect, const uint32_t SckFreq)
{
    // Initialises the hardware interface.
    // Written - 2025-01-21
    // Tested - 

    if (pIntDev == NULL) {
        return ERR__SPI_PARAMETER_ERROR;
    }
    // Get our data.
    Spi_Config_t* cfg = static_cast<Spi_Config_t*>(pIntDev);
    // Configure the IO the chosen SPI
    auto pinSinks = SonataPinmux::PinSinks();
	auto blockSinks = SonataPinmux::BlockSinks();

    uint8_t sel = 0;
    // SLCK
    if(false == pinSinks.get(cfg->sclk).select(cfg->sclk_sel)) {
            return ERR__SPI_PARAMETER_ERROR;
    }
    // COPI
    if(false == pinSinks.get(cfg->copi).select(cfg->copi_sel)) {
        return ERR__SPI_PARAMETER_ERROR;
    }
    // CS0
    if(cfg->cs0_sel > 0) {
        if(false == pinSinks.get(cfg->cs0).select(cfg->cs0_sel)) {
            return ERR__SPI_PARAMETER_ERROR;
        }
    }
    // CS1
    if(cfg->cs1_sel > 0) {
        if(false == pinSinks.get(cfg->cs1).select(cfg->cs1_sel)) {
            return ERR__SPI_PARAMETER_ERROR;
        }
    }
    // CS2
    if(cfg->cs2_sel > 0) {
        if(false == pinSinks.get(cfg->cs2).select(cfg->cs2_sel)) {
            return ERR__SPI_PARAMETER_ERROR;
        }
    }
    // CIPO
    if(false == blockSinks.get(cfg->cipo_sel).select(cfg->cipo)) {
        return ERR__SPI_PARAMETER_ERROR;
	}
        
    //--- Configure an SPI peripheral ---
    cfg->spi->init(
		false,	// Clock Polarity = 0
	    false,	// Clock Phasse = 0
	    true,	// MSB first = true
	    SPI_CLOCK_SPEED_SETTING);	// The settings is the length of a half period of the SPI clock, measured in system clock cycles reduced by 1.

    return ERR_OK;
}

//=============================================================================
// MCP251XFD SPI transfer data for the Sonata
//=============================================================================
eERRORRESULT MCP251XFD_InterfaceTransfer_Sonata(void *pIntDev, uint8_t chipSelect, uint8_t *txData, uint8_t *rxData, size_t size)
{
    // Perform an SPI transfer.
    // Written - 2025-01-21
    // Tested - 

    if (pIntDev == NULL) {
        return ERR__SPI_PARAMETER_ERROR;
    }
    if (txData == NULL) {
        return ERR__SPI_PARAMETER_ERROR;
    }
    Spi_Config_t* cfg = static_cast<Spi_Config_t*>(pIntDev);
    
    // Debug::log("{}() {}: txData = {}", __FUNCTION__, __LINE__, txData);
    // Debug::log("{}() {}: rxData = {}", __FUNCTION__, __LINE__, rxData);
    // Debug::log("{}() {}:   size = {}", __FUNCTION__, __LINE__, size);
    // if(txData != NULL) Debug::log("{}() {}: txData[]: {}, {}", __FUNCTION__, __LINE__, txData[0], txData[1]);
    // if(rxData != NULL) Debug::log("{}() {}: rxData[]: {}, {}", __FUNCTION__, __LINE__, rxData[0], rxData[1]);

    cfg->spi->chip_select_assert<false>(chipSelect, true);   // Chip Select Low
    cfg->spi->blocking_transfer(txData, rxData, size);
    cfg->spi->wait_idle();	// Wait for the Rx to finish (shouldn't be needed).
    cfg->spi->chip_select_assert<false>(chipSelect, false);   // Chip Select High

    return ERR_OK;
}