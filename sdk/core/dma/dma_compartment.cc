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

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "DMA Compartment">;

// Import some useful things from the CHERI namespace.
using namespace CHERI;

Ibex::PlatformDMA platformDma;

namespace {

    std::unique_ptr<char> claimedSource;
    std::unique_ptr<char> claimedDestination;

} // namespace

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
    
    claimedSource = claim(sourceAddress);
    claimedDestination = claim(targetAddress);
   
    if (!claimedSource || !claimedDestination)
    {
        return -EINVAL;
    }

    /**
     *  return if sufficient permissions are not present 
     *  and if not long enough 
     */

    if (!check_pointer<PermissionSet{Permission::Load, Permission::Global}>(sourceAddress, lengthInBytes) ||
           !check_pointer<PermissionSet{Permission::Store, Permission::Global}>(targetAddress, lengthInBytes) )
    {
        return -EINVAL;
    }

    platformDma.write_conf_and_start(sourceAddress, targetAddress, lengthInBytes, 
                                        sourceStrides, targetStrides, byteSwapAmount);         

    /**
     *  return here, if both claims are successful 
     */
    return 0;

}

void reset_and_clear_dma()
{   
    // Resetting the claim pointers 
    // and cleaning up the dma registers.
    
    // Claim registers are meant to be 
    // cleared by every DMA operation,
    // that is why we are explicitely resetting 
    // them at this exit function.
    
    Debug::log("before dropping claims");

    claimedSource.reset();
    claimedDestination.reset();

    platformDma.reset_dma();

}
