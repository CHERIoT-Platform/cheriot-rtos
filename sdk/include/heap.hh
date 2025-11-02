#pragma once
#include "cheri.hh"
#include "stdlib.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static inline CHERI::ErrorOr<void>
heap_allocate_cpp(Timeout            *timeout,
                  AllocatorCapability heapCapability,
                  size_t              size,
                  uint32_t            flags = AllocateWaitAny)
{
	return {heap_allocate(timeout, heapCapability, size, flags)};
}

static inline CHERI::ErrorOr<void>
heap_allocate_array_cpp(Timeout            *timeout,
                        AllocatorCapability heapCapability,
                        size_t              nmemb,
                        size_t              size,
                        uint32_t            flags = AllocateWaitAny)
{
	return {heap_allocate_array(timeout, heapCapability, nmemb, size, flags)};
}
#pragma clang diagnostic pop
