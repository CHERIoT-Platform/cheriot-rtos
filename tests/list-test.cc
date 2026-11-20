// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "List"
#include "tests.hh"
#include <allocator.h>
#include <ds/linked_list.h>

using CHERI::Capability;

namespace
{
	/**
	 * Example class we want to link into a doubly linked list.
	 *
	 * The class contains a single integer for the purposes of the test.
	 *
	 * `ds::linked_list` is an intrusive list: we embed the list node into
	 * the class we want to link.  There are various implementations of the
	 * list nodes. Here we use the most simple one
	 * (`ds::linked_list::cell::Pointer`) which relies on two pointers
	 * `next` and `prev`.
	 */
	struct LinkedObject
	{
		using ObjectRing = ds::linked_list::cell::Pointer;

		int data;
		/**
		 * List node: links objects into the doubly-linked list.
		 */
		ObjectRing ring __attribute__((__cheri_no_subobject_bounds__)) = {};
		/**
		 * Container-of for the above field. This is used to retrieve the
		 * corresponding object from a list element.
		 */
		__always_inline static struct LinkedObject *from_ring(ObjectRing *c)
		{
			return reinterpret_cast<struct LinkedObject *>(
			  reinterpret_cast<uintptr_t>(c) -
			  offsetof(struct LinkedObject, ring));
		}
		/**
		 *
		 */
		__always_inline ObjectRing *to_ring()
		{
			return &this->ring;
		}
	};

} // namespace

/**
 * `ds::linked_list`s are circular, doubly-linked collections. While they can
 * stand on their own as rings of objects, it is sometimes convenient to create
 * a designated 'sentinel' node that participates in the collection without
 * being part of the collection:
 *
 * - a sentinel node provides pointers to the effective head and tail of the
 *   collection (the successor and predecessor of the sentinel, respectively)
 *
 * - a sentinel allows not having to special-case 'the collection is empty' in
 *   as many places as some other representations (that is, collections with
 *   sentinels need fewer NULL pointer checks)
 *
 * - a sentinel provides many handy functions to operate on the list
 *
 * Note: do not allocate the sentinel (or any list cell) on the stack, because
 * it would lead some list nodes to hold a pointer to a stack value, i.e., to
 * an invalid capability. This would manifest as a crash while using the list.
 */
ds::linked_list::TypedSentinel<LinkedObject,
                               LinkedObject::ObjectRing,
                               &LinkedObject::to_ring,
                               LinkedObject::from_ring>
  objects = {};

int test_list()
{
	debug_log("Testing the list implementation.");

	// Number of elements we will add to the list in the test. Must be
	// divisible by two.
	static constexpr int NumberOfListElements = 30;

	auto heapAtStart = heap_quota_remaining(MALLOC_CAPABILITY);

	TEST(objects.is_empty(), "Newly created list is not empty");

	// Create heap-allocated objects, and link them into the linked list.
	for (int i = 0; i < NumberOfListElements; i++)
	{
		Timeout       t{UnlimitedTimeout};
		LinkedObject *o = static_cast<LinkedObject *>(
		  heap_allocate(&t, MALLOC_CAPABILITY, sizeof(LinkedObject)));
		TEST(Capability{o}.is_valid(), "Cannot allocate linked object");

		// Use the object integer as an index.
		o->data = i;

		// Test that we can retrieve the object from the link node and
		// that this results in a capability which is identical to what
		// we got from the allocator.
		TEST(Capability{o} == Capability{LinkedObject::from_ring(&(o->ring))},
		     "The container of method does not return the right object");
		TEST(Capability{LinkedObject::from_ring(&(o->ring))}.is_valid(),
		     "Capability retrieved from `from_ring` is invalid");

		// Add the new object to the list through the sentinel node.
		objects.append_emplace(o);
	}

	TEST(!objects.is_empty(), "The list is empty after adding objects");

	// Test that the sentinel can be used to retrieve the first and last
	// elements of the list as expected.
	TEST(objects.last()->data == NumberOfListElements - 1,
	     "Last element of the list is incorrect, expected {}, got {}",
	     NumberOfListElements - 1,
	     objects.last()->data);
	TEST(objects.last()->ring.cell_next() == &objects.untyped.sentinel,
	     "Last element in not followed by the sentinel");
	TEST(&objects.last()->ring == objects.untyped.sentinel.cell_prev(),
	     "Sentinel is not preceeded by the last element");

	TEST(objects.first()->data == 0,
	     "First element of the list is incorrect, expected {}, got {}",
	     0,
	     objects.last()->data);
	TEST(objects.first()->ring.cell_prev() == &objects.untyped.sentinel,
	     "First element in not preceeded by the sentinel");
	TEST(&objects.first()->ring == objects.untyped.sentinel.cell_next(),
	     "Sentinel is not followed by the first element");

	// Test that we can go through the list by following `cell_next`
	// pointers as expected.  This peers through some of the abstractions the
	// (Typed)Sentinel provides.
	int counter = 0;
	// While at it, retrieve a pointer to the middle element which we will
	// use to cleave the list later.
	LinkedObject::ObjectRing *middle = nullptr;
	// We reach the sentinel when we have gone through all elements of the
	// list.
	for (auto *cell = &objects.first()->ring; cell != &objects.untyped.sentinel;
	     cell       = cell->cell_next())
	{
		struct LinkedObject *o = LinkedObject::from_ring(cell);
		TEST(
		  o->data == counter,
		  "Ordering of elements in the list is incorrect, expected {}, got {}",
		  o->data,
		  counter);
		if (counter == NumberOfListElements / 2)
		{
			middle = cell;
		}
		counter++;
	}

	TEST(middle != nullptr, "Could not find middle element of the list");

	// Do that again with the convenience affordances of TypedSentinel
	auto middleAgain = objects.search(
	  [](LinkedObject *o) { return o->data == NumberOfListElements / 2; });

	TEST(middleAgain && &middleAgain.value()->ring == middle,
	     "Failed to find the middle again");

	// Cut the list in the middle. `middle` is now a handle to the (valid)
	// collection of objects [middle, last] that have become detached from
	// the sentinel.
	ds::linked_list::remove(middle, &objects.last()->ring);

	// This should leave us with a list of size `NumberOfListElements / 2`.
	// Use the untyped search interface, for slightly better experience.
	counter = 0;
	objects.untyped.search([&counter](LinkedObject::ObjectRing *) {
		counter++;
		return false;
	});
	TEST(counter == NumberOfListElements / 2,
	     "Cleaving didn't leave a list with the right number of elements");

	// Now remove (and free) a single element from the list.
	TEST(objects.first()->data == 0,
	     "First element of the list is incorrect, expected {}, got {}",
	     0,
	     objects.first()->data);

	// We must keep a reference to the removed object to free it, as
	// `remove` returns a pointer to the residual list (return value which
	// we do not use here), not to the removed element.
	auto *removedNode = objects.first();
	ds::linked_list::remove(&removedNode->ring);

	TEST_EQUAL(heap_free(MALLOC_CAPABILITY, removedNode),
	           0,
	           "Failed to free removed cell");
	TEST(objects.first()->data == 1,
	     "First element of the list is incorrect after removing the first "
	     "element, expected {}, got {}",
	     1,
	     objects.first()->data);
	TEST(objects.first()->ring.cell_prev() == &objects.untyped.sentinel,
	     "First element in not preceeded by the sentinel after removing the "
	     "first object");

	// We are done with the list, free the first two using the unsafe sentinel
	// complete with update of the iterator and then the rest of it using the
	// more ergonomic TypedSentinel::search_safe.
	counter = 0;
	objects.untyped.search_safe([&counter](LinkedObject::ObjectRing *cell) {
		auto *toFree = LinkedObject::from_ring(cell);

		ds::linked_list::unsafe_remove(cell);

		TEST_EQUAL(heap_free(MALLOC_CAPABILITY, toFree),
		           0,
		           "Failed to free list object");
		counter++;

		return counter > 2;
	});

	objects.search_safe([&counter](LinkedObject *object) {
		ds::linked_list::unsafe_remove(&object->ring);
		TEST_EQUAL(heap_free(MALLOC_CAPABILITY, object),
		           0,
		           "Failed to free list object");
		counter++;
		return false;
	});

	TEST(counter == (NumberOfListElements / 2) - 1,
	     "Incorrect number of elements freed, expected {}, got {}",
	     (NumberOfListElements / 2) - 1,
	     counter);

	// Now that the list is freed, reset the sentinel.
	objects.reset();

	TEST(objects.is_empty(), "Reset-ed list is not empty");

	// We must also free the span of the list which we removed earlier.
	// This time use the raw `::search` method to go through that collection.
	ds::linked_list::search(
	  middle, [&counter](LinkedObject::ObjectRing *&cell) {
		  // `unsafe_remove` does not update the node pointers of the
		  // removed cell. This is great here because we will free the
		  // object anyways. We could also use `remove` here.
		  auto l = ds::linked_list::unsafe_remove(cell);
		  TEST_EQUAL(
		    heap_free(MALLOC_CAPABILITY, LinkedObject::from_ring(cell)),
		    0,
		    "Failed to free searched object");
		  // `l` is the predecessor of `cell` in the residual ring, so
		  // this does exactly what we want when `::search` iterates.
		  cell = l;
		  counter++;
		  return false;
	  });
	// `::search` does not visit the element passed (`middle`)
	TEST_EQUAL(heap_free(MALLOC_CAPABILITY, LinkedObject::from_ring(middle)),
	           0,
	           "Failed to free middle object");
	counter++;

	TEST(counter == NumberOfListElements - 1,
	     "Incorrect number of elements freed, expected {}, got {}",
	     NumberOfListElements - 1,
	     counter);

	// Check that we didn't leak anything in the process
	auto heapAtEnd = heap_quota_remaining(MALLOC_CAPABILITY);
	TEST(heapAtStart == heapAtEnd,
	     "The list leaked {} bytes ({} vs. {})",
	     heapAtEnd - heapAtStart,
	     heapAtStart,
	     heapAtEnd);

	debug_log("Done testing the list.");
	return 0;
}

namespace
{
	/**
	 * \defgroup TypedLinkedObject Non-Standard-Layout Intrusive Lists
	 *
	 * We can also use C++'s inheritance system for non-standard-layout classes.
	 * This requires a bit more work up front, but doesn't rely on `offsetof`.
	 *
	 * See `test_non_standard_layout_lists()`
	 *
	 * @{
	 */

	/**
	 * Linkage cell 1 for TypedLinkedObject
	 */
	struct BaseCell1 : public ds::linked_list::cell::functor::Pointer<BaseCell1>
	{
	};
	static_assert(ds::linked_list::cell::HasCellOperationsReset<BaseCell1>);

	/**
	 * Linkage cell 2 for TypedLinkedObject
	 */
	struct BaseCell2 : public ds::linked_list::cell::functor::Pointer<BaseCell2>
	{
	};
	static_assert(ds::linked_list::cell::HasCellOperationsReset<BaseCell2>);

	struct DerivedObject : public BaseCell1, public BaseCell2
	{
		int data;

		__always_inline struct BaseCell1 *to_cell1()
		{
			return static_cast<BaseCell1 *>(this);
		}

		__always_inline static struct DerivedObject *from_cell1(BaseCell1 *c)
		{
			return static_cast<DerivedObject *>(c);
		}

		/**
		 * We can even define TypedSentinel "containers" within the derived
		 * object itself, which is handy if we want this type to maintain things
		 * like "collections of all instances" or "all instances such that ...".
		 */
		static inline ds::linked_list::TypedSentinel<DerivedObject,
		                                             BaseCell1,
		                                             &DerivedObject::to_cell1,
		                                             DerivedObject::from_cell1>
		  sentinel1 = {};

		__always_inline struct BaseCell2 *to_cell2()
		{
			return static_cast<BaseCell2 *>(this);
		}

		__always_inline static struct DerivedObject *from_cell2(BaseCell2 *c)
		{
			return static_cast<DerivedObject *>(c);
		}

		DerivedObject(int d) : data(d)
		{
			sentinel1.append(this);
		}

		~DerivedObject()
		{
			ds::linked_list::unsafe_remove(to_cell1());
		}
	};

	/*
	 * This is emphatically not a standard layout class, since we have two base
	 * types with members as well as a member in the derived class itself.
	 */
	static_assert(!std::is_standard_layout_v<DerivedObject>);

	/**
	 * @}
	 */

} // namespace

ds::linked_list::TypedSentinel<DerivedObject,
                               BaseCell2,
                               &DerivedObject::to_cell2,
                               DerivedObject::from_cell2>
  typedSentinel2 = {};

int test_non_standard_layout_list()
{
	debug_log("Testing the non-standard layout list implementation.");

	// Number of elements we will add to the list in the test.
	static constexpr int NumberOfListElements = 10;

	auto heapAtStart = heap_quota_remaining(MALLOC_CAPABILITY);

	for (int i = 0; i < 10; i++)
	{
		auto *d = new DerivedObject(i);

		// Thread half the objects onto one list; the constructor threads
		// everything onto the other
		if (i & 1)
		{
			// new runs constructors, so linkages are initialized and we need
			// not use the emplace variant.
			typedSentinel2.prepend(d);
		}
	}

	// Free all the objects on the typedSentinel2 ring.
	typedSentinel2.search_safe([](DerivedObject *d) {
		ds::linked_list::unsafe_remove(d->to_cell2());

		// The destructor will manage unlinking us from DerivedObject::sentinel1
		delete d;

		return false;
	});

	DerivedObject::sentinel1.search_safe([](DerivedObject *d) {
		// Nodes still on the list were never part of the typedSentinel2 ring,
		// so they should have singleton BaseCell2 linkages.
		TEST(ds::linked_list::is_singleton(d->to_cell2()),
		     "Object has bad Cell2 linkages");

		delete d;
		return false;
	});

	// Check that we didn't leak anything in the process
	auto heapAtEnd = heap_quota_remaining(MALLOC_CAPABILITY);
	TEST(heapAtStart == heapAtEnd,
	     "The list leaked {} bytes ({} vs. {})",
	     heapAtEnd - heapAtStart,
	     heapAtStart,
	     heapAtEnd);

	debug_log("Done testing the non-standard layout list.");
	return 0;
}
