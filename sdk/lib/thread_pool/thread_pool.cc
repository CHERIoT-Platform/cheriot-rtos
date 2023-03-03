// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <queue.h>
#include <thread_pool.h>

using namespace CHERI;

namespace
{
	/**
	 * Helper to lazily initialise a queue.
	 */
	void *get_queue()
	{
		static void *queue = []() {
			void   *q;
			Timeout t{UnlimitedTimeout};
			int     ret =
			  queue_create(&t, MALLOC_CAPABILITY, &q, 2 * sizeof(void *), 16);
			assert(ret == 0);
			return q;
		}();
		return queue;
	}

} // namespace

int thread_pool_async(ThreadPoolCallback fn, void *data)
{
	Capability<void> fnCap{reinterpret_cast<void *>(fn)};
	Capability<void> dataCap{data};
	// The function must be sealed with the type used for export table entries
	// for us to be able to invoke it.  The data capability doesn't *have* to
	// be sealed, but it's a bad idea if it is unsealed because it adds the
	// thread pool and scheduler to the TCB for confidentiality and integrity.
	// This isn't a normal pointer check because we don't actually care about
	// whether the data capability is usable, we just care that the caller is
	// not leaking information.
	if (!fnCap.is_valid() || (fnCap.type() != 9) ||
	    (dataCap.is_valid() && !dataCap.is_sealed()))
	{
		return -EINVAL;
	}

	void             *queue = get_queue();
	ThreadPoolMessage message{fn, data};
	int               ret;
	Timeout           t{UnlimitedTimeout};
	do
	{
		ret = queue_send(&t, queue, &message);
	} while (ret == -ETIMEDOUT);

	return 0;
}

void __cheri_compartment("thread_pool") thread_pool_run()
{
	void             *queue = get_queue();
	ThreadPoolMessage message;
	Timeout           t{UnlimitedTimeout};

	while (true)
	{
		// Retry until we get a message.
		while (queue_recv(&t, queue, &message) < 0)
		{
			yield();
		}
		message.invoke(message.data);
	}
}
