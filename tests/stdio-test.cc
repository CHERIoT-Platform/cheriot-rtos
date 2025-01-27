#include <cstdio>
#define TEST_NAME "stdio"
#include "tests.hh"
#include <stdio.h>

int test_stdio()
{
	debug_log("Printing 'Hello, world!' to stdout");
	printf("Hello, world!\n");
	debug_log("Printing 'Hello, world!' to stderr");
	fprintf(stderr, "Hello, world!\n");
	const size_t BufferSize = 64;
	char         buffer[BufferSize];
	snprintf(buffer, BufferSize, "%d", 42);
	// Using std::string_view makes equality do string comparison
	TEST_EQUAL(std::string_view(buffer), "42", "snprintf(\"%d\", 42) failed");
	snprintf(buffer, BufferSize, "%d", -42);
	TEST_EQUAL(std::string_view(buffer), "-42", "snprintf(\"%d\", -42) failed");
	sprintf(buffer, "%x", 6 * 9);
	TEST_EQUAL(std::string_view(buffer), "36", "sprintf(\"%x\", 6 * 9) failed");
	return 0;
}
