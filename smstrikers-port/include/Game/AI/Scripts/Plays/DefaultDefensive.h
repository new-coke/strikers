#ifndef _DEFAULTDEFENSIVE_H_
#define _DEFAULTDEFENSIVE_H_

#include "Game/AI/DecisionEntity.h"
#include "Game/AI/FuzzyVariant.h"
#include "Game/Team.h"

class cFielder;

namespace Fuzzy
{
FuzzyVariant AbortDefencePlay(cDecisionEntity* pEntity);
FuzzyVariant DefaultDefencePlay(cDecisionEntity* pDecision);
FuzzyVariant DefendPassInPlay(float fConfidence, cDecisionEntity* pEntity);
FuzzyVariant TryAttacking(float fConfidence, cDecisionEntity* pEntity);
FuzzyVariant AttackBallOwner(float fConfidence, cDecisionEntity* pEntity);
FuzzyVariant UsePowerupDefensive(float fConfidence, cDecisionEntity* pEntity);
FuzzyVariant GetPowerupTargetDefensive(cTeam* TheTeam);
FuzzyVariant InGoodWindupPosition(cFielder*);
} // namespace Fuzzy

#endif // _DEFAULTDEFENSIVE_H_
