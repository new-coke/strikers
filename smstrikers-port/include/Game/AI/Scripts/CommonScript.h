#ifndef _COMMONSCRIPT_H_
#define _COMMONSCRIPT_H_

#include "Game/AI/Fuzzy.h"
#include "Game/AI/FuzzyVariant.h"

class cTeam;
class cPlayer;
class cFielder;

namespace Fuzzy
{
FuzzyVariant GetStrategicBallCarrier(cTeam* TheTeam);
FuzzyVariant GetBestBallInterceptor(cTeam* TheTeam);
FuzzyVariant GetSwapControllerScore(cPlayer* ThePlayer);
FuzzyVariant ShouldIStrafeBall(cFielder* TheFielder);
FuzzyVariant ShouldIStrafeMark(cFielder* TheFielder);
FuzzyVariant ShouldIMarkBallOwner(cFielder* pFielder);
FuzzyVariant ShouldIAttemptOneTimer(cFielder* TheFielder);
FuzzyVariant GetBestLooseBallPassTarget(cFielder* TheFielder);
FuzzyVariant GetBestPassTarget(cPlayer* ThePlayer);
FuzzyVariant GoodPassTargetFrom(cFielder* TheTargetFielder, cFielder* TheBallOwner);
FuzzyVariant GetBestHitTarget(cFielder* TheFielder);
FuzzyVariant GetPassDirection(cPlayer* pFromPlayer, cPlayer* pTargetPlayer);
FuzzyVariant GoodToShoot(cFielder* TheFielder);
FuzzyVariant GoodToChipShot(cFielder* TheFielder);
FuzzyVariant GetBestPassReceiveAction(cFielder* TheFielder);
FuzzyVariant GetBestLooseBallAction(cFielder* TheFielder);
FuzzyVariant GetBestWindupShotAction(cFielder* TheFielder);
FuzzyVariant GetPowerupToUseForPassReceiveDefence(cFielder* TheFielder);
FuzzyVariant GetPowerupToUseForWindupDefence(cFielder* TheFielder);
FuzzyVariant InDanger(cFielder* TheFielder);
FuzzyVariant InDangerDelayed(cFielder* TheFielder);
FuzzyVariant GoalieAndGonnaPickupBall(cPlayer* ThePlayer);
} // namespace Fuzzy

#endif // _COMMONSCRIPT_H_
