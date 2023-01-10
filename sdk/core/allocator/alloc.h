// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "alloc_config.h"
#include "revoker.h"
#include <algorithm>
#include <cdefs.h>
#include <cheri.hh>
#include <ds/bits.h>
#include <ds/pointer.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

extern Revocation::Revoker revoker;

// the byte and bit size of a size_t
constexpr size_t BitsInSizeT   = utils::bytes2bits(sizeof(size_t));
constexpr size_t ChunkOverhead = MallocAlignment;
// Pad request bytes into a usable size.
static inline size_t pad_request(size_t req)
{
	return ((req) + ChunkOverhead + MallocAlignMask) & ~MallocAlignMask;
}

constexpr size_t NSmallBinsShift = 3U;
// number of small bins
constexpr size_t NSmallBins = 1U << NSmallBinsShift;
// number of large (tree) bins
constexpr size_t NTreeBins = 12U;
// shift needed to get the index of small bin
constexpr size_t SmallBinShift = MallocAlignShift;
// shift needed to get the index of tree bin
constexpr size_t TreeBinShift = MallocAlignShift + NSmallBinsShift;
// the max size (including header) that still falls in small bins
constexpr size_t MaxSmallSize = 1U << TreeBinShift;
// the maximum requested size that is still categorised as a small bin
constexpr size_t MaxSmallRequest = MaxSmallSize - ChunkOverhead;

// the compressed size. The actual size is SmallSize * MallocAlignment.
using SmallSize               = uint16_t;
constexpr size_t MaxChunkSize = (1U << utils::bytes2bits(sizeof(SmallSize)))
                                << MallocAlignShift;
// the compressed pointer. Used to point to prev and next in free lists.
using SmallPtr = size_t;
// the index to one of the bins
using BIndex = uint32_t;
// the bit map of all the bins. 1 for in-use and 0 for empty.
using Binmap = uint32_t;
static_assert(NSmallBins < utils::bytes2bits(sizeof(Binmap)));
static_assert(NTreeBins < utils::bytes2bits(sizeof(Binmap)));

// Convert small size header into the actual size in bytes.
static inline constexpr size_t head2size(SmallSize h)
{
	return static_cast<size_t>(h) << MallocAlignShift;
}
// Convert byte size into small size header.
static inline constexpr SmallSize size2head(size_t s)
{
	return s >> MallocAlignShift;
}

// Generate a bit mask for a single index.
static inline Binmap idx2bit(BIndex i)
{
	return static_cast<Binmap>(1) << i;
}

// index of the bit mask
static inline BIndex bit2idx(Binmap x)
{
	return ctz(x);
}

// Return treebin index for size s.
static inline BIndex compute_tree_index(size_t s)
{
	/*
	 * The upper bits of the size after TreeBinShift, if larger than this, must
	 * be thrown into the largest bin. Every two tree bins handle one
	 * power-of-two.
	 */
	constexpr size_t MaxTreeComputeMask = (1U << (NTreeBins >> 1)) - 1;

	size_t x = s >> TreeBinShift, k;
	if (x == 0)
	{
		return 0;
	}
	if (x > MaxTreeComputeMask)
	{
		return NTreeBins - 1;
	}
	k          = utils::bytes2bits(sizeof(x)) - 1 - clz(x);
	BIndex ret = (k << 1) + ((s >> (k + (TreeBinShift - 1)) & 1));
	Debug::Assert(
	  ret < NTreeBins, "Return value {} is out of range 0-{}", ret, NTreeBins);
	return ret;
}

// shift placing maximum resolved bit in a treebin at i as sign bit
static inline size_t leftshift_for_tree_index(BIndex i)
{
	return i == NTreeBins - 1 ? 0
	                          : (BitsInSizeT - ((i >> 1) + TreeBinShift - 1U));
}

/**
 * After size << leftshift_for_tree_index, the bit to be resolved must be at the
 * sign bit. Checking if the bit is 1 only needs to check val < 0. This assumes
 * 2's complement.
 *
 * @param val the value already shifted by leftshift_for_tree_index()
 */
static inline size_t leftshifted_val_msb(size_t val)
{
	return static_cast<ssize_t>(val) < 0 ? 1 : 0;
}

// the size of the smallest chunk held in bin with index i
static inline size_t minsize_for_tree_index(BIndex i)
{
	return (1U << ((i >> 1) + TreeBinShift)) |
	       ((1U & i) << ((i >> 1) + TreeBinShift - 1));
}

/**
 * The index of the small bin this size should live in.
 * Size 1 to MallocAlignment live in bin 0, MallocAlignment + 1 to
 * 2 * MallocAlignment live in bin 1, etc.
 */
static inline BIndex small_index(size_t s)
{
	return (s - 1) >> SmallBinShift;
}
// Is this a smallbin size?
static inline bool is_small(size_t s)
{
	return small_index(s) < NSmallBins;
}
// Convert smallbin index to the size it contains.
static inline size_t small_index2size(BIndex i)
{
	return (static_cast<size_t>(i) + 1) << SmallBinShift;
}

/**
 * When chunks are not in use, they are treated as nodes of either
 * lists or trees.
 *
 * Larger chunks are kept in a form of bitwise digital trees (aka
 * tries) keyed on chunksizes.  Because TChunk are only for
 * free chunks greater than 64 bytes, their size doesn't impose any
 * constraints on user chunk sizes.
 *
 * Each tree holding treenodes is a tree of unique chunk sizes.  Chunks
 * of the same size are arranged in a circularly-linked list, with only
 * the oldest chunk (the next to be used, in our FIFO ordering)
 * actually in the tree.  (Tree members are distinguished by a non-null
 * parent pointer.)  If a chunk with the same size an an existing node
 * is inserted, it is linked off the existing node using pointers that
 * work in the same way as fd/bk pointers of small chunks.
 *
 * Each tree contains a power of 2 sized range of chunk sizes (the
 * smallest is 0x100 <= x < 0x180), which is is divided in half at each
 * tree level, with the chunks in the smaller half of the range (0x100
 * <= x < 0x140 for the top nose) in the left subtree and the larger
 * half (0x140 <= x < 0x180) in the right subtree.  This is, of course,
 * done by inspecting individual bits.
 *
 * Using these rules, each node's left subtree contains all smaller
 * sizes than its right subtree.  However, the node at the root of each
 * subtree has no particular ordering relationship to either.  (The
 * dividing line between the subtree sizes is based on trie relation.)
 * If we remove the last chunk of a given size from the interior of the
 * tree, we need to replace it with a leaf node.  The tree ordering
 * rules permit a node to be replaced by any leaf below it.
 *
 * The smallest chunk in a tree (a common operation in a best-fit
 * allocator) can be found by walking a path to the leftmost leaf in
 * the tree.  Unlike a usual binary tree, where we follow left child
 * pointers until we reach a null, here we follow the right child
 * pointer any time the left one is null, until we reach a leaf with
 * both child pointers null. The smallest chunk in the tree will be
 * somewhere along that path.
 *
 * The worst case number of steps to add, find, or remove a node is
 * bounded by the number of bits differentiating chunks within
 * bins. Under current bin calculations, this ranges from 6 up to 21
 * (for 32 bit sizes) or up to 53 (for 64 bit sizes). The typical case
 * is of course much better.
 */

/**
 * Class of an allocator chunk, including the header.
 *
 * This class can reference either a smallbin chunk or a TChunk (and is the base
 * class of TChunk). Header fields should never be accessed directly since the
 * format is subject to change (hence private). The public wrappers also take
 * care of converting chunks and sizes into internal compressed formats.
 */
class __packed __aligned(MallocAlignment)
MChunk
{
	private:
	// compressed size of the previous chunk
	SmallSize sprev;
	// compressed size of this chunk
	SmallSize shead;
	bool      isPrevInUse : 1;
	bool      isCurrInUse : 1;
	// the revocation epoch when this chunk is quarantined.
	// TODO: handle overflow, especially when we add other fields which reduce
	// the bit width of the epoch in the future.
	size_t epochEnq : 30;

	/**
	 * Each MChunk participates in a circular doubly-linked list.  The
	 * exact form of that participation varies by the state and role of the
	 * node:
	 *
	 *   - Small chunks are on the ring whose sentinel node is a smallbin[]
	 *
	 *   - Large chunks directly included in the tree (TChunk-s) use this
	 *     link *as* the sentinel element of a ring of equally sized nodes
	 *
	 *   - Large chunks not directly included in the tree again use this
	 *     link as the link for the above ring.
	 *
	 */

	// internal ptr representation of the previous chunk in free list
	SmallPtr bk;
	// internal ptr representation of the next chunk in free list
	SmallPtr fd;

	// the internal small pointer representation of this chunk
	SmallPtr ptr()
	{
		return CHERI::Capability{this}.address();
	}

	/**
	 * The friend needs to access bk, fd and ptr() to rederive chunks.
	 * XXX: How do we grant access to only these?
	 */
	friend class MState;

	public:
	bool is_in_use()
	{
		return isCurrInUse;
	}
	bool is_prev_in_use()
	{
		return isPrevInUse;
	}

	// Treat the thing at thischunk + s as a chunk.
	MChunk *chunk_plus_offset(size_t s)
	{
		CHERI::Capability<void> ptr{this};
		ptr.address() += s;
		return ptr.cast<MChunk>();
	}
	// Returns the next adjacent chunk.
	MChunk *chunk_next()
	{
		return chunk_plus_offset(size_get());
	}
	// Returns the previous adjacent chunk.
	MChunk *chunk_prev()
	{
		return chunk_plus_offset(-prevsize_get());
	}

	// size of the previous chunk
	size_t prevsize_get()
	{
		return head2size(sprev);
	}
	// size of this chunk
	size_t size_get()
	{
		return head2size(shead);
	}
	/**
	 * Set the size of this chunk, which also takes care of converting sz into
	 * the internal header format and setting the prev field in the next chunk.
	 */
	void size_set(size_t sz)
	{
		shead               = size2head(sz);
		chunk_next()->sprev = size2head(sz);
	}

	/**
	 * Set the in-use bit of this chunk, which takes care of setting the
	 * previous-in-use bit in the next chunk as well. Should only be called when
	 * size has been populated, otherwise it will not find the next chunk.
	 */
	void in_use_set()
	{
		isCurrInUse               = true;
		chunk_next()->isPrevInUse = true;
	}
	/**
	 * Clear the in-use bit of this chunk and the previous-in-use bit of the
	 * next chunk.  Size of this chunk must have been set.
	 */
	void in_use_clear()
	{
		isCurrInUse               = false;
		chunk_next()->isPrevInUse = false;
	}
	/**
	 * @brief Set this chunk as an in-use chunk with a given size, which takes
	 * care of setting fields in the next chunk as well.
	 *
	 * @param sz the size of this chunk
	 */
	void in_use_chunk_set(size_t sz)
	{
		size_set(sz);
		in_use_set();
	}
	/**
	 * @brief Set this chunk as a free chunk with a given size, which takes care
	 * of setting fields in the next chunk as well.
	 *
	 * @param sz the size of this free chunk
	 */
	void free_chunk_set(size_t sz)
	{
		size_set(sz);
		in_use_clear();
	}
	/**
	 * Set this chunk as the footer chunk of a region.
	 *
	 * The reason why we can't use the above functions is that this is the very
	 * end of a region and there's no next chunk to play with.
	 */
	void footchunk_set(size_t sz)
	{
		isCurrInUse = true;
		shead       = size2head(sz);
	}

	// Set the internal back pointer to point to bkchunk.
	void bk_assign(MChunk * bkchunk)
	{
		bk = bkchunk->ptr();
	}
	// Is the bk field pointing to p?
	bool bk_equals(MChunk * p)
	{
		return bk == p->ptr();
	}
	// Returns the address of the bk chunk.
	size_t bk_addr()
	{
		return bk;
	}
	// Returns the address of the fd chunk.
	size_t fd_addr()
	{
		return fd;
	}
	// Set the internal forward pointer to point to fdchunk.
	void fd_assign(MChunk * fdchunk)
	{
		fd = fdchunk->ptr();
	}
	// Is the fd field pointing to p?
	bool fd_equals(MChunk * p)
	{
		return fd == p->ptr();
	}

#ifdef NDEBUG
	bool ok_in_use()
	{
		return true;
	}
	bool ok_prev_in_use()
	{
		return true;
	}
	bool ok_next(MChunk * n)
	{
		return true;
	}
#else
	bool ok_in_use()
	{
		return is_in_use();
	}
	bool ok_prev_in_use()
	{
		return is_prev_in_use();
	}
	// Sanity check that the next chunk is indeed higher than this one.
	bool ok_next(MChunk * n)
	{
		return ptr() < n->ptr();
	}
#endif

	// Write the revocation epoch into the header.
	void epoch_write()
	{
		epochEnq = revoker.system_epoch_get();
	}

	// Has this chunk gone through a full revocation pass?
	bool is_revocation_done()
	{
		return revoker.has_revocation_finished_for_epoch(epochEnq);
	}
};
static_assert(sizeof(MChunk) == 16);
// the minimum size of a chunk (including the header)
constexpr size_t MinChunkSize =
  (sizeof(MChunk) + MallocAlignMask) & ~MallocAlignMask;
// the minimum size of a chunk (excluding the header)
constexpr size_t MinRequest = MinChunkSize - ChunkOverhead;

// Convert a chunk to a user pointer.
static inline CHERI::Capability<void> chunk2mem(CHERI::Capability<MChunk> p)
{
	p.address() += MallocAlignment;
	return p.cast<void>();
}
// Convert a user pointer back to the chunk.
static inline MChunk *mem2chunk(CHERI::Capability<void> p)
{
	p.address() -= MallocAlignment;
	return p.cast<MChunk>();
}

// true if cap address a has acceptable alignment
static inline bool is_aligned(CHERI::Capability<void> a)
{
	return (a.address() & MallocAlignMask) == 0;
}
// true if the size has acceptable alignment
static inline bool is_aligned(size_t s)
{
	return (s & MallocAlignMask) == 0;
}
// the number of bytes to offset an address to align it
static inline size_t align_offset(CHERI::Capability<void> a)
{
	return is_aligned(a) ? 0
	                     : MallocAlignment - (a.address() & MallocAlignMask);
}

/**
 * Format of a large chunk living in large tree bins. The beginning of the
 * header shares the same format with small chunk, and may be accessed with
 * a MChunk pointer when not performing tree operations.
 *
 * Since we have enough room (large/tree chunks are at least 65 bytes), we just
 * put full capabilities here, and the format probably won't change, ever.
 */
class __packed __aligned(MallocAlignment)
TChunk : public MChunk
{
	public:
	// pointers to left and right children in the tree
	TChunk *child[2];
	// pointer to parent
	TChunk *parent;
	// the tree index this chunk is in
	BIndex index;

	TChunk *leftmost_child()
	{
		if (child[0] != nullptr)
			return child[0];
		return child[1];
	}

	static constexpr uintptr_t RingParent = 0;
	static constexpr uintptr_t RootParent = 1;

	bool is_root()
	{
		return reinterpret_cast<uintptr_t>(parent) == RootParent;
	}
	void mark_root()
	{
		parent = reinterpret_cast<TChunk *>(RootParent);
	}

	bool is_tree_ring()
	{
		return parent == reinterpret_cast<TChunk *>(RingParent);
	}
	void mark_tree_ring()
	{
		parent = reinterpret_cast<TChunk *>(RingParent);
	}
};

class MState
{
	public:
	CHERI::Capability<void> heapStart;

	/**
	 * This is not an array of MChunk* but is actually an array of {bk, fd}
	 * fields. Each smallbin has a fake head and we only need bk and fd for
	 * link lists against real chunks. Also see smallbin_at().
	 */
	MChunk *smallbins[(NSmallBins + 1)];
	// The head for each tree bin. Unlike smallbin, no magic is involved.
	TChunk *treebins[NTreeBins];
	// Same with smallbin, we only need bk and fd for the quarantine fake head.
	SmallPtr quarantineBinBk;
	SmallPtr quarantineBinFd;
	// bitmap telling which bins are empty which are not
	Binmap smallmap;
	Binmap treemap;
	size_t heapTotalSize;
	size_t heapFreeSize;
	size_t heapQuarantineSize;

	bool is_free_cap_inbounds(CHERI::Capability<void> mem)
	{
		return mem.is_subset_of(heapStart);
	}

	/// Rederive a capability to a `T` from the heap range.  This does not
	/// apply bounds to the result.
	template<typename T>
	T *rederive(SmallPtr ptr)
	{
		CHERI::Capability<T> cap{heapStart.cast<T>()};
		cap.address() = ptr;
		return cap;
	}

	// Rederive an internal pointer into a chunk in this heap segment.
	MChunk *ptr2chunk(SmallPtr ptr)
	{
		return rederive<MChunk>(ptr);
	}
	// Rederive an internal pointer into a tree chunk in this heap segment.
	TChunk *ptr2tchunk(SmallPtr ptr)
	{
		return rederive<TChunk>(ptr);
	}

	// Returns the smallbin head for index i.
	MChunk *smallbin_at(BIndex i)
	{
		/**
		 * Here we use the "repositioning" trick which is beyond evil. Initially
		 * when a smallbin is empty it has a fake head node which has bk and fd
		 * pointing to itself. When small chunks get added, they will form a
		 * doubly-linked list starting from head. Note that we only use the bk
		 * and fd fields of this fake head so each smallbins[i] is not an
		 * MChunk* but is actually bk and fd for (MChunk*)&smallbin[i - 1]. This
		 * is also why we need smallbin[NSmallBins + 1].
		 *
		 * This also means an MChunk* must be able to contain bk and fd.
		 */
		static_assert(sizeof(MChunk *) >= sizeof(SmallPtr) * 2);
		return reinterpret_cast<MChunk *>(&smallbins[i]);
	}
	// Returns the quarantine bin. Same evil repositioning trick as smallbin[].
	MChunk *qtbin()
	{
		CHERI::Capability<void> cap{&quarantineBinBk};
		cap.address() -= ChunkOverhead;
		return cap.cast<MChunk>();
	}
	// Returns a pointer to the pointer to the root chunk of a tree.
	TChunk **treebin_at(BIndex i)
	{
		return &treebins[i];
	}

	// Mark the bit at smallbin index i.
	void smallmap_mark(BIndex i)
	{
		smallmap |= idx2bit(i);
	}
	// Clear the bit at smallbin index i.
	void smallmap_clear(BIndex i)
	{
		smallmap &= ~idx2bit(i);
	}
	// Is the bit at smallbin index i set?
	bool is_smallmap_marked(BIndex i)
	{
		return smallmap & idx2bit(i);
	}

	// Mark the bit at treebin index i.
	void treemap_mark(BIndex i)
	{
		treemap |= idx2bit(i);
	}
	// Clear the bit at treebin index i.
	void treemap_clear(BIndex i)
	{
		treemap &= ~idx2bit(i);
	}
	// Is the bit at treebin index i set?
	bool is_treemap_marked(BIndex i)
	{
		return treemap & idx2bit(i);
	}

	// Initialize bins for a new MState.
	void init_bins()
	{
		// Establish circular links for smallbins.
		BIndex i;
		for (i = 0; i < NSmallBins; ++i)
		{
			MChunk *bin = smallbin_at(i);
			bin->fd_assign(bin);
			bin->bk_assign(bin);
		}
		// The treebins should be all nullptrs due to memset earlier.
		// Initialise quarantine bin.
		qtbin()->fd_assign(qtbin());
		qtbin()->bk_assign(qtbin());
	}

	/**
	 * @brief Called when adding the first memory chunk to this MState.
	 * At the moment we only support one contiguous MChunk for each MState.
	 *
	 * @param p The chunk used to initialise the MState. It must have the
	 * previous-in-use bit set and the next chunk's in-use bit set to prevent
	 * free() from coalescing below and above, unless you really know what you
	 * are doing.
	 */
	void mspace_firstchunk_add(MChunk *p)
	{
		// Should only be called once during mstate_init() for now.
		Debug::Assert((heapTotalSize == 0) && (heapFreeSize == 0),
		              "First chunk added more than once.  Heap size {} and "
		              "free size {} should both be 0",
		              heapTotalSize,
		              heapFreeSize);
		heapTotalSize += p->size_get();
		mspace_free_internal(p);
	}

	/**
	 * Tag type indicating that the requested allocation can never succeed.
	 */
	struct AllocationFailurePermanent
	{
	};

	/**
	 * Tag type indicating that the requested allocation may be able to succeed
	 * once the revoker has finished, as long as nothing else uses the
	 * available memory until then.
	 */
	struct AllocationFailureRevocationNeeded
	{
	};

	/**
	 * Tag type indicating that the requested allocation cannot succeed until
	 * some objects have been freed.
	 */
	struct AllocationFailureDeallocationNeeded
	{
	};

	/**
	 * Possible outcomes of an attempt to allocate memory.
	 */
	using AllocationResult = std::variant<AllocationFailurePermanent,
	                                      AllocationFailureRevocationNeeded,
	                                      AllocationFailureDeallocationNeeded,
	                                      CHERI::Capability<void>>;

	/**
	 * @brief Adjust size and alignment to ensure precise representability, and
	 * paint the shadow bit for the header to detect valid free().
	 *
	 * @return User pointer if request can be satisfied, nullptr otherwise.
	 */
	AllocationResult mspace_dispatch(size_t bytes)
	{
		size_t alignSize = CHERI::representable_length(bytes);
		// This 0 size check is for:
		// 1. We choose to return nullptr for 0-byte requests.
		// 2. crrl overflows to 0 if bytes is too big. Force failure.
		if (alignSize == 0)
		{
			return AllocationFailurePermanent{};
		}
		CHERI::Capability<void> ret{mspace_memalign(
		  alignSize, -CHERI::representable_alignment_mask(bytes))};
		if (ret == nullptr)
		{
			auto neededSize = alignSize + ChunkOverhead;
			if (heapQuarantineSize > 0 &&
			    (heapQuarantineSize + heapFreeSize) >= neededSize)
			{
				return AllocationFailureRevocationNeeded{};
			}
			if (heapTotalSize < neededSize)
			{
				return AllocationFailurePermanent{};
			}
			return AllocationFailureDeallocationNeeded{};
		}

#ifndef NDEBUG
		// Periodically sanity check the entire state for this mspace.
		static size_t sanityCounter = 0;
		if (sanityCounter % MStateSanityInterval == 0)
		{
			ok_malloc_state();
		}
		++sanityCounter;
#endif
		/*
		 * We abuse shadow bits to give us a marker for the allocated address.
		 * On free, we check that the allocation granule right below the address
		 * should have the shadow bit set, which means this is a valid thing for
		 * free();
		 */
		revoker.shadow_paint_single(ret.address() - MallocAlignment, true);

		ret.bounds() = alignSize;
		return ret;
	}

	/**
	 * @brief Free a valid user cap into this MState. This only places the chunk
	 * onto the quarantine list. It tries to dequeue the quarantine list a
	 * couple of times.
	 *
	 * @param mem the user cap which has been checked, but has not been
	 * rederived into an internal cap yet
	 */
	void mspace_free(CHERI::Capability<void> mem)
	{
		// Expand the bounds of the freed object to the whole heap and set the
		// address that we're looking at to the base of the requested
		// capability.
		ptraddr_t base = mem.base();
		mem            = heapStart;
		mem.address()  = base;
		/*
		 * Rederived into allocator capability. From now on faults on this
		 * pointer are internal.
		 */
		MChunk *p = mem2chunk(mem);
		// At this point, we know mem is capability aligned.
		capaligned_zero(mem, p->size_get() - ChunkOverhead);
		revoker.shadow_paint_range(mem.address(), p->chunk_next()->ptr(), true);
		/*
		 * Shadow bits have been painted. From now on user caps to this chunk
		 * will not come to exist in registers due to load barrier.  Any that
		 * are already there will be addressed (either zeroed or reloaded and,
		 * so, tag-cleared) when we return from this compartment.
		 */
		p->epoch_write();
		// Place it on the quarantine list after writing epoch.
		MChunk *head = qtbin();
		MChunk *back = ptr2chunk(head->bk);
		head->bk_assign(p);
		back->fd_assign(p);
		p->fd_assign(head);
		p->bk_assign(back);
		heapQuarantineSize += p->size_get();
		// Dequeue 3 times. 3 is chosen randomly. 2 is at least needed.
		mspace_qtbin_deqn(3);
		mspace_bg_revoker_kick<false>();
	}

	/**
	 * Try to dequeue some objects from quarantine.
	 *
	 * Returns true if any objects were dequeued, false otherwise.
	 */
	__always_inline bool quarantine_dequeue()
	{
		// 4 chosen by fair die roll.
		return mspace_qtbin_deqn(4) > 0;
	}

	private:
	/**
	 * @brief helper to perform operation on a range of capability words
	 * @param start beginning of the range
	 * @param size the size of the range
	 * @param fn what to be done for each word. Returns true to break loop
	 * @return true if something breaks the loop. false otherwise
	 */
	static bool __always_inline capaligned_range_do(void  *start,
	                                                size_t size,
	                                                bool (*fn)(void *&))
	{
		Debug::Assert((size & (sizeof(void *) - 1)) == 0,
		              "Cap range is not aligned");
		void **capstart = static_cast<void **>(start);
		for (size_t i = 0; i < size / sizeof(void *); ++i)
		{
			if (fn(capstart[i]))
			{
				return true;
			}
		}

		return false;
	}

	static void capaligned_zero(void *start, size_t size)
	{
		capaligned_range_do(start, size, [](void *&word) {
			word = nullptr;
			return false;
		});
	}

	/// Zero all metadata fields.
	template<bool IncludeHeader>
	static void metadata_zero(MChunk *chunk)
	{
		// Sadly, we cannot just treat an MChunk as an MChunk, because it's
		// possible it used to be a TChunk. So, we always have to zero as much
		// as possible, up to how much a TChunk uses for metadata.
		size_t sz = std::min(chunk->size_get(), sizeof(TChunk));
		void  *start;
		if constexpr (IncludeHeader)
		{
			start = chunk;
		}
		else
		{
			start = chunk2mem(chunk);
			sz -= ChunkOverhead;
		}
		// We can use memset(), but we know things are nicely aligned so just
		// use the helper for zeroing.
		capaligned_zero(start, sz);
	}

	/**
	 * Internal debug checks. Crashes the allocator when inconsistency detected.
	 * No-ops in Release build.
	 */
#ifdef NDEBUG
	bool ok_address(ptraddr_t a)
	{
		return true;
	}
	bool ok_address(void *)
	{
		return true;
	}
	void ok_any_chunk(MChunk *p) {}
	void ok_in_use_chunk(MChunk *p) {}
	void ok_free_chunk(MChunk *p) {}
	void ok_malloced_chunk(void *mem, size_t s) {}
	void ok_treebin(BIndex i) {}
	void ok_smallbin(BIndex i) {}
	void ok_malloc_state() {}
#else
	bool ok_address(ptraddr_t a)
	{
		return a >= heapStart.address();
	}
	bool ok_address(void *p)
	{
		return ok_address(CHERI::Capability{p}.address());
	}
	void ok_any_chunk(MChunk *p)
	{
		Debug::Assert(
		  is_aligned(chunk2mem(p)), "Chunk is not correctly aligned: {}", p);
		Debug::Assert(
		  ok_address(p->ptr()), "Invalid address {} for chunk", p->ptr());
	}
	// Sanity check an in-use chunk.
	void ok_in_use_chunk(MChunk *p)
	{
		ok_any_chunk(p);
		Debug::Assert(p->is_in_use(), "In use chunk {} is not in use", p);
		Debug::Assert(p->chunk_next()->is_prev_in_use(),
		              "In use chunk {} is in a list with chunk that expects it "
		              "to not be in use",
		              p);
		Debug::Assert(p->is_prev_in_use() || p->chunk_prev()->chunk_next() == p,
		              "Previous chunk is not in use or the chunk list is "
		              "corrupt for chunk {}",
		              p);
	}
	// Sanity check a free chunk.
	void ok_free_chunk(MChunk *p)
	{
		size_t  sz   = p->size_get();
		MChunk *next = p->chunk_plus_offset(sz);
		ok_any_chunk(p);
		Debug::Assert(!p->is_in_use(), "Free chunk {} is marked as in use", p);
		Debug::Assert(
		  !p->chunk_next()->is_prev_in_use(),
		  "Free chunk {} is in list with chunk that expects it to be in use",
		  p);
		if (sz >= MinChunkSize)
		{
			Debug::Assert((sz & MallocAlignMask) == 0,
			              "Chunk size {} is incorrectly aligned",
			              sz);
			Debug::Assert(is_aligned(chunk2mem(p)),
			              "Chunk {} is insufficiently aligned",
			              p);
			Debug::Assert(
			  next->prevsize_get() == sz,
			  "Chunk {} has size {}, next node expects its size to be {}",
			  sz,
			  next->prevsize_get());
			Debug::Assert(p->is_prev_in_use(),
			              "Free chunk {} should follow an in-use chunk");
			Debug::Assert(
			  next->is_in_use(),
			  "Free chunk {} should be followed by an in-use chunk");
			Debug::Assert(
			  ptr2chunk(p->fd)->bk_equals(p),
			  "Forward and backwards chunk pointers are inconsistent for {}",
			  p);
			Debug::Assert(
			  ptr2chunk(p->bk)->fd_equals(p),
			  "Forward and backwards chunk pointers are inconsistent for {}",
			  p);
		}
		else // Markers are always of size ChunkOverhead.
		{
			Debug::Assert(sz == ChunkOverhead,
			              "Marker chunk size is {}, should always be {}",
			              sz,
			              ChunkOverhead);
		}
	}
	// Sanity check a chunk that was just malloced.
	void ok_malloced_chunk(void *mem, size_t s)
	{
		if (mem != nullptr)
		{
			MChunk *p  = mem2chunk(mem);
			size_t  sz = p->size_get();
			ok_in_use_chunk(p);
			Debug::Assert((sz & MallocAlignMask) == 0,
			              "Chunk size {} is insufficiently aligned",
			              sz);
			Debug::Assert(sz >= MinChunkSize,
			              "Chunk size {} is smaller than minimum size {}",
			              sz,
			              MinChunkSize);
			Debug::Assert(sz >= s,
			              "Chunk size {} is smaller that requested size {}",
			              sz,
			              s);
			/*
			 * Size is less than MinChunkSize more than request. If this does
			 * not hold, then it should have been split into two mallocs.
			 */
			Debug::Assert(sz < (s + MinChunkSize),
			              "Chunk of size {} returned for requested size {}, "
			              "should have been split",
			              sz,
			              s);
		}
	}

	/**
	 * @brief Advance to the next tree chunk.
	 */
	void ok_tree_next(TChunk *from, TChunk *t)
	{
		if (from == t->parent)
		{
			/* Came from parent; descend as leftwards as we can */
			if (t->child[0])
			{
				[[clang::musttail]] return ok_tree(t, t->child[0]);
			}
			if (t->child[1])
			{
				[[clang::musttail]] return ok_tree(t, t->child[1]);
			}

			/* Leaf nodes fall through */
		}
		else if (from == t->child[0] && t->child[1])
		{
			/* Came from left child; descend right if there is one */
			[[clang::musttail]] return ok_tree(t, t->child[1]);
		}

		if (!t->is_root())
		{
			/* Leaf node or came from rightmost child; ascend */
			[[clang::musttail]] return ok_tree_next(t, t->parent);
		}

		/* Otherwise, we are the root node and we have nowhere to go, so stop */
	}
	/**
	 * @brief Sanity check all the chunks in a tree.
	 *
	 * @param from the previously visited node or TChunk::RootParent
	 * @param t the root of a whole tree or a subtree
	 */
	void ok_tree(TChunk *from, TChunk *t)
	{
		BIndex tindex = t->index;
		size_t tsize  = t->size_get();
		BIndex idx    = compute_tree_index(tsize);
		Debug::Assert(
		  tindex == idx, "Chunk index {}, expected {}", tindex, idx);
		Debug::Assert(tsize > MaxSmallSize,
		              "Size {} is smaller than the minimum size",
		              tsize);
		Debug::Assert(tsize >= minsize_for_tree_index(idx),
		              "Size {} is smaller than the minimum size for tree {}",
		              tsize,
		              idx);
		Debug::Assert((idx == NTreeBins - 1) ||
		                (tsize < minsize_for_tree_index((idx + 1))),
		              "Tree shape is invalid");

		/* Properties of this tree node */
		ok_any_chunk(t);
		ok_free_chunk(t);
		Debug::Assert(t->index == tindex,
		              "Chunk has index {}, expected {}",
		              t->index,
		              tindex);
		Debug::Assert(t->size_get() == tsize,
		              "Chunk has size {}, expected {}",
		              t->size_get(),
		              tsize);
		Debug::Assert(
		  !t->is_tree_ring(), "Tree node {} marked as tree ring node", t);
		Debug::Assert(t->parent != t, "Chunk {} is its own parent", t);
		Debug::Assert(t->is_root() || t->parent->child[0] == t ||
		                t->parent->child[1] == t,
		              "Chunk {} is neither root nor a child of its parent");

		/* Equal-sized chunks */
		TChunk *u = ptr2tchunk(t->fd);

		while (u != t)
		{ // Traverse through chain of same-sized nodes.
			ok_any_chunk(u);
			ok_free_chunk(u);
			Debug::Assert(
			  u->size_get() == tsize, "Large chunk {} has wrong size", u);
			Debug::Assert(
			  u->is_tree_ring(), "Chunk {} is not in tree but has parent", u);
			Debug::Assert(u->child[0] == nullptr,
			              "Chunk {} has no parent but has a child {}",
			              u,
			              u->child[0]);
			Debug::Assert(u->child[1] == nullptr,
			              "Chunk {} has no parent but has a child {}",
			              u,
			              u->child[1]);
			u = ptr2tchunk(u->fd);
		};

		auto checkChild = [&](int childIndex) {
			if (t->child[childIndex] != nullptr)
			{
				Debug::Assert(t->child[childIndex]->parent == t,
				              "Chunk {} has child {} ({}) that has parent {}",
				              t,
				              childIndex,
				              t->child[childIndex],
				              t->child[childIndex]->parent);
				Debug::Assert(t->child[childIndex] != t,
				              "Chunk {} is its its own child ({})",
				              t,
				              childIndex);
				Debug::Assert(t->child[childIndex]->size_get() != tsize,
				              "Chunk {} has child {} with equal size {}",
				              t,
				              t->child[childIndex],
				              tsize);
			}
		};
		checkChild(0);
		checkChild(1);
		if ((t->child[0] != nullptr) && (t->child[1] != nullptr))
		{
			Debug::Assert(t->child[0]->size_get() < t->child[1]->size_get(),
			              "Chunk {}'s children are not sorted by size",
			              t);
		}

		[[clang::musttail]] return ok_tree_next(from, t);
	}
	// Sanity check the tree at treebin[i].
	void ok_treebin(BIndex i)
	{
		TChunk **tb    = treebin_at(i);
		TChunk  *t     = *tb;
		bool     empty = (treemap & (1U << i)) == 0;
		if (t == nullptr)
		{
			Debug::Assert(empty, "Null tree is not empty");
		}
		if (!empty)
		{
			Debug::Assert(
			  t->is_root(), "Non-empty bin has non-root tree node {}", t);
			ok_tree(t->parent, t);
		}
	}
	// Sanity check smallbin[i].
	void ok_smallbin(BIndex i)
	{
		MChunk *b     = smallbin_at(i);
		MChunk *p     = ptr2chunk(b->bk);
		bool    empty = (smallmap & (1U << i)) == 0;
		if (p == b)
		{
			Debug::Assert(empty, "Small bin should be empty");
		}
		if (!empty)
		{
			for (; p != b; p = ptr2chunk(p->bk))
			{
				size_t  size = p->size_get();
				MChunk *q;
				// Each chunk claims to be free.
				ok_free_chunk(p);
				// Chunk belongs in this bin.
				Debug::Assert(
				  small_index(size) == i,
				  "Chunk is in bin with index {} but should be in {}",
				  i,
				  small_index(size));
				Debug::Assert(p->bk_equals(b) ||
				                ptr2chunk(p->bk)->size_get() == p->size_get(),
				              "Back pointers do not match on size");
			}
		}
	}
	// Sanity check the entire malloc state in this MState.
	void ok_malloc_state()
	{
		BIndex i;
		size_t total;
		// Check all the bins.
		for (i = 0; i < NSmallBins; ++i)
		{
			ok_smallbin(i);
		}
		for (i = 0; i < NTreeBins; ++i)
		{
			ok_treebin(i);
		}
	}
#endif

	private:
	// Link a free chunk into a smallbin.
	void insert_small_chunk(MChunk *p, size_t size)
	{
		BIndex  i    = small_index(size);
		MChunk *head = smallbin_at(i);
		MChunk *back = head;
		Debug::Assert(
		  size >= MinChunkSize, "Size {} is not a small chunk size", size);
		if (!is_smallmap_marked(i))
		{
			smallmap_mark(i);
		}
		else if (RTCHECK(ok_address(head->bk)))
		{
			back = ptr2chunk(head->bk);
		}
		else
		{
			corruption_error_action();
		}
		head->bk_assign(p);
		back->fd_assign(p);
		p->fd_assign(head);
		p->bk_assign(back);
	}

	/// Unlink a chunk from a smallbin.
	void unlink_small_chunk(MChunk *p, size_t s)
	{
		MChunk *f = ptr2chunk(p->fd);
		MChunk *b = ptr2chunk(p->bk);
		BIndex  i = small_index(s);
		Debug::Assert(p != b, "Chunk {} is circularly referenced", p);
		Debug::Assert(p != f, "Chunk {} is circularly referenced", p);
		Debug::Assert(p->size_get() == small_index2size(i),
		              "Chunk {} is has size {} but is in bin for size {}",
		              p,
		              p->size_get(),
		              small_index2size(i));
		if (RTCHECK(f == smallbin_at(i) ||
		            (ok_address(f->ptr()) && f->bk_equals(p))))
		{
			if (b == f)
			{
				// This is the last chunk in this bin.
				smallmap_clear(i);
			}
			else if (RTCHECK(b == smallbin_at(i) ||
			                 (ok_address(b->ptr()) && b->fd_equals(p))))
			{
				f->bk_assign(b);
				b->fd_assign(f);
			}
			else
			{
				corruption_error_action();
			}
		}
		else
		{
			corruption_error_action();
		}
	}

	// Unlink the first chunk from a smallbin.
	MChunk *unlink_first_small_chunk(BIndex i)
	{
		auto b = smallbin_at(i);
		auto p = ptr2chunk(b->fd);

		auto f = ptr2chunk(p->fd);

		Debug::Assert(p != b, "Chunk {} is circularly referenced", p);
		Debug::Assert(p != f, "Chunk {} is circularly referenced", p);
		Debug::Assert(p->size_get() == small_index2size(i),
		              "Chunk {} is has size {} but is in bin for size {}",
		              p,
		              p->size_get(),
		              small_index2size(i));
		if (b == f)
		{
			smallmap_clear(i);
		}
		else if (RTCHECK(ok_address(f->ptr()) && f->bk_equals(p)))
		{
			f->bk_assign(b);
			b->fd_assign(f);
		}
		else
		{
			corruption_error_action();
		}

		return p;
	}

	// Insert chunk into tree.
	void insert_large_chunk(TChunk *x, size_t s)
	{
		TChunk **head;
		BIndex   i  = compute_tree_index(s);
		head        = treebin_at(i);
		x->index    = i;
		x->child[0] = x->child[1] = nullptr;
		if (!is_treemap_marked(i))
		{
			treemap_mark(i);
			*head = x;
			x->mark_root();
			x->fd_assign(x);
			x->bk_assign(x);
		}
		else
		{
			TChunk *t = *head;
			size_t  k = s << leftshift_for_tree_index(i);
			for (;;)
			{
				if (t->size_get() != s)
				{
					CHERI::Capability<TChunk *> c =
					  &(t->child[leftshifted_val_msb(k)]);
					k <<= 1;
					if (*c != nullptr)
					{
						t = *c;
					}
					else if (RTCHECK(ok_address(c.address())))
					{
						*c        = x;
						x->parent = t;
						x->fd_assign(x);
						x->bk_assign(x);
						break;
					}
					else
					{
						corruption_error_action();
						break;
					}
				}
				else
				{
					TChunk *back = ptr2tchunk(t->bk);
					if (RTCHECK(ok_address(t->ptr()) &&
					            ok_address(back->ptr())))
					{
						t->bk_assign(x);
						back->fd_assign(x);
						x->fd_assign(t);
						x->bk_assign(back);
						x->mark_tree_ring();
						break;
					}

					corruption_error_action();
					break;
				}
			}
		}
	}

	/**
	 * Unlink steps:
	 *
	 * 1. If x is a chained node, unlink it from its same-sized fd/bk links
	 *     and choose its bk node as its replacement.
	 * 2. If x was the last node of its size, but not a leaf node, it must
	 *     be replaced with a leaf node (not merely one with an open left or
	 *     right), to make sure that lefts and rights of descendents
	 *     correspond properly to bit masks.  We use the rightmost descendent
	 *     of x.  We could use any other leaf, but this is easy to locate and
	 *     tends to counteract removal of leftmosts elsewhere, and so keeps
	 *     paths shorter than minimally guaranteed.  This doesn't loop much
	 *     because on average a node in a tree is near the bottom.
	 * 3. If x is the base of a chain (i.e., has parent links) relink
	 *     x's parent and children to x's replacement (or null if none).
	 */
	void unlink_large_chunk(TChunk *x)
	{
		TChunk *xp = x->parent;
		TChunk *r;
		if (!x->bk_equals(x))
		{
			TChunk *f = ptr2tchunk(x->fd);
			r         = ptr2tchunk(x->bk);
			if (RTCHECK(ok_address(f->ptr()) && f->bk_equals(x) &&
			            r->fd_equals(x)))
			{
				f->bk_assign(r);
				r->fd_assign(f);
			}
			else
			{
				corruption_error_action();
			}
		}
		else
		{
			CHERI::Capability<TChunk *> rp;
			if (((r = *(rp = &(x->child[1]))) != nullptr) ||
			    ((r = *(rp = &(x->child[0]))) != nullptr))
			{
				TChunk **cp;
				while ((*(cp = &(r->child[1])) != nullptr) ||
				       (*(cp = &(r->child[0])) != nullptr))
				{
					r = *(rp = cp);
				}
				if (RTCHECK(ok_address(rp.address())))
				{
					*rp = nullptr;
				}
				else
				{
					corruption_error_action();
				}
			}
		}
		if (!x->is_tree_ring())
		{
			TChunk **h = treebin_at(x->index);
			if (x == *h)
			{
				if ((*h = r) == nullptr)
				{
					treemap_clear(x->index);
				}
			}
			else if (RTCHECK(ok_address(xp->ptr())))
			{
				if (xp->child[0] == x)
				{
					xp->child[0] = r;
				}
				else
				{
					xp->child[1] = r;
				}
			}
			else
			{
				corruption_error_action();
			}
			if (r != nullptr)
			{
				if (RTCHECK(ok_address(r->ptr())))
				{
					TChunk *c0, *c1;
					r->parent = xp;
					if ((c0 = x->child[0]) != nullptr)
					{
						if (RTCHECK(ok_address(c0->ptr())))
						{
							r->child[0] = c0;
							c0->parent  = r;
						}
						else
						{
							corruption_error_action();
						}
					}
					if ((c1 = x->child[1]) != nullptr)
					{
						if (RTCHECK(ok_address(c1->ptr())))
						{
							r->child[1] = c1;
							c1->parent  = r;
						}
						else
						{
							corruption_error_action();
						}
					}
				}
				else
				{
					corruption_error_action();
				}
			}
		}
	}

	// Throw p into the correct bin based on s.
	void insert_chunk(MChunk *p, size_t s)
	{
		if (is_small(s))
		{
			insert_small_chunk(p, s);
		}
		else
		{
			insert_large_chunk(static_cast<TChunk *>(p), s);
		}
	}

	// Unlink p from the correct bin based on s.
	void unlink_chunk(MChunk *p, size_t s)
	{
		if (is_small(s))
		{
			unlink_small_chunk(p, s);
		}
		else
		{
			unlink_large_chunk(static_cast<TChunk *>(p));
		}
	}

	/**
	 * @brief Find the smallest chunk in t and allocate nb bytes from it.
	 *
	 * @return Should always succeed because we made sure this tree has chunks
	 * and all chunks in this tree are larger than nb.
	 */
	void *tmalloc_smallest(TChunk *t, size_t nb)
	{
		size_t  rsize = t->size_get() - nb;
		TChunk *v     = t;

		Debug::Assert(t != nullptr, "Chunk must not be null");
		while ((t = t->leftmost_child()) != nullptr)
		{
			size_t trem = t->size_get() - nb;
			if (trem < rsize)
			{
				rsize = trem;
				v     = t;
			}
		}

		if (RTCHECK(ok_address(v->ptr())))
		{
			MChunk *r = v->chunk_plus_offset(nb);
			Debug::Assert(v->size_get() == rsize + nb,
			              "Chunk {} size is {}, should be {}",
			              v,
			              v->size_get(),
			              rsize + nb);
			Debug::Assert(v->is_prev_in_use(),
			              "Free chunk {} follows another free chunk");
			if (RTCHECK(v->ok_next(r)))
			{
				unlink_large_chunk(v);
				if (rsize < MinChunkSize)
				{
					v->in_use_chunk_set(rsize + nb);
				}
				else
				{
					v->in_use_chunk_set(nb);
					/*
					 * The remainder is big enough to to used by another
					 * allocation, place it into the free list.
					 */
					r->free_chunk_set(rsize);
					insert_chunk(r, rsize);
				}
				return chunk2mem(v);
			}
		}

		corruption_error_action();
		return nullptr;
	}

	// Allocate a large request from the best fitting chunk in a treebin.
	void *tmalloc_large(size_t nb)
	{
		TChunk *v     = nullptr;
		size_t  rsize = -nb; // unsigned negation
		TChunk *t;
		BIndex  idx = compute_tree_index(nb);
		if ((t = *treebin_at(idx)) != nullptr)
		{
			// Traverse tree for this bin looking for node with size == nb.
			size_t  sizebits = nb << leftshift_for_tree_index(idx);
			TChunk *rst      = nullptr; // the deepest untaken right subtree
			for (;;)
			{
				TChunk *rt;
				size_t  trem = t->size_get() - nb;
				if (trem < rsize)
				{
					v = t;
					if ((rsize = trem) == 0)
					{
						break;
					}
				}
				rt = t->child[1];
				t  = t->child[leftshifted_val_msb(sizebits)];
				if (rt != nullptr && rt != t)
				{
					rst = rt;
				}
				if (t == nullptr)
				{
					t = rst; // Set t to least subtree holding sizes > nb.
					break;
				}
				sizebits <<= 1;
			}
		}
		if (t == nullptr && v == nullptr)
		{
			/*
			 * We didn't find anything usable in the tree, so use the next tree
			 * if there is one. Set t to root of next non-empty treebin.
			 */
			Binmap leftbits = ds::bits::above_least(idx2bit(idx)) & treemap;
			if (leftbits != 0)
			{
				BIndex i;
				Binmap leastbit = ds::bits::isolate_least(leftbits);
				i               = bit2idx(leastbit);
				t               = *treebin_at(i);
			}
			else
			{
				return nullptr;
			}
		}
		else if (v)
		{
			t = v;
		}

		return tmalloc_smallest(t, nb);
	}

	/**
	 * @brief Allocate a small request from a tree bin. It should return a valid
	 * chunk successfully as long as one tree exists, because all tree chunks
	 * are larger than a small request.
	 */
	void *tmalloc_small(size_t nb)
	{
		TChunk *t;
		size_t  rsize;
		BIndex  i;
		Binmap  leastbit = ds::bits::isolate_least(treemap);
		i                = bit2idx(leastbit);
		t                = *treebin_at(i);

		return tmalloc_smallest(t, nb);
	}

	/**
	 * Move a chunk back onto free lists.
	 *
	 * Note that this chunk does not have to come from quarantine, because it
	 * can come from initialisation or splitting a free chunk.
	 */
	void mspace_free_internal(MChunk *p)
	{
		ok_in_use_chunk(p);
		size_t psize = 0, psizeold = 0;
		if (RTCHECK(ok_address(p->ptr()) && p->ok_in_use()))
		{
			// Clear the shadow bits.
			revoker.shadow_paint_range(
			  chunk2mem(p).address(), p->chunk_next()->ptr(), false);
			psize           = p->size_get();
			psizeold        = psize;
			MChunk *current = p;
			MChunk *next    = p->chunk_next();
			if (!p->is_prev_in_use())
			{
				size_t  prevsize = p->prevsize_get();
				MChunk *prev     = p->chunk_prev();
				psize += prevsize;
				p = prev;
				if (RTCHECK(ok_address(prev->ptr())))
				{ // Consolidate backward.
					unlink_chunk(p, prevsize);
					metadata_zero<true>(current);
				}
				else
				{
					corruption_error_action();
				}
			}

			if (RTCHECK(p->ok_next(next) && next->ok_prev_in_use()))
			{
				if (!next->is_in_use()) // Consolidate forward.
				{
					size_t nsize = next->size_get();
					psize += nsize;
					unlink_chunk(next, nsize);
					metadata_zero<true>(next);
				}
				p->free_chunk_set(psize);

				insert_chunk(p, psize);
				ok_free_chunk(p);
			}
		}

		heapFreeSize += psizeold;
	}

	/**
	 * @brief Start revocation if this MState has accumulated enough things in
	 * quarantine or the free space is too low.
	 * @param Force force start a revocation regardless of heuristics
	 *
	 * @return true if there are things in the quarantine
	 */
	template<bool Force>
	bool mspace_bg_revoker_kick()
	{
		if (heapQuarantineSize == 0)
		{
			return 0;
		}
		/*
		 * Async revocation can run in the background, but sync revocation
		 * blocks and we don't want it to sweep too early, so we have different
		 * quarantine thresholds here.
		 */
		bool shouldKick;
		if constexpr (Revocation::Revoker::IsAsynchronous)
		{
			shouldKick = heapQuarantineSize > heapFreeSize / 4 ||
			             heapFreeSize < heapTotalSize / 8;
		}
		else
		{
			shouldKick = heapQuarantineSize > heapFreeSize / 4 * 3;
		}
		if (Force || shouldKick)
		{
			revoker.system_bg_revoker_kick();
		}

		return 1;
	}

	/**
	 * @brief Try to dequeue the quarantine list multiple times.
	 *
	 * @param loops how many times do we try to dequeue
	 */
	int mspace_qtbin_deqn(size_t loops)
	{
		int     dequeued = 0;
		MChunk *head     = qtbin();
		for (size_t i = 0; i < loops; i++)
		{
			MChunk *fore = ptr2chunk(head->fd);
			MChunk *next;

			/*
			 * We organise the quarantine list in a way that places young chunks
			 * at the tail. If we see a chunk that is too young to be dequeued
			 * when starting from the front, then we know no remaining chunks
			 * can be dequeued because they can only be younger.
			 */
			if (fore == head || !fore->is_revocation_done())
			{
				break;
			}
			next = ptr2chunk(fore->fd);
			head->fd_assign(next);
			next->bk_assign(head);
			heapQuarantineSize -= fore->size_get();
			mspace_free_internal(fore);
			dequeued++;
		}
		return dequeued;
	}

	/**
	 * Successful end to mspace_malloc()
	 */
	void *mspace_malloc_success(void *mem, size_t nb)
	{
		ok_malloced_chunk(mem, nb);

		// If we reached here, then it means we took a real chunk off the free
		// list without errors. Zero the user portion metadata.
		MChunk *finalChunk = mem2chunk(mem);
		size_t  finalSz    = finalChunk->size_get();
		metadata_zero<false>(finalChunk);
		// We sanity check that things off the free list are indeed zeroed out.
		Debug::Assert(capaligned_range_do(mem,
		                                  finalSz - ChunkOverhead,
		                                  [](void *&word) {
			                                  return CHERI::Capability<void>(
			                                           word) != nullptr;
		                                  }) == false,
		              "Memory from free list is not entirely zeroed, size {}",
		              finalSz);
		heapFreeSize -= finalSz;
		return mem;
	}

	/**
	 * This is the only function that takes memory from the free list. All other
	 * wrappers that take memory must call this in the end.
	 */
	void *mspace_malloc(size_t bytes)
	{
		void  *mem;
		size_t nb;

		if (bytes <= MaxSmallRequest)
		{
			BIndex idx;
			Binmap smallbits;
			nb  = (bytes < MinRequest) ? MinChunkSize : pad_request(bytes);
			idx = small_index(nb);
			smallbits = smallmap >> idx;

			if (smallbits & 0x1U)
			{ // exact match
				auto p = unlink_first_small_chunk(idx);
				p->in_use_set();
				mem = chunk2mem(p);

				return mspace_malloc_success(mem, nb);
			}

			if (smallbits != 0)
			{ // Use chunk in next nonempty smallbin.
				Binmap leftbits =
				  (smallbits << idx) & ds::bits::above_least(idx2bit(idx));
				Binmap leastbit = ds::bits::isolate_least(leftbits);
				BIndex i        = bit2idx(leastbit);
				auto   p        = unlink_first_small_chunk(i);
				size_t rsize    = small_index2size(i) - nb;

				if (rsize < MinChunkSize)
				{
					p->in_use_set();
				}
				else
				{
					p->in_use_chunk_set(nb);
					auto r = p->chunk_plus_offset(nb);
					r->free_chunk_set(rsize);
					insert_small_chunk(r, rsize);
				}
				mem = chunk2mem(p);

				return mspace_malloc_success(mem, nb);
			}

			if (treemap != 0 && (mem = tmalloc_small(nb)) != nullptr)
			{
				return mspace_malloc_success(mem, nb);
			}
		}
		else
		{
			nb = pad_request(bytes);
			if (treemap != 0 && (mem = tmalloc_large(nb)) != nullptr)
			{
				return mspace_malloc_success(mem, nb);
			}
		}

		/*
		 * Exhausted all allocation options. Force start a revocation or
		 * continue with synchronous revocation. If kick returns 0, then there's
		 * nothing in the quarantine and we run out of memory for real.
		 *
		 * If we do have leftovers in the quarantine, dequeue the entire
		 * quarantine and punt to the caller to retry.
		 */
		if (mspace_bg_revoker_kick<true>())
		{
			mspace_qtbin_deqn(UINT32_MAX);
		}
		return nullptr;
	}

	// Allocate memory with specific alignment.
	void *mspace_memalign(size_t bytes, size_t alignment)
	{
		size_t                  nb;      // padded request size
		CHERI::Capability<void> m;       // memory returned by malloc call
		MChunk                 *p;       // corresponding chunk
		char                   *brk;     // alignment point within p
		MChunk                 *newp;    // chunk to return
		size_t                  newsize; // its size
		size_t  leadsize;                // leading space before alignment point
		MChunk *remainder;               // spare room at end to split off
		size_t  remainderSize;           // its size
		size_t  size;

		nb = pad_request(bytes);
		// Make sure alignment is power of 2 (in case MINSIZE is not).
		Debug::Assert((alignment & (alignment - 1)) == 0,
		              "Alignment {} is not a power of two",
		              alignment);

		// fast path for not-too-big objects
		if (alignment <= MallocAlignment)
		{
			return mspace_malloc(bytes);
		}

		/*
		 * Strategy: find a spot within that chunk that meets the alignment
		 * request, and then possibly free the leading and trailing space.
		 * Call malloc with worst case padding to hit alignment.
		 */
		m = mspace_malloc(nb + alignment + MinChunkSize);
		if (m == nullptr)
		{
			return m;
		}

		p = mem2chunk(m);
		if ((m.address() % alignment) != 0) // misaligned
		{
			/*
			 * Find an aligned spot inside chunk. Since we need to give back
			 * leading space in a chunk of at least MINSIZE, if the first
			 * calculation places us at a spot with less than MINSIZE
			 * leader, we can move to the next aligned spot -- we've
			 * allocated enough total room so that this is always possible.
			 */
			auto alignedAddress{m};
			alignedAddress.address() =
			  (m.address() + alignment - 1) & -static_cast<ssize_t>(alignment);
			brk = reinterpret_cast<char *>(mem2chunk(alignedAddress));
			if (ds::pointer::diff(p, brk) < MinChunkSize)
			{
				brk += alignment;
			}

			newp     = reinterpret_cast<MChunk *>(brk);
			leadsize = ds::pointer::diff(p, brk);
			newsize  = p->size_get() - leadsize;

			// Give back leader, use the rest.
			newp->in_use_chunk_set(newsize);
			p->in_use_chunk_set(leadsize);
			/*
			 * This pointer is entirely internal. No need to go through
			 * quarantine. Same for another call below.
			 */
			mspace_free_internal(p);
			p = newp;

			Debug::Assert(newsize >= nb &&
			                (chunk2mem(p).address() % alignment) == 0,
			              "Chunk {} does not meet the size ({}) or alignment "
			              "({}) requirements",
			              p,
			              nb,
			              alignment);
		}

		// Also give back spare room at the end.
		size = p->size_get();
		if (size > nb + MinChunkSize)
		{
			remainderSize = size - nb;
			remainder     = p->chunk_plus_offset(nb);
			remainder->in_use_chunk_set(remainderSize);
			p->in_use_chunk_set(nb);
			mspace_free_internal(remainder);
		}

		ok_in_use_chunk(p);
		return chunk2mem(p);
	}

	void corruption_error_action()
	{
		ABORT();
	}
};
