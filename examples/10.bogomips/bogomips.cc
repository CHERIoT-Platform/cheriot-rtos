// Copyright Google LLC and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/*
 * Calculates Linux-style bogomips for cheriot-rtos.
 * See https://en.wikipedia.org/wiki/BogoMips for some background
 * on bogomips.
 * This code is derived from https://github.com/vitalyvch/Bogo/tree/BogoMIPS_v1.3.
 */
#include <fail-simulator-on-error.h>
#include <thread.h>
#include <platform-timer.hh>

#include <debug.hh>

#define USE_MDELAY 1
//#define USE_YDELAY 1

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "bogomips">;

// Marker symbols to hook to enable/disable profiling
extern "C" {
void __attribute__((noinline)) stats_enable(void) { asm(""); }
void __attribute__((noinline)) stats_disable(void) { asm(""); }
}

#if USE_MDELAY
// Use a memory barrier for the "noop".
static void delay(long loops) {
  for (long i = loops; !!(i > 0); --i)
    asm volatile ("" ::: "memory");
}
#define DELAY_IMPL "mdelay"
#elif USE_YDELAY
// Yield to the scheduler instead of a barrier
static void delay(long loops) {
  for (long i = loops; !!(i > 0); --i)
    yield();
}
#define DELAY_IMPL "ydelay"
#else
#error "Need a delay loop implementation"
#endif

void __cheri_compartment("bogomips") entry() {
  Debug::log("Bogomips w/ " DELAY_IMPL " (Thread {})", thread_id_get());

  TimerCore::init();

  stats_enable();
  int64_t bogomips = 0;
  long loops_per_sec = 1;
  while ((loops_per_sec <<= 1)) {
    // NB: this assumes the system clock updates concurrently
    //   with our delay loop, or is updated on demand when we read
    //   from the hardware. For simulation this may require configuration;
    //   e.g. for renode with the mpact simulator you want the
    //   built-in clint and not have renode simulate it because
    //   renode will update the clint only on it's scheduling
    //   boundary which results in time to not advance (resulting
    //   in bogomips ~25x higher).
    uint64_t duration = TimerCore::time();
    delay(loops_per_sec);
    duration = TimerCore::time() - duration;
    if (duration >= CPU_TIMER_HZ) {
      // Bogomips is considered "valid" only when there's at least
      // 1 second of execution time.
      bogomips = (loops_per_sec / duration) * CPU_TIMER_HZ;
      Debug::log("bogomips {}.{} raw bogomips {} lps {} duration {} hz {}",
                 bogomips / 500000, (bogomips / 5000) % 100, bogomips,
                 loops_per_sec, static_cast<int64_t>(duration),
                 static_cast<int64_t>(CPU_TIMER_HZ));
      break;
    }
  }
  stats_disable();
  simulation_exit(0);
}
