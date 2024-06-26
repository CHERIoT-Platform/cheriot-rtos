// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#include "cdefs.h"
#include <compartment.h>

/**
 * Run a validator data in sandpit compartment
 *
 * The validation result is passed via bool*
 * as we can't rely on returning an error value
 * if the compartment error handler gets called.
 * The caller should set this to false before
 * calling validate(), and it is only set to true
 * at the end of the validation
 */
bool __cheri_compartment("validator") validate(void *data, bool *valid);