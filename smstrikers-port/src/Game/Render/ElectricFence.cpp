#include "Game/Render/ElectricFence.h"

#include "Game/Effects/EffectsGroup.h"
#include "Game/Effects/EmissionController.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/Field.h"
#include "Game/Game.h"
#include "Game/GameTweaks.h"
#include "Game/GL/GLMeshWriter.h"
#include "Game/GL/gluMeshWriter.h"
#include "Game/Net.h"
#include "Game/Render/SidelineExplodable.h"
#include "NL/gl/glDraw3.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glState.h"
#include "NL/gl/glView.h"
#include "NL/nlMath.h"
#include "NL/nlTask.h"

#include <math.h>

static float sfGridTextureSize = 7.0f;
static float sfNumGridSquares = 16.0f;
static float sfFadeOutTime = 0.2f;
static float sfAlignmentOffset1 = 0.25f;
static float sfAlignmentOffset2 = 0.13f;
static float sfAngleAnimationRate = 100.0f;
static float sfTimeBetweenEffects = 0.02f;
static int sNumRevolutionsToDisplay = 2;
static float sfAngleRandomOffset = 10.0f;
static float sfStartAngle = 180.0f;
static bool sbUseSparksDuringElectricFenceFlyBy = true;

const unsigned long UnlitProgram = glGetProgram("3d unlit");
const unsigned long LitProgram = glGetProgram("3d pointlit");
const unsigned long LightTexture = glGetTexture("global/lightramp");
const unsigned long BlackTexture = glGetTexture("global/black");
const unsigned long WhiteTexture = glGetTexture("global/white");
const unsigned long GridTexture = glGetTexture("global/grid");

int ElectricFenceData::numAllocated;
nlList<ElectricFenceData> ElectricFenceData::sActiveElectricFences((ElectricFenceData*)NULL, (ElectricFenceData*)NULL);
SlotPool<ElectricFenceData> ElectricFenceData::sElectricFenceDataPool(16, 16);
SlotPool<ElectricFenceGeometry> ElectricFenceGeometry::sElectricFenceGeometryPool(4, 4);

static bool sbIsElectricFenceBeingDisplayed;
static float sfElectricFenceDisplayAngle;

#pragma cpp_extensions on
#pragma msext on

/**
 * Offset/Address/Size: 0x1370 | 0x8016C3A0 | size: 0x2B0
 */
static void GetWallPoint(const nlVector3& impactPosition, float xOffset, float zOffset, nlVector3& outPosition)
{
    float radius = cField::GetCornerRadius();
    float goalLineX = cField::GetGoalLineX(1U);
    float sideLineY = cField::GetSidelineY(1U);
    bool xIsPositive = impactPosition.x > 0.0f;
    bool yIsPositive = impactPosition.y > 0.0f;

    nlVector3 impactPositionPositive = { {
        std::fabs(impactPosition.x),
        std::fabs(impactPosition.y),
        impactPosition.z,
    } };

    float inCoordinate;
    float cornerCircumference = radius;
    cornerCircumference *= 1.5707964f;

    if (impactPositionPositive.x >= goalLineX - radius && impactPositionPositive.y >= sideLineY - radius)
    {
        inCoordinate = nlATan2f(impactPositionPositive.y - (sideLineY - radius), impactPositionPositive.x - (goalLineX - radius));
        inCoordinate = inCoordinate * radius;
    }
    else if (impactPositionPositive.x < goalLineX - radius)
    {
        inCoordinate = cornerCircumference + ((goalLineX - radius) - impactPositionPositive.x);
    }
    else
    {
        inCoordinate = -((sideLineY - radius) - impactPositionPositive.y);
    }

    float increment = sfGridTextureSize / sfNumGridSquares;
    inCoordinate = sfAlignmentOffset2 + increment * (float)floor(inCoordinate / increment);
    float outCoordinate = inCoordinate + xOffset;

    if (outCoordinate <= 0.0f)
    {
        nlVec3Set(outPosition, goalLineX, outCoordinate + (sideLineY - radius), impactPositionPositive.z + zOffset);
    }
    else if (outCoordinate >= cornerCircumference)
    {
        nlVec3Set(outPosition, (goalLineX - radius) - (outCoordinate - cornerCircumference), sideLineY, impactPositionPositive.z + zOffset);
    }
    else
    {
        inCoordinate = nlSin((u16)(s32)(10430.378f * (outCoordinate / radius)));
        nlVec3Set(outPosition,
            (goalLineX - radius) + (radius * nlSin((u16)((u16)(s32)(10430.378f * (outCoordinate / radius)) + 0x4000))),
            (sideLineY - radius) + (radius * inCoordinate),
            impactPositionPositive.z + zOffset);
    }

    if (xIsPositive == 0)
    {
        outPosition.x = -outPosition.x;
    }

    if (yIsPositive == 0)
    {
        outPosition.y = -outPosition.y;
    }
}

#pragma msext reset
#pragma cpp_extensions reset

/**
 * Offset/Address/Size: 0x12CC | 0x8016C2FC | size: 0xA4
 */
static void ElectricFenceFinished(EmissionController& controller)
{
    ElectricFenceData* node = ElectricFenceData::sActiveElectricFences.m_pStart;
    while (node != NULL)
    {
        if (node->mpEmissionController == &controller)
        {
            if (node != NULL)
            {
                nlListRemoveElement<ElectricFenceData>(&ElectricFenceData::sActiveElectricFences.m_pStart, node, &ElectricFenceData::sActiveElectricFences.m_pEnd);
                ElectricFenceData::numAllocated--;
                ElectricFenceGeometry* geom = node->mpGeometry;
                if (geom != NULL)
                {
                    SlotPoolEntry* oldFree = ElectricFenceGeometry::sElectricFenceGeometryPool.m_FreeList;
                    *(SlotPoolEntry**)geom = oldFree;
                    ElectricFenceGeometry::sElectricFenceGeometryPool.m_FreeList = (SlotPoolEntry*)geom;
                }
                SlotPoolEntry* oldFree2 = ElectricFenceData::sElectricFenceDataPool.m_FreeList;
                *(SlotPoolEntry**)node = oldFree2;
                ElectricFenceData::sElectricFenceDataPool.m_FreeList = (SlotPoolEntry*)node;
            }
            return;
        }
        node = node->next;
    }
}

static inline void RenderElectricFenceFlat(const nlVector3& position, const nlVector3& normal, float intensity)
{
    extern const unsigned long GridTexture;

    glSetDefaultState(true);
    glSetRasterState(GLS_AlphaBlend, 2);
    glSetRasterState(GLS_Culling, 0);
    glSetRasterState(GLS_DepthWrite, 0);
    glSetRasterState(GLS_DepthTest, 1);
    glSetCurrentRasterState(glHandleizeRasterState());
    glSetCurrentTexture(GridTexture, GLTT_Diffuse);
    glSetTextureState(GLTS_DiffuseWrap, 0);
    glSetCurrentTextureState(glHandleizeTextureState());

    nlMatrix4 matrix;
    nlMakeRotationMatrixX(matrix, 1.5707964f);

    float angle = nlATan2f(normal.y, normal.x);
    nlMatrix4 matrix2;
    nlMakeRotationMatrixZ(matrix2, 0.0000958738f * (float)(u16)(s32)(10430.378f * angle));
    nlMultMatrices(matrix, matrix, matrix2);

    matrix.m41 = position.x;
    matrix.m42 = position.y;
    matrix.m43 = position.z;
    matrix.m44 = 1.0f;

    glQuad3 quad;
    quad.SetupRotatedRectangle(sfGridTextureSize, sfGridTextureSize, matrix, false, false);

    u8 lightenAmount = (u8)(255.0f * intensity);
    quad.SetColour(lightenAmount, lightenAmount, lightenAmount, 0xFF);
    glAttachQuad3(GLV_ElectricFence, 1, &quad, true);
    glSetDefaultState(false);
}

static inline void DrawPrimitive(const ElectricFenceGeometry& prim, const nlMatrix4& objMatrix, int nPrimType, unsigned long textureHandle, float intensity)
{
    extern const unsigned long UnlitProgram;

    eGLStream stream_decl[3] = { GLStream_Position, GLStream_Colour, GLStream_Diffuse };
    GLMeshWriter mesh;
    nlVector3* pPosition = (nlVector3*)prim.position;
    nlVector2* pTexcoord = (nlVector2*)prim.texcoord;

    glSetDefaultState(true);
    glSetRasterState(GLS_Culling, 0);
    glSetRasterState(GLS_DepthWrite, 0);
    glSetRasterState(GLS_AlphaBlend, 2);
    glSetCurrentRasterState(glHandleizeRasterState());

    uintptr_t matrixHandle = glAllocMatrix();
    if (matrixHandle != (unsigned long)-1)
    {
        glSetMatrix(matrixHandle, objMatrix);
    }
    glSetCurrentMatrix(matrixHandle);

    glSetTextureState(GLTS_DiffuseWrap, 0);
    glSetCurrentTexture(textureHandle, GLTT_Diffuse);
    glSetCurrentTextureState(glHandleizeTextureState());
    glSetCurrentProgram(UnlitProgram);

    u8 lightenAmount = (u8)(255.0f * intensity);
    nlColour c = { 0, 0, 0, 0xFF };
    c.c[0] = lightenAmount;
    c.c[1] = lightenAmount;
    c.c[2] = lightenAmount;

    if (mesh.Begin(prim.vertCount, (eGLPrimitive)nPrimType, 3, stream_decl, false))
    {
        int index = 0;
        while (index < prim.vertCount)
        {
            mesh.Colour(c);
            mesh.Texcoord(pTexcoord[index]);
            mesh.Vertex(pPosition[index]);
            index++;
        }

        if (!mesh.End())
        {
            return;
        }

        glViewAttachModel(GLV_ElectricFence, mesh.GetModel());
    }
}

static inline ElectricFenceData* FindElectricFenceData(EmissionController* pEmissionController)
{
    ElectricFenceData* data = ElectricFenceData::sActiveElectricFences.m_pStart;
    while (data != NULL)
    {
        if (data->mpEmissionController == pEmissionController)
        {
            return data;
        }
        data = data->next;
    }
    return NULL;
}

static void RenderElectricFence(EmissionController& ec);

/**
 * Offset/Address/Size: 0xEAC | 0x8016BEDC | size: 0x420
 */
static void RenderElectricFence(EmissionController& ec)
{
    extern const unsigned long GridTexture;

    ElectricFenceData* pElectricFenceData = FindElectricFenceData(&ec);

    float intensity = 1.0f;
    float remainingTime = ec.GetRemainingTime();
    if (remainingTime < sfFadeOutTime)
    {
        intensity = remainingTime / sfFadeOutTime;
    }

    if (pElectricFenceData == NULL)
    {
        pElectricFenceData = new (ElectricFenceData::sElectricFenceDataPool.Allocate()) ElectricFenceData(&ec);
    }

    if (pElectricFenceData == NULL)
    {
        return;
    }

    if (pElectricFenceData->mbIsFlat)
    {
        RenderElectricFenceFlat(pElectricFenceData->mPosition, nlVector3(pElectricFenceData->mNormal), intensity);
        return;
    }

    nlMatrix4 matrix;
    matrix.SetIdentity();

    DrawPrimitive(*pElectricFenceData->mpGeometry, matrix, GLP_TriStrip, GridTexture, intensity);
}

/**
 * Offset/Address/Size: 0xC8C | 0x8016BCBC | size: 0x220
 */
void EmitElectricFenceBallEffect(const nlVector3& pos, const nlVector3& dir, unsigned long emitterID, bool bNoSpark)
{
    ElectricFenceData* data;
    const char* groupName;
    EmissionController* controller;

    if (g_pGame->mbCaptainShotToScoreOn)
        return;

    nlVector3 clampedPos;
    // PORT: three unsigned longs is twelve bytes on console and twenty-four here.
    ((u32*)&clampedPos)[0] = ((const u32*)&pos)[0];
    ((u32*)&clampedPos)[1] = ((const u32*)&pos)[1];
    ((u32*)&clampedPos)[2] = ((const u32*)&pos)[2];

    float goalLineX = cField::GetGoalLineX(1U);
    float absPosX = (float)__fabs(clampedPos.x);
    if ((float)__fabs(absPosX - goalLineX) < 0.2f)
    {
        if (clampedPos.x > 0.0f)
        {
            clampedPos.x = goalLineX;
        }
        else
        {
            clampedPos.x = -goalLineX;
        }
    }

    groupName = bNoSpark ? "electric_fence_nospark" : "electric_fence";

    if (!EmissionManager::IsPlaying(emitterID, fxGetGroup(groupName)))
    {
        controller = EmissionManager::Create(fxGetGroup(groupName), 0);
        controller->m_uUserData = emitterID;
        controller->SetPosition(clampedPos);

        float angle = nlATan2f(dir.y, dir.x);
        controller->m_aFacing = (u16)(10430.378f * angle);

        data = ElectricFenceData::sElectricFenceDataPool.Allocate();

        new (data) ElectricFenceData(controller);

        controller->SetUpdateCallback(RenderElectricFence);
        controller->SetFinishedCallback(ElectricFenceFinished);
    }
}

/**
 * Offset/Address/Size: 0xAB8 | 0x8016BAE8 | size: 0x1D4
 */
void EmitElectricFenceCharacterEffect(const nlVector3& pos, const nlVector3& dir, unsigned long emitterID)
{
    if (g_pGame->mbCaptainShotToScoreOn)
        return;

    if (!EmissionManager::IsPlaying(emitterID, fxGetGroup("electric_fence_character")))
    {
        EmissionController* controller = EmissionManager::Create(fxGetGroup("electric_fence_character"), 0);
        controller->m_uUserData = emitterID;
        controller->SetPosition(pos);

        float angle = nlATan2f(dir.y, dir.x);
        controller->m_aFacing = (u16)(10430.378f * angle);

        ElectricFenceData* data = ElectricFenceData::sElectricFenceDataPool.Allocate();

        new (data) ElectricFenceData(controller);

        controller->SetUpdateCallback(RenderElectricFence);
        controller->SetFinishedCallback(ElectricFenceFinished);
    }

    SidelineExplodableManager::TriggerExplosions(pos, g_pGame->m_pGameTweaks->fBobombMediumRadius * g_pGame->m_pGameTweaks->fPowerupExplosionRadius);
}

/**
 * Offset/Address/Size: 0xAB4 | 0x8016BAE4 | size: 0x4
 */
void InitializeElectricFence()
{
}

/**
 * Offset/Address/Size: 0x9E8 | 0x8016BA18 | size: 0xCC
 */
void FreeElectricFence()
{
    ElectricFenceData* node;
    while ((node = ElectricFenceData::sActiveElectricFences.m_pStart) != NULL)
    {
        if (node != NULL)
        {
            nlListRemoveElement<ElectricFenceData>(&ElectricFenceData::sActiveElectricFences.m_pStart, node, &ElectricFenceData::sActiveElectricFences.m_pEnd);
            ElectricFenceData::numAllocated--;
            ElectricFenceGeometry* geom = node->mpGeometry;
            if (geom != NULL)
            {
                SlotPoolEntry* oldFree = ElectricFenceGeometry::sElectricFenceGeometryPool.m_FreeList;
                *(SlotPoolEntry**)geom = oldFree;
                ElectricFenceGeometry::sElectricFenceGeometryPool.m_FreeList = (SlotPoolEntry*)geom;
            }
            SlotPoolEntry* oldFree2 = ElectricFenceData::sElectricFenceDataPool.m_FreeList;
            *(SlotPoolEntry**)node = oldFree2;
            ElectricFenceData::sElectricFenceDataPool.m_FreeList = (SlotPoolEntry*)node;
        }
    }
    SlotPoolBase::BaseFreeBlocks(&ElectricFenceData::sElectricFenceDataPool, sizeof(ElectricFenceData));
    SlotPoolBase::BaseFreeBlocks(&ElectricFenceGeometry::sElectricFenceGeometryPool, sizeof(ElectricFenceGeometry));
}

static inline void CreateElectricFenceGeometry(ElectricFenceGeometry& prim, const nlVector3& impactPosition)
{
    prim.vertCount = 32;

    nlVector3* pdst = prim.position;
    nlVector2* tdst = prim.texcoord;

    float startOffset = 0.5f * -sfGridTextureSize;
    float endOffset = 0.5f * sfGridTextureSize;
    float fDeltaSegmentOffset = (endOffset - startOffset) / 15.0f;
    float z0 = impactPosition.z + startOffset;
    float z1 = impactPosition.z + endOffset;

    for (int nSegment = 0; nSegment < 16;)
    {
        nlVector3 wallPoint;
        GetWallPoint(impactPosition, (((float)nSegment) * fDeltaSegmentOffset) + startOffset, 0.0f, wallPoint);

        int segment = nSegment;
        float wallY = wallPoint.y;
        float wallX = wallPoint.x;

        pdst[0].x = wallX;
        pdst[0].y = wallY;
        pdst[0].z = z0;
        tdst[0].x = (float)segment / 15.0f;
        tdst[0].y = 0.0f;

        float wY2 = wallPoint.y;
        float wX2 = wallPoint.x;
        pdst[1].x = wX2;
        pdst[1].y = wY2;
        pdst[1].z = z1;
        tdst[1].x = (float)segment / 15.0f;
        tdst[1].y = 1.0f;
        nSegment++;

        pdst += 2;
        tdst += 2;
    }
}

/**
 * Offset/Address/Size: 0x5A0 | 0x8016B5D0 | size: 0x448
 * 100% match.
 */
ElectricFenceData::ElectricFenceData(EmissionController* pEmissionController)
{
    extern float AIsgn(float);

    float zTop, zBottom, step, negHalf, posHalf, z_val, grid, half;

    mpEmissionController = pEmissionController;
    mfIntensity = 0.0f;
    mpGeometry = NULL;

    nlListAddEnd<ElectricFenceData>(&sActiveElectricFences.m_pStart, &sActiveElectricFences.m_pEnd, this);
    numAllocated++;

    mPosition = pEmissionController->GetPosition();

    f64 absY = __fabs(mPosition.y);
    f64 diffY = __fabs((f32)absY - cField::GetSidelineY(1U));
    f32 distanceFromSideline = (f32)diffY;

    f64 absX = __fabs(mPosition.x);
    f64 diffX = __fabs((f32)absX - cField::GetGoalLineX(1U));
    f32 distanceFromGoal = (f32)diffX;

    float cornerDiameter = 2.0f * cField::GetCornerRadius();
    if (distanceFromGoal > cornerDiameter || distanceFromSideline > cornerDiameter)
    {
        mbIsFlat = true;

        if (distanceFromGoal < distanceFromSideline)
        {
            u8 isXPositive = mPosition.x > 0.0f;
            if (isXPositive)
            {
                f32 goalX;
                if (isXPositive)
                    goalX = cField::GetGoalLineX(1U);
                else
                    goalX = -cField::GetGoalLineX(1U);
                mPosition.x = goalX;
            }

            mNormal.x = 0.0f;
            mNormal.y = 1.0f;
            mNormal.z = 0.0f;

            float increment = sfGridTextureSize / sfNumGridSquares;
            u8 neg = false;
            if (mPosition.y < 0.0f)
            {
                mPosition.y = -mPosition.y;
                neg = true;
            }

            mPosition.y = (increment * (float)floor(mPosition.y / increment)) + sfAlignmentOffset1;
            if (neg)
                mPosition.y = -mPosition.y;

            mPosition.z = increment * (float)floor(mPosition.z / increment);
        }
        else
        {
            u8 isYPositive = mPosition.y > 0.0f;
            if (isYPositive)
            {
                f32 sideY;
                if (isYPositive)
                    sideY = cField::GetSidelineY(1U);
                else
                    sideY = -cField::GetSidelineY(1U);
                mPosition.y = sideY;
            }

            mNormal.x = 1.0f;
            mNormal.y = 0.0f;
            mNormal.z = 0.0f;

            float increment = sfGridTextureSize / sfNumGridSquares;
            mPosition.x = increment * (float)floor(mPosition.x / increment);
            mPosition.z = increment * (float)floor(mPosition.z / increment);
        }
    }
    else
    {
        mbIsFlat = false;

        float increment = sfGridTextureSize / sfNumGridSquares;
        mPosition.z = increment * (float)floor(mPosition.z / increment);

        ElectricFenceGeometry* geom = NULL;

        if (ElectricFenceGeometry::sElectricFenceGeometryPool.m_FreeList == NULL)
            SlotPoolBase::BaseAddNewBlock(&ElectricFenceGeometry::sElectricFenceGeometryPool, sizeof(ElectricFenceGeometry));

        SlotPoolEntry* freeSlot = ElectricFenceGeometry::sElectricFenceGeometryPool.m_FreeList;
        if (freeSlot != NULL)
        {
            geom = (ElectricFenceGeometry*)freeSlot;
            ElectricFenceGeometry::sElectricFenceGeometryPool.m_FreeList = freeSlot->next;
        }

        mpGeometry = geom;

        nlVector3 impactPosition = mPosition;
        ElectricFenceGeometry* activeGeom = mpGeometry;

        cField::GetCornerRadius();
        cField::GetGoalLineX(1U);
        AIsgn(impactPosition.x);
        cField::GetSidelineY(1U);
        AIsgn(impactPosition.y);

        CreateElectricFenceGeometry(*activeGeom, impactPosition);
        return;
    }
}

/**
 * Offset/Address/Size: 0x58C | 0x8016B5BC | size: 0x14
 */
void DisplayElectricFence()
{
    sbIsElectricFenceBeingDisplayed = true;
    sfElectricFenceDisplayAngle = sfStartAngle;
}

/**
 * Offset/Address/Size: 0x560 | 0x8016B590 | size: 0x2C
 */
void StopDisplayingElectricFence()
{
    sbIsElectricFenceBeingDisplayed = false;
    EmissionManager::DestroyAll(true);
}

/**
 * Offset/Address/Size: 0x0 | 0x8016B030 | size: 0x560
 */
void UpdateElectricFence(float fDeltaT)
{
    static unsigned long counter;
    static signed char init;
    static float timeSinceLastEffect;
    static signed char init2;
    if (nlTaskManager::m_pInstance->m_CurrState == 1)
        return;
    if (!sbIsElectricFenceBeingDisplayed)
        return;
    if (!init)
    {
        counter = 1;
        init = 1;
    }
    if (!init2)
    {
        timeSinceLastEffect = 0.0f;
        init2 = 1;
    }
    while (timeSinceLastEffect > sfTimeBetweenEffects)
    {
        float goalLineX = cField::GetGoalLineX(1U);
        float sideLineY = cField::GetSidelineY(1U);
        float randomAngleOffset = nlRandomf(-sfAngleRandomOffset, sfAngleRandomOffset, &nlDefaultSeed);
        nlVector3 pos = { 0.0f, 0.0f, 0.0f };
        nlVector3 normal;
        u16 sinArg = (u16)(s32)(10430.378f * (3.1415927f * (sfElectricFenceDisplayAngle + randomAngleOffset) / 180.0f));
        pos.x = nlSin(sinArg);
        sinArg = (u16)(s32)(10430.378f * (3.1415927f * (sfElectricFenceDisplayAngle + randomAngleOffset) / 180.0f));
        pos.y = nlSin((u16)(sinArg + 0x4000));
        float scale;
        if (pos.x == 0.0f)
        {
            scale = sideLineY;
            normal.x = 0.0f;
            normal.y = 1.0f;
        }
        else if ((float)pos.y == 0.0f)
        {
            scale = goalLineX;
            normal.x = 1.0f;
            normal.y = 0.0f;
        }
        else
        {
            float goalLineScale = goalLineX / pos.x;
            float sideLineScale = sideLineY / pos.y;
            if (goalLineScale < 0.0f)
                goalLineScale = -goalLineScale;
            if (sideLineScale < 0.0f)
                sideLineScale = -sideLineScale;
            if (goalLineScale < sideLineScale)
            {
                scale = goalLineScale;
                normal.x = 1.0f;
                normal.y = 0.0f;
            }
            else
            {
                scale = sideLineScale;
                normal.x = 0.0f;
                normal.y = 1.0f;
            }
        }
        nlVec3Scale(pos, pos, scale);
        pos.z = nlRandomf(0.0f, 5.0f, &nlDefaultSeed);
        if ((counter & 1) == 0)
        {
            pos.x = -pos.x;
        }
        float netWidth = cNet::m_fNetWidth;
        float netHeight = cNet::m_fNetHeight;
        if ((float)__fabs(pos.x - goalLineX) < 0.01)
        {
            if ((float)__fabs(pos.y) < netWidth)
            {
                pos.z = nlRandomf(netHeight, 5.0f, &nlDefaultSeed);
            }
        }
        EmitElectricFenceBallEffect(pos, normal, counter++, !sbUseSparksDuringElectricFenceFlyBy);
        timeSinceLastEffect = timeSinceLastEffect - sfTimeBetweenEffects;
    }
    timeSinceLastEffect = timeSinceLastEffect + fDeltaT;
    sfElectricFenceDisplayAngle = sfElectricFenceDisplayAngle + sfAngleAnimationRate * fDeltaT;
    float endAngle = sfStartAngle + 180.0f * (float)(s32)sNumRevolutionsToDisplay;
    if (sfElectricFenceDisplayAngle > endAngle)
    {
        sbIsElectricFenceBeingDisplayed = false;
    }
}
