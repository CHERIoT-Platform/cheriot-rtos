// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <cheri.hh>
#include <debug.hh>
#include <stdlib.h>
#include <type_traits>

template<size_t NFrames>
struct TrustedStackGeneric;

using TrustedStack = TrustedStackGeneric<0>;

namespace sched
{
	constexpr bool DebugScheduler =
#ifdef DEBUG_SCHEDULER
	  DEBUG_SCHEDULER
#else
	  false
#endif
	  ;

	/// Is scheduler accounting enabled?
	constexpr bool Accounting =
#ifdef SCHEDULER_ACCOUNTING
	  SCHEDULER_ACCOUNTING
#else
	  false
#endif
	  ;

	using Debug = ConditionalDebug<DebugScheduler, "Scheduler">;
	/**
	 * Base class for types that are exported from the scheduler with a common
	 * sealing type.  Includes an inline type marker.
	 */
	struct Handle
	{
		protected:
		/**
		 * The real type of this subclass.
		 *
		 * This must be 32 bits and must be the first word of the class, so that
		 * we are layout-compatible with the static sealing capabilities.  We
		 * use low-value numbers for things that we dynamically allocate to
		 * check types.  Anything that is statically allocated will have this
		 * field initialised by the loader.
		 *
		 * Note that allocator-allocated and static types have a word of
		 * padding here.  This is necessary to ensure that the base of the sub
		 * object is capability-aligned.  In cases created with subclassing,
		 * this is not required - the compiler will insert padding if it needs
		 * to, but we can also put small fields in this space.  For anything
		 * statically allocated, we will need to handle this separately.
		 */
		enum class Type : uint32_t
		{
			Invalid = 0,
			/**
			 * Scheduler queue object.
			 */
			Queue,

			/**
			 * Scheduler event channel type.
			 */
			Event,

			/**
			 * Multiwaiter type.
			 */
			MultiWaiter,
		} type;

		/**
		 * Constructor, takes the type of the subclass.
		 */
		Handle(Type type) : type(type) {}
		~Handle()
		{
			__clang_ignored_warning_push("-Watomic-alignment") __atomic_store_n(
			  reinterpret_cast<uint8_t *>(&type), 0, __ATOMIC_RELAXED);
			__clang_ignored_warning_pop()
		}

		public:
		/**
		 * Unseal `unsafePointer` as a pointer to an object of the specified
		 * type.  Returns nullptr if `unsafePointer` is not a valid sealed
		 * pointer to an object of the correct type.
		 */
		template<typename T>
		static T *unseal(void *unsafePointer)
		{
			return static_cast<Handle *>(unsafePointer)->unseal_as<T>();
		}
		/**
		 * Unseal this object as the specified type.  Returns nullptr if this
		 * is not a valid sealed object of the correct type.
		 */
		template<typename T>
		T *unseal_as()
		{
			static_assert(std::is_base_of_v<Handle, T>,
			              "Cannot down-cast something that is not a subclass "
			              "of Handle");
			static_assert(offsetof(T, type) == 0,
			              "Type field must be at the start of the object");
			auto unsealed = compart_unseal(this);
			if (unsealed.is_valid() && unsealed->type == T::TypeMarker)
			{
				return unsealed.cast<T>();
			}
			return nullptr;
		}
	};
	/**
	 * Info about a thread to be passed from loader to the scheduler. The
	 * scheduler will take this record and initialise the thread block.
	 */
	struct ThreadLoaderInfo
	{
		/// The trusted stack for this thread. This field should be sealed by
		/// the loader and contain populated PCC, CGP and CSP caps.
		TrustedStack *trustedStack;
		/// Thread ID. Certain compartments need to know which thread it is in.
		uint16_t threadid;
		/// Thread priority. The higher the more prioritised.
		uint16_t priority;
	};

	/**
	 * RAII class for preventing nested exceptions.
	 */
	struct ExceptionGuard
	{
		/// The number of exception's currently being handled.
		static inline uint8_t exceptionLevel;

		/// RAII type, no copy constructor.
		ExceptionGuard(const ExceptionGuard &) = delete;
		/// RAII type, no move constructor.
		ExceptionGuard(ExceptionGuard &&) = delete;

		/**
		 * Constructor.  Increments the exception level and calls the handler
		 * if we are in a nested exception.
		 */
		__always_inline ExceptionGuard(auto &&errorHandler)
		{
			exceptionLevel++;
			if (exceptionLevel > 1)
			{
				errorHandler();
			}
		}

		/**
		 * Destructor, decrements the current exception level.
		 */
		__always_inline ~ExceptionGuard()
		{
			exceptionLevel--;
		}

		/**
		 * Panic if we are in a trying to block in an interrupt handler.
		 *
		 * In debug builds, this will report an error message with the caller's
		 * source location.
		 */
		static void
		assert_safe_to_block(SourceLocation loc = SourceLocation::current())
		{
			Debug::Invariant<>(exceptionLevel == 0,
			                   "Trying to block in an interrupt context.",
			                   loc);
		}
	};

	__BEGIN_DECLS
	void exception_entry_asm(void);
	__END_DECLS

	/**
	 * Wrapper around `std::unique_ptr` for objects allocated with a specific
	 * heap capability.  The scheduler is not authorised to allocate memory
	 * except on behalf of callers.
	 */
	template<typename T>
	class HeapObject
	{
		/**
		 * Deleter for use with `std::unique_ptr`, for memory allocated with an
		 * explicit capability.
		 */
		class Deleter
		{
			/**
			 * The capability that should authorise access to the heap.
			 */
			struct SObjStruct *heapCapability;

			public:
			/**
			 * Constructor, captures the capability to use for deallocation.
			 */
			__always_inline Deleter(struct SObjStruct *heapCapability)
			  : heapCapability(heapCapability)
			{
			}

			/**
			 * Apply function, calls the destructor and cleans up the underlying
			 * memory.
			 */
			__always_inline void operator()(T *object)
			{
				object->~T();
				heap_free(heapCapability, object);
			}
		};

		protected:
		/**
		 * The underlying unique pointer.
		 */
		std::unique_ptr<T, Deleter> pointer = {nullptr, nullptr};

		public:
		/// Default constructor, creates a null object.
		HeapObject() = default;
		/**
		 * Constructor for use with externally heap-allocated objects and
		 * placement new.  Takes ownership of `allocatedObject`. The heap
		 * capability will be used to free the object on destruction.
		 */
		HeapObject(struct SObjStruct *heapCapability, T *allocatedObject)
		  : pointer(allocatedObject, heapCapability)
		{
		}

		/**
		 * Attempt to allocate memory for, and construct, an instance of `T`
		 * with the specified arguments.  If memory allocation fails, this will
		 * construct an object wrapping a null pointer and not call the
		 * constructor.  The bool-conversion operator can be used to check for
		 * success.
		 */
		template<typename... Args>
		HeapObject(Timeout           *timeout,
		           struct SObjStruct *heapCapability,
		           Args... args)
		  : pointer(static_cast<T *>(
		              heap_allocate(timeout, heapCapability, sizeof(T))),
		            {heapCapability})
		{
			if (pointer)
			{
				new (pointer.get()) T(std::forward<Args>(args)...);
			}
		}

		/**
		 * Convert to bool.  Returns true if the unique pointer owns a non-null
		 * pointer.
		 */
		operator bool()
		{
			return !!pointer;
		}

		/**
		 * Returns the wrapped pointer without transferring ownership.
		 */
		T *get()
		{
			return pointer.get();
		}

		/**
		 * Returns the wrapped pointer, transferring ownership to the caller.
		 */
		T *release()
		{
			return pointer.release();
		}
	};

	/**
	 * Wrapper for a buffer of a dynamic length, allocated on the heap owned by
	 * another compartment.
	 */
	struct HeapBuffer : public HeapObject<char>
	{
		HeapBuffer() = default;

		/**
		 * Construct an array of `count` elemets, each of which is `size` bytes
		 * long, using `heapCapability` to authorise allocation.
		 */
		HeapBuffer(Timeout           *timeout,
		           struct SObjStruct *heapCapability,
		           size_t             size,
		           size_t             count)
		  : HeapObject(
		      heapCapability,
		      static_cast<char *>(
		        heap_allocate_array(timeout, heapCapability, size, count)))
		{
		}

		/**
		 * Update the address of the wrapped pointer.
		 */
		void set_address(ptraddr_t address)
		{
			CHERI::Capability old{pointer.release()};
			old.address() = address;
			pointer.reset(old);
		}
	};

} // namespace sched
