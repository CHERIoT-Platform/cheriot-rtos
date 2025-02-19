// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/**
 * @file Pointer utilities
 */

#pragma once

#include <cheri.hh>
#include <concepts>

namespace ds::pointer
{

	/**
	 * Offset a pointer by a number of bytes.  The return type must be
	 * explicitly specified by the caller.  The type of the displacement offset
	 * (Offset) is templated so that we can accept both signed and unsigned
	 * offsets.
	 */
	template<typename T, typename U>
	static inline __always_inline T *offset(U *base, std::integral auto offset)
	{
		CHERI::Capability c{base};
		c.address() += offset;
		return c.template cast<void>().template cast<T>();
	}

	/**
	 * Compute the unsigned difference in bytes between two pointers' target
	 * addresses.  To be standards-compliant, cursor must be part of the same
	 * allocation as base and at a higher address.
	 */
	static inline __always_inline size_t diff(const void *base,
	                                          const void *cursor)
	{
		return static_cast<size_t>(reinterpret_cast<const char *>(cursor) -
		                           reinterpret_cast<const char *>(base));
	}

	namespace proxy
	{

		/**
		 * Proxies<P,T> if P is a proxy object for T*-s.
		 */
		template<typename P, typename T>
		concept Proxies = std::same_as<T, typename P::Type> &&
		                  requires(P &proxy, P &proxy2, T *ptr) {
			                  /* Probe for operator=(T*) */
			                  { proxy = ptr } -> std::same_as<P &>;

			                  /* Probe for operator T*() */
			                  { ptr == proxy } -> std::same_as<bool>;

			                  /* TODO: How to probe for operator-> ? */

			                  /* Probe for operator==(T*) */
			                  { proxy == ptr } -> std::same_as<bool>;

			                  /* Probe for operator==(P&) */
			                  { proxy == proxy2 } -> std::same_as<bool>;

			                  /* Probe for operator<=>(T*) */
			                  {
				                  proxy <=> ptr
			                  } -> std::same_as<std::strong_ordering>;

			                  /* Probe for operator<=>(P) */
			                  {
				                  proxy <=> proxy2
			                  } -> std::same_as<std::strong_ordering>;
		                  };

		/**
		 * Pointer references are pointer proxies, shockingly enough.
		 */
		template<typename T>
		class Pointer
		{
			T *&ref;

			public:
			using Type = T;

			__always_inline Pointer(T *&r) : ref(r) {}

			__always_inline operator T *()
			{
				return ref;
			}

			__always_inline T *operator->()
			{
				return *this;
			}

			__always_inline Pointer<T> &operator=(T *t)
			{
				ref = t;
				return *this;
			}

			__always_inline Pointer<T> &operator=(Pointer const &p)
			{
				ref = p.ref;
				return *this;
			}

			__always_inline bool operator==(Pointer &p)
			{
				return this->ref == p.ref;
			}

			__always_inline auto operator<=>(Pointer &p)
			{
				return this->ref <=> p.ref;
			}
		};
		static_assert(Proxies<Pointer<void>, void>);

		/**
		 * Equipped with a context for bounds, an address reference can be a
		 * proxy for a pointer.
		 */
		template<typename T>
		class PtrAddr
		{
			CHERI::Capability<void> ctx;
			ptraddr_t              &ref;

			public:
			using Type = T;

			__always_inline PtrAddr(void *c, ptraddr_t &r) : ctx(c), ref(r) {}

			__always_inline operator T *()
			{
				auto c      = ctx;
				c.address() = ref;
				return c.cast<T>().get();
			}

			__always_inline T *operator->()
			{
				return *this;
			}

			__always_inline PtrAddr &operator=(T *p)
			{
				ref = CHERI::Capability{p}.address();
				return *this;
			}

			__always_inline PtrAddr &operator=(PtrAddr const &p)
			{
				ref = p.ref;
				return *this;
			}

			/*
			 * Since the context is used only for bounds, don't bother
			 * implicitly converting both proxies up to T*
			 */

			__always_inline bool operator==(PtrAddr &p)
			{
				return ref == p.ref;
			}

			__always_inline auto operator<=>(PtrAddr &p)
			{
				return ref <=> p.ref;
			}
		};
		static_assert(Proxies<PtrAddr<void>, void>);

		/**
		 * Deduction gude for the common enough case where the context
		 * type and the represented type are equal.
		 */
		template<typename T>
		PtrAddr(T *, ptraddr_t) -> PtrAddr<T>;

		/**
		 * Like the above, but with a constant offset on the interpretation of
		 * its addresss fields.  This is useful for building points-to-container
		 * data structures (rather than points-to-member as with the above two).
		 * The container_of and address-taking operations that move back and
		 * forth between container and link member should fuse away with the
		 * offsetting operations herein.  You may prefer this if your common or
		 * fast-paths involve lots of container_of operations.
		 */
		template<ptrdiff_t Offset, typename T>
		class OffsetPtrAddr
		{
			CHERI::Capability<void> ctx;
			ptraddr_t              &ref;

			public:
			using Type = T;

			__always_inline OffsetPtrAddr(void *c, ptraddr_t &r)
			  : ctx(c), ref(r)
			{
			}

			__always_inline operator T *()
			{
				auto c      = ctx;
				c.address() = ref + Offset;
				return c.cast<T>().get();
			}

			__always_inline T *operator->()
			{
				return *this;
			}

			__always_inline OffsetPtrAddr &operator=(T *p)
			{
				ref = CHERI::Capability{p}.address() - Offset;
				return *this;
			}

			__always_inline OffsetPtrAddr &operator=(OffsetPtrAddr const &p)
			{
				ref = p.ref;
				return *this;
			}

			/*
			 * Since the context is used only for bounds, don't bother
			 * implicitly converting both proxies up to T*.  This also probably
			 * saves the optimizer the effort of cancelling the Offset
			 * arithmetic on either side of the comparison.
			 */

			__always_inline bool operator==(OffsetPtrAddr &p)
			{
				return ref == p.ref;
			}

			__always_inline auto operator<=>(OffsetPtrAddr &p)
			{
				return ref <=> p.ref;
			}
		};
		static_assert(Proxies<OffsetPtrAddr<8, void>, void>);

	} // namespace proxy

} // namespace ds::pointer
