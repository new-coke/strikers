#ifndef _FIELDERACTIONS_H_
#define _FIELDERACTIONS_H_

#include "NL/nlMath.h"
#include "Game/Effects/EmissionController.h"
#include "Game/Camera/MatrixEffectCam.h"
#include "Game/Player.h"
#include "Game/Sys/eventman.h"

#include "Game/AI/Fielder.h"

struct PlayerAttackData : public EventData
{
    virtual u32 GetID() { return 0x19a; }

    /* 0x04 */ const cFielder* pAttacker;
    /* 0x08 */ int nAttackerPadID;
    /* 0x0C */ cFielder* pTarget;
    /* 0x10 */ float fAttackIntensity;
}; // total size: 0x14

void HyperStrikeEffectUpdate(EmissionController& controller);
void OtherMatrixCamFinishedCallback(MatrixEffectCam* pMatrixCam);
void MatrixCamFinishedCallback(MatrixEffectCam* pMatrixCam);
nlVector3 GetClosestWallPoint(const nlVector3& pos);

// class Function1<void, EmissionController&>
// {
// public:
//     void FunctorBase::~FunctorBase();
// };

// class cNet
// {
// public:
//     void GetPostRadius();
// };

// class LexicalCast<BasicString<char, Detail
// {
// public:
//     void TempStringAllocator>, const char*>(const char* const&);
// };

// class Detail
// {
// public:
//     void LexicalCastImpl<BasicString<char, Detail::TempStringAllocator>, const char*>::Do(const char*);
// };

// class Format<BasicString<char, Detail
// {
// public:
//     void TempStringAllocator>, const char*>(const BasicString<char, Detail::TempStringAllocator>&, const char* const&);
// };

// class FormatImpl<BasicString<char, Detail
// {
// public:
//     void TempStringAllocator>>::operator%<const char*>(const char* const&);
//     void TempStringAllocator>>::operator BasicString<char, Detail::TempStringAllocator>() const;
// };

#endif // _FIELDERACTIONS_H_
