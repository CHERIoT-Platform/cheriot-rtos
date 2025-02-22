/*******************************************************************************
    File name:    CRC16_CMS.h
    Author:       FMA
    Version:      1.0
    Date (d/m/y): 14/05/2020
    Description:  CRC16-CMS implementation
                  The CRC16-CMS polynomial is x^16 + x^15 + x^2 + 1 (0x8005)
                  Does not use RefIN and RefOUT, the initial value 0xFFFF
                  The result is XORed with 0x0000
                  http://reveng.sourceforge.net/crc-catalogue/16.htm

    History :
*******************************************************************************/
#ifndef CRC16_CMS_H_INC
#define CRC16_CMS_H_INC
//=============================================================================

//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
//-----------------------------------------------------------------------------
/// @cond 0
/**INDENT-OFF**/
#ifdef __cplusplus
extern "C" {
#endif
/**INDENT-ON**/
/// @endcond
//-----------------------------------------------------------------------------





#ifndef CRC16CMS_NOTABLE
/*! @brief Compute a byte stream with CRC16-CMS with a table
 *
 * @param[in] *data Is the pointed byte stream
 * @param[in] size Is the size of the pointed byte stream
 * @return The CRC computed
 */
uint16_t ComputeCRC16CMS(const uint8_t* data, uint32_t size);

#else

/*! @brief Compute a byte stream with CRC16-CMS without a table
 *
 * @param[in] *data Is the pointed byte stream
 * @param[in] size Is the size of the pointed byte stream
 * @return The CRC computed
 */
uint16_t ComputeCRC16CMS(const uint8_t* data, uint16_t size);

#endif





//-----------------------------------------------------------------------------
/// @cond 0
/**INDENT-OFF**/
#ifdef __cplusplus
}
#endif
/**INDENT-ON**/
/// @endcond
//-----------------------------------------------------------------------------
#endif /* CRC16_CMS_H_INC */