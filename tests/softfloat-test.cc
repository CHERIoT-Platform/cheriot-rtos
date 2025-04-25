#define TEST_NAME "Softfloat"
#include "stack_tests.h"
#include "tests.hh"

/**
 * Tests for the softfloat integration.
 *
 * These tests are *not* checking that softfloat in any way conforms to IEE
 * 754, they are checking that our *integration* of the upstream LLVM
 * soft-float library is correct (i.e. that the compiler inserts the correct
 * calls and gets plausible results).
 */

namespace
{
	template<typename T, template<typename> typename Op>
	void test_op(const char *opname)
	{
		volatile T x = 4.5;
		T          y = x;
		Op<T>      op;
		x = op(y, 2);
		debug_log("4.5 {} 2 = {}", opname, static_cast<int>(x));
	}

	template<typename T, typename I>
	void test_conv()
	{
		volatile T x = I(4);
		I          y = static_cast<I>(x);
		TEST_EQUAL(y, 4, "Conversion result mismatch");
		volatile T z = static_cast<T>(y);
		TEST_EQUAL(z, T(4), "Conversion result mismatch");
	}

	template<typename T>
	void test()
	{
		test_op<T, std::plus>("+");
		test_op<T, std::minus>("-");
		test_op<T, std::multiplies>("*");
		test_op<T, std::divides>("/");
		test_conv<T, int>();
		test_conv<T, long>();
		test_conv<T, unsigned int>();
		test_conv<T, unsigned long>();
	}
} // namespace

int test_softfloat()
{
	debug_log("Testing float");
	test<float>();
	debug_log("Testing double");
	test<double>();
	return 0;
}
