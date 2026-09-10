#include "Game/BeginFrameTask.h"

#include "Game/Debug/FrameCounter.h"
#include "Game/Drawable/DrawableModel.h"
#include "Game/NisPlayer.h"
#include "Game/Render/depthoffield.h"
#include "Game/main.h"
#include "NL/gl/glState.h"
#include "NL/gl/glUserData.h"
#include "Game/GameObjectLighting.h"

#include "NL/nlConfig.h"
#include "NL/nlFileGC.h"
#include "NL/nlDLRing.h"
#include "NL/gl/gl.h"
#include "NL/gl/glConstant.h"
#include "NL/gl/glDraw3.h"
#include "NL/gl/glFont.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glView.h"
#include "NL/glx/glxTexture.h"
#include "Game/GL/gluMeshWriter.h"

#include "Game/Camera/CameraMan.h"
#include "Game/Character.h"
#include "Game/Effects/ParticleSystem.h"
#include "Game/Game.h"
#include "Game/GameInfo.h"
#include "Game/WorldManager.h"
#include "port/aspect.h"

static inline float GetAspectRatio()
{
    // PORT: the game's own widescreen request is not consulted, it asks for the anamorphic squeeze.
    bool bWide = false;
    u32 currState = nlTaskManager::m_pInstance->m_CurrState;
    if (currState == 0x100 || (currState == 1 && nlTaskManager::m_pInstance->m_PrevState == 0x100))
    {
        bWide = true;
    }
    if (bWide)
    {
        return 1.3323944f;
    }
    return 1.25f;
}

const u32 GLTT_BumpLocal_bit = 1 << (int)GLTT_BumpLocal;

static float dimx = 48.0f;
static float dimy = 28.0f;

eModelSkinMethod BeginFrameTask::s_GameplaySkin;
eModelSkinMethod BeginFrameTask::s_ReplaySkin;
bool BeginFrameTask::s_FramerateLocked;

static float offx;
static float offy;
static s32 g_eWaitMode;

bool g_bCoPlanarRefVisible;
bool g_bCoPlanarDepthTest;
bool g_bCoPlanarDepthWrite;
bool g_bFrameSmiler;
bool g_bFrameStatsOnScreen;

// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
bool g_bDrawSafeFrame;
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
s32 g_nGridDisplaySpacing;

/**
 * Offset/Address/Size: 0x19E8 | 0x801700C8 | size: 0x98
 */
static glModel* cb_ParticleLighting(glModel* pModel)
{
    glModelPacket* pPacket; // r27
    u32 LitProgram = glGetProgram("3d pointlit");
    u32 lightRamp = GetGameObjectLightRamp();
    void* pLightData = GetInGameLightData();
    pPacket = pModel->packets;

    while (pPacket < &pModel->packets[pModel->numPackets])
    {
        pPacket->state.program = LitProgram;
        pPacket->state.texture[5] = lightRamp;
        pPacket->state.texconfig |= GLTT_BumpLocal_bit;
        glUserAttach(pLightData, pPacket, 0);
        pPacket++;
    }
    return pModel;
}

/**
 * Offset/Address/Size: 0x14B8 | 0x8016FB98 | size: 0x530
 */
void SetupMatrices()
{
    nlMatrix4 ortho;
    // PORT: from the frame; a literal 640x480 stretches the 2D space to fit a wider frame.
    glMatrixOrthographicCentered(ortho, glGetOrthographicWidth(),
                                 glGetOrthographicHeight(), 0.0f, 5000.0f);
    uintptr_t hOrtho = glAllocMatrix();
    if (hOrtho != 0xFFFFFFFF)
    {
        glSetMatrix(hOrtho, ortho);
    }
    glViewSetProjectionMatrix(GLV_Anark, hOrtho);

    // PORT: see above. This one is top-left anchored rather than centred.
    glMatrixOrthographic(ortho, glGetOrthographicWidth(),
                         glGetOrthographicHeight());
    uintptr_t hFlat = glAllocMatrix();
    if (hFlat != 0xFFFFFFFF)
    {
        glSetMatrix(hFlat, ortho);
    }
    glViewSetProjectionMatrix(GLV_ShadowBlend0, hFlat);
    glViewSetProjectionMatrix(GLV_ShadowBlend1, hFlat);
    glViewSetProjectionMatrix(GLV_FrontEnd, hFlat);
    glViewSetProjectionMatrix(GLV_Debug, hFlat);
    glViewSetProjectionMatrix(GLV_Transitions, hFlat);
    glViewSetProjectionMatrix(GLV_WarbleBlend, hFlat);
    glViewSetProjectionMatrix(GLV_DepthOfField, hFlat);
    glViewSetProjectionMatrix(GLV_BigBlackPolygon, hFlat);
    glViewSetProjectionMatrix(GLV_UnsortedOrtho, hFlat);

    // PORT: the console's frustum ratio mapped onto the shape the port is presenting.
    float fAspect;
    float fFrontEndFOV;
    PortPerspective(0.4712389f, GetAspectRatio(), &fFrontEndFOV, &fAspect);

    nlMatrix4 proj;
    glMatrixPerspective(proj, fFrontEndFOV, fAspect, 0.25f, 1024.0f);
    uintptr_t hPersp = glAllocMatrix();
    if (hPersp != 0xFFFFFFFF)
    {
        glSetMatrix(hPersp, proj);
    }
    glViewSetProjectionMatrix(GLV_Anark3D_BG, hPersp);
    glViewSetProjectionMatrix(GLV_Anark3D_FG, hPersp);
    glViewSetProjectionMatrix(GLV_Transitions3D, hPersp);

    nlVector3 at = { 0.0f, 0.0f, 0.0f };
    nlVector3 to = { 0.0f, 0.0f, -1.0f };
    nlVector3 up = { 0.0f, 1.0f, 0.0f };
    nlMatrix4 view;
    glMatrixLookAt(view, at, to, up);
    uintptr_t hView1 = glAllocMatrix();
    if (hView1 != 0xFFFFFFFF)
    {
        glSetMatrix(hView1, view);
    }
    glViewSetViewMatrix(GLV_Transitions3D, hView1);

    nlVector3 at2 = { 0.0f, 12.0f, 0.0f };
    nlVector3 to2 = { 0.0f, 0.0f, -1.0f };
    nlVector3 up2 = { 0.0f, 0.0f, 1.0f };
    nlMatrix4 view2;
    glMatrixLookAt(view2, at2, to2, up2);
    uintptr_t hView2 = glAllocMatrix();
    if (hView2 != 0xFFFFFFFF)
    {
        glSetMatrix(hView2, view2);
    }
    glViewSetViewMatrix(GLV_Anark3D_BG, hView2);
    glViewSetViewMatrix(GLV_Anark3D_FG, hView2);

    float fFOV = 3.1415927f * cCameraManager::m_fFOV / 180.0f;
    // PORT: as above. The camera's fov is a horizontal one too.
    PortPerspective(fFOV, GetAspectRatio(), &fFOV, &fAspect);
    glMatrixPerspective(proj, fFOV, fAspect, 0.25f, 1024.0f);
    uintptr_t hCamProj = glAllocMatrix();
    if (hCamProj != 0xFFFFFFFF)
    {
        glSetMatrix(hCamProj, proj);
    }
    glViewSetProjectionMatrix(GLV_Skybox, hCamProj);
    glViewSetProjectionMatrix(GLV_CameraSpace, hCamProj);
    glViewSetProjectionMatrix(GLV_Shadowed, hCamProj);
    glViewSetProjectionMatrix(GLV_Shadow0, hCamProj);
    glViewSetProjectionMatrix(GLV_Shadow1, hCamProj);
    glViewSetProjectionMatrix(GLV_WorldShadowed, hCamProj);
    glViewSetProjectionMatrix(GLV_Characters, hCamProj);
    glViewSetProjectionMatrix(GLV_Unshadowed, hCamProj);
    glViewSetProjectionMatrix(GLV_LingeringParticles, hCamProj);
    glViewSetProjectionMatrix(GLV_Particles, hCamProj);
    glViewSetProjectionMatrix(GLV_Warble, hCamProj);
    glViewSetProjectionMatrix(GLV_UnsortedPerspective, hCamProj);
    glViewSetProjectionMatrix(GLV_InvisiblePlane, hCamProj);
    glViewSetProjectionMatrix(GLV_ElectricFence, hCamProj);
    glViewSetProjectionMatrix(GLV_CoPlanar0, hCamProj);
    glViewSetProjectionMatrix(GLV_CoPlanar, hCamProj);

    uintptr_t hCamView = glAllocMatrix();
    if (hCamView != 0xFFFFFFFF)
    {
        glSetMatrix(hCamView, cCameraManager::m_matView);
    }

    cBaseCamera* pCamera = nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack);
    if (pCamera != NULL)
    {
        pCamera = nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack);
        cCameraManager::m_pBeginFrameCameraType = pCamera->GetType();
    }

    glViewSetViewMatrix(GLV_Skybox, hCamView);
    glViewSetViewMatrix(GLV_Shadowed, hCamView);
    glViewSetViewMatrix(GLV_Shadow0, hCamView);
    glViewSetViewMatrix(GLV_Shadow1, hCamView);
    glViewSetViewMatrix(GLV_WorldShadowed, hCamView);
    glViewSetViewMatrix(GLV_Characters, hCamView);
    glViewSetViewMatrix(GLV_Unshadowed, hCamView);
    glViewSetViewMatrix(GLV_Warble, hCamView);
    glViewSetViewMatrix(GLV_LingeringParticles, hCamView);
    glViewSetViewMatrix(GLV_Particles, hCamView);
    glViewSetViewMatrix(GLV_UnsortedPerspective, hCamView);
    glViewSetViewMatrix(GLV_InvisiblePlane, hCamView);
    glViewSetViewMatrix(GLV_ElectricFence, hCamView);
    glViewSetViewMatrix(GLV_CoPlanar0, hCamView);
    glViewSetViewMatrix(GLV_CoPlanar, hCamView);

    glViewSetViewMatrix(GLV_CameraSpace, glGetIdentityMatrix());

    ParticleSystem::m_LightingCallback = cb_ParticleLighting;
}

/**
 * Offset/Address/Size: 0x1068 | 0x8016F748 | size: 0x450
 */
static void SetupRenderInfo()
{
    static bool bGotWait;
    static s8 init;

    s32 state;

    if (BeginFrameTask::s_GameplaySkin != 2)
    {
        cCharacter::m_ModelType = (eCharacterModelType)(BeginFrameTask::s_GameplaySkin != 0);
    }
    else
    {
        if (WorldManager::s_World != NULL)
        {
            state = nlTaskManager::m_pInstance->m_CurrState;
            if (state == 1)
            {
                state = nlTaskManager::m_pInstance->m_PrevState;
            }

            switch (state)
            {
            case 0x10:
            case 0x100:
                cCharacter::m_ModelType = CharModel_Blend;
                break;
            case 0x20000:
                if (BeginFrameTask::s_ReplaySkin == 0)
                {
                    cCharacter::m_ModelType = CharModel_Rigid;
                }
                else
                {
                    cCharacter::m_ModelType = CharModel_Blend;
                }
                break;
            default:
                cCharacter::m_ModelType = CharModel_Rigid;
                break;
            }
        }
        else
        {
            cCharacter::m_ModelType = CharModel_Rigid;
        }
    }

    if (g_pGame != NULL)
    {
        if (g_pGame->mbCaptainShotToScoreOn)
        {
            cCharacter::m_ModelType = CharModel_Blend;
        }
    }

    if (!init)
    {
        bGotWait = false;
        init = 1;
    }

    if (!bGotWait)
    {
        BasicString<char, Detail::TempStringAllocator> wait = Config::Global().Get<BasicString<char, Detail::TempStringAllocator> >(
            "wait_vblank", BasicString<char, Detail::TempStringAllocator>("default"));

        if (wait == "never")
        {
            g_eWaitMode = 1;
        }
        else if (wait == "always")
        {
            g_eWaitMode = 2;
        }
        else if (wait == "float" || wait == "floating")
        {
            g_eWaitMode = 3;
        }
        else
        {
            g_eWaitMode = 0;
        }

        bGotWait = true;
    }

    nlVector4 vwait;

    if (g_eWaitMode == 0 || g_eWaitMode == 3)
    {
        if (BeginFrameTask::s_FramerateLocked && nlTaskManager::m_pInstance->m_CurrState == 0x100)
        {
            vwait.x = 2.0f;
            vwait.y = 2.0f;
            vwait.z = 2.0f;
            vwait.w = 2.0f;
        }
        else
        {
            float vwaitValue;
            if (g_eWaitMode == 3)
            {
                vwaitValue = 0.5f;
            }
            else
            {
                vwaitValue = 1.0f;
            }

            switch (nlTaskManager::m_pInstance->m_CurrState)
            {
            case 0x10:
            case 4:
            case 0x100:
                vwait.x = vwaitValue;
                vwait.y = vwaitValue;
                vwait.z = vwaitValue;
                vwait.w = vwaitValue;
                break;
            default:
                vwait.x = vwaitValue;
                vwait.y = vwaitValue;
                vwait.z = vwaitValue;
                vwait.w = vwaitValue;
                break;
            }
        }
    }
    else if (g_eWaitMode == 1)
    {
        vwait.x = 0.0f;
        vwait.y = 0.0f;
        vwait.z = 0.0f;
        vwait.w = 0.0f;
    }
    else
    {
        vwait.x = 1.0f;
        vwait.y = 1.0f;
        vwait.z = 1.0f;
        vwait.w = 1.0f;
    }

    glConstantSet("glxswap/vwait", vwait);
}

static inline void meshTexcoord(GLMeshWriterCore& w, const nlVector2& tc)
{
    w.Texcoord(tc);
}

static inline void meshVertex(GLMeshWriterCore& w, float x, float y, float z)
{
    nlVector3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    w.Vertex(v);
}

struct LineStreams
{
    eGLStream e[3];
};

static inline LineStreams MakeLineStreams();

/**
 * Offset/Address/Size: 0x950 | 0x8016F030 | size: 0x718
 */
static void DrawSafeFrame()
{
    extern u32 glx_GetScaledXFBWidth();

    nlColour safeFrameColour = { 0x40, 0xFF, 0x40, 0xFF };
    int offset = (glx_GetScaledXFBWidth() - 640) / 2;
    int leftEdge = offset + 16;
    int rightEdge = 624 - offset;

    {
        GLMeshWriter mesh;
        LineStreams streams = MakeLineStreams();
        glSetDefaultState(false);
        glSetCurrentTexture(glGetTexture("global/white"), GLTT_Diffuse);
        glSetCurrentProgram(glGetProgram("2d unlit"));

        if (mesh.Begin(2, GLP_LineList, 3, streams.e, false))
        {
            nlVector2 tc;
            tc.x = 0.0f;
            tc.y = 0.0f;
            meshTexcoord(mesh, tc);
            mesh.Colour(safeFrameColour);

            meshVertex(mesh, (float)leftEdge, 30.0f, 0.0f);

            nlVector2 tc2;
            tc2.x = 0.0f;
            tc2.y = 0.0f;
            meshTexcoord(mesh, tc2);
            mesh.Colour(safeFrameColour);

            meshVertex(mesh, (float)rightEdge, 30.0f, 0.0f);

            if (mesh.End())
            {
                glViewAttachModel(GLV_Debug, 6, mesh.GetModel());
            }
        }
    }

    {
        GLMeshWriter mesh;
        LineStreams streams = MakeLineStreams();
        glSetDefaultState(false);
        glSetCurrentTexture(glGetTexture("global/white"), GLTT_Diffuse);
        glSetCurrentProgram(glGetProgram("2d unlit"));

        if (mesh.Begin(2, GLP_LineList, 3, streams.e, false))
        {
            nlVector2 tc;
            tc.x = 0.0f;
            tc.y = 0.0f;
            meshTexcoord(mesh, tc);
            mesh.Colour(safeFrameColour);

            meshVertex(mesh, (float)leftEdge, 450.0f, 0.0f);

            nlVector2 tc2;
            tc2.x = 0.0f;
            tc2.y = 0.0f;
            meshTexcoord(mesh, tc2);
            mesh.Colour(safeFrameColour);

            meshVertex(mesh, (float)rightEdge, 450.0f, 0.0f);

            if (mesh.End())
            {
                glViewAttachModel(GLV_Debug, 6, mesh.GetModel());
            }
        }
    }

    {
        GLMeshWriter mesh;
        LineStreams streams = MakeLineStreams();
        glSetDefaultState(false);
        glSetCurrentTexture(glGetTexture("global/white"), GLTT_Diffuse);
        glSetCurrentProgram(glGetProgram("2d unlit"));

        if (mesh.Begin(2, GLP_LineList, 3, streams.e, false))
        {
            nlVector2 tc;
            tc.x = 0.0f;
            tc.y = 0.0f;
            meshTexcoord(mesh, tc);
            mesh.Colour(safeFrameColour);

            meshVertex(mesh, (float)leftEdge, 30.0f, 0.0f);

            nlVector2 tc2;
            tc2.x = 0.0f;
            tc2.y = 0.0f;
            meshTexcoord(mesh, tc2);
            mesh.Colour(safeFrameColour);

            meshVertex(mesh, (float)leftEdge, 450.0f, 0.0f);

            if (mesh.End())
            {
                glViewAttachModel(GLV_Debug, 6, mesh.GetModel());
            }
        }
    }

    {
        GLMeshWriter mesh;
        LineStreams streams = MakeLineStreams();
        glSetDefaultState(false);
        glSetCurrentTexture(glGetTexture("global/white"), GLTT_Diffuse);
        glSetCurrentProgram(glGetProgram("2d unlit"));

        if (mesh.Begin(2, GLP_LineList, 3, streams.e, false))
        {
            nlVector2 tc;
            tc.x = 0.0f;
            tc.y = 0.0f;
            meshTexcoord(mesh, tc);
            mesh.Colour(safeFrameColour);

            meshVertex(mesh, (float)rightEdge, 30.0f, 0.0f);

            nlVector2 tc2;
            tc2.x = 0.0f;
            tc2.y = 0.0f;
            meshTexcoord(mesh, tc2);
            mesh.Colour(safeFrameColour);

            meshVertex(mesh, (float)rightEdge, 450.0f, 0.0f);

            if (mesh.End())
            {
                glViewAttachModel(GLV_Debug, 6, mesh.GetModel());
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x410 | 0x8016EAF0 | size: 0x540
 */
static void DrawGrid(int spacing)
{
    nlColour gridColour = { 0x40, 0x40, 0xFF, 0xFF };
    nlColour centerColour = { 0xFF, 0x40, 0x40, 0xFF };

    for (int y = 0; y < 480; y += spacing)
    {
        GLMeshWriter mesh;
        LineStreams streams = MakeLineStreams();
        glSetDefaultState(false);
        glSetCurrentTexture(glGetTexture("global/white"), GLTT_Diffuse);
        glSetCurrentProgram(glGetProgram("2d unlit"));

        if (mesh.Begin(2, GLP_LineList, 3, streams.e, false))
        {
            nlVector2 tc;
            tc.x = 0.0f;
            tc.y = 0.0f;
            meshTexcoord(mesh, tc);
            mesh.Colour(gridColour);
            meshVertex(mesh, 0.0f, (float)y, 0.0f);

            nlVector2 tc2;
            tc2.x = 0.0f;
            tc2.y = 0.0f;
            meshTexcoord(mesh, tc2);
            mesh.Colour(gridColour);
            meshVertex(mesh, 640.0f, (float)y, 0.0f);

            if (mesh.End())
            {
                glViewAttachModel(GLV_Debug, 5, mesh.GetModel());
            }
        }
    }

    for (int x = 0; x < 640; x += spacing)
    {
        GLMeshWriter mesh;
        LineStreams streams = MakeLineStreams();
        glSetDefaultState(false);
        glSetCurrentTexture(glGetTexture("global/white"), GLTT_Diffuse);
        glSetCurrentProgram(glGetProgram("2d unlit"));

        if (mesh.Begin(2, GLP_LineList, 3, streams.e, false))
        {
            nlVector2 tc;
            tc.x = 0.0f;
            tc.y = 0.0f;
            meshTexcoord(mesh, tc);
            mesh.Colour(gridColour);
            meshVertex(mesh, (float)x, 0.0f, 0.0f);

            nlVector2 tc2;
            tc2.x = 0.0f;
            tc2.y = 0.0f;
            meshTexcoord(mesh, tc2);
            mesh.Colour(gridColour);
            meshVertex(mesh, (float)x, 480.0f, 0.0f);

            if (mesh.End())
            {
                glViewAttachModel(GLV_Debug, 5, mesh.GetModel());
            }
        }
    }

    {
        GLMeshWriter mesh;
        LineStreams streams = MakeLineStreams();
        glSetDefaultState(false);
        glSetCurrentTexture(glGetTexture("global/white"), GLTT_Diffuse);
        glSetCurrentProgram(glGetProgram("2d unlit"));

        if (mesh.Begin(2, GLP_LineList, 3, streams.e, false))
        {
            nlVector2 tc;
            tc.x = 0.0f;
            tc.y = 0.0f;
            meshTexcoord(mesh, tc);
            mesh.Colour(centerColour);
            meshVertex(mesh, 320.0f, 0.0f, 0.0f);

            nlVector2 tc2;
            tc2.x = 0.0f;
            tc2.y = 0.0f;
            meshTexcoord(mesh, tc2);
            mesh.Colour(centerColour);
            meshVertex(mesh, 320.0f, 480.0f, 0.0f);

            if (mesh.End())
            {
                glViewAttachModel(GLV_Debug, 5, mesh.GetModel());
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8016E6E0 | size: 0x410
 */
void BeginFrameTask::Run(float dt)
{
    extern bool g_bCoPlanarPerObject;

    g_FrameCounter.StartTimer(0);

    if (g_bFrameSmiler)
    {
        g_FrameCounter.DisplayFrameSmiler();
    }

    NisPlayer::Instance()->Update(dt);
    NisPlayer::Instance()->Render();
    cCameraManager::Update(dt);

    if ((nlTaskManager::m_pInstance->m_CurrState & 0x110) != 0)
    {
        DepthOfFieldManager::instance.TurnOn();
    }
    else
    {
        DepthOfFieldManager::instance.TurnOff();
    }

    glBeginFrame();
    SetupMatrices();
    SetupRenderInfo();
    nlServiceFileSystem();

    if (!g_bCoPlanarPerObject)
    {
        static u32 BlackTexture;
        static s8 init;

        if (!init)
        {
            BlackTexture = glGetTexture("global/black");
            init = 1;
        }

        float z = GetCoPlanarZ();
        float dx = dimx;
        float dy = dimy;
        float nx = -dx;
        float ox = offx;
        float oy = offy;
        float ny = -dy;

        float right = dx + ox;
        float left = nx + ox;
        float bottom = ny + oy;
        float top = dy + oy;

        nlVector3 points[4] = { 0 };

        points[0].x = left;
        points[0].y = bottom;
        points[0].z = z;
        points[1].x = right;
        points[1].y = bottom;
        points[1].z = z;
        points[2].x = right;
        points[2].y = top;
        points[2].z = z;
        points[3].x = left;
        points[3].y = top;
        points[3].z = z;

        glSetDefaultState(false);
        glSetRasterState(GLS_Culling, g_bCoPlanarRefVisible ? 1 : 3);
        glSetRasterState(GLS_DepthTest, g_bCoPlanarDepthTest);
        glSetRasterState(GLS_DepthWrite, g_bCoPlanarDepthWrite);
        glSetCurrentRasterState(glHandleizeRasterState());
        glSetCurrentTexture(BlackTexture, GLTT_Diffuse);

        glQuad3 q;
        q.m_pos[0] = points[0];
        q.m_uv[0].x = 0.0f;
        q.m_uv[0].y = 0.0f;
        q.m_pos[1] = points[1];
        q.m_uv[1].x = 0.0f;
        q.m_uv[1].y = 0.0f;
        q.m_pos[2] = points[2];
        q.m_uv[2].x = 0.0f;
        q.m_uv[2].y = 0.0f;
        q.m_pos[3] = points[3];
        q.m_uv[3].x = 0.0f;
        q.m_uv[3].y = 0.0f;

        q.SetColour(0xFF, 0xFF, 0xFF, 0xFF);
        q.Attach(GLV_CoPlanar, 0, true);

        z = GetCoPlanar0Z();
        q.m_pos[0].z = z;
        q.m_pos[1].z = z;
        q.m_pos[2].z = z;
        q.m_pos[3].z = z;
        q.Attach(GLV_CoPlanar0, 0, true);
    }

    switch (nlTaskManager::m_pInstance->m_CurrState)
    {
    case 0x10:
        ParticleSystem::m_AllowInFront = 0;
        break;
    default:
        ParticleSystem::m_AllowInFront = 1;
        break;
    }

    if (g_bDrawSafeFrame)
    {
        DrawSafeFrame();
    }

    if (g_nGridDisplaySpacing > 0)
    {
        DrawGrid(g_nGridDisplaySpacing);
    }

    static bool showRegion;
    static s8 init;

    if (!init)
    {
        Config& cfg = Config::Global();
        TagValuePair& tvp = cfg.FindTvp("show_region");

        bool regionVisible;
        if (tvp.tag == 0)
        {
            cfg.Set("show_region", false);
            regionVisible = false;
        }
        else if (tvp.type == _BOOL)
        {
            regionVisible = LexicalCast<bool, bool>(tvp.value.b);
        }
        else if (tvp.type == _INT)
        {
            regionVisible = LexicalCast<bool, int>(tvp.value.i);
        }
        else if (tvp.type == _FLOAT)
        {
            regionVisible = LexicalCast<bool, float>(tvp.value.f);
        }
        else if (tvp.type == _STRING)
        {
            regionVisible = LexicalCast<bool, const char*>(tvp.value.s);
        }
        else
        {
            regionVisible = false;
        }

        showRegion = regionVisible;
        init = 1;
    }

    if (showRegion)
    {
        glFontBegin(false);
        nlColour white = { 0xFF, 0xFF, 0xFF, 0xFF };
        glFontPrintf((eGLView)0x21, 1, 1, white, "Region %d", *GetRegion());
        glFontEnd();
    }
}

static inline LineStreams MakeLineStreams()
{
    LineStreams s = { GLStream_Position, GLStream_Colour, GLStream_Diffuse };
    return s;
}
