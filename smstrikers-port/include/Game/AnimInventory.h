#ifndef _ANIMINVENTORY_H_
#define _ANIMINVENTORY_H_

#include "types.h"
#include "NL/nlList.h"

#include "Game/Inventory.h"
#include "Game/SAnim.h"

struct AnimProperties
{
    /* 0x00 */ const char* enumName;
    /* 0x04 */ const char* animName;
    /* 0x08 */ ePlayMode playMode;
    /* 0x0C */ float blendAmount;
    /* 0x10 */ bool mirror;
    // /* 0x11 */ u8 pad11[3];
    /* 0x14 */ int ballRotationMode;
    /* 0x18 */ int endPhase;
    /* 0x1C */ u8 matchCharacterSpeed;
    // /* 0x1D */ u8 pad1d[3];
};

class cAnimInventory
{
public:
    cAnimInventory(const AnimProperties* props, int count);
    ~cAnimInventory();

    // bundle ingest and lookups
    void AddAnimBundle(const char* szFilename);
    cSAnim* GetAnim(int i);
    ePlayMode GetPlayMode(int i);
    float GetBlendTime(int i);
    bool GetMirrored(int i);
    int GetBallRotationMode(int i);
    int GetEndPhase(int i);
    u8 GetMatchCharacterSpeed(int i);

    /* 0x00 */ int m_nNumProperties;
    /* 0x04 */ cInventory<cSAnim>* m_pSAnimInventory;
    /* 0x08 */ cSAnim** m_pSAnims;
    /* 0x0C */ const AnimProperties* m_pAnimProperties;
};

#endif // _ANIMINVENTORY_H_
