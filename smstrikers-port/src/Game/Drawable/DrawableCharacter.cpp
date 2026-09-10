#include <stdio.h>
#include <stdlib.h>
#include "port/input.h"
// Hoisted: Game/Replay.h's ReplayablePolymorphicPtr calls the qualified ::Replayable<N>, which ADL cannot rescue.
class LoadFrame;
class SaveFrame;
class cPoseNode;
template <int N>
void Replayable(LoadFrame& frame, char typeId, cPoseNode*& poseNode);
template <int N>
void Replayable(SaveFrame& frame, char typeId, cPoseNode*& poseNode);

#include "Game/SAnim/pnSingleAxisBlender.h"
#include "NL/vmath.h"
#include "Game/SAnim/pnSAnimController.h"
#include "Game/SAnim/pnFeather.h"
#include "Game/SAnim/pnBlender.h"
#include "Game/Drawable/DrawableCharacter.h"
#include "Game/Character.h"
#include "Game/Player.h"
#include "Game/Team.h"
#include "Game/AI/HeadTrack.h"
#include "Game/PoseAccumulator.h"

template <int N>
void Replayable(LoadFrame& frame, char typeId, cPoseNode*& poseNode)
{
    if (N == 0 || frame.mInterval == N)
    {
        if (typeId == 0)
        {
            cPN_Blender* blender = new cPN_Blender;
            blender->Replay(frame);
            poseNode = blender;
        }
        else if (typeId == 1)
        {
            cPN_Feather* feather = new cPN_Feather;
            feather->Replay(frame);
            poseNode = feather;
        }
        else if (typeId == 2)
        {
            cPN_SAnimController* controller = new cPN_SAnimController;
            controller->Replay(frame);
            poseNode = controller;
        }
        else if (typeId == 3)
        {
            cPN_SingleAxisBlender* singleAxis = new cPN_SingleAxisBlender;
            singleAxis->Replay(frame);
            poseNode = singleAxis;
        }
    }
}

template <int N>
void Replayable(SaveFrame& frame, char typeId, cPoseNode*& poseNode)
{
    if (N == 0 || frame.mInterval == N)
    {
        if (typeId < 0 || typeId > 3)
            nlBreak();

        if (typeId == 0)
        {
            cPN_Blender* pn = (cPN_Blender*)poseNode;
            pn->Replay(frame);
        }
        else if (typeId == 1)
        {
            cPN_Feather* pn = (cPN_Feather*)poseNode;
            pn->Replay(frame);
        }
        else if (typeId == 2)
        {
            cPN_SAnimController* pn = (cPN_SAnimController*)poseNode;
            pn->Replay(frame);
        }
        else if (typeId == 3)
        {
            cPN_SingleAxisBlender* pn = (cPN_SingleAxisBlender*)poseNode;
            pn->Replay(frame);
        }
    }
}

#include "Game/Render/RenderShadow.h"
#include "Game/Debug/ShapeRender.h"
#include "Game/GameObjectLighting.h"
#include "Game/WorldManager.h"
#include "Game/BasicStadium.h"
#include "Game/GL/GLInventory.h"
#include "NL/gl/glAppAttach.h"  // PORT: ResolvedWhiteTexture, at its real type
#include "NL/gl/glView.h"
#include "NL/gl/glModel.h"
#include "NL/gl/glUserData.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glModify.h"
#include "NL/nlTask.h"
#include "NL/nlTicker.h"
#include "NL/nlString.h"
#include "NL/nlDebug.h"
#include <dolphin/os.h>
const u32 GLTT_BumpLocal_bit = 1 << (int)GLTT_BumpLocal;
const u32 GLTT_Detail_bit = 1 << (int)GLTT_Detail;

static unsigned long UnlitProgram = glGetProgram("3d unlit");
static unsigned long LitProgram = glGetProgram("3d pointlit");
static unsigned long LightTexture = glGetTexture("global/lightramp");
static unsigned long BlackTexture = glGetTexture("global/black");
static unsigned long WhiteTexture = glGetTexture("global/white");
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
int g_nShowBones;

unsigned char DrawableCharacter::sShadowRenderingDisabled;
cCharacter* DrawableCharacter::spRenderOnlyThisCharacter = nullptr;
bool DrawableCharacter::sbRenderOpposingGoalieToo = false;
bool DrawableCharacter::sSTSLighting = false;
bool DrawableCharacter::sCameraRelativeLighting = false;

static unsigned long CharacterDirtProgram = glGetProgram("3d pointlit dirt");
static const unsigned long CharacterProgram = glGetProgram("3d pointlit");

static int g_nOnscreenUpdate[3] = { 4, 3, 2 };
static int g_nOffscreenUpdate[3] = { 8, 8, 6 };

struct ShadowScale
{
    float fRadius;
    float fHeight;
    float fScalar;
};

ShadowScale shadowScale[] = {
    { 1.5f, 1.0f, 1.0f },
    { 1.75f, 1.25f, 1.25f },
    { 2.8f, 1.25f, 2.0f },
    { 1.5f, 1.0f, 1.0f },
    { 1.5f, 1.0f, 1.0f },
    { 1.5f, 1.0f, 1.0f },
    { 1.5f, 1.0f, 1.0f },
    { 1.75f, 1.25f, 1.25f },
    { 1.5f, 1.0f, 1.0f },
    { 2.0f, 1.25f, 1.5f },
    { 1.5f, 1.0f, 1.0f },
    { 1.5f, 1.0f, 1.0f },
    { 1.75f, 1.25f, 1.25f },
    { 1.75f, 1.25f, 1.25f },
};

int charSizes[] = {
    0,
    1,
    2,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    0,
    1,
    0,
};

static float g_fRadiusScale = 1.175f;
static unsigned char g_bSloppyBounds = 1;

/**
 * Offset/Address/Size: 0x2D50 | 0x8011BC00 | size: 0x4C
 */
DrawableCharacter::DrawableCharacter()
{
    mVisible = true;

    mPoseTree = nullptr;
    mPoseAccumulator = nullptr;
    mEffectsTexturing = nullptr;
    mCharacter = nullptr;
    mBowser = nullptr;

    mPosition.x = 0.0f;
    mPosition.y = 0.0f;
    mPosition.z = 0.0f;
    mBip01Position.x = 0.0f;
    mBip01Position.y = 0.0f;
    mBip01Position.z = 0.0f;
    mHeadPosition.x = 0.0f;
    mHeadPosition.y = 0.0f;
    mHeadPosition.z = 0.0f;
}

/**
 * Offset/Address/Size: 0x2C6C | 0x8011BB1C | size: 0xE4
 */
DrawableCharacter::~DrawableCharacter()
{
    delete mPoseAccumulator;
}

DrawableCharacter& DrawableCharacter::operator=(const DrawableCharacter& other)
{
    if (this != &other)
    {
        mVisible = other.mVisible;
        mPosition = other.mPosition;
        mHeight = other.mHeight;
        mVelocity = other.mVelocity;
        mFacingDirection = other.mFacingDirection;
        mHeadSpin = other.mHeadSpin;
        mHeadTilt = other.mHeadTilt;
        mPoseTree = NULL;
        *mPoseAccumulator = *other.mPoseAccumulator;
        mEffectsTexturing = other.mEffectsTexturing;
        mCharacter = other.mCharacter;
        mBowser = other.mBowser;
        mDirt = other.mDirt;
    }
    return *this;
}

/**
 * Offset/Address/Size: 0x2BA4 | 0x8011BA54 | size: 0xC8
 */
void DrawableCharacter::Free()
{
    delete mPoseAccumulator;
    mPoseAccumulator = NULL;
}

/**
 * Offset/Address/Size: 0x2B98 | 0x8011BA48 | size: 0xC
 */
cPN_SAnimController& DrawableCharacter::GetAnimController() const
{
    return *mCharacter->m_pCurrentAnimController;
}

/**
 * Offset/Address/Size: 0x2A1C | 0x8011B8CC | size: 0x17C
 */
void DrawableCharacter::Grab(cCharacter& character)
{
    mCharacter = &character;
    mBowser = NULL;
    mPosition = character.m_v3Position;
    mBip01Position = character.GetJointPosition(character.m_nBip01JointIndex_0xA4);
    mHeadPosition = character.GetJointPosition(character.m_nHeadJointIndex);
    mHeight = mBip01Position.z;
    mVelocity = character.m_v3Velocity;
    mFacingDirection = character.m_aActualFacingDirection;
    mHeadSpin = (unsigned short)character.m_pHeadTrack->m_fHeadSpin;
    mHeadTilt = (unsigned short)character.m_pHeadTrack->m_fHeadTilt;
    mPoseTree = character.m_pPoseTree;
    mVisible = true;
    mDirt = (unsigned char)(255.0f * character.m_Dirt);

    if (mPoseAccumulator == NULL)
    {
        cPoseAccumulator* p = (cPoseAccumulator*)nlMalloc(sizeof(cPoseAccumulator), 8, false);
        p = new (p) cPoseAccumulator(*character.m_pPoseAccumulator);
        mPoseAccumulator = p;
    }
    else
    {
        *mPoseAccumulator = *character.m_pPoseAccumulator;
    }

    EffectsTexturing* tex = character.m_pEffectsTexturing;
    if (tex == NULL)
    {
        tex = fxGetTexturing(eFXTex_Nothing);
    }
    mEffectsTexturing = tex;
}

/**
 * Offset/Address/Size: 0x29F0 | 0x8011B8A0 | size: 0x2C
 */
static void DrawableCharacterHeadTrackCallback(uintptr_t ctx, unsigned int, cPoseAccumulator* poseAccumulator, unsigned int currentNodeIndex, int)
{
    DrawableCharacter* drawableChar = (DrawableCharacter*)ctx;
    CalcHeadTrackMatrix(drawableChar->mHeadSpin, drawableChar->mHeadTilt, poseAccumulator, currentNodeIndex);
}

/**
 * Offset/Address/Size: 0x290C | 0x8011B7BC | size: 0xE4
 */
void DrawableCharacter::DrawableBowserHeadTrackCallback(uintptr_t ctx, unsigned int nParam2, cPoseAccumulator* poseAccumulator, unsigned int currentNodeIndex, int nParentIndex)
{
    DrawableCharacter* drawableChar = (DrawableCharacter*)ctx;
    nlMatrix4& nodeMatrix = poseAccumulator->GetNodeMatrix(currentNodeIndex);
    drawableChar->mBowser->mLastHeadMatrix = nodeMatrix;
    CalcHeadTrackMatrix(drawableChar->mHeadSpin, drawableChar->mHeadTilt, poseAccumulator, currentNodeIndex);
}

/**
 * Offset/Address/Size: 0x27F4 | 0x8011B6A4 | size: 0x118
 */
void DrawableCharacter::BuildNodeMatrices()
{
    nlMatrix4 worldMatrix;
    float angle = 0.0000958738f * (float)mFacingDirection;
    nlMakeRotationMatrixZ(worldMatrix, angle);

    worldMatrix.SetRow_(3, mPosition);

    if (mCharacter != nullptr)
    {
        mPoseAccumulator->SetBuildNodeMatrixCallback(mCharacter->m_nHeadJointIndex, DrawableCharacterHeadTrackCallback, (uintptr_t)this, 0);
    }
    else
    {
        if (mBowser != nullptr)
        {
            mPoseAccumulator->SetBuildNodeMatrixCallback(mBowser->mnHeadJointIndex, DrawableBowserHeadTrackCallback, (uintptr_t)this, 0);
        }
    }

    mPoseAccumulator->BuildNodeMatrices(worldMatrix);

    if (mCharacter != nullptr)
    {
        mPoseAccumulator->SetBuildNodeMatrixCallback(
            mCharacter->m_nHeadJointIndex,
            nullptr,
            0,
            0);
    }
    else if (mBowser != nullptr)
    {
        mPoseAccumulator->SetBuildNodeMatrixCallback(
            mBowser->mnHeadJointIndex,
            nullptr,
            0,
            0);
    }
}

/**
 * Offset/Address/Size: 0x26C4 | 0x8011B574 | size: 0x130
 */
void DrawableCharacter::Render(cCharacter& character) const
{
    static const bool bDrawProbe = getenv("STRIKERS_PROBE_CHARDRAW") != NULL;
    static int nCalls, nInvisible, nRenderOnly, nDrawn, nLastReport;
    if (bDrawProbe)
    {
        unsigned long f = PortInputFrame();
        ++nCalls;
        if ((int)f - nLastReport >= 60)
        {
            nLastReport = (int)f;
            nlMatrix4& m0 = mPoseAccumulator->GetNodeMatrix(0);
            fprintf(stderr, "[chardraw] frame %lu: %d call(s), %d invisible, "
                    "%d blocked by render-only(%p), %d drawn; "
                    "char pos %.1f,%.1f,%.1f node0 xlat %.1f,%.1f,%.1f "
                    "scale %.3f,%.3f,%.3f vel %.1f,%.1f,%.1f act %d\n",
                    f, nCalls, nInvisible, nRenderOnly,
                    (void*)spRenderOnlyThisCharacter, nDrawn,
                    character.m_v3Position.x, character.m_v3Position.y,
                    character.m_v3Position.z,
                    m0.m41, m0.m42, m0.m43,
                    m0.m11, m0.m22, m0.m33,
                    character.m_v3Velocity.x, character.m_v3Velocity.y,
                    character.m_v3Velocity.z,
                    (int)((cFielder*)&character)->m_eActionState);
            nCalls = nInvisible = nRenderOnly = nDrawn = 0;
        }
    }

    if (!mVisible)
    {
        if (bDrawProbe) ++nInvisible;
        return;
    }

    character.PoseSkinMesh(mPoseAccumulator);

    if (mCharacter->m_pPropModel != NULL)
    {
        nlMatrix4& propWorldMatrix = mPoseAccumulator->GetNodeMatrix(character.m_nPropJointIndex);
        mCharacter->m_pPropModel->m_worldMatrix = propWorldMatrix;
    }

    cCharacter* renderOnly = spRenderOnlyThisCharacter;
    if (renderOnly != NULL && renderOnly != &character)
    {
        if (!sbRenderOpposingGoalieToo)
        {
            if (bDrawProbe) ++nRenderOnly;
            return;
        }
        cTeam* otherTeam = ((cPlayer*)renderOnly)->m_pTeam->GetOtherTeam();
        if (&character != (cCharacter*)otherTeam->GetGoalie())
        {
            if (bDrawProbe) ++nRenderOnly;
            return;
        }
    }

    if (bDrawProbe) ++nDrawn;
    SendToGl(character);
}

static bool IsVisible(const nlVector3& worldPosition, eCharacterClass cc)
{
    if (nlTaskManager::m_pInstance->m_CurrState == 0x100)
        return 1;
    if (WorldManager::s_World != nullptr)
    {
        float fRadius;
        if (cc == DONKEYKONG)
            fRadius = 3.5f;
        else
            fRadius = 2.5f;
        nlMatrix4 mWorld;
        mWorld.SetIdentity();
        mWorld.m41 = worldPosition.x;
        mWorld.m42 = worldPosition.y;
        mWorld.m43 = worldPosition.z;
        mWorld.m44 = 1.0f;
        return WorldManager::s_World->IsSphereInFrustum(mWorld, fRadius);
    }
    return 1;
}

static void DrawSphere(const nlVector3& vCentre, float fRadius, const nlColour& colour)
{
    extern GLInventory glInventory;

    glModel* pSphereModel = glModelDup(glInventory.GetModel(nlStringHash("debug/sphere")), true);

    nlMatrix4 sphereWorldMatrix;
    sphereWorldMatrix.SetIdentity();
    sphereWorldMatrix.m41 = vCentre.x;
    sphereWorldMatrix.m42 = vCentre.y;
    sphereWorldMatrix.m43 = vCentre.z;
    sphereWorldMatrix.m44 = 1.0f;
    sphereWorldMatrix.m11 = fRadius;
    sphereWorldMatrix.m22 = fRadius;
    sphereWorldMatrix.m33 = fRadius;

    uintptr_t matrix = glAllocMatrix();
    if (matrix != 0xFFFFFFFF)
    {
        glSetMatrix(matrix, sphereWorldMatrix);
    }

    void* pConstantColour = glUserAlloc(GLUD_ConstantColour, 4, false);
    nlColour* pDebugColour = (nlColour*)glUserGetData(pConstantColour);
    u8 alpha = colour.c[3];
    *pDebugColour = colour;
    for (glModelPacket* pSpherePacket = pSphereModel->packets; pSpherePacket < pSphereModel->packets + pSphereModel->numPackets; pSpherePacket++)
    {
        pSpherePacket->state.matrix = matrix;
        pSpherePacket->state.texture[GLTT_Diffuse] = (uintptr_t)ResolvedWhiteTexture;
        if (alpha != 0xFF)
        {
            glSetRasterState(pSpherePacket->state.raster, GLS_AlphaBlend, GLB_Standard);
        }
        glUserAttach(pConstantColour, pSpherePacket, false);
    }

    glViewAttachModel(GLV_Characters, 6, pSphereModel);
}

static const float kBigFloat = 1e30f;

/**
 * Offset/Address/Size: 0x22EC | 0x8011B19C | size: 0x3D8
 */
static void FindBoundingSphereAccurate(nlVector3* pOutSphere, float* pOutRadius, int numVertices, const nlVector3* pVertices)
{
    nlVector3 minXPt, maxXPt, minYPt, maxYPt, minZPt, maxZPt;
    nlVector3 span1, span2;
    const nlVector3* p;
    int i;
    float radiusSq;

    minZPt.z = kBigFloat;
    minYPt.y = kBigFloat;
    minXPt.x = kBigFloat;
    maxZPt.z = -kBigFloat;
    maxYPt.y = -kBigFloat;
    maxXPt.x = -kBigFloat;

    p = pVertices;
    for (i = 0; i < numVertices; i++, p++)
    {
        if (p->x < minXPt.x)
        {
            minXPt = *p;
        }
        if (p->x > maxXPt.x)
        {
            maxXPt = *p;
        }
        if (p->y < minYPt.y)
        {
            minYPt = *p;
        }
        if (p->y > maxYPt.y)
        {
            maxYPt = *p;
        }
        if (p->z < minZPt.z)
        {
            minZPt = *p;
        }
        if (p->z > maxZPt.z)
        {
            maxZPt = *p;
        }
    }

    float xSpanDistSq = nlGetLengthSquared3D(maxXPt.x - minXPt.x, maxXPt.y - minXPt.y, maxXPt.z - minXPt.z);
    float ySpanDistSq = nlGetLengthSquared3D(maxYPt.x - minYPt.x, maxYPt.y - minYPt.y, maxYPt.z - minYPt.z);
    float zSpanDistSq = nlGetLengthSquared3D(maxZPt.x - minZPt.x, maxZPt.y - minZPt.y, maxZPt.z - minZPt.z);

    span1 = minXPt;
    span2 = maxXPt;
    if (ySpanDistSq > xSpanDistSq)
    {
        xSpanDistSq = ySpanDistSq;
        span1 = minYPt;
        span2 = maxYPt;
    }
    if (zSpanDistSq > xSpanDistSq)
    {
        span1 = minZPt;
        span2 = maxZPt;
    }

    nlVec3Set(*pOutSphere, 0.5f * (span1.x + span2.x), 0.5f * (span1.y + span2.y), 0.5f * (span1.z + span2.z));

    nlVector3 result;
    nlVec3Sub(result, span2, *pOutSphere);
    radiusSq = nlGetLengthSquared3D(result.x, result.y, result.z);

    *pOutRadius = nlSqrt(radiusSq, false);

    for (i = 0; i < numVertices; pVertices++, i++)
    {
        float distSq = nlGetLengthSquared3D(pVertices->x - pOutSphere->x, pVertices->y - pOutSphere->y, pVertices->z - pOutSphere->z);
        if (distSq > radiusSq)
        {
            float dist = nlSqrt(distSq, false);
            *pOutRadius = 0.5f * (*pOutRadius + dist);
            float d = dist - *pOutRadius;
            radiusSq = *pOutRadius * *pOutRadius;
            pOutSphere->x = (*pOutRadius * pOutSphere->x + d * pVertices->x) / dist;
            pOutSphere->y = (*pOutRadius * pOutSphere->y + d * pVertices->y) / dist;
            pOutSphere->z = (*pOutRadius * pOutSphere->z + d * pVertices->z) / dist;
        }
    }
}

static inline void RenderCharacterBoundingSphere(nlMatrix4& sphereWorldMatrix, const nlVector3& vCenter, float fRadius)
{
    extern GLInventory glInventory;

    float sphereRadius = fRadius;
    nlColour debugColour = { 0xFF, 0xFF, 0x40, 0x50 };
    u8 alpha;
    glModel* pSphereModel;
    glModelPacket* pSpherePacket;
    void* pConstantColour;
    pSphereModel = glModelDup(glInventory.GetModel(nlStringHash("debug/sphere")), true);

    sphereWorldMatrix.SetIdentity();
    sphereWorldMatrix.m41 = vCenter.x;
    sphereWorldMatrix.m42 = vCenter.y;
    sphereWorldMatrix.m43 = vCenter.z;
    sphereWorldMatrix.m44 = 1.0f;
    sphereWorldMatrix.m11 = sphereRadius;
    sphereWorldMatrix.m22 = sphereRadius;
    sphereWorldMatrix.m33 = sphereRadius;

    uintptr_t matrix = glAllocMatrix();
    if (matrix != 0xFFFFFFFF)
    {
        glSetMatrix(matrix, sphereWorldMatrix);
    }

    pConstantColour = glUserAlloc(GLUD_ConstantColour, 4, false);
    nlColour* pDebugColour = (nlColour*)glUserGetData(pConstantColour);
    alpha = debugColour.c[3];
    *pDebugColour = debugColour;
    for (pSpherePacket = pSphereModel->packets; pSpherePacket < pSphereModel->packets + pSphereModel->numPackets; pSpherePacket++)
    {
        pSpherePacket->state.matrix = matrix;
        pSpherePacket->state.texture[GLTT_Diffuse] = (uintptr_t)ResolvedWhiteTexture;
        if (alpha != 0xFF)
        {
            glSetRasterState(pSpherePacket->state.raster, GLS_AlphaBlend, GLB_Standard);
        }
        glUserAttach(pConstantColour, pSpherePacket, false);
    }

    glViewAttachModel(GLV_Characters, 6, pSphereModel);
}

static void FindBoundingSphereSloppy(const nlVector3& vCenter, float* pOutRadius, int numVertices, const nlVector3* pVertices)
{
    float maxDistSq = 0.0f;

    for (int i = 0; i < numVertices; i++)
    {
        float distSq = nlGetLengthSquared3D(
            pVertices[i].x - vCenter.x,
            pVertices[i].y - vCenter.y,
            pVertices[i].z - vCenter.z);
        maxDistSq = (maxDistSq >= distSq) ? maxDistSq : distSq;
    }

    *pOutRadius = nlSqrt(maxDistSq, false);
}

/**
 * Offset/Address/Size: 0x1AE4 | 0x8011A994 | size: 0x808
 */
void DrawableCharacter::SendToGl(const cCharacter& character) const
{
    extern GLInventory glInventory;

    ProjectedShadowParams params;
    nlMatrix4 mWorld;
    nlMatrix4 sphereWorldMatrix;
    EffectsTexturing* fxtex = mEffectsTexturing;
    eCharacterClass ec = character.m_eCharacterClass;

    if (fxtex != nullptr && fxtex->m_uTexture == 0xFFFFFFFF)
    {
        fxtex = nullptr;
    }

    u8 hasDetail = 0;
    if (fxtex != nullptr && fxtex->m_bDetail)
    {
        hasDetail = 1;
    }

    unsigned long program = CharacterDirtProgram;
    if (hasDetail != 0)
    {
        program = CharacterProgram;
    }

    GLSkinMesh* skinMesh = character.GetSkinMesh();
    skinMesh->PrepareToRender(program, nullptr);

    if (ec == DONKEYKONG || ec == MYSTERY)
    {
        for (glModelPacket* pPacket = skinMesh->pModel->packets; pPacket < skinMesh->pModel->packets + skinMesh->pModel->numPackets; pPacket++)
        {
            glSetRasterState(pPacket->state.raster, GLS_Culling, 0);
        }
    }

    u32 lightTexture;
    World* world = WorldManager::s_World;
    if (DrawableCharacter::sSTSLighting != 0)
    {
        lightTexture = world->m_GlobalLightRampSTSTex;
    }
    else
    {
        lightTexture = GetGameObjectLightRamp();
    }

    void* pSpecularData;
    void* pLightData;
    if (DrawableCharacter::sSTSLighting != 0)
    {
        pLightData = world->m_pIntensityData;
    }
    else if (DrawableCharacter::sCameraRelativeLighting || AlwaysUseCameraRelativeCharacterLighting())
    {
        pLightData = GetCameraRelativeLightData();
    }
    else
    {
        pLightData = GetInGameLightData();
    }

    void* pEnviroData = nullptr;
    if (fxtex != nullptr && fxtex->m_bEnviro)
    {
        pEnviroData = glUserAlloc((eGLUserData)0xE, 0, false);
    }

    pSpecularData = WorldManager::s_World->m_pSTSIntensity;
    glModel* pModel = glModelDup(skinMesh->pModel, true);

    bool isVisible;
    if (nlTaskManager::m_pInstance->m_CurrState == 0x100)
    {
        isVisible = 1;
    }
    else if (WorldManager::s_World != nullptr)
    {
        float fRadius;
        if (ec == DONKEYKONG)
        {
            fRadius = 3.5f;
        }
        else
        {
            fRadius = 2.5f;
        }

        mWorld.SetIdentity();
        mWorld.m41 = mBip01Position.x;
        mWorld.m42 = mBip01Position.y;
        mWorld.m43 = mBip01Position.z;
        mWorld.m44 = 1.0f;

        isVisible = WorldManager::s_World->IsSphereInFrustum(mWorld, fRadius);
    }
    else
    {
        isVisible = 1;
    }

    if (isVisible)
    {
        glModelPacket* pPacket = pModel->packets;
        u32 dirtValue = (u32)(u8)(int)(63.0f * (1.0f - ((float)mDirt / 255.0f)));

        for (; pPacket < pModel->packets + pModel->numPackets; pPacket++)
        {
            if (pLightData != nullptr)
            {
                glUserAttach(pLightData, pPacket, false);
            }

            if (pPacket->state.texconfig & 0x10)
            {
                nlBreak();
                glUserAttach(pSpecularData, pPacket, false);
            }

            pPacket->state.texture[GLTT_BumpLocal] = lightTexture;
            pPacket->state.texconfig |= GLTT_BumpLocal_bit;

            if (fxtex != nullptr)
            {
                if (fxtex->m_eBlendMode != GLB_None)
                {
                    glSetRasterState(pPacket->state.raster, GLS_AlphaBlend, fxtex->m_eBlendMode);
                }

                if (fxtex->m_bDetail)
                {
                    if (ec == MYSTERY)
                    {
                        pPacket->state.texture[GLTT_Diffuse] = fxtex->m_uTexture;
                    }
                    else
                    {
                        pPacket->state.texture[GLTT_Detail] = fxtex->m_uTexture;
                        pPacket->state.texconfig |= GLTT_Detail_bit;
                        glSetTextureState(pPacket->state.texturestate, (eGLTextureState)0xC, 0xF);
                    }
                }
                else
                {
                    pPacket->state.texture[GLTT_Diffuse] = fxtex->m_uTexture;
                }

                if (pEnviroData != nullptr)
                {
                    glUserAttach(pEnviroData, pPacket, false);
                }
            }

            if ((pPacket->state.texconfig & 0x2) && (fxtex == nullptr || !fxtex->m_bDetail))
            {
                glSetTextureState(pPacket->state.texturestate, (eGLTextureState)0xC, dirtValue);
            }
        }

        u8 isMapped = 0;
        if (character.m_uNormalTextureID != character.m_uSwapTextureID)
        {
            gl_ModifyAddMapping(GLMod_DiffuseTex, character.m_uNormalTextureID, character.m_uSwapTextureID);
            isMapped = 1;
        }

        if (fxtex == nullptr)
        {
            character.PerformBlinking(skinMesh, pModel);
        }

        glViewAttachModel(GLV_Characters, pModel);

        if (isMapped != 0)
        {
            gl_ModifyClearLastMapping();
        }
    }

    if (g_nShowBones > 0)
    {
        static u32 tDiff = 0;
        static u32 counter = 0;

        bool endpointBounds = (g_nShowBones == 1);
        PhysicsCharacterBase* pPhysicsCharacter = character.m_pPhysicsCharacter;
        int numBoneVolumePoints = pPhysicsCharacter->GetNumBoneVolumePoints(endpointBounds);

        if (numBoneVolumePoints <= 0xC0)
        {
            u32 startTick = nlGetTicker();

            nlVector3 points[0xC0];
            pPhysicsCharacter->GetBoneVolumePoints(points, endpointBounds);

            nlVector3 vCenter;
            float fRadius;
            if (g_bSloppyBounds != 0)
            {
                vCenter = mBip01Position;
                float maxDistSq = 0.0f;

                for (int i = 0; i < numBoneVolumePoints; i++)
                {
                    float distSq = nlGetLengthSquared3D(
                        points[i].x - vCenter.x,
                        points[i].y - vCenter.y,
                        points[i].z - vCenter.z);
                    maxDistSq = (maxDistSq >= distSq) ? maxDistSq : distSq;
                }

                fRadius = nlSqrt(maxDistSq, false);
            }
            else
            {
                FindBoundingSphereAccurate(&vCenter, &fRadius, numBoneVolumePoints, points);
            }

            u32 endTick = nlGetTicker();
            u32 tickDiff = nlSubtractTicks(startTick, endTick);

            tDiff += tickDiff;
            if (++counter >= 0x1E0)
            {
                float ms = nlGetTickerDifference(0, tDiff);
                ms = ms / (float)counter;
                ms = 8.0f * ms;
                u32 avgTicks = tDiff / counter;
                tDiff = avgTicks;
                OSReport("%u avg ticks (%0.3fms for 8 chars) to find bounding sphere\n", avgTicks, ms);
                tDiff = 0;
                counter = 0;
            }

            RenderCharacterBoundingSphere(sphereWorldMatrix, vCenter, fRadius);
        }
    }

    if (sShadowRenderingDisabled == 0)
    {
        const LightObject* pLight = ((BasicStadium*)WorldManager::s_World)->m_pShadowLight;
        if (pLight != nullptr)
        {
            static float s_fHeightFudge = 1.125f;

            params.fScalar = 1.0f;
            float fRadius;
            float fHeight;
            int characterSizeIndex;

            if (ec < NUM_FIELDER_CLASSES)
            {
                const ShadowScale& ss = shadowScale[ec];
                fRadius = ss.fRadius;
                fHeight = ss.fHeight;
                params.fScalar = ss.fScalar;
                characterSizeIndex = charSizes[ec];
            }
            else
            {
                const ShadowScale& ss = shadowScale[NUM_FIELDER_CLASSES];
                fRadius = ss.fRadius;
                fHeight = ss.fHeight;
                params.fScalar = ss.fScalar;
                characterSizeIndex = charSizes[NUM_FIELDER_CLASSES];
            }

            nlVec4Set(params.vLight, pLight->m_worldPosition.x, pLight->m_worldPosition.y, pLight->m_worldPosition.z, 1.0f);
            params.vPosition = mBip01Position;
            params.fRadius = g_fRadiusScale * fRadius;
            params.fHeight = s_fHeightFudge * fHeight;
            params.fWidth = params.fHeight;
            params.pModel = pModel;
            params.nPartitionIndex = GetShadowPartitionIndex();
            params.nVisibleInterval = g_nOnscreenUpdate[characterSizeIndex];
            params.nInvisibleInterval = g_nOffscreenUpdate[characterSizeIndex];

            if (ShouldShadowBeUpdated(params))
            {
                params.pModel = glModelDupNoStreams(pModel, true, false);
                RenderCharacterIntoTexture(params);
            }

            RenderProjectedShadow(params);
        }
    }
}

/**
 * Offset/Address/Size: 0x924 | 0x801197D4 | size: 0x148
 */
void DrawableCharacter::Grab(SkinAnimatedMovableNPC& npc)
{
    mCharacter = nullptr;

    if (static_cast<SkinAnimatedNPC&>(npc).GetSkinAnimatedNPC_Type() == SkinAnimatedNPC_BOWSER)
    {
        mBowser = (Bowser*)&npc;
    }

    mPosition = npc.mv3Position;

    nlMatrix4& nodeMatrix = npc.mpPoseAccumulator->GetNodeMatrix(0);
    mHeight = nodeMatrix.m43;

    mVelocity.x = 0.0f;
    mVelocity.y = 0.0f;
    mVelocity.z = 0.0f;

    mFacingDirection = npc.maFacingDirection;

    float headSpin = npc.GetHeadSpin();
    mHeadSpin = (unsigned short)(int)headSpin;

    float headTilt = npc.GetHeadTilt();
    mHeadTilt = (unsigned short)(int)headTilt;

    mPoseTree = npc.mpPoseTree;
    mVisible = npc.mbIsVisible;
    mDirt = false;

    if (mPoseAccumulator == nullptr)
    {
        mPoseAccumulator = new (nlMalloc(sizeof(cPoseAccumulator), 8, false)) cPoseAccumulator(*npc.mpPoseAccumulator);
    }
    else
    {
        *mPoseAccumulator = *npc.mpPoseAccumulator;
    }

    mEffectsTexturing = nullptr;
}

/**
 * Offset/Address/Size: 0x87C | 0x8011972C | size: 0xA8
 */
void DrawableCharacter::Render(SkinAnimatedMovableNPC& npc) const
{
    if (!mVisible)
    {
        return;
    }

    nlMatrix4 worldMatrix;
    float angle = 0.0000958738f * (float)mFacingDirection;
    nlMakeRotationMatrixZ(worldMatrix, angle);

    worldMatrix.SetRow_(3, mPosition);

    npc.mbIsVisible = mVisible;
    npc.RenderFromReplay(*mPoseAccumulator, &worldMatrix);
}

/**
 * Offset/Address/Size: 0x2B8 | 0x80119168 | size: 0x5C4
 */
void DrawableCharacter::Blend(const float* blendFactors, const DrawableCharacter& lhs, const DrawableCharacter& rhs)
{
    mVisible = lhs.mVisible && rhs.mVisible;
    mCharacter = lhs.mCharacter;
    mBowser = lhs.mBowser;
    mDirt = lhs.mDirt;
    mPosition.x = (1.0f - *blendFactors) * lhs.mPosition.x + *blendFactors * rhs.mPosition.x;
    mPosition.y = (1.0f - *blendFactors) * lhs.mPosition.y + *blendFactors * rhs.mPosition.y;
    mPosition.z = (1.0f - *blendFactors) * lhs.mPosition.z + *blendFactors * rhs.mPosition.z;
    mBip01Position.x = (1.0f - *blendFactors) * lhs.mBip01Position.x + *blendFactors * rhs.mBip01Position.x;
    mBip01Position.y = (1.0f - *blendFactors) * lhs.mBip01Position.y + *blendFactors * rhs.mBip01Position.y;
    mBip01Position.z = (1.0f - *blendFactors) * lhs.mBip01Position.z + *blendFactors * rhs.mBip01Position.z;
    mHeadPosition.x = (1.0f - *blendFactors) * lhs.mHeadPosition.x + *blendFactors * rhs.mHeadPosition.x;
    mHeadPosition.y = (1.0f - *blendFactors) * lhs.mHeadPosition.y + *blendFactors * rhs.mHeadPosition.y;
    mHeadPosition.z = (1.0f - *blendFactors) * lhs.mHeadPosition.z + *blendFactors * rhs.mHeadPosition.z;
    float t = *blendFactors;
    mFacingDirection = lhs.mFacingDirection + (short)(t * (float)(short)(rhs.mFacingDirection - lhs.mFacingDirection));
    mHeadSpin = lhs.mHeadSpin + (short)(t * (float)(short)(rhs.mHeadSpin - lhs.mHeadSpin));
    mHeadTilt = lhs.mHeadTilt + (short)(t * (float)(short)(rhs.mHeadTilt - lhs.mHeadTilt));
    mPoseTree = nullptr;
    if (mPoseAccumulator == nullptr)
    {
        mPoseAccumulator = new (nlMalloc(sizeof(cPoseAccumulator), 8, false)) cPoseAccumulator(lhs.mPoseAccumulator->m_BaseSHierarchy, false);
    }
    mPoseAccumulator->InitAccumulators();
    const float rhsWeight = *blendFactors;
    RotAccum* lhsRot;
    RotAccum* rhsRot;
    for (int i = 0; i < mPoseAccumulator->GetNumNodes(); i++)
    {
        lhsRot = &lhs.mPoseAccumulator->m_rot[i];
        rhsRot = &rhs.mPoseAccumulator->m_rot[i];
        float rhsRotAroundZWeight = rhsRot->rotAroundZAccumulatedWeight * rhsWeight;
        mPoseAccumulator->BlendRotAroundZ(i, lhsRot->rotAroundZ, lhsRot->rotAroundZAccumulatedWeight * (1.0f - *blendFactors));
        mPoseAccumulator->BlendRotAroundZ(i, rhsRot->rotAroundZ, rhsRotAroundZWeight);
        float rhsQuatWeight = rhsRot->quatAccumulatedWeight * rhsWeight;
        mPoseAccumulator->BlendRot(i, &lhsRot->q, lhsRot->quatAccumulatedWeight * (1.0f - *blendFactors), false);
        mPoseAccumulator->BlendRot(i, &rhsRot->q, rhsQuatWeight, false);
        mPoseAccumulator->BlendTrans(i, &lhs.mPoseAccumulator->m_trans[i].t, 1.0f - *blendFactors, false);
        mPoseAccumulator->BlendTrans(i, &rhs.mPoseAccumulator->m_trans[i].t, *blendFactors, false);
        mPoseAccumulator->BlendScale(i, &lhs.mPoseAccumulator->m_scale[i].s, 1.0f - *blendFactors, false);
        mPoseAccumulator->BlendScale(i, &rhs.mPoseAccumulator->m_scale[i].s, *blendFactors, false);
    }

    Vector<float, DefaultAllocator>& lhsMorphWeights = lhs.mPoseAccumulator->m_MorphWeights;
    Vector<float, DefaultAllocator>& rhsMorphWeights = rhs.mPoseAccumulator->m_MorphWeights;
    float* factors = (float*)blendFactors;
    for (int i = 0; i < 8; i++)
    {
        float& lhsMorphWeight = lhsMorphWeights[i];
        mPoseAccumulator->m_MorphWeights[i] += lhsMorphWeight * (1.0f - *blendFactors);
        float& rhsMorphWeight = rhsMorphWeights[i];
        mPoseAccumulator->m_MorphWeights[i] += rhsMorphWeight * factors[0];
    }
    nlMatrix4 rotMatrix;
    nlMakeRotationMatrixZ(rotMatrix, 0.0000958738f * (float)mFacingDirection);
    rotMatrix.e2[3][0] = mPosition.x;
    rotMatrix.e2[3][1] = mPosition.y;
    rotMatrix.e2[3][2] = mPosition.z;
    if (mCharacter != nullptr)
    {
        mPoseAccumulator->SetBuildNodeMatrixCallback(mCharacter->m_nHeadJointIndex, DrawableCharacterHeadTrackCallback, (uintptr_t)this, 0);
    }
    else if (mBowser != nullptr)
    {
        mPoseAccumulator->SetBuildNodeMatrixCallback(mBowser->mnHeadJointIndex, DrawableBowserHeadTrackCallback, (uintptr_t)this, 0);
    }
    mPoseAccumulator->BuildNodeMatrices(rotMatrix);
    if (mCharacter != nullptr)
    {
        mPoseAccumulator->SetBuildNodeMatrixCallback(mCharacter->m_nHeadJointIndex, nullptr, 0, 0);
    }
    else if (mBowser != nullptr)
    {
        mPoseAccumulator->SetBuildNodeMatrixCallback(mBowser->mnHeadJointIndex, nullptr, 0, 0);
    }
    mEffectsTexturing = lhs.mEffectsTexturing;
}

/**
 * Offset/Address/Size: 0xD8 | 0x80118F88 | size: 0x1E0
 */
void DrawableCharacter::EvaluateFrom(const cPoseNode& poseNode, const nlVector3& offset, unsigned short facingAngle)
{
    mPosition = offset;

    mVelocity.x = 0.0f;
    mVelocity.y = 0.0f;
    mVelocity.z = 0.0f;

    mFacingDirection = facingAngle;
    mHeadSpin = 0;
    mHeadTilt = 0;
    mHeight = 0.0f;

    mPoseAccumulator->InitAccumulators();

    poseNode.Evaluate(1.0f, mPoseAccumulator);

    mEffectsTexturing = fxGetTexturing(eFXTex_Nothing);

    nlMatrix4 rotMatrix;
    float angle = 0.0000958738f * (float)mFacingDirection;
    nlMakeRotationMatrixZ(rotMatrix, angle);
    rotMatrix.SetRow_(3, mPosition);

    if (mCharacter != nullptr)
    {
        mPoseAccumulator->SetBuildNodeMatrixCallback(mCharacter->m_nHeadJointIndex, DrawableCharacterHeadTrackCallback, (uintptr_t)this, 0);
    }
    else
    {
        if (mBowser != nullptr)
        {
            mPoseAccumulator->SetBuildNodeMatrixCallback(mBowser->mnHeadJointIndex, DrawableBowserHeadTrackCallback, (uintptr_t)this, 0);
        }
    }

    mPoseAccumulator->BuildNodeMatrices(rotMatrix);

    if (mCharacter != nullptr)
    {
        mPoseAccumulator->SetBuildNodeMatrixCallback(
            mCharacter->m_nHeadJointIndex,
            nullptr,
            0,
            0);
    }
    else if (mBowser != nullptr)
    {
        mPoseAccumulator->SetBuildNodeMatrixCallback(
            mBowser->mnHeadJointIndex,
            nullptr,
            0,
            0);
    }

    nlMatrix4& bip01Matrix = mPoseAccumulator->GetNodeMatrix(mCharacter->m_nBip01JointIndex_0xA4);
    mBip01Position = bip01Matrix.GetTranslation();

    nlMatrix4& headMatrix = mPoseAccumulator->GetNodeMatrix(mCharacter->m_nHeadJointIndex);
    mHeadPosition = headMatrix.GetTranslation();
}

/**
 * Offset/Address/Size: 0x88 | 0x80118F38 | size: 0x50
 */
nlVector3 DrawableCharacter::GetBallPosition() const
{
    cPlayer* pPlayer = (cPlayer*)mCharacter;
    nlMatrix4& matrix = mPoseAccumulator->GetNodeMatrix(pPlayer->m_nBallJointIndex);
    return matrix.GetTranslation();
}

/**
 * Offset/Address/Size: 0x24 | 0x80118ED4 | size: 0x64
 */
nlQuaternion DrawableCharacter::GetBallOrientation() const
{
    nlQuaternion ret;
    nlMatrixToQuat(ret, mPoseAccumulator->GetNodeMatrix(((cPlayer*)mCharacter)->m_nBallJointIndex));
    return ret;
}

/**
 * Offset/Address/Size: 0x18 | 0x80118EC8 | size: 0xC
 */
void DrawableCharacter::RenderOnlyOneCharacter(const cCharacter& character, bool renderOpposingGoalieToo)
{
    spRenderOnlyThisCharacter = (cCharacter*)&character;
    sbRenderOpposingGoalieToo = renderOpposingGoalieToo;
}

/**
 * Offset/Address/Size: 0x8 | 0x80118EB8 | size: 0x10
 */
void DrawableCharacter::RenderAllCharacters()
{
    spRenderOnlyThisCharacter = nullptr;
    sbRenderOpposingGoalieToo = false;
}

/**
 * Offset/Address/Size: 0x0 | 0x80118EB0 | size: 0x8
 */
cCharacter* DrawableCharacter::OnlyRenderingOneCharacter()
{
    return spRenderOnlyThisCharacter;
}

template <typename T>
void DrawableCharacter::Replay(T& frame)
{
    Replayable<1>(frame, mDirt);
    Replayable<1>(frame, FloatCompressor<-128, 128, 8>(mPosition.x));
    Replayable<1>(frame, FloatCompressor<-128, 128, 8>(mPosition.y));
    Replayable<1>(frame, FloatCompressor<-128, 128, 8>(mPosition.z));
    Replayable<1>(frame, FloatCompressor<-128, 128, 8>(mBip01Position.x));
    Replayable<1>(frame, FloatCompressor<-128, 128, 8>(mBip01Position.y));
    Replayable<1>(frame, FloatCompressor<-128, 128, 8>(mBip01Position.z));
    Replayable<1>(frame, FloatCompressor<-128, 128, 8>(mHeadPosition.x));
    Replayable<1>(frame, FloatCompressor<-128, 128, 8>(mHeadPosition.y));
    Replayable<1>(frame, FloatCompressor<-128, 128, 8>(mHeadPosition.z));
    Replayable<1>(frame, FloatCompressor<-512, 512, 8>(mVelocity.x));
    Replayable<1>(frame, FloatCompressor<-512, 512, 8>(mVelocity.y));
    Replayable<1>(frame, FloatCompressor<-512, 512, 8>(mVelocity.z));
    Replayable<1>(frame, mVisible);
    Replayable<1>(frame, mFacingDirection);
    Replayable<1>(frame, mHeadSpin);
    Replayable<1>(frame, mHeadTilt);
    Replayable<1>(frame, (unsigned long&)mEffectsTexturing);
    ReplayablePolymorphic<1>(frame, mPoseTree);

    if (ReplayFrameTraits<T>::IsLoadFrame && frame.mInterval == 1)
    {
        mPoseAccumulator->InitAccumulators();
        mPoseTree->Evaluate(1.0f, mPoseAccumulator);

        BuildNodeMatrices();

        delete mPoseTree;
        mPoseTree = nullptr;
    }
}

template void DrawableCharacter::Replay<SaveFrame>(SaveFrame&);
template void DrawableCharacter::Replay<LoadFrame>(LoadFrame&);
