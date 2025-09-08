#include "../timing.h"
#include <compartment.h>
#include <debug.hh>
#include <fail-simulator-on-error.h>
#include <futex.h>
#include <interrupt.h>
#include <simulator.h>
#include <thread.h>
#include <timeout.h>
#include <platform-timer.hh>

using Debug = ConditionalDebug<true, "irq">;

/////
// Choose your metric.
/////

#if !defined(METRIC)
#	define METRIC rdcycle
#endif

/////
// Choose your IRQ source
/////

#if defined(SAIL)

struct Source
{
	static constexpr const char *Name = "Stub";

	auto futex()
	{
		return static_cast<uint32_t*>(nullptr);
	}

	void init()
	{
		Debug::log("Sail lacks external IRQs; this is a stub for CI!");
		simulation_exit(0);
	}

	void go() {}

	void done() {}
};

#elif defined(IRQ_SOURCE_ibex_revoker)

#	if !__has_include(<platform-hardware_revoker.hh>)
#		error This benchmark requires a hardware revoker as an IRQ source.
#	endif
#	include <platform-hardware_revoker.hh>

#	if !DEVICE_EXISTS(revoker) && !defined(CLANG_TIDY)
#		error Memory map was not configured with a revoker device
#	endif

#	include <platform/concepts/hardware_revoker.hh>

template<Revocation::IsHardwareRevokerDevice T>
    requires requires(T v) {
	    { v.interrupt_futex() } -> std::same_as<const uint32_t *>;
	    { v.request_interrupt() } -> std::same_as<void>;
    }
struct RevokerSource
{
	T revoker;

	RevokerSource() : revoker() {}

	auto futex()
	{
		return revoker.interrupt_futex();
	}

	void init()
	{
		revoker.init();

		// Wait for any in-progress scan to complete.  There shouldn't be one,
		// but it can't hurt to check.
		while (revoker.system_epoch_get() & 1)
		{
			;
		}
	}

	void go()
	{
		revoker.request_interrupt();
		revoker.system_bg_revoker_kick();
	}

	void done()
	{
		// snapshot epoch value, because it can change between calls
		auto epoch = revoker.system_epoch_get();
		Debug::Invariant(
		  (epoch & 1) == 0, "Unexpected odd revoker epoch: {}", epoch);
	}
};

struct Source : public RevokerSource<Ibex::HardwareRevoker>
{
	static constexpr const char *Name = "Ibex Revoker";
};

#elif defined(IRQ_SOURCE_sunburst_uart1)

#	include "platform/sunburst/platform-pinmux.hh"

DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(uart1InterruptCap,
                                        Uart1Interrupt,
                                        true,
                                        true);

struct Source
{
	static constexpr const char *Name = "Sunburst UART 1";

	volatile Uart &uart1;

	Source() : uart1(*MMIO_CAPABILITY(Uart, uart1)) {}

	auto futex()
	{
		return interrupt_futex_get(STATIC_SEALED_VALUE(uart1InterruptCap));
	}

	void init()
	{
		auto pinSinks =
		  MMIO_CAPABILITY(SonataPinmux::PinSinks, pinmux_pins_sinks);
		pinSinks->get(SonataPinmux::PinSink::ser1_tx)
		  .select(1); // uart1 tx -> ser1_tx
		auto blockSinks =
		  MMIO_CAPABILITY(SonataPinmux::BlockSinks, pinmux_block_sinks);
		blockSinks->get(SonataPinmux::BlockSink::uart_1_rx)
		  .select(1); // ser1_rx -> uart1 rx

		uart1.init(300);

		uart1.fifoCtrl = OpenTitanUart::FifoControlTransmitReset |
		                 OpenTitanUart::FifoControlReceiveReset;
	}

	void go()
	{
		while ((uart1.status & OpenTitanUart::StatusTransmitFull) == 0)
		{
			uart1.writeData = 'A';
		}
		uart1.interruptEnable = OpenTitanUart::InterruptTransmitWatermark;
		interrupt_complete(STATIC_SEALED_VALUE(uart1InterruptCap));
	}

	void done()
	{
		uart1.interruptEnable = 0;
	}
};

#else

#	error Unknown or unspecified IRQ source

#endif

extern "C"
{
	int start;
}

/**
 * Initialize the system, wait for an IRQ, and determine how long it took
 */
int __cheri_compartment("interrupt_bench") entry_high_priority()
{
	TimerCore::init();
	// Debug::log("time={}", TimerCore::time());
	TimerCore::settimelow(0xffffffff);
	// Debug::log("time={}", TimerCore::time());

	Source source{};

	Debug::log("Using {} for IRQs", source.Name);

	source.init();

	auto     interruptFutex = source.futex();
	uint32_t lastIrqCount   = *interruptFutex - 1;

	int c = 2;

	while (c-- > 0)
	{
		Timeout t{MS_TO_TICKS(2000)};

		auto irqCount = *interruptFutex;
		source.go();

		auto waitStart = METRIC();
		auto waitRes   = futex_timed_wait(&t, interruptFutex, irqCount);
		auto end       = METRIC();

		// Force the metric read to happen prior to our invariant checks &c.
		asm volatile("" : : : "memory");

		source.done();

		auto waitDelta = end - waitStart;
		auto wakeDelta = end - start;

		Debug::log("{} latency at IRQ count {}; cycle {} end {}, wait {}, wake {} ",
		           __XSTRING(METRIC),
		           irqCount,
				   TimerCore::time(),
		           end,
		           waitDelta,
		           wakeDelta);

		/*
		 * If this reports ETIMEDOUT, check that `t` is actually allowing enough
		 * time to lapse.  SAFE's Ibex revoker, for example, takes over a
		 * simulated second to run.
		 */
		Debug::Invariant(
		  waitRes == 0, "Unexpected result from futex_timed_wait: {}", waitRes);

		Debug::Invariant(wakeDelta < waitDelta,
		                 "Wait did not yield; make the source do more work");

		Debug::Invariant(irqCount == lastIrqCount + 1,
		                 "Missed IRQ at {}; was {}",
		                 irqCount,
		                 lastIrqCount);
		lastIrqCount = irqCount;

#ifndef SIMULATION
		// Rate limit us to make the output easier to observe
		t = MS_TO_TICKS(250);
		thread_sleep(&t, ThreadSleepNoEarlyWake);
#endif
	}
	simulation_exit(0);
	return 0;
}

/**
 * This lower priority thread will run once the higher priority thread is
 * waiting on the IRQ futex.  All it does is keep the starting value as up to
 * date as it can, so that when the high priority thread wakes, we have a
 * reasonable snapshot of the last value observed on core prior to the IRQ
 * handler firing.
 */
int __cheri_compartment("interrupt_bench") entry_low_priority()
{
	while (true)
	{
		start = METRIC();
	}
}
