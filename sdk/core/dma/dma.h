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
	concept IsDmaDevice = requires(T device, uint32_t *sourceAddress, uint32_t *targetAddress, size_t lengthInBytes,
                                        uint32_t sourceStrides, uint32_t targetStrides, uint32_t byteSwapAmount)
	{
		{device.init()};
		{
			device.write_conf(sourceAddress, destinationAddress, lengthInBytes)
			} -> std::same_as<void>;
		{
			device.start_dma()
			} -> std::same_as<void>;
        {
			device.reset_dma(sourceAddress, destinationAddress)
			} -> std::same_as<void>;
	};

    template<template<typename, size_t>
	         typename PlatformDMA>
	requires IsDmaDevice<PlatformDMA<WordT, TCMBaseAddr>>
	class GenericDMA : public PlatformDMA<WordT, TCMBaseAddr>
	{
		public:

        int configure_and_launch_dma(uint32_t *sourceAddress, uint32_t *targetAddress, uint32_t lengthInBytes,
                        uint32_t sourceStrides, uint32_t targetStrides, uint32_t byteSwapAmount)
        {
            return dma_compartment(sourceAddress, targetAddress, lengthInBytes,
                        sourceStrides, targetStrides, byteSwapAmount)
        }

	};

    using DMA = GenericDMA<uint32_t, OtherInformation, PlatformDMA>;
}

