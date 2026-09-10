#ifndef _NISPLAYER_H_
#define _NISPLAYER_H_

#include "Game/InterpreterCore.h"

#include "Game/Camera/animcam.h"
#include "Game/Render/Nis.h"
#include "Game/Sys/eventman.h"
#include "NL/nlFile.h"
#include "NL/nlMath.h"

namespace Detail
{
class TempStringAllocator;
}

template <typename CharT, typename Allocator>
class BasicString;

enum NisUseStadiumOffset
{
    NIS_NO_STADIUM_OFFSET = 0,
    NIS_AWAY_STADIUM_OFFSET = 1,
    NIS_HOME_STADIUM_OFFSET = 2,
};

enum NisUseFilter
{
    NIS_NO_FILTER = 0,
    NIS_FILTER = 1,
};

enum NisEffectLifetime
{
    NIS_EFFECT_STOP_AT_END = 0,
    NIS_EFFECT_TIME_OUT = 1,
};

class NisPlayer : public InterpreterCore
{
public:
    NisPlayer();
    /* 0x08 */ virtual ~NisPlayer();
    /* 0x0C */ virtual void DoFunctionCall(unsigned int);

    void SetExtraNameFilter(const char* filter);
    void ResetEffects();
    void Effect(float frame, const char* name, const char* target, NisEffectLifetime lifetime);
    bool IsMirrored(NisTarget target, const char* name, NisWinnerType winnerType) const;
    int TargetToIndex(NisTarget target, int index, NisWinnerType winnerType) const;
    void EventHandler(Event* event);
    static void EventHandler(Event* event, void* userData) { ((NisPlayer*)userData)->EventHandler(event); }
    void PlayCharacterDirection();
    void Load(const char* nisType, NisTarget target, NisUseStadiumOffset useStadiumOffset, NisUseFilter useFilter, NisWinnerType winnerType);
    BasicString<char, Detail::TempStringAllocator> GetTargetFilter(NisTarget target, NisWinnerType winnerType) const;
    static void AsyncLoad(nlFile* file, void* buffer, unsigned int size, uintptr_t param);
    void LoadTriggers(Nis& nis);
    void Load(char* buffer, unsigned int size, NisHeader& nisHeader);
    void Render() const;
    bool Play();
    void Reset();
    void Update(float deltaT);
    void HandleAsyncs();
    bool WorldIsFrozen() const;
    float TimeLeft() const;

    static NisPlayer* Instance();

    /* 0x24   */ bool mActive;                  // offset 0x24, size 0x1
    /* 0x28   */ int mDictSize;                 // offset 0x28, size 0x4
    /* 0x2C   */ NisHeader mDict[256];          // offset 0x2C, size 0xBC00
    /* 0xBC2C */ char* mMemory;                 // offset 0xBC2C, size 0x4
    /* 0xBC30 */ int mMaxNumBallsVisible;       // offset 0xBC30, size 0x4
    /* 0xBC34 */ Nis* mPlaying[4];              // offset 0xBC34, size 0x10
    /* 0xBC44 */ Nis* mLoaded[4];               // offset 0xBC44, size 0x10
    /* 0xBC54 */ NisHeader* mLoadQueue[4];      // offset 0xBC54, size 0x10
    /* 0xBC64 */ bool mAsyncStarted[4];         // offset 0xBC64, size 0x4
    /* 0xBC68 */ bool mLoadingFromBack;         // offset 0xBC68, size 0x1
    /* 0xBC6C */ int mUsedFromFront;            // offset 0xBC6C, size 0x4
    /* 0xBC70 */ int mUsedFromBack;             // offset 0xBC70, size 0x4
    /* 0xBC74 */ s32 mGoalScorerCharIndex;      // offset 0xBC74, size 0x4
    /* 0xBC78 */ cAnimCamera mCamera;           // offset 0xBC78, size 0xAC
    /* 0xBD24 */ Nis* mNisForTriggerLoading;    // offset 0xBD24, size 0x4
    /* 0xBD28 */ int mWinnerSide[2];            // offset 0xBD28, size 0x8
    /* 0xBD30 */ nlVector3 mBeginPositions[10]; // offset 0xBD30, size 0x78
    /* 0xBDA8 */ char mExtraNameFilter[128];    // offset 0xBDA8, size 0x80
}; // total size: 0xBE28

#endif // _NISPLAYER_H_
