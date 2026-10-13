// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "cancellation-private.h"
#include <allocator.h>
#include <cheri.hh>
#include <debug.hh>
#include <errno.h>
#include <unwind.h>

int __cheriot_libcall cancellation_token_check(CancellationToken token)
{
	if (CHERI::Capability{token}.is_valid())
	{
		CHERIOT_DURING
		{
			return token->token;
		}
		CHERIOT_HANDLER
		CHERIOT_END_HANDLER
	}
	return -EINVAL;
}
