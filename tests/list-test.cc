// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "List"
#include "tests.hh"
#include <ds/linked_list.h>
#include <heap.hh>

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
ds::linked_list::Sentinel<LinkedObject::ObjectRing> objects = {};

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
		  heap_allocate_cpp(&t, MALLOC_CAPABILITY, sizeof(LinkedObject))
		    .as_pointer());
		TEST(Capability{o}.is_valid(), "Cannot allocate linked object");

		// Use the object integer as an index.
		o->data = i;
		// The list node has not yet been initialized.
		o->ring.cell_reset();

		// Test that we can retrieve the object from the link node and
		// that this results in a capability which is identical to what
		// we got from the allocator.
		TEST(Capability{o} == Capability{LinkedObject::from_ring(&(o->ring))},
		     "The container of method does not return the right object");
		TEST(Capability{LinkedObject::from_ring(&(o->ring))}.is_valid(),
		     "Capability retrieved from `from_ring` is invalid");

		// Add the new object to the list through the sentinel node.
		objects.append(&(o->ring));
	}

	TEST(!objects.is_empty(), "The list is empty after adding objects");

	// Test that the sentinel can be used to retrieve the first and last
	// elements of the list as expected.
	TEST(LinkedObject::from_ring(objects.last())->data ==
	       NumberOfListElements - 1,
	     "Last element of the list is incorrect, expected {}, got {}",
	     NumberOfListElements - 1,
	     LinkedObject::from_ring(objects.last())->data);
	TEST(objects.last()->cell_next() == &objects.sentinel,
	     "Last element in not followed by the sentinel");
	TEST(objects.last() == objects.sentinel.cell_prev(),
	     "Sentinel is not preceeded by the last element");

	TEST(LinkedObject::from_ring(objects.first())->data == 0,
	     "First element of the list is incorrect, expected {}, got {}",
	     0,
	     LinkedObject::from_ring(objects.last())->data);
	TEST(objects.first()->cell_prev() == &objects.sentinel,
	     "First element in not preceeded by the sentinel");
	TEST(objects.first() == objects.sentinel.cell_next(),
	     "Sentinel is not followed by the first element");

	// Test that we can go through the list by following `cell_next`
	// pointers as expected.
	int counter = 0;
	// While at it, retrieve a pointer to the middle element which we will
	// use to cleave the list later.
	LinkedObject::ObjectRing *middle = nullptr;
	// We reach the sentinel when we have gone through all elements of the
	// list.
	for (auto *cell = objects.first(); cell != &objects.sentinel;
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

	// Cut the list in the middle. `middle` is now a handle to the (valid)
	// collection of objects [middle, last] that have become detached from
	// the sentinel.
	ds::linked_list::remove(middle, objects.last());

	// This should leave us with a list of size `NumberOfListElements / 2`.
	counter = 0;
	for (auto *cell = objects.first(); cell != &objects.sentinel;
	     cell       = cell->cell_next())
	{
		counter++;
	}
	TEST(counter == NumberOfListElements / 2,
	     "Cleaving didn't leave a list with the right number of elements");

	// Now remove (and free) a single element from the list.
	TEST(LinkedObject::from_ring(objects.first())->data == 0,
	     "First element of the list is incorrect, expected {}, got {}",
	     0,
	     LinkedObject::from_ring(objects.first())->data);
	// We must keep a reference to the removed object to free it, as
	// `remove` returns a pointer to the residual list (return value which
	// we do not use here), not to the removed element.
	LinkedObject::ObjectRing *removedCell = objects.first();
	ds::linked_list::remove(objects.first());
	TEST_EQUAL(
	  heap_free(MALLOC_CAPABILITY, LinkedObject::from_ring(removedCell)),
	  0,
	  "Failed to free removed cell");
	TEST(LinkedObject::from_ring(objects.first())->data == 1,
	     "First element of the list is incorrect after removing the first "
	     "element, expected {}, got {}",
	     1,
	     LinkedObject::from_ring(objects.first())->data);
	TEST(objects.first()->cell_prev() == &objects.sentinel,
	     "First element in not preceeded by the sentinel after removing the "
	     "first object");

	// We are done with the list, free it.
	counter                        = 0;
	LinkedObject::ObjectRing *cell = objects.first();
	while (cell != &objects.sentinel)
	{
		struct LinkedObject *o = LinkedObject::from_ring(cell);
		cell                   = cell->cell_next();
		TEST_EQUAL(
		  heap_free(MALLOC_CAPABILITY, o), 0, "Failed to free list object");
		counter++;
	}

	TEST(counter == (NumberOfListElements / 2) - 1,
	     "Incorrect number of elements freed, expected {}, got {}",
	     (NumberOfListElements / 2) - 1,
	     counter);

	// Now that the list is freed, reset the sentinel.
	objects.reset();

	TEST(objects.is_empty(), "Reset-ed list is not empty");

	// We must also free the span of the list which we removed earlier.
	// This time use the `::search` method to go through the collection.
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
