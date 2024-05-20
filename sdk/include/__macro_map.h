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
 *
 * CHERIoT modifications:
 * - Added CHERIOT_ prefix to all macros to avoid namespace pollution
 * - Used pragma once instead of include guards
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

#define CHERIOT_MAP_LIST_NEXT1(test, next)                                     \
	CHERIOT_MAP_NEXT0(test, CHERIOT_MAP_COMMA next, 0)
#define CHERIOT_MAP_LIST_NEXT(test, next)                                      \
	CHERIOT_MAP_LIST_NEXT1(CHERIOT_MAP_GET_END test, next)

#define CHERIOT_MAP_LIST0(f, x, peek, ...)                                     \
	f(x) CHERIOT_MAP_LIST_NEXT(peek, CHERIOT_MAP_LIST1)(f, peek, __VA_ARGS__)
#define CHERIOT_MAP_LIST1(f, x, peek, ...)                                     \
	f(x) CHERIOT_MAP_LIST_NEXT(peek, CHERIOT_MAP_LIST0)(f, peek, __VA_ARGS__)

/**
 * Applies the function macro `f` to each of the remaining parameters.
 */
#define CHERIOT_MAP(f, ...)                                                    \
	CHERIOT_EVAL(CHERIOT_MAP1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/**
 * Applies the function macro `f` to each of the remaining parameters and
 * inserts commas between the results.
 */
#define CHERIOT_MAP_LIST(f, ...)                                               \
	CHERIOT_EVAL(CHERIOT_MAP_LIST1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))
