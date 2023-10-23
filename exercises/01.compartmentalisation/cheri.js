// FFI Imports
// Each function imported from the host environment needs to be assigned to a
// global like this and identified by a constant that the resolver in the C/C++
// code will understand.
// These constants are defined in the `Exports` enumeration.


/**
 * Log function, writes all arguments to the UART.
 */
export const print = vmImport(1);

/*******************************************************************************
 * CHERI functions give access to a virtual machine for writing exploits.  This
 * allows 8 virtual registers to be accessed as source and destination operands
 * to any of the registers.
 *
 * The constants CSP, PCC, and CGP can also be used to access the values in
 * these physical registers directly, but only as source operands.
 ******************************************************************************/

export const CSP = 8
export const CGP = 9
export const PCC = 10

/**
 * register_move(destination, source)
 *
 * Move a value between two registers.  
 */
export const register_move = vmImport(2);

/**
 * load_capability(destination, source, offset)
 *
 * Loads a capability from the specified offset to the source capability
 * register, into the destination register.
 */
export const load_capability = vmImport(3);

/**
 * load_int(source, offset)
 *
 * Loads and returns an integer from memory, from the specified offset to the
 * source register.
 */
export const load_int = vmImport(4);

/**
 * store(source, destination, offset)
 *
 * Stores a capability from a source register to the specified offset relative
 * to the destination register.
 */
export const store = vmImport(5);

/**
 * get_address(source)
 *
 * Returns the address for the specified source register (capability).
 */
export const get_address = vmImport(6);

/**
 * set_address(source, address)
 *
 * Sets the address for the specified register (capability).
 */
export const set_address = vmImport(7);

/**
 * get_base(source)
 *
 * Returns the base address for the specified source register (capability).
 */
export const get_base = vmImport(8);

/**
 * get_length(source)
 *
 * Returns the length for the specified source register (capability).
 */
export const get_length = vmImport(9);

/**
 * get_permissions(source)
 *
 * Returns the permissions for the specified source register (capability).
 */
export const get_permissions = vmImport(10);

/**
 * check_secret(guess) 
 *
 * Resets the secret and prints the old value.  Takes your guess as an argument
 * and will report whether you guessed correctly.  Use to see if you've managed
 * to leak the value.
 */
export const check_secret = vmImport(11);
