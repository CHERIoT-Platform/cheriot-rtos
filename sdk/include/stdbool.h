// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#ifndef _STDBOOL_H_
#define _STDBOOL_H_

#ifndef __cplusplus
#if __STDC_VERSION__ < 202000
typedef _Bool bool;
#	define true 1
#	define false 0
#endif
#endif // __cplusplus

#endif // _STDBOOL_H_
