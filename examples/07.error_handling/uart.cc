// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "hello.h"
#include <debug.hh>
#include <futex.h>
#include <locks.hh>
#include <platform-uart.hh>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "UART compartment">;

// Import some useful things from the CHERI namespace.
using namespace CHERI;

FlagLock lock;

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	auto [exceptionCode, registerNumber] = extract_cheri_mtval(mtval);
	void *faultingRegister               = nullptr;
	if (registerNumber == RegisterNumber::PCC)
	{
		faultingRegister = frame->pcc;
	}
	else if ((registerNumber > RegisterNumber::CZR) &&
	         (registerNumber <= RegisterNumber::CA5))
	{
		// The registers array does not include cnull.
		faultingRegister = frame->registers[int(registerNumber) - 1];
	}
	// Make sure that we have a new line before the debug output.
	// This uses the UART driver directly to write a single byte.
	MMIO_CAPABILITY(Uart, uart)->blocking_write('\n');

	Debug::log("Detected {} trying to write to UART.  Register {} contained "
	           "invalid value: {}",
	           exceptionCode,
	           registerNumber,
	           faultingRegister);
	lock.unlock();
	return ErrorRecoveryBehaviour::ForceUnwind;
}

/// Write a message to the UART.
void write(const char *msg)
{
	LockGuard g{lock};
	Debug::log("Message provided by caller: {}", msg);
}
