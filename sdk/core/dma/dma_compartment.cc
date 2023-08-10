#include "futex.h"
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
#include <interrupt.h>
#include <locks.hh>

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "DMA Compartment">;

// Import some useful things from the CHERI namespace.
using namespace CHERI;

Ibex::PlatformDMA platformDma;

DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(dmaInterruptCapability, dma, true, true);

namespace {
    /**
     * Flag lock to control ownership 
	 * over the dma controller
     */
	FlagLock dmaOwnershipLock;

    uint32_t dmaIsLaunched = 0;
    uint32_t previousInterruptCounter = 0;

    /**
     * Claims pointers so that the following 
     * addresses will not be freed ahead of time
     */
    std::unique_ptr<char> claimedSource;
    std::unique_ptr<char> claimedDestination;

} // namespace

int launch_dma(uint32_t *sourceAddress, uint32_t *targetAddress, uint32_t lengthInBytes,
                        uint32_t sourceStrides, uint32_t targetStrides, uint32_t byteSwapAmount)
{    

	/** 
     *  Before launching a dma, check whether dma
	 *  is occupied by another thread, 
     *  else lock the ownership and start the dma
     *  via LockGuard.
     * 
     *  This lock automatically unlocks at the end of this function.
     */	
	LockGuard g{dmaOwnershipLock};

    /**
     *  If dma is already launched, we need to check for the interrupt status.
	 *  No need for validity and permissions checks though for the scheduler 
	 *  once futex is created   
     */
    
	const uint32_t *dmaFutex = interrupt_futex_get(STATIC_SEALED_VALUE(dmaInterruptCapability));
    uint32_t currentInterruptCounter = *dmaFutex;
    
    /** 
     * sleep a bit, so that dmaIsLaunched can be flipped successfully
     * in the worst case, the caller thread will fail too
     */

    if (dmaIsLaunched)
    {
        /**
         *  If dma is already running and interrupt has not been received yet, 
         *  return the negative incremented interrupt value,
         *  so that it can wait for timeout via passing this value  
         */
        if (previousInterruptCounter == currentInterruptCounter) 
        {
            return -(currentInterruptCounter + 1);
        } 

        /*
         *  Potentially, redundant futex wait and context switch
         *  todo: do we wanna remove it and let the thread pass?
         */
        uint32_t *dmaIsLaunchedPtr = &dmaIsLaunched;
        futex_wait(dmaIsLaunchedPtr, dmaIsLaunched);
        
    } 

    /**
     *  Designate that the dmaIsLaunched,
     *  if the thread accessed the dma first
     */
    dmaIsLaunched = 1;

    /**
     *  After acquiring a ownership over lock,
	 *  now it is the time to launch dma.
     * 
     *  Here, we claim the memory with default malloc capability,
     *  as declaring another heap capability is an extra entry 
     *  that leaves the default capability let unused.
     *
     *  Unique pointers are used to avoid explicit heap_free() calls.
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
     *  return here, if all operations 
     *  were successful.
     *  Save the current interrupt counter to the previous one
     *  to differentiate betweent the start and end afterwards 
     */
    
    previousInterruptCounter = currentInterruptCounter;
    return currentInterruptCounter;;

}

void reset_and_clear_dma(uint32_t interruptNumber)
{   
    /**
     *  Resetting the claim pointers 
     *  and cleaning up the dma registers.
     *
     *  Claim registers are meant to be 
     *  cleared by every DMA operation,
     *  that is why we are explicitely resetting 
     *  them at this exit function.
     */

    /**
     *  Flip the dmaIsRunnning value to show that launch is finished,
     *  in case finishing thread wakes from the interrupt first 
     */
    dmaIsLaunched = 0;
    
    /**
     *  sleep until fired interrupt is fired with futex_wait
     */

    Debug::log("before dropping claims");

    claimedSource.reset();
    claimedDestination.reset();

    /**
     *  Resetting the dma registers
     */
    platformDma.reset_dma();

    /**
     *  Acknowledging interrupt here irrespective of the reset status
     */
    interrupt_complete(STATIC_SEALED_VALUE(dmaInterruptCapability));

    

}
