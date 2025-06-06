#include <stdarg.h>

#define TEST_NAME "Softfloat"
#include "stack_tests.h"
#include "tests.hh"

/**
 * Tests for the softfloat integration.
 *
 * These tests are *not* checking that softfloat in any way conforms to IEEE
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
		T          result = op(y, 2);
		debug_log("4.5 {} 2 = {}", opname, result);
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
		test_op<T, std::less>("<");
		test_op<T, std::less_equal>("<=");
		test_op<T, std::greater>(">");
		test_op<T, std::greater_equal>(">=");
		test_op<T, std::equal_to>("==");
		test_op<T, std::not_equal_to>("!=");
		test_conv<T, int>();
		test_conv<T, long>();
		test_conv<T, unsigned int>();
		test_conv<T, unsigned long>();
	}
} // namespace

/**
 * Regression test for compiler bug with floats and varargs.
 * See https://github.com/CHERIoT-Platform/llvm-project/pull/165
 * The bug depends a bit on stack alignment so try a couple of different
 * argument types.
 */
double test_varargs(int c, ...)
{
	va_list ap;
	va_start(ap, c);
	switch (c)
	{
		case 'i':
			va_arg(ap, int);
			break;
		case 'l':
			va_arg(ap, long long);
			break;
	}
	double f = va_arg(ap, double);
	va_end(ap);
	return f;
}

int test_softfloat()
{
	debug_log("Testing float");
	test<float>();
	debug_log("Testing double");
	test<double>();
	TEST_EQUAL(test_varargs('i', 1, 2.5), 2.5, "varargs i failed");
	TEST_EQUAL(test_varargs('l', 1ll, -1.0), -1.0, "varargs l failed");
	return 0;
}
