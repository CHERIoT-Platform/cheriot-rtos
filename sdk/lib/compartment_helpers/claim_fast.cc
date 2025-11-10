#include "cheri.hh"
#include "compartment-macros.h"
#include <allocator.h>
#include <cheriot-atomic.hh>
#include <compartment.h>
#include <errno.h>
#include <futex.h>
#include <switcher.h>

int heap_claim_ephemeral(Timeout *timeout, const void *ptr, const void *ptr2)
{
	void   **hazards = switcher_thread_hazard_slots();
	auto    *epochCounter{const_cast<
	     cheriot::atomic<uint32_t> *>(SHARED_OBJECT_WITH_PERMISSIONS(
      cheriot::atomic<uint32_t>, allocator_epoch, true, false, false, false))};
	uint32_t epoch  = epochCounter->load();
	int      values = 2;
	// Skip processing pointers that don't refer to heap memory.
	if (!heap_address_is_valid(ptr))
	{
		ptr = nullptr;
		values--;
	}
	if (!heap_address_is_valid(ptr2))
	{
		ptr2 = nullptr;
		values--;
	}
	// If neither pointer refers to heap memory, set without synchronizing.  It
	// doesn't matter if the revoker sees these or not, they cannot extend the
	// lifetime of a heap object.
	if (values == 0)
	{
		hazards[0] = nullptr;
		hazards[1] = nullptr;
		return 0;
	}
	uint32_t oldEpoch;
	do
	{
		while (epoch & 1)
		{
			if (timeout->may_block())
			{
				Timeout t{1};
				(void)futex_timed_wait(
				  &t,
				  reinterpret_cast<uint32_t *>(epochCounter),
				  epoch,
				  FutexPriorityInheritance);
				timeout->elapse(t.elapsed);
			}
			else
			{
				return -ETIMEDOUT;
			}
			epoch = epochCounter->load();
		}
		hazards[0] = const_cast<void *>(ptr);
		hazards[1] = const_cast<void *>(ptr2);
		oldEpoch   = epoch;
		epoch      = epochCounter->load();
	} while (epoch != oldEpoch);
	auto isValidOrNull = [](CHERI::Capability<const void> pointer) {
		return pointer.is_valid() || (pointer == nullptr);
	};
	if (isValidOrNull(ptr) && isValidOrNull(ptr2))
	{
		return 0;
	}
	hazards[0] = nullptr;
	hazards[1] = nullptr;
	return -EINVAL;
}
