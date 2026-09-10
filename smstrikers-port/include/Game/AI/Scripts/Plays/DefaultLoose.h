#ifndef _DEFAULTLOOSE_H_
#define _DEFAULTLOOSE_H_

#include "Game/AI/DecisionEntity.h"
#include "Game/AI/FuzzyVariant.h"

class cTeam;
class cPlayer;

namespace Fuzzy
{
FuzzyVariant AbortLoosePlay(cDecisionEntity* pDecision);
FuzzyVariant DefaultLoosePlay(cDecisionEntity* pDecision);
FuzzyVariant GetBestBallInterceptor(cTeam*);
FuzzyVariant GoalieAndGonnaPickupBall(cPlayer*);
} // namespace Fuzzy

#endif // _DEFAULTLOOSE_H_
