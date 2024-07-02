// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#define CHERIOT_NO_AMBIENT_MALLOC
#define CHERIOT_NO_NEW_DELETE

#include <debug.hh>
#include <string.h>
using Debug = ConditionalDebug<true, "Sandbox">;

#include "data.h"

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	auto [exceptionCode, registerNumber] = CHERI::extract_cheri_mtval(mtval);
	void *faultingRegister               = nullptr;
	if (registerNumber == CHERI::RegisterNumber::PCC)
	{
		faultingRegister = frame->pcc;
	}
	else if ((registerNumber > CHERI::RegisterNumber::CZR) &&
	         (registerNumber <= CHERI::RegisterNumber::CA5))
	{
		// The registers array does not include cnull.
		faultingRegister = frame->registers[int(registerNumber) - 1];
	}
	Debug::log("Detected {} in Sandbox.  Register {} contained "
	           "invalid value: {}",
	           exceptionCode,
	           registerNumber,
	           faultingRegister);

	return ErrorRecoveryBehaviour::ForceUnwind;
}

//
// Wapper to memcopy to run it in a sandbox
//
int __cheri_compartment("sandbox")
  sandbox_copy(void *src, void *dst, size_t size)
{
	memcpy(dst, src, size);
	return 0;
}

//
// Data validator
//
int __cheri_compartment("sandbox") sandbox_validate(void *data)
{
	// Mock some validation that can cause BoundsViolation
	// when we're sent a small object - the point here is to
	// show how a sandbox can protect against data errors.
	Data *d = static_cast<Data *>(data);
	if ((strncmp(d->token, "Wile-E", 6) != 0) &&
	    (strncmp(d->token, "Coyote", 6) != 0))
	{
		return -1;
	}

	// If we go here then the data is at least safe
	// to try and access
	return 0;
}
