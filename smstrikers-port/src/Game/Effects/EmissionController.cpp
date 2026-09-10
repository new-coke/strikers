#include "Game/Effects/EmissionController.h"
#include "NL/vmath.h"
#include "Game/Effects/ParticleSystem.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/SAnim/pnSAnimController.h"
#include "NL/nlFile.h"
#include "NL/nlFileGC.h"
#include "NL/nlMemory.h"
#include "types.h"

static int numLingeringSystems;

struct UserEffectSpecClone : public UserEffectSpec
{
    virtual UserEffectSpec* Clone() = 0;
};

static inline UserEffectSpec** GetUserSpecs(EffectsGroup* pGroup)
{
    return pGroup->m_userSpecsPtr;
}

/**
 * Offset/Address/Size: 0xF2C | 0x801F881C | size: 0x104
 */
EmissionController::EmissionController(EffectsGroup* pEffectsGroup, unsigned short id, eGLView view)
{
    m_GlView = view;
    m_Replaying = false;
    m_Age = 0.0f;
    m_ReplayDeltaTime = 0.0f;
    m_Id = id;
    m_bPoseErrorDisplayed = false;
    m_pGroup = pEffectsGroup;
    m_Mirror.x = 1.0f;
    m_Mirror.y = 1.0f;
    m_Mirror.z = 1.0f;
    m_uUserData = 0;

    InitializeSystemsFromGroup();

    m_fGround = 0.015625f;
    m_aFacing = 0;
    m_vPosition.x = 0.0f;
    m_vPosition.y = 0.0f;
    m_vPosition.z = 0.0f;

    m_vDirection.x = 0.0f;
    m_vDirection.y = 0.0f;
    m_vDirection.z = 1.0f;

    m_vVelocity.x = 0.0f;
    m_vVelocity.y = 0.0f;
    m_vVelocity.z = 0.0f;
    m_pPose = nullptr;
    m_pAnimController = nullptr;
    m_uJointIDOverride = 0;
    m_bVisible = true;
    m_bDisabled = false;

    if (m_pGroup->m_isLingering != 0)
    {
        numLingeringSystems++;
    }

    EmissionManager::KillOldest(numLingeringSystems - 12, true);
}

/**
 * Offset/Address/Size: 0xD84 | 0x801F8674 | size: 0x1A8
 */
void EmissionController::InitializeSystemsFromGroup()
{
    EffectsSpec* pSpec;
    EffectsSpec* pEndSpec;
    EffectsSpec* pSpecs;
    pSpecs = m_pGroup->m_specs;
    pSpec = pSpecs;
    pEndSpec = pSpecs + m_pGroup->m_numSpecs;

    while (pSpec < pEndSpec)
    {
        if (pSpec->m_eAttach == FXBind_Joint && pSpec->m_uJointID == 0xFFFFFFFF)
        {
            pSpec++;
            continue;
        }

        EffectsTerrainSpec* pTerrain = pSpec->m_pTerrainSpec;
        u8 bUseThisSpec = true;

        if (pTerrain != NULL && fxGetTerrain != NULL)
        {
            if (!pTerrain->HasTerrain(fxGetTerrain()))
            {
                bUseThisSpec = false;
            }
        }

        if (bUseThisSpec)
        {
            ParticleSystem* pSys = new (nlMalloc(sizeof(ParticleSystem), 8, false)) ParticleSystem(pSpec->m_pTemplate, pSpec);

            pSys->m_fDelay = pSpec->m_fDelay;
            m_Systems.Append(pSys);
        }

        pSpec++;
    }

    m_fGround = 0.015625f;
    m_aFacing = 0;
    m_vPosition.x = 0.0f;
    m_vPosition.y = 0.0f;
    m_vPosition.z = 0.0f;
    m_vDirection.x = 0.0f;
    m_vDirection.y = 0.0f;
    m_vDirection.z = 1.0f;
    m_vVelocity.x = 0.0f;
    m_vVelocity.y = 0.0f;
    m_vVelocity.z = 0.0f;
    m_pPose = NULL;
    m_pAnimController = NULL;
    m_pUserEffects = NULL;
    m_nUserEffects = m_pGroup->m_userSpecs;

    if (m_nUserEffects > 0)
    {
        UserEffectSpec** pUserSpecs = GetUserSpecs(m_pGroup);
        m_pUserEffects = (UserEffectSpec**)nlMalloc(m_nUserEffects * sizeof(UserEffectSpec*), 8, false);

        for (int i = 0; i < m_nUserEffects; i++)
        {
            m_pUserEffects[i] = ((UserEffectSpecClone*)pUserSpecs[i])->Clone();
        }
    }
}

/**
 * Offset/Address/Size: 0xB30 | 0x801F8420 | size: 0x254
 */
EmissionController::~EmissionController()
{
    if (mFinishedCallback)
    {
        mFinishedCallback(*this);
        mFinishedCallback = Function<FnEmissionController>();
    }

    while (m_Systems.m_headNode != NULL)
    {
        ParticleSystem* pSys = (ParticleSystem*)m_Systems.Remove();
        delete pSys;
    }

    if (m_pUserEffects != NULL)
    {
        int i = 0;
        int ofs = 0;
        while (i < m_nUserEffects)
        {
            delete m_pUserEffects[ofs];
            ofs++;
            i++;
        }
        delete[] m_pUserEffects;
    }

    if (m_pGroup->m_isLingering != 0)
    {
        numLingeringSystems--;
    }
}

/**
 * Offset/Address/Size: 0xB14 | 0x801F8404 | size: 0x1C
 */
void EmissionController::SetPosition(const nlVector3& pos)
{
    m_vPosition = pos;
}

/**
 * Offset/Address/Size: 0xB0C | 0x801F83FC | size: 0x8
 */
const nlVector3& EmissionController::GetPosition() const
{
    return m_vPosition;
}

/**
 * Offset/Address/Size: 0xAF0 | 0x801F83E0 | size: 0x1C
 */
void EmissionController::SetDirection(const nlVector3& dir)
{
    m_vDirection = dir;
}

/**
 * Offset/Address/Size: 0xAD4 | 0x801F83C4 | size: 0x1C
 */
void EmissionController::SetVelocity(const nlVector3& velocity)
{
    m_vVelocity = velocity;
}

/**
 * Offset/Address/Size: 0xACC | 0x801F83BC | size: 0x8
 */
void EmissionController::SetPoseAccumulator(const cPoseAccumulator& pose)
{
    m_pPose = &pose;
}

/**
 * Offset/Address/Size: 0xAC4 | 0x801F83B4 | size: 0x8
 */
void EmissionController::SetAnimController(const cPN_SAnimController& animController)
{
    m_pAnimController = &animController;
}

/**
 * Offset/Address/Size: 0x988 | 0x801F8278 | size: 0x13C
 */
void EmissionController::Die()
{
    ParticleSystem* p = (ParticleSystem*)m_Systems.m_headNode;
    while (p != NULL)
    {
        p->Die();
        p = (ParticleSystem*)p->m_nextNode;
    }

    if (mFinishedCallback)
    {
        mFinishedCallback(*this);
        mFinishedCallback = Function<FnEmissionController>();
    }
}

/**
 * Offset/Address/Size: 0x924 | 0x801F8214 | size: 0x64
 */
float EmissionController::GetRemainingTime() const
{
    float maxTime = 0.0f;
    efBaseNode* node = m_Systems.m_headNode;

    while (node != nullptr)
    {
        ParticleSystem* system = (ParticleSystem*)node;
        float remainingTime = system->GetRemainingTime();
        if (remainingTime > maxTime)
        {
            maxTime = remainingTime;
        }
        node = node->m_nextNode;
    }

    return maxTime;
}

/**
 * Offset/Address/Size: 0x918 | 0x801F8208 | size: 0xC
 */
bool EmissionController::IsLingering() const
{
    return m_pGroup->m_isLingering;
}

static inline const cPoseAccumulator* GetEmissionPose(const EmissionController* controller)
{
    return controller->m_pPose;
}

static inline cSHierarchy* GetPoseHierarchy(const cPoseAccumulator* pPose)
{
    return pPose->m_BaseSHierarchy;
}

static inline void ComputeAscendingJointPosition(nlVector3& out, const EmissionController* controller, u32 uJointID, float fVelocity, float fcurrentTime)
{
    float fsetDistance = fVelocity * fcurrentTime;
    const cPoseAccumulator* pPose = GetEmissionPose(controller);
    cSHierarchy* pHier = GetPoseHierarchy(pPose);
    int jointIndex = pHier->GetNodeIndexByID(uJointID);
    int parentIndex = pHier->GetParent(jointIndex);

    while (parentIndex != -1)
    {
        const nlMatrix4& jointMat = pPose->GetNodeMatrix(jointIndex);
        const nlMatrix4& parentMat = pPose->GetNodeMatrix(parentIndex);

        float dy = parentMat.e2[3][1] - jointMat.e2[3][1];
        float dz = parentMat.e2[3][2] - jointMat.e2[3][2];
        float dx = parentMat.e2[3][0] - jointMat.e2[3][0];
        float dist = nlSqrt(dx * dx + dy * dy + dz * dz, true);

        if (dist >= fsetDistance)
        {
            float invRatio;
            float ratio = fsetDistance / dist;
            invRatio = 1.0f - ratio;
            float x = ratio * parentMat.e2[3][0];
            float y = ratio * parentMat.e2[3][1];
            float z = ratio * parentMat.e2[3][2];
            out.x = invRatio * jointMat.e2[3][0] + x;
            out.y = invRatio * jointMat.e2[3][1] + y;
            out.z = invRatio * jointMat.e2[3][2] + z;
            break;
        }

        fsetDistance -= dist;
        jointIndex = parentIndex;
        parentIndex = pHier->GetParent(parentIndex);
    }

    if (parentIndex == -1)
    {
        const nlMatrix4& jointMat = pPose->GetNodeMatrix(jointIndex);
        out = jointMat.GetTranslation();
    }
}

/**
 * Offset/Address/Size: 0x32C | 0x801F7C1C | size: 0x5EC
 */
bool EmissionController::Update(float dt)
{
    if (m_bDisabled)
    {
        return true;
    }

    ParticleSystem* pNext;
    EffectsSpec* pSpec;
    ParticleSystem* pSys;
    int numDel;
    int numSys;
    UserEffectInfo info;
    numSys = 0;
    numDel = 0;

    if (m_Replaying)
    {
        dt = m_ReplayDeltaTime;
    }
    else
    {
        m_Age += dt;
    }

    if (mUpdateCallback)
    {
        mUpdateCallback(*this);
    }

    if (dt <= 0.0f)
    {
        return true;
    }

    pSys = (ParticleSystem*)m_Systems.m_headNode;

    while (pSys != NULL)
    {
        pNext = (ParticleSystem*)pSys->m_nextNode;

        pSys->m_aFacing = m_aFacing;

        pSpec = pSys->m_pSpec;
        EffectsTerrainSpec* pTerrain = pSpec->m_pTerrainSpec;

        if (pTerrain != NULL)
        {
            if (fxGetTerrain() != 0)
            {
                if (!pTerrain->HasTerrain(fxGetTerrain()))
                {
                    pSys = pNext;
                    continue;
                }
            }
        }

        numSys++;
        pSys->m_uLayer = pSpec->m_uLayer;

        nlVector3 pos = m_vPosition;
        nlVector3 vel = m_vVelocity;

        if (pSpec->m_eAttach == FXBind_Joint)
        {
            if (pSpec->m_eJointBinding == JB_Ascend && m_pPose != NULL)
            {
                u32 jointID = m_uJointIDOverride;
                float fJointVelocity;
                float fAge = m_Age;
                vel.x = 0.0f;
                fJointVelocity = pSpec->m_fJointVelocity;
                vel.y = 0.0f;
                vel.z = 0.0f;

                if (jointID == 0)
                {
                    jointID = pSpec->m_uJointID;
                }

                ComputeAscendingJointPosition(pos, this, jointID, fJointVelocity, fAge);
            }
            else if (m_pPose != NULL)
            {
                u32 jointID = pSpec->m_uJointID;

                if (m_pAnimController != NULL && m_pAnimController->m_bMirror)
                {
                    cSHierarchy* pHier = m_pPose->m_BaseSHierarchy;
                    int nodeIndex = pHier->GetNodeIndexByID(jointID);
                    jointID = pHier->GetNodeID(pHier->GetMirroredNode(nodeIndex));
                }

                u32 finalJointID = m_uJointIDOverride;
                if (finalJointID == 0)
                {
                    finalJointID = jointID;
                }

                const nlMatrix4& mat = m_pPose->GetNodeMatrixByHashID(finalJointID);
                pos = mat.GetTranslation();
            }
            else
            {
                if (!m_bPoseErrorDisplayed)
                {
                    EmissionManager::AddError("No Pose Buffer To Play Effect - playing at default position");
                    m_bPoseErrorDisplayed = true;
                }
            }
        }

        if (pSpec->m_bGround)
        {
            pos.z = m_fGround;
        }

        pos.z += pSpec->m_fOffset;

        pSys->m_vPosition = pos;
        pSys->m_vVelocity = vel;
        pSys->m_vForward = m_vDirection;
        pSys->m_Mirror = m_Mirror;

        pSys->UpdateCoordSys();

        pSys->m_bVisible = m_bVisible;

        if (!pSys->Update(dt))
        {
            m_Systems.Remove(pSys);
            delete pSys;
            numDel++;
        }

        pSys = pNext;
    }

    u8 isFinished;
    if (numSys == numDel)
    {
        isFinished = true;
    }
    else
    {
        isFinished = false;
    }

    if (isFinished && mFinishedCallback)
    {
        mFinishedCallback(*this);
        mFinishedCallback = Function<FnEmissionController>();
    }

    if (m_nUserEffects > 0)
    {
        info.pv3Position = &m_vPosition;
        info.pv3Direction = &m_vDirection;

        int i = 0;
        int ofs = 0;
        while (i < m_nUserEffects)
        {
            m_pUserEffects[ofs]->Update(dt, &info);
            isFinished &= m_pUserEffects[ofs]->IsFinished();
            ofs++;
            i++;
        }
    }

    return !isFinished;
}

/**
 * Offset/Address/Size: 0x288 | 0x801F7B78 | size: 0xA4
 */
void* fxLoadEntireFileHigh(const char* filename, unsigned long* fileSize)
{
    void* buffer = nullptr;
    u32 datasize = 0;

    nlFile* file = nlOpen(filename);
    if (file != nullptr)
    {
        unsigned int size;
        datasize = nlFileSize(file, &size);
        buffer = nlMalloc(size, 0x20, true);
        nlRead(file, buffer, datasize);
        nlClose(file);
    }

    if (fileSize != nullptr)
    {
        *fileSize = datasize;
    }

    return buffer;
}

/**
 * Offset/Address/Size: 0x150 | 0x801F7A40 | size: 0x138
 */
void EmissionController::Render()
{
    struct UserEffectInfo
    {
        nlVector3* pv3Position;
        nlVector3* pv3Direction;
    };

    if (m_bDisabled)
    {
        return;
    }

    ParticleSystem* sys = (ParticleSystem*)m_Systems.m_headNode;
    f32 lingerThreshold = 0.0f;

    while (sys != NULL)
    {
        EffectsSpec* spec = sys->m_pSpec;
        bool isLingering = (spec == NULL || spec->m_fLingerStart > lingerThreshold);

        s32 view;
        if (isLingering)
        {
            view = 0x12;
        }
        else
        {
            view = m_GlView;
        }

        if (sys->m_bVisible)
        {
            sys->RenderAllParticles((eGLView)view);
        }

        sys = (ParticleSystem*)sys->m_nextNode;
    }

    if (m_nUserEffects > 0)
    {
        UserEffectInfo info;
        info.pv3Position = &m_vPosition;
        info.pv3Direction = &m_vDirection;
        s32 i = 0;
        s32 j = 0;
        while (i < m_nUserEffects)
        {
            if (!m_pUserEffects[j]->IsFinished())
            {
                s8 view = m_GlView;
                m_pUserEffects[j]->Render((const nlVector3**)&info, (s32)view);
            }
            j++;
            i++;
        }
    }
}

/**
 * Offset/Address/Size: 0xA8 | 0x801F7998 | size: 0xA8
 */
void EmissionController::SetUpdateCallback(const Function1<void, EmissionController&>& callback)
{
    mUpdateCallback = callback;
}

/**
 * Offset/Address/Size: 0x0 | 0x801F78F0 | size: 0xA8
 */
void EmissionController::SetFinishedCallback(const Function1<void, EmissionController&>& callback)
{
    mFinishedCallback = callback;
}
