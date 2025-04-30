#include <cstdio>
#define TEST_NAME "stdio"
#include "tests.hh"
#include <stdio.h>

// The expected output for floats depends on config option
#ifdef CHERIOT_PRINT_DOUBLES
#	define DOUBLE_STRING "123.456"
#else
#	define DOUBLE_STRING "<float>"
#endif

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
	sprintf(buffer, "%f", 123.456);
	TEST_EQUAL(std::string_view(buffer),
	           DOUBLE_STRING,
	           "sprintf(\"%f\", 123.456) failed");
	sprintf(buffer, "%f %d", 123.456, 42);
	TEST_EQUAL(std::string_view(buffer),
	           DOUBLE_STRING " 42",
	           "sprintf(buffer, \"%f %d\", 123.456, 42) failed");
	return 0;
}
