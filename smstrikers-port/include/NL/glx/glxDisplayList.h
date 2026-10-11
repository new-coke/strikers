#ifndef _GLXDISPLAYLIST_H_
#define _GLXDISPLAYLIST_H_

#include "NL/gl/glUserData.h"

struct DisplayList
{
    /* 0x00 */ u32 magic;
    /* 0x04 */ void* list;
    /* 0x08 */ u32 size;
    // PORT: inline stream count and direct-color flag; retain the console layout.
    /* 0x0C */ unsigned short indices[2];
}; // total size: 0x10

DisplayList* dlMakeDisplayList(const glModelPacket* packet, bool permanent);
u32 dlGetSize(uintptr_t addr);
void* dlGetDisplayList(uintptr_t addr);
bool dlIsDisplayList(uintptr_t addr);
DisplayList* dlGetStruct(uintptr_t addr);

#endif // _GLXDISPLAYLIST_H_
