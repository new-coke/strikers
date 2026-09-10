#ifndef _DEFAULTOFFENSIVE_H_
#define _DEFAULTOFFENSIVE_H_

#include "Game/AI/DecisionEntity.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/FuzzyVariant.h"
#include "Game/Team.h"

class cPlayer;

// PORT: a namespace, as CommonScript.h and DefaultLoose.h already have it and as every definition is compiled.
namespace Fuzzy
{
FuzzyVariant AbortOffensivePlay(cDecisionEntity* pDecision);
FuzzyVariant DefaultOffensivePlay(cDecisionEntity* pDecision);
FuzzyVariant DoPassing(float fConfidence, cDecisionEntity* pDecision);
FuzzyVariant GetBestPassTarget(cPlayer*);
FuzzyVariant GetStrategicBallCarrier(cTeam*);
FuzzyVariant GoodBallCarrier(cFielder* TheFielder);
FuzzyVariant GoodToShoot(cFielder*);
FuzzyVariant GoodToChipShot(cFielder*);
FuzzyVariant InGoodWindupPosition(cFielder* TheFielder);
FuzzyVariant InDanger(cFielder*);
FuzzyVariant CutAndBreak(cFielder* TheFielder);
FuzzyVariant DoShooting(float fConfidence, cDecisionEntity* pDecision);
FuzzyVariant FurthestBackOnMyTeam(cFielder* TheFielder);
FuzzyVariant UsePowerupOffensive(float fConfidence, cDecisionEntity* pDecision);
FuzzyVariant GetPowerupTargetOffensive(cTeam* TheTeam);
FuzzyVariant InDangerDelayed(cFielder*);
} // namespace Fuzzy

#endif // _DEFAULTOFFENSIVE_H_
