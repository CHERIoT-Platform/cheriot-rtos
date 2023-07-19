#pragma once

#include <cdefs.h>
#include <stdint.h>

/**
 * A function below claims the source and target addresses of the DMA interface.
 * While, DMA is in progress, these addresses will be claimed by the DMA compartment 
 * and so the memory will not be freed.
 */
__cheri_compartment("dma") int dma_compartment(uint32_t *sourceAddress, uint32_t *targetAddress, uint32_t lengthInBytes,
                                                    uint32_t sourceStrides, uint32_t targetStrides, uint32_t byteSwapAmount);

