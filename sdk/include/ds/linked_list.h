// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/**
 * @file A (circular) doubly linked list, abstracted over cons cell
 * representations.
 *
 * See `tests/list-test.cc` for additional information on how to use it.
 */

#pragma once

#include <concepts>
#include <ds/pointer.h>
#include <optional>

namespace ds::linked_list
{

	namespace cell
	{
		/**
		 * The primitive, required, abstract interface to our cons cells.
		 *
		 * All methods are "namespaced" with `cell_` to support the case where
		 * the encoded forms are also representing other state (for example,
		 * bit-packed flags in pointer address bits).
		 */
		template<typename T>
		concept HasCellOperations = requires(T &t) {
			/** Proxies for list linkages */
			{ t.cell_next() } -> ds::pointer::proxy::Proxies<T>;
			{ t.cell_prev() } -> ds::pointer::proxy::Proxies<T>;
		};

		/**
		 * Reset a cell to a singleton ring.  Not all cons cells are required to
		 * be able to do this, though if you're sticking to rings and not
		 * (ab)using the machinery here in interesting ways, this should be easy
		 * to specify.
		 *
		 * This is a method, and not a constructor, to handle cases where the
		 * cell is also packing other state into its representation.
		 */
		template<typename T>
		concept HasReset = requires(T &t) {
			{ t.cell_reset() } -> std::same_as<void>;
		};

		template<typename T>
		concept HasCellOperationsReset = HasCellOperations<T> && HasReset<T>;

		/**
		 * Additional, optional overrides available within implementation of
		 * cons cells.  It may be useful to static_assert() these in
		 * implementations to make sure we are not falling back to the defaults
		 * in terms of the above primops.
		 *
		 * @{
		 */
		template<typename T>
		concept HasIsSingleton = requires(T &t) {
			{ t.cell_is_singleton() } -> std::same_as<bool>;
		};

		template<typename T>
		concept HasIsSingletonCheck = requires(T &t) {
			{ t.cell_is_singleton_check() } -> std::same_as<bool>;
		};

		template<typename T>
		concept HasIsDoubleton = requires(T &t) {
			{ t.cell_is_doubleton() } -> std::same_as<bool>;
		};

		/** @} */

	} // namespace cell

	/**
	 * Self-loops indicate either the sentinels of an empty list or,
	 * less often, singletons without their sentinels; it's up to
	 * the caller to know which is being tested for, here.
	 *
	 * The default implementation decodes and compares one link;
	 * implementations may have more efficient mechanisms.
	 *
	 * @{
	 */
	template<cell::HasCellOperations T>
	    requires(!cell::HasIsSingleton<T>)
	__always_inline bool is_singleton(T *e)
	{
		return e == e->cell_prev();
	}

	template<cell::HasCellOperations T>
	    requires(cell::HasIsSingleton<T>)
	__always_inline bool is_singleton(T *e)
	{
		return e->cell_is_singleton();
	}
	/** @} */

	/**
	 * Like is_singleton(), but checks both edges.  Useful only for
	 * testing invariants.
	 *
	 * The default implementation decodes and compares both links.
	 */
	template<cell::HasCellOperations T>
	    requires(!cell::HasIsSingletonCheck<T>)
	__always_inline bool is_singleton_check(T *e)
	{
		return (e == e->cell_next()) && (e == e->cell_prev());
	}

	template<cell::HasCellOperations T>
	    requires(cell::HasIsSingletonCheck<T>)
	__always_inline bool is_singleton_check(T *e)
	{
		return e->is_singleton_check();
	}
	/** @} */

	/**
	 * Doubletons are either singleton collections (with both the sentinel
	 * and the single element satisfying this test) or, less often, a pair
	 * of elements without a sentinel.  The caller is expected to know
	 * what's meant by this test.
	 *
	 * The default implementation decodes and compares both links.
	 * @{
	 */
	template<cell::HasCellOperations T>
	    requires(!cell::HasIsDoubleton<T>)
	__always_inline bool is_doubleton(T *e)
	{
		return e->cell_prev() == e->cell_next();
	}

	template<cell::HasCellOperations T>
	    requires(cell::HasIsDoubleton<T>)
	__always_inline bool is_doubleton(T *e)
	{
		return e->cell_is_doubleton();
	}
	/** @} */

	/**
	 * Verify linkage invariants.  Again, useful only for testing.
	 *
	 * The default implementation decodes all four relevant links.
	 */
	template<cell::HasCellOperations T>
	__always_inline bool is_well_formed(T *e)
	{
		return (e == e->cell_prev()->cell_next()) &&
		       (e == e->cell_next()->cell_prev());
	}

	/**
	 * Insert a ring of `elem`-ents (typically, a singleton ring) before the
	 * `curr`-ent element (or sentinel) in the ring.  In general, you will
	 * probably want to make sure that at most one of `elem` or `curr`
	 * points to a ring with a sentinel node.
	 *
	 * If `curr` is the sentinel, this is appending to the list, in the
	 * sense that the element(s) occupy (or span) the next-most and
	 * prev-least position from the sentinel.
	 *
	 * By symmetry, if `elem` is, instead, the sentinel, then `curr` is
	 * prepended to the list in the same sense.
	 */
	template<cell::HasCellOperations Cell>
	__always_inline void insert_before(Cell *curr, Cell *elem)
	{
		curr->cell_prev()->cell_next() = elem->cell_next();
		elem->cell_next()->cell_prev() = curr->cell_prev();
		curr->cell_prev()              = elem;
		elem->cell_next()              = curr;
	}

	/**
	 * Emplacement before.  This fuses initialization and insertion, so that
	 *
	 *    emplace_before(c, e);
	 *
	 * is semantically equivalent to
	 *
	 *    e->cell_reset(); insert_before(c, e);
	 *
	 * but spelled in a way that the compiler can understand a bit better, with
	 * less effort spent in provenance and/or alias analysis.
	 */
	template<cell::HasCellOperations Cell, typename P>
	    requires std::same_as<P, Cell *> || ds::pointer::proxy::Proxies<P, Cell>
	__always_inline void emplace_before(P curr, Cell *elem)
	{
		auto prev         = curr->cell_prev();
		elem->cell_next() = curr;
		elem->cell_prev() = prev;
		prev->cell_next() = elem;
		prev              = elem;
	}

	/**
	 * Emplacement after.  This fuses initialization and insertion, so that
	 *
	 *    emplace_after(c, e);
	 *
	 * is semantically equivalent to
	 *
	 *    e->cell_reset(); insert_before(e, c);
	 *
	 * but spelled in a way that the compiler can understand a bit better, with
	 * less effort spent in provenance and/or alias analysis.
	 */
	template<cell::HasCellOperations Cell, typename P>
	    requires std::same_as<P, Cell *> || ds::pointer::proxy::Proxies<P, Cell>
	__always_inline void emplace_after(P curr, Cell *elem)
	{
		auto next         = curr->cell_next();
		elem->cell_prev() = curr;
		elem->cell_next() = next;
		next->cell_prev() = elem;
		next              = elem;
	}

	/**
	 * Remove from the list without turning the removed span into a
	 * well-formed ring.  This is useful only if that invariant will be
	 * restored later (prior to insertion, at the very least).
	 *
	 * The removed element or span instead retains links into the ring
	 * whence it was removed, but is no longer well-formed, since that ring
	 * no longer references the removed element or span.
	 *
	 * This can be used to remove...
	 *
	 *   - a single element (`el == er`)
	 *
	 *   - the sentinel (`el == er`), leaving the rest of the ring, if any,
	 *     as a sentinel-free ring
	 *
	 *   - a span of elements from `el` to `er` via the `next` links; the
	 *     removed span is damaged and must be corrected, while the residual
	 *     ring remains well-formed.
	 *
	 * In all cases, `el`'s previous element is returned as a handle to the
	 * residual ring.
	 */
	template<cell::HasCellOperations Cell>
	__always_inline Cell *unsafe_remove(Cell *el, Cell *er)
	{
		auto p         = el->cell_prev();
		auto n         = er->cell_next();
		n->cell_prev() = p;
		p->cell_next() = n;
		return p;
	}

	template<cell::HasCellOperations Cell>
	__always_inline Cell *unsafe_remove(Cell *e)
	{
		return unsafe_remove(e, e);
	}

	/**
	 * Remove a particular element `rem` from the ring, already knowing its
	 * adjacent, previous link `prev`.  `prev` remains connected to the ring
	 * but `rem` will no longer be well-formed.  Returns a proxy to prev's
	 * next field.
	 */
	template<cell::HasCellOperations Cell>
	__always_inline auto unsafe_remove_link(Cell *prev, Cell *rem)
	{
		auto next         = rem->cell_next();
		auto prevnext     = prev->cell_next();
		prevnext          = next;
		next->cell_prev() = prev;
		return prevnext;
	}

	/**
	 * Remove from the ring, cleaving the ring into two well-formed rings.
	 *
	 * This can be used to remove...
	 *
	 *   - a single element (`el == er`)
	 *
	 *   - the sentinel (`el == er`), leaving the rest of the ring, if any,
	 *     as a sentinel-free collection
	 *
	 *   - a span of elements from `el` to `er` via `next` links; the
	 *     removed span is made into a ring and the residual ring is left
	 *     well-formed.
	 *
	 * In all cases, `el`'s previous element is returned as a handle to the
	 * residual ring.  (The caller must already have a reference to the span
	 * being removed).  This is especially useful when `remove`-ing elements
	 * during a `search`, below: overwriting the callback's Cell pointer
	 * (passed by *reference*) will continue the iteration, calling back at
	 * the removed node's successor.
	 *
	 * Removing a singleton from its ring from itself causes no change, as
	 * any would-be residual ring is empty.  This corner case requires some
	 * care on occasion.
	 */
	template<cell::HasCellOperations Cell>
	__always_inline Cell *remove(Cell *el, Cell *er)
	{
		Cell *p         = unsafe_remove(el, er);
		el->cell_prev() = er;
		er->cell_next() = el;
		return p;
	}

	template<cell::HasCellOperations Cell>
	__always_inline Cell *remove(Cell *e)
	{
		return remove(e, e);
	}

	/**
	 * Search through a span of a ring, inclusively from `from` through
	 * exclusively to `to`, applying `f` to each cons cell in turn.  If `f`
	 * returns `true`, the search stops early and returns an optional carrying a
	 * pointer to the Cell just scrutinized; otherwise, search returns an empty
	 * option.  To (side-effectfully) visit every node in the
	 * span, have `f` always return false.
	 *
	 * The callback may take the Cell pointer by reference and so
	 * side-effectfully influence iteration by updating that reference and
	 * returning false.  The callback may also update that reference when
	 * returning true, which will impact the return value of this call, but that
	 * is unlikely to be the intended outcome.
	 *
	 * In order to guarantee termination, `to` must always remain reachable from
	 * the iterator.  This is usually trivially satisfied, but matters if, for
	 * example, `from` and `to` are the same element and the iterator is
	 * removing visited elements (though in that case, one might prefer
	 * search_safe).
	 */
	template<bool Reverse = false, cell::HasCellOperations Cell, typename F>
	__always_inline std::optional<Cell *> search(Cell *from, Cell *to, F f)
	{
		Cell *elem;
		for (elem = from; elem != to;
		     elem = Reverse ? elem->cell_prev() : elem->cell_next())
		{
			if (f(elem))
			{
				return {elem};
			}
		}
		return {};
	}

	/**
	 * Search through all elements of a ring *except* `elem`.  If `elem` is the
	 * sentinel of a ring, then this is, as one expects, a `search` over all
	 * non-sentinel members of the ring.
	 *
	 * In order to guarantee termination, `elem` must always remain reachable
	 * from the iterator.
	 */
	template<bool Reverse = false, cell::HasCellOperations Cell, typename F>
	__always_inline auto search(Cell *elem, F f)
	{
		return search<Reverse>(static_cast<Cell *>(elem->cell_next()), elem, f);
	}

	/**
	 * Like `search`, but this form caches the next pointer across the callback
	 * invocation, making it safe to modify the linkages of, or even free, the
	 * element provided to the callback.
	 *
	 * While the callback could, like search, take the Cell pointer by
	 * reference, because this form caches the next pointer, updating that
	 * reference would serve no purpose.
	 *
	 * Because this form is useful for elementwise destruction, unlike `search`,
	 * it simply returns a boolean indicating whether the search was successful.
	 *
	 * In order to guarantee termination, `to` must always remain reachable from
	 * the iterator.  This is usually trivially satisfied, but matters if `from`
	 * and `to are the same element.
	 */
	template<bool Reverse = false, cell::HasCellOperations Cell, typename F>
	__always_inline bool search_safe(Cell *from, Cell *to, F f)
	{
		Cell *elem = from;
		Cell *next;
		for (elem = from; elem != to; elem = next)
		{
			next = Reverse ? elem->cell_prev() : elem->cell_next();
			if (f(elem))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * search_safe through all elements of a ring *except* `elem`.  If `elem` is
	 * the sentinel of a ring, then this is, as one expects, a `search` over all
	 * non-sentinel members of the ring.
	 *
	 * In order to guarantee termination, `elem` must always remain reachable
	 * from the iterator.
	 */
	template<bool Reverse = false, cell::HasCellOperations Cell, typename F>
	__always_inline auto search_safe(Cell *elem, F f)
	{
		return search_safe<Reverse>(
		  static_cast<Cell *>(elem->cell_next()), elem, f);
	}

	/**
	 * Convenience wrapper for a sentinel cons cell, encapsulating some common
	 * patterns.
	 */
	template<cell::HasCellOperationsReset CellTemplateArg>
	struct Sentinel
	{
		using Cell = CellTemplateArg;

		/**
		 * The sentinel node itself.  Viewing the ring as a list, this
		 * effectively serves as pointers to the head (next) and to the tail
		 * (prev) of the list.  Unlike more traditional nullptr-terminated
		 * lists, though, here, the sentinel participates in the ring.
		 *
		 * This is marked `cheri_no_subobject_bounds` because some of our cons
		 * cell implementations use pointer proxies that rely on the bounds
		 * provided by `this` (which, in turn, is likely to be
		 * `cheri_no_subobject_bounds`)
		 */
		Cell sentinel __attribute__((__cheri_no_subobject_bounds__)) = {};

		__always_inline void reset()
		{
			sentinel.cell_reset();
		}

		__always_inline bool is_empty()
		{
			return linked_list::is_singleton(&sentinel);
		}

		__always_inline void append(Cell *elem)
		{
			linked_list::insert_before(&sentinel, elem);
		}

		__always_inline void append_emplace(Cell *elem)
		{
			linked_list::emplace_before(&sentinel, elem);
		}

		__always_inline void prepend(Cell *elem)
		{
			linked_list::insert_before(elem, &sentinel);
		}

		__always_inline void prepend_emplace(Cell *elem)
		{
			linked_list::emplace_after(&sentinel, elem);
		}

		__always_inline Cell *first()
		{
			return sentinel.cell_next();
		}

		__always_inline Cell *last()
		{
			return sentinel.cell_prev();
		}

		__always_inline Cell *unsafe_take_first()
		{
			Cell *f = sentinel.cell_next();
			linked_list::unsafe_remove_link(&sentinel, f);
			return f;
		}

		__always_inline Cell *take_all()
		{
			auto p = linked_list::unsafe_remove(&sentinel);
			sentinel.cell_reset();
			return p;
		}

		template<bool Reverse = false, typename F>
		__always_inline auto search(F f)
		{
			return linked_list::search<Reverse>(&sentinel, f);
		}

		template<bool Reverse = false, typename F>
		__always_inline auto search_safe(F f)
		{
			return linked_list::search_safe<Reverse>(
			  &sentinel, [f](Cell *c) { return f(c); });
		}
	};

	namespace cell
	{

		/** Cons cell using two pointers */
		class Pointer
		{
			Pointer *prev, *next;

			public:
			Pointer()
			{
				this->cell_reset();
			}

			__always_inline void cell_reset()
			{
				prev = next = this;
			}

			__always_inline auto cell_next()
			{
				return ds::pointer::proxy::Pointer(next);
			}

			__always_inline auto cell_prev()
			{
				return ds::pointer::proxy::Pointer(prev);
			}
		};
		static_assert(HasCellOperationsReset<Pointer>);

		/**
		 * Encode a linked list cons cell as a pair of addresses (but present an
		 * interface in terms of pointers).  CHERI bounds on the returned
		 * pointers are inherited from the pointer to `this` cons cell.
		 */
		class __cheri_no_subobject_bounds PtrAddr
		{
			ptraddr_t prev, next;

			public:
			PtrAddr()
			{
				this->cell_reset();
			}
			/* Primops */

			__always_inline void cell_reset()
			{
				prev = next = CHERI::Capability{this}.address();
			}

			__always_inline auto cell_next()
			{
				return ds::pointer::proxy::PtrAddr(this, next);
			}

			__always_inline auto cell_prev()
			{
				return ds::pointer::proxy::PtrAddr(this, prev);
			}

			/*
			 * Specialized implementations that may be slightly fewer
			 * instructions than the generic approaches in terms of the primops.
			 */

			__always_inline bool cell_is_singleton()
			{
				return prev == CHERI::Capability{this}.address();
			}

			__always_inline bool cell_is_doubleton()
			{
				return prev == next;
			}
		};
		static_assert(HasCellOperationsReset<PtrAddr>);
		static_assert(HasIsSingleton<PtrAddr>);
		static_assert(HasIsDoubleton<PtrAddr>);

		/**
		 * Encode a linked list cons cell as a pair of addresses (but present an
		 * interface in terms of pointers).  CHERI bounds on the returned
		 * pointers are inherited from the pointer to `this` cons cell.
		 */
		template<ptrdiff_t Offset>
		class OffsetPtrAddr
		{
			ptraddr_t prev, next;

			public:
			OffsetPtrAddr()
			{
				this->cell_reset();
			}

			/* Primops */

			__always_inline void cell_reset()
			{
				prev = next = CHERI::Capability{this}.address() - Offset;
			}

			__always_inline auto cell_next()
			{
				return ds::pointer::proxy::OffsetPtrAddr<Offset, OffsetPtrAddr>(
				  this, next);
			}

			__always_inline auto cell_prev()
			{
				return ds::pointer::proxy::OffsetPtrAddr<Offset, OffsetPtrAddr>(
				  this, prev);
			}

			/*
			 * Specialized implementations that may be slightly fewer
			 * instructions than the generic approaches in terms of the primops.
			 */

			__always_inline bool cell_is_singleton()
			{
				return prev == CHERI::Capability{this}.address() - Offset;
			}

			__always_inline bool cell_is_doubleton()
			{
				return prev == next;
			}
		};
		static_assert(HasCellOperationsReset<OffsetPtrAddr<0>>);
		static_assert(HasIsSingleton<OffsetPtrAddr<0>>);
		static_assert(HasIsDoubleton<OffsetPtrAddr<0>>);

	} // namespace cell

} // namespace ds::linked_list
