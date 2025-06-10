#include <enum_utils.hh>

#include <magic_enum/magic_enum.hpp>
using namespace magic_enum::bitwise_operators;

enum class [[clang::flag_enum]] E1
{
	Flag0 = 1 << 0,
	Flag1 = 1 << 1,

	Field0Mask = 7 << 4,
	Field0Bit0 = 1 << 4,
	Field0Bit1 = 2 << 4,
	Field0Bit2 = 4 << 4,
	Field0Val0 = 0 << 4,
	Field0Val3 = 3 << 4,
	Field0Val6 = 6 << 4,
};

enum class E2
{
	X = 1 << 0
};

void test()
{
	static_assert(static_cast<int>(enum_or<E1>()) == 0);

	static_assert(enum_or(E1::Flag0) == E1::Flag0);

	static_assert(ENUM_OR(E1, Field0Bit0, Field0Bit1) == E1::Field0Val3);

	static_assert(ENUM_CLEAR_THEN_SET(E1::Field0Val3,
	                                  (Field0Bit0, Field0Bit1),
	                                  (Field0Bit1, Field0Bit2)) ==
	              E1::Field0Val6);
	static_assert(ENUM_CLEAR_THEN_SET(E1::Field0Val3, (Field0Bit0), ()) ==
	              E1::Field0Bit1);
	static_assert(ENUM_CLEAR_THEN_SET(E1::Field0Val3, (), (Field0Bit2)) ==
	              E1::Field0Mask);
	static_assert(ENUM_CLEAR_THEN_SET(E1::Field0Val3, (), ()) ==
	              E1::Field0Val3);

	constexpr auto E1v32 = ENUM_OR(E1, Flag1, Field0Val3);
	static_assert(static_cast<int>(E1v32) == 0x32);

	static_assert(static_cast<int>(ENUM_CLEAR_THEN_SET(
	                E1v32, (Flag1, Field0Bit0), (Flag0, Field0Bit2))) == 0x61);

	// Numerically, one can mix and match enum types...
	static_assert((static_cast<int>(E1::Flag1) | static_cast<int>(E2::X)) ==
	              0x3);

	/*
	 * However, types mean that this doesn't compile because X isn't brought
	 * into scope by the "using enum E1" in ENUM_OR.
	 */
	// static_assert(static_cast<int>(ENUM_OR(E1, Flag1, X)) == 0x3);

	/*
	 * Similarly, types mean that this doesn't compile because of the requires
	 * clause on enum_or; even if that weren't there, we would balk because we'd
	 * have inferred E to be E1, and we can't static_cast<E1>(E2::...).
	 */
	// static_assert(static_cast<int>(enum_or(E1::Flag1, E2::X)) == 0x3);

	constexpr auto Frob = [](E1 e) {
		ENUM_UPDATE_CLEAR_THEN_SET(e, (Flag1, Field0Bit1), (Flag0, Field0Bit2));
		return e;
	};
	static_assert(static_cast<int>(Frob(E1v32)) == 0x51);
	static_assert(static_cast<int>(Frob(
	                ENUM_CLEAR_THEN_SET(E1v32, (Field0Bit0), ()))) == 0x41);

	static_assert(ENUM_IS_SET(E1v32, Flag1));
	static_assert(!ENUM_IS_SET(E1v32, Flag0));
	static_assert(!ENUM_ARE_ALL_SET(E1v32, Flag0, Flag1));
	static_assert(ENUM_IS_ANY_SET(E1v32, Flag0, Flag1));

	static_assert(ENUM_MASKED_EQUAL(E1v32, Field0Mask, Field0Val3));
	static_assert(
	  ENUM_MASKED_EQUAL(E1v32, Field0Mask, (Field0Bit0 | Field0Bit1)));

	/*
	 * We can even use magic_enum's bitwise operators ourselves, and with the
	 * right "using enum" in scope that's reasonably pleasant...
	 */
	static_assert(
	  ENUM_MASKED_EQUAL(E1::Field0Val3, (Field0Bit0 | Field0Bit2), Field0Bit0));
	static_assert(
	  ENUM_MASKED_EQUAL(E1v32, Field0Mask, (Field0Bit0 | Field0Bit1)));
	/*
	 * The only subtlety is that we need the first argument to be something
	 * syntactically understood by the caller, so we can't write this:
	 *
	 *   ENUM_MASKED_EQUAL((E1v32 | Field0Bit2), Field0Mask, Field0Mask)
	 *
	 * but instead have to write that with a sufficiently qualified Field0Bit2,
	 * thus:
	 */
	static_assert(
	  ENUM_MASKED_EQUAL((E1v32 | E1::Field0Bit2), Field0Mask, Field0Mask));
}
