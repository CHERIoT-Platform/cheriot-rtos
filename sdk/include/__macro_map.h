/*
 * Created by William Swanson in 2012.
 *
 * I, William Swanson, dedicate this work to the public domain.
 * I waive all rights to the work worldwide under copyright law,
 * including all related and neighboring rights,
 * to the extent allowed by law.
 *
 * You can copy, modify, distribute and perform the work,
 * even for commercial purposes, all without asking permission.
 */
/*
 * The CHERIoT import is taken from https://github.com/swansontec/map-macro,
 * specifically, the commit with SHA-1 c5189e61f4b86975ff44fc45e798c7a7fdd5ad81
 * and lightly modified as follows:
 * - Added CHERIOT_ prefix to all macros to avoid namespace pollution
 * - Used pragma once instead of include guards
 * - Formatted to fit local conventions
 * - Removed the MAP_UD_I and MAP_LIST_UD_I variants (and the MAP_INC* helpers)
 */

#pragma once

#define CHERIOT_EVAL0(...) __VA_ARGS__
#define CHERIOT_EVAL1(...)                                                     \
	CHERIOT_EVAL0(CHERIOT_EVAL0(CHERIOT_EVAL0(__VA_ARGS__)))
#define CHERIOT_EVAL2(...)                                                     \
	CHERIOT_EVAL1(CHERIOT_EVAL1(CHERIOT_EVAL1(__VA_ARGS__)))
#define CHERIOT_EVAL3(...)                                                     \
	CHERIOT_EVAL2(CHERIOT_EVAL2(CHERIOT_EVAL2(__VA_ARGS__)))
#define CHERIOT_EVAL4(...)                                                     \
	CHERIOT_EVAL3(CHERIOT_EVAL3(CHERIOT_EVAL3(__VA_ARGS__)))
#define CHERIOT_EVAL(...)                                                      \
	CHERIOT_EVAL4(CHERIOT_EVAL4(CHERIOT_EVAL4(__VA_ARGS__)))

#define CHERIOT_MAP_END(...)
#define CHERIOT_MAP_OUT
#define CHERIOT_MAP_COMMA ,

#define CHERIOT_MAP_GET_END2() 0, CHERIOT_MAP_END
#define CHERIOT_MAP_GET_END1(...) CHERIOT_MAP_GET_END2
#define CHERIOT_MAP_GET_END(...) CHERIOT_MAP_GET_END1
#define CHERIOT_MAP_NEXT0(test, next, ...) next CHERIOT_MAP_OUT
#define CHERIOT_MAP_NEXT1(test, next) CHERIOT_MAP_NEXT0(test, next, 0)
#define CHERIOT_MAP_NEXT(test, next)                                           \
	CHERIOT_MAP_NEXT1(CHERIOT_MAP_GET_END test, next)

#define CHERIOT_MAP0(f, x, peek, ...)                                          \
	f(x) CHERIOT_MAP_NEXT(peek, CHERIOT_MAP1)(f, peek, __VA_ARGS__)
#define CHERIOT_MAP1(f, x, peek, ...)                                          \
	f(x) CHERIOT_MAP_NEXT(peek, CHERIOT_MAP0)(f, peek, __VA_ARGS__)

#define CHERIOT_MAP0_UD(f, ud, x, peek, ...)                                   \
	f(x, ud) CHERIOT_MAP_NEXT(peek, CHERIOT_MAP1_UD)(f, ud, peek, __VA_ARGS__)
#define CHERIOT_MAP1_UD(f, ud, x, peek, ...)                                   \
	f(x, ud) CHERIOT_MAP_NEXT(peek, CHERIOT_MAP0_UD)(f, ud, peek, __VA_ARGS__)

#define CHERIOT_MAP_LIST_NEXT1(test, next)                                     \
	CHERIOT_MAP_NEXT0(test, CHERIOT_MAP_COMMA next, 0)
#define CHERIOT_MAP_LIST_NEXT(test, next)                                      \
	CHERIOT_MAP_LIST_NEXT1(CHERIOT_MAP_GET_END test, next)

#define CHERIOT_MAP_LIST0(f, x, peek, ...)                                     \
	f(x) CHERIOT_MAP_LIST_NEXT(peek, CHERIOT_MAP_LIST1)(f, peek, __VA_ARGS__)
#define CHERIOT_MAP_LIST1(f, x, peek, ...)                                     \
	f(x) CHERIOT_MAP_LIST_NEXT(peek, CHERIOT_MAP_LIST0)(f, peek, __VA_ARGS__)

#define CHERIOT_MAP_LIST0_UD(f, ud, x, peek, ...)                              \
	f(x, ud) CHERIOT_MAP_LIST_NEXT(peek, CHERIOT_MAP_LIST1_UD)(                \
	  f, ud, peek, __VA_ARGS__)
#define CHERIOT_MAP_LIST1_UD(f, ud, x, peek, ...)                              \
	f(x, ud) CHERIOT_MAP_LIST_NEXT(peek, CHERIOT_MAP_LIST0_UD)(                \
	  f, ud, peek, __VA_ARGS__)

/**
 * Applies the function macro `f` to each of the remaining parameters.
 */
#define CHERIOT_MAP(f, ...)                                                    \
	CHERIOT_EVAL(CHERIOT_MAP1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/**
 * Applies the function macro `f` to each of the remaining parameters and passes
 * userdata as the second parameter to each invocation, e.g. MAP_UD(f, x, a, b,
 * c) evaluates to f(a, x) f(b, x) f(c, x)
 */
#define CHERIOT_MAP_UD(f, ud, ...)                                             \
	CHERIOT_EVAL(CHERIOT_MAP1_UD(f, ud, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/**
 * Applies the function macro `f` to each of the remaining parameters and
 * inserts commas between the results.
 */
#define CHERIOT_MAP_LIST(f, ...)                                               \
	CHERIOT_EVAL(CHERIOT_MAP_LIST1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/**
 * Applies the function macro `f` to each of the remaining parameters, inserts
 * commas between the results, and passes userdata as the second parameter to
 * each invocation, e.g. MAP_LIST_UD(f, x, a, b, c) evaluates to f(a, x), f(b,
 * x), f(c, x)
 */
#define CHERIOT_MAP_LIST_UD(f, ud, ...)                                        \
	CHERIOT_EVAL(                                                              \
	  CHERIOT_MAP_LIST1_UD(f, ud, __VA_ARGS__, ()()(), ()()(), ()()(), 0))
