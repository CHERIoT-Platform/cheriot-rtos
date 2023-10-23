// Import everything from the environment
import * as host from "./cheri.js"

// Change this address to whatever address was logged at the start of execution.
const SecretAddress = 0x2004cc8c

function run()
{
	// Get the global capability into VM register 0
	host.register_move(0, host.CGP)
	// Check that the address is in the bounds of the globals capability.
	if (SecretAddress < host.get_base(0))
	{
		host.print('Secret is before the start of CGP')
		return
	}
	if ((SecretAddress+4) > (host.get_base(0) + host.get_length(0)))
	{
		host.print('Secret is after the end of CGP')
		return
	}
	// Set the address in our copy of the global capability to the address of
	// the secret.
	host.set_address(0, SecretAddress)
	// See if we managed to leak the secret correctly.
	host.check_secret(host.load_int(0, 0));
}

vmExport(1234, run);
