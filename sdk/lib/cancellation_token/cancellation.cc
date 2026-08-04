// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "cancellation-private.h"
#include <cheri.hh>
#include <errno.h>
#include <token.h>

using namespace CHERI;

#define CANCELLATION_TOKEN_KEY STATIC_SEALING_TYPE(CancellationToken)

CancealltionTokenSource
cancellation_token_source_create(TimeoutArgument     timeout,
                                 AllocatorCapability allocator)
{
	auto [unsealed, sealed] = token_allocate<CancellationTokenState>(
	  timeout, allocator, CANCELLATION_TOKEN_KEY);
	if (!cheri_is_valid(unsealed))
	{
		return reinterpret_cast<CancealltionTokenSource>(&unsealed);
	}
	return sealed;
}

CancellationToken cancellation_token_get(CancealltionTokenSource source)
{
	if (Capability unsealed = token_unseal(CANCELLATION_TOKEN_KEY, source))
	{
		unsealed.permissions() &=
		  PermissionSet{Permission::Load, Permission::Global};
		return unsealed;
	}
	return reinterpret_cast<CancellationToken>(static_cast<uintptr_t>(-EINVAL));
}

int cancellation_token_signal(CancealltionTokenSource source)
{
	if (Capability unsealed = token_unseal(CANCELLATION_TOKEN_KEY, source))
	{
		unsealed->token = true;
		return 0;
	}
	return -EINVAL;
}

int cancellation_token_source_destroy(CancealltionTokenSource tokenSource,
                                      AllocatorCapability     allocator)
{
	return token_obj_destroy(allocator, CANCELLATION_TOKEN_KEY, tokenSource);
}
