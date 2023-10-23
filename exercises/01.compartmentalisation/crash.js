// Import everything from the environment
import * as host from "./cheri.js"


function run()
{
	host.print('This should crash')
	// Make sure that this really is invalid
	host.set_address(0,0);
	host.load_int(0,0);
}

// FFI exports.  Each function that we export needs to be assigned a unique
// number that can be used to look it up.
vmExport(1234, run);
