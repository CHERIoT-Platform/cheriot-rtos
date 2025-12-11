#define MALLOC_QUOTA 0x100000
#define TEST_NAME "RUST"
#include "cheri.h"
#include "cheri.hh"
#include "fail-simulator-on-error.h"
#include <cstdlib>

using Test = ConditionalDebug<true, "RustRunner">;

template<typename... Args>
void debug_log(const char *fmt, Args... args)
{
	Test::log(fmt, std::forward<Args>(args)...);
}

#define TEST(cond, msg, ...) Test::Invariant((cond), msg, ##__VA_ARGS__)

/* Imports from Rust */
extern "C" int do_it(int);

/* Things that Rust expects from us */
extern "C" void cheriot_print_str(char *s)
{
	printf("%s", s);
}

extern "C" void *cheriot_alloc(size_t size)
{
	// debug_log("Trying to allocate {} bytes!", size);
	Timeout timeout{5};
	void   *ret = heap_allocate(&timeout, MALLOC_CAPABILITY, size);

	TEST(CHERI::Capability{ret}.is_valid(),
	     "Allocation is invalid, got pointer: {} -- {}",
	     ret,
	     (int)ret);
	return ret;
}

extern "C" void cheriot_free(void *ptr)
{
	free(ptr);
}

extern "C" void cheriot_panic()
{
	TEST(false, "Reached panic!");
}

unsigned short lfsr = 0xACE1u;
unsigned       bit;

extern "C" char cheriot_random_byte()
{
	bit         = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
	return lfsr = (lfsr >> 1) | (bit << 15);
}

int __attribute__((cheriot_compartment("run_rust"))) run_rust()
{
	Test::log("Calling Rust...");
	do_it(-10);
	Test::log("Called!");

	return 0;
}
