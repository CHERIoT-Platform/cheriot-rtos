#include <cancellation.h>
#include <errno.h>
#define TEST_NAME "Cancellation token"
#include "tests.hh"
#include <fail-simulator-on-error.h>

int test_cancellation()
{
	TEST_EQUAL(cancellation_token_check(nullptr),
	           -EINVAL,
	           "Checking an invalid token failed to report an error");
	Timeout t{1};
	auto    source = cancellation_token_source_create(&t, MALLOC_CAPABILITY);
	TEST(cheri_tag_get(source),
	     "cancellation_token_source_create returned an untagged capability");
	TEST(cheri_type_get(source) != 0,
	     "cancellation_token_source_create returned an unsealed capability");
	auto token = cancellation_token_get(source);
	TEST(cheri_tag_get(token),
	     "cancellation_token_get returned an untagged capability");
	TEST_EQUAL(cheri_type_get(token),
	           0,
	           "cancellation_token_get returned a sealed capability");

	TEST_EQUAL(cancellation_token_check(token),
	           0,
	           "Checking an unsignalled token should report unsignalled");

	TEST_EQUAL(cancellation_token_signal(source), 0, "Signalling token failed");

	TEST_EQUAL(cancellation_token_check(token),
	           1,
	           "Checking a signalled token should report signalled");

	TEST_EQUAL(cancellation_token_source_destroy(source, MALLOC_CAPABILITY),
	           0,
	           "Failed to destroy cancellation-token source");
	TEST_EQUAL(cancellation_token_check(token),
	           -EINVAL,
	           "Checking a deallocated token should report invalid");
	return 0;
}
