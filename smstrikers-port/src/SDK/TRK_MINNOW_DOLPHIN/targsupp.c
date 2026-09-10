#include "types.h"

// clang-format off

// TRKAccessFile = .text:0x80228F70; // type:label scope:global
#pragma function_align 16
ASM void TRKAccessFile()
{
#ifdef __MWERKS__
	twi 31, r0, 0
	blr
#endif
}
#pragma function_align 4

// TRKOpenFile = .text:0x80228F78; // type:label scope:global
ASM void TRKOpenFile()
{
#ifdef __MWERKS__
	twi 31, r0, 0
	blr
#endif
}

// TRKCloseFile = .text:0x80228F80; // type:label scope:global
ASM void TRKCloseFile()
{
#ifdef __MWERKS__
	twi 31, r0, 0
	blr
#endif
}

// TRKPositionFile = .text:0x80228F88; // type:label scope:global
ASM void TRKPositionFile()
{
#ifdef __MWERKS__
	twi 31, r0, 0
	blr
#endif
}

// clang-format on
