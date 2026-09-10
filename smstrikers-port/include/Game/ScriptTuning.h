#ifndef _SCRIPTTUNING_H_
#define _SCRIPTTUNING_H_

#include "NL/nlMath.h"

#include "Game/TweaksBase.h"

class FuzzyTweaks : public TweaksBase
{
public:
    FuzzyTweaks(const char* name);
    ~FuzzyTweaks();
    void Init();

    /* 0x24, */ nlVector2 vCloseTeammateConfidenceDistance;     // offset 0x24, size 0x8
    /* 0x2C, */ nlVector2 vNearTeammateConfidenceDistance;      // offset 0x2C, size 0x8
    /* 0x34, */ nlVector2 vFarTeammateConfidenceDistance;       // offset 0x34, size 0x8
    /* 0x3C, */ nlVector2 vCloseOpponentConfidenceDistance;     // offset 0x3C, size 0x8
    /* 0x44, */ nlVector2 vNearOpponentConfidenceDistance;      // offset 0x44, size 0x8
    /* 0x4C, */ nlVector2 vFarOpponentConfidenceDistance;       // offset 0x4C, size 0x8
    /* 0x54, */ nlVector2 vReallyCloseToBallDistanceConfidence; // offset 0x54, size 0x8
    /* 0x5C, */ nlVector2 vCloseBallConfidenceDistance;         // offset 0x5C, size 0x8
    /* 0x64, */ nlVector2 vNearBallConfidenceDistance;          // offset 0x64, size 0x8
    /* 0x6C, */ nlVector2 vFarBallConfidenceDistance;           // offset 0x6C, size 0x8
    /* 0x74, */ nlVector2 vCloseNetConfidenceDistance;          // offset 0x74, size 0x8
    /* 0x7C, */ nlVector2 vNearNetConfidenceDistance;           // offset 0x7C, size 0x8
    /* 0x84, */ nlVector2 vFarNetConfidenceDistance;            // offset 0x84, size 0x8
    /* 0x8C, */ nlVector2 vCloseBallNetConfidenceDistance;      // offset 0x8C, size 0x8
    /* 0x94, */ nlVector2 vNearBallNetConfidenceDistance;       // offset 0x94, size 0x8
    /* 0x9C, */ nlVector2 vFarBallNetConfidenceDistance;        // offset 0x9C, size 0x8
    /* 0xA4, */ nlVector2 vCloseToFormationPositionDistance;    // offset 0xA4, size 0x8
    /* 0xAC, */ nlVector2 vNearToFormationPositionDistance;     // offset 0xAC, size 0x8
    /* 0xB4, */ nlVector2 vFarToFormationPositionDistance;      // offset 0xB4, size 0x8
    /* 0xBC, */ nlVector2 vCloseGoalieConfidenceDistance;       // offset 0xBC, size 0x8
    /* 0xC4, */ nlVector2 vNearGoalieConfidenceDistance;        // offset 0xC4, size 0x8
    /* 0xCC, */ nlVector2 vFarGoalieConfidenceDistance;         // offset 0xCC, size 0x8
    /* 0xD4, */ nlVector2 vCloseToSidelineDistanceConfidence;   // offset 0xD4, size 0x8
    /* 0xDC, */ nlVector2 vNearToSidelineDistanceConfidence;    // offset 0xDC, size 0x8
    /* 0xE4, */ nlVector2 vFarFromSidelineDistanceConfidence;   // offset 0xE4, size 0x8
    /* 0xEC, */ nlVector2 vHighBallConfidenceDistance;          // offset 0xEC, size 0x8
    /* 0xF4, */ nlVector2 vReallyHighBallConfidenceDistance;    // offset 0xF4, size 0x8
    /* 0xFC, */ s16 nFacingFullConfidenceAngle;                 // offset 0xFC, size 0x2
    /* 0xFE, */ s16 nFacingNoConfidenceAngle;                   // offset 0xFE, size 0x2
    /* 0x100 */ nlVector2 vControlConfidenceDistance;           // offset 0x100, size 0x8
    /* 0x108 */ nlVector2 vIdealTacklingDistance;               // offset 0x108, size 0x8
    /* 0x110 */ f32 fPassLaneDistance;                          // offset 0x110, size 0x4
    /* 0x114 */ f32 fShotLaneDistance;                          // offset 0x114, size 0x4
    /* 0x118 */ f32 fPassInPlayFullConfidenceDistance;          // offset 0x118, size 0x4
    /* 0x11C */ f32 fShotInPlayFullConfidenceDistance;          // offset 0x11C, size 0x4
    /* 0x120 */ nlVector2 vOnGroundConfidenceDistance;          // offset 0x120, size 0x8
    /* 0x128 */ f32 fInterceptBallSwapControlerScoreWeight;     // offset 0x128, size 0x4
    /* 0x12C */ f32 fInterceptBallScoreWeight;                  // offset 0x12C, size 0x4
    /* 0x130 */ nlVector2 vInterceptBallConfidenceTime;         // offset 0x130, size 0x8
    /* 0x138 */ nlVector2 vInterceptBallConfidenceDistance;     // offset 0x138, size 0x8
    /* 0x140 */ f32 fPressuredNearWeight;                       // offset 0x140, size 0x4
    /* 0x144 */ nlVector2 vAvoidBowserRepulsionConfidence;      // offset 0x144, size 0x8
    /* 0x14C */ nlVector2 vAvoidGoalieRepulsionConfidence;      // offset 0x14C, size 0x8
    /* 0x154 */ nlVector2 vAvoidFieldersRepulsionConfidence;    // offset 0x154, size 0x8
    /* 0x15C */ nlVector2 vAvoidPowerupsRepulsionConfidence;    // offset 0x15C, size 0x8
    /* 0x164 */ nlVector2 vPlayerShotDistance;                  // offset 0x164, size 0x8
    /* 0x16C */ nlVector2 vIdealDistanceForShooting;            // offset 0x16C, size 0x8
    /* 0x174 */ nlVector2 vGoalieOutOfPositionDistance;         // offset 0x174, size 0x8
    /* 0x17C */ nlVector2 vOutOfNetConfidenceDistance;          // offset 0x17C, size 0x8
    /* 0x184 */ f32 fUpfieldMaxDistance;                        // offset 0x184, size 0x4
    /* 0x188 */ f32 fDownfieldMaxDistance;                      // offset 0x188, size 0x4
    /* 0x18C */ f32 fClosingSpeedMax;                           // offset 0x18C, size 0x4
    /* 0x190 */ f32 fSeparatingSpeedMax;                        // offset 0x190, size 0x4
    /* 0x194 */ f32 fFrontOfNetMidAngle;                        // offset 0x194, size 0x4
    /* 0x198 */ f32 fFrontOfNetMaxAngle;                        // offset 0x198, size 0x4
    /* 0x19C */ f32 fFrontOfNetMidScore;                        // offset 0x19C, size 0x4
    /* 0x1A0 */ nlVector2 vGetOpenPassLaneOffset;               // offset 0x1A0, size 0x8
    /* 0x1A8 */ nlVector2 vGetOpenPassLaneDist;                 // offset 0x1A8, size 0x8
    /* 0x1B0 */ nlVector2 vOpenRadius;                          // offset 0x1B0, size 0x8
    /* 0x1B8 */ nlVector2 vWideOpenRadius;                      // offset 0x1B8, size 0x8
    /* 0x1C0 */ nlVector2 vInBetweenInterceptRange;             // offset 0x1C0, size 0x8
    /* 0x1C8 */ nlVector2 vInBetweenConeWidth;                  // offset 0x1C8, size 0x8
    /* 0x1D0 */ f32 fPassDeadZone;                              // offset 0x1D0, size 0x4
    /* 0x1D4 */ f32 fLosingScoreDelta;                          // offset 0x1D4, size 0x4
    /* 0x1D8 */ f32 fWinningScoreDelta;                         // offset 0x1D8, size 0x4
    /* 0x1DC */ f32 fTiedScoreDelta;                            // offset 0x1DC, size 0x4
    /* 0x1E0 */ f32 fGameTimeCloseToOver;                       // offset 0x1E0, size 0x4
    /* 0x1E4 */ f32 fGameTimeNearlyOver;                        // offset 0x1E4, size 0x4
    /* 0x1E8 */ f32 fGameTimeFarFromOver;                       // offset 0x1E8, size 0x4
    /* 0x1EC */ nlVector2 vDefensiveConfidenceDistances;        // offset 0x1EC, size 0x8
    /* 0x1F4 */ nlVector2 vOffensiveConfidenceDistances;        // offset 0x1F4, size 0x8
}; // total size: 0x1FC

#endif // _SCRIPTTUNING_H_
