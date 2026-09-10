#include "Game/Render/CameraGuy.h"

#include "limits.h"

#include "Game/ReplayManager.h"
#include "NL/nlTask.h"

#include "NL/nlMemory.h"

inline int GetCameraTiltMax()
{
    return 13653;
}

inline u16 GetCameraOOIConstraint()
{
    return 0x8000;
}

inline int GetCameraSpinMax()
{
    return 0x8000;
}

u16 g_aCameraOOIConstraint = GetCameraOOIConstraint();
int g_nCameraSpinMax = (int)GetCameraOOIConstraint();
int g_nCameraTiltMax = GetCameraTiltMax();

static const float kUnitsToRad = 0.0000958738f;
inline float AngUnitsToRad_fromFloat(float fUnits)
{
    unsigned short u16 = (unsigned short)(int)fUnits;
    return (float)u16 * kUnitsToRad;
}

/**
 * Offset/Address/Size: 0x1D0 | 0x8015DB34 | size: 0x1AC
 */
static void CameraGuyHeadTrackUpdateCallback(uintptr_t ctx, unsigned int arg1, cPoseAccumulator* poseAccumulator, unsigned int arg3, int arg4)
{
    nlMatrix4 m4Intermediate;  // r1+0x88
    nlMatrix4 m4RotMatrix;     // r1+0x48
    nlMatrix4 m4NewHeadMatrix; // r1+0x8

    int index = arg3;
    const CameraGuy* pCameraGuy = (CameraGuy*)ctx;
    const nlMatrix4& m4AnimatedHeadMatrix = poseAccumulator->GetNodeMatrix(index);

    pCameraGuy->mpHeadTrack->Update(
        m4AnimatedHeadMatrix,
        m4AnimatedHeadMatrix,
        nlTaskManager::m_pInstance->m_fCurrentTimeDelta,
        g_aCameraOOIConstraint,
        g_nCameraSpinMax,
        g_nCameraTiltMax,
        0.1f);

    nlMakeRotationMatrixX(m4RotMatrix, AngUnitsToRad_fromFloat(pCameraGuy->mpHeadTrack->m_fHeadSpin));
    nlMultMatrices(m4Intermediate, m4RotMatrix, m4AnimatedHeadMatrix);
    nlMakeRotationMatrixZ(m4RotMatrix, AngUnitsToRad_fromFloat(pCameraGuy->mpHeadTrack->m_fHeadTilt));
    nlMultMatrices(m4NewHeadMatrix, m4RotMatrix, m4Intermediate);

    poseAccumulator->m_NodeMatrices.mData[index] = m4NewHeadMatrix;
}

/**
 * Offset/Address/Size: 0x170 | 0x8015DAD4 | size: 0x60
 */
CameraGuy::CameraGuy(cSHierarchy& pHierarchy, int nModelID)
    : SkinAnimatedNPC(pHierarchy, nModelID)
{
    mpHeadTrack = new (nlMalloc(sizeof(cHeadTrack), 8, 0)) cHeadTrack();
}

/**
 * Offset/Address/Size: 0x138 | 0x8015DA9C | size: 0x38
 */
void CameraGuy::Init()
{
    mpPoseAccumulator->SetBuildNodeMatrixCallback(2, CameraGuyHeadTrackUpdateCallback, (uintptr_t)this, 0);
}

/**
 * Offset/Address/Size: 0xB4 | 0x8015DA18 | size: 0x84
 */
CameraGuy::~CameraGuy()
{
    delete mpHeadTrack;
}

/**
 * Offset/Address/Size: 0x94 | 0x8015D9F8 | size: 0x20
 */
void CameraGuy::Render()
{
    SkinAnimatedNPC::Render();
}

/**
 * Offset/Address/Size: 0x6C | 0x8015D9D0 | size: 0x28
 */
void CameraGuy::SetIdleAnim(cSAnim& pAnim)
{
    SkinAnimatedNPC::SetAnimState(pAnim, 0.2f, PM_CYCLIC);
}

/**
 * Offset/Address/Size: 0x0 | 0x8015D964 | size: 0x6C
 */
void CameraGuy::Update(float dt)
{
    SkinAnimatedNPC::Update(dt);
    mpHeadTrack->m_bTrackOOI = true;

    if (ReplayManager::Instance()->mRender != NULL)
    {
        mpHeadTrack->m_v3OOI = ReplayManager::Instance()->mRender->mBall.mPosition;
    }
}
