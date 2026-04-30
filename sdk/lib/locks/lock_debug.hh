#include <debug.hh>

namespace
{
	constexpr bool DebugLocks =
#ifdef DEBUG_LOCKS
	  DEBUG_LOCKS
#else
	  false
#endif
	  ;
	using Debug = ConditionalDebug<DebugLocks, "Locking">;
} // namespace

