// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
/**
 * \file
 * \brief Helpers for exposing field offsets and structure sizes into
 * assembly.
 *
 * This file provides macros that should be included in both C++ and assembly
 * files. In C++, they will check (at compile time) that the values are correct
 * and print a helpful error message if they do not.  In assembly, they will
 * simply expose the values as assembler symbol definitions.
 */

#ifdef __cplusplus
/**
 * Helper class for checking the size of a structure.  Used in static
 * asserts so that the expected size shows up in compiler error messages.
 */
template<auto Real, auto Expected>
struct CheckSize
{
	static constexpr bool Value = Real == Expected;
};

/**
 * Export a macro into assembly named `name` with value `value`.  In C++, this
 * macro will report an error if the provided value does not equal the constexpr
 * evaluation of `expression`.
 */
#	define EXPORT_ASSEMBLY_NAME(name, val)                                    \
		static_assert(CheckSize<name, val>::Value,                             \
		              "Value provided for assembly is incorrect");

/**
 * Export a macro into assembly named `name` with value `value`.  In C++, this
 * macro will report an error if the provided value does not equal the constexpr
 * evaluation of `expression`.
 */
#	define EXPORT_ASSEMBLY_EXPRESSION(name, expression, val)                  \
		static constexpr size_t name = expression;                             \
		static_assert(CheckSize<name, val>::Value,                             \
		              "Value provided for assembly is incorrect");

/**
 * Export a macro into assembly of the form `{structure}_offset_{field}`.  The
 * value of this macro will be `value`.  In C++, this macro will report an error
 * if the provided value does not match the compiler's understanding of the
 * field offset.  The error message will contain the correct value.
 *
 * This macros also defines a static constexpr value of the same name as the
 * assembly definition.
 */
#	define EXPORT_ASSEMBLY_OFFSET(structure, field, val)                      \
		static constexpr size_t structure##_offset_##field = val;              \
		static_assert(CheckSize<offsetof(structure, field), val>::Value,       \
		              "Offset provided for assembly is incorrect");

/**
 * Variant of `EXPORT_ASSEMBLY_OFFSET` that can be used to specify the name of
 * the exported offset.  This is useful for providing the offset of fields in
 * nested structures when assembly code does not need to know the shape of the
 * overall structure, only the offset of various fields.
 *
 * This macros also defines a static constexpr value of the same name as the
 * assembly definition.
 */
#	define EXPORT_ASSEMBLY_OFFSET_NAMED(structure, field, value, name)        \
		static constexpr size_t name = value;                                  \
		EXPORT_ASSEMBLY_OFFSET(structure, field, value)
/**
 * Export a macro into assembly of the form `{structure}_size`.  The value of
 * this macro will be `value`.  In C++, this macro will report an error if the
 * provided value does not match the compiler's understanding of the structure
 * size.  The error message will contain the correct value.
 *
 * This macros also defines a static constexpr value of the same name as the
 * assembly definition.
 */
#	define EXPORT_ASSEMBLY_SIZE(structure, val)                               \
		static constexpr size_t structure##_size = val;                        \
		static_assert(CheckSize<sizeof(structure), val>::Value,                \
		              "Size provided for assembly is incorrect");

/**
 * Export a macro into assembly of the form
 * `{structure}_{structMember}_mask_{memberField}` which can be used to extract
 * `memberField` from a Bitpack of `structure::structMember` using bitwise AND.
 * The value of this macro will be `value`, which must be the raw value of the
 * mask of the `memberField` within the Bitpack named `structMember` within the
 * larger aggregate type `structure`.  In C++, this macro will report an error
 * if the provided `value` is not equal to the compiler's evaluation of
 * `BITPACK_FIELD_MASK_DECLTYPE(structure{}.structMember, memberField).raw()`.
 */
#	define EXPORT_ASSEMBLY_BITPACK_FIELD_MASK(                                \
	  structure, structMember, memberField, value)                             \
		static constexpr decltype(structure::structMember)::Storage            \
		  structure##_##structMember##_mask_##memberField = value;             \
		static_assert((value) == []() {                                        \
			return BITPACK_FIELD_MASK_DECLTYPE(                                \
			         decltype(structure::structMember){}, memberField)         \
			  .raw();                                                          \
		}());

/**
 * Export a macro into assembly of the form
 * `{structure}_{structMember}_shift_{memberField}` .  The value of this macro
 * will be `value`, which must be the 0-based index of the least significant bit
 * occupied by the `memberField` within the Bitpack named `structMember` within
 * the larger aggregate type `structure`.  In C++, this macro will report an
 * error if the provided value is not equal to the compiler's evaluation of
 * `BITPACK_FIELD_SHIFT_DECLTYPE(structure{}.structMember, memberField)`.
 */

#	define EXPORT_ASSEMBLY_BITPACK_FIELD_SHIFT(                               \
	  structure, structMember, memberField, value)                             \
		static constexpr size_t                                                \
		  structure##_##structMember##_shift_##memberField = value;            \
		static_assert((value) == []() {                                        \
			return BITPACK_FIELD_SHIFT_DECLTYPE(                               \
			  decltype(structure::structMember){}, memberField);               \
		}());

/**
 * Export a macro into assembly of the form
 * `{structure}_{structMember}_{memberField}_{fieldValue}` .  The value of this
 * macro will be `value`, which must be the raw value of a Bitpack (of the same
 * type as `structMember` within the larger aggregate type `structure`) holding
 * the value `fieldValue` in its member of type `memberField`.  In C++, this
 * macro will report an error if the provided `value` is not equal to the
 * compiler's evaluation of `BITPACK_WITH_WRAP_DECLTYPE(
 *  structure{}.structMember, memberField, fieldValue).raw()`.
 */

#	define EXPORT_ASSEMBLY_BITPACK_FIELD_WRAP(                                \
	  structure, structMember, memberField, fieldValue, value)                 \
		static constexpr decltype(structure::structMember)::Storage            \
		  structure##_##structMember##_##memberField##_##fieldValue = value;   \
		static_assert((value) == []() {                                        \
			return BITPACK_WITH_WRAP_DECLTYPE(                                 \
			         decltype(structure::structMember){},                      \
			         memberField,                                              \
			         fieldValue)                                               \
			  .raw();                                                          \
		}());

/**
 * Export a macro into assembly of the form
 * `{structure}_{structMember}_{memberField}_{fieldValue}` .  The value of this
 * macro will be `value`, which must be the raw value of a Bitpack (of the same
 * type as `structMember` within the larger aggregate type `structure`) holding
 * the value `fieldValue` in its member of type `memberField`.  In C++, this
 * macro will report an error if the provided `value` is not equal to the
 * compiler's evaluation of `BITPACK_WITH_QUALIFY_DECLTYPE(
 *  structure{}.structMember, memberField, fieldValue).raw()`.
 */
#	define EXPORT_ASSEMBLY_BITPACK_FIELD_QUALIFY(                             \
	  structure, structMember, memberField, fieldValue, value)                 \
		static constexpr decltype(structure::structMember)::Storage            \
		  structure##_##structMember##_##memberField##_##fieldValue = value;   \
		static_assert((value) == []() {                                        \
			return BITPACK_WITH_QUALIFY_DECLTYPE(                              \
			         decltype(structure::structMember){},                      \
			         memberField,                                              \
			         fieldValue)                                               \
			  .raw();                                                          \
		}());

/**
 * A wrapper of `EXPORT_ASSEMBLY_BITPACK_FIELD_WRAP` that, in C++, also asserts
 * that the given `value` is the mask value of the `memberField` field (that is,
 * the field value with all bits set to 1).
 *
 * This additional assertion allows for micro-optimizations, such as replacing
 * `(v & ~mask) | value` with `v | value` or `(v & mask) == value` with `(v &
 * value) == value`.   For single-bit fields, `(v & mask) == value` may be
 * replaced with `(v & value) != 0`.
 */
#	define EXPORT_ASSEMBLY_BITPACK_FIELD_IS_MASK_WRAP(                        \
	  structure, structMember, memberField, fieldValue, value)                 \
		EXPORT_ASSEMBLY_BITPACK_FIELD_WRAP(                                    \
		  structure, structMember, memberField, fieldValue, value)             \
		static_assert((value) == []() {                                        \
			return BITPACK_FIELD_MASK_DECLTYPE(                                \
			         decltype(structure::structMember){}, memberField)         \
			  .raw();                                                          \
		}());

/**
 * A wrapper of `EXPORT_ASSEMBLY_BITPACK_FIELD_QUALIFY` that, in C++, also
 * asserts that the given `value` is the mask value of the `memberField` field
 * (that is, the field value with all bits set to 1).
 *
 * See `EXPORT_ASSEMBLY_BITPACK_FIELD_IS_MASK_WRAP`.
 */
#	define EXPORT_ASSEMBLY_BITPACK_FIELD_IS_MASK_QUALIFY(                     \
	  structure, structMember, memberField, fieldValue, value)                 \
		EXPORT_ASSEMBLY_BITPACK_FIELD_QUALIFY(                                 \
		  structure, structMember, memberField, fieldValue, value)             \
		static_assert((value) == []() {                                        \
			return BITPACK_FIELD_MASK_DECLTYPE(                                \
			         decltype(structure::structMember){}, memberField)         \
			  .raw();                                                          \
		}());

#elif defined(__ASSEMBLER__)
#	define EXPORT_ASSEMBLY_NAME(name, value) .set name, value
#	define EXPORT_ASSEMBLY_EXPRESSION(name, expression, value) .set name, value
#	define EXPORT_ASSEMBLY_OFFSET_NAMED(structure, memberField, value, name)  \
		.set name, value
#	define EXPORT_ASSEMBLY_OFFSET(structure, field, value)                    \
		.set structure##_offset_##field, value
#	define EXPORT_ASSEMBLY_SIZE(structure, value) .set structure##_size, value

#	define EXPORT_ASSEMBLY_BITPACK_FIELD_MASK(                                \
	  structure, structMember, memberField, value)                             \
		.set structure##_##structMember##_mask_##memberField, value
#	define EXPORT_ASSEMBLY_BITPACK_FIELD_SHIFT(                               \
	  structure, structMember, memberField, value)                             \
		.set structure##_##structMember##_shift_##memberField, value
#	define EXPORT_ASSEMBLY_BITPACK_FIELD_WRAP(                                \
	  structure, structMember, memberField, fieldValue, value)                 \
		.set structure##_##structMember##_##memberField##_##fieldValue, value
#	define EXPORT_ASSEMBLY_BITPACK_FIELD_QUALIFY(                             \
	  structure, structMember, memberField, fieldValue, value)                 \
		.set structure##_##structMember##_##memberField##_##fieldValue, value
#	define EXPORT_ASSEMBLY_BITPACK_FIELD_IS_MASK_WRAP(                        \
	  structure, structMember, memberField, fieldValue, value)                 \
		.set structure##_##structMember##_##memberField##_##fieldValue, value
#	define EXPORT_ASSEMBLY_BITPACK_FIELD_IS_MASK_QUALIFY(                     \
	  structure, structMember, memberField, fieldValue, value)                 \
		.set structure##_##structMember##_##memberField##_##fieldValue, value
#else
#	define EXPORT_ASSEMBLY_NAME(name, value)
#	define EXPORT_ASSEMBLY_EXPRESSION(name, expression, value)
#	define EXPORT_ASSEMBLY_OFFSET(structure, field, value)
#	define EXPORT_ASSEMBLY_SIZE(structure, name, value)
#	define EXPORT_ASSEMBLY_OFFSET_NAMED(structure, field, value, name)

#	define EXPORT_ASSEMBLY_BITPACK_FIELD_MASK(                                \
	  structure, structMember, memberField, value)
#	define EXPORT_ASSEMBLY_BITPACK_FIELD_SHIFT(                               \
	  structure, structMember, memberField, value)
#	define EXPORT_ASSEMBLY_BITPACK_FIELD_WRAP(                                \
	  structure, structMember, memberField, fieldValue, value)
#	define EXPORT_ASSEMBLY_BITPACK_FIELD_QUALIFY(                             \
	  structure, structMember, memberField, fieldValue, value)
#	define EXPORT_ASSEMBLY_BITPACK_FIELD_IS_MASK_WRAP(                        \
	  structure, structMember, memberField, fieldValue, value)
#endif
