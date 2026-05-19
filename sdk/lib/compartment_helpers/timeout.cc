#include <debug.hh>
#include <timeout.h>

bool timeout_is_valid(TimeoutArgument timeout)
{
	return !timeout.is_relative() ||
	       check_timeout_pointer(timeout.relativeTimeout);
}

bool timeout_has_expired(TimeoutArgument timeout)
{
	if (timeout_is_relative(timeout))
	{
		return timeout.relativeTimeout->remaining == 0;
	}
	return timeout.absoluteTimeout < platform_monotonic_time_read();
}

void timeout_elapse(TimeoutArgument timeout, Ticks ticks)
{
	if (timeout_is_relative(timeout))
	{
		timeout.relativeTimeout->elapse(ticks);
	}
}
