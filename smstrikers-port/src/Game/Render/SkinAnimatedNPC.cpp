#include "Game/Render/SkinAnimatedNPC.h"
#include "Game/GL/GLInventory.h"
#include "Game/GameObjectLighting.h"
#include "Game/WorldManager.h"
#include "Game/World.h"
#include "NL/gl/gl.h"
#include "NL/gl/glView.h"
#include "NL/gl/glUserData.h"
#include "NL/gl/glState.h"
#include "Game/SAnim/pnSAnimController.h"
#include "Game/SAnim/pnBlender.h"
#include "types.h"

extern GLInventory glInventory;
extern float GetPlanarShadowOpacity();
extern void DrawPlanarShadow(const glModel*, const nlMatrix4&, float, bool, bool, bool, uintptr_t);

const u32 GLTT_BumpLocal_bit = 1 << (int)GLTT_BumpLocal;

/**
 * Offset/Address/Size: 0x60C | 0x80164FB0 | size: 0xC0
 */
SkinAnimatedNPC::SkinAnimatedNPC(cSHierarchy& pHierarchy, int nModelID)
{
    mpAnimController = 0;
    mpPoseAccumulator = 0;
    mpSkinMesh = 0;
    mpPoseTree = 0;
    mbIsVisible = 0;

    if ((u32)nModelID == (u32)-1)
    {
        mpSkinMesh = 0;
    }
    else
    {
        // PORT: truncate to 32 bits before widening, an id with bit 31 set would otherwise sign-extend and miss its own entry.
        mpSkinMesh = glInventory.MakeSkinMesh((unsigned long)(u32)nModelID);
    }

    cPoseAccumulator* pAccum = new (nlMalloc(sizeof(cPoseAccumulator), 8, false)) cPoseAccumulator(&pHierarchy, true);
    mpPoseAccumulator = pAccum;

    mWorldMatrix.SetIdentity();
    mbIsVisible = true;
}

/**
 * Offset/Address/Size: 0x4D8 | 0x80164E7C | size: 0x134
 */
SkinAnimatedNPC::~SkinAnimatedNPC()
{
    delete mpPoseTree;
    delete mpPoseAccumulator;
    if (mpSkinMesh != NULL)
    {
        delete mpSkinMesh;
    }
}

/**
 * Offset/Address/Size: 0x474 | 0x80164E18 | size: 0x64
 */
void SkinAnimatedNPC::Update(float dt)
{
    mpPoseTree = mpPoseTree->Update(dt);
    mpPoseAccumulator->InitAccumulators();
    mpPoseTree->Evaluate(1.0f, mpPoseAccumulator);
}

/**
 * Offset/Address/Size: 0x424 | 0x80164DC8 | size: 0x50
 */
void SkinAnimatedNPC::Render()
{
    mpPoseAccumulator->BuildNodeMatrices(mWorldMatrix);
    RenderFromReplay(*mpPoseAccumulator, &mWorldMatrix);
}

/**
 * Offset/Address/Size: 0x398 | 0x80164D3C | size: 0x8C
 */
void SkinAnimatedNPC::RenderFromReplay(const cPoseAccumulator& poseAccumulator, const nlMatrix4* pWorldMatrix)
{
    if (!mbIsVisible)
        return;
    if (!mpSkinMesh)
        return;

    mpSkinMesh->Pose(const_cast<cPoseAccumulator*>(&poseAccumulator));
    SendToGL();
    DrawShadow(poseAccumulator, *pWorldMatrix);
}

/**
 * Offset/Address/Size: 0x240 | 0x80164BE4 | size: 0x158
 */
void SkinAnimatedNPC::SendToGL() const
{
    FORCE_DONT_INLINE;
    static int index = 0;
    static unsigned long prevFrame = 0;

    unsigned long frame = glGetCurrentFrame();
    if (prevFrame != frame)
    {
        prevFrame = frame;
        index = 0;
    }

    unsigned long prog;
    void* lightData = GetInGameLightData();
    prog = glGetProgram("3d pointlit");
    u32 lightRamp = GetGameObjectLightRamp();

    GLSkinMesh* skinMesh = mpSkinMesh;
    skinMesh->PrepareToRender(prog, 0);

    glModel* pModel = glModelDup(skinMesh->pModel, true);
    glModelPacket* pPacket = pModel->packets;
    while (pPacket < pModel->packets + pModel->numPackets)
    {
        if (lightData)
        {
            glUserAttach(lightData, pPacket, false);
        }

        if (pPacket->state.texconfig & 0x10)
        {
            void* specData = WorldManager::s_World->GetCustomSpecularData(pPacket, false);
            glUserAttach(specData, pPacket, false);
        }

        pPacket->state.texture[5] = lightRamp;
        pPacket->state.texconfig |= GLTT_BumpLocal_bit;

        pPacket++;
    }

    glViewAttachModel((eGLView)6, pModel);
    const_cast<SkinAnimatedNPC*>(this)->mpLastModel = pModel;
}

/**
 * Offset/Address/Size: 0x1A4 | 0x80164B48 | size: 0x9C
 */
void SkinAnimatedNPC::DrawShadow(const cPoseAccumulator& poseAccumulator, const nlMatrix4& worldMatrix)
{
    GLSkinMesh* skinMesh = mpSkinMesh;
    unsigned long prog = glGetProgram("3d unlit");
    skinMesh->PrepareToRender(prog, 0);
    glModel* pModel = glModelDup(skinMesh->pModel, true);
    float opacity = GetPlanarShadowOpacity();
    DrawPlanarShadow(pModel, worldMatrix, opacity, false, true, true, (uintptr_t)this);
}

/**
 * Offset/Address/Size: 0x74 | 0x80164A18 | size: 0x130
 */
void SkinAnimatedNPC::SetAnimState(cSAnim& pAnim, float fBlendTime, ePlayMode playMode)
{
    cPN_SAnimController* controller = ::new (AllocateSAnimController()) cPN_SAnimController(&pAnim, (const AnimRetarget*)0, playMode, (void (*)(uintptr_t, cPN_SAnimController*))0, (unsigned int)0, (bool)0);

    if (mpPoseTree != NULL && fBlendTime > 0.0f)
    {
        cPN_Blender* blender = ::new (AllocateBlender()) cPN_Blender(mpPoseTree, controller, fBlendTime);
        mpPoseTree = blender;
    }
    else
    {
        mpPoseTree = controller;
    }

    mpAnimController = controller;
}

/**
 * Offset/Address/Size: 0x0 | 0x801649A4 | size: 0x74
 */
void SkinAnimatedNPC::DrawShadow(const glModel* pModel, const nlMatrix4& matrix)
{
    glModel* dup = glModelDupNoStreams(pModel, true, false);
    float opacity = GetPlanarShadowOpacity();
    DrawPlanarShadow(dup, matrix, opacity, false, true, true, (uintptr_t)this);
}
