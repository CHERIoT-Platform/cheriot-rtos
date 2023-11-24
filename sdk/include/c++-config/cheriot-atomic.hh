#include <atomic>

namespace cheriot
{
	// Compatibility definition of CHERIoT atomics now that we have a more
	// complete standard definition.
	template<typename T>
	using atomic = std::atomic<T>;
} // namespace cheriot
