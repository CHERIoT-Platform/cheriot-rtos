#pragma once

#ifndef __ASSEMBLER__
/**
 * Macro to allow sealing to work with old and new compilers.  With new
 * compilers, this will expand to a sealed version of the pointer type provided
 * as the argument.  For old compilers it will expand to the opaque type that
 * older versions of CHERIoT RTOS used for all sealed objects.
 *
 * You can explicitly opt into the old behaviour by defining
 * `CHERIOT_NO_SEALED_POINTERS`.
 */
#	ifndef CHERI_SEALED
#		if __has_extension(cheri_sealed_pointers) &&                          \
		  !defined(CHERIOT_NO_SEALED_POINTERS)
#			define CHERI_SEALED(T) T __sealed_capability
#		else
struct SObjStruct;
typedef struct SObjStruct *SObj;
#			define CHERI_SEALED(T) struct SObjStruct *
#		endif
#	endif
#endif
