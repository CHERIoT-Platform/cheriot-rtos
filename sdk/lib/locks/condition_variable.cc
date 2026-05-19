#include "lock_debug.hh"
#include <locks.h>

int condition_variable_notify(ConditionVariableState *conditionVariable,
                              uint32_t                waiters)
{
	conditionVariable->sequenceCounter++;
	return futex_wake(
	  reinterpret_cast<uint32_t *>(&conditionVariable->sequenceCounter),
	  waiters);
}

int condition_variable_wait(TimeoutArgument         timeout,
                            ConditionVariableState *conditionVariable,
                            void                   *mutex,
                            int (*mutexLock)(TimeoutArgument, void *),
                            int (*mutexUnlock)(void *))
{
	auto counter = conditionVariable->sequenceCounter.load();
	int  result  = mutexUnlock(mutex);
	if (result != 0)
	{
		return result;
	}
	result = conditionVariable->sequenceCounter.wait(timeout, counter);
	if (result != 0)
	{
		return result;
	}
	return mutexLock(timeout, mutex);
}
