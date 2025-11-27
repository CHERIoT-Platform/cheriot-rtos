#pragma once
#include <compartment.h>


int __cheri_compartment("victim") guess_secret(uint64_t guess);
int __cheri_compartment("victim") insecure_memcpy(void* dest, const void* src, size_t n);
int __cheri_compartment("victim") secure_memcpy(void* dest, const void* src, size_t n);
