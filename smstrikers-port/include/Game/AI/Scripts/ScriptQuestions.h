#ifndef _SCRIPTQUESTIONS_H_
#define _SCRIPTQUESTIONS_H_

#include "NL/nlMath.h"
#include "Game/Player.h"
#include "Game/Ball.h"
#include "Game/Team.h"
#include "Game/Game.h"
#include "Game/AI/Fielder.h"

extern cFielder* g_pScriptCurrentFielder;
extern cFielder* g_pScriptCurrentMark;

enum eScriptFielderDesire
{
    edNone = 0,
    edCutAndBreak = 1,
    edBlockPass = 6,
    edBlockShot = 6,
    edDeke = 2,
    edGetInPosition = 3,
    edGetOpen = 4,
    edHeavyAttack = 5,
    edInterceptBall = 6,
    edMark = 7,
    edProtectBall = 8,
    edRunToNet = 9,
    edRunUpfield = 10,
    edRunDownfield = 11,
    edRunToLocation = 12,
    edPass = 13,
    edShoot = 14,
    edSlideAttack = 15,
    edSupportBallDefensive = 16,
    edSupportBallOffensive = 17,
    edUsePowerup = 18,
    edWindupPass = 19,
    edWindupShot = 20,
    edUserControl = 22,
    edOneTimer = 24,
    edPostWhistle = 25,
    edWait = 28,
};

float InOffensiveZoneOfPlayer(cBall* pBall, cPlayer* pPlayer);
float InDefensiveZoneOfPlayer(cBall* pBall, cPlayer* pPlayer);
float InOffensiveZone(cPlayer* pPlayer);
float InDefensiveZone(cPlayer* pPlayer);
float InOffensiveZone(const nlVector3& v3Position, eTeamSide teamside);
float Difficult(cTeam* pTeam);
float TimeFarFromOver(cGame* pGame);
float TimeNearlyOver(cGame* pGame);
float TimeCloseToOver(cGame* pGame);
float PerfectPassCandidateFrom(cFielder* pReceiver, cFielder* pBallOwner);
float IsPerfectPassInPlay();
float IsPassInPlayDelayed();
float Stalling(cTeam* pTeam);
float Loose(cTeam* pTeam);
float Defensive(cTeam* pTeam);
float Offensive(cTeam* pTeam);
float Winning(cTeam* team);
float Tied(cTeam* team);
float Losing(cTeam* team);
float GonnaGetBall(cTeam* team);
float UserControlledT(cTeam* team);
float Passive(cTeam* team);
float Moderate(cTeam* team);
float AggressiveT(cTeam* team);
float Ownerless(cBall* ball);
float High(cBall* ball);
float ReceivingVolleyPassDelayed(cPlayer* pPlayer);
float PassReceiveCloseToDone(cFielder* pFielder);
float ReceivingPassDelayed(cFielder* pFielder);
float ReceivingVolleyPass(cPlayer* pPlayer);
float ReceivingPass(cFielder* pFielder);
float ChasingBall(cPlayer* pPlayer);
float OnMushrooms(cFielder* fielder);
float WindingUpForShot(cFielder* fielder);
float InControlOfBall(cFielder* fielder);
float FarToPlayersNet(cBall* ball, cPlayer* player);
float NearToPlayersNet(cBall* ball, cPlayer* player);
float CloseToPlayersNet(cBall* ball, cPlayer* player);
float FarToTheirNetB(cBall* ball);
float NearToTheirNetB(cBall* ball);
float CloseToTheirNetB(cBall* ball);
float Stunned(Goalie* pGoalie);
float OutOfNet(Goalie* pGoalie);
float SeparatingFrom(cPlayer* pFielder1, cPlayer* pFielder2);
float ClosingTo(cPlayer* pPlayer, cBall* pBall);
float ClosingTo(cPlayer* pFielder1, cPlayer* pFielder2);
float DownfieldFrom(cPlayer* pDownfieldPlayer, cPlayer* pFromPlayer);
float UpfieldFrom(cPlayer* pUpfieldPlayer, cPlayer* pFromPlayer);
float Facing(cPlayer* pCandidateFielder, cPlayer* pTargetFielder);
float PositionIsAtIdealDistanceForShooting(const nlVector3& vPosition, const nlVector3& vOffNetPosition);
float AtIdealDistanceForTackling(cPlayer* pFielder1, cPlayer* pFielder2);
float StuckOnSidelines(cFielder* pFielder);
float FacingSideline(cFielder* pFielder);
float CloseToSideline(cFielder* pFielder);
float NearToSideline(const nlVector3& v3Position);
float CloseToSideline(const nlVector3& v3Position, const nlVector2* vDistanceConfidence, bool bInvert);
float FarToTheirGoalie(cPlayer* pPlayer);
float NearToTheirGoalie(cPlayer* pPlayer);
float CloseToTheirGoalie(cPlayer* pPlayer);
float NearToGoaliePosition(const nlVector3& v3FromPos, const nlVector3& v3GoaliePos);
float FarTo(cPlayer* pPlayer1, cPlayer* pPlayer2);
float NearTo(cPlayer* pPlayer1, cPlayer* pPlayer2);
float CloseTo(cPlayer* pPlayer1, cPlayer* pPlayer2);
float OpenTo(cPlayer* pFromFielder, cPlayer* pToFielder);
float InBetweenMyNetAnd(cFielder* pFielder, cBall* pBall);
float InBetweenMyNetAnd(cFielder* pFielder, cFielder* pOtherFielder);
float OpenToMyNet(cFielder* pFielder);
float OpenToTheirNet(cFielder* pFielder);
float WideOpen(cFielder* pFielder);
float Open(cFielder* pFielder);
float WideOpenPosition(const nlVector3& v3Position, cTeam* pOpponentTeam, cPlayer* pCurrentPlayer);
float OpenPosition(const nlVector3& v3Position, cTeam* pOpponentTeam, cPlayer* pCurrentPlayer, const nlVector2* vOpenRadius);
float OpenToPosition(const nlVector3& v3FromPos, const nlVector3& v3ToPos, const cTeam* pBlockingTeam, const cPlayer* pIgnorePlayer1, const cPlayer* pIgnorePlayer2, bool bIgnoreGoalies);
float OnBreakaway(cFielder* pFielder);
float InFrontOfTheirNet(cFielder* pFielder);
float PositionIsInFrontOfNet(const nlVector3& v3Position, const cNet* pNet);
float GoalieOutOfPosition(cFielder* pFielder);
float LikelyToScore(cFielder* pFielder);
float PlayerShotDistance(cFielder* pFielder);
float LikelyToScoreFromPosition(const nlVector3& v3Position, const nlVector3& v3GoaliePosition, const cNet* pNet, bool bIsChipShot);
float FallenDown(cFielder* pFielder);
float Frozen(cFielder* pFielder);
float Incapacitated(cPlayer* pPlayer);
float Invincible(cFielder* pFielder);
float AvoidingPowerups(cFielder* pFielder);
float Attacked(cFielder* pFielder);
float Pressured(cFielder* pFielder);
float FarToTheirNet(cPlayer* pPlayer);
float NearToTheirNet(cPlayer* pPlayer);
float CloseToTheirNet(cPlayer* pPlayer);
float FarToMyNet(cPlayer* pPlayer);
float NearToMyNet(cPlayer* pPlayer);
float CloseToMyNet(cPlayer* pPlayer);
float FarToBall(cPlayer* pPlayer);
float NearToBall(cPlayer* pPlayer);
float CloseToBall(cPlayer* pPlayer);
float LikelyToUsePowerup(cFielder* pFielder, int ePowerup);
float AbleToUsePowerup(cFielder* pFielder, int ePowerup);
float AbleToInterceptBallForSwapController(cFielder* pFielder);
float AbleToInterceptBall(cPlayer* pPlayer);
float RepeatingLastDesire(cFielder* fielder, eScriptFielderDesire eDesire);
float Deker(cFielder* fielder);
float Passer(cFielder* fielder);
float Shooter(cFielder* fielder);
float Aggressive(cFielder* fielder);
float InPassingLane(cFielder* pFielder);
float UserControlled(cFielder* fielder);
float StrategicBallOwner(cFielder* pFielder);
float OnTheGround(cPlayer* player);
float OnScreen(cPlayer* player);
float OnTheirTeam(cFielder* fielder);
float Marking(cFielder* pMarking, cPlayer* pMarked);
float GoalieType(cPlayer* player);
float Captain(cFielder* fielder);
float Defence(cFielder* fielder);
float Midfield(cFielder* fielder);
float Winger(cFielder* fielder);
float Striker(cFielder* fielder);
float LastBallOwner(cPlayer* player);
float BallOwnerT(cTeam* team);
float BallOwner(cPlayer* player);
float CalcSelectChance(float fDifficultyChance, float fPlayerAttribute);

static float FacingMark(cFielder* fielder)
{
    return Facing(fielder, g_pScriptCurrentMark);
}

static float FacingMe(cFielder* fielder)
{
    return Facing(fielder, g_pScriptCurrentFielder);
}

static float FarToMark(cFielder* fielder)
{
    return FarTo(fielder, g_pScriptCurrentMark);
}

static float NearToMark(cFielder* fielder)
{
    return NearTo(fielder, g_pScriptCurrentMark);
}

static float CloseToMark(cFielder* fielder)
{
    return CloseTo(fielder, g_pScriptCurrentMark);
}

static float OpenFromMe(cPlayer* fielder)
{
    return OpenTo(g_pScriptCurrentFielder, fielder);
}

static float OpenToMe(cPlayer* fielder)
{
    return OpenTo(fielder, g_pScriptCurrentFielder);
}

static float FarToMe(cPlayer* fielder)
{
    return FarTo(fielder, g_pScriptCurrentFielder);
}

static float NearToMe(cPlayer* fielder)
{
    return NearTo(fielder, g_pScriptCurrentFielder);
}

static float CloseToMe(cPlayer* fielder)
{
    return CloseTo(fielder, g_pScriptCurrentFielder);
}

static float FarToMyNetB(cBall* ball)
{
    return FarToPlayersNet(ball, g_pScriptCurrentFielder);
}

static float NearToMyNetB(cBall* ball)
{
    return NearToPlayersNet(ball, g_pScriptCurrentFielder);
}

static float CloseToMyNetB(cBall* ball)
{
    return CloseToPlayersNet(ball, g_pScriptCurrentFielder);
}

template <typename T>
nlVector3& PositionOf(T pObject)
{
    return pObject->m_v3Position;
}

#endif // _SCRIPTQUESTIONS_H_
