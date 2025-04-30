#pragma once
// Copyright Benoit Blanchon and CHERIoT Contributors.
// SPDX-License-Identifier: MIT
//
// This code is derived from the ArduinoJson project:
// Copyright Benoit Blanchon 2014-2017
// MIT License

/**
 * This file provides routines to help convert from floating-point values to
 * strings.
 *
 * This uses the algorithm described here:
 *
 * https://blog.benoitblanchon.fr/lightweight-float-to-string/
 *
 * This approach is optimised for code size, not performance or accuracy.  It
 * will give more rounding errors than common implementations but is a fraction
 * of the code size.
 */

#include <array>
#include <type_traits>

namespace
{
	/**
	 * Number of powers of ten required to represent all values that fit in any
	 * supported floating-point type.  9 is sufficient for IEEE 754 64-bit
	 * floating-point values.  Currently, CHERIoT does not provide a long
	 * double type.
	 */
	constexpr size_t NumberOfPowersOfTen = 9;

	/**
	 * Helper that computes the necessary value of `NumberOfPowersOfTen` for a
	 * given type.
	 */
	template<typename T>
	constexpr size_t NecessaryNumberOfPowersOfTen = []() {
		return 32 - __builtin_clz(std::numeric_limits<T>::max_exponent10);
	}();

	/**
	 * Set of supported floating-point types.
	 *
	 * If new types are added to the allowed list, the
	 * `NecessaryNumberOfPowersOfTen` part of the expression must also be
	 * updated.
	 */
	template<typename T>
	concept SupportedFloatingPointType = requires() {
		requires((std::is_same_v<T, float> || std::is_same_v<T, double>) &&
		         (NecessaryNumberOfPowersOfTen<double> <= NumberOfPowersOfTen));
	};

	/**
	 * Table of powers of ten.  These are expensive to compute dynamically.
	 */
	template<typename T>
	constexpr std::array<T, NumberOfPowersOfTen> PositiveBinaryPowersOfTen = {
	  T(1e1),
	  T(1e2),
	  T(1e4),
	  T(1e8),
	  T(1e16),
	  T(1e32),
	  T(1e64),
	  T(1e128),
	  T(1e256)};

	/**
	 * Table of negative powers of ten.  These are expensive to compute
	 * dynamically.
	 */
	template<typename T>
	constexpr std::array<T, NumberOfPowersOfTen> NegativeBinaryPowersOfTen = {
	  T(1e-1),
	  T(1e-2),
	  T(1e-4),
	  T(1e-8),
	  T(1e-16),
	  T(1e-32),
	  T(1e-64),
	  T(1e-128),
	  T(1e-256)};

	/// Largest value that is printed without an exponent
	template<typename T>
	constexpr T PositiveExponentiationThreshold = 1e7;

	/// Smallest value that is printed without an exponent
	template<typename T>
	constexpr T NegativeExponentiationThreshold = 1e-5;

	/**
	 * Scale the floating-point value up or down in powers of ten so that the
	 * decimal point will end up in a sensible place.
	 *
	 * The return value is the number of powers of ten (positive or negative)
	 * that the value was scaled by.
	 *
	 * This uses binary exponentiation, see the blog post linked at the top of
	 * the file for more details.
	 */
	template<SupportedFloatingPointType T>
	inline int normalize(T &value)
	{
		int powersOf10 = 0;

		int8_t index = NecessaryNumberOfPowersOfTen<T> - 1;
		int    bit   = 1 << index;

		if (value >= PositiveExponentiationThreshold<T>)
		{
			for (; index >= 0; index--)
			{
				if (value >= PositiveBinaryPowersOfTen<T>[index])
				{
					value *= NegativeBinaryPowersOfTen<T>[index];
					powersOf10 = (powersOf10 + bit);
				}
				bit >>= 1;
			}
		}

		if (value > 0 && value <= NegativeExponentiationThreshold<T>)
		{
			for (; index >= 0; index--)
			{
				if (value < NegativeBinaryPowersOfTen<T>[index] * 10)
				{
					value *= PositiveBinaryPowersOfTen<T>[index];
					powersOf10 = (powersOf10 - bit);
				}
				bit >>= 1;
			}
		}

		return powersOf10;
	}

	/**
	 * A floating-point value decomposed into components for printing.
	 */
	struct FloatParts
	{
		/// The integral component (before the decimal point).
		uint32_t integral;
		/// The decimal component (after the decimal point).
		uint32_t decimal;
		/// The exponent (the power of ten that this is raised to).
		int16_t exponent;
		/**
		 * The number of decimal places in the exponent.  The exponent is
		 * represented as a value that can be converted to a string with
		 * integer-to-string conversion.  This may have leading zeroes.  For
		 * example, the number 123.045 will have an `integral` value of 123, a
		 * `decimal` value of 45, an `exponent` of 1, and a `decimalPlaces` of
		 * 3.  The value 45 must therefore be printed with a leading zero.
		 */
		int8_t decimalPlaces;
	};

	constexpr uint32_t pow10(int exponent)
	{
		return (exponent == 0) ? 1 : 10 * pow10(exponent - 1);
	}

	template<SupportedFloatingPointType T>
	FloatParts decompose_float(T value, int8_t decimalPlaces)
	{
		uint32_t maxDecimalPart = pow10(decimalPlaces);

		int exponent = normalize(value);

		uint32_t integral = static_cast<uint32_t>(value);
		// reduce number of decimal places by the number of integral places
		for (uint32_t tmp = integral; tmp >= 10; tmp /= 10)
		{
			maxDecimalPart /= 10;
			decimalPlaces--;
		}

		T remainder = (value - T(integral)) * T(maxDecimalPart);

		uint32_t decimal = uint32_t(remainder);
		remainder        = remainder - T(decimal);

		// rounding:
		// increment by 1 if remainder >= 0.5
		decimal += uint32_t(remainder * 2);
		if (decimal >= maxDecimalPart)
		{
			decimal = 0;
			integral++;
			if (exponent && integral >= 10)
			{
				exponent++;
				integral = 1;
			}
		}

		// remove trailing zeros
		while (decimal % 10 == 0 && decimalPlaces > 0)
		{
			decimal /= 10;
			decimalPlaces--;
		}

		return {
		  integral, decimal, static_cast<int16_t>(exponent), decimalPlaces};
	}

} // namespace
