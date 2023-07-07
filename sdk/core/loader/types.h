// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "../switcher/tstack.h"
#include "defines.h"
#include <cdefs.h>
#include <cheri.hh>
#include <concepts>
#include <magic_enum/magic_enum.hpp>
#include <stddef.h>
#include <stdint.h>

namespace loader
{
	struct CapReloc
	{
		ptraddr_t addr;
		ptraddr_t base;
		size_t    offset;
		size_t    len;
		size_t    flags;

		/**
		 * Returns true if this is a function pointer, false otherwise.
		 */
		bool is_function()
		{
			constexpr size_t FunctionBit = 0x80000000;
			return flags & FunctionBit;
		}
	};

	class Root
	{
		public:
		/// Hardware roots. Each correspond to a real root defined in the ISA.
		enum class ISAType : size_t
		{
			/// Sealing root, has sealing and user-defined permissions.
			Seal,
			/// Store root.  Has memory-access permissions including store but
			/// not execute.
			RW,
			/// Execute root.  Has memory-access permissions including execute
			/// but not store.
			Execute,
		};

		/**
		 * Software-defined roots. Depending on the ISA, these may or may not
		 * correspond to actual hardware roots.
		 */
		enum class Type : size_t
		{
			/// Sealing type, for creating sealing and unsealing keys.
			Seal,
			/**
			 * Type for building trusted stacks.  Trusted stacks are accessible
			 * only by the switcher and are the only things that are permitted
			 * to have both global and permit-store-local permissions.  Only
			 * sealed versions of these ever leave the switcher.
			 */
			TrustedStack,
			/**
			 * Execute type, used to build capabilities that will become
			 * compartment and library PCCs.
			 */
			Execute,
			/**
			 * Global type, used for all non-stack (global, heap) capabilities.
			 */
			RWGlobal,
			/**
			 * Store-local root.  Used for building stacks and for the loader to
			 * store local capabilities.
			 */
			RWStoreL
		};

		/**
		 * The architectural root for each type.  Template generic case
		 * evaluates to a null pointer so that it will give a type error if
		 * used.
		 */
		template<Type>
		constexpr static std::nullptr_t ArchitecturalRoot = nullptr;
		/**
		 * The software-defined sealing root corresponds directly to the
		 * architectural sealing root.
		 */
		template<>
		constexpr static ISAType ArchitecturalRoot<Type::Seal> = ISAType::Seal;
		/**
		 * The software-defined trusted-stack root corresponds directly to the
		 * architectural read-write root.
		 */
		template<>
		constexpr static ISAType ArchitecturalRoot<Type::TrustedStack> =
		  ISAType::RW;
		/**
		 * The software-defined read-write global root is derived from the
		 * architectural rear-write root.
		 */
		template<>
		constexpr static ISAType ArchitecturalRoot<Type::RWGlobal> =
		  ISAType::RW;
		/**
		 * The software-defined store-local root is derived from the
		 * architectural rear-write root.
		 */
		template<>
		constexpr static ISAType ArchitecturalRoot<Type::RWStoreL> =
		  ISAType::RW;
		/**
		 * The software-defined executable root corresponds directly to the
		 * architectural executable root.
		 */
		template<>
		constexpr static ISAType ArchitecturalRoot<Type::Execute> =
		  ISAType::Execute;

		/**
		 * Mapping from architectural type to permissions.  Generic case is a
		 * null pointer to give type errors if ever used with an invalid value.
		 */
		template<ISAType>
		constexpr static std::nullptr_t ArchitecturalPermissions = nullptr;

		/**
		 * The permissions held by the sealing root.
		 */
		template<>
		constexpr static CHERI::PermissionSet
		  ArchitecturalPermissions<ISAType::Seal> = {CHERI::Permission::Global,
		                                             CHERI::Permission::Seal,
		                                             CHERI::Permission::Unseal,
		                                             CHERI::Permission::User0};
		/**
		 * The permissions held by the execute root.
		 */
		template<>
		constexpr static CHERI::PermissionSet
		  ArchitecturalPermissions<ISAType::Execute> = {
		    CHERI::Permission::Global,
		    CHERI::Permission::Execute,
		    CHERI::Permission::Load,
		    CHERI::Permission::LoadStoreCapability,
		    CHERI::Permission::LoadMutable,
		    CHERI::Permission::LoadGlobal,
		    CHERI::Permission::AccessSystemRegisters};
		/**
		 * The permissions held by the store root.
		 */
		template<>
		constexpr static CHERI::PermissionSet
		  ArchitecturalPermissions<ISAType::RW> = {
		    CHERI::Permission::Global,
		    CHERI::Permission::Load,
		    CHERI::Permission::Store,
		    CHERI::Permission::StoreLocal,
		    CHERI::Permission::LoadStoreCapability,
		    CHERI::Permission::LoadMutable,
		    CHERI::Permission::LoadGlobal};

		/**
		 * Permissions for this (software-defined) root.  By default, these are
		 * identical to the architectural root from which this is derived.
		 */
		template<Type Ty>
		constexpr static CHERI::PermissionSet Permissions =
		  ArchitecturalPermissions<ArchitecturalRoot<Ty>>;
		/**
		 * The permissions held by the global (software-defined) root.  In
		 * software, we ensure that nothing has both global and store-local
		 * permissions by deriving two new roots from the root that permits
		 * storing.  The global root has global permission but not store local.
		 */
		template<>
		constexpr static CHERI::PermissionSet Permissions<Type::RWGlobal> =
		  ArchitecturalPermissions<ISAType::RW>.without(
		    CHERI::Permission::StoreLocal);
		/**
		 * The permissions held by the store-local (software-defined) root.  In
		 * software, we ensure that nothing has both global and store-local
		 * permissions by deriving two new roots from the root that permits
		 * storing.  The store-local root has store-local permission but not
		 * global.
		 */
		template<>
		constexpr static CHERI::PermissionSet Permissions<Type::RWStoreL> =
		  ArchitecturalPermissions<ISAType::RW>.without(
		    CHERI::Permission::Global);

		/**
		 * Install a root corresponding to an architectural root type.
		 *
		 * This derives and stores capabilities for each of the
		 * software-defined roots that should inherit from this.  After this
		 * call, the architectural roots can be destroyed.
		 */
		template<ISAType ArchitecturalType>
		static inline void
		install_root(CHERI::Capability<void> architecturalRootCapability)
		{
			magic_enum::enum_for_each<Type>([&](auto val) {
				constexpr Type SoftwareType = val;
				if constexpr (ArchitecturalRoot<SoftwareType> ==
				              ArchitecturalType)
				{
					static_assert(
					  Permissions<SoftwareType>.can_derive_from(
					    ArchitecturalPermissions<ArchitecturalType>),
					  "Software-defined root expects more permissions than the "
					  "architectural root from which it is derived");
					auto softwareRoot = architecturalRootCapability;
					if constexpr (ArchitecturalPermissions<ArchitecturalType> !=
					              Permissions<SoftwareType>)
					{
						softwareRoot.permissions() &= Permissions<SoftwareType>;
					}
					roots[static_cast<size_t>(SoftwareType)] = softwareRoot;
				}
			});
		}

		template<Type                 Type,
		         CHERI::PermissionSet RequestedPermissions,
		         bool                 Precise = true>
		static inline void *build_from_root(ptraddr_t addr, size_t size)
		{
			check_permissions<RequestedPermissions, Permissions<Type>>();
			auto ret      = roots[static_cast<size_t>(Type)];
			ret.address() = addr;
			if constexpr (Precise)
			{
				ret.bounds() = size;
			}
			else
			{
				ret.bounds().set_inexact(size);
			}
			ret.permissions() &= RequestedPermissions;

			return ret;
		}

		private:
		/**
		 * Helper template that checks permissions.  This is used so that the
		 * permission bitmaps for the requested and original permissions are
		 * present in the compiler's error message, making it easy to see the
		 * ones that are missing.
		 */
		template<CHERI::PermissionSet Derived, CHERI::PermissionSet Original>
		static constexpr void check_permissions()
		{
			static_assert(Derived.can_derive_from(Original),
			              "Cannot derive the requested permissions from the "
			              "specified root");
		}

		/**
		 * The set of capabilities from which all others will be derived.
		 */
		static inline constinit CHERI::Capability<void>
		  roots[magic_enum::enum_count<Type>()];
	};

	/**
	 * Helper concept for determining if something is an address.
	 */
	template<typename T>
	concept IsAddress = std::same_as<T, ptraddr_t> ||
	  std::same_as<T, ptraddr_t &> || std::same_as<T, const ptraddr_t &>;

	/**
	 * Concept for a raw address range.  This exposes a range of addresses.
	 */
	template<typename T>
	concept RawAddressRange = requires(T range)
	{
		{
			range.size()
			} -> IsAddress;
		{
			range.start()
			} -> IsAddress;
	};

	/**
	 * The header that describes a firmware image.  This contains all of the
	 * information that the loader needs to find compartments and initialise
	 * imports and exports.
	 */
	struct __packed ImgHdr
	{
		/**
		 * Helper for encapsulating an address range.
		 */
		struct __packed AddressRange
		{
			/**
			 * Start address.
			 */
			ptraddr_t startAddress;

			/**
			 * Size in bytes.
			 */
			uint16_t smallSize;

			/**
			 * Returns the start address.
			 */
			[[nodiscard]] ptraddr_t start() const
			{
				return startAddress;
			}

			/**
			 * Returns the size as a 32-bit value.
			 */
			[[nodiscard]] size_t size() const
			{
				return smallSize;
			}
		};

		static_assert(RawAddressRange<AddressRange>,
		              "AddressRange should be a raw address range");

		/**
		 * The loader metadata.  This is simply the PCC and GDC ranges and is
		 * accessed only by the assembly stub that initialises the C++
		 * environment.
		 */
		struct __packed
		{
			/**
			 * The range for the loader's PCC.
			 */
			AddressRange code;

			/**
			 * The range for the loader's globals.
			 */
			AddressRange data;
		} loader;

		/**
		 * The header for the switcher.  This is not a full compartment, it has
		 * no state other than its code, which contains read-only globals for a
		 * sealing key and the capabilities required to enter the scheduler.
		 */
		struct __packed
		{
			/**
			 * The PCC for the compartment switcher.
			 */
			AddressRange code;

			/**
			 * The offset relative to the PCC of the switcher's entry point.
			 */
			uint16_t entryPoint;

			/**
			 * The PCC-relative location of the sealing key, which the
			 * compartment switcher will use to unseal import table entries.
			 */
			uint16_t sealingKey;

			/**
			 * The PCC-relative location of the scheduler's entry point
			 * capability. This is used by the switcher to transition to the
			 * scheduler.
			 */
			uint16_t schedulerPCC;

			/**
			 * The PCC-relative location of the scheduler's global capability.
			 * This is used by the switcher to transition to the scheduler.
			 */
			uint16_t schedulerCGP;

			/**
			 * The PCC-relative location of the scheduler's stack capability.
			 * This is used by the switcher to transition to the scheduler.
			 */
			uint16_t schedulerCSP;

			/**
			 * The entry point as a full address.
			 */
			[[nodiscard]] ptraddr_t entry_point() const
			{
				return code.start() + entryPoint;
			}

			/**
			 * The location of the sealing key as a full address.
			 */
			[[nodiscard]] ptraddr_t sealing_key() const
			{
				return code.start() + sealingKey;
			}

			/**
			 * The location of scheduler's PCC as a full address.
			 */
			[[nodiscard]] ptraddr_t scheduler_pcc() const
			{
				return code.start() + schedulerPCC;
			}

			/**
			 * The location of the scheduler's CGP as a full address.
			 */
			[[nodiscard]] ptraddr_t scheduler_cgp() const
			{
				return code.start() + schedulerCGP;
			}

			/**
			 * The location of the scheduler's CGP as a full address.
			 */
			[[nodiscard]] ptraddr_t scheduler_csp() const
			{
				return code.start() + schedulerCSP;
			}
		} switcher;

		/**
		 * Privileged compartment header.  Privileged compartments have a
		 * sealing key, in addition to the other attributes of a compartment,
		 * and may be handled differently by the loader.
		 */
		struct __packed PrivilegedCompartment
		{
			/**
			 * The compartment's PCC.
			 */
			AddressRange code;

			/**
			 * The compartment's globals.
			 */
			AddressRange data;

			/**
			 * The distance from the start of the code region to the end of the
			 * import table.
			 */
			uint16_t importTableSize;

			/**
			 * The export table for the scheduler.
			 */
			AddressRange exportTable;

			/**
			 * Returns the range of the import table.
			 */
			[[nodiscard]] AddressRange import_table() const
			{
				// Skip the sealing keys
				const size_t SealingKeysSize = 2 * sizeof(void *);
				return {
				  code.start() + SealingKeysSize,
				  static_cast<uint16_t>(importTableSize - SealingKeysSize)};
			}

			/**
			 * Returns the location of the sealing key used by the scheduler.
			 */
			[[nodiscard]] ptraddr_t sealing_key() const
			{
				return code.start();
			}

			/**
			 * Privileged libraries are privileged compartments without data
			 * segments.
			 */
			[[nodiscard]] bool is_privileged_library() const
			{
				return (data.start() == 0) && (data.size() == 0);
			}
		};

		class __packed
		{
			/**
			 * Indexes of the privileged compartments.
			 */
			enum PrivilegedCompartmentNumbers
			{
				/// The scheduler
				Scheduler,
				/// The memory allocator
				Allocator,
				/// The token server
				TokenServer,
#ifdef SOFTWARE_REVOKER
				/// The software revoker.  This privileged compartment is
				/// optional.
				SoftwareRevoker,
#endif
				/// The number of privileged compartments.
				PrivilegedCompartmentCount
			};

			/**
			 * Array of the privileged compartments.
			 */
			PrivilegedCompartment compartments[PrivilegedCompartmentCount];

			public:
			/**
			 * Returns a reference to the scheduler's header.
			 */
			[[nodiscard]] const PrivilegedCompartment &scheduler() const
			{
				return compartments[Scheduler];
			}

			/**
			 * Returns a reference to the allocator's header.
			 */
			[[nodiscard]] const PrivilegedCompartment &allocator() const
			{
				return compartments[Allocator];
			}

			[[nodiscard]] const PrivilegedCompartment &token_server() const
			{
				return compartments[TokenServer];
			}

#ifdef SOFTWARE_REVOKER
			/**
			 * Returns a reference to the software revoker's header.
			 *
			 * The software revoker may not exist in a firmware image, this
			 * method will be hidden at compile time if this is the case.
			 */
			[[nodiscard]] const PrivilegedCompartment &software_revoker() const
			{
				return compartments[SoftwareRevoker];
			}
#endif

			/**
			 * Returns an iterator for privileged compartments.
			 */
			[[nodiscard]] const PrivilegedCompartment *begin() const
			{
				return compartments;
			}

			/**
			 * Returns an end iterator for privileged compartments.
			 */
			[[nodiscard]] const PrivilegedCompartment *end() const
			{
				return compartments + PrivilegedCompartmentCount;
			}
		} privilegedCompartments;

		/**
		 * Convenience function to get the header for the scheduler.
		 */
		[[nodiscard]] const PrivilegedCompartment &scheduler() const
		{
			return privilegedCompartments.scheduler();
		}

		/**
		 * Convenience function to get the header for the allocator.
		 */
		[[nodiscard]] const PrivilegedCompartment &allocator() const
		{
			return privilegedCompartments.allocator();
		}

		[[nodiscard]] const PrivilegedCompartment &token_server() const
		{
			return privilegedCompartments.token_server();
		}

		/**
		 * Magic number.  A random value that is always at this location.
		 * Provides some sanity checking that the linker script is in sync with
		 * this header.
		 */
		uint32_t magic;

		/**
		 * Returns true if the magic field contains the expected value.
		 */
		[[nodiscard]] bool is_magic_valid() const
		{
			// This is a random 32-bit number and should be changed whenever
			// the compartment header layout changes to provide some sanity
			// checking.
			return magic == 0x43f6af90;
		}

		/**
		 * The total number of libraries.  This is the number of entries in
		 * the `compartments` array that are libraries.
		 */
		uint16_t libraryCount;

		/**
		 * The total number of compartments.  This is the number of entries in
		 * the `compartments` array that are stateful compartments.
		 */
		uint16_t compartmentCount;

		/**
		 * The raw compartment header that's generated in the final link step.
		 * This is parsed to an internal representation, used during linking,
		 * and then discarded.
		 */
		struct __packed CompartmentHeader
		{
			/**
			 * The bias in the code size.  The value stored in the header is
			 * left shifted by this quantity to give the correct value.
			 */
			static constexpr uint16_t CodeSizeShift = 3;

			/**
			 * Compartment headers should never be copied or moved, only
			 * referenced, so delete the copy constructor.
			 */
			CompartmentHeader(CompartmentHeader &) = delete;

			/**
			 * The code section for this compartment.
			 */
			struct __packed
			{
				/**
				 * Start of the PCC region.  Stored as a raw 32-bit address.
				 */
				ptraddr_t startAddress;

				/**
				 * The size of the PCC region.  The PCC region must be at least
				 * capability aligned and so this is stored as a 16-bit integer,
				 * right-shifted by 3, giving a maximum of 512 KiB of code per
				 * compartment.
				 */
				uint16_t rawSize;

				/**
				 * Returns the start address.
				 */
				[[nodiscard]] ptraddr_t start() const
				{
					return startAddress;
				}

				/**
				 * The size of the PC region.
				 */
				[[nodiscard]] size_t size() const
				{
					return static_cast<size_t>(rawSize) << CodeSizeShift;
				}
			} code;

			static_assert(
			  RawAddressRange<decltype(code)>,
			  "Compartment code range should be a raw address range");

			/**
			 * Length in bytes of the import table.  The start of the import
			 * table is always at the start of the code section.
			 */
			uint16_t importTableSize;

			/**
			 * Returns the import table as an address range.
			 */
			[[nodiscard]] AddressRange import_table() const
			{
				return {code.start(), importTableSize};
			}

			/**
			 * Export table start location.
			 */
			AddressRange exportTable;

			/**
			 * The (mutable) data section for this compartment.
			 */
			AddressRange data;

			/**
			 * The size in bytes of the data section that is initialised
			 * (everything after this should be zeroes).  This is currently
			 * unused but can eventually allow compartment reset.
			 */
			uint16_t initialisedDataSize;

			/**
			 * The range of the cap relocs for this section.
			 */
			AddressRange capRelocs;

			/**
			 * The range of statically allocated sealed objects for this
			 * compartment.
			 */
			AddressRange sealedObjects;

			/**
			 * The range of initialised data for this compartment.
			 */
			AddressRange initialised_data()
			{
				return {data.start(), initialisedDataSize};
			}

			/**
			 * The range of zeroed data for this compartment.
			 */
			AddressRange zeroed_data()
			{
				return {
				  data.start() + initialisedDataSize,
				  static_cast<uint16_t>(data.smallSize - initialisedDataSize)};
			}
		};

		/**
		 * Array of compartments.
		 */
		CompartmentHeader compartmentHeaders[];

		/**
		 * Class encapsulating a range of typed values.
		 */
		template<typename T>
		class Range
		{
			friend class ImgHdr;
			const T *first;
			const T *onePastEnd;
			Range(const T *first, const T *onePastEnd)
			  : first(first), onePastEnd(onePastEnd)
			{
			}

			public:
			/**
			 * Begin iterator.
			 */
			[[nodiscard]] const T *begin() const
			{
				return first;
			}

			/**
			 * End iterator.
			 */
			[[nodiscard]] const T *end() const
			{
				return onePastEnd;
			}
		};

		/**
		 * Structure describing the configuration of a single thread.
		 */
		struct __packed ThreadConfig
		{
			/**
			 * The priority of this thread.
			 */
			uint16_t priority;
			/**
			 * The address of the export table entry for the entry point for
			 * this thread.
			 */
			ptraddr_t entryPoint;
			/**
			 * The location for the stack for this thread.
			 */
			AddressRange stack;
			/**
			 * The location for the trusted stack for this thread.
			 */
			AddressRange trustedStack;
		};

		/**
		 * Structure describing the threads in this image.
		 */
		class __packed ThreadInfo
		{
			/**
			 * The total number of threads.
			 */
			uint16_t threadCount;

			/**
			 * The array of per-thread configuration data.
			 */
			ThreadConfig threadConfigs[];

			public:
			/**
			 * Returns the per-thread configuration data range.
			 */
			[[nodiscard]] Range<const ThreadConfig> threads() const
			{
				return {threadConfigs, &threadConfigs[threadCount]};
			}
		};

		/// Convenience type for a range of compartment headers.
		using CompartmentHeaderRange = Range<CompartmentHeader>;

		[[nodiscard]] const CompartmentHeaderRange libraries() const
		{
			return {compartmentHeaders, &compartmentHeaders[libraryCount]};
		}

		[[nodiscard]] const CompartmentHeaderRange compartments() const
		{
			return {&compartmentHeaders[libraryCount],
			        &compartmentHeaders[libraryCount + compartmentCount]};
		}

		[[nodiscard]] const CompartmentHeaderRange
		libraries_and_compartments() const
		{
			return {compartmentHeaders,
			        &compartmentHeaders[libraryCount + compartmentCount]};
		}

		[[nodiscard]] auto threads() const
		{
			return reinterpret_cast<const ThreadInfo *>(
			         &compartmentHeaders[libraryCount + compartmentCount])
			  ->threads();
		}
	};

	/**
	 * An entry in an import table.  This is an address and an optional size on
	 * load and becomes a capability.
	 */
	union ImportEntry
	{
		/**
		 * State on boot.
		 */
		struct
		{
			/**
			 * Address.  If the low bit is 1 and this points to an export table
			 * entry then this should be a direct call, skipping the compartment
			 * switcher and not changing $cgp.
			 */
			ptraddr_t address;

			/**
			 * The size.  This is currently unused except for MMIO imports.
			 */
			size_t size;
		};

		/**
		 * The initialised value of this.
		 */
		void *pointer;
	};

	struct ImportTable
	{
		void       *switcher;
		ImportEntry imports[];
	};

	/**
	 * The header of an export table.
	 */
	struct ExportTableHeader
	{
		/**
		 * The compartment's $pcc.
		 */
		void *pcc;

		/**
		 * The compartment's $cgp.
		 */
		void *cgp;

		/**
		 * The offset of the compartment's error handler from the start of
		 * `pcc`.  This must always be positive, a value of -1 is used to
		 * indicate that this compartment does not provide an error handler.
		 */
		ptrdiff_t errorHandler;
	};

	/**
	 * The interrupt status for a called function.
	 *
	 * The values correspond to the encoding in the export table.
	 */
	enum class InterruptStatus
	{
		/// Interrupt status is inherited from the caller.
		Inherited = 0,
		/// Interrupts are enabled.
		Enabled = 1,
		/// Interrupts are disabled.
		Disabled = 2,
	};

	/**
	 * An export table entry, used to describe a compartment's entry point.
	 */
	struct __packed ExportEntry
	{
		/**
		 * The shift amount to move the bits used to store the interrupt status
		 * to the low bits of the word.
		 */
		static constexpr uint8_t InterruptStatusShift = 3;

		/**
		 * The mask to isolate the bits that describe interrupt status.
		 */
		static constexpr uint8_t InterruptStatusMask = uint8_t(0b11)
		                                               << InterruptStatusShift;

		/**
		 * The flag indicating that this is a fake entry used to identify
		 * sealing types.  No import table entries should refer to this other
		 * than from the same compartment, which will be populated with a
		 * sealing capability. Statically sealed objects will have their first
		 * word initialised to point to this, the loader will set them up to
		 * instead hold the value of the sealing key.
		 */
		static constexpr uint8_t SealingTypeEntry = uint8_t(0b100000);

		static_assert((InterruptStatusMask & SealingTypeEntry) == 0);

		/**
		 * The offset from the start of this compartment's PCC of the called
		 * function.
		 *
		 * If this is a sealing key, then this will be filled in by the loader
		 * to indicate the sealing type.
		 */
		uint16_t functionStart;

		/**
		 * The minimum stack size required by this function.
		 */
		uint8_t minimumStackSize;

		/**
		 * Flags.  The low three bits indicate the number of registers that
		 * should be cleared in the compartment switcher.  The next two bits
		 * indicate the interrupt status.  The remaining three are currently
		 * unused.
		 */
		uint8_t flags;

		/**
		 * Returns the interrupt status for this entry.
		 */
		InterruptStatus interrupt_status()
		{
			uint8_t status =
			  (flags & InterruptStatusMask) >> InterruptStatusShift;
			return InterruptStatus(status);
		}

		/**
		 * Returns true if this export table entry is a static sealing type.
		 */
		bool is_sealing_type()
		{
			return (flags & SealingTypeEntry);
		}
	};

	/**
	 * Export table.  This is the full export table, including the header and
	 * the entries.
	 */
	struct ExportTable : public ExportTableHeader
	{
		/**
		 * The export entries in this table.
		 */
		ExportEntry entries[];
	};

#include "../switcher/export-table-assembly.h"
} // namespace loader
