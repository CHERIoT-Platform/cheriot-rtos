// Import everything from the environment
import * as host from "./cheri.js"

function run()
{
	host.print('Hello world')
}


vmExport(1234, run);
