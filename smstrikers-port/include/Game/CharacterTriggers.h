#ifndef _CHARACTERTRIGGERS_H_
#define _CHARACTERTRIGGERS_H_

#include "NL/nlMath.h"

enum eBallShotEffectType
{
    BALL_EFFECT_S2S_SUPER_SHOT = 0,
    BALL_EFFECT_S2S_SHOT = 1,
    BALL_EFFECT_PERFECT_SHOT = 2,
    BALL_EFFECT_PERFECT_PASS = 3,
    BALL_EFFECT_REGULAR_SHOT = 4,
    BALL_EFFECT_ONETIMER_SHOT = 5,
    BALL_EFFECT_CHIP_SHOT = 6,
    NUM_BALL_EFFECTS = 7,
};

class cCharacter;
class cPlayer;
class cFielder;
class EmissionController;

void KillSlideTackleTrail(cCharacter* pCharacter);
void EmitSlideTackleTrail(cCharacter* pCharacter);
void EmitTackleImpact(cPlayer* pCharacter);
void EmitChainBite(cFielder* pFielder);
void KillStar(cFielder* pFielder);
void EmitStar(cFielder* pFielder);
void KillMushroom(cFielder* pFielder);
void EmitMushroom(cFielder* pFielder);
void EmitDust(cPlayer* player, const char* name);
void EmitTurbo(cPlayer* player, const char* unused);
void KillWindup(cCharacter* pCharacter, const char* name, bool bKill);
void KillWindups(cCharacter* pCharacter);
void EmitWindupAtCharacter(cCharacter* pCharacter, const char* name);
void EmitWindupAtBall(cCharacter* pCharacter, const char* name);
void EmitShootToScoreHyperStrike(cFielder* pFielder);
void EmitGoalieCatch(cPlayer* pPlayer, const char* name, bool bRumble);
void EmitBallWallHit(const char* name);
void CharacterElectrocutionEffect(cCharacter* pCharacter, const nlVector3& v3Position, const nlVector3& v3Normal);
void EmitUnFreeze(cPlayer* pCharacter);
void EmitFreeze(cPlayer* pCharacter);
void KillDaze(cPlayer* player);
void EmitDaze(cPlayer* pCharacter);
void EmitElectrocutionExplosion(cCharacter* pCharacter);
void EmitSolidRumble(cPlayer* player);
void KillBallShot(const char* name, bool kill);
void EmitBallShot(cPlayer* pCharacter, eBallShotEffectType eNewBallEffect, cPlayer* pPassTarget, bool bSilent);
void EmitBallPass(cPlayer* pPlayer);
void EmitBallImpact(cPlayer* pPlayer, bool bSilent);
float GetCurrentAnimTriggerTime(cCharacter* pCharacter, unsigned long uTriggerID, unsigned int uInstanceNumber);
void GetAnimTriggerInfo(cCharacter* pCharacter, int animIndex, bool (*callback)(float, float, unsigned long, float, void*), void* pData);
void CharacterTriggerHandler(uintptr_t uParam);
EmissionController* EmitGeneric(cCharacter* pCharacter, const char* baseName, const char* characterName);
void UpdateEmitterFromBall(EmissionController& emitter);
void UpdateEmitterFromCharacterIdxWithCoordSys(EmissionController& ec, int characterIdx);
void UpdateEmitterFromCharacterIdxWithoutAnimController(EmissionController& emitter, int index);

#endif // _CHARACTERTRIGGERS_H_
