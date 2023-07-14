#pragma once

#include <cdefs.h>
#include <stdint.h>

/**
 * A function below claims the source and target addresses of the DMA interface.
 * While, DMA is in progress, these addresses should not be freed
 */
__cheri_compartment("dma") int claim_dma(uint32_t *sourceAddress, 
                                                        uint32_t *targetAddress);

/**
 * The function below frees the source and target addresses of the DMA interface.
 * It should be called once the DMA operation is completed.
 */
__cheri_compartment("dma") void free_dma(uint32_t *sourceAddress, 
                                                        uint32_t *targetAddress);