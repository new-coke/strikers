#include "Game/Camera/CameraMan.h"
#include "NL/vmath.h"

#include "NL/nlDLRing.h"
#include "NL/nlConfig.h"
#include "NL/nlFormat.h"
#include "NL/nlString.h"
#include "NL/platqmath.h"

#include "Game/Sys/eventman.h"

#include "Game/Camera/animcam.h"
#include "Game/Camera/rumblefilter.h"
#include "Game/Camera/GameplayCam.h"
#include "NL/nlBasicString.h"
#include "Game/Debug/ShapeRender.h"
#include "Game/Drawable/DrawableObj.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/Field.h"
#include "Game/Net.h"
#include "Game/WorldManager.h"
#include "Game/AI/AiUtil.h"

#include "math.h"

extern float g_fSimulationTick;

static f32 CANT_COLLIDE = *(f32*)__float_max;

eCameraType g_eCurrentCameraType;
cBaseCamera* cCameraManager::m_cameraStack;
float cCameraManager::m_fTransitionSpeed;
float cCameraManager::m_fPrevFOV;
eCameraTransition cCameraManager::m_transition;
u16 cCameraManager::m_aJoystickRemap;
void (*cCameraManager::m_pCallback)(enum eCameraMessage);
int cCameraManager::m_UpVectorStackSize;

nlMatrix4 cCameraManager::m_matView;
nlVector3 cCameraManager::m_cameraPosition;
nlMatrix4 cCameraManager::m_matPrevView;
int cCameraManager::m_pBeginFrameCameraType = 13;
float cCameraManager::m_fTransitionTime = 1.0f;
float cCameraManager::m_fFOV = 50.0f;

nlVector3 cCameraManager::m_UpVectorStack[2] = { { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } };

cRumbleFilter* pRumbleFilter;

/**
 * Offset/Address/Size: 0x1D04 | 0x801A838C | size: 0x68
 */
void FireCameraRumbleFilter(float fRumbleX, float fRumbleY)
{
    nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack);
    if (nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack)->m_pFilter != NULL)
    {
        nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack)->m_pFilter->Rumble(fRumbleX, fRumbleY);
    }
}

/**
 * Offset/Address/Size: 0x1768 | 0x801A7DF0 | size: 0x59C
 */
void cCameraManager::Startup()
{
    int i;
    cBaseCamera* pBaseCamera = (cBaseCamera*)new ((GameplayCamera*)nlMalloc(sizeof(GameplayCamera), 8, false)) GameplayCamera();
    pBaseCamera->m_pFilter = pRumbleFilter = new (nlMalloc(sizeof(cRumbleFilter), 8, false)) cRumbleFilter();

    if (m_transition != eCT_NONE)
    {
        nlPrintf("Camera Transition In Progress\n");
        if (m_pCallback != NULL)
            m_pCallback(eCM_ABORTED_BY_PUSH);
    }
    m_transition = eCT_NONE;

    StopCurrentCamRumbleFilterSFXLoop();

    nlDLRingAddStart<cBaseCamera>(&m_cameraStack, pBaseCamera);
    g_eCurrentCameraType = pBaseCamera->GetType();

    cAnimCamera::LoadCameraAnimation("art/cameras/startscreen.cam", "startscreen", true);
    cAnimCamera::LoadCameraAnimation("art/cameras/ShootToScoreCamera.cam", "ShootToScoreCamera", true);

    for (i = 0; i < NUM_TEAMS; i++)
    {
        BasicString<char, Detail::TempStringAllocator> fileName = Format(BasicString<char, Detail::TempStringAllocator>("art/cameras/{0}_shoottoscorecamera.cam"), GetTeamName((eTeamID)i));
        BasicString<char, Detail::TempStringAllocator> camName = Format(BasicString<char, Detail::TempStringAllocator>("{0}_ShootToScoreCamera"), GetTeamName((eTeamID)i));
        cAnimCamera::LoadCameraAnimation(fileName.c_str(), camName.c_str(), true);
    }

    cAnimCamera::LoadCameraAnimation("art/cameras/pause.cam", "pause", true);

    Update(0.017f);
}

/**
 * Offset/Address/Size: 0x1730 | 0x801A7DB8 | size: 0x38
 */
void cCameraManager::Shutdown()
{
    nlDeleteDLRing<cBaseCamera>(&cCameraManager::m_cameraStack);
    cCameraManager::m_cameraStack = NULL;
    cAnimCamera::FreeCameraAnimations();
    delete pRumbleFilter;
}

/**
 * Offset/Address/Size: 0x10CC | 0x801A7754 | size: 0x664
 */
void cCameraManager::Update(float fDeltaT)
{
    nlVector3 v3TransTo;
    nlVector3 v3TransFrom;
    nlQuaternion qSlerped;
    nlQuaternion qCur;
    nlQuaternion qPrev;
    nlMatrix4 cameraToWorldMatrix;
    nlMatrix4 prevViewCopy;
    nlMatrix4 curViewCopy;
    nlMatrix4 filteredViewNone;
    nlMatrix4 filteredViewEase;

    if (m_cameraStack == NULL)
        return;
    UpdateGameCameraType();

    cBaseCamera* pCamera = nlDLRingGetStart<cBaseCamera>(m_cameraStack);
    pCamera->mUpVector = m_UpVectorStack[m_UpVectorStackSize];
    pCamera->Update(fDeltaT);
    if (pCamera->m_pFilter != NULL)
        pCamera->m_pFilter->Update(fDeltaT);

    if (m_transition != eCT_NONE)
    {
        switch (m_transition)
        {
        case eCT_EASE_IN:
        {
            float fSimTick = g_fSimulationTick;
            pCamera = nlDLRingGetStart<cBaseCamera>(m_cameraStack);
            if (pCamera != NULL)
            {
                prevViewCopy = m_matPrevView;
                nlMatrixToQuat(qPrev, prevViewCopy);

                v3TransFrom = prevViewCopy.GetTranslation();

                curViewCopy = PeekCamera()->GetViewMatrix();
                if (PeekCamera()->m_pFilter != NULL)
                {
                    PeekCamera()->m_pFilter->Filter(curViewCopy, filteredViewEase);
                    curViewCopy = filteredViewEase;
                }
                nlMatrixToQuat(qCur, curViewCopy);

                float oneMinusT;
                float t = m_fTransitionTime;
                float smoothT = t * t * t * (t * (6.0f * t + (-15.0f)) + 10.0f);
                v3TransTo = curViewCopy.GetTranslation();
                nlQuatSlerp(qSlerped, qPrev, qCur, smoothT);
                oneMinusT = 1.0f - smoothT;

                nlQuatToMatrix(m_matView, qSlerped);
                m_matView.m41 = oneMinusT * v3TransFrom.x + smoothT * v3TransTo.x;
                m_matView.m42 = oneMinusT * v3TransFrom.y + smoothT * v3TransTo.y;
                m_matView.m43 = oneMinusT * v3TransFrom.z + smoothT * v3TransTo.z;
                m_matView.m44 = 1.0f;

                m_fFOV = Interpolate(m_fPrevFOV, pCamera->GetFOV(), smoothT);
                if (m_fFOV < 1.0f)
                    m_fFOV = 1.0f;

                m_fTransitionTime = m_fTransitionTime + fSimTick * m_fTransitionSpeed;
                if (m_fTransitionTime > 1.0f)
                {
                    m_transition = eCT_NONE;
                    if (m_pCallback != NULL)
                    {
                        m_pCallback(eCM_COMPLETE);
                        m_pCallback = NULL;
                    }
                }
            }
            nlInvertRotTransMatrix(cameraToWorldMatrix, m_matView);
            m_cameraPosition = cameraToWorldMatrix.GetTranslation();
            break;
        }
        default:
            break;
        }
    }
    else
    {
        m_matView = pCamera->GetViewMatrix();
        m_cameraPosition = pCamera->GetCameraPosition();
        m_fFOV = pCamera->GetFOV();
        if (m_fFOV < 1.0f)
            m_fFOV = 1.0f;
        if (PeekCamera()->m_pFilter != NULL)
        {
            PeekCamera()->m_pFilter->Filter(m_matView, filteredViewNone);
            m_matView = filteredViewNone;
        }
    }

    m_aJoystickRemap = (u16)(int)(nlATan2f(m_matView.m23, m_matView.m13) * 10430.378f);
    m_aJoystickRemap += 0x8000;
}

void* nlMalloc(size_t, unsigned int, bool);

class cDebugCamera
{
public:
    cDebugCamera();
};

class ReplayCamera
{
public:
    ReplayCamera();
};

class TopDownCamera
{
public:
    TopDownCamera();
};

class cFollowCamera
{
public:
    enum FollowTarget
    {
        FOLLOW_BALL = 0,
        FOLLOW_CHARACTER = 1,
        FOLLOW_SELECTABLE = 2,
        FOLLOW_ANIM_VIEWER_CHARACTER = 3,
    };

    cFollowCamera(FollowTarget);
};

class cKickOffCamera
{
public:
    cKickOffCamera();
};

class GoalCamera
{
public:
    GoalCamera();
};

class cShootToScoreCamera
{
public:
    cShootToScoreCamera();
};

class cAnimViewerCamera
{
public:
    cAnimViewerCamera();
};

class FaceCam
{
public:
    FaceCam(float);
};

/**
 * Offset/Address/Size: 0xD78 | 0x801A7400 | size: 0x354
 */
void cCameraManager::UpdateGameCameraType()
{
    cBaseCamera* pBaseCamera = nlDLRingGetEnd(cCameraManager::m_cameraStack);

    if (g_eCurrentCameraType != pBaseCamera->GetType())
    {
        Config& cfg = Config::Global();
        Config::TagValuePair& tvp = cfg.FindTvp("nocameratweakcrash");

        bool noCameraTweakCrash;
        if (tvp.tag == NULL)
        {
            cfg.Set("nocameratweakcrash", false);
            noCameraTweakCrash = false;
        }
        else if (tvp.type == 0)
        {
            noCameraTweakCrash = LexicalCast<bool, bool>(tvp.value.b);
        }
        else if (tvp.type == 1)
        {
            noCameraTweakCrash = LexicalCast<bool, int>(tvp.value.i);
        }
        else if (tvp.type == 2)
        {
            noCameraTweakCrash = LexicalCast<bool, float>(tvp.value.f);
        }
        else if (tvp.type == 3)
        {
            noCameraTweakCrash = LexicalCast<bool, const char*>(tvp.value.s);
        }
        else
        {
            noCameraTweakCrash = false;
        }

        if (noCameraTweakCrash)
        {
            if (g_eCurrentCameraType > eCameraType_Gameplay)
            {
                g_eCurrentCameraType = eCameraType_Gameplay;
            }
        }

        WorldManager::s_World->HandleCameraSwitch();
        pBaseCamera->m_pFilter = NULL;
        nlDLRingRemoveEnd(&cCameraManager::m_cameraStack);
        delete pBaseCamera;

        switch (g_eCurrentCameraType)
        {
        case eCameraType_Debug:
            pBaseCamera = (cBaseCamera*)new ((cDebugCamera*)nlMalloc(sizeof(cDebugCamera), 8, false)) cDebugCamera();
            break;
        case eCameraType_Replay:
            pBaseCamera = (cBaseCamera*)new ((ReplayCamera*)nlMalloc(sizeof(ReplayCamera), 8, false)) ReplayCamera();
            break;
        case eCameraType_TopDown:
            pBaseCamera = (cBaseCamera*)new ((TopDownCamera*)nlMalloc(sizeof(TopDownCamera), 8, false)) TopDownCamera();
            break;
        case eCameraType_FollowCharacter:
            pBaseCamera = (cBaseCamera*)new ((cFollowCamera*)nlMalloc(sizeof(cFollowCamera), 8, false)) cFollowCamera(cFollowCamera::FOLLOW_CHARACTER);
            break;
        case eCameraType_FollowBall:
            pBaseCamera = (cBaseCamera*)new ((cFollowCamera*)nlMalloc(sizeof(cFollowCamera), 8, false)) cFollowCamera(cFollowCamera::FOLLOW_BALL);
            break;
        case eCameraType_Animated:
            pBaseCamera = (cBaseCamera*)new ((cAnimCamera*)nlMalloc(sizeof(cAnimCamera), 8, false)) cAnimCamera();
            break;
        case eCameraType_KickOff:
            pBaseCamera = (cBaseCamera*)new ((cKickOffCamera*)nlMalloc(sizeof(cKickOffCamera), 8, false)) cKickOffCamera();
            break;
        case eCameraType_Gameplay:
            pBaseCamera = (cBaseCamera*)new ((GameplayCamera*)nlMalloc(sizeof(GameplayCamera), 8, false)) GameplayCamera();
            break;
        case eCameraType_Goal:
            pBaseCamera = (cBaseCamera*)new ((GoalCamera*)nlMalloc(sizeof(GoalCamera), 8, false)) GoalCamera();
            break;
        case eCameraType_ShootToScore:
            pBaseCamera = (cBaseCamera*)new ((cShootToScoreCamera*)nlMalloc(sizeof(cShootToScoreCamera), 8, false)) cShootToScoreCamera();
            break;
        case eCameraType_AnimViewer:
            pBaseCamera = (cBaseCamera*)new ((cAnimViewerCamera*)nlMalloc(sizeof(cAnimViewerCamera), 8, false)) cAnimViewerCamera();
            break;
        case eCameraType_FaceCloseup:
            pBaseCamera = (cBaseCamera*)new ((FaceCam*)nlMalloc(sizeof(FaceCam), 8, false)) FaceCam(2.0f);
            break;
        default:
            break;
        }

        nlDLRingAddEnd(&cCameraManager::m_cameraStack, pBaseCamera);
    }
}

/**
 * Offset/Address/Size: 0xD50 | 0x801A73D8 | size: 0x28
 */
bool cCameraManager::HasCamera(cBaseCamera* pCamera)
{
    return nlDLRingValidateContainsElement<cBaseCamera>(cCameraManager::m_cameraStack, pCamera);
}

/**
 * Offset/Address/Size: 0xC58 | 0x801A72E0 | size: 0xF8
 */
void cCameraManager::PushCamera(cBaseCamera* pCamera)
{
    if (cCameraManager::m_transition != eCT_NONE)
    {
        nlPrintf("Camera Transition In Progress\n");
        if (cCameraManager::m_pCallback != NULL)
        {
            (*cCameraManager::m_pCallback)(eCM_ABORTED_BY_PUSH);
        }
    }

    cCameraManager::m_transition = eCT_NONE;

    cCameraManager::StopCurrentCamRumbleFilterSFXLoop();

    nlDLRingAddStart<cBaseCamera>(&cCameraManager::m_cameraStack, pCamera);
}

/**
 * Offset/Address/Size: 0xBA8 | 0x801A7230 | size: 0xB0
 */
void cCameraManager::Remove(const cBaseCamera& camera)
{
    bool actuallyRemoved = true;
    while (actuallyRemoved)
    {
        actuallyRemoved = nlDLRingRemoveSafely<cBaseCamera>(&cCameraManager::m_cameraStack, &camera);
        if (actuallyRemoved
            && (nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack) != NULL)
            && (nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack)->m_pFilter != 0))
        {
            cRumbleFilter* pFilter = nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack)->m_pFilter;
            pFilter->Reset();
            nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack)->Reactivate();
        }
    }
}

/**
 * Offset/Address/Size: 0xA94 | 0x801A711C | size: 0x114
 */
void cCameraManager::Remove(eCameraType type, bool bDeleteAfterRemoving)
{
    bool actuallyRemoved;
    cBaseCamera* pCamera = cCameraManager::m_cameraStack;

    if (cCameraManager::m_cameraStack != NULL)
    {
        cBaseCamera* pCameraNext;
        do
        {
            pCameraNext = pCamera->m_next;
            if (type == pCamera->GetType())
            {
                actuallyRemoved = true;
                while (actuallyRemoved)
                {
                    actuallyRemoved = nlDLRingRemoveSafely<cBaseCamera>(&cCameraManager::m_cameraStack, pCamera);
                    if (actuallyRemoved
                        && (nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack) != NULL)
                        && (nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack)->m_pFilter != 0))
                    {
                        cRumbleFilter* pFilter = nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack)->m_pFilter;
                        pFilter->Reset();
                        nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack)->Reactivate();
                    }
                }

                if (bDeleteAfterRemoving)
                {
                    delete pCamera;
                }
            }
            pCamera = pCameraNext;
        } while (pCameraNext != cCameraManager::m_cameraStack);
    }
}

/**
 * Offset/Address/Size: 0x7F8 | 0x801A6E80 | size: 0x29C
 */
void cCameraManager::PushCameraWithTransition(cBaseCamera* pCamera, float fDuration, eCameraTransition transition, void (*pCallback)(eCameraMessage))
{
    if (cCameraManager::m_transition != eCT_NONE)
    {
        nlPrintf("Camera Transition In Progress\n");
        if (cCameraManager::m_pCallback != NULL)
        {
            (*cCameraManager::m_pCallback)(eCM_ABORTED_BY_PUSH);
        }
    }

    cCameraManager::m_matPrevView = cCameraManager::PeekCamera()->GetViewMatrix();
    cCameraManager::m_fPrevFOV = cCameraManager::PeekCamera()->GetFOV();

    if (cCameraManager::PeekCamera()->m_pFilter != NULL)
    {
        nlMatrix4 matView;
        cCameraManager::PeekCamera()->m_pFilter->Filter(cCameraManager::m_matPrevView, matView);
        cCameraManager::m_matPrevView = matView;
    }

    cCameraManager::m_transition = transition;
    cCameraManager::m_fTransitionSpeed = 1.0f / fDuration;
    cCameraManager::m_fTransitionTime = 0.0f;
    cCameraManager::m_pCallback = pCallback;

    if ((cCameraManager::PeekCamera() != NULL)
        && (cCameraManager::PeekCamera()->m_pFilter != 0))
    {
        nlVector2 diff_pos;
        nlVec2Sub(diff_pos, cCameraManager::PeekCamera()->m_pFilter->v2Pos0, cCameraManager::PeekCamera()->m_pFilter->v2Pos1);
        if (nlSqrt((diff_pos.x * diff_pos.x) + (diff_pos.y * diff_pos.y), 1) > 0.0f)
        {
            g_pEventManager->CreateValidEvent(0x58, 0x14);
        }
    }

    nlDLRingAddStart<cBaseCamera>(&cCameraManager::m_cameraStack, pCamera);
}

/**
 * Offset/Address/Size: 0x590 | 0x801A6C18 | size: 0x268
 */
cBaseCamera* cCameraManager::PopCameraWithTransition(float fDuration, eCameraTransition transition, void (*pCallback)(eCameraMessage))
{
    if (cCameraManager::m_transition != eCT_NONE)
    {
        nlPrintf("Camera Transition In Progress\n");
        if (cCameraManager::m_pCallback != NULL)
        {
            (*cCameraManager::m_pCallback)(eCM_ABORTED_BY_POP);
        }
    }

    cCameraManager::m_matPrevView = cCameraManager::PeekCamera()->GetViewMatrix();
    cCameraManager::m_fPrevFOV = cCameraManager::PeekCamera()->GetFOV();

    if (cCameraManager::PeekCamera()->m_pFilter != NULL)
    {
        nlMatrix4 matView;
        cCameraManager::PeekCamera()->m_pFilter->Filter(cCameraManager::m_matPrevView, matView);
        cCameraManager::m_matPrevView = matView;
    }

    cCameraManager::m_transition = transition;
    cCameraManager::m_pCallback = pCallback;
    cCameraManager::m_fTransitionSpeed = 1.0f / fDuration;
    cCameraManager::m_fTransitionTime = 1.0f - cCameraManager::m_fTransitionTime;

    return cCameraManager::PerformCameraPop();
}

static void DrawBoundingSphere(const DrawableObject* drawable)
{
    const nlMatrix4& worldMatrix = drawable->GetWorldMatrix();
    nlVector3 centre = *(const nlVector3*)&worldMatrix.m41;
    const nlColour yellow = { 0xFF, 0xFF, 0, 0 };
    const nlColour blue = { 0, 0, 0xFF, 0 };

    nlMatrix4 viewMatrix = cCameraManager::PeekCamera()->GetViewMatrix();
    nlVector4 xVector;
    nlVector4 yVector;
    nlVec4Set(xVector, viewMatrix.m11, viewMatrix.m12, viewMatrix.m13, 0.0f);
    nlVec4Set(yVector, viewMatrix.m21, viewMatrix.m22, viewMatrix.m23, 0.0f);

    u16 startAngle = (u16)(s32)(10430.378f * nlATan2f(xVector.y, xVector.x));
    nlVector3 lastPoint;
    nlVector3 firstPoint;
    int i;
    for (i = 0; i < 16; i++)
    {
        nlVector3 p;
        nlVector3 v;
        nlSinCos(&v.x, &v.y, (u16)(startAngle + (i << 12)));
        v.z = 0.0f;
        p.x = centre.x + drawable->m_fBoundingRadius * (xVector.x * v.x + yVector.x * v.y);
        p.y = centre.y + drawable->m_fBoundingRadius * (xVector.y * v.x + yVector.y * v.y);
        p.z = centre.z + drawable->m_fBoundingRadius * (xVector.z * v.x + yVector.z * v.y);

        if (i == 0)
        {
            firstPoint = p;
        }
        else
        {
            g_ShapeRenderer.DrawLine3D(lastPoint, p, blue, true);
        }
        lastPoint = p;
    }

    g_ShapeRenderer.DrawLine3D(lastPoint, firstPoint, blue, true);
    g_ShapeRenderer.DrawSpherePrimitive(worldMatrix, drawable->m_fBoundingRadius, yellow);
}

/**
 * Offset/Address/Size: 0x200 | 0x801A6888 | size: 0x390
 */
unsigned char cCameraManager::IsObjectOccludingField(const DrawableObject* drawable)
{
    const nlMatrix4& worldMatrix = drawable->GetWorldMatrix();
    nlVector3 boundingSphereCentre = *(const nlVector3*)&worldMatrix.m41;
    const nlVector3& cameraPosition = m_cameraPosition;

    float objectRadius = drawable->m_fBoundingRadius;
    float netDepth = cNet::m_fNetDepth;
    float goalLineXPlusNetDepth = cField::GetGoalLineX(1U);
    goalLineXPlusNetDepth = goalLineXPlusNetDepth + netDepth;
    float sideLineY = cField::GetSidelineY(1U);

    if ((cameraPosition.x > -goalLineXPlusNetDepth)
        && (cameraPosition.x < goalLineXPlusNetDepth)
        && (cameraPosition.y > -sideLineY)
        && (cameraPosition.y < sideLineY))
    {
        bool objectInBounds = false;
        if ((boundingSphereCentre.x > -goalLineXPlusNetDepth)
            && (boundingSphereCentre.x < goalLineXPlusNetDepth)
            && (boundingSphereCentre.y > -sideLineY)
            && (boundingSphereCentre.y < sideLineY))
        {
            objectInBounds = true;
        }

        if (objectInBounds == false)
        {
            return false;
        }
    }

    if ((cameraPosition.x < -goalLineXPlusNetDepth) && (boundingSphereCentre.x > goalLineXPlusNetDepth))
        return false;
    if ((cameraPosition.x > goalLineXPlusNetDepth) && (boundingSphereCentre.x < -goalLineXPlusNetDepth))
        return false;
    if ((cameraPosition.y < -sideLineY) && (boundingSphereCentre.y > sideLineY))
        return false;
    if ((cameraPosition.y > sideLineY) && (boundingSphereCentre.y < -sideLineY))
        return false;

    if ((boundingSphereCentre.z - objectRadius) < 0.0f)
    {
        boundingSphereCentre.z += objectRadius - boundingSphereCentre.z;
    }

    nlVector3 fieldCorners[4];
    nlVector3 planeNormals[4];

    fieldCorners[0].x = -goalLineXPlusNetDepth;
    fieldCorners[0].y = -sideLineY;
    fieldCorners[0].z = 0.0f;
    fieldCorners[1].x = -goalLineXPlusNetDepth;
    fieldCorners[1].y = sideLineY;
    fieldCorners[1].z = 0.0f;
    fieldCorners[2].x = goalLineXPlusNetDepth;
    fieldCorners[2].y = sideLineY;
    fieldCorners[2].z = 0.0f;
    fieldCorners[3].x = goalLineXPlusNetDepth;
    fieldCorners[3].y = -sideLineY;
    fieldCorners[3].z = 0.0f;

    int i;
    for (i = 0; i < 4; i++)
    {
        int next = (i + 1) % 4;
        nlVector3 edge;
        nlVector3 delta;
        nlVec3Sub(edge, fieldCorners[next], fieldCorners[i]);
        nlVec3Sub(delta, fieldCorners[i], cameraPosition);
        nlVec3Cross(planeNormals[i], edge, delta);
        float lengthSquared = planeNormals[i].GetLengthSq3D();
        float invLength = nlRecipSqrt(lengthSquared, true);
        nlVec3Scale(planeNormals[i], invLength);
    }

    nlVector3 objectDelta;
    nlVec3Sub(objectDelta, boundingSphereCentre, cameraPosition);

    for (i = 0; i < 4; i++)
    {
        if (nlVec3DotProduct(planeNormals[i], objectDelta) > objectRadius)
            return false;
    }
    return true;
}

/**
 * Offset/Address/Size: 0x1A4 | 0x801A682C | size: 0x5C
 */
float cCameraManager::GetDistanceFromCameraToObject(const nlVector3& objectPosition)
{
    nlVector3 diff;
    nlVec3Set(diff,
        objectPosition.x - cCameraManager::m_cameraPosition.x,
        objectPosition.y - cCameraManager::m_cameraPosition.y,
        objectPosition.z - cCameraManager::m_cameraPosition.z);

    return nlSqrt(((diff.x) * (diff.x)) + ((diff.y) * (diff.y)) + ((diff.z) * (diff.z)), 1);
}

/**
 * Offset/Address/Size: 0x174 | 0x801A67FC | size: 0x30
 */
void cCameraManager::GetViewVector(nlVector3& viewVector)
{
    nlVec3Set(viewVector,
        -cCameraManager::m_matView.m13,
        -cCameraManager::m_matView.m23,
        -cCameraManager::m_matView.m33);
}

/**
 * Offset/Address/Size: 0x150 | 0x801A67D8 | size: 0x24
 */
void cCameraManager::GetUpVector(nlVector3& upVector)
{
    nlVec3Set(upVector,
        cCameraManager::m_matView.m12,
        cCameraManager::m_matView.m22,
        cCameraManager::m_matView.m32);
}

/**
 * Offset/Address/Size: 0x1C | 0x801A66A4 | size: 0x134
 */
void cCameraManager::SetWorldUpVectorTilt(float fXAxisTilt, float fYAxisTilt)
{
    float fSin;
    float fCos;

    nlSinCos(&fSin, &fCos, ((s32)(65536.0f * fXAxisTilt)) / 360);

    nlVector3* const pUp = &m_UpVectorStack[0];

    pUp->x = 0.0f;
    pUp->y = fSin;
    pUp->z = fCos;

    nlSinCos(&fSin, &fCos, ((s32)(65536.0f * fYAxisTilt)) / 360);

    pUp->x = fSin;
    pUp->z = pUp->z * fCos;

    float xx = pUp->x * pUp->x;
    float yy = pUp->y * pUp->y;
    float zz = pUp->z * pUp->z;
    float temp_f1 = nlRecipSqrt(xx + yy + zz, true);
    nlVec3Scale(*pUp, temp_f1);
}

void cCameraManager::StopCurrentCamRumbleFilterSFXLoop()
{
    if (nlDLRingGetStart<cBaseCamera>(m_cameraStack) != NULL)
    {
        if (nlDLRingGetStart<cBaseCamera>(m_cameraStack)->m_pFilter != NULL)
        {
            cRumbleFilter* pFilter1 = nlDLRingGetStart<cBaseCamera>(m_cameraStack)->m_pFilter;
            cRumbleFilter* pFilter2 = nlDLRingGetStart<cBaseCamera>(m_cameraStack)->m_pFilter;
            float dy = pFilter2->v2Pos0.y - pFilter1->v2Pos1.y;
            float dx = pFilter2->v2Pos0.x - pFilter1->v2Pos1.x;
            if (nlSqrt(dx * dx + dy * dy, true) > 0.0f)
            {
                g_pEventManager->CreateValidEvent(0x58, 0x14);
            }
        }
    }
}

/**
 * Offset/Address/Size: 0xC | 0x801A6694 | size: 0x10
 */
void cCameraManager::PushWorldUpVector()
{
    cCameraManager::m_UpVectorStackSize++;
}

/**
 * Offset/Address/Size: 0x0 | 0x801A6688 | size: 0xC
 */
void cCameraManager::PopWorldUpVector()
{
    cCameraManager::m_UpVectorStackSize = 0;
}
