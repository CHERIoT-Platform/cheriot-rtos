#pragma once

#ifndef __ASSEMBLER__
#	ifndef CHERI_SEALED
#		if __has_extension(cheri_sealed_pointers) && !defined(CHERIOT_NO_SEALED_POINTERS)
#			define CHERI_SEALED(T) T __sealed_capability
#		else
struct SObjStruct;
typedef struct SObjStruct *SObj;
#			define CHERI_SEALED(T) struct SObjStruct *
#		endif
#	endif
#endif
