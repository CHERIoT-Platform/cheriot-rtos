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
	TEST(strcmp(buffer, "42") == 0,
	     "snprintf(\"%d\", 42) gave {}",
	     std::string_view{buffer, BufferSize});
	snprintf(buffer, BufferSize, "%d", -42);
	TEST(strcmp(buffer, "-42") == 0,
	     "snprintf(\"%d\", -42) gave {}",
	     std::string_view{buffer, BufferSize});
	return 0;
}
