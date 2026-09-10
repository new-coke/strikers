#ifndef _SKINANIMATEDNPC_H_
#define _SKINANIMATEDNPC_H_

#include "Game/SHierarchy.h"
#include "Game/PoseAccumulator.h"
#include "Game/SHierarchy.h"
#include "Game/SAnim.h"
#include "NL/gl/glModel.h"
#include "NL/nlMath.h"
#include "Game/GL/ShaderSkinMesh.h"

class cPN_SAnimController;
class cPoseNode;

enum SkinAnimatedNPC_Type
{
    SkinAnimatedNPC_BASE = 0,
    SkinAnimatedNPC_MOVABLE = 1,
    SkinAnimatedNPC_CAMERA_GUY = 2,
    SkinAnimatedNPC_CHAIN_CHOMP = 3,
    SkinAnimatedNPC_BOWSER = 4,
    NUM_SkinAnimatedNPC_TYPES = 5,
};

class SkinAnimatedNPC
{
public:
    SkinAnimatedNPC(cSHierarchy& pHierarchy, int nModelID);
    /* 0x08 */ virtual ~SkinAnimatedNPC();
    /* 0x0C */ virtual SkinAnimatedNPC_Type GetSkinAnimatedNPC_Type() const;
    /* 0x10 */ virtual void Render();
    /* 0x14 */ virtual void RenderFromReplay(const cPoseAccumulator& poseAccumulator, const nlMatrix4* pWorldMatrix);
    /* 0x18 */ virtual void Update(float dt);
    /* 0x1C */ virtual void DrawShadow(const glModel* pModel, const nlMatrix4& matrix);
    /* 0x20 */ virtual void DrawShadow(const cPoseAccumulator& poseAccumulator, const nlMatrix4& worldMatrix);

    void SendToGL() const;
    void SetAnimState(cSAnim& pAnim, float fBlendTime, ePlayMode playMode);

    /* 0x04 */ nlMatrix4 mWorldMatrix;                // offset 0x4, size 0x40
    /* 0x44 */ cPN_SAnimController* mpAnimController; // offset 0x44, size 0x4
    /* 0x48 */ cPoseAccumulator* mpPoseAccumulator;   // offset 0x48, size 0x4
    /* 0x4C */ GLSkinMesh* mpSkinMesh;                // offset 0x4C, size 0x4
    /* 0x50 */ cPoseNode* mpPoseTree;                 // offset 0x50, size 0x4
    /* 0x54 */ bool mbIsVisible;                      // offset 0x54, size 0x1
    /* 0x58 */ glModel* mpLastModel;                  // offset 0x58, size 0x4
}; // total size: 0x5C

inline SkinAnimatedNPC_Type SkinAnimatedNPC::GetSkinAnimatedNPC_Type() const
{
    return SkinAnimatedNPC_BASE;
}

#endif // _SKINANIMATEDNPC_H_
