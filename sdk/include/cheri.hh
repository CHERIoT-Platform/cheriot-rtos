// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
/**
 * C++ helpers for operating on capabilities.
 */
#include <cheri.h>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <magic_enum/magic_enum.hpp>

namespace CHERI
{
	/**
	 * The complete set of architectural permissions.
	 */
	enum class Permission : uint32_t
	{
		/**
		 * Capability refers to global memory (this capability may be stored
		 * anywhere).
		 */
		Global = CheriPermissionGlobal,
		/**
		 * Global capabilities can be loaded through this capability.  Without
		 *  this permission, any capability loaded via this capability will
		 *  have `Global` and `LoadGlobal` removed.
		 */
		LoadGlobal = CheriPermissionLoadGlobal,
		/**
		 * Capability may be used to store.  Any store via a capability without
		 * this permission will trap.
		 */
		Store = CheriPermissionStore,
		/**
		 * Capabilities with store permission may be loaded through this
		 * capability.  Without this, any loaded capability will have
		 * `LoadMutable` and `Store` removed.
		 */
		LoadMutable = CheriPermissionLoadMutable,
		/**
		 * This capability may be used to store capabilities that do not have
		 * `Global` permission.
		 */
		StoreLocal = CheriPermissionStoreLocal,
		/**
		 * This capability can be used to load.
		 */
		Load = CheriPermissionLoad,
		/**
		 * Any load and store permissions on this capability convey the right to
		 * load or store capabilities in addition to data.
		 */
		LoadStoreCapability = CheriPermissionLoadStoreCapability,
		/**
		 * If installed as the program counter capability, running code may
		 * access privileged system registers.
		 */
		AccessSystemRegisters = CheriPermissionAccessSystemRegisters,
		/**
		 * This capability may be used as a jump target and used to execute
		 * instructions.
		 */
		Execute = CheriPermissionExecute,
		/**
		 * This capability may be used to unseal other capabilities.  The
		 * 'address' range is in the sealing type namespace and not in the
		 * memory namespace.
		 */
		Unseal = CheriPermissionUnseal,
		/**
		 * This capability may be used to seal other capabilities.  The
		 * 'address' range is in the sealing type namespace and not in the
		 * memory namespace.
		 */
		Seal = CheriPermissionSeal,
		/**
		 * Software defined permission bit, no architectural meaning.
		 */
		User0 = CheriPermissionUser0
	};

	/**
	 * Class encapsulating a set of permissions.
	 */
	class PermissionSet
	{
		/**
		 * Helper that returns the bit associated with a given permission.
		 */
		static constexpr uint32_t permission_bit(Permission p)
		{
			return 1 << static_cast<uint32_t>(p);
		}

		/**
		 * Helper for building permissions, adds a permission to the raw
		 * bitfield.
		 */
		__always_inline constexpr void add_permission(Permission p)
		{
			rawPermissions |= permission_bit(p);
		}

		/**
		 * Private constructor for creating a permission set from a raw bitmask.
		 * This should never be used accidentally and so is hidden behind a
		 * factory method with an explicit name.  Callers should use
		 * `PermissionSet::from_raw`.
		 */
		constexpr PermissionSet(uint32_t rawPermissions)
		  : rawPermissions(rawPermissions)
		{
		}

		public:
		/**
		 * Computes (at compile time) a bitmask containing the set of valid
		 * permission bits.
		 *
		 * FIXME would ideally make this private and expose a public static
		 * constexpr field but this seems to trigger a compiler bug when trying
		 * to initialise said field using this function.
		 */
		static constexpr uint32_t valid_permissions_mask()
		{
			uint32_t mask = 0;
			for (auto permission : magic_enum::enum_values<Permission>())
			{
				mask |= 1 << static_cast<uint32_t>(permission);
			}
			return mask;
		}

		private:
		/**
		 * Permissions iterator.  Stores the permissions and iterates over them
		 * one bit at a time.
		 */
		class Iterator
		{
			/// `PermissionSet` may construct this.
			friend class PermissionSet;
			/// The raw permissions bitmap.
			uint32_t permissions;
			/// Constructor, take a raw permissions bitmap.
			constexpr Iterator(uint32_t rawPermissions)
			  : permissions(rawPermissions)
			{
			}

			public:
			/**
			 * Dereference, returns the lowest-numbered permission.
			 */
			constexpr Permission operator*()
			{
				return Permission(__builtin_ffs(permissions) - 1);
			}

			/**
			 * Preincrement, drops the lowest-numbered permission.
			 */
			constexpr Iterator &operator++()
			{
				permissions &= ~(1 << (__builtin_ffs(permissions) - 1));
				return *this;
			}

			/**
			 * Returns true if the other iterator has a different set of
			 * permissions.
			 */
			constexpr bool operator!=(const Iterator Other)
			{
				return permissions != Other.permissions;
			}
		};

		public:
		/**
		 * The raw bitmap of permissions.  This is public so that this class
		 * meets the requirements of a structural type and can therefore be
		 * used as a template parameter.  This field should never be directly
		 * modified.
		 */
		uint32_t rawPermissions = 0;

		/**
		 * Constructs a permission set from a raw permission mask.
		 */
		static constexpr PermissionSet from_raw(uint32_t raw)
		{
			raw &= valid_permissions_mask();
			return {raw};
		}

		/**
		 * Constructs a permission set from a single permission.
		 */
		constexpr PermissionSet(Permission p)
		{
			add_permission(p);
		}

		/**
		 * Construct a permission set from a list of permissions.
		 */
		__always_inline constexpr PermissionSet(
		  std::initializer_list<Permission> permissions)
		{
			for (auto p : permissions)
			{
				add_permission(p);
			}
		}

		/**
		 * Copy constructor.
		 */
		constexpr PermissionSet(const PermissionSet &other)

		  = default;

		/**
		 * Returns a permission set representing all permissions.
		 */
		constexpr static PermissionSet omnipotent()
		{
			return PermissionSet{valid_permissions_mask()};
		}

		/**
		 * And-permissions operation, creates a new permission set containing
		 * only permissions present in both this set and the argument.
		 */
		constexpr PermissionSet operator&(PermissionSet p)
		{
			return PermissionSet{rawPermissions & p.rawPermissions};
		}

		/**
		 * And-permissions operation, removes all permissions that are not
		 * present in both permission sets.
		 */
		constexpr PermissionSet &operator&=(PermissionSet p)
		{
			rawPermissions &= p.rawPermissions;
			return *this;
		}

		/**
		 * Constructs a new permission set without the specified permission.
		 */
		[[nodiscard]] constexpr PermissionSet without(Permission p) const
		{
			return {rawPermissions & ~permission_bit(p)};
		}

		/**
		 * Constructs a new permission set without the specified permissions.
		 */
		template<std::same_as<Permission>... Permissions>
		[[nodiscard]] constexpr PermissionSet without(Permission p,
		                                              Permissions... ps) const
		{
			return this->without(p).without(ps...);
		}

		/**
		 * Returns true if, and only if, this permission set can be derived
		 * from the argument set.
		 */
		[[nodiscard]] constexpr bool can_derive_from(PermissionSet other) const
		{
			return (rawPermissions & other.rawPermissions) == rawPermissions;
		}

		/**
		 * Returns true if this permission set contains the specified
		 * permission.
		 */
		[[nodiscard]] constexpr bool contains(Permission permission) const
		{
			return (permission_bit(permission) & rawPermissions) ==
			       permission_bit(permission);
		}

		/**
		 * Returns true if this permission set contains the specified
		 * permissions.
		 */
		template<std::same_as<Permission>... Permissions>
		[[nodiscard]] constexpr bool contains(Permission p,
		                                      Permissions... ps) const
		{
			return this->contains(p) && this->contains(ps...);
		}

		/**
		 * Returns the raw permission mask as an integer containing a bitfield
		 * of permissions.
		 */
		[[nodiscard]] constexpr uint32_t as_raw() const
		{
			return rawPermissions;
		}

		/**
		 * Returns an iterator over the permissions starting at the
		 * lowest-numbered permission.
		 */
		[[nodiscard]] constexpr Iterator begin() const
		{
			return {rawPermissions};
		}

		/**
		 * Returns an end iterator.
		 */
		[[nodiscard]] constexpr Iterator end() const
		{
			// Each increment of an iterator will drop one permission and so an
			// iterator will compare equal to {0} once all permissions have
			// been dropped.
			return {0};
		}

		/**
		 * Three-way comparison.  Treats a superset as greater-than, identical
		 * permissions as equivalent, and sets that don't have a superset
		 * releationship as unordered.
		 */
		constexpr auto operator<=>(const PermissionSet Other) const
		{
			if (rawPermissions == Other.rawPermissions)
			{
				return std::partial_ordering::equivalent;
			}
			if (can_derive_from(Other))
			{
				return std::partial_ordering::less;
			}
			if (Other.can_derive_from(*this))
			{
				return std::partial_ordering::greater;
			}
			return std::partial_ordering::unordered;
		}

		/**
		 * Equality operator, wraps the three-way compare operator.
		 */
		constexpr bool operator==(PermissionSet other) const
		{
			// Clang-tidy spuriously suggests that this 0 should be nullptr.
			return (*this <=> other) == 0; // NOLINT(modernize-use-nullptr)
		}
	};

	/**
	 * Rounds `len` up to a CHERI representable length for the current
	 * architecture.
	 */
	__always_inline inline size_t representable_length(size_t length)
	{
		return __builtin_cheri_round_representable_length(length);
	}

	/**
	 * Returns the alignment mask required for a given length.
	 */
	__always_inline inline size_t representable_alignment_mask(size_t length)
	{
		return __builtin_cheri_representable_alignment_mask(length);
	}

	/// Can the range [base, base + size) be precisely covered by a capability?
	inline bool is_precise_range(ptraddr_t base, size_t size)
	{
		return (base & ~representable_alignment_mask(size)) == 0 &&
		       representable_length(size) == size;
	}

	/**
	 * Helper class for accessing capability properties on pointers.
	 */
	template<typename T>
	class Capability
	{
		protected:
		/// The capability that this class wraps.
		T *ptr;

		private:
		/**
		 * Constructs a PermissionSet with the permissions of the given pointer.
		 */
		static PermissionSet permission_set_from_pointer(const void *p)
		{
			auto perms = __builtin_cheri_perms_get(p);
			auto mask  = PermissionSet::valid_permissions_mask();
			/* FIXME teach the compiler that the builtin always returns a value
			 * that is a subset of the mask, otherwise it unnecessarily
			 * constructs and applies the mask in from_raw */
			__builtin_assume((perms & ~mask) == 0);
			return PermissionSet::from_raw(perms);
		}

		/**
		 * Base class for the proxies that accessors in this class return.
		 */
		class PropertyProxyBase
		{
			protected:
			/**
			 * The capability that this proxy refers to.
			 */
			Capability &cap;

			/**
			 * Replaces the underlying capability
			 */
			template<typename U>
			void set(U *newPtr)
			{
				cap.ptr = static_cast<T *>(newPtr);
			}

			/**
			 * Returns the capability's pointer.
			 */
			[[nodiscard]] T *ptr() const
			{
				return cap.ptr;
			}

			public:
			/// Constructor, takes the capability whose property this class is
			/// proxying.
			PropertyProxyBase(Capability &c) : cap(c) {}
		};

		/**
		 * Proxy for accessing a capability's address.
		 */
		struct AddressProxy : public PropertyProxyBase
		{
			/// Inherit the constructor from the base class.
			using PropertyProxyBase::PropertyProxyBase;
			/// Inherit the pointer accesors
			/// @{
			using PropertyProxyBase::ptr;
			using PropertyProxyBase::set;
			/// @}

			/**
			 * Implicit casts can convert this to an address.
			 */
			operator ptraddr_t() const
			{
				return __builtin_cheri_address_get(ptr());
			}

			/**
			 * Set the address in the underlying capability.
			 */
			AddressProxy &operator=(ptraddr_t addr)
			{
				set(__builtin_cheri_address_set(ptr(), addr));
				return *this;
			}

			/**
			 * Set the address in the underlying capability given another
			 * address proxy.
			 */
			AddressProxy &operator=(AddressProxy addr)
			{
				set(__builtin_cheri_address_set(ptr(), addr));
				return *this;
			}

			/**
			 * Add a displacement to the capability's address.
			 */
			AddressProxy &operator+=(ptrdiff_t displacement)
			{
				set(__builtin_cheri_offset_increment(ptr(), displacement));
				return *this;
			}

			/**
			 * Subtract a displacement from the capability's address.
			 */
			AddressProxy &operator-=(ptrdiff_t displacement)
			{
				set(__builtin_cheri_offset_increment(ptr(), -displacement));
				return *this;
			}
		};

		/**
		 * Proxy for accessing an object's bounds.
		 */
		struct BoundsProxy : public PropertyProxyBase
		{
			/// Inherit the constructor from the base class.
			using PropertyProxyBase::PropertyProxyBase;
			/// Inherit the pointer accesors
			/// @{
			using PropertyProxyBase::ptr;
			using PropertyProxyBase::set;
			/// @}

			/**
			 * Return the object's bounds (displacement from the address to the
			 * end).
			 */
			operator ptrdiff_t() const
			{
				return __builtin_cheri_length_get(ptr()) -
				       (__builtin_cheri_address_get(ptr()) -
				        __builtin_cheri_base_get(ptr()));
			}

			/**
			 * Set the capability's bounds, giving an invalid capability if this
			 * cannot be represented exactly.
			 */
			BoundsProxy &operator=(size_t bounds)
			{
				set(__builtin_cheri_bounds_set_exact(ptr(), bounds));
				return *this;
			}

			/**
			 * Set the bounds, adding some padding (up to the bounds of the
			 * original capability) if necessary for alignment.
			 */
			BoundsProxy &set_inexact(size_t bounds)
			{
				set(__builtin_cheri_bounds_set(ptr(), bounds));
				return *this;
			}

			private:
			BoundsProxy &set_inexact_at_most_slow(size_t bounds)
			{
				ptraddr_t newBaseAddress = this->cap.address();

				// The number of bits in CHERIoT's capability encoding's
				// mantissa.  This is part of the capability encoding and
				// so, ideally, wouldn't be hard coded here.
				static constexpr size_t MantissaBits = 9;

				// The maximum possible representable length given the new
				// base is a full mantissa width of 1s followed by 0s with
				// its least significant 1 aligned to the least significant
				// 1 in the base address.
				size_t maximumLength = ((1 << MantissaBits) - 1)
				                       << __builtin_ctz(newBaseAddress);

				// Ensure that the requested length is representable by
				// making sure that it fits within a mantissa width,
				// rounding down by dropping any lower bits.  This might be
				// excessive by up to one bit position, because the
				// representable alignment mask is designed to work with the
				// rounding-up inexact bounds setting instruction.  As a result,
				// we might not return the largest possible representable
				// length, but we won't return a wildly too small one, either.
				size_t alignedLength =
				  bounds & representable_alignment_mask(bounds);

				// Select the smaller of those two lengths.
				bounds = std::min<size_t>(alignedLength, maximumLength);
				*this  = bounds;
				return *this;
			}

			public:
			/**
			 * Set the bounds to `length` if `length` is representable with the
			 * current alignment of `buffer`. If not, then select a smaller
			 * `length` that is representable.  Unlike set_inexact(), the
			 * resulting base will always be the current address; that is, there
			 * will be no padding below the current address.
			 *
			 * The caller must call .length() on the resulting capability to
			 * determine the imposed bounds.
			 *
			 * See is_precise_range().
			 */
			__always_inline BoundsProxy &set_inexact_at_most(size_t bounds)
			{
				// Just try to set the requested bounds, first.  If that works,
				// there's no need for bit-twiddling at all.
				Capability p = ptr();
				p.bounds()   = bounds;
				if (p.is_valid())
				{
					set(static_cast<T *>(p));
					return *this;
				}

				return set_inexact_at_most_slow(bounds);
			}
		};

		/**
		 * Proxy for accessing a capability's permissions
		 */
		struct PermissionsProxy : public PropertyProxyBase
		{
			/// Inherit the constructor from the base class.
			using PropertyProxyBase::PropertyProxyBase;
			/// Inherit the pointer accesors
			/// @{
			using PropertyProxyBase::ptr;
			using PropertyProxyBase::set;
			/// @}

			/**
			 * Implicitly convert to a permission set.
			 */
			operator PermissionSet() const
			{
				return permission_set_from_pointer(ptr());
			}

			/**
			 * And-permissions operation, removes all permissions that are not
			 * present in both permission sets from the capability.
			 */
			PermissionsProxy &operator&=(PermissionSet permissions)
			{
				set(__builtin_cheri_perms_and(ptr(), permissions.as_raw()));
				return *this;
			}

			/**
			 * Returns a permission set containing only the permissions held by
			 * the capability and the argument.
			 */
			constexpr PermissionSet operator&(PermissionSet p)
			{
				return static_cast<PermissionSet>(*this) & p;
			}

			/**
			 * Constructs a new permission set without the specified
			 * permissions.
			 */
			template<std::same_as<Permission>... Permissions>
			constexpr PermissionSet without(Permissions... ps) const
			{
				return static_cast<PermissionSet>(*this).without(ps...);
			}

			/**
			 * Returns true if, and only if, this permission set can be derived
			 * from the argument set.
			 */
			[[nodiscard]] constexpr bool
			can_derive_from(PermissionSet other) const
			{
				return static_cast<PermissionSet>(*this).can_derive_from(other);
			}

			/**
			 * Returns true if this permission set contains the specified
			 * permissions.
			 */
			template<std::same_as<Permission>... Permissions>
			constexpr bool contains(Permissions... permissions) const
			{
				return static_cast<PermissionSet>(*this).contains(
				  permissions...);
			}

			/**
			 * Returns the raw permission mask as an integer containing a
			 * bitfield of permissions.
			 */
			[[nodiscard]] constexpr uint32_t as_raw() const
			{
				return static_cast<PermissionSet>(*this).as_raw();
			}

			/**
			 * Returns an iterator over the permissions starting at the
			 * lowest-numbered permission.
			 */
			auto begin()
			{
				return static_cast<PermissionSet>(*this).begin();
			}

			/**
			 * Returns an end iterator.
			 */
			auto end()
			{
				return static_cast<PermissionSet>(*this).end();
			}

			/**
			 * Comparison operator.
			 */
			constexpr std::partial_ordering
			operator<=>(const PermissionSet Other) const
			{
				return static_cast<PermissionSet>(*this) <=> Other;
			}

			/**
			 * Equality operator, wraps the three-way compare operator.
			 */
			constexpr bool operator==(const PermissionSet Other) const
			{
				return (*this <=> Other) == 0;
			}
		};

		/// The property proxy base is allowed to directly access the pointer
		/// that this class wraps.
		friend class PropertyProxyBase;

		public:
		/// Constructor from a null pointer.
		constexpr Capability(std::nullptr_t) : ptr(nullptr) {}
		/// Default constructor, initialises with a null pointer.
		constexpr Capability() : ptr(nullptr) {}
		/// Constructor, takes an existing pointer to wrap
		constexpr Capability(T *p) : ptr(p) {}
		/// Copy constructor, aliases the object that is pointed to by `ptr`.
		constexpr Capability(const Capability &other) : ptr(other.ptr) {}
		/// Move constructor.
		constexpr Capability(Capability &&other) : ptr(other.ptr)
		{
			other.ptr = nullptr;
		}

		/**
		 * Replace the pointer that this capability wraps with another.
		 */
		Capability &operator=(const Capability &other)
		{
			ptr = other.ptr;
			return *this;
		}

		/**
		 * Transfer the pointer that this capability wraps from .
		 */
		Capability &operator=(Capability &&other)
		{
			ptr       = other.ptr;
			other.ptr = nullptr;
			return *this;
		}

		/**
		 * Access the address of the capability.
		 */
		AddressProxy address() [[clang::lifetimebound]]
		{
			return {*this};
		}

		/**
		 * Return the address as an integer from a `const` capability.
		 */
		[[nodiscard]] ptraddr_t address() const
		{
			return __builtin_cheri_address_get(ptr);
		}

		/**
		 * Access (read, set) the capability's bounds.
		 */
		BoundsProxy bounds() [[clang::lifetimebound]]
		{
			return {*this};
		}

		/**
		 * Return the bounds as an integer.
		 */
		[[nodiscard]] ptrdiff_t bounds() const
		{
			return __builtin_cheri_length_get(ptr) -
			       (__builtin_cheri_address_get(ptr()) -
			        __builtin_cheri_base_get(ptr()));
		}

		/**
		 * Access the permissions of this capability.
		 */
		PermissionsProxy permissions() [[clang::lifetimebound]]
		{
			return {*this};
		}

		/**
		 * Get a copy of the permissions from a `const` capability.
		 */
		[[nodiscard]] PermissionSet permissions() const
		{
			return permission_set_from_pointer(ptr);
		}

		/**
		 * Remove some permissions from this capability.
		 *
		 * Because this function computes the permission mask by clearing bits
		 * in the PermissionSet::omnipotent() all-ones mask, rather than from
		 * the set of permissions currently held by this Capability, it is safe
		 * to use to clear Global permission on a sealed capability.
		 */
		template<std::same_as<Permission>... Permissions>
		void without_permissions(Permissions... drop)
		{
			permissions() &= PermissionSet::omnipotent().without(drop...);
		}

		/**
		 * Pointer subtraction.
		 */
		Capability operator-(ptrdiff_t diff)
		{
			return {ptr - diff};
		}

		/**
		 * Pointer subtraction.
		 */
		Capability &operator-=(ptrdiff_t diff)
		{
			ptr -= diff;
			return *this;
		}

		/**
		 * Pointer addition.
		 */
		Capability operator+(ptrdiff_t diff)
		{
			return {ptr + diff};
		}

		/**
		 * Pointer addition.
		 */
		Capability &operator+=(ptrdiff_t diff)
		{
			ptr += diff;
			return *this;
		}

		/**
		 * Returns the tag bit indicating whether this is a valid capability.
		 */
		[[nodiscard]] bool is_valid() const
		{
			// The clang static analyser doesn't yet know that null is untagged
			// and so warns of possible null dereferences after this method
			// returns true.  Explicitly assume that a tagged thing is non-null
			// to fix this.
			if (__builtin_cheri_tag_get(ptr))
			{
				__builtin_assume(ptr != nullptr);
				return true;
			}
			return false;
		}

		/**
		 * Return whether this is a sealed capability.
		 */
		[[nodiscard]] bool is_sealed() const
		{
			return __builtin_cheri_type_get(ptr) != 0;
		}

		/**
		 * Returns the type of this capability, 0 if this is not a sealed
		 * capability.
		 */
		[[nodiscard]] uint32_t type() const
		{
			return __builtin_cheri_type_get(ptr);
		}

		/**
		 * Returns the base address of this capability.
		 */
		[[nodiscard]] ptraddr_t base() const
		{
			return __builtin_cheri_base_get(ptr);
		}

		/**
		 * Returns the length of this capability.
		 */
		[[nodiscard]] size_t length() const
		{
			return __builtin_cheri_length_get(ptr);
		}

		/**
		 * Returns the address of the top of this capability.
		 */
		[[nodiscard]] ptraddr_t top() const
		{
			return base() + length();
		}

		/**
		 * Capability comparison.  Defines ordered comparison for capabilities
		 * with the same bounds and permissions.  All other capabilities are
		 * either equivalent (identical bit pattern, including the tag bit) or
		 * unordered.
		 */
		constexpr std::partial_ordering operator<=>(T *other) const
		{
			return (*this <=> Capability<T>{other}) == 0;
		}

		/**
		 * Comparison against null pointer.
		 *
		 * Returns equivalent if this is a canonical null pointer, returns
		 * unordered for any other (tagged or untagged) value.  Callers may
		 * often want `is_valid` instead of this.
		 */
		constexpr std::partial_ordering operator<=>(std::nullptr_t) const
		{
			if (__builtin_cheri_equal_exact(ptr, nullptr))
			{
				return std::partial_ordering::equivalent;
			}
			return std::partial_ordering::unordered;
		}

		constexpr bool operator==(const Capability Other) const
		{
			return __builtin_cheri_equal_exact(ptr, Other.ptr);
		}

		/**
		 * Capability comparison.  Defines ordered comparison for capabilities
		 * with the same bounds and permissions.  All other capabilities are
		 * either equivalent (identical bit pattern, including the tag bit) or
		 * unordered.
		 *
		 * Callers may want to compare addresses, rather than capabilities, if
		 * they want a defined comparison that is stable between two objects.
		 */
		constexpr std::partial_ordering
		operator<=>(const Capability Other) const
		{
			if (__builtin_cheri_equal_exact(ptr, Other.ptr))
			{
				return std::partial_ordering::equivalent;
			}
			// If neither capability is sealed, check if everything except the
			// address is the same and define ordered comparison on pointers to
			// the same object.
			if (!(is_sealed() || Other.is_sealed()) &&
			    __builtin_cheri_equal_exact(__builtin_address_set(
			      ptr, __builtin_address_get(Other), Other)))
			{
				return static_cast<ptraddr_t>(ptr) <=>
				       static_cast<ptraddr_t>(Other);
			}
			// Comparison of pointers to different objects is ub, you probably
			// want address comparison:
			return std::partial_ordering::unordered;
		}

		/**
		 * Equality operator, wraps the three-way compare operator.
		 */
		constexpr bool operator==(std::nullptr_t) const
		{
			return (*this <=> nullptr) == 0;
		}

		/**
		 * Implicit cast to the raw pointer type.
		 */
		template<typename U = T>
		requires(!std::same_as<U, void>) operator U *()
		{
			return ptr;
		}

		/**
		 * Implicit cast to a raw pointer type.
		 */
		operator void *()
		{
			return ptr;
		}

		/**
		 * Access fields of the target as if this were a raw pointer.
		 */
		T *operator->()
		{
			return ptr;
		}

		/**
		 * Explicitly get the raw pointer.
		 */
		T *get()
		{
			return ptr;
		}

		/**
		 * Dereference operator.
		 */
		template<typename U = T>
		requires(!std::same_as<U, void>) U &operator*()
		{
			return *ptr;
		}

		/**
		 * Cast this capability to some other type.
		 */
		template<typename U>
		Capability<U> cast()
		{
			return {static_cast<U *>(ptr)};
		}

		/**
		 * Returns true if the tags of `this` and `other` match and if `this`
		 * conveys no rights that are not present in `other`.  Returns false in
		 * all other cases.
		 */
		template<typename U>
		bool is_subset_of(Capability<U> other)
		{
			return __builtin_cheri_subset_test(other.ptr, ptr);
		}

		/**
		 * Seal this capability with the given key.
		 */
		Capability<T> &seal(void *key)
		{
			ptr = static_cast<T *>(__builtin_cheri_seal(ptr, key));
			return *this;
		}

		/**
		 * Unseal this capability with the given key.
		 */
		Capability<T> &unseal(void *key)
		{
#ifdef FLUTE
			// Flute still throws exceptions on invalid use.  As a temporary
			// work-around, add a quick check that this thing has the sealing
			// type and don't unseal if it hasn't.  This isn't a complete test,
			// it's just sufficient to get the tests passing on Flute.
			if (type() != __builtin_cheri_address_get(key))
			{
				ptr = nullptr;
			}
			else
#endif
			{
				ptr = static_cast<T *>(__builtin_cheri_unseal(ptr, key));
			}
			return *this;
		}

		/**
		 * Subscript operator.
		 */
		template<typename U = T>
		requires(!std::same_as<U, void>) U &operator[](size_t index)
		{
			return ptr[index];
		}

		/**
		 * Returns true if the capability is `align`-byte aligned, false
		 * otherwise.
		 */
		bool is_aligned(size_t align)
		{
			return __builtin_is_aligned(ptr, align);
		}

		/**
		 * Aligns the capability down to the nearest `align`-byte boundary.
		 */
		Capability &align_down(size_t align)
		{
			ptr = __builtin_align_down(ptr, align);
			return *this;
		}

		/**
		 * Aligns the capability up to the nearest `align`-byte boundary.
		 */
		Capability &align_up(size_t align)
		{
			ptr = __builtin_align_up(ptr, align);
			return *this;
		}
	};

	/**
	 * Concept that matches pointers.
	 */
	template<typename T>
	concept IsPointer = std::is_pointer_v<T>;

	/**
	 * Concept that matches smart pointers, i.e., classes which implements
	 * a `get` method returning a pointer, and supports `operator=` with
	 * the return value of `get`. This will match `Capability`, standard
	 * library smart pointers, etc.
	 */
	template<typename T>
	concept IsSmartPointerLike = requires(T b)
	{
		{
			b.get()
			} -> IsPointer;
	}
	&&requires(T b)
	{
		b = b.get();
	};

	/**
	 * Checks that `ptr` is valid, unsealed, has at least `Permissions`,
	 * and has at least `Space` bytes after the current offset.
	 *
	 * `ptr` can be a pointer, or a smart pointer, i.e., any class that
	 * supports a `get` method returning a pointer, and `operator=`. This
	 * includes `Capability` and standard library smart pointers.
	 *
	 * If the permissions do not include Global, then this will also check
	 * that the capability does not point to the current thread's stack.
	 * This behaviour can be disabled (for example, for use in a shared
	 * library) by passing `false` for `CheckStack`.
	 *
	 * If `EnforceStrictPermissions` is set to `true`, this will also set
	 * the permissions of passed capability reference to `Permissions`, and
	 * its bounds to `space`. This is useful for detecting cases where
	 * compartments ask for less permissions than they actually require.
	 *
	 * This function is provided as a wrapper for the `::check_pointer` C
	 * API. It is always inlined. For each call site, it materialises the
	 * constants needed before performing an indirect call to
	 * `::check_pointer`.
	 */
	template<PermissionSet Permissions = PermissionSet{Permission::Load},
	         bool          CheckStack  = true,
	         bool          EnforceStrictPermissions = false>
	__always_inline inline bool check_pointer(
	  auto  &ptr,
	  size_t space = sizeof(
	    std::remove_pointer<
	      decltype(ptr)>)) requires(std::
	                                  is_pointer_v<
	                                    std::remove_cvref_t<decltype(ptr)>> ||
	                                IsSmartPointerLike<
	                                  std::remove_cvref_t<decltype(ptr)>>)
	{
		// We can skip a stack check if we've asked for Global because the
		// stack does not have this permission.
		constexpr bool StackCheckNeeded =
		  CheckStack && !Permissions.contains(Permission::Global);
		constexpr bool IsRawPointer =
		  std::is_pointer_v<std::remove_cvref_t<decltype(ptr)>>;

		bool isValid;
		if constexpr (IsRawPointer)
		{
			// If passed `ptr` as a raw capability (e.g., `void*`),
			// pass it as-is to ::check_pointer.
			isValid = ::check_pointer(
			  ptr, space, Permissions.as_raw(), StackCheckNeeded);
		}
		else
		{
			// Otherwise, call `get` on `ptr` to retrieve a raw
			// capability.
			isValid = ::check_pointer(
			  ptr.get(), space, Permissions.as_raw(), StackCheckNeeded);
		}
		// If passed `EnforceStrictPermissions`, set the permissions
		// of `ptr` to `Permissions`, and its bounds to `space`
		if constexpr (EnforceStrictPermissions)
		{
			if (isValid)
			{
				if constexpr (IsRawPointer)
				{
					Capability cap{ptr};
					cap.permissions() &= Permissions;
					cap.bounds() = space;
					ptr          = cap.get();
				}
				else
				{
					Capability cap{ptr.get()};
					cap.permissions() &= Permissions;
					cap.bounds() = space;
					ptr          = cap.get();
				}
			}
		}
		return isValid;
	}

	/**
	 * Invokes the passed callable object with interrupts disabled.
	 */
	template<typename T>
	[[cheri::interrupt_state(disabled)]] auto with_interrupts_disabled(T &&fn)
	{
		return fn();
	}

	/**
	 * The codes used in the cause field of the mtval CSR when the processor
	 * takes a CHERI exception.
	 */
	enum class CauseCode
	{
		/**
		 * No exception. This value is passed to the error handler after a
		 * forced unwind in a called compartment.
		 */
		None = CheriCauseCodeNone,
		/**
		 * Attempted to use a capability outside its bounds.
		 */
		BoundsViolation = CheriCauseCodeBoundsViolation,
		/**
		 * Attempted to use an untagged capability to authorize something.
		 */
		TagViolation = CheriCauseCodeTagViolation,
		/**
		 * Attempted to use a sealed capability to authorize something.
		 */
		SealViolation = CheriCauseCodeSealViolation,
		/**
		 * Attempted to jump to a capability without `Permission::Execute`.
		 */
		PermitExecuteViolation = CheriCauseCodePermitExecuteViolation,
		/**
		 * Attempted to load via a capability without `Permission::Load`.
		 */
		PermitLoadViolation = CheriCauseCodePermitLoadViolation,
		/**
		 * Attempted to store via a capability without `Permission::Store`.
		 */
		PermitStoreViolation = CheriCauseCodePermitStoreViolation,
		/**
		 * Attempted to store a tagged capability via a capability without
		 * `Permission::LoadStoreCapability`.
		 */
		PermitStoreCapabilityViolation =
		  CheriCauseCodePermitStoreCapabilityViolation,
		/**
		 * Attempted to store a tagged capability without `Permission::Global`
		 * via capability without `Permission::StoreLocal`.
		 */
		PermitStoreLocalCapabilityViolation =
		  CheriCauseCodePermitStoreLocalCapabilityViolation,
		/**
		 * Attempted to access a restricted CSR or SCR with PCC without
		 * `Permission::AccessSystemRegisters`.
		 */
		PermitAccessSystemRegistersViolation =
		  CheriCauseCodePermitAccessSystemRegistersViolation,
		/**
		 * Used to represent a value that has no valid meaning in hardware.
		 */
		Invalid = CheriCauseCodeInvalid,
	};

	/**
	 * Register numbers as reported in cap idx field of  `mtval` CSR when
	 * a CHERI exception is taken. Values less than 32 refer to general
	 * purpose registers and others to SCRs (of these, only PCC can actually
	 * cause an exception).
	 */
	enum class RegisterNumber
	{
		/**
		 * The zero register, which always contains the `NULL` capability.
		 */
		CZR = CheriRegisterNumberCzr,
		/**
		 * `$c1` / `$cra` used by the ABI as the return address.
		 * Not preserved across calls.
		 */
		CRA = CheriRegisterNumberCra,
		/**
		 * `$c2` / `$csp` used by the ABI as the stack pointer.
		 * Preserved across calls.
		 */
		CSP = CheriRegisterNumberCsp,
		/**
		 * `$c3` / `$cgp` used by the ABI as the global pointer.
		 * Not allocatable by the compiler, set by the switcher on compartment
		 * entry.
		 */
		CGP = CheriRegisterNumberCgp,
		/**
		 * `$c4` / `$ctp` used by the ABI as the thread pointer.
		 * Currently unused by the compiler.
		 * Not preserved across compartment calls.
		 */
		CTP = CheriRegisterNumberCtp,
		/**
		 * `$c5` / `$ct0` used by the ABI as temporary register.
		 * Not preserved across calls.
		 */
		CT0 = CheriRegisterNumberCT0,
		/**
		 * `$c6` / `$ct1` used by the ABI as temporary register.
		 * Not preserved across calls.
		 */
		CT1 = CheriRegisterNumberCT1,
		/**
		 * `$c7` / `$ct2` used by the ABI as temporary register.
		 * Not preserved across calls.
		 */
		CT2 = CheriRegisterNumberCT2,
		/**
		 * `$c8` / `$cs0` used by the ABI as a callee-saved register.
		 * Preserved across calls.
		 */
		CS0 = CheriRegisterNumberCS0,
		/**
		 * `$c9` / `$cs1` used by the ABI as a callee-saved register.
		 * Preserved across calls.
		 */
		CS1 = CheriRegisterNumberCS1,
		/**
		 * `$c10` / `$ca0` used by the ABI as an argument register.
		 * Not preserved across calls.
		 */
		CA0 = CheriRegisterNumberCA0,
		/**
		 * `$c11` / `$ca1` used by the ABI as an argument register.
		 * Not preserved across calls.
		 */
		CA1 = CheriRegisterNumberCA1,
		/**
		 * `$c12` / `$ca2` used by the ABI as an argument register.
		 * Not preserved across calls.
		 */
		CA2 = CheriRegisterNumberCA2,
		/**
		 * `$c13` / `$ca3` used by the ABI as an argument register.
		 * Not preserved across calls.
		 */
		CA3 = CheriRegisterNumberCA3,
		/**
		 * `$c14` / `$ca4` used by the ABI as an argument register.
		 * Not preserved across calls.
		 */
		CA4 = CheriRegisterNumberCA4,
		/**
		 * `$c15` / `$ca5` used by the ABI as an argument register.
		 * Not preserved across calls.
		 */
		CA5 = CheriRegisterNumberCA5,
		/**
		 * The Program Counter Capability.
		 *
		 * Special capability register used to authorize instruction fetch. The
		 * address is that of the faulting instruction. Also used for accessing
		 * read-only globals.
		 */
		PCC = CheriRegisterNumberPcc,
		/**
		 * Machine-mode Trap Code Capability.
		 *
		 * Special capability register that
		 * is installed in PCC when the CPU takes a trap. The address has the
		 * same semantics as the RISC-V `mtvec` CSR. Only accessible when PCC
		 * has the AccessSystemRegisters permission.
		 */
		MTCC = CheriRegisterNumberMtcc,
		/**
		 * Machine-mode Tusted Data Capability.
		 *
		 * Special capability register that contains the memory root capability
		 * on boot. Only accessible when PCC has the AccessSystemRegisters
		 * permission.  Use by the RTOS to store a capability to the trusted
		 * stack.
		 */
		MTDC = CheriRegisterNumberMtdc,
		/**
		 * Machine-mode Scratch Capability. Special capabiltiy register that
		 * contains the sealing root capability on boot. Only accessible when
		 * PCC has the AccessSystemRegisters permission.
		 */
		MScratchC = CheriRegisterNumberMScratchC,
		/**
		 * Machine-mode Exception Program Counter Capability. Special capability
		 * register that contains the PCC of the faulting instruction on trap.
		 * The address has the same semantics as the RISC-V `mepc` CSR. Only
		 * accessible when PCC has the AccessSystemRegisters permission.
		 */
		MEPCC = CheriRegisterNumberMepcc,
		/**
		 * Indicates a value that is not used by the hardware to refer to a
		 * register.
		 */
		Invalid = CheriRegisterNumberInvalid,
	};

	/**
	 * Decompose the value reported in the `mtval` CSR on CHERI exception
	 * into a pair of `CauseCode` and `RegisterNumber`.
	 *
	 * Will return `CauseCode::Invalid` if the code field is not one
	 * of the defined causes and `RegisterNumber::Invalid` if the register
	 * number is not a valid register number. Other bits of mtval are ignored.
	 */
	inline std::pair<CauseCode, RegisterNumber>
	extract_cheri_mtval(uint32_t mtval)
	{
		auto causeCode = magic_enum::enum_cast<CauseCode>(mtval & 0x1f)
		                   .value_or(CauseCode::Invalid);
		auto registerNumber =
		  magic_enum::enum_cast<RegisterNumber>((mtval >> 5) & 0x3f)
		    .value_or(RegisterNumber::Invalid);
		return {causeCode, registerNumber};
	}
} // namespace CHERI
