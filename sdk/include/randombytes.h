#pragma once

#include <cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

/**
 * Populate `output` with `n` bytes of entropy from the system's entropy source.
 *
 * This will be cryptographically secure entropy if and only if the system
 * entropy source is cryptographically secure.
 *
 * Returns 0 on success.
 */
__cheriot_compartment("randombytes") int randombytes(uint8_t *output, size_t n);

__END_DECLS
