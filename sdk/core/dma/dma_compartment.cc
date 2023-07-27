#define MALLOC_QUOTA 0x100000

#include "dma_compartment.hh"
#include <cstdint>
#include <memory>

#include <cheri.hh>
#include <compartment-macros.h>
#include <utils.hh>
#include "platform-dma.hh"

// Import some useful things from the CHERI namespace.
using namespace CHERI;

Ibex::PlatformDMA platformDma;

void free_dma(uint32_t *sourceAddress, uint32_t *targetAddress)
{
    int sourceStatus = heap_free(MALLOC_CAPABILITY, sourceAddress);
    int targetStatus = heap_free(MALLOC_CAPABILITY, targetAddress);

    /**
     *  ideally, free should not fail if claim was successful
     *  todo: but in case, assert a Debug::Assert() for heap_free() later
     */    
}

int launch_dma(uint32_t *sourceAddress, uint32_t *targetAddress, uint32_t lengthInBytes,
                        uint32_t sourceStrides, uint32_t targetStrides, uint32_t byteSwapAmount)
{    

    /**
     *  claim the memory with default malloc capability,
     *  as declaring another heap capability is an extra entry 
     *  that leaves the default capability let unused.
     *
     *  unique pointers are used to avoid explicit heap_free() calls.
     *  when this pointer is not used anymore, it is automatically deleted
     */
    
    std::unique_ptr<uint32_t> uniqueSourceAddress(sourceAddress);
    std::unique_ptr<uint32_t> uniqueTargetAddress(targetAddress);

    size_t sourceClaimSize = heap_claim(MALLOC_CAPABILITY, static_cast<void*>(uniqueSourceAddress.get()));

    /**
     *  return with failure if 
     *  no claim is available 
     */

    if (sourceClaimSize == 0) 
    {
        return -1;
    } 

    size_t targetClaimSize = heap_claim(MALLOC_CAPABILITY, static_cast<void*>(uniqueTargetAddress.get()));

    if (targetClaimSize == 0) 
    {   
        /**
         *  if the first claim was successful, but this one failed,
         *  then free the first claim and exit the function
         *  todo: assert a Debug::Assert() for heap_free() result later
         */
        int sourceStatus = heap_free(MALLOC_CAPABILITY, sourceAddress);

        return -1;
    } 

    /**
     *  return if sufficient permissions are not present 
     *  and if not long enough 
     */

    if (!check_pointer<PermissionSet{Permission::Load, Permission::Global}>(sourceAddress, lengthInBytes) ||
           !check_pointer<PermissionSet{Permission::Store, Permission::Global}>(targetAddress, lengthInBytes) )
    {
        return -1;
    }

    platformDma.write_conf_and_start(sourceAddress, targetAddress, lengthInBytes, 
                                        sourceStrides, targetStrides, byteSwapAmount);         

    // todo: need to check for status via polling maybe from here
    // or for the mvp, we can just check or cancel after some timeout

    // todo: eventually we need some interrupt support and the futex call here
    // todo: eventually, we wanna separate this free and reset dma 
    // logics from the start dma as well    
    
    // free_dma(sourceAddress, targetAddress);

    platformDma.reset_dma();

    /**
     *  return here, if both claims are successful 
     */
    return 0;

}
