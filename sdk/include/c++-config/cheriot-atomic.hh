// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <concepts>
#include <futex.h>
#include <type_traits>

__clang_ignored_warning_push("-Watomic-alignment") namespace cheriot
{
	enum class memory_order : int
	{
		relaxed = __ATOMIC_RELAXED,
		consume = __ATOMIC_CONSUME,
		acquire = __ATOMIC_ACQUIRE,
		release = __ATOMIC_RELEASE,
		acq_rel = __ATOMIC_ACQ_REL,
		seq_cst = __ATOMIC_SEQ_CST,
	};
	inline constexpr memory_order memory_order_relaxed = memory_order::relaxed;
	inline constexpr memory_order memory_order_consume = memory_order::consume;
	inline constexpr memory_order memory_order_acquire = memory_order::acquire;
	inline constexpr memory_order memory_order_release = memory_order::release;
	inline constexpr memory_order memory_order_acq_rel = memory_order::acq_rel;
	inline constexpr memory_order memory_order_seq_cst = memory_order::seq_cst;

	namespace detail
	{
		/**
		 * Version of atomic<T> for primitive types.  This calls the atomics
		 * support library for everything on platforms with no A extension and
		 * will use atomic instructions on ones that do.
		 *
		 * This differs from std::atomic in two intentional ways:
		 *
		 *  - The `wait` and `notify_*` methods are defined only on 4-byte
		 *    values.  If there is a requirement for anything else, we should
		 *    extend the futex APIs in the scheduler to deal with other types.
		 *  - The `wait` call has a non-standard extension that handles takes a
		 *    CHERIoT timeout parameter.
		 *
		 *  Any other divergence is a bug.
		 *
		 *  This is a base class that is extended for arithmetic and pointer
		 *  types.
		 */
		template<typename T>
		class primitive_atomic
		{
			/**
			 * SFINAE helper to give us the underlying type of enums and the
			 * raw type of everything else.
			 */
			template<typename U, bool = std::is_enum_v<U>>
			struct underlying_type
			{
				using type = U;
			};

			template<typename U>
			struct underlying_type<U, true> : ::std::underlying_type<U>
			{
			};

			protected:
			typename underlying_type<T>::type value;
			static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T> ||
			                std::is_pointer_v<T> || std::is_null_pointer_v<T>,
			              "Invalid type for primitive atomic");

			__always_inline auto *pointer_for_intrinsics(T *pointer)
			{
				if constexpr (std::is_enum_v<T>)
				{
					return static_cast<std::underlying_type_t<T> *>(pointer);
				}
				else
				{
					return pointer;
				}
			}

			static decltype(value) *as_underlying(T *v)
			{
				return reinterpret_cast<decltype(value) *>(v);
			}
			static decltype(value) as_underlying(T v)
			{
				return static_cast<decltype(value)>(v);
			}

			public:
			using value_type                          = T;
			static constexpr bool is_always_lock_free = true;
			__always_inline bool  is_lock_free() const noexcept
			{
				return true;
			}
			__always_inline bool is_lock_free() const volatile noexcept
			{
				return true;
			}

			constexpr primitive_atomic() noexcept = default;
			__always_inline constexpr primitive_atomic(T desired) noexcept
			{
				value = desired;
			}
			primitive_atomic(const primitive_atomic &) = delete;

			__always_inline T operator=(T desired) noexcept
			{
				store(desired);
				return desired;
			}
			__always_inline T operator=(T desired) volatile noexcept
			{
				return *const_cast<primitive_atomic<T> *>(this) = desired;
			}
			primitive_atomic &operator=(const primitive_atomic &) = delete;
			primitive_atomic &
			operator=(const primitive_atomic &) volatile = delete;

			__always_inline void
			store(T desired, memory_order order = memory_order_seq_cst) noexcept
			{
				__atomic_store_n(&value, as_underlying(desired), int(order));
			}
			__always_inline void
			store(T            desired,
			      memory_order order = memory_order_seq_cst) volatile noexcept
			{
				const_cast<primitive_atomic<T> *>(this)->store(desired, order);
			}

			__always_inline T
			load(memory_order order = memory_order_seq_cst) const noexcept
			{
				return __atomic_load_n(&value, int(order));
			}

			__always_inline T load(
			  memory_order order = memory_order_seq_cst) const volatile noexcept
			{
				return const_cast<primitive_atomic<T> *>(this)->load(order);
			}

			__always_inline operator T() const noexcept
			{
				return load();
			}
			__always_inline operator T() const volatile noexcept
			{
				return load();
			}

			__always_inline T
			exchange(T            desired,
			         memory_order order = memory_order_seq_cst) noexcept
			{
				return T(__atomic_exchange_n(
				  &value, as_underlying(desired), int(order)));
			}
			__always_inline T exchange(
			  T            desired,
			  memory_order order = memory_order_seq_cst) volatile noexcept
			{
				return const_cast<primitive_atomic<T> *>(this)->exchange(
				  desired, order);
			}

			__always_inline bool
			compare_exchange_weak(T           &expected,
			                      T            desired,
			                      memory_order success,
			                      memory_order failure) noexcept
			{
				__atomic_compare_exchange_n(&value,
				                            as_underlying(&expected),
				                            as_underlying(desired),
				                            true,
				                            int(success),
				                            int(failure));
			}
			__always_inline bool
			compare_exchange_weak(T           &expected,
			                      T            desired,
			                      memory_order success,
			                      memory_order failure) volatile noexcept
			{
				return const_cast<primitive_atomic<T> *>(this)
				  ->compare_exchange_weak(expected, desired, success, failure);
			}
			__always_inline bool compare_exchange_weak(
			  T           &expected,
			  T            desired,
			  memory_order order = memory_order_seq_cst) noexcept
			{
				return compare_exchange_weak(expected, desired, order, order);
			}
			__always_inline bool compare_exchange_weak(
			  T           &expected,
			  T            desired,
			  memory_order order = memory_order_seq_cst) volatile noexcept
			{
				return const_cast<primitive_atomic<T> *>(this)
				  ->compare_exchange_weak(expected, desired, order);
			}

			__always_inline bool
			compare_exchange_strong(T           &expected,
			                        T            desired,
			                        memory_order success,
			                        memory_order failure) noexcept
			{
				return __atomic_compare_exchange_n(&value,
				                                   as_underlying(&expected),
				                                   as_underlying(desired),
				                                   false,
				                                   int(success),
				                                   int(failure));
			}
			__always_inline bool
			compare_exchange_strong(T           &expected,
			                        T            desired,
			                        memory_order success,
			                        memory_order failure) volatile noexcept
			{
				return const_cast<primitive_atomic<T> *>(this)
				  ->compare_exchange_strong(
				    expected, desired, success, failure);
			}
			__always_inline bool compare_exchange_strong(
			  T           &expected,
			  T            desired,
			  memory_order order = memory_order_seq_cst) noexcept
			{
				return compare_exchange_strong(expected, desired, order, order);
			}

			__always_inline bool compare_exchange_strong(
			  T           &expected,
			  T            desired,
			  memory_order order = memory_order_seq_cst) volatile noexcept
			{
				return const_cast<primitive_atomic<T> *>(this)
				  ->compare_exchange_strong(expected, desired, order);
			}

			__always_inline void
			wait(T            old,
			     memory_order order = memory_order::seq_cst) const noexcept
			  requires(sizeof(T) == sizeof(uint32_t))
			{
				futex_wait(reinterpret_cast<const uint32_t *>(&value),
				           reinterpret_cast<uint32_t>(as_underlying(old)));
			}

			__always_inline int
			wait(Timeout       *timeout,
			     T              old,
			     memory_order   order = memory_order::seq_cst,
			     FutexWaitFlags flags = FutexNone) const noexcept
			  requires(sizeof(T) == sizeof(uint32_t))
			{
				return futex_timed_wait(
				  timeout,
				  reinterpret_cast<const uint32_t *>(&value),
				  static_cast<uint32_t>(as_underlying(old)),
				  flags);
			}

			__always_inline int
			wait(Timeout *timeout, T old, FutexWaitFlags flags) const noexcept
			  requires(sizeof(T) == sizeof(uint32_t))
			{
				return wait(timeout, old, memory_order::seq_cst, flags);
			}

			__always_inline void
			wait(T old, memory_order order = memory_order::seq_cst) const
			  volatile noexcept requires(sizeof(T) == sizeof(uint32_t))
			{
				const_cast<primitive_atomic<T> *>(this)->wait(old, order);
			}

			__always_inline void notify_one() noexcept
			  requires(sizeof(T) == sizeof(uint32_t))
			{
				futex_wake(reinterpret_cast<uint32_t *>(&value), 1);
			}
			__always_inline void notify_one() volatile noexcept
			  requires(sizeof(T) == sizeof(uint32_t))
			{
				const_cast<primitive_atomic<T> *>(this)->notify_one();
			}

			__always_inline void notify_all() noexcept
			  requires(sizeof(T) == sizeof(uint32_t))
			{
				futex_wake(reinterpret_cast<uint32_t *>(&value),
				           std::numeric_limits<uint32_t>::max());
			}
			__always_inline void notify_all() volatile noexcept
			  requires(sizeof(T) == sizeof(uint32_t))
			{
				const_cast<primitive_atomic<T> *>(this)->notify_all();
			}
		};

		/**
		 * Version of atomic for arithmetic types.  This adds the arithmetic
		 * methods.
		 */
		template<typename T>
		class arithmetic_atomic : public primitive_atomic<T>
		{
			public:
			using primitive_atomic<T>::primitive_atomic;
			using primitive_atomic<T>::operator=;
			using difference_type = typename primitive_atomic<T>::value_type;

			__always_inline T
			fetch_add(T arg, memory_order order = memory_order_seq_cst) noexcept
			{
				return __atomic_fetch_add(&this->value, arg, int(order));
			}
			__always_inline T fetch_add(
			  T            arg,
			  memory_order order = memory_order_seq_cst) volatile noexcept
			{
				const_cast<arithmetic_atomic<T> *>(this)->fetch_add(arg, order);
			}

			__always_inline T
			fetch_sub(T arg, memory_order order = memory_order_seq_cst) noexcept
			{
				return __atomic_fetch_sub(&this->value, arg, int(order));
			}
			__always_inline T fetch_sub(
			  T            arg,
			  memory_order order = memory_order_seq_cst) volatile noexcept
			{
				const_cast<arithmetic_atomic<T> *>(this)->fetch_sub(arg, order);
			}

			__always_inline T
			fetch_and(T arg, memory_order order = memory_order_seq_cst) noexcept
			{
				return __atomic_fetch_and(&this->value, arg, int(order));
			}
			__always_inline T fetch_and(
			  T            arg,
			  memory_order order = memory_order_seq_cst) volatile noexcept
			{
				const_cast<arithmetic_atomic<T> *>(this)->fetch_and(arg, order);
			}

			__always_inline T
			fetch_or(T arg, memory_order order = memory_order_seq_cst) noexcept
			{
				return __atomic_fetch_or(&this->value, arg, int(order));
			}
			__always_inline T fetch_or(
			  T            arg,
			  memory_order order = memory_order_seq_cst) volatile noexcept
			{
				const_cast<arithmetic_atomic<T> *>(this)->fetch_or(arg, order);
			}

			__always_inline T
			fetch_xor(T arg, memory_order order = memory_order_seq_cst) noexcept
			{
				return __atomic_fetch_xor(&this->value, arg, int(order));
			}
			__always_inline T fetch_xor(
			  T            arg,
			  memory_order order = memory_order_seq_cst) volatile noexcept
			{
				const_cast<arithmetic_atomic<T> *>(this)->fetch_xor(arg, order);
			}

			__always_inline T operator++() noexcept
			{
				return fetch_add(1) + 1;
			}
			__always_inline T operator++() volatile noexcept
			{
				return fetch_add(1) + 1;
			}
			__always_inline T operator++(int) noexcept
			{
				return fetch_add(1);
			}
			__always_inline T operator++(int) volatile noexcept
			{
				return fetch_add(1);
			}
			__always_inline T operator--() noexcept
			{
				return fetch_sub(1) - 1;
			}
			__always_inline T operator--() volatile noexcept
			{
				return fetch_sub(1) - 1;
			}
			__always_inline T operator--(int) noexcept
			{
				return fetch_sub(1);
			}
			__always_inline T operator--(int) volatile noexcept
			{
				return fetch_sub(1);
			}

			__always_inline T operator+=(T arg) noexcept
			{
				return fetch_add(arg) + arg;
			}
			__always_inline T operator+=(T arg) volatile noexcept
			{
				return fetch_add(arg) + arg;
			}
			__always_inline T operator-=(T arg) noexcept
			{
				return fetch_sub(arg) - arg;
			}
			__always_inline T operator-=(T arg) volatile noexcept
			{
				return fetch_sub(arg) - arg;
			}

			__always_inline T operator&=(T arg) noexcept
			{
				return fetch_and(arg) & arg;
			}
			__always_inline T operator&=(T arg) volatile noexcept
			{
				return fetch_and(arg) & arg;
			}
			__always_inline T operator|=(T arg) noexcept
			{
				return fetch_or(arg) | arg;
			}
			__always_inline T operator|=(T arg) volatile noexcept
			{
				return fetch_or(arg) | arg;
			}
			__always_inline T operator^=(T arg) noexcept
			{
				return fetch_xor(arg) ^ arg;
			}
			__always_inline T operator^=(T arg) volatile noexcept
			{
				return fetch_xor(arg) ^ arg;
			}
		};

		/**
		 * Version of atomic for pointer types.  This adds pointer arithmetic
		 * methods.
		 */
		template<typename T>
		class pointer_atomic : primitive_atomic<T>
		{
			public:
			using primitive_atomic<T>::primitive_atomic;
			using difference_type = std::ptrdiff_t;

			T *fetch_add(std::ptrdiff_t arg,
			             memory_order   order = memory_order_seq_cst) noexcept
			{
				return __atomic_fetch_add(
				  &this->value, arg * sizeof(T), int(order));
			}
			T *fetch_add(
			  std::ptrdiff_t arg,
			  memory_order   order = memory_order_seq_cst) volatile noexcept
			{
				const_cast<pointer_atomic<T> *>(this)->fetch_add(arg,
				                                                 int(order));
			}

			T *fetch_sub(std::ptrdiff_t arg,
			             memory_order   order = memory_order_seq_cst) noexcept
			{
				return __atomic_fetch_sub(
				  &this->value, arg * sizeof(T), int(order));
			}
			T *fetch_sub(
			  std::ptrdiff_t arg,
			  memory_order   order = memory_order_seq_cst) volatile noexcept
			{
				const_cast<pointer_atomic<T> *>(this)->fetch_sub(arg,
				                                                 int(order));
			}

			__always_inline T *operator++() noexcept
			{
				return fetch_add(1) + 1;
			}
			__always_inline T *operator++() volatile noexcept
			{
				return fetch_add(1) + 1;
			}
			__always_inline T *operator++(int) noexcept
			{
				return fetch_add(1);
			}
			__always_inline T *operator++(int) volatile noexcept
			{
				return fetch_add(1);
			}
			__always_inline T *operator--() noexcept
			{
				return fetch_sub(1) - 1;
			}
			__always_inline T *operator--() volatile noexcept
			{
				return fetch_sub(1) - 1;
			}
			__always_inline T *operator--(int) noexcept
			{
				return fetch_sub(1);
			}
			__always_inline T *operator--(int) volatile noexcept
			{
				return fetch_sub(1);
			}

			__always_inline T *operator+=(std::ptrdiff_t arg) noexcept
			{
				return fetch_add(arg) + arg;
			}
			__always_inline T *operator+=(std::ptrdiff_t arg) volatile noexcept
			{
				return fetch_add(arg) + arg;
			}
			__always_inline T *operator-=(std::ptrdiff_t arg) noexcept
			{
				return fetch_sub(arg) - arg;
			}
			__always_inline T *operator-=(std::ptrdiff_t arg) volatile noexcept
			{
				return fetch_sub(arg) - arg;
			}
		};

		/**
		 * Simple flag lock.  This uses `primitive_atomic` to build a trivial
		 * lock.
		 */
		class flag_lock
		{
			/**
			 * Possible states of the flag lock.
			 */
			enum class LockState : uint32_t
			{
				/// Lock is not held.
				Unlocked,
				/// Lock is held, no waiters.
				Locked,
				/// Lock is held and one or more waiters exist.
				LockedWithWaiters
			};
			/// The lock state.
			primitive_atomic<LockState> lockWord;

			public:
			/**
			 * Acquire the lock.  Blocks indefinitely.
			 */
			__noinline void lock()
			{
				LockState old = LockState::Unlocked;
				while (true)
				{
					switch (old)
					{
						// If the lock is not held, try to acquire it and return
						// if we can.
						case LockState::Unlocked:
							if (lockWord.compare_exchange_strong(
							      old, LockState::Locked))
							{
								return;
							}
							break;
							// If the lock is held, mark it as having waiters
							// and then wait.
						case LockState::Locked:
							lockWord.exchange(LockState::LockedWithWaiters);
							[[fallthrough]];
							// If the lock is blocked with waiters, sleep
						case LockState::LockedWithWaiters:
							lockWord.wait(LockState::LockedWithWaiters);
					}
				}
			}

			/**
			 * Release the lock, waking any waiters if there are any.
			 */
			__noinline void unlock()
			{
				auto old = lockWord.exchange(LockState::Unlocked);
				if (old == LockState::LockedWithWaiters)
				{
					lockWord.notify_all();
				}
			}
		};

		/**
		 * Fallback `atomic` implementation that uses a lock to protect the
		 * value.
		 */
		template<typename T>
		class locked_atomic
		{
			private:
			/// The atomic value.
			T value;
			/// Lock protecting this object, at the end so that it can go into
			/// padding.
			mutable flag_lock lock;

			struct guard
			{
				flag_lock &lock;
				__always_inline ~guard()
				{
					lock.unlock();
				}
			};
			__always_inline guard acquire_lock()
			{
				lock.lock();
				return {lock};
			}

			public:
			using value_type                          = T;
			static constexpr bool is_always_lock_free = false;
			__always_inline bool  is_lock_free() const noexcept
			{
				return false;
			}
			__always_inline bool is_lock_free() const volatile noexcept
			{
				return false;
			}

			constexpr locked_atomic() noexcept = default;
			__always_inline constexpr locked_atomic(T desired) noexcept
			{
				value = desired;
			}
			locked_atomic(const locked_atomic &) = delete;

			__always_inline T operator=(T desired) noexcept
			{
				auto g = acquire_lock();
				value  = desired;
				return desired;
			}
			__always_inline T operator=(T desired) volatile noexcept
			{
				return *const_cast<locked_atomic<T> *>(this) = desired;
			}
			locked_atomic &operator=(const locked_atomic &) = delete;
			locked_atomic &operator=(const locked_atomic &) volatile = delete;

			__always_inline void
			store(T desired, memory_order order = memory_order_seq_cst) noexcept
			{
				auto g = acquire_lock();
				value  = desired;
			}
			__always_inline void
			store(T            desired,
			      memory_order order = memory_order_seq_cst) volatile noexcept
			{
				const_cast<locked_atomic<T> *>(this)->store(desired, order);
			}

			__always_inline T
			load(memory_order order = memory_order_seq_cst) const noexcept
			{
				auto g = acquire_lock();
				return value;
			}

			__always_inline T load(
			  memory_order order = memory_order_seq_cst) const volatile noexcept
			{
				return const_cast<locked_atomic<T> *>(this)->load(order);
			}

			__always_inline operator T() const noexcept
			{
				return load();
			}
			__always_inline operator T() const volatile noexcept
			{
				return load();
			}

			__always_inline T
			exchange(T            desired,
			         memory_order order = memory_order_seq_cst) noexcept
			{
				auto g   = acquire_lock();
				T    tmp = value;
				value    = desired;
				return tmp;
			}
			__always_inline T exchange(
			  T            desired,
			  memory_order order = memory_order_seq_cst) volatile noexcept
			{
				return const_cast<locked_atomic<T> *>(this)->exchange(desired,
				                                                      order);
			}

			__always_inline bool
			compare_exchange_weak(T           &expected,
			                      T            desired,
			                      memory_order success,
			                      memory_order failure) noexcept
			{
				auto g = acquire_lock();
				if (value == expected)
				{
					desired = value;
				}
				expected = value;
				return true;
			}
			__always_inline bool
			compare_exchange_weak(T           &expected,
			                      T            desired,
			                      memory_order success,
			                      memory_order failure) volatile noexcept
			{
				return const_cast<locked_atomic<T> *>(this)
				  ->compare_exchange_weak(expected, desired, success, failure);
			}
			__always_inline bool compare_exchange_weak(
			  T           &expected,
			  T            desired,
			  memory_order order = memory_order_seq_cst) noexcept
			{
				return compare_exchange_weak(expected, desired, order, order);
			}
			__always_inline bool compare_exchange_weak(
			  T           &expected,
			  T            desired,
			  memory_order order = memory_order_seq_cst) volatile noexcept
			{
				return const_cast<locked_atomic<T> *>(this)
				  ->compare_exchange_weak(expected, desired, order);
			}

			__always_inline bool
			compare_exchange_strong(T           &expected,
			                        T            desired,
			                        memory_order success,
			                        memory_order failure) noexcept
			{
				return compare_exchange_weak(
				  expected, desired, success, failure);
			}
			__always_inline bool
			compare_exchange_strong(T           &expected,
			                        T            desired,
			                        memory_order success,
			                        memory_order failure) volatile noexcept
			{
				return const_cast<locked_atomic<T> *>(this)
				  ->compare_exchange_strong(
				    expected, desired, success, failure);
			}
			__always_inline bool compare_exchange_strong(
			  T           &expected,
			  T            desired,
			  memory_order order = memory_order_seq_cst) noexcept
			{
				return compare_exchange_strong(expected, desired, order, order);
			}

			__always_inline bool compare_exchange_strong(
			  T           &expected,
			  T            desired,
			  memory_order order = memory_order_seq_cst) volatile noexcept
			{
				return const_cast<locked_atomic<T> *>(this)
				  ->compare_exchange_strong(expected, desired, order);
			}
		};
	}; // namespace detail

	/**
	 * Select the correct `atomic` implementation based on the type.
	 */
	template<typename T>
	using atomic = std::conditional_t<
	  std::is_pointer_v<T>,
	  detail::pointer_atomic<T>,
	  std::conditional_t<std::is_arithmetic_v<T>,
	                     detail::arithmetic_atomic<T>,
	                     std::conditional_t<std::is_enum_v<T>,
	                                        detail::primitive_atomic<T>,
	                                        detail::locked_atomic<T>>>>;

} // namespace cheriot
__clang_ignored_warning_pop()
