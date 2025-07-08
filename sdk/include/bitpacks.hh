#pragma once

/**
 * @file
 *
 * `Bitpack`-s are bitfield-esque composite structures within numeric words.
 * They are, roughly, meant to serve as lenses through which one views the bits
 * of an underlying numeric word as as a product of (typed) fields, each
 * occuping a contiguous span of bits within that word.
 *
 * In implementation, these sub-word fields are accessed through typed
 * `Bitpack::Field::View` proxy objects.  These are almost always, ultimately,
 * built through the `Bitpack::view()` method, but a myriad of helper functions
 * exist to reduce the syntactic clutter for common use cases.
 *
 * `Bitpack`-s are meant to work in a variety of C++ contexts, including both
 * within `constexpr` expressions and subject to `volatile` qualification
 * (especially useful when `Bitpack`-s are used to describe MMIO interfaces).
 * Type-level restrictions attempt to ensure that accesses to `volatile`
 * `Bitpack`-s are explicit in the source; see individual method and operator
 * documentation for details.
 *
 * Multiple styles of definition are supported: fields may be entirely manually
 * named and defined, or there is machinery for type-directed summoning of View
 * proxies based on the field's type.  The two approaches may be used in tandem
 * within the same `Bitpack`, should that be desirable.
 *
 * It is often useful to define a dedicated type for each field within a
 * `Bitpack`, especially when the values of that field are an arbitrary
 * enumeration of a finite space of named options.  Values of that type can be
 * made more challenging to accidentially use in other places.  `enum class`-es
 * can be particularly well suited for this purpose, save that they have awkward
 * C++ syntactic requirements.  As such, there is a convenience macro for such
 * definitions (`BITPACK_DEFINE_TYPE_ENUM_CLASS`) and there are quite a few
 * convenience macros (`BITPACK_ENUM_QTYPE_*`) that should assist with their
 * use.
 *
 * Similarly, some Fields are more numeric than enumerative, and there are
 * macros meant to assist there, too, again at definitions
 * (`BITPACK_DEFINE_TYPE_NUMERIC`) and at use sites (`BITPACK_NUMERIC_QTYPE_*`).
 *
 * Because `Bitpack`-s encourage the use of types defined within derived
 * classes, there are cases where the qualified names of these types are
 * "dependent", such as when the derived type is a template parameter.  For such
 * cases, use the macros with `_TQTYPE` in their name (rather than `_QTYPE`).
 *
 * For cases where fields' types are defined externally to a `Bitpack`, which
 * may happen when a `Bitpack` is interfacing with existing code, use the macros
 * that have `_UTYPE` in their name (rather than `_QTYPE`).
 */

#include <concepts>
#include <stddef.h>
#include <type_traits>

/*
 * Mark the Bitpack class declaration with cdef.h's `CHERIOT_EXPERIMENTAL()` --
 * that is, as eliciting a warning if CHERIOT_EXPERIMENTAL_APIS_WARN is defined
 * -- unless CHERIOT_EXPERIMENTAL_NOWARN_BITPACKS is defined.
 */
#if !defined(CHERIOT_EXPERIMENTAL_NOWARN_BITPACKS)
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
template<typename Storage>
    requires std::is_unsigned_v<Storage>
class BITPACK_DECL_ANNOTATION Bitpack
{
	Storage value;

	public:
	/// Construct a `Bitpack` value with an underlying Storage of all zero bits
	constexpr Bitpack() : value(0) {}

	/// Construct a `Bitpack` value from a value of its underlying Storage type
	constexpr Bitpack(Storage v) : value(v) {}

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
	constexpr Storage raw() const
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

		constexpr T raw() const
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
		 * Should this field be viewed as constant (and so mutation be slightly
		 * less ergonomic)?
		 */
		bool isConst = false;
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
		static_assert(Info.maxIndex >= Info.minIndex);

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
		 * Views are templated on their containing derived type (so that they
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
		class View
		{
			/// A qualified reference to the containing `Bitpack`'s `Storage`.
			RefTypeParam ref;

			public:
			/// Name the Field of which we are a view
			using Field = Field;

			/// Expose the qualified reference type we hold to the Storage
			using RefType = RefTypeParam;

			/**
			 * Construct a view given a reference to the storage word.
			 *
			 * May take any reference type implicitly convertible to RefType,
			 * not just RefType itself.
			 */
			template<typename R>
			constexpr View(R &r) : ref(r.value)
			{
			}

			/**
			 * Compute and store an updated `Bitpack` value with a new value for
			 * this field.
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
			 * Explicit conversion of a Field::View to the field value.
			 *
			 * The View must not have a `volatile` reference to Storage.
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
			constexpr Field::Type raw() const
			{
				return static_cast<Field::Type>(*this);
			}

			/**
			 * Construct a new `Bitpack` value with an updated value of this
			 * field.
			 *
			 * This is emphatically not `volatile`-qualified, and has no
			 * `volatile`-qualified overload.  `.read()` the containing
			 * `Bitpack`, first.
			 */
			constexpr DerivedBitpack with(Field::Type rhs) const
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
				return raw_with(storage, f(raw_view(storage)));
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
		View(BitpackType &r)
		  -> View<std::remove_cvref_t<BitpackType>,
		          ConditionalConstRef<Info.isConst, decltype((r.value))>>;
	};

	protected:
	/**
	 * Build a Field::View of an explicitly given type and info.
	 *
	 * This is protected so that it is available to subclasses but not more
	 * generally.
	 */
	template<typename FieldType, FieldInfo Info, typename Self>
	constexpr auto view(this Self &&self)
	{
		return typename Field<FieldType, Info>::View(self);
	}

	public:
	/**
	 * Build a Field::View proxy by asking the derived Self class for a
	 * FieldInfo structure computed from template expansion of
	 * Self::field_info_for_type<FieldType>().
	 */
	template<typename FieldType, typename Self>
	    requires std::is_convertible_v<
	      decltype(std::remove_cvref_t<Self>::template field_info_for_type<
	               FieldType>()),
	      const FieldInfo>
	constexpr auto view(this Self &&self)
	{
		/*
		 * Find the information for the field type; "deducing this" gives us
		 * access to the derived class's type without need for the CRTP.
		 */
		static constexpr FieldInfo Info =
		  std::remove_cvref_t<Self>::template field_info_for_type<FieldType>();

		return self.template view<FieldType, Info>();
	}

	/**
	 * Fetch the value of a field in this `Bitpack` based on the field type.
	 *
	 * For `volatile` `Bitpack`s, this will perform a load.
	 */
	template<typename FieldType, typename Self>
	constexpr FieldType get(this Self &&self)
	{
		return self.template view<FieldType>();
	}

	/**
	 * Compute a new `Bitpack` value with an updated field of a given type.
	 *
	 * For `volatile` `Bitpack`s, this will perform a load.
	 */
	template<typename FieldType, typename Self>
	constexpr std::remove_cvref_t<Self> with(this Self &&self, FieldType v)
	{
		return self.template view<FieldType>().with(v);
	}

	/**
	 * Compute a new `Bitpack` value with an updated field of a given type.
	 *
	 * For `volatile` `Bitpack`s, this will perform a load and store.
	 */
	template<typename FieldType, typename Self>
	constexpr void set(this Self &&self, FieldType v)
	{
		self.template view<FieldType>() = v;
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
 * Capture the incantations often at the top of a `Bitpack` structure,
 * especially one with type-directed field views.
 */
#define BITPACK_USUAL_PREFIX                                                   \
	using Bitpack::Bitpack;                                                    \
	using Bitpack::operator=;                                                  \
	template<typename FieldType>                                               \
	static constexpr FieldInfo field_info_for_type() = delete;

/**
 * Define a named accessor for a field.
 *
 * Takes the name of the accessor, the type to use for the view, and the fields
 * of the FieldInfo (as varargs).  If the latter is omitted, this defines an
 * alias for a field whose type is sufficient to resolve the FieldInfo through
 * field_info_for_type.
 */
#define BITPACK_NAMED_VIEW(name, Type, ...)                                    \
	template<typename Self>                                                    \
	constexpr auto name(this Self &&self)                                      \
	{                                                                          \
		return self.template view<Type __VA_OPT__(, {__VA_ARGS__})>();         \
	}

/**
 * Encapsulate the gyrations required to define an `enum class`-typed field and
 * its associated `field_info_for_type` within a `Bitpack`.  Takes the name of
 * the `enum` class to define, its underlying type, and then the fields of a
 * `FieldInfo` as varargs; the enumerators should follow the macro invocation,
 * surrounded in curly braces and terminated with a semicolon.
 */
#define BITPACK_DEFINE_TYPE_ENUM_CLASS(Type, Base, ...)                        \
	enum class Type : Base;                                                    \
	template<>                                                                 \
	constexpr FieldInfo field_info_for_type<Type>()                            \
	{                                                                          \
		return {__VA_ARGS__};                                                  \
	}                                                                          \
	enum class Type : Base

/**
 * Encapsulate the gyrations required to define a `Numeric`-typed field and its
 * associated `field_info_for_type` within a `Bitpack`.  Takes the name of the
 * struct to define, the `Numeric`'s underlying type, and then the fields of a
 * `FieldInfo` as varargs.
 */
#define BITPACK_DEFINE_TYPE_NUMERIC(Type, Base, ...)                           \
	struct Type : Numeric<Base>                                                \
	{                                                                          \
		using Numeric::Numeric;                                                \
	};                                                                         \
	template<>                                                                 \
	constexpr FieldInfo field_info_for_type<Type>()                            \
	{                                                                          \
		return {__VA_ARGS__};                                                  \
	}

/**
 * A convenience macro that presumes the type `t` is defined within the
 * `Bitpack` `b` and finds such a field's view proxy.
 *
 * There is no `BITPACK_BY_UTYPE(b, t)` analogue, because that's just
 * `b.view<t>()`.
 */
#define BITPACK_BY_QTYPE(b, t) (b).view<decltype(b)::t>()

/**
 * Like BITPACK_BY_QTYPE, but with additional "template" and "typename" tokens
 * so we can use a dependently-typed bitpack `b` (say, the type of `b` is a
 * function of a template parameter).
 */
#define BITPACK_BY_TQTYPE(b, t) (b).template view<typename decltype(b)::t>()

/// Assign a field in a bitpack based on the type of the (qualified) value.
#define BITPACK_QVAL_ASSIGN(b, value)                                          \
	{                                                                          \
		auto qval                  = decltype(b)::value;                       \
		(b).view<decltype(qval)>() = qval;                                     \
	}

/// Assign a field in a bitpack based on the type of the (unqualified) value.
#define BITPACK_UVAL_ASSIGN(b, value)                                          \
	{                                                                          \
		(b).view<decltype(value)>() = value;                                   \
	}

/**
 * Convenience for scoped enum fields, bringing the enumerators of the field
 * type into scope while evaluating the value.
 */
#define BITPACK_ENUM_OP(fview, op, value)                                      \
	({                                                                         \
		using E = decltype(fview)::Field::Type;                                \
		static_assert(std::is_enum_v<E>);                                      \
		using enum E;                                                          \
		(fview) op(value);                                                     \
	})

/// A fusion of BITPACK_BY_QTYPE and BITPACK_ENUM_OP
#define BITPACK_ENUM_QTYPE_OP(b, t, op, v)                                     \
	BITPACK_ENUM_OP(BITPACK_BY_QTYPE(b, t), op, v)

/// A fusion of .view<>() and BITPACK_ENUM_OP
#define BITPACK_ENUM_UTYPE_OP(b, t, op, v) BITPACK_ENUM_OP((b).view<t>(), op, v)

/**
 * Get the value of a `Numeric` `Field::View`
 *
 * This (ab)uses the fact that `static_cast<>`s can chain explicit conversions.
 */
#define BITPACK_NUMERIC_VALUE(fview)                                           \
	({                                                                         \
		using F = decltype(fview)::Field::Type;                                \
		static_cast<F::NumericType>(static_cast<F>(fview));                    \
	})

/// A fusion of BITPACK_NUMERIC_VALUE and BITPACK_BY_QTYPE
#define BITPACK_NUMERIC_QTYPE_VALUE(b, t)                                      \
	BITPACK_NUMERIC_VALUE(BITPACK_BY_QTYPE(b, t))

/// A fusion of BITPACK_NUMERIC_VALUE and BITPACK_BY_TQTYPE
#define BITPACK_NUMERIC_TQTYPE_VALUE(b, t)                                     \
	BITPACK_NUMERIC_VALUE(BITPACK_BY_TQTYPE(b, t))

/// Operate between a `Numeric` `Field::View` and a given value.
#define BITPACK_NUMERIC_OP(fview, op, value)                                   \
	({                                                                         \
		using F = decltype(fview)::Field::Type;                                \
		static_cast<F::NumericType>(static_cast<F>(                            \
		  fview)) /* NOLINTNEXTLINE(bugprone-macro-parentheses) */             \
		  op static_cast<F::NumericType>(value);                               \
	})

/// A fusion of BITPACK_BY_QTYPE and BITPACK_NUMERIC_OP
#define BITPACK_NUMERIC_QTYPE_OP(b, t, op, v)                                  \
	BITPACK_NUMERIC_OP(BITPACK_BY_QTYPE(b, t), op, v)

/// A fusion of .view<>() and BITPACK_NUMERIC_OP
#define BITPACK_NUMERIC_UTYPE_OP(b, t, op, v)                                  \
	BITPACK_NUMERIC_OP((b).view<t>(), op, v)

/**
 * Given a view of an enum-valued field in a bitpack, operate on its value, with
 * the the field type's enum's values into scope while computing the value.
 */
#define BITPACK_ENUM_ASSIGN(fview, value) BITPACK_ENUM_OP(fview, =, value)

/**
 * A fusion of BITPACK_BY_QTYPE and BITPACK_ENUM_ASSIGN.  Unless the value is
 * making repeated reference to the enum type's values, prefer the clearer
 * BITPACK_QVAL_ASSIGN
 */
#define BITPACK_ENUM_QTYPE_ASSIGN(b, t, v)                                     \
	BITPACK_ENUM_ASSIGN(BITPACK_BY_QTYPE(b, t), v)

/**
 * A fusion of .view<>() and BITPACK_ENUM_ASSIGN.  Unless the value is making
 * repeated reference to the enum type's values, prefer the clearer
 * BITPACK_UVAL_ASSIGN.
 */
#define BITPACK_ENUM_UTYPE_ASSIGN(b, t, v) BITPACK_ENUM_ASSIGN((b).view<t>(), v)

/**
 * Given a view of an enum-valued field in a bitpack, compute a new bitpack
 * value with an updated value for this field, with the the field type's enum's
 * values into scope while computing the value.
 */
#define BITPACK_ENUM_WITH(fview, value) BITPACK_ENUM_OP(fview, .with, value)

/// A fusion of BITPACK_BY_QTYPE and BITPACK_ENUM_WITH
#define BITPACK_ENUM_QTYPE_WITH(b, t, v)                                       \
	BITPACK_ENUM_WITH(BITPACK_BY_QTYPE(b, t), v)

/// A fusion of .view<>() and BITPACK_ENUM_WITH
#define BITPACK_ENUM_UTYPE_WITH(b, t, v) BITPACK_ENUM_WITH((b).view<t>(), v)

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

/// Map function for BITPACK_QUALIFY
#define BITPACK_QUALIFY_HELPER(x, b) decltype(b)::x

/**
 * Given bitpack `b`, qualify each additional argument with `decltype(b)`.  That
 * is, `BITPACK_QUALIFY(b, X, Y)` evalutes to  `decltype(b)::X, decltype(b)::Y`.
 *
 * This requires #include <__macro_map.h> (and will give confusing error
 * messages if not included).  (Because this is very much a C++ token level
 * hack, unlike the other __macro_map users, it's hard for us to statically
 * assert and give nice error messages.)
 */
#define BITPACK_QUALIFY(b, ...)                                                \
	CHERIOT_MAP_LIST_UD(BITPACK_QUALIFY_HELPER, b, __VA_ARGS__)

/// Map function for BITPACK_QUALIFY_TYPENAME
#define BITPACK_TYPENAME_QUALIFY_HELPER(x, b) typename decltype(b)::x

/**
 * Given bitpack `b`, qualify each additional argument with `typename
 * decltype(b)`.  That is, `BITPACK_QUALIFY(b, X, Y)` evalutes to
 * `typename decltype(b)::X, typename decltype(b)::Y`.
 *
 * This requires #include <__macro_map.h>.
 */
#define BITPACK_TYPENAME_QUALIFY(b, ...)                                       \
	CHERIOT_MAP_LIST_UD(BITPACK_TYPENAME_QUALIFY_HELPER, b, __VA_ARGS__)

/// Map function for BITPACK_WITHS
#define BITPACK_WITHS_HELPER(x) .with(x)

/**
 * Construct a chain of .with() whose arguments are all qualified with the type
 * of the bitpack.  #include <__macro_map.h> if you want to use this.
 */
#define BITPACK_WITHS(b, ...)                                                  \
	({                                                                         \
		static_assert(BITPACK_HAS_MACRO_MAP,                                   \
		              "BITPACK_WITHS requires __macro_map.h");                 \
		(b) CHERIOT_MAP(BITPACK_WITHS_HELPER, __VA_ARGS__);                    \
	})

/// A fusion of BITPACK_WITH and BITPACK_QUALIFY
#define BITPACK_QWITHS(b, ...) BITPACK_WITHS(b, BITPACK_QUALIFY(b, __VA_ARGS__))

/// A fusion of BITPACK_WITH and BITPACK_TYPENAME_QUALIFY
#define BITPACK_TQWITHS(b, ...)                                                \
	BITPACK_WITHS(b, BITPACK_TYPENAME_QUALIFY(b, __VA_ARGS__))

/// Helper for BITPACK_MASKED_OP, for computing individual field's masks
#define BITPACK_MASKED_OP_HELPER(x, b)                                         \
	| ({                                                                       \
		using BT = decltype(b);                                                \
		using FT = decltype(x);                                                \
		BT::Field<FT, BT::field_info_for_type<FT>()>::FieldMask;               \
	})

/**
 * Given a list of field values (which must be fully qualified names), compute
 * the mask of these fields and the bitpack value holding these field values,
 * then mask `b` with the computed mask, and then use `op` to relate the result
 * with the computed bitpack value.  The last value for each field is used.
 * #include <__macro_map.h> if you want to use this.
 */
#define BITPACK_MASKED_OP(b, op, ...)                                          \
	({                                                                         \
		static_assert(BITPACK_HAS_MACRO_MAP,                                   \
		              "BITPACK_MASKED_OP requires __macro_map.h");             \
		constexpr decltype(b.raw()) __bitpack_mask =                           \
		  (0)CHERIOT_MAP_UD(BITPACK_MASKED_OP_HELPER, b, __VA_ARGS__);         \
		constexpr auto __bitpack_query =                                       \
		  BITPACK_WITHS((decltype(b))(0), __VA_ARGS__);                        \
		(b.raw() & __bitpack_mask) op(__bitpack_query);                        \
	})

/// A fusion of BITPACK_MASKED_OP and BITPACK_QUALIFY
#define BITPACK_MASKED_OP_Q(b, op, ...)                                        \
	BITPACK_MASKED_OP(b, op, BITPACK_QUALIFY(b, __VA_ARGS__))
