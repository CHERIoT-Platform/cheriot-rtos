// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <queue.h>
#include <ring_buffer.hh>
#include <thread.h>
#include <thread_pool.h>

using namespace CHERI;

namespace
{

	/**
	 * Ring buffer.  We use a ticket lock to ensure that things are dispatched
	 * in order (independent of priority) on the sending side and a flag lock on
	 * the other side to ensure that the highest-priority thread picks up
	 * messages as soon as they're ready.
	 */
	RingBuffer<ThreadPoolMessage, 16, TicketLock, FlagLock> queue;

} // namespace

int thread_pool_async(ThreadPoolCallback fn, CHERI_SEALED(void *) data)
{
	Capability<void>       fnCap{reinterpret_cast<void *>(fn)};
	Capability<void, true> dataCap{data};
	// The function must be sealed with the type used for export table entries
	// for us to be able to invoke it.  The data capability doesn't *have* to
	// be sealed, but it's a bad idea if it is unsealed because it adds the
	// thread pool to the TCB for confidentiality and integrity.
	// We want to avoid this being able to make us trap and so we validate that
	// the function is cross-compartment entry point and both can be stored in
	// the message queue.
	if (!fnCap.is_valid() || (fnCap.type() != 9) ||
	    !fnCap.permissions().contains(Permission::Global) ||
	    (dataCap.is_valid() && !dataCap.is_sealed()) ||
	    (dataCap.is_valid() &&
	     !dataCap.permissions().contains(Permission::Global)))
	{
		return -EINVAL;
	}

	queue.push({fn, data});

	return 0;
}

int __cheri_compartment("thread_pool") thread_pool_run()
{
	while (true)
	{
		ThreadPoolMessage message = queue.pop();
		message.invoke(message.data);
	}
}
