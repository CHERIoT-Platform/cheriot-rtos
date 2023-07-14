#pragma once

#include "dma_compartment.cc"
#include "dma_compartment.h"
#include "platform/ibex/platform-dma.hh"
#include <concepts>
#include <stdint.h>
#include <utils.hh>

#if __has_include(<platform-dma.hh>)
#	include <platform-dma.hh>
#endif

namespace DMA 
{
    template<typename T>
	concept IsDmaDevice = requires(T device)
	{
		{device.init()};
		{
			device.write_conf()
			} -> std::same_as<void>;
		{
			device.start_dma()
			} -> std::same_as<void>;
        {
			device.reset_dma()
			} -> std::same_as<void>;
	};

    template<template<typename, size_t>
	         typename PlatformDMA>
	requires IsDmaDevice<PlatformDMA<WordT, TCMBaseAddr>>
	class GenericDMA : public PlatformDMA<WordT, TCMBaseAddr>
	{
		public:

		/**
		 * Initialise a DMA device.
		 */

		void init()
		{
			PlatformDMA<WordT, TCMBaseAddr>::init();
		}

        void write_conf(uint32_t sourceAddress, uint32_t targetAddress, uint32_t lengthInBytes)
        {
            /**
		     * Claim the DMA-able addresses first.
             * If successful, then call an original 
             * dma write configuration function
		     */

            int claimStatus = claim_dma(sourceAddress, targetAddress);

            if (claimStatus == 0)
            {
                PlatformDMA::write_conf(sourceAddress, targetAddress, lengthInBytes);
            }
            
        }

        void reset_dma(uint32_t sourceAddress, uint32_t targetAddress)
        {
            free_dma(sourceAddress, targetAddress);

            PlatformDMA::reset_dma();
        }

	};

    using DMA = GenericDMA<uint32_t, REVOKABLE_MEMORY_START, PlatformDMA>;
}