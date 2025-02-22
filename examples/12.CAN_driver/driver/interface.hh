#pragma once
/*!*****************************************************************************
 * @file    Interface.hh
 * @details
 * The Sonata specific files required for the MCP251XFD driver.
 * This is designed to be able to switch multple CAN interfaces so we need to
 * define the chip select lines to do this correctly.
 * Chip Select   Pin
 *      0        RPi Header pin 24 (SPI0_CE0)
 *      1        RPi Header pin 26 (SPI0_CE1)
 ******************************************************************************/

// #ifndef MCP251XFD_H_INC
// #define MCP251XFD_H_INC


#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
#include <platform/sunburst/platform-spi.hh>
#include <platform/sunburst/platform-gpio.hh>
#include <platform/sunburst/platform-pinmux.hh>

struct Spi_Config {
    volatile SonataSpi::Generic<> *spi;
    uint8_t spi_num;
    SonataPinmux::PinSink cs0;    // Setting Chip Select 0. Setting this to ser0_tx (0) disables this output.
    uint8_t cs0_sel;
    SonataPinmux::PinSink cs1;    // Setting Chip Select 1. Setting this to ser0_tx (0) disables this output.
    uint8_t cs1_sel;
    SonataPinmux::PinSink cs2;    // Setting Chip Select 2. Setting this to ser0_tx (0) disables this output.
    uint8_t cs2_sel;
    SonataPinmux::PinSink sclk;   // Setting the clock output. MUST be a valid output to work.
    uint8_t sclk_sel;
    SonataPinmux::PinSink copi;   // Setting the data output. Setting this to ser0_tx (0) disables this output.
    uint8_t copi_sel;
    uint8_t cipo;                   // Setting the data input. Setting to 0 disables.
    SonataPinmux::BlockSink cipo_sel;
};
typedef struct Spi_Config Spi_Config_t;

/*! @brief Sonata configure SPI set-up function.
*
* This fills in the Spi_Config which will then be set in MCP251XFD->InterfaceDevice. 
* @param[out] *cfg We will fill this in with the required settings.
* @param[in] spi_num Is the SPI number. It MUST be 1 or 2 only.
* @param[in] cs0 Which pin (from SonataPinmux::PinSink) to use as CS0. Set to a valid option or will return an error. If it's not being used then set it to SonataPinmux::PinSink::ser0_tx
* @param[in] cs1 Which pin (from SonataPinmux::PinSink) to use as CS1. Set to a valid option or will return an error. If it's not being used then set it to SonataPinmux::PinSink::ser0_tx
* @param[in] cs2 Which pin (from SonataPinmux::PinSink) to use as CS2. Set to a valid option or will return an error. If it's not being used then set it to SonataPinmux::PinSink::ser0_tx
* @param[in] sclk Which pin (from SonataPinmux::PinSink) to use as SPI SCLK. Set to a valid option or will return an error.
* @param[in] copi Which pin (from SonataPinmux::PinSink) to use as SPI COPI (MOSI). Set to a valid option or will return an error.
* @param[in] cipo Which pin to use as SPI CIPO. Set to a valid option or will return an error. Must be 1 to 3, inclusive. See the pinmux documentation for the meaning of the input block to set here.
* @return Returns an #eERRORRESULT value enum
*/
eERRORRESULT GetSpiConfig(Spi_Config* cfg, uint8_t spi_num, SonataPinmux::PinSink cs0, SonataPinmux::PinSink cs1, SonataPinmux::PinSink cs2, SonataPinmux::PinSink sclk, SonataPinmux::PinSink copi, uint8_t cipo);

/*! @brief MCP251XFD_X get millisecond
*
* This function will be called when the driver needs to get current millisecond
*/
uint32_t GetCurrentms_Sonata(void);

/*! @brief MCP251XFD_X compute CRC16-CMS
*
* This function will be called when a CRC16-CMS computation is needed (ie. in CRC mode or Safe Write). In normal
mode, this can point to NULL.
* @param[in] *data Is the pointed byte stream
* @param[in] size Is the size of the pointed byte stream
* @return The CRC computed
*/
uint16_t ComputeCRC16_Sonata(const uint8_t* data, size_t size);

//*******************************************************************************************************************
/*! @brief MCP251XFD_Ext1 SPI interface configuration for the Sonata
*
* This function will be called at driver initialization to configure the interface driver SPI
* @param[in] *pIntDev Is the MCP251XFD_Desc.InterfaceDevice of the device that call the interface initialization
* @param[in] chipSelect Is the Chip Select index to use for the SPI initialization
* @param[in] sckFreq Is the SCK frequency in Hz to set at the interface initialization
* @return Returns an #eERRORRESULT value enum
*/
eERRORRESULT MCP251XFD_InterfaceInit_Sonata(void *pIntDev, uint8_t chipSelect, const uint32_t sckFreq);

/*! @brief MCP251XFD_Ext1 SPI transfer for the Sonata
*
* This function will be called at driver read/write data from/to the interface driver SPI
* @param[in] *pIntDev Is the MCP251XFD_Desc.InterfaceDevice of the device that call this function
* @param[in] chipSelect Is the Chip Select index to use for the SPI transfer
* @param[in] *txData Is the buffer to be transmit to through the SPI interface
* @param[out] *rxData Is the buffer to be received to through the SPI interface (can be NULL if it's just a send of
data)
* @param[in] size Is the size of data to be send and received trough SPI. txData and rxData shall be at least the
same size
* @return Returns an #eERRORRESULT value enum
*/
eERRORRESULT MCP251XFD_InterfaceTransfer_Sonata(void *pIntDev, uint8_t chipSelect, uint8_t *txData, uint8_t *rxData,
size_t size);

//-----------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif // __cplusplus
//-----------------------------------------------------------------------------
// #endif /* INTERFACE_H_INC */