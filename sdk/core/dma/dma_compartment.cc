#pragma once

#define MALLOC_QUOTA 0x100000

#include <cheri.hh>
#include <compartment-macros.h>
#include <utils.hh>

#include <thread_pool.h>

using thread_pool::async;
/**
 *  creating a global heap below.
 *  todo: we assume that this heap capability can serve 
 *  for different compartments, irrespective of 
 *  the origin because this is a driver code?
*/
DECLARE_AND_DEFINE_ALLOCATOR_CAPABILITY(dmaDriverHeap, 8144);

SObjStruct *dmaHeap;

int claim_dma(uint32_t *sourceAddress, uint32_t *targetAddress)
{    
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

    /**
     *  return here, if both claims are successful 
     */
    return 0;

}

void free_dma(uint32_t *sourceAddress, uint32_t *targetAddress)
{
    int sourceStatus = heap_free(dmaHeap, sourceAddress);
    int targetStatus = heap_free(dmaHeap, targetAddress);

    /**
     *  ideally, free should not fail if claim was successful
     *  todo: but in case, assert a Debug::Assert() for heap_free() later
     */    
}