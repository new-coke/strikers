#ifndef _PLAYER_H_
#define _PLAYER_H_

#include "Game/Character.h"

#include "NL/nlTimer.h"
#include "NL/globalpad.h"

class cBall;
class cPoseAccumulator;
class cPN_SingleAxisBlender;
class cPN_SAnimController;
class cPN_Feather;
class CollisionPlayerWallData;
#include "Game/AI/AIPad.h"
class CollisionPlayerPlayerData;
class SpaceSearch;
class cFielder;
class cSHierarchy;
class cAnimInventory;
class CharacterPhysicsData;
class AnimRetargetList;

#include "Game/CharacterTweaks.h"

enum ePositionSeekState
{
    PSS_ARRIVED = 0,
    PSS_NEAR_SEEKING = 1,
    PSS_FAR_SEEKING = 2,
};

enum eBallRotationMode
{
    BRM_ANIMATED = 0,
    BRM_MATCH_VELOCITY = 1,
};

enum ePadActions
{
    PAD_CAMERA_UP = 0,
    PAD_CAMERA_DOWN = 1,
    PAD_CAMERA_ZOOM_IN = 2,
    PAD_CAMERA_ZOOM_OUT = 3,
    PAD_ENTER_REPLAY = 4,
    PAD_REPLAY_REVERSE = 5,
    PAD_REPLAY_FORWARD = 6,
    PAD_SKIP_PRESENTATION = 7,
    PAD_TOGGLE_TWEAKER = 8,
    PAD_TWEAKER_ACCEL = 9,
    PAD_TWEAKER_BACK = 10,
    PAD_DPAD_LEFT = 11,
    PAD_DPAD_RIGHT = 12,
    PAD_DPAD_UP = 13,
    PAD_DPAD_DOWN = 14,
    PAD_NEXT_ANIM = 15,
    PAD_PREV_ANIM = 16,
    PAD_NEXT_MODEL = 17,
    PAD_PREV_MODEL = 18,
    PAD_START_STOP_ANIM = 19,
    PAD_TURBO = 20,
    PAD_AIM = 21,
    PAD_USE = 22,
    PAD_TOGGLE_POWERUP = 23,
    PAD_HIT = 24,
    PAD_SLIDE_ATTACK = 25,
    PAD_SWITCH = 26,
    PAD_PASS = 27,
    PAD_SHOOT = 28,
    PAD_DEKE = 29,
    PAD_DEKE_LEFT = 30,
    PAD_DEKE_RIGHT = 31,
    PAD_RESET_PLAYER_HOLD = 32,
    PAD_RESET_PLAYER_PRESS = 33,
    PAD_SWITCH_STADIUM = 34,
    PAD_SWITCH_CAMERA = 35,
    PAD_START = 36,
    PAD_NONE = 37,
    PAD_NUM_ACTIONS = 38,
};

class cPlayer : public cCharacter
{
public:
    cPlayer(int nPlayerID, eCharacterClass characterClass, const int* nModelID, cSHierarchy* hierarchy, cAnimInventory* animInventory, const CharacterPhysicsData* physData, PlayerTweaks* playerTweaks, AnimRetargetList* animRetargetList, eClassTypes classType);
    virtual ~cPlayer();

    int GetUniqueID(int nTeamID) const;
    float SuggestPassTargetPosition(nlVector3& suggestedTarget, cPlayer* fromPlayer, bool volleyPass, bool bIsPerfectPass);
    bool SuggestPassDirection(nlVector3& suggestedDirection, cPlayer* fromPlayer, bool volleyPass, bool bIsPerfectPass);
    void SetNoPickUpTime(float NewNoPickUpTime);
    nlVector3 GetAIDefNetLocation(const nlVector3* v3ReferencePos);
    nlVector3 GetAIOffNetLocation(const nlVector3* v3ReferencePos);
    bool CanPickupBallFromPass(cBall* pBall);
    virtual bool CanPickupBall(cBall* pBall);
    virtual void PostPhysicsUpdate();
    virtual void PreUpdate(float dt);
    virtual void PrePhysicsUpdate(float dt);
    static void PlayerHeadTrackCallback(uintptr_t nSelf, unsigned int nParam2, cPoseAccumulator* pPoseAccumulator,
        unsigned int nJointIndex, int nParentIndex);
    cPN_SingleAxisBlender* CreateSingleAxisBlender(const int* pSABAnims, int nNumSABAnims, int nPrimaryAnim, void (*fWeightCB)(uintptr_t, cPN_SingleAxisBlender*), float fWeightSeek, cPN_SAnimController* pSynchingController);
    virtual void CollideWithBallCallback(cBall* pBall);
    virtual void CollideWithCharacterCallback(CollisionPlayerPlayerData* pData);
    virtual void CollideWithWallCallback(const CollisionPlayerWallData* pData);
    void SetPowerupAnimState(int animID);
    void ClearSwapControllerTimer();
    void ClearPowerupAnimState(bool bIsEndGame);
    void DoRegularPassing(cPlayer* pTeammate, bool bVolleyPass, bool bAllowLeadPass, bool bParam3, bool bParam4);
    void ResetUnPossessionTimer();
    void ReleaseBall();
    cGlobalPad* GetGlobalPad();
    cPlayer* DoFindBestPassTarget(bool bAllowLeadPass, bool bIsPerfectPass);
    bool IsCaptain() const;
    bool IsOnSameTeam(cPlayer* other);
    cTeam* GetTeam() const { return m_pTeam; }
    bool HasBall() const { return m_pBall != NULL; }
    int GetBallJointIndex() const { return m_nBallJointIndex; }
    void SetAIPad(cAIPad* pPad);
    void PlayAttackReactionSounds(float fScale);
    void PickupBall(cBall* pBall);
    cFielder* GetClosestOpponentFielder(nlVector3* pPosition);
    float DoFlashLight(const nlVector3& Position, unsigned short aDirection, float fAngleWeighting, float fIgnoreObjectCloserThanThis, float fIgnoreObjectFartherThanThis);
    static float DoFlashLight(const nlVector3& Position1, const nlVector3& Position2, unsigned short aDirection, float fAngleWeighting, float fIgnoreObjectCloserThanThis, float fIgnoreObjectFartherThanThis);
    virtual void SetAnimID(int animID);
    void GetAnimatedBallOrientation(nlQuaternion& qRetval);
    virtual void Update(float fDeltaT);
    u8 SwapController();
    void SetDesiredFacingDirection();
    void ResetDesiredDirections(unsigned short direction);
    void SetSpaceSearch(SpaceSearch* pSpaceSearch);
    virtual void InitActionPostWhistle() { };

    /* 0x12C */ s32 m_ID;                                // offset 0x12C, size 0x4
    /* 0x130 */ bool m_bIsContactingWall;                // offset 0x130, size 0x1
    /* 0x134 */ ePositionSeekState m_ePositionSeekState; // offset 0x134, size 0x4
    /* 0x138 */ nlVector3 m_v3AIPosition;                // offset 0x138, size 0xC
    /* 0x144 */ eBallRotationMode m_eBallRotationMode;   // offset 0x144, size 0x4
    /* 0x148 */ bool m_ResetBaseBallOrientation;         // offset 0x148, size 0x1
    /* 0x14C */ nlQuaternion m_BaseBallOrientation;      // offset 0x14C, size 0x10
    /* 0x15C */ Timer m_tBallPossessionTimer;            // offset 0x15C, size 0x4
    /* 0x160 */ Timer m_tBallUnPossessionTimer;          // offset 0x160, size 0x4
    /* 0x164 */ Timer m_tNoPickupTimer;                  // offset 0x164, size 0x4
    /* 0x168 */ Timer m_tNoPickupPassInterceptTimer;     // offset 0x168, size 0x4
    /* 0x16C */ f32 m_fShotStrengthTime;                 // offset 0x16C, size 0x4
    /* 0x170 */ Timer m_tSlideAttackTimer;               // offset 0x170, size 0x4
    /* 0x174 */ Timer m_tLooseBallPassTimer;             // offset 0x174, size 0x4
    /* 0x178 */ Timer m_tInactivityTimer;                // offset 0x178, size 0x4
    /* 0x17C */ Timer m_tSwapControllerTimer[4];         // offset 0x17C, size 0x10
    /* 0x18C */ bool m_bCanTestController;               // offset 0x18C, size 0x1
    /* 0x190 */ ePadActions m_eLastPadAction;            // offset 0x190, size 0x4
    /* 0x194 */ u16 m_aSwapFacingDirection;              // offset 0x194, size 0x2
    /* 0x198 */ Timer m_tSwapFacingTimer;                // offset 0x198, size 0x4
    /* 0x19C */ f32 m_UserControlledTime;                // offset 0x19C, size 0x4
    /* 0x1A0 */ cPN_Feather* m_pPowerupLayer;            // offset 0x1A0, size 0x4
    /* 0x1A4 */ cPN_Feather* m_pReceivePassLayer;        // offset 0x1A4, size 0x4
    /* 0x1A8 */ s32 m_nBip01JointIndex;                  // offset 0x1A8, size 0x4
    /* 0x1AC */ s32 m_nBallJointIndex;                   // offset 0x1AC, size 0x4
    /* 0x1B0 */ s32 m_nRightFootJointIndex;              // offset 0x1B0, size 0x4
    /* 0x1B4 */ s32 m_nLeftFootJointIndex;               // offset 0x1B4, size 0x4
    /* 0x1B8 */ s32 m_nLeftHandJointIndex;               // offset 0x1B8, size 0x4
    /* 0x1BC */ s32 m_nRightHandJointIndex;              // offset 0x1BC, size 0x4
    /* 0x1C0 */ cAIPad* m_pController;                   // offset 0x1C0, size 0x4
    /* 0x1C4 */ PlayerTweaks* m_pTweaks;                 // offset 0x1C4, size 0x4
    /* 0x1C8 */ cBall* m_pBall;                          // offset 0x1C8, size 0x4
    /* 0x1CC */ cTeam* m_pTeam;                          // offset 0x1CC, size 0x4
    /* 0x1D0 */ SpaceSearch* m_pSpaceSearch;             // offset 0x1D0, size 0x4
}; // total size: 0x1D4

#endif // _PLAYER_H_
