#define MALLOC_QUOTA 0x100000

#include "dma_compartment.hh"
#include <cstdint>
#include <memory>
#include <debug.hh>

#include <cheri.hh>
#include <compartment-macros.h>
#include <utils.hh>
#include "platform-dma.hh"
#include <errno.h>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Simple DMA request compartment">;

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
    
    auto claim = [](void  *ptr) -> std::unique_ptr<char>
    {
        if (heap_claim(MALLOC_CAPABILITY, ptr) == 0)
        {
            return {nullptr};
        }
        
        return std::unique_ptr<char> { static_cast<char*>(ptr)};
    };
    
    auto claimedSource = claim(sourceAddress);
    auto claimedDestination = claim(targetAddress);
   
    if (!claimedSource || !claimedDestination)
    {
        return -EINVAL;
    }

    Debug::log("after claim check");

    /**
     *  return if sufficient permissions are not present 
     *  and if not long enough 
     */

    if (!check_pointer<PermissionSet{Permission::Load, Permission::Global}>(sourceAddress, lengthInBytes) ||
           !check_pointer<PermissionSet{Permission::Store, Permission::Global}>(targetAddress, lengthInBytes) )
    {
        return -1;
    }

    Debug::log("after right check");

    platformDma.write_conf_and_start(sourceAddress, targetAddress, lengthInBytes, 
                                        sourceStrides, targetStrides, byteSwapAmount);         

    Debug::log("after conf write");

    /**
     *  return here, if both claims are successful 
     */
    return 0;

}

int reset_and_clear_dma(uint32_t interruptStatus)
{
    // todo: eventually we need some interrupt support and the futex call here
    // todo: eventually, we wanna separate this free and reset dma 
    // logics from the start dma as well    
    
    // free_dma(sourceAddress, targetAddress);

    if (interruptStatus)
    {
        platformDma.reset_dma();
        Debug::log("after reset write");

        return 0;
    } 
   
    return -EINVAL;
}
