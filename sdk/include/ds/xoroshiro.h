// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT
//
// Imported from snmalloc 4f9d991449380ed7d881a25ba02cc5668c1ff394.

#pragma once

#include <cstdint>
#include <cstdlib>
#include <debug.hh>
#include <initializer_list>

namespace ds::xoroshiro
{

	namespace detail
	{
		using Debug = ConditionalDebug<false, "xoroshiro">;

		/**
		 * The xoroshiro+ (not ++) generator(s) of Blackman and Vigna's
		 * Scrambled Linear Pseudorandom Number Generators
		 * (https://arxiv.org/abs/1805.01407, Figure 1).
		 *
		 * This spelling is parameterized on the type of the two state
		 * variables used internally (State), the type of each sample (Result),
		 * and the seed state values (A, B, C).
		 */
		template<typename State,
		         typename Result,
		         State A,
		         State B,
		         State C,
		         State Jump0     = 0,
		         State Jump1     = 0,
		         State LongJump0 = 0,
		         State LongJump1 = 0>
		class XorOshiro
		{
			private:
			static constexpr unsigned StateBits  = 8 * sizeof(State);
			static constexpr unsigned ResultBits = 8 * sizeof(Result);

			static_assert(StateBits >= ResultBits,
			              "State must have at least as many bits as Result");

			/* Parameters must be valid shifts */
			static_assert(0 <= A && A <= StateBits);
			static_assert(0 <= B && B <= StateBits);
			static_assert(0 <= C && C <= StateBits);

			State x;
			State y;

			static inline State rotl(State x, State k)
			{
				return (x << k) | (x >> (StateBits - k));
			}

			void jump(State jump0, State jump1)
			{
				const State Jump[] = {jump0, jump1};
				uint64_t    s0     = 0;
				uint64_t    s1     = 0;
				for (int i : {0, 1})
				{
					for (int b = 0; b < 64; b++)
					{
						if (Jump[i] & static_cast<uint64_t>(1) << b)
						{
							s0 ^= x;
							s1 ^= y;
						}
						next();
					}
				}
				x = s0;
				y = s1;
			}

			public:
			XorOshiro(State x = 5489, State y = 0) : x(x), y(y)
			{
				// If both zero, then this does not work
				Debug::Invariant((x != 0) || (y != 0),
				                 "Invalid state, both x and y are zero");

				next();
			}

			void set_state(State nx, State ny = 0)
			{
				// If both zero, then this does not work
				Debug::Invariant((nx != 0) || (ny != 0),
				                 "Invalid state, both nx and ny are zero");

				x = nx;
				y = ny;
				next();
			}

			Result next()
			{
				State oldX = x;
				State oldY = y;
				State r    = x + y;
				y ^= x;
				x = rotl(x, A) ^ y ^ (y << B);
				y = rotl(y, C);
				// If both zero, then this does not work
				Debug::Invariant((x != 0) || (y != 0),
				                 "Invalid state, both x and y are zero after "
				                 "next() with x: {}, y: {}",
				                 oldX,
				                 oldY);
				return r >> (StateBits - ResultBits);
			}

			Result operator()()
			{
				return next();
			}

			/**
			 * Jump.  If supported, this is equivalent to 2^64 calls to next().
			 */
			void jump()
			    requires(Jump0 != 0) && (Jump1 != 0)
			{
				jump(Jump0, Jump1);
			}

			/**
			 * Jump a *really* long way.  If supported, this is equivalent to
			 * 2^96 calls to next().
			 */
			void long_jump()
			    requires(LongJump0 != 0) && (LongJump1 != 0)
			{
				jump(LongJump0, LongJump1);
			}
		};
	} // namespace detail

	/**
	 * xoroshiro128+ with 64-bit output using the 2018 parameters.
	 *
	 * Parameters (including jump parameters) from:
	 * https://prng.di.unimi.it/xoroshiro128plus.c
	 */
	using P128R64 = detail::XorOshiro<uint64_t,
	                                  uint64_t,
	                                  24,
	                                  16,
	                                  37,
	                                  0xdf900294d8f554a5,
	                                  0x170865df4b3201fc,
	                                  0xd2a98b26625eee7b,
	                                  0xdddf9b1090aa7ac1>;

	/**
	 * xoroshiro128+ with 32-bit output using the 2018 parameters.
	 *
	 * Parameters as per P128R64, above.
	 */
	using P128R32 = detail::XorOshiro<uint64_t, uint32_t, 24, 16, 37>;

	/**
	 * A "xoroshiro64+" with 32-bit outputs.
	 *
	 * As per Sebastino Vigna himself, writing in
	 * https://groups.google.com/g/prng/c/Ll-KDIbpO8k/m/bfHK4FlUCwAJ, these
	 * parameters are one of may full-period triples.
	 */
	using P64R32 = detail::XorOshiro<uint32_t, uint32_t, 27, 7, 20>;

	/**
	 * A "xoroshiro64+" with 16-bit outputs.
	 *
	 * Parameters as per P64R32, above.
	 */
	using P64R16 = detail::XorOshiro<uint32_t, uint16_t, 27, 7, 20>;

	/**
	 * A "xoroshiro32+" with 16-bit outputs.
	 *
	 * Parameters as per the Parallax Propeller 2 PRNG (discarding the "++"
	 * variant's fourth parameter "R").  See
	 * https://forums.parallax.com/discussion/comment/1448894 .
	 */
	using P32R16 = detail::XorOshiro<uint16_t, uint16_t, 13, 5, 10>;

	/**
	 * A "xoroshiro32+" with 16-bit outputs.
	 *
	 * Parameters as per P32R16, above.
	 */
	using P32R8 = detail::XorOshiro<uint16_t, uint8_t, 13, 5, 10>;

	/**
	 * A "xoroshiro16+" with 8-bit outputs.
	 *
	 * As per Sebastino Vigna himself, writing in
	 * https://groups.google.com/g/prng/c/MWJjq11zRis/m/7nM_cwRzAQAJ , the
	 * parameters are one of may full-period triples, but "There are no good
	 * 16-bit generators".
	 */
	using P16R8 = detail::XorOshiro<uint8_t, uint8_t, 4, 7, 3>;
} // namespace ds::xoroshiro
