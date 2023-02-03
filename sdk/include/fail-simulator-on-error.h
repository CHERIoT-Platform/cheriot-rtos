#include "compartment.h"
#include <cheri.hh>
#include <debug.hh>
#include <priv/riscv.h>
#include <simulator.h>

using DebugErrorHandler = ConditionalDebug<
#ifdef NDEBUG
  false
#else
  true
#endif
  ,
  "Error handler">;

/**
 * A error handler that reports unexpected traps and calls the scheduler to
 * request that the simulator exit with exit code 1 (FAILURE).
 *
 * This is useful because the default behaviour in the absence of an error
 * handler is to unwind the trusted stack and exit the thread when the top of
 * the stack is reached. If all threads exit then the scheduler will exit the
 * simulation with SUCCESS, which might mask errors.
 *
 * We attempt to detect the trap that occurs when a thread returns from its
 * entry point function and do not halt the simulation as this is considered
 * normal.
 *
 * Unwinds caused by errors in called compartments are also detected and are not
 * treated as errors -- instead we resume execution at the call point with a
 * return value of -1 as though this handler is not present.
 *
 * On non-simulation platforms this error handler reverts to the default force
 * unwind behaviour.
 *
 * If NDEBUG is not defined then errors and thread exits will be logged.
 *
 * To use this simply include this file in your compartment's c / cc file.
 */
extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	if (mcause == priv::MCAUSE_CHERI)
	{
		auto [exceptionCode, registerNumber] =
		  CHERI::extract_cheri_mtval(mtval);
		// The thread entry point is called with a NULL return address so the
		// cret at the end of the entry point function will trap if it is
		// reached. We don't want to treat this as an error but thankfully we
		// detect it quite specifically by checking for all of:
		// 1) CHERI cause is tag violation
		// 2) faulting register is CRA
		// 3) value of CRA is NULL
		// 4) we've reached the top of the thread's stack
		CHERI::Capability stackCapability{
		  frame->get_register_value<CHERI::RegisterNumber::CSP>()};
		CHERI::Capability returnCapability{
		  frame->get_register_value<CHERI::RegisterNumber::CRA>()};
		if (registerNumber == CHERI::RegisterNumber::CRA &&
		    returnCapability.address() == 0 &&
		    exceptionCode == CHERI::CauseCode::TagViolation &&
		    stackCapability.top() == stackCapability.address())
		{
			// looks like thread exit -- just log it then ForceUnwind
			DebugErrorHandler::log("Thread exit CSP={}", stackCapability);
		}
		else if (exceptionCode == CHERI::CauseCode::None)
		{
			// An unwind occurred from a called compartment, just resume.
			return ErrorRecoveryBehaviour::InstallContext;
		}
		else
		{
			// An unexpected error -- log it and end the simulation with error.
			DebugErrorHandler::log("{} error at {}", exceptionCode, frame->pcc);
			simulation_exit(1);
		}
	}
	else
	{
		// other error (e.g. __builtin_trap causes ReservedInstruciton)
		// log and end simulation with error.
		DebugErrorHandler::log("Unhandled error {} at {}", mcause, frame->pcc);
		simulation_exit(1);
	}
	return ErrorRecoveryBehaviour::ForceUnwind;
}
