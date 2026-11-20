// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <cdefs.h>
#include <stdlib.h>
#include <timeout.h>

/**
 * \file .
 *
 * Cancellation token APIs.
 *
 * Cancellation tokens are an abstraction that allows long-running tasks in
 * other threads to be cooperatively cancelled.  A cancellation token is created
 * from a *cancellation-token source*.  The holder of the cancellation-token
 * source may signal cancellation.  Holders of the token can check whether it
 * has been cancelled.
 */

/**
 * State associated with a cancellation token.  The cancellation token and the
 * cancellation-token source are both views of this object, but it is not used
 */
struct CancellationTokenState;

/**
 * The handle for a cancellation-token source.
 */
typedef struct CancellationTokenState
  *__sealed_capability CancealltionTokenSource;

/**
 * The type for a cancellation token.  This can be queried, but cannot be
 * signalled.
 */
typedef const struct CancellationTokenState *CancellationToken;

/**
 * Create a cancellation-token source, allocated from the caller's quota.
 *
 * This returns either a tagged capability for a cancellation token or an
 * untagged value that is an error code that corresponds to a value that the
 * allocator would return on failure.
 */
CancealltionTokenSource __cheriot_compartment("cancellation_token")
  cancellation_token_source_create(TimeoutArgument     timeout,
                                   AllocatorCapability allocator);

/**
 * Returns a pointer to the cancellation token for this cancellation-token
 * source.  This is a read-only *unsealed* capability.
 *
 * Returns an untagged value if the cross-compartment call fails or if `source`
 * is not a valid cancellation-token source.
 */
CancellationToken __cheriot_compartment("cancellation_token")
  cancellation_token_get(CancealltionTokenSource source);

/**
 * Signal a cancellation token.  This ensures that the next call to
 * `cancellation_token_check` will return 1.
 */
int __cheriot_compartment("cancellation_token")
  cancellation_token_signal(CancealltionTokenSource tokenSource);

/**
 * Destroys a cancellation token.  The `allocator` parameter must be the same
 * as that passed to `cancellation_token_source_create`.  Returns 0 on success,
 * or `-EINVAL` if either of the arguments is not the correct type.
 */
int __cheriot_compartment("cancellation_token")
  cancellation_token_source_destroy(CancealltionTokenSource tokenSource,
                                    AllocatorCapability     allocator);

/**
 * Check whether the cancellation token has been signalled.  This will acquire
 * an ephemeral claim over the token, so is safe to use even if the token has
 * been deallocated.
 *
 * The ephemeral claim acquisition will potentially block for an unbounded
 * amount of time.  If this is not acceptable, use
 * `cancellation_token_check_unsafe`.
 *
 * Return values are:
 *
 * * 0: The cancellation token is still valid.
 * * 1: The cancellation token has been signalled.
 * * `-EINVAL`: The argument is not a cancellation token (this can happen if the
 *   cancellation token has been deallocated).
 */
int __cheriot_libcall cancellation_token_check(CancellationToken token);
