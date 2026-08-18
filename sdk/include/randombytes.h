#pragma once

#include <cdefs.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#	include <concepts>
#endif

/**
 * \file
 *
 * API for accessing a shared entropy source in the `randombytes` compartment.
 */

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

#ifdef __cplusplus

/**
 * Populate a container-like `output` with entropy using its `.size()` and
 * `.data`() APIs.
 *
 * This will be cryptographically secure entropy if and only if the system
 * entropy source is cryptographically secure.
 *
 * Returns 0 on success.
 */
template<typename T>
    requires requires(T &v) {
	    requires std::is_pointer_v<decltype(v.data())>;
	    requires std::is_convertible_v<decltype(v.size()), size_t>;
    }
__always_inline int randombytes(T &output)
{
	return randombytes(reinterpret_cast<uint8_t *>(output.data()),
	                   output.size() * sizeof(*output.data()));
}

/**
 * Populate an `output` array of length `N` with `N * sizeof(output[0])` bytes
 * of entropy from the system's entropy source.
 *
 * This will be cryptographically secure entropy if and only if the system
 * entropy source is cryptographically secure.
 *
 * Returns 0 on success.
 */
template<typename T, size_t N>
    requires std::is_arithmetic_v<T>
__always_inline int randombytes(T (&output)[N])
{
	return randombytes(reinterpret_cast<uint8_t *>(&output), sizeof(T));
}

/**
 * Populate `output` with `sizeof(output)` bytes of entropy from the system's
 * entropy source.
 *
 * This will be cryptographically secure entropy if and only if the system
 * entropy source is cryptographically secure.
 *
 * Returns 0 on success.
 */
template<typename T>
    requires std::is_arithmetic_v<T>
__always_inline int randombytes(T &output)
{
	return randombytes(reinterpret_cast<uint8_t *>(&output), sizeof(output));
}

#endif
