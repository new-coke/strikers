#ifndef _SEBRINGSOUNDDEFINES_H_
#define _SEBRINGSOUNDDEFINES_H_

#include "types.h"

struct SoundDefinesTable
{
    /* 0x0 */ u32 musyxID;
    /* 0x4 */ const char* musyxStr;
    /* 0x8 */ u32 playCount;
}; // total size: 0xC

extern SoundDefinesTable SebringSoundDefines[];

u32 GetNumSoundDefines();

#endif // _SEBRINGSOUNDDEFINES_H_
