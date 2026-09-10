#ifndef _DRAWABLECHARACTER_H_
#define _DRAWABLECHARACTER_H_

#include "Game/Replay.h"
#include "Game/Drawable/DrawableObj.h"
#include "Game/CharacterEffects.h"
#include "Game/Render/Bowser.h"

class cCharacter;
class cPoseNode;
class SkinAnimatedMovableNPC;
class SkinAnimatedMovableNPC;
class cPoseAccumulator;

class LoadFrame;
class LoadFrame;

class DrawableCharacter
{
public:
    template <typename T>
    void Replay(T&);

    DrawableCharacter();
    ~DrawableCharacter();
    DrawableCharacter& operator=(const DrawableCharacter& other);

    void Free();
    cPN_SAnimController& GetAnimController() const;
    void Grab(cCharacter& character);
    static void DrawableBowserHeadTrackCallback(uintptr_t ctx, unsigned int nParam2, cPoseAccumulator* poseAccumulator, unsigned int currentNodeIndex, int nParentIndex);
    void BuildNodeMatrices();
    void Render(cCharacter& character) const;
    void SendToGl(const cCharacter& character) const;
    void Grab(SkinAnimatedMovableNPC& npc);
    void Render(SkinAnimatedMovableNPC& npc) const;
    void Blend(const float* blendFactors, const DrawableCharacter& lhs, const DrawableCharacter& rhs);
    void EvaluateFrom(const cPoseNode& poseNode, const nlVector3& offset, unsigned short facingAngle);
    nlVector3 GetBallPosition() const;
    nlQuaternion GetBallOrientation() const;

    static void RenderOnlyOneCharacter(const cCharacter& character, bool renderOpposingGoalieToo);
    static void RenderAllCharacters();
    static cCharacter* OnlyRenderingOneCharacter();

    /* 0x00 */ bool mVisible;
    /* 0x04 */ nlVector3 mPosition;
    /* 0x10 */ nlVector3 mBip01Position;
    /* 0x1C */ nlVector3 mHeadPosition;
    /* 0x28 */ float mHeight;
    /* 0x2C */ nlVector3 mVelocity;
    /* 0x38 */ unsigned short mFacingDirection;
    /* 0x3A */ unsigned short mHeadSpin;
    /* 0x3C */ unsigned short mHeadTilt;
    /* 0x40 */ cPoseNode* mPoseTree;
    /* 0x44 */ cPoseAccumulator* mPoseAccumulator;
    /* 0x48 */ EffectsTexturing* mEffectsTexturing;
    /* 0x4C */ cCharacter* mCharacter;
    /* 0x50 */ Bowser* mBowser;
    /* 0x54 */ unsigned char mDirt;

    static bool sCameraRelativeLighting;
    static unsigned char sShadowRenderingDisabled;
    static bool sSTSLighting;
    static cCharacter* spRenderOnlyThisCharacter;
    static bool sbRenderOpposingGoalieToo;

}; // total size: 0x58

#endif // _DRAWABLECHARACTER_H_
