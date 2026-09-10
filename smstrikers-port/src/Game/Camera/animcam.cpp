#include "Game/Camera/animcam.h"
#include "NL/nlWare.h"
#include "dolphin/os.h"
extern "C" unsigned long port_cam_swap(void*, unsigned long);

#include "Game/AI/AiUtil.h"
#include "NL/nlFile.h"
#include "NL/gl/glMatrix.h"
#include "NL/nlMemory.h"
#include "Game/Render/depthoffield.h"
#include "Game/FixedUpdateTask.h"

static float dofBehindTarget = 2.0f;

cCameraData* cAnimCamera::m_cameraDataList;

/**
 * Offset/Address/Size: 0x113C | 0x801A5D30 | size: 0x2C
 */
static void EnableDofDebug()
{
    if (DepthOfFieldManager::instance.m_bDebugView)
    {
        DepthOfFieldManager::instance.m_bDebugView = 0;
    }
    else
    {
        DepthOfFieldManager::instance.m_bDebugView = 1;
    }
}

struct DofDebugFlag
{
    DofDebugFlag()
    {
        m_active = false;
        m_callback = EnableDofDebug;
    }
    bool m_active;
    void (*m_callback)();
};

static DofDebugFlag g_EnableDofDebug;

static inline void* nlGetChunkData(nlChunk* chunk)
{
    u32 alignField = chunk->m_ID & 0x7F000000;
    u32 isAligned = ((-alignField) | alignField) >> 31;
    if (isAligned != 0)
    {
        u32 alignment = 1u << (alignField >> 24);
        uintptr_t ptr = (uintptr_t)chunk + alignment;
        ptr += 7;
        ptr &= ~(alignment - 1);
        return (void*)ptr;
    }
    return (void*)((u8*)chunk + 8);
}

static inline nlChunk* nlGetNextChunk(nlChunk* chunk)
{
    return (nlChunk*)((u8*)chunk + chunk->m_Size + 8);
}

template <class T>
static inline void nlGetChunkDataAs(nlChunk* chunk, T*& out)
{
    u32 alignField = chunk->m_ID & 0x7F000000;
    u32 isAligned = ((-alignField) | alignField) >> 31;
    if (isAligned != 0)
    {
        u32 alignment;
        T* ptr;
        alignment = 1u << (alignField >> 24);
        ptr = (T*)((uintptr_t)chunk + alignment);
        ptr = (T*)((uintptr_t)ptr + 7);
        out = (T*)((uintptr_t)ptr & ~(uintptr_t)(alignment - 1));
    }
    else
    {
        out = (T*)((u8*)chunk + 8);
    }
}

/**
 * Offset/Address/Size: 0xCA4 | 0x801A5898 | size: 0x498
 */
static bool LoadAnimCameraData(nlChunk* outerChunk, nlChunk* outerEnd, cCameraData* pAnimCameraData, bool ownsKeyData)
{
    pAnimCameraData->ownsKeyData = ownsKeyData;
    while (outerChunk < outerEnd)
    {
        u32 id = outerChunk->m_ID;
        u32 type = id & 0x80FFFFFF;
        switch (type)
        {
        case 0x15508:
            pAnimCameraData->m_uKeyCount = *(u32*)nlGetChunkData(outerChunk);
            if (ownsKeyData)
            {
                pAnimCameraData->cameraPos = (nlVector3*)nlMalloc(pAnimCameraData->m_uKeyCount * sizeof(nlVector3), 8, false);
                pAnimCameraData->targetPos = (nlVector3*)nlMalloc(pAnimCameraData->m_uKeyCount * sizeof(nlVector3), 8, false);
                pAnimCameraData->cameraRot = (nlQuaternion*)nlMalloc(pAnimCameraData->m_uKeyCount * sizeof(nlQuaternion), 8, false);
                pAnimCameraData->fFOV = (float*)nlMalloc(pAnimCameraData->m_uKeyCount * sizeof(float), 8, false);
                pAnimCameraData->fFocalLength = (float*)nlMalloc(pAnimCameraData->m_uKeyCount * sizeof(float), 8, false);
            }
            else
            {
                pAnimCameraData->cameraPos = NULL;
                pAnimCameraData->targetPos = NULL;
                pAnimCameraData->cameraRot = NULL;
                pAnimCameraData->fFOV = NULL;
                pAnimCameraData->fFocalLength = NULL;
            }
            break;
        case 0x15509:
        {
            nlVector3* v3Pos;
            nlGetChunkDataAs(outerChunk, v3Pos);
            if (ownsKeyData)
            {
                unsigned long offset;
                unsigned long i;
                i = 0;
                offset = 0;
                while (i < pAnimCameraData->m_uKeyCount)
                {
                    *(nlVector3*)((u8*)pAnimCameraData->cameraPos + offset) = *v3Pos;
                    i++;
                    offset += sizeof(nlVector3);
                    v3Pos++;
                }
            }
            else
                pAnimCameraData->cameraPos = v3Pos;
            break;
        }
        case 0x1550C:
        {
            nlVector3* v3Pos;
            nlGetChunkDataAs(outerChunk, v3Pos);
            if (ownsKeyData)
            {
                unsigned long offset;
                unsigned long i;
                i = 0;
                offset = 0;
                while (i < pAnimCameraData->m_uKeyCount)
                {
                    *(nlVector3*)((u8*)pAnimCameraData->targetPos + offset) = *v3Pos;
                    i++;
                    offset += sizeof(nlVector3);
                    v3Pos++;
                }
            }
            else
                pAnimCameraData->targetPos = v3Pos;
            break;
        }
        case 0x15511:
        {
            nlQuaternion* rot;
            nlGetChunkDataAs(outerChunk, rot);
            if (ownsKeyData)
            {
                unsigned long offset;
                unsigned long i;
                i = 0;
                offset = 0;
                while (i < pAnimCameraData->m_uKeyCount)
                {
                    *(nlQuaternion*)((u8*)pAnimCameraData->cameraRot + offset) = *rot;
                    i++;
                    offset += sizeof(nlQuaternion);
                    rot++;
                }
            }
            else
                pAnimCameraData->cameraRot = rot;
            break;
        }
        case 0x1550F:
            if (ownsKeyData)
            {
                unsigned long offset;
                u8* chunkData = (u8*)outerChunk + 8;
                unsigned long i;
                u32 one = 1;
                i = 0;
                offset = 0;
                while (i < pAnimCameraData->m_uKeyCount)
                {
                    u32 alignField = outerChunk->m_ID & 0x7F000000;
                    u32 isAligned = ((-alignField) | alignField) >> 31;
                    float* src;
                    if (isAligned != 0)
                    {
                        u32 mask = (one << (alignField >> 24)) - 1;
                        src = (float*)(((uintptr_t)chunkData + mask) & ~(uintptr_t)mask);
                    }
                    else
                    {
                        src = (float*)chunkData;
                    }
                    *(float*)((u8*)pAnimCameraData->fFOV + offset) = *(float*)((u8*)src + offset);
                    i++;
                    offset += sizeof(float);
                }
            }
            else
            {
                float* data;
                nlGetChunkDataAs(outerChunk, data);
                pAnimCameraData->fFOV = data;
            }
            break;
        case 0x15510:
            if (ownsKeyData)
            {
                unsigned long offset;
                u8* chunkData = (u8*)outerChunk + 8;
                unsigned long i;
                u32 one = 1;
                i = 0;
                offset = 0;
                while (i < pAnimCameraData->m_uKeyCount)
                {
                    u32 alignField = outerChunk->m_ID & 0x7F000000;
                    u32 isAligned = ((-alignField) | alignField) >> 31;
                    float* src;
                    if (isAligned != 0)
                    {
                        u32 mask = (one << (alignField >> 24)) - 1;
                        src = (float*)(((uintptr_t)chunkData + mask) & ~(uintptr_t)mask);
                    }
                    else
                    {
                        src = (float*)chunkData;
                    }
                    *(float*)((u8*)pAnimCameraData->fFocalLength + offset) = *(float*)((u8*)src + offset);
                    i++;
                    offset += sizeof(float);
                }
            }
            else
            {
                float* data;
                nlGetChunkDataAs(outerChunk, data);
                pAnimCameraData->fFocalLength = data;
            }
            break;
        }
        outerChunk = nlGetNextChunk(outerChunk);
    }
    return true;
}

/**
 * Offset/Address/Size: 0xBF8 | 0x801A57EC | size: 0xAC
 */
bool cAnimCamera::LoadCameraAnimation(nlChunk* begin, nlChunk* end, const char* cameraName, bool ownsKeyData)
{
    cCameraData* pData;
    void* mem = nlMalloc(sizeof(cCameraData), 8, false);
    pData = (cCameraData*)mem;
    if (mem != NULL)
    {
        ((cCameraData*)mem)->next = NULL;
        ((cCameraData*)mem)->m_uHashID = 0;
        ((cCameraData*)mem)->m_uKeyCount = 0;
        ((cCameraData*)mem)->cameraPos = NULL;
        ((cCameraData*)mem)->targetPos = NULL;
        ((cCameraData*)mem)->cameraRot = NULL;
        ((cCameraData*)mem)->fFOV = NULL;
        ((cCameraData*)mem)->fFocalLength = NULL;
    }
    pData->m_uHashID = nlStringLowerHash(cameraName);
    bool b = LoadAnimCameraData(begin, end, pData, ownsKeyData);
    nlListAddStart(&m_cameraDataList, pData, (cCameraData**)NULL);
    return b;
}

/**
 * Offset/Address/Size: 0xB14 | 0x801A5708 | size: 0xE4
 */
bool cAnimCamera::LoadCameraAnimation(const char* szFilename, const char* szCameraName, bool ownsKeyData)
{
    bool result;
    unsigned long uSize = 0;
    nlChunk* end;
    nlChunk* begin;
    cCameraData* pCamData;
    void* pData = nlLoadEntireFile(szFilename, &uSize, 0x20, (eAllocType)0);
    if (pData == NULL)
    {
        return false;
    }

    // PORT: big-endian and walked in place; a file cam_endian.c refuses would be walked out of bounds.
    if (port_cam_swap(pData, uSize) == 0)
    {
        OSReport("Error: '%s' is not a well-formed camera animation\n", szFilename);
        operator delete(pData);
        return false;
    }
    begin = (nlChunk*)((u8*)pData + 8);
    end = (nlChunk*)((u8*)pData + ((nlChunk*)pData)->m_Size + 8);

    void* mem = nlMalloc(sizeof(cCameraData), 8, false);
    pCamData = (cCameraData*)mem;
    if (mem != NULL)
    {
        ((cCameraData*)mem)->next = NULL;
        ((cCameraData*)mem)->m_uHashID = 0;
        ((cCameraData*)mem)->m_uKeyCount = 0;
        ((cCameraData*)mem)->cameraPos = NULL;
        ((cCameraData*)mem)->targetPos = NULL;
        ((cCameraData*)mem)->cameraRot = NULL;
        ((cCameraData*)mem)->fFOV = NULL;
        ((cCameraData*)mem)->fFocalLength = NULL;
    }
    pCamData->m_uHashID = nlStringLowerHash(szCameraName);
    result = LoadAnimCameraData(begin, end, pCamData, ownsKeyData);
    nlListAddStart(&m_cameraDataList, pCamData, (cCameraData**)NULL);
    operator delete(pData);
    return result;
}

/**
 * Offset/Address/Size: 0xAE8 | 0x801A56DC | size: 0x2C
 */
void cAnimCamera::FreeCameraAnimations()
{
    nlDeleteList(&m_cameraDataList);
    m_cameraDataList = NULL;
}

/**
 * Offset/Address/Size: 0xA54 | 0x801A5648 | size: 0x94
 */
cAnimCamera::cAnimCamera()
{
    m_bUseSimulationTime = false;
    m_LetManagerDoUpdate = true;
    m_bUnusedPad = false;
    m_fAnimationTime = 0.0f;
    m_fAnimationSpeed = 1.0f;
    m_fLastSimulationTime = -1.0f;
    m_pActiveCameraData = NULL;
    mFacingAngle = 0;
    m_EndOfAnimationCallback = NULL;
    nlVec3Set(m_vecTarget, 0.0f, 0.0f, 0.0f);
    nlVec3Set(m_OffsetPos, 0.0f, 0.0f, 0.0f);
    nlVec3Set(m_Mirror, 1.0f, 1.0f, 1.0f);
    m_pActiveCameraData = m_cameraDataList;
    m_bCyclic = true;
}

/**
 * Offset/Address/Size: 0x9F8 | 0x801A55EC | size: 0x5C
 */
cAnimCamera::~cAnimCamera()
{
}

/**
 * Offset/Address/Size: 0x41C | 0x801A5010 | size: 0x5DC
 */
void cAnimCamera::BuildAnimViewMatrix(nlMatrix4& mView)
{
    float fRealIndex = m_fAnimationTime * (float)(m_pActiveCameraData->m_uKeyCount - 1);
    int nIndex = (int)fRealIndex;
    float fWeightB = fRealIndex - (float)nIndex;
    float fWeightA = 1.0f - fWeightB;
    nlVector3 cameraPos = { };
    nlVector3 targetPos = { };
    nlQuaternion cameraRot = { 0.0f, 0.0f, 0.0f, 1.0f };
    nlMatrix4 viewMatrix;
    nlMatrix4 facingAngleMatrix;
    if (m_fAnimationTime >= 1.0f)
    {
        m_Fov = m_pActiveCameraData->fFOV[nIndex];
        cameraPos = m_pActiveCameraData->cameraPos[nIndex];
        targetPos = m_pActiveCameraData->targetPos[nIndex];
        cameraRot = m_pActiveCameraData->cameraRot[nIndex];
    }
    else
    {
        nlVector3& cpN = m_pActiveCameraData->cameraPos[nIndex + 1];
        nlVector3& cpK = m_pActiveCameraData->cameraPos[nIndex];
        nlVector3 delta;
        nlVec3Sub(delta, cpK, cpN);
        float distSq = delta.GetLengthSq3D();
        if (distSq > 16.0f)
        {
            if (fWeightB < 0.5f)
            {
                cameraPos = m_pActiveCameraData->cameraPos[nIndex];
                targetPos = m_pActiveCameraData->targetPos[nIndex];
                cameraRot = m_pActiveCameraData->cameraRot[nIndex];
                m_Fov = m_pActiveCameraData->fFOV[nIndex];
            }
            else
            {
                cameraPos = m_pActiveCameraData->cameraPos[nIndex + 1];
                targetPos = m_pActiveCameraData->targetPos[nIndex + 1];
                cameraRot = m_pActiveCameraData->cameraRot[nIndex + 1];
                m_Fov = m_pActiveCameraData->fFOV[nIndex + 1];
            }
        }
        else
        {
            m_Fov = fWeightA * m_pActiveCameraData->fFOV[nIndex] + fWeightB * m_pActiveCameraData->fFOV[nIndex + 1];
            nlVec3WeightedSum(cameraPos, fWeightA, m_pActiveCameraData->cameraPos[nIndex], fWeightB, m_pActiveCameraData->cameraPos[nIndex + 1]);
            nlVec3WeightedSum(targetPos, fWeightA, m_pActiveCameraData->targetPos[nIndex], fWeightB, m_pActiveCameraData->targetPos[nIndex + 1]);
            nlQuatSlerp(cameraRot, m_pActiveCameraData->cameraRot[nIndex], m_pActiveCameraData->cameraRot[nIndex + 1], fWeightB);
        }
    }

    nlVector3 dofDelta;
    nlVec3Sub(dofDelta, m_vecCamera, m_vecTarget);
    float dist = nlSqrt(dofDelta.GetLengthSq3D(), true);
    DepthOfFieldManager::instance.m_fDistanceFromCamera = dofBehindTarget + dist;
    cameraPos.x *= m_Mirror.x;
    cameraPos.y *= m_Mirror.y;
    cameraPos.z *= m_Mirror.z;
    targetPos.x *= m_Mirror.x;
    targetPos.y *= m_Mirror.y;
    targetPos.z *= m_Mirror.z;
    if (m_Mirror.x != 1.0f || m_Mirror.y != 1.0f || m_Mirror.z != 1.0f)
    {
        cameraRot.x *= -m_Mirror.x;
        cameraRot.y *= -m_Mirror.y;
        cameraRot.z *= -m_Mirror.z;
    }
    nlVec3Add(m_vecCamera, cameraPos, m_OffsetPos);
    nlVec3Add(m_vecTarget, targetPos, m_OffsetPos);
    GetLocalPoint(m_vecCamera, m_vecCamera, m_OffsetPos, 0);
    GetWorldPoint(m_vecCamera, m_vecCamera, m_OffsetPos, mFacingAngle);
    if (m_bUnusedPad)
    {
        nlVector3 up = { 0.0f, 0.0f, 1.0f };
        glMatrixLookAt(mView, m_vecCamera, m_vecTarget, up);
    }
    else
    {
        nlQuatToMatrix(viewMatrix, cameraRot);
        float facingAngleRadians = (float)mFacingAngle * 0.0000958738f;
        nlMakeRotationMatrixZ(facingAngleMatrix, facingAngleRadians);
        nlMultMatrices(viewMatrix, viewMatrix, facingAngleMatrix);
        viewMatrix.m41 = m_vecCamera.x;
        viewMatrix.m42 = m_vecCamera.y;
        viewMatrix.m43 = m_vecCamera.z;
        viewMatrix.m44 = 1.0f;
        nlInvertMatrix(mView, viewMatrix);
    }
}

/**
 * Offset/Address/Size: 0x410 | 0x801A5004 | size: 0xC
 */
void cAnimCamera::UnselectCameraAnimation()
{
    m_pActiveCameraData = 0;
}

/**
 * Offset/Address/Size: 0x39C | 0x801A4F90 | size: 0x74
 */
void cAnimCamera::SelectCameraAnimation(const char* name)
{
    m_fLastSimulationTime = -1.0f;
    u32 hash = nlStringLowerHash(name);

    cCameraData* pData = m_cameraDataList;
    while (pData != nullptr)
    {
        if (pData->m_uHashID == hash)
        {
            m_pActiveCameraData = pData;
            return;
        }
        pData = pData->next;
    }

    m_fAnimationTime = 0.0f;
    BuildAnimViewMatrix(m_matView);
}

/**
 * Offset/Address/Size: 0x34C | 0x801A4F40 | size: 0x50
 */
bool cAnimCamera::CameraAnimationExists(const char* name) const
{
    u32 hash = nlStringLowerHash(name);
    cCameraData* data = m_cameraDataList;
    while (data != NULL)
    {
        if (data->m_uHashID == hash)
        {
            return true;
        }
        data = data->next;
    }
    return false;
}

/**
 * Offset/Address/Size: 0x2AC | 0x801A4EA0 | size: 0xA0
 */
void cAnimCamera::FreeCameraAnimation(const char* szCameraName)
{
    unsigned long uHashID = nlStringLowerHash(szCameraName);
    cCameraData* pCameraData = m_cameraDataList;
    while (pCameraData != NULL)
    {
        if (pCameraData->m_uHashID == uHashID)
        {
            nlListRemoveElement(&m_cameraDataList, pCameraData, (cCameraData**)NULL);
            if (pCameraData != NULL)
            {
                if (pCameraData->ownsKeyData)
                {
                    operator delete[](pCameraData->cameraPos);
                    operator delete[](pCameraData->targetPos);
                    operator delete[](pCameraData->cameraRot);
                    operator delete[](pCameraData->fFOV);
                    operator delete[](pCameraData->fFocalLength);
                }
                operator delete(pCameraData);
            }
            return;
        }
        pCameraData = pCameraData->next;
    }
}

static inline float GetSimulationDelta(float lastTime, float& simTime)
{
    simTime = FixedUpdateTask::mSimulationTime;
    return simTime - lastTime;
}

/**
 * Offset/Address/Size: 0x150 | 0x801A4D44 | size: 0x15C
 */
void cAnimCamera::Update(float dt)
{
    if (!m_LetManagerDoUpdate)
    {
        return;
    }

    if (m_bUseSimulationTime)
    {
        float lastTime = m_fLastSimulationTime;
        float duration = 0.0f;
        if (lastTime < duration)
        {
            m_fLastSimulationTime = FixedUpdateTask::mSimulationTime;
        }
        else
        {
            float simTime;
            float delta = GetSimulationDelta(lastTime, simTime);
            if (m_pActiveCameraData != NULL)
            {
                duration = (float)(m_pActiveCameraData->m_uKeyCount - 1) / 30.0f;
            }
            m_fAnimationTime += (delta * m_fAnimationSpeed) / duration;
            m_fLastSimulationTime = simTime;
        }
    }
    else
    {
        float duration;
        if (m_pActiveCameraData != NULL)
        {
            duration = (float)(m_pActiveCameraData->m_uKeyCount - 1) / 30.0f;
        }
        else
        {
            duration = 0.0f;
        }
        m_fAnimationTime += (dt * m_fAnimationSpeed) / duration;
    }

    if (m_fAnimationTime >= 1.0f)
    {
        if (m_bCyclic)
        {
            m_fAnimationTime -= 1.0f;
        }
        else
        {
            m_fAnimationTime = 1.0f;
        }
        if (m_EndOfAnimationCallback != NULL)
        {
            m_EndOfAnimationCallback();
        }
    }

    BuildAnimViewMatrix(m_matView);
}

/**
 * Offset/Address/Size: 0x0 | 0x801A4BF4 | size: 0x150
 */
void cAnimCamera::ManualUpdate(float dt)
{
    if (m_bUseSimulationTime)
    {
        float lastTime = m_fLastSimulationTime;
        float duration = 0.0f;
        if (lastTime < duration)
        {
            m_fLastSimulationTime = FixedUpdateTask::mSimulationTime;
        }
        else
        {
            float simTime = FixedUpdateTask::mSimulationTime;
            float delta = simTime - lastTime;
            if (m_pActiveCameraData != NULL)
            {
                duration = (float)(m_pActiveCameraData->m_uKeyCount - 1) / 30.0f;
            }
            m_fAnimationTime += (delta * m_fAnimationSpeed) / duration;
            m_fLastSimulationTime = simTime;
        }
    }
    else
    {
        float duration;
        if (m_pActiveCameraData != NULL)
        {
            duration = (float)(m_pActiveCameraData->m_uKeyCount - 1) / 30.0f;
        }
        else
        {
            duration = 0.0f;
        }
        m_fAnimationTime += (dt * m_fAnimationSpeed) / duration;
    }

    if (m_fAnimationTime >= 1.0f)
    {
        if (m_bCyclic)
        {
            m_fAnimationTime -= 1.0f;
        }
        else
        {
            m_fAnimationTime = 1.0f;
        }
        if (m_EndOfAnimationCallback != NULL)
        {
            m_EndOfAnimationCallback();
        }
    }

    BuildAnimViewMatrix(m_matView);
}
