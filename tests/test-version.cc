// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "tests.hh"

int print_version_information()
{
	debug_log("Build of " CHERIOT_RTOS_GIT_DESCRIPTION " on " __DATE__);
	return 0;
}
