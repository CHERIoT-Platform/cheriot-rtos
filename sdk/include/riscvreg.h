// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

#define GR_READ(name)                                                          \
	({                                                                         \
		size_t val;                                                            \
		__asm __volatile("mv %0, " #name : "=r"(val));                         \
		val;                                                                   \
	})

#define GR_WRITE(name, val)                                                    \
	({ __asm __volatile("mv " #name ", %0" ::"r"(val)); })

#define CSR_READ64(csr)                                                        \
	({                                                                         \
		uint64_t val;                                                          \
		size_t   high, low;                                                    \
		__asm __volatile("1: "                                                 \
		                 "csrr t0, " #csr "h\n"                                \
		                 "csrr %0, " #csr "\n"                                 \
		                 "csrr %1, " #csr "h\n"                                \
		                 "bne t0, %1, 1b"                                      \
		                 : "=r"(low), "=r"(high)                               \
		                 :                                                     \
		                 : "t0");                                              \
		val = (low | ((uint64_t)high << 32));                                  \
		val;                                                                   \
	})

// This hack reads the absolute integer address of a symbol.
#define LA_ABS(symbol)                                                         \
	({                                                                         \
		size_t val;                                                            \
		__asm __volatile("lui %0, %%hi(" #symbol ")\n"                         \
		                 "addi %0, %0, %%lo(" #symbol ")"                      \
		                 : "=r"(val));                                         \
		val;                                                                   \
	})

#define BARRIER() __asm volatile("" : : : "memory")
