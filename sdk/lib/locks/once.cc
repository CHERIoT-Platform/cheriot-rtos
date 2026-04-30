#include "lock_debug.hh"
#include <locks.h>

int __cheriot_libcall run_once_slow_path(struct OnceState *state,
                                         void (*callback)())
{
	auto expected = OnceStateNotRun;
	// Try to transition from not-run to running.  If this happens, we have an
	// exclusive lock on the once state and can call the callback.
	if (state->state.compare_exchange_strong(expected, OnceStateRunning))
	{
		callback();
		state->state = OnceStateRun;
		state->state.notify_all();
		return 0;
	}
	// If we lost the race and the winner might not have finished, wait (skip
	// the wait if we lost the race and the winner finished already).
	if (expected == OnceStateRunning)
	{
		state->state.wait(expected);
	}
	Debug::Assert(state->state == OnceStateRun,
	              "Invalid state for once guard: {}",
	              state->state.load());
	return 0;
}
