// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#define CHERIOT_NO_AMBIENT_MALLOC
#define CHERIOT_NO_NEW_DELETE

#include <debug.hh>
using Debug = ConditionalDebug<true, "Validator">;

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
	Debug::log("Detected {} in validator.  Register {} contained "
	           "invalid value: {}",
	           exceptionCode,
	           registerNumber,
	           faultingRegister);

	return ErrorRecoveryBehaviour::ForceUnwind;
}

void __cheri_compartment("validator") validate(void *data, bool *valid)
{
	static bool fail = false;

	// Mock some validation that can cause BoundsViolation
	// when were send a small object.  There's a lot more
	// explict validation we could do on the capability,
	// but the point here is to show how a sandbox can
	// cope with unforeseen errors.
	Data *d = static_cast<Data *>(data);
	if ((strncmp(d->token, "Wile-E", 6) != 0) &&
	    (strncmp(d->token, "Coyote", 6) != 0))
	{
		*valid = false;
		return;
	}

	// If we go here then the data is at least safe
	// to try and access
	*valid = true;
}