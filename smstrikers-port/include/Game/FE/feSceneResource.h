#ifndef _FESCENERESOURCE_H_
#define _FESCENERESOURCE_H_

#include "types.h"
#include "Game/FE/feResourceManager.h"
// #include "Game/FE/feScene.h"

class FEScene;

class FESceneResource : public FEResourceHandle
{
public:
    FESceneResource();

    /* 0x14 */ FEScene* m_pFESceneContext;            // offset 0x14, size 0x4
    /* 0x18 */ unsigned long long m_glResourceMarker; // offset 0x18, size 0x8

    // /* 0x00 */ s32 m_unk_0x00;
    // /* 0x04 */ s32 m_unk_0x04;
    // /* 0x08 */ s32 m_unk_0x08;
    // /* 0x0C */ char padC[4];
    // /* 0x10 */ s8 m_unk_0x10;
    // /* 0x11 */ char pad11[3];
    // /* 0x14 */ s32 m_unk_0x14;
}; // total size: 0x20

#endif // _FESCENERESOURCE_H_
