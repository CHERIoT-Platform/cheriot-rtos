// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
/**
 * Helpers for exposing field offsets and structure sizes into assembly.  This
 * file provides macros that should be included in both C++ and assembly files.
 * In C++, they will check (at compile time) that the values are correct and
 * print a helpful error message if they do not.  In assembly, they will simply
 * expose the values as assembler symbol definitions.
 */

#ifdef __cplusplus
/**
 * Helper class for checking the size of a structure.  Used in static
 * asserts so that the expected size shows up in compiler error messages.
 */
template<auto Real, auto Expected>
struct CheckSize
{
	static constexpr bool value = Real == Expected;
};

/**
 * Export a macro into assembly named `name` with value `value`.  In C++, this
 * macro will report an error if the provided value does not equal the constexpr
 * evaluation of `expression`.
 */
#	define EXPORT_ASSEMBLY_NAME(name, val)                                    \
		static_assert(CheckSize<name, val>::value,                             \
		              "Value provided for assembly is incorrect");

/**
 * Export a macro into assembly named `name` with value `value`.  In C++, this
 * macro will report an error if the provided value does not equal the constexpr
 * evaluation of `expression`.
 */
#	define EXPORT_ASSEMBLY_EXPRESSION(name, expression, val)                  \
		static constexpr size_t name = expression;                             \
		static_assert(CheckSize<name, val>::value,                             \
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
		static_assert(CheckSize<offsetof(structure, field), val>::value,       \
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
		static_assert(CheckSize<sizeof(structure), val>::value,                \
		              "Size provided for assembly is incorrect");
#elif defined(__ASSEMBLER__)
#	define EXPORT_ASSEMBLY_NAME(name, value)                                  \
		.set name, value
#	define EXPORT_ASSEMBLY_EXPRESSION(name, expression, value)                \
		.set name, value
#	define EXPORT_ASSEMBLY_OFFSET_NAMED(structure, field, value, name)        \
		.set name, value
#	define EXPORT_ASSEMBLY_OFFSET(structure, field, value)                    \
		.set structure##_offset_##field, value
#	define EXPORT_ASSEMBLY_SIZE(structure, value) .set structure##_size, value
#else
#	define EXPORT_ASSEMBLY_NAME(name, value)
#	define EXPORT_ASSEMBLY_EXPRESSION(name, expression, value)
#	define EXPORT_ASSEMBLY_OFFSET(structure, field, name, value)
#	define EXPORT_ASSEMBLY_SIZE(structure, name, value)
#	define EXPORT_ASSEMBLY_OFFSET_NAMED(structure, field, value, name)
#endif
