// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/*
 * This test tests nothing during run-time. It's a compile-time test. The
 * purpose is to be sure that all the headers not only work for C++ but for C
 * as well.
 */
#include <assert.h>
#include <cdefs.h>
#include <cheri-builtins.h>
#include <compartment.h>
#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <futex.h>
#include <interrupt.h>
#include <inttypes.h>
#include <limits.h>
#include <locks.h>
#include <multiwaiter.h>
#include <queue.h>
#include <riscvreg.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <strings.h>
#include <switcher.h>
#include <thread.h>
#include <thread_pool.h>
#include <time.h>
#include <timeout.h>
#include <token.h>
#include <cheri.h>
