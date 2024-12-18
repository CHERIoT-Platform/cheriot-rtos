#include <debug.hh>
#include <fail-simulator-on-error.h>

using Debug = ConditionalDebug<true, "top">;

#if __has_include(<platform-hardware_revoker.hh>)
#	include <platform-hardware_revoker.hh>
#else
#	error No platform-hardware_revoker.hh found, are you building for the right platform?
#endif

using Revoker = HardwareRevoker<uint32_t, REVOKABLE_MEMORY_START>;
static_assert(Revoker::IsAsynchronous, "This test is for async revokers");

void __cheri_compartment("top") entry()
{
	Revoker r{};
	r.init();

	uint32_t epoch = r.system_epoch_get();
	Debug::log("At startup, revocation epoch is {}; waiting...", epoch);

	// Just in case a revocation is somehow active...
	epoch &= ~1;
	r.system_bg_revoker_kick();

	for (int i = 0; i < 10; i++)
	{
		bool     res;
		uint32_t newepoch;
		Timeout  t{50};

		res      = r.wait_for_completion(&t, (epoch & ~1) + 2);
		newepoch = r.system_epoch_get();

		Debug::log("After wait: for {}, result {}, epoch now is {}, "
		           "wait elapsed {} remaining {}",
		           epoch,
		           res,
		           newepoch,
		           t.elapsed,
		           t.remaining);

		Debug::Assert(t.remaining > 0,
		              "Timed out waiting for revoker to advance");

		if (res)
		{
			epoch = newepoch;
			r.system_bg_revoker_kick();
		}
	}
}
