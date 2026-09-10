
#include "Game/ScriptTuning.h"
#include "Game/AI/Scripts/ScriptCaching.h"
#include "Game/AI/Scripts/ScriptQuestions.h"
#include "NL/nlConfig.h"

/**
 * Offset/Address/Size: 0xBD4 | 0x800854E0 | size: 0x50
 */
FuzzyTweaks::FuzzyTweaks(const char* name)
    : TweaksBase(name)
{
    Init();
}

/**
 * Offset/Address/Size: 0xB74 | 0x80085480 | size: 0x60
 */
FuzzyTweaks::~FuzzyTweaks()
{
}

/**
 * Offset/Address/Size: 0x0 | 0x8008490C | size: 0xB74
 */
void FuzzyTweaks::Init()
{
    Config cfg(Config::ALLOCATE_HIGH);
    cfg.LoadFromFile(mszFileName);

    vCloseTeammateConfidenceDistance.x = cfg.Get<float>("Close 2 Teammate Min Dist", -9999.9f);
    vCloseTeammateConfidenceDistance.y = cfg.Get<float>("Close 2 Teammate Max Dist", -9999.9f);
    vNearTeammateConfidenceDistance.x = cfg.Get<float>("Near 2 Teammate Min Dist", -9999.9f);
    vNearTeammateConfidenceDistance.y = cfg.Get<float>("Near 2 Teammate Max Dist", -9999.9f);
    vFarTeammateConfidenceDistance.x = cfg.Get<float>("Far 2 Teammate Min Dist", -9999.9f);
    vFarTeammateConfidenceDistance.y = cfg.Get<float>("Far 2 Teammate Max Dist", -9999.9f);

    vCloseOpponentConfidenceDistance.x = cfg.Get<float>("Close 2 Opponent Min Dist", -9999.9f);
    vCloseOpponentConfidenceDistance.y = cfg.Get<float>("Close 2 Opponent Max Dist", -9999.9f);
    vNearOpponentConfidenceDistance.x = cfg.Get<float>("Near 2 Opponent Min Dist", -9999.9f);
    vNearOpponentConfidenceDistance.y = cfg.Get<float>("Near 2 Opponent Max Dist", -9999.9f);
    vFarOpponentConfidenceDistance.x = cfg.Get<float>("Far 2 Opponent Min Dist", -9999.9f);
    vFarOpponentConfidenceDistance.y = cfg.Get<float>("Far 2 Opponent Max Dist", -9999.9f);
    vReallyCloseToBallDistanceConfidence.x = cfg.Get<float>("ReallyClose 2 Ball Min Dist", -9999.9f);
    vReallyCloseToBallDistanceConfidence.y = cfg.Get<float>("ReallyClose 2 Ball Max Dist", -9999.9f);
    vCloseBallConfidenceDistance.x = cfg.Get<float>("Close 2 Ball Min Dist", -9999.9f);
    vCloseBallConfidenceDistance.y = cfg.Get<float>("Close 2 Ball Max Dist", -9999.9f);
    vNearBallConfidenceDistance.x = cfg.Get<float>("Near 2 Ball Min Dist", -9999.9f);
    vNearBallConfidenceDistance.y = cfg.Get<float>("Near 2 Ball Max Dist", -9999.9f);
    vFarBallConfidenceDistance.x = cfg.Get<float>("Far 2 Ball Min Dist", -9999.9f);
    vFarBallConfidenceDistance.y = cfg.Get<float>("Far 2 Ball Max Dist", -9999.9f);
    vCloseNetConfidenceDistance.x = cfg.Get<float>("Close 2 Net Min Dist", -9999.9f);
    vCloseNetConfidenceDistance.y = cfg.Get<float>("Close 2 Net Max Dist", -9999.9f);
    vNearNetConfidenceDistance.x = cfg.Get<float>("Near 2 Net Min Dist", -9999.9f);
    vNearNetConfidenceDistance.y = cfg.Get<float>("Near 2 Net Max Dist", -9999.9f);
    vFarNetConfidenceDistance.x = cfg.Get<float>("Far 2 Net Min Dist", -9999.9f);
    vFarNetConfidenceDistance.y = cfg.Get<float>("Far 2 Net Max Dist", -9999.9f);
    vCloseBallNetConfidenceDistance.x = cfg.Get<float>("Ball 2 Net Close Min Dist", -9999.9f);
    vCloseBallNetConfidenceDistance.y = cfg.Get<float>("Ball 2 Net Close Max Dist", -9999.9f);
    vNearBallNetConfidenceDistance.x = cfg.Get<float>("Ball 2 Net Near Min Dist", -9999.9f);
    vNearBallNetConfidenceDistance.y = cfg.Get<float>("Ball 2 Net Near Max Dist", -9999.9f);
    vFarBallNetConfidenceDistance.x = cfg.Get<float>("Ball 2 Net Far Min Dist", -9999.9f);
    vFarBallNetConfidenceDistance.y = cfg.Get<float>("Ball 2 Net Far Max Dist", -9999.9f);
    vCloseToFormationPositionDistance.x = cfg.Get<float>("Close 2 FormationPos Min Dist", -9999.9f);
    vCloseToFormationPositionDistance.y = cfg.Get<float>("Close 2 FormationPos Max Dist", -9999.9f);
    vNearToFormationPositionDistance.x = cfg.Get<float>("Near 2 FormationPos Min Dist", -9999.9f);
    vNearToFormationPositionDistance.y = cfg.Get<float>("Near 2 FormationPos Max Dist", -9999.9f);
    vFarToFormationPositionDistance.x = cfg.Get<float>("Far 2 FormationPos Min Dist", -9999.9f);
    vFarToFormationPositionDistance.y = cfg.Get<float>("Far 2 FormationPos Max Dist", -9999.9f);
    vCloseGoalieConfidenceDistance.x = cfg.Get<float>("Close 2 Goalie Min Dist", -9999.9f);
    vCloseGoalieConfidenceDistance.y = cfg.Get<float>("Close 2 Goalie Max Dist", -9999.9f);
    vNearGoalieConfidenceDistance.x = cfg.Get<float>("Near 2 Goalie Min Dist", -9999.9f);
    vNearGoalieConfidenceDistance.y = cfg.Get<float>("Near 2 Goalie Max Dist", -9999.9f);
    vFarGoalieConfidenceDistance.x = cfg.Get<float>("Far 2 Goalie Min Dist", -9999.9f);
    vFarGoalieConfidenceDistance.y = cfg.Get<float>("Far 2 Goalie Max Dist", -9999.9f);
    vCloseToSidelineDistanceConfidence.x = cfg.Get<float>("Close 2 Sideline Min Dist", -9999.9f);
    vCloseToSidelineDistanceConfidence.y = cfg.Get<float>("Close 2 Sideline Max Dist", -9999.9f);
    vNearToSidelineDistanceConfidence.x = cfg.Get<float>("Near 2 Sideline Min Dist", -9999.9f);
    vNearToSidelineDistanceConfidence.y = cfg.Get<float>("Near 2 Sideline Max Dist", -9999.9f);
    vFarFromSidelineDistanceConfidence.x = cfg.Get<float>("Far 2 Sideline Min Dist", -9999.9f);
    vFarFromSidelineDistanceConfidence.y = cfg.Get<float>("Far 2 Sideline Max Dist", -9999.9f);
    vHighBallConfidenceDistance.x = cfg.Get<float>("Ball Height High Min Dist", -9999.9f);
    vHighBallConfidenceDistance.y = cfg.Get<float>("Ball Height High Max Dist", -9999.9f);
    vReallyHighBallConfidenceDistance.x = cfg.Get<float>("Ball Height Really High Min Dist", -9999.9f);
    vReallyHighBallConfidenceDistance.y = cfg.Get<float>("Ball Height Really High Max Dist", -9999.9f);

    nFacingFullConfidenceAngle = (s16)cfg.Get<int>("Facing Angle Max", -0x270F);
    nFacingNoConfidenceAngle = (s16)cfg.Get<int>("Facing Angle Min", -0x270F);

    vControlConfidenceDistance.x = cfg.Get<float>("Ball Control Distance From Owner Min", -9999.9f);
    vControlConfidenceDistance.y = cfg.Get<float>("Ball Control Distance From Owner Max", -9999.9f);
    vIdealTacklingDistance.x = cfg.Get<float>("Slide Tackle Distance Ideal Max", -9999.9f);
    vIdealTacklingDistance.y = cfg.Get<float>("Slide Tackle Distance Ideal Min", -9999.9f);
    fPassLaneDistance = cfg.Get<float>("Pass Lane Max Width", -9999.9f);
    fShotLaneDistance = cfg.Get<float>("Shot Lane Max Width", -9999.9f);
    fPassInPlayFullConfidenceDistance = cfg.Get<float>("Pass In Play Dist 2 Target Max", -9999.9f);
    fShotInPlayFullConfidenceDistance = cfg.Get<float>("Shot In Play Dist 2 Target Max", -9999.9f);
    vOnGroundConfidenceDistance.x = cfg.Get<float>("Player OnGround Min Dist", -9999.9f);
    vOnGroundConfidenceDistance.y = cfg.Get<float>("Player OnGround Max Dist", -9999.9f);
    fInterceptBallSwapControlerScoreWeight = cfg.Get<float>("InterceptBall SwapController Time Weight", -9999.9f);
    fInterceptBallScoreWeight = cfg.Get<float>("InterceptBall Time Weight", -9999.9f);
    vInterceptBallConfidenceTime.x = cfg.Get<float>("InterceptBall Min Time", -9999.9f);
    vInterceptBallConfidenceTime.y = cfg.Get<float>("InterceptBall Max Time", -9999.9f);
    vInterceptBallConfidenceDistance.x = cfg.Get<float>("InterceptBall Min Dist", -9999.9f);
    vInterceptBallConfidenceDistance.y = cfg.Get<float>("InterceptBall Max Dist", -9999.9f);
    fPressuredNearWeight = cfg.Get<float>("Pressured Near Weight", -9999.9f);
    vAvoidBowserRepulsionConfidence.x = cfg.Get<float>("Avoid Bowser Min Repulsion", -9999.9f);
    vAvoidBowserRepulsionConfidence.y = cfg.Get<float>("Avoid Bowser Max Repulsion", -9999.9f);
    vAvoidGoalieRepulsionConfidence.x = cfg.Get<float>("Avoid Goalie Min Repulsion", -9999.9f);
    vAvoidGoalieRepulsionConfidence.y = cfg.Get<float>("Avoid Goalie Max Repulsion", -9999.9f);
    vAvoidFieldersRepulsionConfidence.x = cfg.Get<float>("Avoid Fielders Min Repulsion", -9999.9f);
    vAvoidFieldersRepulsionConfidence.y = cfg.Get<float>("Avoid Fielders Max Repulsion", -9999.9f);
    vAvoidPowerupsRepulsionConfidence.x = cfg.Get<float>("Avoid Powerups Min Repulsion", -9999.9f);
    vAvoidPowerupsRepulsionConfidence.y = cfg.Get<float>("Avoid Powerups Max Repulsion", -9999.9f);
    vPlayerShotDistance.x = cfg.Get<float>("Shot Distance Min", -9999.9f);
    vPlayerShotDistance.y = cfg.Get<float>("Shot Distance Max", -9999.9f);
    vIdealDistanceForShooting.x = cfg.Get<float>("Ideal Shot Distance", -9999.9f);
    vIdealDistanceForShooting.y = cfg.Get<float>("Ideal Shot Distance Delta", -9999.9f);
    vGoalieOutOfPositionDistance.x = cfg.Get<float>("Goalie Out Of Position Min", -9999.9f);
    vGoalieOutOfPositionDistance.y = cfg.Get<float>("Goalie Out Of Position Max", -9999.9f);
    vOutOfNetConfidenceDistance.x = cfg.Get<float>("OutOfNet Min Dist", -9999.9f);
    vOutOfNetConfidenceDistance.y = cfg.Get<float>("OutOfNet Max Dist", -9999.9f);
    fUpfieldMaxDistance = cfg.Get<float>("Upfield Max Distance", -9999.9f);
    fDownfieldMaxDistance = cfg.Get<float>("Downfield Max Distance", -9999.9f);
    fClosingSpeedMax = cfg.Get<float>("Closing Max Speed", -9999.9f);
    fSeparatingSpeedMax = cfg.Get<float>("Separating Max Speed", -9999.9f);
    fFrontOfNetMidAngle = cfg.Get<float>("InFrontOfNet Mid Angle", -9999.9f);
    fFrontOfNetMaxAngle = cfg.Get<float>("InFrontOfNet Max Angle", -9999.9f);
    fFrontOfNetMidScore = cfg.Get<float>("InFrontOfNet Mid Score", -9999.9f);
    vGetOpenPassLaneOffset.x = cfg.Get<float>("OpenToPosition Pass Lane Offset Min", -9999.9f);
    vGetOpenPassLaneOffset.y = cfg.Get<float>("OpenToPosition Pass Lane Offset Max", -9999.9f);
    vGetOpenPassLaneDist.x = cfg.Get<float>("OpenToPosition Pass Lane Dist Min", -9999.9f);
    vGetOpenPassLaneDist.y = cfg.Get<float>("OpenToPosition Pass Lane Dist Max", -9999.9f);
    vOpenRadius.x = cfg.Get<float>("OpenPosition Radius Min", -9999.9f);
    vOpenRadius.y = cfg.Get<float>("OpenPosition Radius Max", -9999.9f);
    vWideOpenRadius.x = cfg.Get<float>("WideOpen Radius Min", -9999.9f);
    vWideOpenRadius.y = cfg.Get<float>("WideOpen Radius Max", -9999.9f);
    vInBetweenInterceptRange.x = cfg.Get<float>("InBetween Intercept Range Min", -9999.9f);
    vInBetweenInterceptRange.y = cfg.Get<float>("InBetween Intercept Range Max", -9999.9f);
    vInBetweenConeWidth.x = cfg.Get<float>("InBetween Cone Width Min", -9999.9f);
    vInBetweenConeWidth.y = cfg.Get<float>("InBetween Cone Width Max", -9999.9f);
    fPassDeadZone = cfg.Get<float>("Pass Dead Thought Zone", -9999.9f);
    fLosingScoreDelta = cfg.Get<float>("Losing Score Delta", -9999.9f);
    fWinningScoreDelta = cfg.Get<float>("Winning Score Delta", -9999.9f);
    fTiedScoreDelta = cfg.Get<float>("Tied Score Delta", -9999.9f);
    fGameTimeCloseToOver = cfg.Get<float>("GameTime CloseTo Over", -9999.9f);
    fGameTimeNearlyOver = cfg.Get<float>("GameTime Nearly Over", -9999.9f);
    fGameTimeFarFromOver = cfg.Get<float>("GameTime FarFrom Over", -9999.9f);
    vDefensiveConfidenceDistances.x = cfg.Get<float>("Defensive Min Dist", -9999.9f);
    vDefensiveConfidenceDistances.y = cfg.Get<float>("Defensive Max Dist", -9999.9f);
    vOffensiveConfidenceDistances.x = cfg.Get<float>("Offensive Min Dist", -9999.9f);
    vOffensiveConfidenceDistances.y = cfg.Get<float>("Offensive Max Dist", -9999.9f);
}
