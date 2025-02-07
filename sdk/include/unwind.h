#pragma once
#include <cdefs.h>
#include <setjmp.h>
#include <switcher.h>
#include <thread.h>

/**
 * On-stack linked list of cleanup handlers.
 */
struct CleanupList
{
	/// Next pointer.
	struct CleanupList *next;
	/// Jump buffer to return to.
	struct __jmp_buf env;
};

#include <unwind-assembly.h>

/**
 * Head of the cleanup list.
 *
 * This is stored in the space that the switcher reserves at the top of the
 * stack.  The stack is zeroed on entry to a compartment and so this will be
 * null until explicitly written to.
 */
__always_inline static inline struct CleanupList **cleanup_list_head()
{
	static const size_t InvocationLocalUnwindListOffset =
	  INVOCATION_LOCAL_UNWIND_LIST_OFFSET / sizeof(void *) - 1;
	_Static_assert(InvocationLocalUnwindListOffset == 0,
	               "unwind.h should be using invocation state slot 0");

	return (struct CleanupList **)invocation_state_slot(
	  InvocationLocalUnwindListOffset);
}

/**
 * Unwind the stack to the most recent `CHERIOT_HANDLER` block.
 */
__always_inline static inline void cleanup_unwind(void)
{
	struct CleanupList **__head = cleanup_list_head();
	struct CleanupList  *__top  = *__head;
	*__head                     = __top->next;
	switcher_handler_invocation_count_reset();
	longjmp(&__top->env, 1);
}

/**
 * Simple error handling macros.  These are modelled on the OpenStep exception
 * macros and are similarly built on top of `setjmp`.  Code between
 * `CHERIOT_DURING` and `CHERIOT_HANDLER` corresponds to a `try` block.  Code
 * between `CHERIOT_HANDLER` and `CHERIOT_END_HANDLER` corresponds to a `catch`
 * block, though no exception value is actually thrown.
 *
 * Any automatic-storage values accessed in both blocks must be declared
 * `volatile`.
 */
#define CHERIOT_DURING                                                         \
	{                                                                          \
		struct CleanupList   cleanupListEntry;                                 \
		struct CleanupList **__head = cleanup_list_head();                     \
		cleanupListEntry.next       = *__head;                                 \
		*__head                     = &cleanupListEntry;                       \
		if (setjmp(&cleanupListEntry.env) == 0)                                \
		{
/// See CHERIOT_DURING.
#define CHERIOT_HANDLER                                                        \
	*__head = cleanupListEntry.next;                                           \
	}                                                                          \
	else                                                                       \
	{                                                                          \
		*__head = cleanupListEntry.next;

/// See CHERIOT_DURING.
#define CHERIOT_END_HANDLER                                                    \
	}                                                                          \
	}

#ifdef __cplusplus

/**
 * On-error helper.  Invokes `fn` and, if `cleanup_unwind` is called, invokes
 * `err`.  Destructors in between `fn` and the frame that calls
 * `cleanup_unwind` are not called, but this function returns normally and so
 * destructors of objects above this on the stack will be called normally.
 */
static inline void on_error(auto fn, auto err)
{
	CHERIOT_DURING
	fn();
	CHERIOT_HANDLER
	err();
	CHERIOT_END_HANDLER
}

/**
 * On-error helper with no error handler (returns normally from forced unwind).
 */
static inline void on_error(auto fn)
{
	on_error(fn, []() {});
}
#endif
