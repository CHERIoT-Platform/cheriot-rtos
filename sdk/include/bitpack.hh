#pragma once

/**
 * \defgroup bitpacks Bitpacks
 *
 * \section bitpacks_intro Introduction
 *
 * `Bitpack`-s are a way to handle situations where many flags and/or fields of
 * arbitrary widths are packed into an underlying integer (such as a `uint32_t`
 * and referred to as a `Bitpack`'s `Storage` type) at arbitrary bit alignments.
 * This is similar to C and C++'s bitfields, but more portable, more versatile,
 * and with a type-centric interface.  They encapsulate the bitwise AND, OR, and
 * shift operations required to get at spans of bits within a word.  They are
 * meant to be particularly useful for handling memory mapped I/O (MMIO)
 * registers for hardware drivers.
 *
 * Those readers interested in formal details are invited to see \ref
 * bitpacks_formal.
 *
 * `Bitpack`-s support the following features:
 *
 * * Arbitrary width numeric fields at arbitrary bit position within an
 *   underlying integer type.
 *
 * * Arbitrary width enumeration fields, again at arbitrary position.
 *
 * * `const` fields to represent, for example, fields mutable only by hardware.
 *
 * * `volatile` `Bitpacks` require overt reads and writes, making it easier to
 *   map program source to memory or register updates.  Individual methods'
 *   documentation will call out special handling of `volatile` forms.
 *
 * * a type-centric perspective on fields, with type-based accessors (getters,
 *   setters, read-modify-write helpers, &c).
 *
 * * usable in `constexpr` contexts.
 *
 * \subsection bitpacks_intro_defining Defining Bitpacks and Fields
 *
 * To define a bitpack, begin by defining a `class` (or `struct`) that inherits
 * from the `Bitpack` template given below, specifying the underlying `Storage`
 * type as the template parameter.  It usually suffices to pick one of the
 * `uintNN_t` `cstdint` types.  For reasons of C++, one often wants a bit of
 * syntax at the top of these definitions, which is encapsulated in the
 * `BITPACK_USUAL_PREFIX` macro; add that at the top of your derived class.  We
 * then need to specify fields of the bitpack in question.
 *
 * For each field, this means, ultimately, defining a call to the
 * `Bitpack::member` method that users of the bitpack can call to access that
 * field.  While it is perfectly sensible to do so directly, this file also
 * offers a large number of utilities and affordances which capture common
 * occurrences.  We will need to specify the type of the field as seen from C++,
 * though in many cases one will simultaneously define the field and its type.
 * Further, one will need to specify the `FieldInfo` for the field: this is a
 * structure holding, in order, the minimum and maximum (both inclusive) bit
 * positions occupied by the field in the underlying word, and a (default false)
 * flag to indicate that the field is constant; many of our macros will simply
 * let us write the values of these properties in order as arguments.
 *
 * At the lowest level of assistance, the `BITPACK_MEMBER_ADD` macro
 * encapsulates the C++ syntax for a method wrapping a call to `member`.  It
 * takes the name of the method to define, the type of the field, and the values
 * to use for the `FieldInfo`.
 *
 * \remark
 * \parblock
 *
 * For example, `BITPACK_MEMBER_ADD(ticks, uint8_t, 3, 9);` defines a method
 * `ticks()` that gives `uint8_t`-flavored access to a field spanning bits 3 to
 * 9 of the bitpack's underlying word.  Callers of this method get a `Proxy` of
 * the bitpack that can be used to get, set, or alter (read-modify-write) that
 * field and will see the field's value as a `uint8_t`.
 *
 * \endparblock
 *
 * Most of the time, though, just as defining a bitpack creates a new C++ type,
 * one can think of a field within a bitpack as having a unique type within its
 * containing bitpack.  As such, most of our definition utility macros serve to
 * define a field while simultaneously defining a type which is both used as the
 * type of values of the field and as a name for the field.  Behind the scenes,
 * there is some C++ machinery to provide a `Bitpack::member<FieldType>`
 * template method that can look up a field's `FieldInfo` from its type; our
 * macros encapsulate that machinery.
 *
 * Often, fields take on enumerated values, for which C++ `enum class` types are
 * usually reasonably convenient representations.  The
 * `BITPACK_MEMBER_ADD_ENUM` macro encapsulates the syntax for defining an
 * `enum class` (with given underlying type) and an associated `FieldInfo`.
 *
 * \remark
 * \parblock
 *
 * For example, this block of code...
 *
 *     BITPACK_MEMBER_ADD_ENUM(Drivers, uint8_t, 12, 13)
 *     {
 *       None     = 0,
 *       UpOnly   = 1,
 *       DownOnly = 2,
 *       Both     = 3,
 *     };
 *
 * defines an `enum class Drivers` within the bitpack containing it (multiple
 * bitpack definitions may each have their own `Drivers` type within, but each
 * such may have only one).  This enumeration has an underlying type of
 * `uint8_t` and four defined values: `Drivers::None`, `Drivers::UpOnly`, &c.
 * The numeric values of the enumerators are used as values of the field within
 * the bitpack.  At the same time, the macro will have defined a `FieldInfo`
 * calling for a non-constant, two-bit field at positions 12 and 13. and
 * associated this with the `Drivers` type, so that a call to
 * `member<Drivers>()` within the bitpack will return a `Proxy` for that field
 * which works with `Drivers`-typed values.
 *
 * \endparblock
 *
 * It is often useful to have single-bit fields with custom names for the
 * 0/`false` and 1/`true` values.  This is provided by the
 * `BITPACK_MEMBER_ADD_ENUM_BOOL` macro; some popular options for names are
 * provided as wrappers thereof.
 *
 * Sometimes fields are numeric rather than enumerative, such as clock dividers
 * or event counters.  `Bitpack`-s have a `Numeric<Base>` template class to
 * facilitate creating bespoke C++ types for such fields, and the
 * `BITPACK_MEMBER_ADD_NUMERIC` macro encapsulates the syntactic clutter of its
 * use.
 *
 * \remark
 * \parblock
 *
 * For example, `BITPACK_MEMBER_ADD_NUMERIC(Ticks, uint16_t, 14, 25, true);`
 * defines a type `Ticks`, which wraps a `uint16_t`, within the bitpack
 * containing it.  Like with `BITPACK_MEMBER_ADD_ENUM`, this also defines a
 * `FieldInfo` and associates it with the `Ticks` type, so that
 * `member<Ticks>()` within the bitpack will return a proxy of this field.  The
 * `true` at the end sets the `FieldInfo` `isConst` flag and so will inhibit
 * attempts to use the `member()` field proxy to change its value.
 *
 * \endparblock
 *
 * \subsection bitpacks_intro_proxies Using Bitpacks
 *
 * Having defined a bitpack and its fields and internal machinery, one can now
 * make use of it!  Most often, bitpacks are used directly by external code;
 * that is, the proxies of its fields are public rather than being used by
 * methods of the bitpack class itself (though, of course, that also works).
 * As such, C++'s somewhat inflexible syntax rears its head on occasion, and
 * bitpacks offer macros in an attempt to compensate.
 *
 * As a consequence of defining types (of fields) within other types (their
 * contianing bitpack class), the former are in scope outside the latter only
 * when qualified with the name of the latter.  Having to always write out the
 * full `MyBitpackClass::TheFieldType` would be exhausting and distracting.
 * Conveniently, with a bitpack value in hand, `decltype` gives us a generic way
 * to refer to its type and can be used to qualify the names of contained types.
 * The `BITPACK_MEMBER_DECLTYPE(b, T)` macro gets the `Proxy` of the `T`-typed
 * field within the bitpack value `b` without needing to spell out the latter's
 * type.  If the type of `b` is dependent (for example, is `auto` or is or
 * involves a template argument), use `BITPACK_MEMBER_DEPENDENT(b, T)` instead.
 * See \ref bitpacks_macros_member .
 *
 * `Bitpack::Field::Proxy`-s offer six core methods:

 * 1. Projection ("getter") of the field's current value within the containing
 *    bitpack.  Proxies have implicit conversions to their field's type, so in
 *    many cases, projection is syntactiaclly free,  When it must be made
 *    explicit, projection is available as the `raw()` method on the proxy.
 *    Proxies of field types with underlying types (specifically, `enum` and
 *    `enum class` types and types derived from the `Bitpack::Numeric` template
 *    class) also have a `rawer()` method that will return the field's value as
 *    this underlying type.

 * 2. Assignment ("setter"), in the form of an overloaded `=` operator.  This
 *    mutates the underlying `Storage` value such that the field being proxied
 *    now has the given value.  All other bits in the bitpack are unaltered.
 *
 * 3. Modification ("read modify write", "RMW"), in the form of the `alter()`
 *    method.  `alter()` takes a callback, which should take one argument, whose
 *    type is the field's, and return another value of the same type.  The
 *    callback will be given the field's current value in the containing bitpack
 *    and the containing bitpack will be updated so that the field's value is as
 *    returned from the callback.
 *
 * 4. Cloning with override, in the form of the `with()` methods.  These return
 *    a copy of the bitpack with the proxied field changed as directed.
 *    `with()` can be given the new field value directly, or it may be given a
 *    callback that takes the field's current value and returns the desired new
 *    value.
 *
 * 5. Comparison.  For convenience, `Proxy`-s overload the spaceship operator,
 *    `<=>`, and so implicitly also provide `<`, `<=`, `==`, `!=`, `>=`, and `>`
 *    between pairs of `Proxy`-s of the same field or between a `Proxy` and a
 *    value of its field's type.
 *
 * 6. Assignment from zero, in the form of the `assign_from()` method.
 *    `assign_from` takes a callback which should return a value of the field's
 *    type when given the zero value of that type.  This is sort of like
 *    `alter()`, except that it does not extract the field's current value
 *    first.  Mostly, this is useful for polymorpic type shenanigans.
 *
 * Atop this core, there are many convenience macros for working with bitpacks
 * and field proxies.
 *
 * Because `Bitpack`-s encourage the use of types and named values defined
 * within derived classes, code using `Bitpack`-s often needs to use qualified
 * names or wrap values in type constructors (in addition to qualifying the
 * field type with the bitpack's type when constructing the field proxy as in
 the
 * `BITPACK_MEMBER_` macros above).  The first two families of macros atop
 * proxies help with these cases.
 *
 * * The family of \ref bitpacks_macros_proxyop_qualify is built around the
 *   `BITPACK_OPERATE_QUALIFY(proxy, operator, value)` macro, which qualifies
 *   `value` with the field type of the proxy `proxy` and relates the proxy and
 *   qualified value with `operator`, which is often `=` for assignment or `==`
 *   for equality testing.  For example,
 *   `BITPACK_OPERATE_QUALIFY(p, =, UpOnly)`, with `p` a `Proxy` of a
 *   `Drivers`-typed field (recall above), would assign the field to `UpOnly`
 *   without needing to use a fully qualified form like
 *   `MyBitpack::Drivers::UpOnly`.
 *
 * * The family of \ref bitpacks_macros_proxyop_construct is built around the
 *   `BITPACK_OPERATE_WRAP(proxy, operator, value)` macro.  Here, the `value` is
 *   passed to a single-argument constructor of the proxy's field type.  This
 *   works well for `Numeric` types: `BITPACK_OPERATE_WRAP(p, ==, 7)` will
 *   compare the value of the field being proxied by `p` with the value `7`
 *   converted to that field's type (by said type's constructor).
 *
 * Each of these families have further wrappers around their center macros:
 *
 * * `BITPACK_OPERATE_{QUALIFY,WRAP}_{DECLTYPE,DEPENDENT}(b, T, operator, v)`
 *   fuse `BITPACK_OPERATE_{QUALIFY,WRAP}` and
 *   `BITPACK_MEMBER_{DECLTYPE,DEPENDENT}`, building the `Proxy`
 *   of the `T`-typed field in the bitpack value `b`, rather than requiring it
 *   to be explicitly built first.
 *
 * * For the case where the type `T` does not need qualification when finding
 *   its associated field in `b`, use the
 *   `BITPACK_OPERATE_{QUALIFY,WRAP}_TYPE(b, T, operator, v)` macro (which can
 *   use `b.member<T>()`, rather than a `BITPACK_MEMBER_` macro, internally).
 *
 * * `BITPACK_WITH_{QUALIFY_WRAP}(proxy, value)` and
 *   `BITPACK_WITH_{QUALIFY,WRAP}_{DECLTYPE,DEPENDENT,TYPE}(b, T, v)` are
 *   specializations of their `BITPACK_OPERATE_...` siblings with `.with` as the
 *   operator, as this is a common enough occurrence.
 *
 * The family of \ref bitpacks_macros_proxyop_directed is somewhat dual: rather
 * than using the field type to in some way influence the meaning of a `value`,
 * this family uses the type of the value to find the appropriate field and a
 * proxy thereof within a bitpack value.  This can be convenient when a field
 * value is already available.  The centerpiece of this family is the
 * `BITPACK_OPERATE_VALUE(b, operator, value)` macro, which relates with
 * `operator` a proxy of the field in the bitpack value `b` whose type is that
 * of `value` and `value` itself.  The
 * `BITPACK_OPERATE_{DECLTYPE,DEPENDENT}(b, operator, value)` wrappers qualify
 * `value` with the type of the bitpack value `b`.  As above, there are also
 * `_WITH` specializations of the `_OPERATE` forms.
 *
 * There are other macros likely of more niche interest; see...
 *
 * * \ref bitpacks_macros_proxyop_enum and
 * * \ref bitpacks_macros_vararg .
 *
 * \section bitpacks_formal A More Formal Perspective
 *
 * More formally, `Bitpack`-s are a kind of aggregate structure within a numeric
 * word and offer lens-like, typed access to contiguous spans of bits therein,
 * offering a view of bits as a product of typed fields, occuping a disjoint,
 * contiguous span of bits within that word.
 *
 * The static information about a field is represented by a specialization of
 * the `Bitpack::Field<Type, Info>` template.  `Type` is the exposed C++ type of
 * the field in question, and `Info` is a (necessarily constexpr) value of the
 * `FieldInfo` type, which holds the bit indicies of the field's span within the
 * `Storage` type.  `FieldInfo` also has an `isConst` flag, used to specify that
 * the field is not mutable from software even if the larger bitpack is
 * non-constant.
 *
 * Our lenses, the centerpiece of the whole thing, are expressed as "proxy"
 * objects, instances of specializations of the [Bitpack::Field<Type,
 * Info>::Proxy<DerivedBitpack, RefType>](#Bitpack::Field::Proxy) class, which
 * wrap references to the underlying `Storage`-typed word in a `Bitpack` class
 * (or a derived class).  The type of such references, `RefType`, inherits any
 * `const` and/or `volatile` qualification of the user's handle to the bitpack,
 * and are additionally `const` qualified if the associated `FieldInfo` has
 * `isConst` asserted.
 *
 * @{
 */

#include <cdefs.h>
#include <concepts>
#include <limits.h>
#include <stddef.h>
#include <type_traits>

/*
 * Mark the Bitpack class declaration with cdef.h's `CHERIOT_EXPERIMENTAL()` --
 * that is, as eliciting a warning if CHERIOT_EXPERIMENTAL_APIS_WARN is defined
 * -- unless CHERIOT_EXPERIMENTAL_NOWARN_BITPACKS is defined.
 */
#if defined(CHERIOT_EXPERIMENTAL) &&                                           \
  !defined(CHERIOT_EXPERIMENTAL_NOWARN_BITPACKS)
#	define BITPACK_DECL_ANNOTATION                                            \
		CHERIOT_EXPERIMENTAL(                                                  \
		  "Bitpacks are an experimental CHERIoT RTOS feature")
#else
#	define BITPACK_DECL_ANNOTATION
#endif

/**
 * The `Bitpack` structure itself, templated on the underlying Storage type.
 *
 * This should be the sole (non-empty) superclass of a type representing a
 * bitfield-esque composite structure.
 */
template<typename StorageParam>
    requires std::is_unsigned_v<StorageParam>
class BITPACK_DECL_ANNOTATION Bitpack
{
	public:
	/// Expose the underlying storage type
	using Storage = StorageParam;

	private:
	Storage value;

	public:
	/// Construct a `Bitpack` value with an underlying Storage of all zero bits
	constexpr Bitpack() : value(0) {}

	/**
	 * Construct a `Bitpack` value from a value of its underlying Storage type.
	 *
	 * This is marked `explicit` to make things like "device->register = 0x7;"
	 * slightly harder to spell, with the intent of encouraging use of named
	 * constants.
	 */
	constexpr explicit Bitpack(Storage v) : value(v) {}

	/**
	 * Assign a whole `Bitpack` at once given a non-volatile `Bitpack` of the
	 * same (or convertible, including derived) type.
	 *
	 * To assign from a `volatile` `Bitpack`, perform an explicit `.read()`
	 * first.
	 *
	 * "Deducing this" gives us CRTP-esque behavior without, well, the CRTP, so
	 * this will not accept an unrelated derived `Bitpack` class.  (But will
	 * accept types that can be converted, so assigning to a `Bitpack`, as
	 * opposed to a derived class, will accept any derived class, for example.
	 * Probably best avoid that.)
	 */
	template<typename Self>
	// NOLINTNEXTLINE(misc-unconventional-assign-operator)
	constexpr Self &&operator=(this Self                      &&self,
	                           const std::remove_cvref_t<Self> &b)
	{
		self.value = b.value;
		return self;
	}

	/**
	 * Compare `Bitpack`-s for equality (reflecting that of `Storage`).
	 *
	 * Neither side may be volatile; use explicit `.read()`-s first.
	 */
	template<typename Self>
	    requires(!std::is_volatile_v<Self>)
	constexpr bool operator==(this Self                      &&self,
	                          const std::remove_cvref_t<Self> &b)
	{
		return self.value == b.value;
	}

	/**
	 * Spaceship operator on `Bitpack`-s (reflecing that of `Storage`).
	 *
	 * Neither side may be volatile; use explicit `.read()`-s first.
	 */
	template<typename Self>
	    requires(!std::is_volatile_v<Self>)
	constexpr auto operator<=>(this Self                      &&self,
	                           const std::remove_cvref_t<Self> &b)
	{
		return self.value <=> b.value;
	}

	/**
	 * Extract the underlying `Storage` value.
	 *
	 * The `Bitpack` must not be `volatile`.  Use `.read()` first.
	 */
	constexpr explicit operator Storage() const
	{
		return this->value;
	}

	/**
	 * A shorter way of spelling `static_cast<Storage>(...)`.
	 *
	 * The `Bitpack` must not be `volatile`.  Use `.read()` first.
	 */
	[[nodiscard]] constexpr Storage raw() const
	{
		return static_cast<Storage>(*this);
	}

	/**
	 * Return a snapshot of the underlying `Storage`.
	 *
	 * Notably, this can be used to get a `const` `Bitpack` from a `volatile`
	 * `Bitpack` (`const` or not).
	 */
	template<typename Self>
	const auto read(this Self &&self)
	{
		return std::remove_cvref_t<Self>{self.value};
	}

	/// Convenience wrapper for the types of numeric fields.
	template<typename T>
	    requires std::is_unsigned_v<T>
	struct Numeric
	{
		/// Export the underlying numeric type
		using NumericType = T;

		/*
		 * The field type must be smaller than the `Bitpack`'s underlying
		 * `Storage`.
		 */
		static_assert(sizeof(T) <= sizeof(Storage));

		T value;

		constexpr Numeric(T v) : value(v) {}

		constexpr operator T() const
		{
			return this->value;
		}

		[[nodiscard]] constexpr T raw() const
		{
			return static_cast<T>(this->value);
		}
	};

	/**
	 * Information about a field within a `Bitpack`.
	 *
	 * Fields are contiguous spans of bits and so have a lowest and highest bit
	 * position within the underlying Storage word.
	 *
	 * Fields may be marked as constant to dissuade software from attempting to
	 * change their value.  Do note, though, that any store of an underlying
	 * Storage word will also, necessarily, include the bits for constant
	 * Fields.  It is likely generally more advisable to not mix constant and
	 * non-constant fields within the same Storage word or `Bitpack`.
	 */
	struct FieldInfo
	{
		/// Minimum 0-indexed bit position occupied by this field
		size_t minIndex;
		/// Maximum 0-indexed bit position occupied by this field
		size_t maxIndex;
		/**
		 * Should this field be proxied as constant (and so mutation be slightly
		 * less ergonomic)?
		 */
		bool isConst = false;

		bool operator==(const FieldInfo &) const = default;
	};

	/**
	 * A particular Field within a `Bitpack`.
	 *
	 * Fields are templated on the type as seen by external code and the
	 * FieldInfo data giving the field's location and other properties.
	 */
	template<typename TypeParam, FieldInfo InfoParam>
	struct Field
	{
		/// Expose the type parameter
		using Type = TypeParam;
		/// Expose the FieldInfo parameter
		static constexpr FieldInfo Info = InfoParam;

		// The field's span of bits must start before it ends
		static_assert(Info.maxIndex >= Info.minIndex,
		              "Field span ends before it begins");

		static_assert(!std::is_base_of_v<Numeric<bool>, Type> ||
		                (Info.maxIndex == Info.minIndex),
		              "Numeric<bool> fields should be exactly one bit wide");

		static_assert(
		  !std::is_enum_v<Type> ||
		    !requires {
			    { std::underlying_type_t<Type>() } -> std::same_as<bool>;
		    } || (Info.maxIndex == Info.minIndex),
		  "Enum fields with underlying type bool should be exactly "
		  "one bit wide");

		static_assert((Info.maxIndex - Info.minIndex + 1) <=
		                CHAR_BIT * sizeof(Type),
		              "Field type is narrower than specified field bit width");

		static_assert((Info.maxIndex - Info.minIndex + 1) <
		                CHAR_BIT * sizeof(Storage),
		              "Field width is not smaller than Bitpack's Storage type");

		/// A span of set bits, starting at index 0, and of the field's width.
		static constexpr Storage ValueMask =
		  (1U << (Info.maxIndex - Info.minIndex + 1)) - 1;

		/// The mask of bits occupied by this value (occupied bits are set).
		static constexpr Storage FieldMask = ValueMask << Info.minIndex;

		/// Extract a Field from a raw Storage value
		constexpr static Type raw_view(Storage storage)
		{
			return static_cast<Type>((storage >> Info.minIndex) & ValueMask);
		}

		/// Compute the underlying Storage transform for a Field update
		constexpr static Storage raw_with(Storage lhs, Storage rhs)
		{
			return (lhs & ~FieldMask) | ((rhs & ValueMask) << Info.minIndex);
		}

		/// Update for fields whose type can be static_cast to Storage
		constexpr static Storage raw_with(Storage lhs, Type rhs)
		    requires requires { static_cast<Storage>(std::declval<Type>()); }
		{
			return raw_with(lhs, static_cast<Storage>(rhs));
		}

		/// Update for fields that are themselves `Bitpacks` at smaller types
		template<typename OtherStorage>
		    requires requires { sizeof(OtherStorage) < sizeof(Storage); }
		constexpr static Storage raw_with(Storage               lhs,
		                                  Bitpack<OtherStorage> rhs)
		{
			return raw_with(lhs, static_cast<OtherStorage>(rhs));
		}

		/**
		 * A proxy for this Field within the `Bitpack`'s `Storage`.
		 *
		 * This provides getters, setters, and mutators phrased in terms of the
		 * `Field`'s exposed `Type` rather than the raw bits within the
		 * `Bitpack`'s underlying `Storage` word.
		 *
		 * Proxies are templated on their containing derived type (so that they
		 * can act "in situ" in the class hierarchy, consuming and returning the
		 * same type, which must have `Bitpack` as its base type) and the type
		 * of the reference they hold to their containing `Bitpack`'s underlying
		 * `Storage` word (so that they can properly propagate `volatile` and
		 * `const` qualifications from the `Bitpack` value and the `Field`'s
		 * properties to perceived type).
		 */
		template<typename DerivedBitpack, typename RefTypeParam>
		    requires std::is_base_of_v<Bitpack, DerivedBitpack> &&
		             std::is_lvalue_reference_v<RefTypeParam> &&
		             std::is_same_v<Storage, std::remove_cvref_t<RefTypeParam>>
		class Proxy
		{
			/// A qualified reference to the containing `Bitpack`'s `Storage`.
			RefTypeParam ref;

			public:
			/// Name the Field of which we are a proxy
			using Field = Field;

			/// Expose the qualified reference type we hold to the Storage
			using RefType = RefTypeParam;

			/**
			 * Construct a proxy given a reference to the storage word.
			 *
			 * May take any reference type implicitly convertible to RefType,
			 * not just RefType itself.
			 */
			template<typename R>
			constexpr Proxy(R &r) : ref(r.value)
			{
			}

			/**
			 * Compute and store an updated `Bitpack` value with a new value for
			 * this field (of the field type itself).
			 *
			 * For `volatile` `Bitpack`s, this will perform a load and store.
			 */
			template<typename Self>
			    requires(
			      !std::is_const_v<std::remove_reference_t<RefTypeParam>>)
			// NOLINTNEXTLINE(misc-unconventional-assign-operator)
			constexpr Self &&operator=(this Self &&self, Field::Type rhs)
			{
				self.ref = raw_with(self.ref, rhs);
				return self;
			}

			/**
			 * Compute and store an updated `Bitpack` value with a new value for
			 * this field, when this field's type is a Numeric wrapper, and the
			 * RHS is the underlying type inside the Numeric wrapper.
			 *
			 * For `volatile` `Bitpack`s, this will perform a load and store.
			 */
			template<typename Self, typename RHS>
			    requires(
			      !std::is_const_v<std::remove_reference_t<RefTypeParam>> &&
			      std::is_same_v<Field::Type, Numeric<RHS>>)
			// NOLINTNEXTLINE(misc-unconventional-assign-operator)
			constexpr Self &&operator=(this Self &&self, RHS rhs)
			{
				self.ref = raw_with(self.ref, Field::Type{rhs});
				return self;
			}

			/**
			 * Compute and store an updated `Bitpack` value with a new value for
			 * this field, when this field's type is an enum class and the
			 * RHS is the underlying type of that enum.
			 *
			 * For `volatile` `Bitpack`s, this will perform a load and store.
			 */
			template<typename Self, typename RHS>
			    requires(
			      !std::is_const_v<std::remove_reference_t<RefTypeParam>> &&
			      std::is_scoped_enum_v<Field::Type> &&
			      std::is_same_v<std::underlying_type_t<Field::Type>, RHS>)
			// NOLINTNEXTLINE(misc-unconventional-assign-operator)
			constexpr Self &&operator=(this Self &&self, RHS rhs)
			{
				self.ref = raw_with(self.ref, Field::Type{rhs});
				return self;
			}

			/**
			 * Explicit conversion of a Field::Proxy to the field value.
			 *
			 * The Proxy must not have a `volatile` reference to Storage.
			 * Use `.read()` on the containing `Bitpack`, first.
			 */
			template<typename Self>
			    requires(
			      !std::is_volatile_v<std::remove_reference_t<RefTypeParam>>)
			constexpr operator Field::Type(this Self &&self)
			{
				return raw_view(self.ref);
			}

			/// A shorter way of spelling static_cast<Field::Type>()...
			[[nodiscard]] constexpr Field::Type raw() const
			{
				return static_cast<Field::Type>(*this);
			}

			/// Get the value of this enum-typed field as its underlying type.
			[[nodiscard]] constexpr auto rawer() const
			    requires(std::is_enum_v<typename Field::Type>)
			{
				return std::to_underlying(this->raw());
			}

			/**
			 * Get the value of this Numeric-typed field as its underlying type.
			 */
			[[nodiscard]] constexpr auto rawer() const
			    requires(std::is_base_of_v<
			             Numeric<typename std::remove_cvref_t<
			               typename Field::Type>::NumericType>,
			             typename std::remove_cvref_t<typename Field::Type>>)
			{
				return this->raw().raw();
			}

			/**
			 * Construct a new `Bitpack` value with an updated value of this
			 * field.
			 *
			 * This is emphatically not `volatile`-qualified, and has no
			 * `volatile`-qualified overload.  `.read()` the containing
			 * `Bitpack`, first.
			 */
			[[nodiscard]] constexpr DerivedBitpack with(Field::Type rhs) const
			{
				return DerivedBitpack{raw_with(this->ref, rhs)};
			}

			/**
			 * Construct a new `Bitpack` value with an updated value of this
			 * field as a function of its current value.
			 *
			 * This is emphatically not `volatile`-qualified, and has no
			 * `volatile`-qualified overload.  `.read()` the containing
			 * `Bitpack`, first.
			 */
			constexpr DerivedBitpack with(auto &&f) const
			    requires std::
			      is_invocable_r_v<Field::Type, decltype(f), Field::Type>
			{
				/*
				 * Perform one read and use it twice to avoid double-tapping
				 * volatile storage!
				 */
				Storage storage = this->ref;
				return DerivedBitpack{raw_with(storage, f(raw_view(storage)))};
			}

			/**
			 * Compute and store an updated `Bitpack` value with a new value for
			 * this field as a function of its current value.
			 *
			 * For `volatile` `Bitpack`s, this will perform a load and store.
			 */
			template<typename Self>
			constexpr void alter(this Self &&self, auto &&f)
			    requires(
			      std::
			        is_invocable_r_v<Field::Type, decltype(f), Field::Type> &&
			      !std::is_const_v<std::remove_reference_t<RefTypeParam>>)
			{
				Storage storage = self.ref;
				self.ref        = raw_with(storage, f(raw_view(storage)));
			}

			/**
			 * Convenience function for unconditionally assigning a field when
			 * it helps to have a zero value of that field's type.
			 *
			 * Use `.alter()` if the current value of the field is required.
			 *
			 * Because this requires a read of the whole `Bitpack` to update,
			 * we refuse to operate on `volatile` values, and because it's an
			 * assignment, we don't operate on `const` values either.
			 */
			constexpr void assign_from(auto &&f)
			    requires(
			      std::is_invocable_r_v<Field::Type, decltype(f), Field::Type>)
			{
				this->ref = raw_with(this->ref, f(raw_view(0)));
			}

			constexpr auto operator<=>(Proxy rhs) const
			    requires(requires(Field::Type v) {
				    { v <=> v };
			    })
			{
				return raw() <=> rhs.raw();
			}

			constexpr auto operator<=>(Field::Type rhs) const
			    requires(requires(Field::Type v) {
				    { v <=> v };
			    })
			{
				return raw() <=> rhs;
			}

			constexpr bool operator==(Proxy rhs) const
			    requires(requires(Field::Type v) {
				    { v == v } -> std::same_as<bool>;
			    })
			{
				return raw() == rhs.raw();
			}

			constexpr bool operator==(Field::Type rhs) const
			    requires(requires(Field::Type v) {
				    { v == v } -> std::same_as<bool>;
			    })
			{
				return raw() == rhs;
			}
		};

		template<bool C, typename T>
		    requires std::is_lvalue_reference_v<T>
		using ConditionalConstRef = std::
		  conditional_t<C, std::add_const_t<std::remove_reference_t<T>> &, T>;

		/*
		 * Guide template deduction to conclude that a Field has a RefType
		 * that is as CV-qualified as the `Bitpack` of which it is a part, with
		 * the further possibility of const-qualification for fields annotated
		 * as constant.
		 *
		 * Yes, the `decltype(())` is deliberate: we want the lvalue expression
		 * rather than the prvalue expression.
		 */
		template<typename BitpackType>
		Proxy(BitpackType &r)
		  -> Proxy<std::remove_cvref_t<BitpackType>,
		           ConditionalConstRef<Info.isConst, decltype((r.value))>>;
	};

	protected:
	/**
	 * Build a Field::Proxy of an explicitly given type and info.
	 *
	 * This is protected so that it is available to subclasses but not more
	 * generally.
	 */
	template<typename FieldType, FieldInfo Info, typename Self>
	constexpr auto member(this Self &&self)
	{
		return typename Field<FieldType, Info>::Proxy(self);
	}

	public:
	/**
	 * Build a Field::Proxy proxy by asking the derived Self class for a
	 * FieldInfo structure computed from template expansion of
	 * Self::field_info_for_type<FieldType>().
	 */
	template<typename FieldType, typename Self>
	    requires std::is_convertible_v<
	      decltype(std::remove_cvref_t<Self>::template field_info_for_type<
	               FieldType>()),
	      const FieldInfo>
	constexpr auto member(this Self &&self)
	{
		/*
		 * Find the information for the field type; "deducing this" gives us
		 * access to the derived class's type without need for the CRTP.
		 */
		static constexpr FieldInfo Info =
		  std::remove_cvref_t<Self>::template field_info_for_type<FieldType>();

		return self.template member<FieldType, Info>();
	}

	/**
	 * Fetch the value of a field in this `Bitpack` based on the field type.
	 *
	 * For `volatile` `Bitpack`s, this will perform a load.
	 */
	template<typename FieldType, typename Self>
	constexpr FieldType get(this Self &&self)
	{
		return self.template member<FieldType>();
	}

	/**
	 * Compute a new `Bitpack` value with an updated field of a given type.
	 *
	 * For `volatile` `Bitpack`s, this will perform a load.
	 */
	template<typename FieldType, typename Self>
	constexpr std::remove_cvref_t<Self> with(this Self &&self, FieldType v)
	{
		return self.template member<FieldType>().with(v);
	}

	/**
	 * Compute a new `Bitpack` value with an updated field of a given type.
	 *
	 * For `volatile` `Bitpack`s, this will perform a load and store.
	 */
	template<typename FieldType, typename Self>
	constexpr void set(this Self &&self, FieldType v)
	{
		self.template member<FieldType>() = v;
	}

	/**
	 * Convenience function for unconditionally changing several sub-fields at
	 * once.  Intended particularly for use with `volatile` `Bitpack`-s, to help
	 * ensure only one read and one write takes place, as the callback operates
	 * on a non-`volatile` `Bitpack`.
	 */
	template<typename Self>
	constexpr void alter(this Self &&self, auto &&f)
	    requires std::is_invocable_r_v<std::remove_cvref_t<Self>,
	                                   decltype(f),
	                                   std::remove_cvref_t<Self>>
	{
		std::remove_cvref_t<Self> value{self.value};
		self = f(value);
	}

	/**
	 * Convenience function for unconditionally assigning an entire `Bitpack`
	 * from a computed value.  The function receives a zero-valued instance of
	 * the `Bitpack` type.
	 */
	template<typename Self>
	constexpr void assign_from(this Self &&self, auto &&f)
	    requires std::is_invocable_r_v<std::remove_cvref_t<Self>,
	                                   decltype(f),
	                                   std::remove_cvref_t<Self>>
	{
		self = f(std::remove_cvref_t<Self>(0));
	}
};

/**
 * It is occasionally useful to derive one bitpack from another.  Using
 * BitpackDerived<B> as the sole (non-empty) base class of an aggregate type
 * reduces the syntactic clutter of deriving from the bitpack B.
 *
 * See `BITPACK_DERIVED_PREFIX` and `BITPACK_DERIVED_FIELD_INFO_FOR_TYPE`.
 */
template<typename B>
    requires std::derived_from<B, Bitpack<typename B::Storage>>
struct BitpackDerived : B
{
	using B::B;
	using B::operator=;

	protected:
	using ParentBitpack = B;
};

/**
 * \defgroup bitpacks_macros Convenience macros
 * @{
 */

/**
 * \defgroup bitpacks_macros_defn Field definition macros
 * @{
 */

/**
 * Capture the incantations often at the top of a `Bitpack` structure,
 * especially one with type-directed field proxies.
 */
#define BITPACK_USUAL_PREFIX                                                   \
	using Bitpack::Bitpack;                                                    \
	using Bitpack::operator=;                                                  \
	template<typename FieldType>                                               \
	static constexpr FieldInfo field_info_for_type() = delete;

/**
 * Define a named accessor for a field.
 *
 * Takes the name of the accessor, the type to use for the proxy, and the fields
 * of the FieldInfo (as varargs).  If the latter is omitted, this defines an
 * alias for a field whose type is sufficient to resolve the FieldInfo through
 * field_info_for_type.
 */
#define BITPACK_MEMBER_ADD(name, Type, ...)                                    \
	template<typename Self>                                                    \
	constexpr auto name(this Self &&self)                                      \
	{                                                                          \
		return self.template member<Type __VA_OPT__(, {__VA_ARGS__})>();       \
	}

/**
 * Encapsulate the gyrations required to define an `enum class`-typed field and
 * its associated `field_info_for_type` within a `Bitpack`.  Takes the name of
 * the `enum` class to define, its underlying type, and then the fields of a
 * `FieldInfo` as varargs; the enumerators should follow the macro invocation,
 * surrounded in curly braces and terminated with a semicolon.
 */
#define BITPACK_MEMBER_ADD_ENUM(Type, Base, ...)                               \
	enum class Type : Base;                                                    \
	template<>                                                                 \
	constexpr FieldInfo field_info_for_type<Type>()                            \
	{                                                                          \
		constexpr auto info = FieldInfo{__VA_ARGS__};                          \
		static_assert(requires { Field<Type, info>(); });                      \
		return info;                                                           \
	}                                                                          \
	enum class Type : Base

/**
 * Define a new scoped enumeration type whose underlying type is bool, with the
 * given false and true value names, at the given bit position.
 *
 * FieldInfo fields other than minIndex and maxIndex may be provided as
 * additional arguments.
 *
 * Contrast `BITPACK_MEMBER_ADD_BOOL`, which does not introduce the custom
 * enumerator values like our `FalseVal` and `TrueVal`.
 *
 * See `BITPACK_MEMBER_ADD_ENUM`, which this uses, for more details.
 */
#define BITPACK_MEMBER_ADD_ENUM_BOOL(Type, FalseVal, TrueVal, BitIndex, ...)   \
	BITPACK_MEMBER_ADD_ENUM(Type, bool, BitIndex, BitIndex, __VA_ARGS__)       \
	{                                                                          \
		FalseVal = false, TrueVal = true,                                      \
	}

/**
 * Define a new boolean scoped enumeration with values named "Cleared" (0)
 * and "Asserted" (1) at the given bit index.
 */
#define BITPACK_MEMBER_ADD_ENUM_BOOL_CLEARED_ASSERTED(Type, BitIndex, ...)     \
	BITPACK_MEMBER_ADD_ENUM_BOOL(Type, Cleared, Asserted, BitIndex, __VA_ARGS__)

/**
 * Define a new boolean scoped enumeration with values named "Disabled" (0)
 * and "Enabled" (1) at the given bit index.
 */
#define BITPACK_MEMBER_ADD_ENUM_BOOL_DISABLED_ENABLED(Type, BitIndex, ...)     \
	BITPACK_MEMBER_ADD_ENUM_BOOL(Type, Disabled, Enabled, BitIndex, __VA_ARGS__)

/**
 * Encapsulate the gyrations required to define a `Numeric`-typed field and its
 * associated `field_info_for_type` within a `Bitpack`.  Takes the name of the
 * struct to define, the `Numeric`'s underlying type, and then the fields of a
 * `FieldInfo` as varargs.
 */
#define BITPACK_MEMBER_ADD_NUMERIC(Type, Base, ...)                            \
	struct Type : Numeric<Base>                                                \
	{                                                                          \
		using Numeric::Numeric;                                                \
	};                                                                         \
	template<>                                                                 \
	constexpr FieldInfo field_info_for_type<Type>()                            \
	{                                                                          \
		constexpr auto info = FieldInfo{__VA_ARGS__};                          \
		static_assert(requires { Field<Type, info>(); });                      \
		return info;                                                           \
	}

/**
 * Define a new type wrapper around bool for a 1-bit field at a given BitIndex.
 *
 * FieldInfo fields other than minIndex and maxIndex may be provided as
 * additional arguments.
 *
 * By contrast to `BITPACK_MEMBER_ADD_ENUM_BOOL`, the values introduced here are
 * `Type{false}` and `Type{true}` rather than custom enumerators.
 *
 * See `BITPACK_MEMBER_ADD_NUMERIC`, which this uses, for more details.
 */
#define BITPACK_MEMBER_ADD_BOOL(Type, BitIndex, ...)                           \
	BITPACK_MEMBER_ADD_NUMERIC(Type, bool, BitIndex, BitIndex, __VA_ARGS__)

/// @}

/**
 * \defgroup bitpacks_macros_member Type-qualifying member accessor macros
 * @{
 */

/**
 * A convenience macro that presumes the type `T` is defined within the
 * `Bitpack` `b` and finds such a field's proxy.
 *
 * There is no `BITPACK_MEMBER_TYPE(b, T)` analogue, because that's just
 * `b.member<T>()`.
 */
#define BITPACK_MEMBER_DECLTYPE(b, T) (b).template member<decltype(b)::T>()

/**
 * Like BITPACK_MEMBER_DECLTYPE, but with additional an "typename" keyword so we
 * can use a dependently-typed bitpack `b` (say, the type of `b` is `auto` or,
 * more generally, involves a template argument).
 */
#define BITPACK_MEMBER_DEPENDENT(b, T)                                         \
	(b).template member<typename decltype(b)::T>()

/// @}

/**
 * \defgroup bitpacks_macros_derived Derived bitpack definition macros
 * @{
 */

/**
 * Capture the incantations often at the top of a `BitpackDerived` structure.
 * especially one with type-directed field proxys.
 */
#define BITPACK_DERIVED_PREFIX                                                 \
	using BitpackDerived<ParentBitpack>::BitpackDerived;                       \
	using BitpackDerived<ParentBitpack>::operator=;                            \
	/* Inherit field info for most types from parent bitpack */                \
	template<typename FieldType>                                               \
	static constexpr FieldInfo field_info_for_type()                           \
	{                                                                          \
		return ParentBitpack::field_info_for_type<FieldType>();                \
	}

/// Modify the field information for a type in a derived bitpack.
#define BITPACK_DERIVED_FIELD_INFO_FOR_TYPE(Type, lambda)                      \
	template<>                                                                 \
	constexpr FieldInfo field_info_for_type<Type>()                            \
	{                                                                          \
		static_assert(                                                         \
		  std::is_invocable_r_v<void, decltype(lambda), FieldInfo &>);         \
		auto fi = ParentBitpack::field_info_for_type<Type>();                  \
		lambda(fi);                                                            \
		return fi;                                                             \
	}

/**
 * Modify the constness field information for a type in a derived bitpack.
 * This is a thin wrapper around BITPACK_DERIVED_FIELD_INFO_FOR_TYPE.
 */
#define BITPACK_DERIVED_FIELD_CONST_FOR_TYPE(Type, c)                          \
	BITPACK_DERIVED_FIELD_INFO_FOR_TYPE(Type, [](auto &fi) { fi.isConst = c; })

/// @}

/**
 * \defgroup bitpacks_macros_proxyop_qualify Type-qualifying proxy operator
 * macros
 * @{
 */

/**
 * Operate between a `Field::Proxy` `proxy` and a given `value`, which will be
 * qualifed with `proxy`'s `Field::Type` name.
 *
 * This will not work on `volatile` bitpacks; use `.read` first.
 */
#define BITPACK_OPERATE_QUALIFY(proxy, operator, value)                        \
	({                                                                         \
		using F = decltype(proxy)::Field::Type;                                \
		(proxy) operator(F::value);                                            \
	})

/// A fusion of BITPACK_MEMBER_DECLTYPE and BITPACK_OPERATE_QUALIFY
#define BITPACK_OPERATE_QUALIFY_DECLTYPE(b, T, operator, v)                    \
	BITPACK_OPERATE_QUALIFY(BITPACK_MEMBER_DECLTYPE(b, T), operator, v)

/// A fusion of BITPACK_MEMBER_DEPENDENT and BITPACK_OPERATE_QUALIFY
#define BITPACK_OPERATE_QUALIFY_DEPENDENT(b, T, operator, v)                   \
	BITPACK_OPERATE_QUALIFY(BITPACK_MEMBER_DEPENDENT(b, T), operator, v)

/// A fusion of .member<>() and BITPACK_OPERATE_QUALIFY
#define BITPACK_OPERATE_QUALIFY_TYPE(b, T, operator, v)                        \
	BITPACK_OPERATE_QUALIFY((b).template member<T>(), operator, v)

/// A specialization of BITPACK_OPERATE_QUALIFY using `.with` as the operator
#define BITPACK_WITH_QUALIFY(proxy, value)                                     \
	BITPACK_OPERATE_QUALIFY(proxy, .with, value)

/**
 * A specialization of BITPACK_OPERATE_QUALIFY_DECLTYPE using `.with` as the
 * operator
 */
#define BITPACK_WITH_QUALIFY_DECLTYPE(b, T, v)                                 \
	BITPACK_OPERATE_QUALIFY_DECLTYPE(b, T, .with, v)

/**
 * A specialization of BITPACK_OPERATE_QUALIFY_DEPENDENT using `.with` as the
 * operator
 */
#define BITPACK_WITH_QUALIFY_DEPENDENT(b, T, v)                                \
	BITPACK_OPERATE_QUALIFY_DEPENDENT(b, T, .with, v)

/**
 * A specialization of BITPACK_OPERATE_QUALIFY_TYPE using `.with` as the
 * operator
 */
#define BITPACK_WITH_QUALIFY_TYPE(b, T, v)                                     \
	BITPACK_OPERATE_QUALIFY_TYPE(b, T, .with, v)

/// @}

/**
 * \defgroup bitpacks_macros_proxyop_construct Type-constructing proxy operator
 * macros
 *
 * @{
 */

/**
 * Operate between a `Field::Proxy` `proxy` and a given `value`, which will be
 * wrapped with `proxy`'s `Field::Type`'s constructor.
 *
 * This will not work on `volatile` bitpacks; use `.read` first.
 */
#define BITPACK_OPERATE_WRAP(proxy, operator, value)                           \
	({                                                                         \
		using F = decltype(proxy)::Field::Type;                                \
		(proxy) operator(F{value});                                            \
	})

/// A fusion of BITPACK_MEMBER_DECLTYPE and BITPACK_OPERATE_WRAP
#define BITPACK_OPERATE_WRAP_DECLTYPE(b, T, operator, v)                       \
	BITPACK_OPERATE_WRAP(BITPACK_MEMBER_DECLTYPE(b, T), operator, v)

/// A fusion of BITPACK_MEMBER_DEPENDENT and BITPACK_OPERATE_WRAP
#define BITPACK_OPERATE_WRAP_DEPENDENT(b, T, operator, v)                      \
	BITPACK_OPERATE_WRAP(BITPACK_MEMBER_DEPENDENT(b, T), operator, v)

/// A fusion of .member<>() and BITPACK_OPERATE_WRAP
#define BITPACK_OPERATE_WRAP_TYPE(b, T, operator, v)                           \
	BITPACK_OPERATE_WRAP((b).template member<T>(), operator, v)

/// A specialization of BITPACK_OPERATE_WRAP using `.with` as the operator
#define BITPACK_WRAP_WITH(proxy, value)                                        \
	BITPACK_OPERATE_WRAP(proxy, .with, value)

/**
 * A specialization of BITPACK_OPERATE_WRAP_DECLTYPE using `.with` as the
 * operator.
 */
#define BITPACK_WITH_WRAP_DECLTYPE(b, T, v)                                    \
	BITPACK_OPERATE_WRAP_DECLTYPE(b, T, .with, v)

/**
 * A specialization of BITPACK_OPERATE_WRAP_DEPENDENT using `.with` as the
 * operator
 */
#define BITPACK_WITH_WRAP_DEPENDENT(b, T, v)                                   \
	BITPACK_OPERATE_WRAP_DEPENDENT(b, T, .with, v)

/// A specialization of BITPACK_OPERATE_WRAP_TYPE using `.with` as the operator
#define BITPACK_WITH_WRAP_TYPE(b, T, v)                                        \
	BITPACK_OPERATE_WRAP_TYPE(b, T, .with, v)

/// @}

/**
 * \defgroup bitpacks_macros_proxyop_directed Type-directed proxy operator
 * macros
 *
 * @{
 */

/**
 * Given a bitpack `b` -- not a Proxy of a Field therein -- and a value, operate
 * between the bitpack's view of that value's type and the value.
 */
#define BITPACK_OPERATE_VALUE(b, operator, value)                              \
	({ (b).template member<decltype(value)>() operator(value); })

/**
 * Given a bitpack `b` -- not a Proxy of a Field therein -- qualify the given
 * value with the bitpack's type and then operate between the bitpack's Proxy of
 * that qualified value's type and said value.
 */
#define BITPACK_OPERATE_VALUE_DECLTYPE(b, operator, value)                     \
	BITPACK_OPERATE_VALUE(b, operator, decltype(b)::value)

/**
 * BITPACK_OPERATE_VALUE with dependent qualification for the type of the
 * bitpack.
 */
#define BITPACK_OPERATE_VALUE_DEPENDENT(b, operator, value)                    \
	BITPACK_OPERATE_VALUE(b, operator, typename decltype(b)::value)

/// A specialization of BITPACK_OPERATE_VALUE using `.with` as the operator
#define BITPACK_WITH_VALUE(b, value) BITPACK_OPERATE_VALUE(b, .with, value)

/**
 * A specialization of BITPACK_OPERATE_VALUE_DECLTYPE using `.with` as the
 * operator
 */
#define BITPACK_WITH_VALUE_DECLTYPE(b, value)                                  \
	BITPACK_OPERATE_VALUE_DECLTYPE(b, .with, value)

/**
 * A specialization of BITPACK_OPERATE_VALUE_DEPENDENT using `.with` as the
 * operator.
 */
#define BITPACK_WITH_VALUE_DEPENDENT(b, value)                                 \
	BITPACK_OPERATE_VALUE_DEPENDENT(b, .with, value)

/// @}

/**
 * \defgroup bitpacks_macros_proxyop_enum "using enum" proxy operator macros
 *
 * @{
 */

/**
 * Convenience for scoped enum fields, bringing the enumerators of the field
 * type into scope while evaluating the value.
 *
 * Note that this uses `using enum` internally, and so cannot work with
 * `Proxy`-s whose Field::Type-s are dependent.  Alas, this precludes the
 * existence of a `BITPACK_*_ENUM_DEPENDENT` family of helpers.
 *
 * Probably prefer BITPACK_OPERATE_QUALIFY if you do not need repeated use of
 * the enumeration's values within the passed `value`.
 */
#define BITPACK_OPERATE_ENUM(proxy, operator, value)                           \
	({                                                                         \
		using E = decltype(proxy)::Field::Type;                                \
		static_assert(std::is_enum_v<E>);                                      \
		using enum E;                                                          \
		(proxy) operator(value);                                               \
	})

/// A fusion of BITPACK_MEMBER_DECLTYPE and BITPACK_OPERATE_ENUM
#define BITPACK_OPERATE_ENUM_DECLTYPE(b, T, operator, v)                       \
	BITPACK_OPERATE_ENUM(BITPACK_MEMBER_DECLTYPE(b, T), operator, v)

/// A fusion of .member<>() and BITPACK_OPERATE_ENUM
#define BITPACK_OPERATE_ENUM_TYPE(b, T, operator, v)                           \
	BITPACK_OPERATE_ENUM((b).template member<T>(), operator, v)

/// A specialization of BITPACK_OPERATE_ENUM using `.with` as the operator
#define BITPACK_WITH_ENUM(proxy, value)                                        \
	BITPACK_OPERATE_ENUM(proxy, .with, value)

/**
 * A specialization of BITPACK_OPERATE_ENUM_DECLTYPE using `.with` as the
 * operator.
 */
#define BITPACK_WITH_ENUM_DECLTYPE(b, T, v)                                    \
	BITPACK_OPERATE_ENUM_DECLTYPE(b, T, .with, v)

/// A specialization of BITPACK_OPERATE_ENUM_TYPE using `.with` as the operator
#define BITPACK_WITH_ENUM_TYPE(b, T, v)                                        \
	BITPACK_OPERATE_ENUM_TYPE(b, T, .with, v)

/// @}

/**
 * \defgroup bitpacks_macros_vararg Multi-field utility macros
 *
 * @{
 */

/**
 * \defgroup bitpacks_macros_vararg_internal Internal helpers
 *
 * @{
 */

/**
 * An in-situ probe for the #include-sion of <__macro_map.h>.
 *
 * While we could gate our behavior on `#if defined(CHERIOT_EVAL0)`, for
 * example, that would require that <__macro_map.h> be included before us, which
 * is slightly rude.  Instead, we can take creative advantage of the syntactic
 * primitives __macro_map.h uses internally and some C++ quirks to (ab)usefully
 * test the behavior of `CHERIOT_EVAL0` at each expansion of this macro.
 *
 * In terms of operation, this relies on `U CHERIOT_EVAL0(T);` meaning one of
 *  two very different things and so dramatically changing what the subsequent
 * `sizeof(T)` means.
 *
 *  - If CHERIOT_EVAL0 hasn't been #defined, then this defines a function named
 *    CHERIOT_EVAL0 which takes a `struct T` and returns a `U`.  In this case,
 *    the `T` in `sizeof(T)` means `struct T`, and that's 1 by definition,
 *    since `sizeof(char)` is defined to be 1.
 *
 *  - If CHERIOT_EVAL0 has been defined as per <__macro_map.h>, then this is
 *    preprocessed into `U T;`, a variable declaration, and the `T` in
 *    `sizeof(T)` now refers to that variable and evaluates to 2.
 */
#define BITPACK_HAS_MACRO_MAP                                                  \
	([]() {                                                                    \
		struct BITPACK_HAS_MACRO_MAP_T                                         \
		{                                                                      \
			char t;                                                            \
		};                                                                     \
		struct BITPACK_HAS_MACRO_MAP_U                                         \
		{                                                                      \
			char u[2];                                                         \
		};                                                                     \
		BITPACK_HAS_MACRO_MAP_U CHERIOT_EVAL0(BITPACK_HAS_MACRO_MAP_T);        \
		return sizeof(BITPACK_HAS_MACRO_MAP_T);                                \
	}() > 1)

/// Map function for BITPACK_MAP_DECLTYPE
#define BITPACK_MAP_DECLTYPE_HELPER(x, b) decltype(b)::x

/**
 * Given bitpack `b`, qualify each additional argument with `decltype(b)`.  That
 * is, `BITPACK_MAP_DECLTYPE(b, X, Y)` evalutes to
 * `decltype(b)::X, decltype(b)::Y`.
 *
 * This requires #include <__macro_map.h> (and will give confusing error
 * messages if not included).  (Because this is very much a C++ token level
 * hack, unlike the other __macro_map users, it's hard for us to statically
 * assert and give nice error messages.)
 */
#define BITPACK_MAP_DECLTYPE(b, ...)                                           \
	CHERIOT_MAP_LIST_UD(BITPACK_MAP_DECLTYPE_HELPER, b, __VA_ARGS__)

/// Map function for BITPACK_MAP_DEPENDENT
#define BITPACK_MAP_DEPENDENT_HELPER(x, b) typename decltype(b)::x

/**
 * Given bitpack `b`, qualify each additional argument with `typename
 * decltype(b)`.  That is, `BITPACK_DEPENDENT(b, X, Y)` evalutes to
 * `typename decltype(b)::X, typename decltype(b)::Y`.
 *
 * This requires #include <__macro_map.h>.
 */
#define BITPACK_MAP_DEPENDENT(b, ...)                                          \
	CHERIOT_MAP_LIST_UD(BITPACK_MAP_DEPENDENT_HELPER, b, __VA_ARGS__)

/// Map function for BITPACK_WITHS
#define BITPACK_MAP_WITHS_HELPER(x) .with(x)

/// @}

/**
 * Construct a chain of .with() whose arguments are all qualified with the type
 * of the bitpack.  #include <__macro_map.h> if you want to use this.
 */
#define BITPACK_WITHS(b, ...)                                                  \
	({                                                                         \
		static_assert(BITPACK_HAS_MACRO_MAP,                                   \
		              "BITPACK_WITHS requires __macro_map.h");                 \
		(b) CHERIOT_MAP(BITPACK_MAP_WITHS_HELPER, __VA_ARGS__);                \
	})

/**
 * A version of BITPACK_WITHS where the arguments to `.with()` are qualified
 * with the type of the bitpack.
 */
#define BITPACK_WITHS_DECLTYPE(b, ...)                                         \
	BITPACK_WITHS(b, BITPACK_MAP_DECLTYPE(b, __VA_ARGS__))

/**
 * A version of BITPACK_WITHS where the arguments to `.with()` are dependently
 * qualified with the type of the bitpack.
 */
#define BITPACK_WITHS_DEPENDENT(b, ...)                                        \
	BITPACK_WITHS(b, BITPACK_MAP_DEPENDENT(b, __VA_ARGS__))

/**
 * @addtogroup macros_vararg_internal
 * @{
 */

/// Helper for BITPACK_RELATE_MASKED, for computing individual field's masks
#define BITPACK_RELATE_MASKED_HELPER(x, b)                                     \
	| ({                                                                       \
		using BT = decltype(b);                                                \
		using FT = decltype(x);                                                \
		BT::Field<FT, BT::field_info_for_type<FT>()>::FieldMask;               \
	})

/// @}

/**
 * Given a list of field values (which must be fully qualified names), compute
 * the mask of these fields and the bitpack value holding these field values,
 * then mask `b` with the computed mask, and then use `operator` to relate the
 * result with the computed bitpack value.  The last value for each field is
 * used.  #include <__macro_map.h> if you want to use this.
 */
#define BITPACK_RELATE_MASKED(b, operator, ...)                                \
	({                                                                         \
		static_assert(BITPACK_HAS_MACRO_MAP,                                   \
		              "BITPACK_MASKED_REL requires __macro_map.h");            \
		constexpr decltype(b.raw()) __bitpack_mask =                           \
		  (0)CHERIOT_MAP_UD(BITPACK_RELATE_MASKED_HELPER, b, __VA_ARGS__);     \
		constexpr auto __bitpack_query =                                       \
		  BITPACK_WITHS((decltype(b))(0), __VA_ARGS__).raw();                  \
		(b.raw() & __bitpack_mask) operator(__bitpack_query);                  \
	})

/**
 * A version of BITPACK_RELATE_MASKED where the field values are qualified with
 * the type of the bitpack.
 */
#define BITPACK_RELATE_MASKED_DECLTYPE(b, operator, ...)                       \
	BITPACK_RELATE_MASKED(b, operator, BITPACK_MAP_DECLTYPE(b, __VA_ARGS__))

/**
 * A version of BITPACK_RELATE_MASKED where the field values are dependently
 * qualified with the type of the bitpack.
 */
#define BITPACK_RELATE_MASKED_DEPENDENT(b, operator, ...)                      \
	BITPACK_RELATE_MASKED(b, operator, BITPACK_MAP_DEPENDENT(b, __VA_ARGS__))

/// @}
/// @}
/// @}
