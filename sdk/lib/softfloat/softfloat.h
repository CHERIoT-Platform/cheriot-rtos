#pragma once

#include <cdefs.h>

#include "../../third_party/compiler-rt/int_lib.h"
#undef COMPILER_RT_ALIAS
#define COMPILER_RT_ALIAS(name, aliasname)                                                 \
	__asm__(".globl " SYMBOL_NAME(__library_export_libcalls_##aliasname) "\n" SYMBOL_NAME( \
	  __library_export_libcalls_##aliasname) " = " SYMBOL_NAME(__library_export_libcalls_##name));

#define DECLARE_OP(op)                                                         \
	CHERIOT_DECLARE_STANDARD_LIBCALL(__##op##sf3, float, float, float)         \
	CHERIOT_DECLARE_STANDARD_LIBCALL(__##op##df3, double, double, double)

#define DECLARE_CMP(op)                                                        \
	CHERIOT_DECLARE_STANDARD_LIBCALL(__##op##sf2, long, float, float)          \
	CHERIOT_DECLARE_STANDARD_LIBCALL(__##op##df2, long, double, double)

DECLARE_OP(add)
DECLARE_OP(sub)
DECLARE_OP(mul)
DECLARE_OP(div)

// Negation
CHERIOT_DECLARE_STANDARD_LIBCALL(__negsf2, float, float)
CHERIOT_DECLARE_STANDARD_LIBCALL(__negdf2, double, double)

// Float <-> double conversion
CHERIOT_DECLARE_STANDARD_LIBCALL(__extendsfdf2, double, float)
CHERIOT_DECLARE_STANDARD_LIBCALL(__truncdfsf2, float, double)

// Floating-point <-> integer conversion
CHERIOT_DECLARE_STANDARD_LIBCALL(__fixsfsi, int, float)
CHERIOT_DECLARE_STANDARD_LIBCALL(__fixdfsi, int, double)
CHERIOT_DECLARE_STANDARD_LIBCALL(__fixsfdi, long long, float)
CHERIOT_DECLARE_STANDARD_LIBCALL(__fixdfdi, long long, double)
CHERIOT_DECLARE_STANDARD_LIBCALL(__fixunssfsi, unsigned int, float)
CHERIOT_DECLARE_STANDARD_LIBCALL(__fixunsdfsi, unsigned int, double)
// Note: compiler-rt's return types for these don't match the GCC
// documentation, which says that they should return long, not long long
CHERIOT_DECLARE_STANDARD_LIBCALL(__fixunssfdi, unsigned long long, float)
CHERIOT_DECLARE_STANDARD_LIBCALL(__fixunsdfdi, unsigned long long, double)

CHERIOT_DECLARE_STANDARD_LIBCALL(__floatsisf, float, int)
CHERIOT_DECLARE_STANDARD_LIBCALL(__floatsidf, double, int)
CHERIOT_DECLARE_STANDARD_LIBCALL(__floatdisf, float, long long)
CHERIOT_DECLARE_STANDARD_LIBCALL(__floatdidf, double, long long)
CHERIOT_DECLARE_STANDARD_LIBCALL(__floattisf, float, long long)
CHERIOT_DECLARE_STANDARD_LIBCALL(__floattidf, double, long long)

CHERIOT_DECLARE_STANDARD_LIBCALL(__floatunsisf, float, unsigned int)
CHERIOT_DECLARE_STANDARD_LIBCALL(__floatunsidf, double, unsigned int)
CHERIOT_DECLARE_STANDARD_LIBCALL(__floatundisf, float, unsigned long long)
CHERIOT_DECLARE_STANDARD_LIBCALL(__floatundidf, double, unsigned long long)
CHERIOT_DECLARE_STANDARD_LIBCALL(__floatuntisf, float, unsigned long long)
CHERIOT_DECLARE_STANDARD_LIBCALL(__floatuntidf, double, unsigned long long)

// Floating-point comparisons
DECLARE_CMP(cmp)
DECLARE_CMP(ord)
DECLARE_CMP(unord)
DECLARE_CMP(eq)
DECLARE_CMP(ge)
DECLARE_CMP(lt)
DECLARE_CMP(le)
DECLARE_CMP(gt)

// Power
CHERIOT_DECLARE_STANDARD_LIBCALL(__powisf2, float, float, int)
CHERIOT_DECLARE_STANDARD_LIBCALL(__powidf2, double, double, int)

// Complex multiply / divide
CHERIOT_DECLARE_STANDARD_LIBCALL(__mulsc3,
                                 _Complex float,
                                 float,
                                 float,
                                 float,
                                 float)
CHERIOT_DECLARE_STANDARD_LIBCALL(__muldc3,
                                 _Complex double,
                                 double,
                                 double,
                                 double,
                                 double)
CHERIOT_DECLARE_STANDARD_LIBCALL(__divsc3,
                                 _Complex float,
                                 float,
                                 float,
                                 float,
                                 float)
CHERIOT_DECLARE_STANDARD_LIBCALL(__divdc3,
                                 _Complex double,
                                 double,
                                 double,
                                 double,
                                 double)
