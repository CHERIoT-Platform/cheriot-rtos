#pragma once

#include <concepts>
#include <stddef.h>
#include <type_traits>

template<typename Storage>
    requires std::is_unsigned_v<Storage>
class Bitpack
{
	Storage value;

	public:
	constexpr Bitpack() : value(0) {}
	constexpr Bitpack(Storage v) : value(v) {}

	/**
	 * Assign a whole Bitpack at once given a (const, not volatile) Bitpack of
	 * the same type.
	 *
	 * To assign from a `volatile` Bitpack, perform an explicit `.read()` first.
	 *
	 * "Deducing this" gives us CRTP-esque behavior without, well, the CRTP, so
	 * this will not accept an unrelated derived Bitpack class.  (But will
	 * accept types that can be converted, so assigning to a Bitpack, not a
	 * derived class, will accept any derived class, for example.)
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
	 * Extract the underlying Storage value.
	 *
	 * This is emphatically not `volatile`-qualified, and has no
	 * `volatile`-qualified overload, to make it more apparent where `volatile`
	 * reads are taking place in code handling `Bitpack`-s.  Use `read()` first.
	 */
	constexpr explicit operator Storage() const
	{
		return this->value;
	}

	/// A shorter way of spelling static_cast<Storage>()...
	constexpr Storage raw() const
	{
		return static_cast<Storage>(*this);
	}

	/**
	 * Return a snapshot of the underlying storage.
	 *
	 * Notably, this can be used to get a `const Bitpack` from a `volatile
	 * Bitpack` (`const` or not).
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
		using NumericType = T;

		static_assert(sizeof(T) <= sizeof(Storage));

		T value;

		constexpr Numeric(T v) : value(v) {}

		constexpr operator T() const
		{
			return this->value;
		}
	};

	/// Information about a field within a Bitpack
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

	template<typename TypeParam, FieldInfo InfoParam>
	struct Field
	{
		using Type                      = TypeParam;
		static constexpr FieldInfo Info = InfoParam;

		static_assert(Info.maxIndex >= Info.minIndex);

		static constexpr Storage ValueMask =
		  (1U << (Info.maxIndex - Info.minIndex + 1)) - 1;
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

		/// Update for fields that are themselves Bitpacks at smaller types
		template<typename OtherStorage>
		    requires requires { sizeof(OtherStorage) < sizeof(Storage); }
		constexpr static Storage raw_with(Storage               lhs,
		                                  Bitpack<OtherStorage> rhs)
		{
			return raw_with(lhs, static_cast<OtherStorage>(rhs));
		}

		template<typename DerivedBitpack, typename RefTypeParam>
		    requires std::is_base_of_v<Bitpack, DerivedBitpack> &&
		             std::is_lvalue_reference_v<RefTypeParam> &&
		             std::is_same_v<Storage, std::remove_cvref_t<RefTypeParam>>
		class View
		{
			RefTypeParam ref;

			public:
			using Field   = Field;
			using RefType = RefTypeParam;

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
			 * This is emphatically not `volatile`-qualified, and has no
			 * `volatile`-qualified overload.  `.read()` the containing
			 * `Bitpack`, first.
			 */
			constexpr operator Field::Type() const
			{
				return raw_view(ref);
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
		};

		template<bool C, typename T>
		    requires std::is_lvalue_reference_v<T>
		using ConditionalConstRef = std::
		  conditional_t<C, std::add_const_t<std::remove_reference_t<T>> &, T>;

		/*
		 * Guide template deduction to conclude that a Field has a RefType
		 * that is as CV-qualified as the Bitpack of which it is a part, with
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
	 * once.  Intended particularly for use with `volatile` Bitpacks, to help
	 * ensure only one read and one write takes place, as the callback operates
	 * on a non-volatile Bitpack.
	 */
	template<typename Self>
	constexpr void alter(this Self &&self, auto &&f)
	    requires std::is_invocable_r_v<std::remove_cvref_t<Self>,
	                                   decltype(f),
	                                   std::remove_cvref_t<Self>>
	{
		std::remove_cvref_t<Self> value{self.value};
		self.value = f(value);
	}
};

/**
 * Capture the incantations often at the top of a Bitpack structure, especially
 * one with type-directed field views.
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
 * of the FieldInfo (as varargs).
 */
#define BITPACK_NAMED_VIEW(name, Type, ...)                                    \
	template<typename Self>                                                    \
	constexpr auto name(this Self &&self)                                      \
	{                                                                          \
		return self.template view<Type, {__VA_ARGS__}>();                      \
	}

/**
 * Encapsulate the gyrations required to define an enum class field and its
 * associated field_info_for_type within a Bitpack.  Takes the name of the enum
 * class to define, its underlying type, and then the fields of a FieldInfo as
 * varargs; the enumerators should follow the macro invocation, surrounded in
 * curly braces and terminated with a semicolon.
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
 *
 * Encapsulate the gyrations required to define a Numeric field and its
 * associated field_info_for_type within a Bitpack.  Takes the name of the
 * struct to define, the Numeric's underlying type, and then the fields of a
 * FieldInfo as varargs.
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
 * A convenience macro that presumes the type `t` is defined within the Bitpack
 * `b` and finds such a field's view proxy.
 */
#define BITPACK_BY_QTYPE(b, t) (b).view<decltype(b)::t>()

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
#define BITPACK_ENUM_OP(view, op, value)                                       \
	({                                                                         \
		using E = decltype(view)::Field::Type;                                 \
		static_assert(std::is_enum_v<E>);                                      \
		using enum E;                                                          \
		(view) op(value);                                                      \
	})

/// A fusion of BITPACK_BY_QTYPE and BITPACK_ENUM_OP
#define BITPACK_ENUM_QTYPE_OP(b, t, op, v)                                     \
	BITPACK_ENUM_OP(BITPACK_BY_QTYPE(b, t), op, v)

/// A fusion of .view<>() and BITPACK_ENUM_OP
#define BITPACK_ENUM_UTYPE_OP(b, t, op, v) BITPACK_ENUM_OP((b).view<t>(), op, v)

/// Compare a `Numeric` `Field::View` against a numeric value.
#define BITPACK_NUMERIC_OP(view, op, value)                                    \
	({                                                                         \
		using F = decltype(view)::Field::Type;                                 \
		static_cast<F::NumericType>(static_cast<F>(                            \
		  view)) /* NOLINTNEXTLINE(bugprone-macro-parentheses) */              \
		  op static_cast<F::NumericType>(value);                               \
	})

/// A fusion of BITPACK_BY_QTYPE and BITPACK_NUMERIC_OP
#define BITPACK_NUMERIC_QTYPE_OP(b, t, op, v)                                  \
	BITPACK_NUMERIC_OP(BITPACK_BY_QTYPE(b, t), op, v)

/// A fusion of .view<>() and BITPACK_NUMERIC_OP
#define BITPACK_NUMERIC_UTYPE_OP(b, t, op, v)                                  \
	BITPACK_NUMERIC_OP((b).view<t>(), op, v)

/**
 * Given a view of an enum-valued field in a bitpack, assign its value with the
 * the field type's enum's values into scope while computing the value.
 */
#define BITPACK_ENUM_ASSIGN(view, value) BITPACK_ENUM_OP(view, =, value)

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
#define BITPACK_ENUM_WITH(view, value) BITPACK_ENUM_OP(view, .with, value)

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
		struct T                                                               \
		{                                                                      \
			char t;                                                            \
		};                                                                     \
		struct U                                                               \
		{                                                                      \
			char u[2];                                                         \
		};                                                                     \
		U CHERIOT_EVAL0(T);                                                    \
		return sizeof(T);                                                      \
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
