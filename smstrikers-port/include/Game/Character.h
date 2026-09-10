#ifndef _CHARACTER_H_
#define _CHARACTER_H_

#include "types.h"
#include "NL/nlMath.h"

#include "Game/AI/HeadTrack.h"
#include "Game/SAnim/pnSAnimController.h"

#include "Game/CharacterEffects.h"
#include "Game/Blinker.h"

#include "Game/Physics/PhysicsCharacter.h"
#include "Game/CharacterAudio.h"

#include "Game/PoseNode.h"
#include "Game/PoseAccumulator.h"

#include "Game/ObjectBlur.h"
#include "Game/GL/gluSkinMesh.h"

class Event;

#include "Game/EventDataTypes.h"

// Forward declarations
class cSHierarchy;
class cAnimInventory;
class CharacterPhysicsData;
class AnimRetargetList;
class GLSkinMesh;
struct glModel;
class EmissionController;
class EffectsGroup;
class cTeam;
class cPlayer;

enum eCharacterClass
{
    CHARACTER_CLASS_INVALID = -1,
    BIRDO = 0,
    DAISY = 1,
    DONKEYKONG = 2,
    HAMMERBROS = 3,
    KOOPA = 4,
    LUIGI = 5,
    MARIO = 6,
    PEACH = 7,
    TOAD = 8,
    WALUIGI = 9,
    WARIO = 10,
    YOSHI = 11,
    MYSTERY = 12,
    NUM_FIELDER_CLASSES = 13,
    DAISY_GOALIE = 13,
    DONKEYKONG_GOALIE = 14,
    LUIGI_GOALIE = 15,
    MARIO_GOALIE = 16,
    PEACH_GOALIE = 17,
    WALUIGI_GOALIE = 18,
    WARIO_GOALIE = 19,
    YOSHI_GOALIE = 20,
    SUPERTEAM_GOALIE = 21,
    NUM_CHARACTER_CLASSES = 22,
    NUM_GOALIE_CLASSES = 9,
};

enum eMovementState
{
    MOVEMENT_COAST = 0,
    MOVEMENT_DECELERATE_EXPONENTIAL = 1,
    MOVEMENT_FROM_ANIM = 2,
    MOVEMENT_FROM_ANIM_SEEK = 3,
    MOVEMENT_NONE = 4,
    MOVEMENT_RUNNING = 5,
    MOVEMENT_RUNNING_NO_TURN = 6,
    MOVEMENT_STRAFING = 7,
    MOVEMENT_UNUSED = 8,
};

enum eClassTypes
{
    CHARACTER = 0,
    PLAYER = 1,
    FIELDER = 2,
    GOALIE = 3,
    NUM_CLASSES = 4,
};

enum eCharacterModelType
{
    CharModel_Rigid = 0,
    CharModel_Blend = 1,
    CharModel_Num = 2,
};

#include "Game/Drawable/DrawableObj.h"

class cCharacter //: public PhysicsCharacterBase
{
public:
    virtual ~cCharacter();
    cCharacter(eCharacterClass cc, const int* nModelID, cSHierarchy* pHierarchy, cAnimInventory* pAnimInventory, const CharacterPhysicsData* pPhysicsData, float fPhysicsCapsuleHeight, float fPhysicsCapsuleWidth, AnimRetargetList* pAnimRetargetList,
        eClassTypes eNewClassType);

    virtual void PostPhysicsUpdate();
    virtual void PrePhysicsUpdate(float dt);
    virtual void PreUpdate(float dt);
    virtual void SetAnimID(int animID);

    void AddRandomDirt();
    void SetElectrocutionTextureEnabled(bool isEnabled);
    void PerformBlinking(GLSkinMesh* skinMesh, glModel* model) const;
    void UpdateBlinking(float fDeltaT);
    void PlayRandomCharDialogue(unsigned long dialogueType, PosUpdateMethod posUpdateMethod, float f1, float f2);
    int Play3DSFX(Audio::eCharSFX sfxType, PosUpdateMethod posUpdateMethod, float fMaxVol);
    void StopPlayingAllTrackedSFX();
    void StopSFX(Audio::eCharSFX sfxType);
    int PlaySFX(Audio::SoundAttributes& attributes);
    void SetSFX(SoundPropAccessor* pSoundPropAccessor);
    void UpdateMovementState(float fDeltaT);
    bool IsPlayingEffect(const EffectsGroup* effectGroup) const;
    void EndEffect(const EffectsGroup* effectGroup);
    void KillEffect(const EffectsGroup* effectGroup);
    bool ShouldStartCrossBlend(int animID);
    void SetVelocity(const nlVector3& velocity);
    virtual void SetPosition(const nlVector3& position);
    void SetFacingDirection(unsigned short dir);
    void SetAnimState(int animID, bool useBlendTime, float fNonDefaultBlendTime, bool bRestartCyclic, bool bForceMirrorSwap);
    float SeekSpeedExponential(float currentValue, float targetValue, float responsiveness, float deltaTime);
    void ResetEffects();
    void CreateWorldMatrix();
    void PoseSkinMesh(cPoseAccumulator* pPoseAccumulator);
    void PoseLocalSpace();
    cPN_SAnimController* NewAnimController(int animID, bool bRestartCyclic, bool bForceMirrorSwap, void (*funcPlaybackSpeedCallback)(uintptr_t, cPN_SAnimController*), uintptr_t nPlaybackSpeedCallbackParam);
    static void MatchAnimSpeedToCharacterSpeed(uintptr_t nParam, cPN_SAnimController* pController);
    void InitMovementStrafing(float fDirectionSeekSpeed, float fDirectionSeekFalloff, float fAccel, float fDecel);
    void InitMovementRunningNoTurn(float fAccel, float fDecel);
    void InitMovementRunning(float fDirectionSeekSpeed, float fDirectionSeekFalloff, float fAccel, float fDecel);
    void InitMovementNone(float fDirectionSeekSpeed, float fDirectionSeekFalloff);
    void InitMovementFromAnimSeek(float fDirectionSeekSpeed, float fDirectionSeekFalloff);
    void InitMovementFromAnim(short fDirectionSeekSpeed, const nlVector3& v3AnimMoveAdjust, float fAdjustEndTime, bool bBlended);
    void InitMovementDecelerateExponential(float fDecel);
    void InitMovementCoast();
    void EndBlur();
    void InitBlur(int nLength);
    nlVector3& GetPrevJointPosition(int jointIndex);
    void GetCurrentAnimFuture(int nJointIndex, float fTime, nlVector3& v3Out, nlVector3& v3FutureRoot, unsigned short& outFacing);
    void GetJointPositionFuture(nlVector3* v3Out, int nAnimIndex, int nJointIndex, float fTime, bool bAddRootTrans, bool bAddRootRot, bool bUsePrevPosition);
    nlVector3& GetJointPosition(int jointIndex) const;
    s16 GetFacingDeltaToPosition(const nlVector3& position);
    s16 CalcAnimTurnAdjust(unsigned short aFacingDirection, unsigned short aDesiredFacingDirection, int nAnimID);
    void AttachEffect(EmissionController* pEmissionController);
    void AdjustPoseMatrices();
    GLSkinMesh* GetSkinMesh() const;

    cAnimInventory* GetAnimInventory() const
    {
        return m_pAnimInventory;
    }

    int GetHeadJointIndex() const // does not omit code?
    {
        return m_nHeadJointIndex;
    }

    inline bool IsTurboingAnimID()
    {
        int animID = m_eAnimID;
        bool result = false;
        if (!(animID != 0x1D && animID != 0x1E && animID != 0x1F && animID != 0x20 && animID != 0x21 && animID != 0x22))
        {
            result = true;
        }
        return result;
    }

    inline bool IsTurboingWithoutBallAnimID()
    {
        int animID = m_eAnimID;
        bool result = false;
        if (!(animID != 0x10 && animID != 0x0F && animID != 0x11))
        {
            result = true;
        }
        return result;
    }

    inline const nlVector3& GetPosition() const
    {
        return m_v3Position;
    }

    inline const nlVector3& GetVelocity() const
    {
        return m_v3Velocity;
    }

    inline u16 GetDesiredFacing() const
    {
        return m_aDesiredFacingDirection;
    }

    inline u16 GetActualFacing() const
    {
        return m_aActualFacingDirection;
    }

    virtual void Update(float fDeltaT);

    /* 0x04 */ eCharacterClass m_eCharacterClass;
    /* 0x08 */ const CharacterPhysicsData* m_pPhysicsData;
    /* 0x0C */ GLSkinMesh* m_pSkinMesh[2];
    /* 0x14 */ PhysicsCharacter* m_pPhysicsCharacter;
    /* 0x18 */ nlVector3 m_v3Position;
    /* 0x24 */ nlVector3 m_v3PrevPosition;
    /* 0x30 */ nlVector3 m_v3Velocity;
    /* 0x3C */ eMovementState m_eMovementState;
    /* 0x40 */ u16 m_aDesiredFacingDirection;
    /* 0x42 */ u16 m_aActualFacingDirection;
    /* 0x44 */ u16 m_aPrevFacingDirection;
    /* 0x46 */ u16 m_aDesiredMovementDirection;
    /* 0x48 */ u16 m_aActualMovementDirection;
    /* 0x4A */ bool m_bFromAnimBlended;
    /* 0x4C */ float m_fAnimAdjustBeginTime;
    /* 0x50 */ float m_fAnimAdjustEndTime;
    /* 0x54 */ float m_fDirectionSeekSpeed;
    /* 0x58 */ float m_fDirectionSeekFalloff;
    /* 0x5C */ float m_fAccel;
    /* 0x60 */ float m_fDecel;
    /* 0x64 */ float m_fDesiredSpeed;
    /* 0x68 */ float m_fActualSpeed;
    /* 0x6C */ float m_fLeanAmount;
    /* 0x70 */ s16 m_nAnimTurnAdjust;
    /* 0x74 */ nlVector3 m_v3AnimMoveAdjust;
    /* 0x80 */ cAnimInventory* m_pAnimInventory;
    /* 0x84 */ cPoseAccumulator* m_pPoseAccumulator;
    /* 0x88 */ cPoseNode* m_pPoseTree;
    /* 0x8C */ cPoseNode** m_pAILayer;
    /* 0x90 */ cPN_SAnimController* m_pCurrentAnimController;
    /* 0x94 */ int m_eAnimID;
    /* 0x98 */ AnimRetargetList* m_pAnimRetargetList;
    /* 0x9C */ cHeadTrack* m_pHeadTrack;
    /* 0xA0 */ s32 m_nHeadJointIndex;
    /* 0xA4 */ s32 m_nBip01JointIndex_0xA4; // strange, exists twice... (in character and player)
    /* 0xA8 */ s32 m_nPropJointIndex;
    /* 0xAC */ s32 m_nSpine1JointIndex;
    /* 0xB0 */ const char* m_szEffectsName;
    /* 0xB4 */ eClassTypes m_eClassType;
    /* 0xB8 */ bool m_bIsUsingElectrocutionTexture;
    /* 0xBC */ Audio::cCharacterSFX* m_pCharacterSFX;
    /* 0xC0 */ DrawableObject* m_pPropModel;
    /* 0xC4 */ u32 m_uNormalTextureID;
    /* 0xC8 */ u32 m_uSwapTextureID;

    /* 0xCC */ nlMatrix4 m_m4WorldMatrix;
    /* 0x10C */ float m_Dirt;
    /* 0x110 */ float m_MinDirt;
    /* 0x114 */ BlurHandler* m_pBlurHandler;
    /* 0x118 */ Blinker* m_pBlinker;
    /* 0x11C */ EffectsTexturing* m_pEffectsTexturing;
    /* 0x120 */ nlVector3 m_v3ScreenPosition;

    inline cPoseNode* GetAILayer() const
    {
        return m_pAILayer[0];
    }

    static eCharacterModelType m_ModelType;
};

// class cPN_SAnimController
// {
// public:
// };

// class cHeadTrack
// {
// public:
// };

// class PhysicsCharacter
// {
// public:
// };

// class GLSkinMesh
// {
// public:

//     /* 0x04 */ glModel* pModel;
// };

#endif // _CHARACTER_H_
