#pragma once

#define MALLOC_QUOTA 0x100000

#include <cheri.hh>
#include <compartment-macros.h>
#include <utils.hh>
#include <thread_pool.h>

#if __has_include(<platform-dma.hh>)
#	include <platform-dma.hh>
#endif

// Import some useful things from the CHERI namespace.
using namespace CHERI;

using thread_pool::async;
/**
 *  creating a global heap below.
 *  todo: we assume that this heap capability can serve 
 *  for different compartments, irrespective of 
 *  the origin because this is a driver code?
*/
DECLARE_AND_DEFINE_ALLOCATOR_CAPABILITY(dmaDriverHeap, 8144);

SObjStruct *dmaHeap;

Ibex::PlatformDMA platformDma;

void free_dma(uint32_t *sourceAddress, uint32_t *targetAddress)
{
    int sourceStatus = heap_free(dmaHeap, sourceAddress);
    int targetStatus = heap_free(dmaHeap, targetAddress);

    /**
     *  ideally, free should not fail if claim was successful
     *  todo: but in case, assert a Debug::Assert() for heap_free() later
     */    
}

int dma_compartment(uint32_t *sourceAddress, uint32_t *targetAddress, uint32_t lengthInBytes,
                        uint32_t sourceStrides, uint32_t targetStrides, uint32_t byteSwapAmount)
{    
    /**
     *  return if sufficient permissions are not present 
     *  and if not long enough 
     */

    if (!check_pointer<PermissionSet{Permission::Load, Permission::Global}>(sourceAddress, lengthInBytes) ||
           !check_pointer<PermissionSet{Permission::Store, Permission::Global}>(targetAddress, lengthInBytes) )
    {
        return -1;
    }

    /**
     *  create a heap capability for dma below. 
     */
    dmaHeap = STATIC_SEALED_VALUE(dmaDriverHeap);

    /**
     *  claim the memory 
     */
    
    size_t sourceClaimSize = heap_claim(dmaHeap, sourceAddress);

    /**
     *  return with failure if 
     *  no claim is available 
     */

    if (sourceClaimSize == 0) 
    {
        return -1;
    } 

    size_t targetClaimSize = heap_claim(dmaHeap, targetAddress);

    if (targetClaimSize == 0) 
    {   
        /**
         *  if the first claim was successful, but this one failed,
         *  then free the first claim and exit the function
         *  todo: assert a Debug::Assert() for heap_free() result later
         */
        int sourceStatus = heap_free(dmaHeap, sourceAddress);

        return -1;
    } 

    platformDma::write_strides(sourceStrides, targetStrides);
    platformDma::byte_swap_en(byteSwapAmount);
        
    platformDma::start_dma();            

    // todo: need to check for status via polling maybe from here
    // or for the mvp, we can just check or cancel after some timeout

    // todo: eventually we need some interrupt support and the futex call here
    
    free_dma(sourceAddress, targetAddress);

    platformDma::reset_dma();

    /**
     *  return here, if both claims are successful 
     */
    return 0;

}
