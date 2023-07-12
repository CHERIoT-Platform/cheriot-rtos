// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <cdefs.h>

/**
 * The type of a thread-pool callback.  This is a CHERI callback so that it can
 * run in the compartment that schedule the work to run, but a different
 * thread.
 */
typedef __cheri_callback void (*ThreadPoolCallback)(void *);

/**
 * Message to a thread pool.  This encapsulates a simple closure that will be
 * invoked in the next available thread in a thread pool.
 */
struct ThreadPoolMessage
{
	/**
	 * The function pointer that will be called in the thread pool.
	 */
	ThreadPoolCallback invoke;
	/**
	 * The data associated with the asynchronous invocation.
	 */
	void *data;
};

__BEGIN_DECLS

/**
 * Invoke a function that takes a `void*` argument in another thread.  The
 * function will be invoked with `data` as the argument.  The `data` argument
 * must be sealed.
 *
 * This can block indefinitely until the thread pool is able to process a
 * message.
 *
 * FIXME: We should add a non-blocking variant of this.
 *
 * Returns 0 on success, -EINVAL if either of the arguments are invalid.
 */
int __cheri_compartment("thread_pool")
  thread_pool_async(ThreadPoolCallback fn, void *data);

/**
 * Run a thread pool.  This does not return and can be used as a thread entry
 * point.
 */
void __cheri_compartment("thread_pool") thread_pool_run(void);
__END_DECLS

#ifdef __cplusplus
#	include <memory>
#	include <stdlib.h>
#	include <token.h>
#	include <type_traits>
#	include <cheri.hh>
/**
 * For C++ programmers, we provide some more user-friendly wrappers.
 */
namespace thread_pool
{
	/**
	 * Implementation details, should not be used outside of this header.
	 */
	namespace detail
	{
		/**
		 * Helper that generates a different sealing key per type using the
		 * allocator's token mechanism.
		 */
		template<typename T>
		inline SKey sealing_key_for_type()
		{
			static SKey key = token_key_new();
			return key;
		}

		/**
		 * Helper that provides a callback function for invoking and then
		 * freeing a sealed heap-allocated lambda.  The lambda is sealed with
		 * the software-defined sealing key constructed by the
		 * `sealing_key_for_type` helper.
		 */
		template<typename T>
		__cheri_callback void wrap_callback_lambda(void *rawFn)
		{
			auto key = sealing_key_for_type<T>();
			auto fn  = token_unseal(key, Sealed(static_cast<T *>(rawFn)));
			if (fn == nullptr)
			{
				return;
			}
			(*fn)();
			token_obj_destroy(MALLOC_CAPABILITY, key, static_cast<SObj>(rawFn));
		}

		/**
		 * Helper to wrap pure C function (including a stateless lambda) as a
		 * callback.
		 */
		template<typename Fn>
		__cheri_callback void wrap_callback_function(void *)
		{
			// C++ objects are guaranteed to have unique addresses and so a
			// zero-sized object is actually 1 byte and using `sizeof(Fn) == 1`
			// would not let us tell the difference between empty lambdas and
			// ones with a one-byte capture.  To ensure that this object is
			// really stateless, we make it a field of another type, marking it
			// as not requiring a unique address, and then check that the
			// offset of the following field is 0.
			struct Test
			{
				// The stateless callable object
				[[no_unique_address]] Fn f;
				// An field that can share the same address as `f` if `f` is
				// truly stateless.
				char b;
			};
			static_assert(offsetof(Test, b) == 0,
			              "Stateless callable object is not stateless");
			// Invoke an empty instance of this stateless callable object.
			Fn{}();
		}

	} // namespace detail

	/**
	 * Asynchronously invoke a lambda.  This moves the lambda to the heap and
	 * passes it to the thread pool's queue.  If the lambda copies any stack
	 * objects by reference then the copy will fail.
	 */
	template<typename T>
	void async(T &&lambda)
	{
		// If this is a stateless function, just send a callback function
		// pointer, don't copy zero bytes of state to the heap.
		if constexpr (std::is_convertible_v<T, void (*)(void)>)
		{
			thread_pool_async(
			  &detail::wrap_callback_function<std::remove_cvref_t<T>>, nullptr);
		}
		else
		{
			// If this is a stateful lambda, move it to the heap, create a
			// callback function that will invoke it and free it, and send a
			// pointer to the wrapper and the heap-allocated object.
			void *buffer;
			using LambdaType = std::remove_reference_t<decltype(lambda)>;
			// Allocate a new sealed object with a key that is unique to this
			// type.
			Timeout t{UnlimitedTimeout};
			void   *sealed = token_sealed_unsealed_alloc(
			    &t,
			    MALLOC_CAPABILITY,
			    detail::sealing_key_for_type<LambdaType>(),
			    sizeof(lambda),
			    &buffer);
			// Copy the lambda into the new allocation.
			// Note: We silence a warning here because we *do* want to
			// explicitly move, not forward.
			T *copy = new (buffer) T(
			  std::move(lambda)); // NOLINT(bugprone-move-forwarding-reference)
			// Create the wrapper that will unseal and invoke the lambda.
			ThreadPoolCallback invoke =
			  &detail::wrap_callback_lambda<LambdaType>;
			// Dispatch it.
			thread_pool_async(invoke, sealed);
		}
	}
} // namespace thread_pool

#endif
