#include <debug.hh>
#include <fail-simulator-on-error.h>

using Debug = ConditionalDebug<true, "top">;

#if __has_include(<platform-hardware_revoker.hh>)
#	include <platform-hardware_revoker.hh>

using Revoker = HardwareRevoker<uint32_t, REVOKABLE_MEMORY_START>;

#elif defined(CLANG_TIDY)

struct Revoker
{
	static constexpr bool IsAsynchronous = true;

	uint32_t system_epoch_get()
	{
		return 0;
	}
	int wait_for_completion(Timeout *, uint32_t)
	{
		return 0;
	}
	void system_bg_revoker_kick() {}
	void init() {}
};

#else
#	error No platform-hardware_revoker.hh found, are you building for the right platform?
#endif

static_assert(Revoker::IsAsynchronous, "This test is for async revokers");

void __cheri_compartment("top") entry()
{
	Revoker r{};
	r.init();

	uint32_t epoch = r.system_epoch_get();
	Debug::log("At startup, revocation epoch is {}; waiting...", epoch);

	r.system_bg_revoker_kick();

	for (int i = 0; i < 10; i++)
	{
		bool     res;
		uint32_t newepoch;
		Timeout  t{50};

		res      = r.wait_for_completion(&t, epoch & ~1);
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
