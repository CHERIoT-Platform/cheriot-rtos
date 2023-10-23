/**
 * Code related to storing a secret value.  The attacker should try to leak
 * the value stored in this file.
 */

#include "secret.h"
#include <compartment.h>
#include <debug.hh>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "JavaScript compartment">;

/// The secret value.
static int32_t secret;

/**
 * Set the secret the first time.
 *
 * This blocks until there is a character available on the UART.  This is
 * *not* a good way of getting entropy, but it's the only one that we have
 * in the simulator.
 */
void set_secret()
{
	auto uart = MMIO_CAPABILITY(Uart, uart);
	while (!uart->can_read()) {}
	secret = static_cast<int32_t>(rdcycle64());
	Debug::log("Secret stored at {}", &secret);
}

/**
 * Report the current value of the secret and pick a new one.
 */
void check_secret(int32_t guess)
{
	Debug::log("Secret was {}, you guessed {}.", secret, guess);
	if (guess == secret)
	{
		Debug::log("CONGRATULATIONS! You correctly leaked the secret!");
	}
	secret = static_cast<int32_t>(rdcycle64());
}
