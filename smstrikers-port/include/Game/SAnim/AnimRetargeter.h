#ifndef _ANIMRETARGETER_H_
#define _ANIMRETARGETER_H_

#include "Game/SAnim.h"

class AnimRetarget
{
public:
    /* 0x0 */ unsigned long m_TargetHierarchySignature;
    /* 0x4 */ long m_NumBones;
    /* 0x8 */ signed short* m_pMap;
}; // total size: 0xC

class AnimRetargetList : public cIdentifier
{
public:
    typedef char* MemType;

    static AnimRetargetList* Initialize(nlChunk* chunkData);
    static bool IsValidChunkID(u32 id)
    {
        return (id & 0x80FFFFFF) == 0x80017104;
    }
    AnimRetarget* GetAnimRetargetWithSignature(const cSAnim* anim);

    /* 0x8 */ long m_NumAnimRetargets;
    /* 0xC */ AnimRetarget* m_pAnimRetarget;
}; // total size: 0x10

#endif // _ANIMRETARGETER_H_
