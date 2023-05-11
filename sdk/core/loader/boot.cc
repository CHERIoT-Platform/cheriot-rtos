// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cdefs.h>
// memcpy is exposed as a libcall in the standard library headers but we want
// to ensure that our version is called directly and not exposed to anything
// else.
#undef __cheri_libcall
#define __cheri_libcall
#include <string.h>

#include "../scheduler/common.h"
#include "../switcher/tstack.h"
#include "constants.h"
#include "debug.h"
#include "defines.h"
#include "types.h"
#include <cheri.hh>
#include <priv/riscv.h>
#include <riscvreg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

using namespace CHERI;

namespace
{
	struct ThreadConfig
	{
		uint16_t threadid;
		uint16_t priority;
		size_t   stackSize;
		size_t   trustedStackFrames;
	};
	constexpr ThreadConfig ThreadConfigs[] = CONFIG_THREADS;

	/**
	 * Round up to a multiple of `Multiple`, which must be a power of two.
	 */
	template<size_t Multiple>
	constexpr size_t round_up(size_t value)
	{
		static_assert((Multiple & (Multiple - 1)) == 0,
		              "Multiple must be a power of two");
		return (value + Multiple - 1) & -Multiple;
	}
	static_assert(round_up<16>(15) == 16);
	static_assert(round_up<16>(28) == 32);
	static_assert(round_up<8>(17) == 24);

	template<size_t N>
	constexpr size_t total_stacksize(const ThreadConfig (&configs)[N])
	{
		size_t ret = 0;
		for (const auto &config : configs)
		{
			ret += round_up<16>(config.stackSize);
			ret +=
			  round_up<16>(sizeof(TrustedStack) + sizeof(TrustedStackFrame) *
			                                        config.trustedStackFrames);
		}
		return ret;
	}

	__BEGIN_DECLS
	char __section(".thread_stacks") bootStack[BOOT_STACK_SIZE];
	char __section(".thread_stacks") bootTStack[BOOT_TSTACK_SIZE];
	static_assert(
	  CheckSize<BOOT_TSTACK_SIZE, sizeof(TrustedStackGeneric<0>)>::Value,
	  "Boot trusted stack sizes do not match.");
	// Stacks must be 16-byte aligned, so ensure that this is 16-byte aligned.
	alignas(16) char __section(".thread_stacks")
	  stackSpace[total_stacksize(ThreadConfigs)];
	// It must also be aligned sufficiently for trusted stacks, so ensure that
	// we've captured that requirement above.
	static_assert(alignof(TrustedStack) <= 16);
	__END_DECLS

	static_assert(
	  CheckSize<sizeof(sched::ThreadLoaderInfo), BOOT_THREADINFO_SZ>::Value);

	/// The return type of the loader, to pass information to the scheduler.
	struct SchedulerEntryInfo
	{
		/// scheduler initial entry point for thread initialisation
		void *schedPCC;
		/// scheduler CGP for thread initialisation
		void *schedCGP;
		/// thread descriptors for all threads
		sched::ThreadLoaderInfo threads[CONFIG_THREADS_NUM];
	};

	/**
	 * Reserved sealing types.
	 */
	enum SealingType
	{
		/**
		 * 0 represents unsealed.
		 */
		Unsealed = 0,

		/**
		 * Sentry that inherits interrupt status.
		 */
		SentryInheriting,

		/// Alternative name: the default sentry type.
		Sentry = SentryInheriting,

		/**
		 * Sentry that disables interrupts on calls.
		 */
		SentryDisabling,

		/**
		 * Sentry that enables interrupts on calls.
		 */
		SentryEnabling,

		/**
		 * Marker for the first sealing type that's valid for data capabilities.
		 */
		FirstDataSealingType = 9,

		/**
		 * The sealing type used for sealed export table entries.
		 */
		SealedImportTableEntries = FirstDataSealingType,

		/**
		 * The compartment switcher has a sealing type for the trusted stack.
		 *
		 * This must be the second data sealing type so that we can also permit
		 * the switcher to unseal sentries and export table entries.
		 */
		SealedTrustedStacks,

		/**
		 * The scheduler has a sealing type for waitable objects.
		 */
		Scheduler,

		/**
		 * The allocator has a sealing type for the software sealing mechanism.
		 */
		Allocator,

		/**
		 * The first sealing key that is reserved for use by the allocator's
		 * software sealing mechanism and used for static sealing types,
		 */
		FirstStaticSoftware = 16,

		/**
		 * The first sealing key in the space that the allocator will
		 * dynamically allocate for sealing types.
		 */
		FirstDynamicSoftware = 0x1000000,
	};

	// We currently have a 3-bit hardware otype, with different sealing spaces
	// for code and data capabilities, giving the range 0-0xf reserved for
	// hardware use. Assert that we're not using more than we need (two in the
	// enum are outside of the hardware space).
	static_assert(magic_enum::enum_count<SealingType>() <= 10,
	              "Too many sealing types reserved for a 3-bit otype field");

	constexpr auto StoreLPerm = Root::Permissions<Root::Type::RWStoreL>;
	/// PCC permissions for the switcher.
	constexpr auto SwitcherPccPermissions =
	  Root::Permissions<Root::Type::Execute>;
	/// PCC permissions for unprivileged compartments
	constexpr auto UserPccPermissions =
	  Root::Permissions<Root::Type::Execute>.without(
	    Permission::AccessSystemRegisters);

	template<typename T, typename U>
	T align_up(T x, U align)
	{
		return __builtin_align_up(x, align);
	}

	/**
	 * Returns a capability of type T* derived from the root specified by Type,
	 * with the specified permissions.  The start and length are given as
	 * arguments.
	 */
	template<typename T                = void,
	         Root::Type    Type        = Root::Type::RWGlobal,
	         PermissionSet Permissions = Root::Permissions<Type>,
	         bool          Precise     = true>
	Capability<T> build(ptraddr_t start, size_t length)
	{
		return static_cast<T *>(
		  Root::build_from_root<Type, Permissions, Precise>(start, length));
	}

	/**
	 * Builds a capability with bounds to access a single object of type `T`,
	 * which starts at address `start`.  The root and permissions are specified
	 * as template arguments.
	 */
	template<typename T,
	         Root::Type    Type        = Root::Type::RWGlobal,
	         PermissionSet Permissions = Root::Permissions<Type>>
	Capability<T> build(ptraddr_t start)
	{
		return build<T, Type, Permissions>(start, sizeof(T));
	}

	/**
	 * Builds a capability with bounds specified by `start` and `length`, which
	 * points to an object of type `T`, at address `address`.  This is derived
	 * from the root and with the permissions given as template arguments.
	 */
	template<typename T                = void,
	         Root::Type    Type        = Root::Type::RWGlobal,
	         PermissionSet Permissions = Root::Permissions<Type>>
	Capability<T> build(ptraddr_t start, size_t length, ptraddr_t address)
	{
		Capability<T> ret{static_cast<T *>(
		  Root::build_from_root<Type, Permissions>(start, length))};
		ret.address() = address;
		return ret;
	}

	/**
	 * Build a capability covering a range specified by a range (
	 */
	template<typename T                = void,
	         Root::Type    Type        = Root::Type::RWGlobal,
	         PermissionSet Permissions = Root::Permissions<Type>,
	         bool          Precise     = true>
	Capability<T> build(auto &&range) requires(RawAddressRange<decltype(range)>)
	{
		return build<T, Type, Permissions, Precise>(range.start(),
		                                            range.size());
	}

	/**
	 * Build a capability to an object of type `T` from a range (start and size
	 * address).
	 */
	template<typename T                = void,
	         Root::Type    Type        = Root::Type::RWGlobal,
	         PermissionSet Permissions = Root::Permissions<Type>>
	Capability<T>
	build(auto    &&range,
	      ptraddr_t address) requires(RawAddressRange<decltype(range)>)
	{
		return build<T, Type, Permissions>(
		  range.start(), range.size(), address);
	}

	/**
	 * Build the PCC for a compartment.  Permissions can be overridden for more
	 * / less privilege compartments.
	 */
	template<const PermissionSet Permissions = UserPccPermissions>
	Capability<void> build_pcc(const auto &compartment)
	{
		return build<void, Root::Type::Execute, Permissions>(compartment.code);
	}

	/**
	 * Build a capability to a compartment's globals.  Permissions can be
	 * overridden via a template parameter for non-default options.
	 *
	 * By default, this function returns a biased $cgp value: the address
	 * points to the middle of the range.  This can be disabled by passing
	 * `false` as the second template parameter.
	 */
	Capability<void> build_cgp(const auto &compartment, bool bias = true)
	{
		auto cgp = build<void, Root::Type::RWGlobal>(compartment.data);
		if (bias)
		{
			cgp.address() += (compartment.data.size() / 2);
		}
		return cgp;
	}

	/**
	 * Returns a sealing capability to use for statically allocated sealing
	 * keys.
	 */
	uint16_t allocate_static_sealing_key()
	{
		static uint16_t nextValue = FirstStaticSoftware;
		// We currently stash the allocated key value in the export table.  We
		// could expand this a bit if we were a bit more clever in how we used
		// that space, but 2^16 static sealing keys will require over 768 KiB
		// of SRAM to store in the firmware, which seems excessive.
		Debug::Invariant(nextValue < std::numeric_limits<uint16_t>::max(),
		                 "Out of static sealing keys");
		return nextValue++;
	}

	/**
	 * Returns a sealing capability in the sealing space with the specified
	 * type.
	 */
	void *build_static_sealing_key(uint16_t type)
	{
		static void *staticSealingRoot;
		Debug::Invariant(type >= FirstStaticSoftware,
		                 "{} is not a valid software sealing key",
		                 type);
		if (staticSealingRoot == nullptr)
		{
			staticSealingRoot =
			  build<void,
			        Root::Type::Seal,
			        PermissionSet{Permission::Global,
			                      Permission::Seal,
			                      Permission::Unseal,
			                      Permission::User0}>(0, FirstDynamicSoftware);
		}
		Capability next = staticSealingRoot;
		next.address()  = type;
		next.bounds()   = 1;
		Debug::Invariant(
		  next.is_valid(), "Invalid static sealing key {}", next);
		return next;
	}

	template<typename T>
	T *seal_entry(Capability<T> ptr, InterruptStatus status)
	{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc99-designator"
		constexpr SealingType Sentries[] = {
		  [int(InterruptStatus::Enabled)]   = SentryEnabling,
		  [int(InterruptStatus::Disabled)]  = SentryDisabling,
		  [int(InterruptStatus::Inherited)] = SentryInheriting};
#pragma clang diagnostic pop
		Debug::Invariant(
		  unsigned(status) < 3, "Invalid interrupt status {}", int(status));
		size_t otype = size_t{Sentries[int(status)]};
		void  *key   = build<void, Root::Type::Seal>(otype, 1);
		return ptr.seal(key);
	}

	/**
	 * Helper to determine whether an object, given by a start address and size,
	 * is completely contained within a specified range.
	 */
	bool contains(const auto &range,
	              ptraddr_t   addr,
	              size_t      size) requires(RawAddressRange<decltype(range)>)
	{
		return (range.start() <= addr) &&
		       (range.start() + range.size() >= addr + size);
	}

	/**
	 * Helper to determine whether an address is within a range.  The template
	 * parameter specifies the type that the object is expected to be.  The
	 * object must be completely contained within the range.
	 */
	template<typename T = char>
	bool contains(const auto &range,
	              ptraddr_t   addr) requires(RawAddressRange<decltype(range)>)
	{
		return contains(range, addr, sizeof(T));
	}

	/**
	 * Helper class representing a range (which can be used with range-based
	 * for loops) built from pointers to contiguous memory.
	 */
	template<typename T>
	class ContiguousPtrRange
	{
		/**
		 * Pointer to the first element.
		 */
		T *start;

		/**
		 * Pointer to one past the last element.
		 */
		T *finish;

		public:
		/**
		 * Constructor, takes pointers to the beginning and end of an array.
		 */
		ContiguousPtrRange(T *s, T *e) : start(s), finish(e) {}

		/**
		 * Returns a pointer to the start.
		 */
		T *begin()
		{
			return start;
		}

		/**
		 * Returns a pointer to the end.
		 */
		T *end()
		{
			return finish;
		}
	};

	/**
	 * Build an range for use with range-based for loops iterating over objects
	 * of type `T` from a virtual address range.
	 */
	template<typename T, bool Precise = true>
	ContiguousPtrRange<T>
	build_range(const auto &range) requires(RawAddressRange<decltype(range)>)
	{
		Capability<T> start = build<T,
		                            Root::Type::RWGlobal,
		                            Root::Permissions<Root::Type::RWGlobal>,
		                            Precise>(range);
		Capability<T> end   = start;
		end.address() += range.size();
		return {start, end};
	}

	/**
	 * The sealing key for the switcher, used to seal all jump targets in the
	 * import tables.
	 */
	Capability<void> switcherKey;

	/**
	 * The sealing key for sealing trusted stacks.
	 */
	Capability<void> trustedStackKey;

	/**
	 * Find an export table target.  This looks for the `target` address within
	 * all of the export tables in the image.  The `size` parameter is used if
	 * this is an MMIO import, the `lib` parameter is used to derive
	 * capabilities for the library compartment.
	 */
	void *find_export_target(const ImgHdr &image,
	                         const auto   &sourceCompartment,
	                         ptraddr_t     target,
	                         size_t        size)
	{
		// Build an MMIO capability.
		auto buildMMIO = [&]() {
			Debug::log("Building mmio capability {} + {}", target, size);
			if constexpr (std::is_same_v<
			                std::remove_cvref_t<decltype(sourceCompartment)>,
			                ImgHdr::PrivilegedCompartment>)
			{
				if (&sourceCompartment == &image.allocator())
				{
					if (target == LA_ABS(__export_mem_heap) &&
					    (size == (LA_ABS(__export_mem_heap_end) -
					              LA_ABS(__export_mem_heap))))
					{
						Debug::log(
						  "Assigning the heap ({}--{}) to the allocator",
						  target,
						  target + size);
						return build(target, size);
					}
				}
			}
			Debug::Invariant(target >= LA_ABS(__mmio_region_start),
			                 "{} is not in the MMIO range",
			                 target);
			Debug::Invariant(target + size <= LA_ABS(__mmio_region_end),
			                 "{} is not in the MMIO range",
			                 target + size);
			return build(target, size);
		};

		// Build an export table entry for the given compartment.
		auto buildExportEntry = [&](const auto &compartment) {
			auto exportEntry = build(compartment.exportTable, target)
			                     .template cast<ExportEntry>();
			auto interruptStatus = exportEntry->interrupt_status();
			Debug::Invariant((interruptStatus == InterruptStatus::Enabled) ||
			                   (interruptStatus == InterruptStatus::Disabled),
			                 "Functions exported from compartments must have "
			                 "an explicit interrupt posture");
			return build(compartment.exportTable, target).seal(switcherKey);
		};

		// If the low bit is 1, then this is either an MMIO region or direct
		// call via a sentry.  The latter case covers code in shared
		// (stateless) libraries and explicit interrupt-toggling sentries for
		// code within the current compartment.
		// First check if it's a sentry call.
		if (target & 1)
		{
			// Clear the low bit to give the real address.
			auto possibleLibcall = target & ~1U;
			// Helper to create the target of a library call.
			auto createLibCall = [&](Capability<void> pcc) {
				// Libcall export table entries are just sentry capabilities to
				// the real target.  We need to get the address and interrupt
				// status of the function from the target's export table and
				// then construct a sentry of the correct kind derived from the
				// compartment's PCC.
				auto ent = build<ExportEntry>(possibleLibcall);
				pcc.address() += ent->functionStart;
				return seal_entry(pcc, ent->interrupt_status());
			};
			// If this is a libcall, set it up
			for (auto &lib : image.libraries())
			{
				if (contains<ExportEntry>(lib.exportTable, possibleLibcall))
				{
					// TODO: Library export tables are not used after the
					// loader has run, we could move them to the end of the
					// image and make that space available for the heap.
					return createLibCall(build_pcc(lib));
				}
			}
			// We also use the library calling convention for local callbacks,
			// so see if this points to our own export table.
			if (contains<ExportEntry>(sourceCompartment.exportTable,
			                          possibleLibcall))
			{
				return createLibCall(build_pcc(sourceCompartment));
			}
			// Otherwise this is an MMIO space entry (we allow byte-granularity
			// delegation of MMIO objects, so a low bit of 1 might be a
			// coincidence).
			return buildMMIO();
		}

		// Privileged compartments don't have sealed objects.
		if constexpr (!std::is_same_v<
		                std::remove_cvref_t<decltype(sourceCompartment)>,
		                ImgHdr::PrivilegedCompartment>)
		{
			if (contains(sourceCompartment.sealedObjects, target, size))
			{
				auto sealingType =
				  build<uint32_t,
				        Root::Type::RWGlobal,
				        PermissionSet{Permission::Load, Permission::Store}>(
				    target);
				// TODO: This currently places a restriction that data memory
				// can't be in the low 64 KiB of the address space.  That may be
				// too restrictive. If we haven't visited this sealed object
				// yet, then we should update its first word to point to the
				// sealing type.
				if (*sealingType >
				    std::numeric_limits<
				      decltype(ExportEntry::functionStart)>::max())
				{
					auto typeAddress = *sealingType;
					auto findExport  = [&](auto &compartment) {
                        if (contains<ExportEntry>(compartment.exportTable,
                                                  typeAddress))
                        {
                            auto exportEntry = build<ExportEntry>(
                              compartment.exportTable, typeAddress);
                            Debug::Invariant(
							   exportEntry->is_sealing_type(),
							   "Sealed object points to invalid sealing type");
                            *sealingType = exportEntry->functionStart;
                            return true;
                        }
                        return false;
					};
					bool found = false;
					for (auto &compartment : image.privilegedCompartments)
					{
						if (found)
						{
							break;
						}
						found = findExport(compartment);
					}
					for (auto &compartment : image.compartments())
					{
						if (found)
						{
							break;
						}
						found = findExport(compartment);
					}
					Debug::Invariant(*sealingType != typeAddress,
					                 "Invalid sealed object {}",
					                 typeAddress);
				}
				Capability sealedObject = build(target, size);
				// Seal with the allocator's sealing key
				sealedObject.seal(build<void, Root::Type::Seal>(Allocator, 1));
				Debug::log("Static sealed object: {}", sealedObject);
				return sealedObject;
			}
		}

		for (auto &compartment : image.privilegedCompartments)
		{
			if (contains<ExportEntry>(compartment.exportTable, target))
			{
				return buildExportEntry(compartment);
			}
		}

		for (auto &compartment : image.compartments())
		{
			if (contains<ExportEntry>(compartment.exportTable, target))
			{
				return buildExportEntry(compartment);
			}
		}

		return buildMMIO();
	}

	/**
	 * As a first pass, scan the import table of this compartment and resolve
	 * any static sealing types.
	 */
	void populate_static_sealing_keys(const ImgHdr &image,
	                                  const auto   &compartment)
	{
		if (compartment.exportTable.size() == 0)
		{
			return;
		}
		const auto &importTable = compartment.import_table();
		if (importTable.size() == 0)
		{
			return;
		}
		// The import table might not have strongly aligned bounds and so we
		// are happy with an imprecise capability here.
		auto impPtr = build<ImportTable,
		                    Root::Type::RWGlobal,
		                    Root::Permissions<Root::Type::RWGlobal>,
		                    false>(importTable);
		// FIXME: This should use a range-based for loop
		for (int i = 0; i < (importTable.size() / sizeof(void *)) - 1; i++)
		{
			ptraddr_t importAddr = impPtr->imports[i].address;
			size_t    importSize = impPtr->imports[i].size;
			// If the size is not 0, this isn't an import table entry.
			if (importSize != 0)
			{
				continue;
			}
			// If the low bit is 1, it's either a library import or an MMIO
			// import.  Skip it either way.
			if (importAddr & 1)
			{
				continue;
			}
			// If this points anywhere other than the current compartment's
			// export table, it isn't a sealing capability entry.
			if (!contains(compartment.exportTable, importAddr))
			{
				continue;
			}
			// Build an export table entry for the given compartment.
			auto exportEntry =
			  build<ExportEntry>(compartment.exportTable, importAddr);

			// If the export entry isn't a sealing type, this is not a
			// reference to a sealing capability.
			if (!exportEntry->is_sealing_type())
			{
				continue;
			}
			Debug::Invariant(exportEntry->functionStart == 0,
			                 "Two import entries point to the same export "
			                 "entry for a sealing key {}",
			                 exportEntry);
			// Allocate a new sealing key type.
			exportEntry->functionStart = allocate_static_sealing_key();
			Debug::log("Creating sealing key {}", exportEntry->functionStart);
			// Build the sealing key corresponding to that type.
			impPtr->imports[i].pointer =
			  build_static_sealing_key(exportEntry->functionStart);
		}
	}

	/**
	 * Populate an import table.  The import table is described by the
	 * `importTable` argument.  The compartment switcher and the library
	 * compartment are passed as arguments.
	 */
	void populate_imports(const ImgHdr &image,
	                      const auto   &sourceCompartment,
	                      void         *switcher)
	{
		const auto &importTable = sourceCompartment.import_table();
		if (importTable.size() == 0)
		{
			return;
		}
		Debug::log("Import table: {}, {} bytes",
		           importTable.start(),
		           importTable.size());
		// The import table might not have strongly aligned bounds and so we
		// are happy with an imprecise capability here.
		auto impPtr = build<ImportTable,
		                    Root::Type::RWStoreL,
		                    Root::Permissions<Root::Type::RWStoreL>,
		                    false>(importTable);

		impPtr->switcher = switcher;
		// FIXME: This should use a range-based for loop
		for (int i = 0; i < (importTable.size() / sizeof(void *)) - 1; i++)
		{
			// If this is a sealing key then we will have initialised it
			// already, skip it now.
			if (Capability{impPtr->imports[i].pointer}.is_valid())
			{
				Debug::log("Skipping sealing type import");
				continue;
			}
			ptraddr_t importAddr = impPtr->imports[i].address;
			size_t    importSize = impPtr->imports[i].size;

			impPtr->imports[i].pointer = find_export_target(
			  image, sourceCompartment, importAddr, importSize);
		}
	}

	/**
	 * Construct the boot threads.
	 */
	void boot_threads_create(const ImgHdr            &image,
	                         sched::ThreadLoaderInfo *threadInfo)
	{
		/*
		 * The entire pool for stacks and trusted stacks may not be precisely
		 * represented, but each allocation will be.
		 */
		auto stackArea = build<void,
		                       Root::Type::TrustedStack,
		                       Root::Permissions<Root::Type::TrustedStack>,
		                       false>(LA_ABS(stackSpace), sizeof(stackSpace));
		/// Allocate some space from the pool.
		auto allocate = [&](size_t size) {
			Debug::log("Rounded up {} to {}", size, round_up<16>(size));
			size         = round_up<16>(size);
			auto ret     = stackArea;
			ret.bounds() = size;
			stackArea.address() += size;
			return ret;
		};
		/// Allocate a normal stack from the pool.
		auto allocateStack = [&](size_t size) {
			auto ret = allocate(size);
			// Trusted stack root has both global and store local,
			ret.permissions() &= ret.permissions().without(Permission::Global);
			// Move the pointer to the top for stack usage.
			Debug::Invariant((ret.address() & 0xf) == 0,
			                 "Stack is not 16-byte aligned: {}",
			                 ret);
			ret.address() += ret.length();
			Debug::Invariant((ret.address() & 0xf) == 0,
			                 "Stack is not 16-byte aligned: {}",
			                 ret);
			return ret;
		};
		/// Allocate a trusted stack from the pool.
		auto allocateTStack = [&](size_t frames) {
			// Calculate the size in bytes from number of trusted stack frames.
			size_t size =
			  sizeof(TrustedStack) + sizeof(TrustedStackFrame) * frames;
			return allocate(size).cast<TrustedStack>();
		};

		/*
		 * XXX: Ideally entries and threadConfigs should be one constexpr array,
		 * but I cannot figure out how to convert entry point symbol addresses
		 * into constexpr.
		 */
		const ptraddr_t Entrypoints[] = CONFIG_THREADS_ENTRYPOINTS;
		static_assert(utils::array_size(Entrypoints) ==
		              utils::array_size(ThreadConfigs));

		for (size_t i = 0; const auto &config : ThreadConfigs)
		{
			auto findCompartment = [&]() -> auto &
			{
				for (auto &compartment : image.compartments())
				{
					Debug::log("Looking in export table {}+{}",
					           compartment.exportTable.start(),
					           compartment.exportTable.size());
					if (contains(compartment.exportTable, Entrypoints[i]))
					{
						return compartment;
					}
				}
				Debug::Invariant(
				  false, "Compartment entry point is not a valid export");
				__builtin_unreachable();
			};
			const auto &compartment = findCompartment();
			Debug::log("Creating thread in compartment {}", &compartment);
			auto pcc = build_pcc(compartment);
			pcc.address() += build<ExportEntry>(Entrypoints[i])->functionStart;
			Debug::log("New thread's pcc will be {}", pcc);
			void *cgp = build_cgp(compartment);
			Debug::log("New thread's cgp will be {}", cgp);

			auto threadTStack = allocateTStack(config.trustedStackFrames);
			Debug::log("New thread's trusted stack is {}", threadTStack);
			threadTStack->mepcc = pcc;
			threadTStack->cgp   = cgp;
			auto stack          = allocateStack(config.stackSize);
			threadTStack->csp   = stack;
			Debug::log("New thread's stack is {}", stack);
			// Enable previous level interrupts and set the previous exception
			// level to M mode.
			threadTStack->mstatus =
			  (priv::MSTATUS_MPIE |
			   (priv::MSTATUS_PRV_M << priv::MSTATUS_MPP_SHIFT));
#ifdef CONFIG_MSHWM
			threadTStack->mshwm  = stack.top();
			threadTStack->mshwmb = stack.base();
#endif
			threadTStack->frameoffset = offsetof(TrustedStack, frames[1]);
			threadTStack->frames[0].calleeExportTable =
			  build(compartment.exportTable);

			threadTStack.seal(trustedStackKey);

			threadInfo[i].trustedStack = threadTStack;
			threadInfo[i].threadid     = config.threadid;
			threadInfo[i].priority     = config.priority;
			i++;
		}
		Debug::log("Finished creating threads");
		Debug::Invariant(stackArea.address() ==
		                   LA_ABS(stackSpace) + sizeof(stackSpace),
		                 "Stack area {} is not the end of stack space: {}",
		                 stackArea.address(),
		                 LA_ABS(stackSpace) + sizeof(stackSpace));
	}

	/**
	 * Resolve capability relocations.
	 *
	 * Note that this assumes that the firmware image was checked to ensure
	 * that no compartment ships with cap relocs that point to another
	 * compartment.  This should be impossible due to how they flow through the
	 * linker but needs to be part of a static auditing pipeline.
	 */
	void populate_caprelocs(const ImgHdr &image)
	{
		// Helper to give the cap relocs section as a range.
		struct
		{
			[[nodiscard]] ptraddr_t start() const
			{
				return LA_ABS(__cap_relocs);
			}
			[[nodiscard]] size_t size() const
			{
				return LA_ABS(__cap_relocs_end) - LA_ABS(__cap_relocs);
			}
		} capRelocsSection;

		// Find the library compartment that contains an address in its code or
		// data section.
		auto findCompartment = [&](ptraddr_t address) -> auto &
		{
			Debug::log("Capreloc address is {}", address);
			for (auto &compartment : image.libraries_and_compartments())
			{
				if (contains(compartment.code, address) ||
				    contains(compartment.data, address))
				{
					return compartment;
				}
			}
			Debug::Invariant(false, "Cannot find compartment for cap reloc");
			__builtin_unreachable();
		};

		Debug::log("Populating cap relocs the insecure way {} + {}",
		           capRelocsSection.start(),
		           capRelocsSection.size());
		for (auto &reloc : build_range<CapReloc, false>(capRelocsSection))
		{
			// Find the compartment that this relocation applies to.
			const auto &compartment = findCompartment(reloc.addr);

			// Compartment's PCC, used to derive function pointers.
			auto pcc = build_pcc(compartment);
			// Compartment's PCC with execute dropped.  Used for pointers to
			// read-only globals.
			auto ropcc = pcc;
			ropcc.permissions() &=
			  pcc.permissions().without(Permission::Execute);
			// Compartment's globals region, used to derive pointers to globals
			// and to write to globals.
			auto cgp = build_cgp(compartment, false);

			Capability<void> locationRegion;
			// Cap relocs for a compartment must point to that compartment's
			// code or data regions.
			if (contains(compartment.code, reloc.addr))
			{
				locationRegion =
				  build<void, Root::Type::RWGlobal>(compartment.code);
			}
			else if (contains(compartment.data, reloc.addr))
			{
				locationRegion = cgp;
			}
			// The location is the address of the reloc, bounded to the region.
			Capability<void *> location{locationRegion.cast<void *>()};
			location.address()      = reloc.addr;
			size_t           offset = reloc.offset;
			Capability<void> cap;

			if (reloc.is_function())
			{
				// If this is a function pointer, use the bounds of that
				// compartment's PCC.
				// FIXME: In our ABI the linker should emit function pointer
				// bounds to be the whole .pcc section, not a single function.
				offset += reloc.base - compartment.code.start();
				cap = pcc;
			}
			else
			{
				if (contains(compartment.code, reloc.base))
				{
					cap = ropcc;
				}
				else if (contains(compartment.data, reloc.base))
				{
					cap = cgp;
				}
				else
				{
					Debug::Invariant(
					  false,
					  "Cap reloc points to something not owned by "
					  "the compartment.");
				}
				// Pointers to globals and read-only data should be bounded as
				// requested. Here, we also use setbounds to check reloc.len,
				// either untagged or an exception.
				cap.address() = reloc.base;
				cap.bounds().set_inexact(reloc.len);
			}
			cap.address() += offset;
			Debug::log("Writing cap reloc {}\nto  {}\nPCC {}\nCGP {}",
			           cap,
			           location,
			           pcc,
			           cgp);
			*location = cap;
		}
	}
} // namespace

// The parameters are passed by the boot assembly sequence.
// XXX: arguments have capptr templates, 4 roots
extern "C" SchedulerEntryInfo loader_entry_point(const ImgHdr &imgHdr,
                                                 void         *almightyPCC,
                                                 void         *almightySeal,
                                                 void         *almightyRW)
{
	SchedulerEntryInfo ret;
	volatile Uart     *uart16550;

	// Populate the 4 roots from system registers.
	Root::install_root<Root::ISAType::Execute>(almightyPCC);
	Root::install_root<Root::ISAType::Seal>(almightySeal);
	Root::install_root<Root::ISAType::RW>(almightyRW);

	auto populateUart = [](volatile Uart *uart, bool intr = false) {
		// Initialise the serial UART.
		// http://wiki.osdev.org/Serial_Ports
		uart->intrEnable = 0x00;
		// Set the MSB (DLAB) to set up baud rate.
		uart->lineControl = 0x83;
#ifdef SIMULATION
		constexpr uint32_t Divisor = 1;
#else
#	error "Unsupported UART"
#endif
		uart->data          = Divisor & 0xff;
		uart->intrEnable    = (Divisor >> 8) & 0xff;
		uart->lineControl   = 0x03;
		uart->intrIDandFifo = 0x01;
		// Enable RX interrupt.
		if (intr)
		{
			uart->intrEnable = 0x01;
		}
	};

	uart16550 =
	  build<volatile Uart,
	        Root::Type::RWGlobal,
	        PermissionSet{
	          Permission::Load, Permission::Store, Permission::Global}>(
	    LA_ABS(__export_mem_uart));
	// This is only for printf() debugging. No input interrupt is needed.
	populateUart(uart16550);

	// Set up the UART that's used for debug output.
	if constexpr (DebugLoader)
	{
		// Set the UART.  `Debug::log` and `Debug::Invariant` work after this
		// point.
		ExplicitUARTOutput::set_uart(uart16550);
	}

	Debug::log("UART initialised!");
	Debug::log("Header: {}", &imgHdr);
	Debug::log("Magic number: {}", imgHdr.magic);

	Debug::Invariant(imgHdr.is_magic_valid(),
	                 "Invalid magic field in header: {}",
	                 imgHdr.magic);

	// Do some sanity checking on the headers.
	ptraddr_t lastCodeEnd = LA_ABS(__compart_pccs);
	ptraddr_t lastDataEnd = LA_ABS(__compart_cgps);
	int       i           = 0;
	Debug::log("Checking compartments");
	for (auto &header : imgHdr.libraries_and_compartments())
	{
		Debug::log("Checking compartment headers for compartment {}", i);
		Debug::Invariant(
		  header.code.start() >= LA_ABS(__compart_pccs),
		  "Compartment {} PCC ({}) is before the PCC section start {}",
		  i,
		  header.code.start(),
		  LA_ABS(__compart_pccs));
		Debug::Invariant(
		  header.code.start() + header.code.size() <=
		    LA_ABS(__compart_pccs_end),
		  "Compartment {} PCC ({} + {}) extends after the PCC section end {}",
		  i,
		  header.code.start(),
		  header.code.size(),
		  LA_ABS(__compart_pccs_end));
		Debug::Invariant(
		  header.code.start() >= lastCodeEnd,
		  "Compartment {} overlaps previous compartment ({} < {}",
		  i,
		  header.code.start(),
		  lastCodeEnd);
		lastCodeEnd = header.code.start() + header.code.size();
		if (header.data.size() != 0)
		{
			Debug::Invariant(
			  header.data.start() >= LA_ABS(__compart_cgps),
			  "Compartment {} CGP ({}) is before the CGP section start {}",
			  i,
			  header.data.start(),
			  LA_ABS(__compart_cgps));
			Debug::Invariant(header.data.start() + header.data.size() <=
			                   LA_ABS(__compart_cgps_end),
			                 "Compartment {} CGP ({} + {}) extends after the "
			                 "CGP section end {}",
			                 i,
			                 header.data.start(),
			                 header.data.size(),
			                 LA_ABS(__compart_cgps_end));
			Debug::Invariant(
			  header.data.start() >= lastDataEnd,
			  "Compartment {} overlaps previous compartment ({} < {}",
			  i,
			  header.data.start(),
			  lastDataEnd);
		}
		lastDataEnd = header.data.start() + header.data.size();
		i++;
	}

	populate_caprelocs(imgHdr);

	auto switcherPCC = build<void, Root::Type::Execute, SwitcherPccPermissions>(
	  imgHdr.switcher.code);
	Debug::log("Setting compartment switcher");
	switcherPCC.address() = imgHdr.switcher.entry_point();
	switcherPCC           = seal_entry(switcherPCC, InterruptStatus::Disabled);

	auto setSealingKey = [](const auto &compartment,
	                        SealingType lower,
	                        size_t      length = 1,
	                        size_t      offset = 0) {
		Debug::log("Creating sealing key {}+{} to store at {} ({}-{})",
		           lower,
		           length,
		           compartment.sealing_key() + offset,
		           compartment.code.start(),
		           compartment.code.start() + compartment.code.size());
		// Writeable version of the compartment's PCC, for filling in the
		// sealing key.
		void **location = build<void *, Root::Type::RWStoreL>(
		  compartment.code, compartment.sealing_key() + offset);
		Debug::log("Sealing key location: {}", location);
		// Derive a sealing capability of the required length.
		void *key = build<void, Root::Type::Seal>(lower, length);
		// FIXME: Some compartments need only permit-unseal (e.g. the
		// compartment switcher).  Drop permit-seal and keep only
		// permit-unseal once these are separated.
		Debug::log("Sealing key: {}", key);
		*location = key;
		return key;
	};

	Debug::log("switcherKey: {}", &switcherKey);
	// Set up the sealing keys for the privileged components.
	switcherKey =
	  setSealingKey(imgHdr.switcher, Sentry, SealedTrustedStacks - Sentry + 1);
	// We need only the rights to seal things with the switcher's data sealing
	// types, so drop all others and store those two types separately.
	trustedStackKey           = switcherKey;
	trustedStackKey.address() = SealedTrustedStacks;
	trustedStackKey.bounds()  = 1;
	switcherKey.address()     = SealedImportTableEntries;
	switcherKey.bounds()      = 1;
	setSealingKey(imgHdr.scheduler(), Scheduler);
	setSealingKey(imgHdr.allocator(), Allocator);
	constexpr size_t DynamicSealingLength =
	  std::numeric_limits<ptraddr_t>::max() - FirstDynamicSoftware + 1;

	setSealingKey(imgHdr.allocator(),
	              FirstDynamicSoftware,
	              DynamicSealingLength,
	              sizeof(void *));

	// Set up export tables

	// Helper to construct a writeable pointer to an export table.
	auto getExportTableHeader = [](const auto &range) {
		auto header = build<ExportTableHeader>(range);
		Debug::Invariant(((header.address()) & 0x7) == 0,
		                 "Export table {} is not capability aligned\n",
		                 header);
		return header;
	};

	for (auto &compartment : imgHdr.privilegedCompartments)
	{
		auto expTablePtr = getExportTableHeader(compartment.exportTable);
		Debug::log("Error handler for compartment is {}",
		           expTablePtr->errorHandler);
		expTablePtr->pcc = build_pcc(compartment);
		expTablePtr->cgp = build_cgp(compartment);
	}

	for (auto &compartment : imgHdr.libraries_and_compartments())
	{
		auto expTablePtr = getExportTableHeader(compartment.exportTable);
		Debug::log("Error handler for compartment is {})",
		           expTablePtr->errorHandler);
		expTablePtr->pcc = build_pcc(compartment);
		expTablePtr->cgp = build_cgp(compartment);
	}

	Debug::log("First pass to find sealing key imports");

	// Populate import entries that refer to static sealing keys first.
	for (auto &compartment : imgHdr.privilegedCompartments)
	{
		populate_static_sealing_keys(imgHdr, compartment);
	}

	for (auto &compartment : imgHdr.libraries_and_compartments())
	{
		populate_static_sealing_keys(imgHdr, compartment);
	}

	Debug::log("Creating import tables");

	// Populate import tables.
	for (auto &compartment : imgHdr.privilegedCompartments)
	{
		populate_imports(imgHdr, compartment, switcherPCC);
	}

	for (auto &compartment : imgHdr.libraries_and_compartments())
	{
		populate_imports(imgHdr, compartment, switcherPCC);
	}

	Debug::log("Creating boot threads\n");
	boot_threads_create(imgHdr, ret.threads);
	// Provide the switcher with the capabilities for entering the scheduler.
	void *schedCGP = build_cgp(imgHdr.scheduler());
	auto  exceptionEntryOffset =
	  build<ExportEntry>(
	    imgHdr.scheduler().exportTable,
	    LA_ABS(
	      __export_sched__ZN5sched15exception_entryEP19TrustedStackGenericILj0EEjjj))
	    ->functionStart;
	auto schedExceptionEntry = build_pcc(imgHdr.scheduler());
	schedExceptionEntry.address() += exceptionEntryOffset;
	schedExceptionEntry =
	  seal_entry(schedExceptionEntry, InterruptStatus::Disabled);
	*build<void *>(imgHdr.switcher.code, imgHdr.switcher.scheduler_pcc()) =
	  schedExceptionEntry;
	*build<void *>(imgHdr.switcher.code, imgHdr.switcher.scheduler_cgp()) =
	  schedCGP;
	// The scheduler will inherit our stack once we're done with it
	Capability<void> csp = ({
		register void *cspRegister asm("csp");
		asm("" : "=C"(cspRegister));
		cspRegister;
	});
	// Reset the stack pointer to the top.
	csp.address() = csp.base() + csp.length();
	// csp is a local capability so must be written via a store-local cap.
	*build<void *, Root::Type::RWStoreL>(imgHdr.switcher.code,
	                                     imgHdr.switcher.scheduler_csp()) = csp;
	Debug::log(
	  "Scheduler exception entry configured:\nPCC: {}\nCGP: {}\nCSP: {}",
	  schedExceptionEntry,
	  schedCGP,
	  csp);

#ifdef SOFTWARE_REVOKER
	// If we are using a software revoker then we need to provide it with three
	// terrifyingly powerful capabilities.  These break some of the rules that
	// we enforce for everything else, especially the last one, which is a
	// stack capability that is reachable from a global.  The only code that
	// accesses these in the revoker is very small and amenable to auditing
	// (the only memory accesses are a load and a store back at the same
	// location, with interrupts disabled, to trigger the load barrier).
	auto scaryCapabilities = build<void *, Root::Type::RWStoreL>(
	  imgHdr.privilegedCompartments.software_revoker().code.start(),
	  3 * sizeof(void *));
	// Read-write capability to all globals.  This is scary because a bug in
	// the revoker could violate compartment isolation.
	Debug::log("Writing scary capabilities for software revoker to {}",
	           scaryCapabilities);
	scaryCapabilities[0] =
	  build(LA_ABS(__compart_cgps),
	        LA_ABS(__compart_cgps_end) - LA_ABS(__compart_cgps));
	Debug::log("Wrote scary capability {}", scaryCapabilities[0]);
	// Read-write capability to the whole heap.  This is scary because a bug in
	// the revoker could violate heap safety.
	scaryCapabilities[1] =
	  build(LA_ABS(__export_mem_heap),
	        LA_ABS(__export_mem_heap_end) - LA_ABS(__export_mem_heap));
	Debug::log("Wrote scary capability {}", scaryCapabilities[1]);
	// Read-write capability to the entire stack.  This is scary because a bug
	// in the revoker could violate thread isolation.
	scaryCapabilities[2] =
	  build<void, Root::Type::RWStoreL>(LA_ABS(stackSpace), sizeof(stackSpace));
	Debug::log("Wrote scary capability {}", scaryCapabilities[2]);
#endif

	// Set up the exception entry point
	auto exceptionEntry = build_pcc<SwitcherPccPermissions>(imgHdr.switcher);
	exceptionEntry.address() = LA_ABS(exception_entry_asm);
	asm volatile("cspecialw mtcc, %0" ::"C"(exceptionEntry.get()));
	Debug::log("Set exception entry point to {}", exceptionEntry);

	// Construct and enter the scheduler compartment.
	auto schedPCC = build_pcc(imgHdr.scheduler());
	// Set the scheduler entry point to the scheduler_entry export.
	// TODO: We should probably have a separate scheduler linker script that
	// exposes this.  If the scheduler is no longer creating threads, we can
	// probably just set up the trusted stack pointer to be the idle thread and
	// invoke the exception entry point.
	auto exportEntry = build<ExportEntry>(
	  imgHdr.scheduler().exportTable,
	  LA_ABS(
	    __export_sched__ZN5sched15scheduler_entryEPKNS_16ThreadLoaderInfoE));
	schedPCC.address() += exportEntry->functionStart;

	Debug::log("Will return to scheduler entry point: {}", schedPCC);

	ret.schedPCC = schedPCC;
	ret.schedCGP = schedCGP;
	return ret;
}

/**
 * a dumb implementation, assuming no overlap and no capabilities
 */
void *memcpy(void *dst, const void *src, size_t n)
{
	char       *dst0 = static_cast<char *>(dst);
	const char *src0 = static_cast<const char *>(src);

	for (size_t i = 0; i < n; ++i)
	{
		dst0[i] = src0[i];
	}

	return dst0;
}
