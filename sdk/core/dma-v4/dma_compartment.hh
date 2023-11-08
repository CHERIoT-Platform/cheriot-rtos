#pragma once

#include <cdefs.h>
#include <stdint.h>

#include "platform-dma.hh"

using namespace Ibex;

/**
 * A function below claims the source and target addresses of the DMA interface.
 * While, DMA is in progress, these addresses will be claimed by the DMA
 * compartment and so the memory will not be freed.
 */

int __cheri_compartment("dma") launch_dma(DMADescriptor *dmaDescriptorPointer);

int __cheri_compartment("dma")
  wait_and_reset_dma(uint32_t       interruptNumber,
                     DMADescriptor *originalDescriptorPointer);

void __cheri_compartment("dma")
  force_stop_dma(DMADescriptor *originalDescriptorPointer);