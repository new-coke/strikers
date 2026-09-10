#ifndef _JUMBOTRON_H_
#define _JUMBOTRON_H_

#include "types.h"

enum eJumboState
{
    JState_Nothing = 0,
    JState_Loading = 1,
    JState_Ready = 2,
    JState_PlayingButWaiting = 3,
    JState_Playing = 4,
    JState_Num = 5,
};
enum eJumboType
{
    Jumbo_Nothing = 0,
    Jumbo_Kickoff = 1,
    Jumbo_Goal = 2,
    Jumbo_Wins = 3,
    Jumbo_Num = 4,
};

class Jumbotron
{
public:
    void Initialize();
    void Uninitialize();
    void Reset();
    void BeginLoad();
    void WaitForLoad();
    void BeginPlaying();
    void StopPlaying();
    void Update(float dt);

    /* 0x00 */ void* m_BundleLoadBase;
    /* 0x04 */ eJumboState m_State;
    /* 0x08 */ eJumboType m_AnimationClass;
    /* 0x0C */ float m_fFramerate;
    /* 0x10 */ float m_fTime;
    /* 0x14 */ u32 m_CurrentTexture;
    /* 0x18 */ int m_nCurrentFrame;
    /* 0x1C */ int m_nNumFrames;
    /* 0x20 */ int m_nMaxNumFrames;
    /* 0x24 */ char m_szPrefix[64];
    /* 0x64 */ char m_szTexture[128];

    static Jumbotron instance;
}; // total size: 0xE4

extern void* g_TrophyTextureLocationInMemory;

#endif // _JUMBOTRON_H_
