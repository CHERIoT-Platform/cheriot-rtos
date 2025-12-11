// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstdio>
#include <debug.hh>
#include "cheri.h"
#include "cheri.hh"
#include <fail-simulator-on-error.h>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Hello world compartment">;

extern "C" int __call_rust();

extern "C" void __rust_free(void *ptr)
{
	free(ptr);
}

extern "C" void *__rust_alloc(int size)
{
	return malloc(size);
}

extern "C" void print(char *buf)
{
	printf("%s", buf);
}

int __cheri_compartment("hello") say_hello()
{
	Debug::log("calling rust..");
	__call_rust();
	Debug::log("called rust!");
	return 0;
}
