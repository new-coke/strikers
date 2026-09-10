#include "types.h"

// clang-format off
/**
 * Offset/Address/Size: 0x0 | 0x802272E8 | size: 0x38
 */
ASM void TRK_flush_cache(u32 param_1, int param_2)
{
#ifdef __MWERKS__
	nofralloc
	lis r5, 0xFFFFFFF1@h
	ori r5, r5, 0xFFFFFFF1@l
	and r5, r5, r3
	subf r3, r5, r3
	add r4, r4, r3
loop:
	dcbst 0, r5
	dcbf 0, r5
	sync
	icbi 0, r5
	addic r5, r5, 8
	addic. r4, r4, -8
	bge loop
	isync
	blr
#endif
}
// clang-format on
