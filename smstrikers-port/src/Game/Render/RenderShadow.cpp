#include "Game/Render/RenderShadow.h"

#include "math.h"

#include "Game/Debug/ShapeRender.h"
#include "Game/Drawable/DrawableModel.h"
#include "Game/GL/GLInventory.h"
#include "Game/WorldManager.h"
#include "NL/gl/glAppAttach.h"
#include "NL/gl/glDraw3.h"
#include "NL/nlString.h"

const unsigned long UnlitProgram = glGetProgram("3d unlit");
const unsigned long LitProgram = glGetProgram("3d pointlit");
const unsigned long LightTexture = glGetTexture("global/lightramp");
const unsigned long BlackTexture = glGetTexture("global/black");
const unsigned long WhiteTexture = glGetTexture("global/white");
int MaxProjectedShadows;

extern GLInventory glInventory;

// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
u8 g_bPreview;
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
u8 g_bShadowBlobs;
static u8 g_bSubdivideShadow;
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
u8 g_bShadowBounds;
static u8 g_bShadowPositionOverride;
static u8 g_bCoPlanarProjectedShadows;

const unsigned long FourTexture = glGetTexture("global/four");

static glModel* pCylinder;
static glModel* pBox;

static bool g_bProjectDirectional = true;
static bool g_bShadowDirectional = true;
static int g_AlphaRef = 0x00000020;
static float g_fProjectionAdjust = 0.125f;
static eGLView g_CharacterShadowView = GLV_WorldShadowed;
static float g_AntiFlimmer = 0.015625f;

static nlVector3 g_vShadowPosition = { 0.0f, 17.0f, 13.0f };
static int g_Alpha[3] = { 180, 80, 32 };

/**
 * Offset/Address/Size: 0x16B4 | 0x801246E8 | size: 0x80
 */
void ShadowStartup()
{
    pCylinder = glInventory.GetModel(nlStringHash("debug/cylinder"));
    pBox = glInventory.GetModel(nlStringHash("debug/box"));
    const nlVector4& result = glConstantGet("target/pshadow_num");
    MaxProjectedShadows = (int)result.x;
}

/**
 * Offset/Address/Size: 0x1558 | 0x8012458C | size: 0x15C
 */
void RenderShadowModel(unsigned long flags, glModel* model, uintptr_t matrix)
{
    unsigned long fourTex;
    eGLView view = GLV_Shadow1;
    glModelPacket* normalPkt;
    glModelPacket* normalDup;
    if (flags & 1)
        view = GLV_Shadow0;

    if (matrix == 0xFFFFFFFF)
        matrix = glGetIdentityMatrix();

    if (g_bPreview)
    {
        glModelPacket* pkt = model->packets;
        while (pkt < model->packets + model->numPackets)
        {
            glModelPacket* dup = glModelPacketDup(pkt, true);
            dup->state.texture[0] = (uintptr_t)ResolvedWhiteTexture;
            dup->state.matrix = matrix;
            glViewAttachPacket(GLV_Unshadowed, dup);
            pkt++;
        }
    }
    else
    {
        s32 pass = 0;
        fourTex = FourTexture;
        do
        {
            normalPkt = model->packets;
            while (normalPkt < model->packets + model->numPackets)
            {
                normalDup = glModelPacketDup(normalPkt, true);
                normalDup->state.texture[0] = fourTex;
                normalDup->state.matrix = matrix;
                glUnHandleizeRasterState(normalDup->state.raster);

                glSetRasterState(GLS_Culling, pass == 0 ? GX_CULL_BACK : GX_CULL_FRONT);
                glSetRasterState(GLS_DepthWrite, 0);

                glSetRasterState(GLS_AlphaBlend, (pass == 0) ? 2 : 7);
                glSetRasterState(GLS_ColourWrite, 2);

                normalDup->state.raster = glHandleizeRasterState();
                glViewAttachPacket(view, normalDup);
                normalPkt++;
            }
            pass++;
        } while (pass < 2);
    }
}

/**
 * Offset/Address/Size: 0x14DC | 0x80124510 | size: 0x7C
 */
int GetShadowPartitionIndex()
{
    static int index = 0;
    static unsigned long prevFrame = 0;

    unsigned long frame = glGetCurrentFrame();
    if (prevFrame != frame)
    {
        prevFrame = frame;
        index = 0;
    }

    int rval = index;
    index++;
    return rval;
}

/**
 * Offset/Address/Size: 0x140C | 0x80124440 | size: 0xD0
 */
u8 ShouldShadowBeUpdated(const ProjectedShadowParams& params)
{
    float fRadius = 2.0f * params.fRadius;
    nlMatrix4 mWorld;
    mWorld.SetIdentity();
    mWorld.m41 = params.vPosition.x;
    mWorld.m42 = params.vPosition.y;
    mWorld.m43 = params.vPosition.z;
    mWorld.m44 = 1.0f;
    mWorld.m43 += 0.625f * params.fHeight;

    u8 isVisible = WorldManager::s_World->IsSphereInFrustum(mWorld, fRadius);
    u32 interval;
    if (isVisible)
    {
        interval = params.nVisibleInterval;
    }
    else
    {
        interval = params.nInvisibleInterval;
    }

    u32 currentFrame = (u32)glGetCurrentFrame();
    u32 frame = (u32)params.nPartitionIndex + currentFrame;
    if (frame % interval != 0)
    {
        return 0;
    }
    return 1;
}

/**
 * Offset/Address/Size: 0xF9C | 0x80123FD0 | size: 0x470
 */
void RenderCharacterIntoTexture(const ProjectedShadowParams& params)
{
    nlVector3 targetPos;
    nlVector3 eyePos;
    nlVector3 shadowPos;
    nlVector3 up = { 0.0f, 0.0f, 1.0f };

    static float s_fFarPlane = 8.0f;
    static float s_fLightDist = -0.125f;

    if (g_bShadowBlobs)
    {
        return;
    }

    float radius = params.fRadius;
    if (g_bShadowPositionOverride)
    {
        shadowPos = g_vShadowPosition;
    }
    else
    {
        shadowPos.x = params.vLight.x;
        shadowPos.y = params.vLight.y;
        shadowPos.z = params.vLight.z;
    }

    targetPos = params.vPosition;

    if (g_bProjectDirectional)
    {
        float lx;
        float ly;
        float lz;
        lz = -shadowPos.z;
        ly = -shadowPos.y;
        lx = -shadowPos.x;
        float lenSq = lx * lx + ly * ly + lz * lz;
        float len = nlSqrt(lenSq, true);
        float invLen = nlRecipSqrt(lenSq, false);

        float scale = len * (s_fLightDist * radius);
        eyePos.x = targetPos.x + scale * (invLen * lx);
        eyePos.y = targetPos.y + scale * (invLen * ly);
        eyePos.z = targetPos.z + scale * (invLen * lz);
    }
    else
    {
        nlVec3Sub(eyePos, targetPos, shadowPos);
        nlVec3ScaleAdd(eyePos, s_fLightDist * radius, eyePos, targetPos);
    }

    float eyeDistance = nlSqrt(nlGetLengthSquared3D(
                                   targetPos.x - eyePos.x,
                                   targetPos.y - eyePos.y,
                                   targetPos.z - eyePos.z),
        true);

    float ratio = radius / eyeDistance;
    float nearPlane = eyeDistance - radius;
    float farPlane = nearPlane + s_fFarPlane;

    float fovY;
    if ((float)fabs(ratio) < 0.01f)
    {
        fovY = 1.0f;
    }
    else
    {
        u16 angle = (u16)(0x4000 - nlACos(ratio));
        fovY = 2.0f * (9.58738e-5f * (float)angle);
    }

    nlMatrix4 view;
    nlMatrix4 projection;

    glMatrixPerspective(projection, fovY, 1.0f, nearPlane, farPlane);
    glMatrixLookAt(view, eyePos, targetPos, up);

    uintptr_t viewMatrix = glAllocMatrix();
    if (viewMatrix != 0xFFFFFFFF)
    {
        glSetMatrix(viewMatrix, view);
    }

    uintptr_t projectionMatrix = glAllocMatrix();
    if (projectionMatrix != 0xFFFFFFFF)
    {
        glSetMatrix(projectionMatrix, projection);
    }

    void* userData = glUserAlloc(GLUD_Viewport, sizeof(GLViewportUserData), false);
    GLViewportUserData* pViewportData = (GLViewportUserData*)glUserGetData(userData);
    pViewportData->view = viewMatrix;
    pViewportData->projection = projectionMatrix;
    pViewportData->x = (s16)((params.nPartitionIndex % 4) * 0xA0);
    pViewportData->y = (s16)((params.nPartitionIndex / 4) * 0x94);
    pViewportData->w = 0xA0;
    pViewportData->h = 0x94;

    glModel* pModel = glModelDup(params.pModel, true);
    glModelPacket* pPacket = pModel->packets;

    while (pPacket < pModel->packets + pModel->numPackets)
    {
        if (glUserHasType(GLUD_Light, pPacket))
        {
            glUserDetach(GLUD_Light, pPacket);
        }

        if (userData)
        {
            glUserAttach(userData, pPacket, false);
        }

        pPacket->state.texture[0] = (uintptr_t)ResolvedBlackTexture;
        glSetTextureState(pPacket->state.texturestate, (eGLTextureState)0xC, 0x3F);

        pPacket++;
    }

    glViewAttachModel(GLV_ShadowTexture, pModel);

    char buffer[32];
    nlSNPrintf(buffer, 32, "target/pshadow_updated%02d", params.nPartitionIndex);

    nlVector4 v;
    v.x = 1.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    v.w = 0.0f;
    glConstantSet(buffer, v);
}

/**
 * Offset/Address/Size: 0xF24 | 0x80123F58 | size: 0x78
 */
void SetCharacterShadowUpdated(int index, bool updated)
{
    char buffer[32];
    nlSNPrintf(buffer, 32, "target/pshadow_updated%02d", index);

    nlVector4 v;
    v.x = updated ? 1.0f : 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    v.w = 0.0f;

    glConstantSet(buffer, v);
}

/**
 * Offset/Address/Size: 0xB44 | 0x80123B78 | size: 0x3E0
 */
static void SubdivideAndRender(glQuad3& quad, eGLView view)
{
    nlVector3 p0;
    nlVector3 p1;
    nlVector2 uv0;
    nlVector2 uv1;
    nlColour c;
    glQuad3 q;
    glModelPacket* pPacket;

    p0.x = 0.5f * quad.m_pos[0].x + 0.5f * quad.m_pos[3].x;
    p0.y = 0.5f * quad.m_pos[0].y + 0.5f * quad.m_pos[3].y;
    p0.z = 0.5f * quad.m_pos[0].z + 0.5f * quad.m_pos[3].z;

    p1.x = 0.5f * quad.m_pos[1].x + 0.5f * quad.m_pos[2].x;
    p1.y = 0.5f * quad.m_pos[1].y + 0.5f * quad.m_pos[2].y;
    p1.z = 0.5f * quad.m_pos[1].z + 0.5f * quad.m_pos[2].z;

    uv0.x = 0.5f * quad.m_uv[0].x + 0.5f * quad.m_uv[3].x;
    uv0.y = 0.5f * quad.m_uv[0].y + 0.5f * quad.m_uv[3].y;

    uv1.x = 0.5f * quad.m_uv[1].x + 0.5f * quad.m_uv[2].x;
    uv1.y = 0.5f * quad.m_uv[1].y + 0.5f * quad.m_uv[2].y;

    c = quad.m_colour[0];
    c.c[3] = (unsigned char)g_Alpha[1];

    // First sub-quad (left half)
    q.m_pos[0] = quad.m_pos[0];
    q.m_pos[1] = quad.m_pos[1];
    q.m_pos[2] = p1;
    q.m_pos[3] = p0;
    q.m_uv[0] = quad.m_uv[0];
    q.m_uv[1] = quad.m_uv[1];
    q.m_uv[2] = uv1;
    q.m_uv[3] = uv0;
    q.m_colour[1] = quad.m_colour[0];
    q.m_colour[0] = quad.m_colour[0];
    q.m_colour[3] = c;
    q.m_colour[2] = c;

    if (g_bCoPlanarProjectedShadows)
    {
        const glModel* pModel = q.GetModel(true);
        pPacket = pModel->packets;
        glSetRasterState(pPacket->state.raster, GLS_AlphaTest, 1);
        glSetRasterState(pPacket->state.raster, GLS_AlphaTestRef, g_AlphaRef);
        glSetRasterState(pPacket->state.raster, GLS_DepthFunc, 3);
        glSetRasterState(pPacket->state.raster, GLS_DepthTest, 1);
        glSetRasterState(pPacket->state.raster, GLS_DepthWrite, 1);
        glUserAttach(glAppGetCoPlanarUserData(), pModel->packets, false);
        glViewAttachModel(GLV_CoPlanar0, 1, pModel);
    }
    else
    {
        q.Attach(view, 0, true);
    }

    // Second sub-quad (right half)
    q.m_pos[0] = p0;
    q.m_pos[1] = p1;
    q.m_pos[2] = quad.m_pos[2];
    q.m_pos[3] = quad.m_pos[3];
    q.m_uv[0] = uv0;
    q.m_uv[1] = uv1;
    q.m_uv[2] = quad.m_uv[2];
    q.m_uv[3] = quad.m_uv[3];
    q.m_colour[1] = c;
    q.m_colour[0] = c;
    q.m_colour[3] = quad.m_colour[2];
    q.m_colour[2] = quad.m_colour[2];

    if (g_bCoPlanarProjectedShadows)
    {
        const glModel* pModel = q.GetModel(true);
        pPacket = pModel->packets;
        glSetRasterState(pPacket->state.raster, GLS_AlphaTest, 1);
        glSetRasterState(pPacket->state.raster, GLS_AlphaTestRef, g_AlphaRef);
        glSetRasterState(pPacket->state.raster, GLS_DepthFunc, 3);
        glSetRasterState(pPacket->state.raster, GLS_DepthTest, 1);
        glSetRasterState(pPacket->state.raster, GLS_DepthWrite, 1);
        glUserAttach(glAppGetCoPlanarUserData(), pModel->packets, false);
        glViewAttachModel(GLV_CoPlanar0, 1, pModel);
    }
    else
    {
        q.Attach(view, 0, true);
    }
}

/**
 * Offset/Address/Size: 0x750 | 0x80123784 | size: 0x3F4
 */
static void RenderBlobShadow(const nlVector3& vPosition, const nlVector3* pPoints, int index, const int* uvOrder, const nlColour* pColour)
{
    static float half_w = 0.625f;
    static float half_h = 0.625f;
    static int alpha = 0x80;

    glQuad3 quad;
    nlColour c;
    nlColour cfade;
    uintptr_t texture;

    float posX, posY, antiFlimmer;
    antiFlimmer = g_AntiFlimmer;
    posY = vPosition.y;
    posX = vPosition.x;

    if (pColour == NULL)
    {
        nlColourSet(c, 0xFF, 0xFF, 0xFF, (u8)alpha);
        cfade = c;
    }
    else
    {
        cfade = *pColour;
        c = *pColour;
        cfade.c[3] = (u8)g_Alpha[2];
    }

    if (pPoints == NULL)
    {
        texture = glGetTexture("global/shadeblob");
        float hw = half_w;
        float hh = half_h;

        quad.m_pos[0].x = posX - hw;
        quad.m_pos[0].y = posY - hh;
        quad.m_pos[0].z = antiFlimmer;
        quad.m_pos[1].x = posX - hw;
        quad.m_pos[1].y = posY + hh;
        quad.m_pos[1].z = antiFlimmer;
        quad.m_pos[2].x = posX + hw;
        quad.m_pos[2].y = posY + hh;
        quad.m_pos[2].z = antiFlimmer;
        quad.m_pos[3].x = posX + hw;
        quad.m_pos[3].y = posY - hh;
        quad.m_pos[3].z = antiFlimmer;

        quad.m_uv[0].x = 1.0f;
        quad.m_uv[0].y = 1.0f;
        quad.m_uv[1].x = 0.0f;
        quad.m_uv[1].y = 1.0f;
        quad.m_uv[2].x = 0.0f;
        quad.m_uv[2].y = 0.0f;
        quad.m_uv[3].x = 1.0f;
        quad.m_uv[3].y = 0.0f;
    }
    else
    {
        char texturename[32] = "target/pshadow00";
        texturename[14] = '0' + (index / 10);
        texturename[15] = '0' + (index % 10);
        texture = glGetTexture(texturename);

        quad.m_pos[0] = pPoints[0];
        quad.m_pos[1] = pPoints[1];
        quad.m_pos[2] = pPoints[2];
        quad.m_pos[3] = pPoints[3];

        int idx;
        nlVector2* pUV;

        if (uvOrder == NULL)
        {
            idx = 0;
        }
        else
        {
            idx = uvOrder[0];
        }
        pUV = &quad.m_uv[idx];
        pUV->x = 0.75f;
        pUV->y = 0.666f;

        if (uvOrder == NULL)
        {
            idx = 1;
        }
        else
        {
            idx = uvOrder[1];
        }
        pUV = &quad.m_uv[idx];
        pUV->x = 0.25f;
        pUV->y = 0.666f;

        if (uvOrder == NULL)
        {
            idx = 2;
        }
        else
        {
            idx = uvOrder[2];
        }
        pUV = &quad.m_uv[idx];
        pUV->x = 0.25f;
        pUV->y = 0.125f;

        if (uvOrder == NULL)
        {
            idx = 3;
        }
        else
        {
            idx = uvOrder[3];
        }
        pUV = &quad.m_uv[idx];
        pUV->x = 0.75f;
        pUV->y = 0.125f;
    }

    quad.m_colour[1] = c;
    quad.m_colour[0] = c;
    quad.m_colour[3] = cfade;
    quad.m_colour[2] = cfade;

    glSetDefaultState(true);
    glSetRasterState(GLS_AlphaBlend, 1);
    glSetRasterState(GLS_Culling, 0);
    glSetRasterState(GLS_DepthWrite, 0);
    glSetCurrentRasterState(glHandleizeRasterState());
    glSetCurrentTexture(texture, GLTT_Diffuse);
    glSetTextureState(GLTS_DiffuseWrap, 3);
    glSetCurrentTextureState(glHandleizeTextureState());

    if (pPoints == NULL || !g_bSubdivideShadow)
    {
        quad.Attach(g_CharacterShadowView, 0, true);
    }
    else
    {
        SubdivideAndRender(quad, g_CharacterShadowView);
    }
}

static inline void CastDirectional(nlVector3& p, const nlVector3& lightPos)
{
    float lz = -lightPos.z;
    float ly = -lightPos.y;
    float lx = -lightPos.x;
    float pz = p.z;
    float py = p.y;
    float px = p.x;
    nlVector3 vDir;
    nlVec3Set(vDir, lx, ly, lz);
    nlVec3Scale(vDir, nlRecipSqrt(vDir.GetLengthSq3D(), false));
    float dirX = vDir.x;
    float dirY = vDir.y;
    float dirZ = vDir.z;

    float Vx = 0.0f;
    float Vy = 0.0f;
    float Vz = 1.0f;
    float num = Vx * px + Vy * py + Vz * pz;
    float den = Vx * dirX + Vy * dirY + Vz * dirZ;
    float t = -(num / den);

    p.x = px + t * dirX;
    p.y = py + t * dirY;
    p.z = pz + t * dirZ;
}

static inline void CastPoint(nlVector3& p, const nlVector3& vLight)
{
    nlVector4 V = { 0.0f, 0.0f, 1.0f, 0.0f };
    nlVector4 Q = { 0.0f, 0.0f, 0.0f, 1.0f };
    nlVector4 L = { 0.0f, 0.0f, 0.0f, 0.0f };

    Q.x = p.x;
    Q.y = p.y;
    Q.z = p.z;

    L.x = Q.x - vLight.x;
    L.y = Q.y - vLight.y;
    L.z = Q.z - vLight.z;

    float t = -((V.x * Q.x + V.y * Q.y + V.z * Q.z + V.w * Q.w)
                / (V.x * L.x + V.y * L.y + V.z * L.z + V.w * L.w));

    p.x = Q.x + t * L.x;
    p.y = Q.y + t * L.y;
    p.z = Q.z + t * L.z;
}

/**
 * Offset/Address/Size: 0x0 | 0x80123034 | size: 0x750
 */
void RenderProjectedShadow(const ProjectedShadowParams& params)
{
    nlVector3 vTemp;
    const nlVector3 vUp = { 0.0f, 0.0f, 1.0f };
    nlVector3 p[4];
    nlVector3 vLight;
    float width;
    float height;
    nlColour c;
    nlVector3 dir;

    if (g_bShadowBlobs)
    {
        RenderBlobShadow(params.vPosition, NULL, -1, NULL, NULL);
        return;
    }

    height = params.fScalar * params.fHeight;
    width = params.fScalar * params.fWidth;

    if (g_bShadowPositionOverride)
    {
        vLight = g_vShadowPosition;
    }
    else
    {
        float y;
        float z;
        float x;
        y = params.vLight.y;
        z = params.vLight.z;
        x = params.vLight.x;
        nlVec3Set(vLight, x, y, z);
    }

    vTemp = params.vPosition;

    {
        nlVector3 vDir;
        vDir.Sub2D(vTemp, vLight);
        vDir.z = 0.0f;
        vTemp.z += 0.5f * height;

        nlVec3Scale(vDir, nlRecipSqrt(vDir.GetLengthSq3D(), false));
        nlVec3CrossProduct(vDir, vDir, vUp);

        vTemp.as_u32[0] = params.vPosition.as_u32[0];
        vTemp.as_u32[1] = params.vPosition.as_u32[1];
        vTemp.as_u32[2] = params.vPosition.as_u32[2];

        nlVec3ScaleAdd(p[0], 0.5f * width, vDir, vTemp);
        nlVec3ScaleAdd(p[1], -0.5f * width, vDir, vTemp);
        nlVec3ScaleAdd(p[0], -0.5f * height, vUp, p[0]);
        nlVec3ScaleAdd(p[1], -0.5f * height, vUp, p[1]);
        float halfH = 0.5f * height;
        vTemp.z += halfH;
        nlVec3ScaleAdd(p[2], -0.5f * width, vDir, vTemp);
        nlVec3ScaleAdd(p[3], 0.5f * width, vDir, vTemp);
        nlVec3ScaleAdd(p[2], 0.5f * height, vUp, p[2]);
        nlVec3ScaleAdd(p[3], 0.5f * height, vUp, p[3]);
    }

    nlVector3* p1 = &p[1];
    nlVector3* p2 = &p[2];
    nlVector3* p3 = &p[3];

    if (g_bShadowBounds)
    {
        nlColourSet(c, 0x40, 0x40, 0xFF, 0xFF);
        g_ShapeRenderer.DrawLine3D(p[0], *p1, c, false);
        g_ShapeRenderer.DrawLine3D(*p1, *p2, c, false);
        g_ShapeRenderer.DrawLine3D(*p2, *p3, c, false);
        g_ShapeRenderer.DrawLine3D(*p3, p[0], c, false);
    }

    {
        nlVector3* pPoint = p;
        for (int i = 0; i < 4; i++, pPoint++)
        {
            if (g_bShadowDirectional)
            {
                CastDirectional(*pPoint, vLight);
            }
            else
            {
                CastPoint(*pPoint, vLight);
            }

            pPoint->z = g_AntiFlimmer;
        }
    }

    dir = vLight;
    dir.z = 0.0f;

    {
        nlVec3Scale(dir, nlRecipSqrt(dir.GetLengthSq3D(), false));
    }

    {
        float xAdjust = g_fProjectionAdjust * dir.x;
        float yAdjust = g_fProjectionAdjust * dir.y;

        p[0].x += xAdjust;
        p[0].y += yAdjust;
        p[1].x += xAdjust;
        p[1].y += yAdjust;
        p[2].x += xAdjust;
        p[2].y += yAdjust;
        p[3].x += xAdjust;
        p[3].y += yAdjust;
    }

    if (g_bShadowBounds)
    {
        nlColourSet(c, 0x40, 0xFF, 0x40, 0xFF);
        g_ShapeRenderer.DrawLine3D(p[0], *p1, c, false);
        g_ShapeRenderer.DrawLine3D(*p1, *p2, c, false);
        g_ShapeRenderer.DrawLine3D(*p2, *p3, c, false);
        g_ShapeRenderer.DrawLine3D(*p3, p[0], c, false);
    }

    {
        float newAntiFlimmer = GetCoPlanarZ();
        nlColour colour = { 0, 0, 0, 0 };
        float oldAntiFlimmer = g_AntiFlimmer;
        g_AntiFlimmer = newAntiFlimmer;

        colour.c[3] = (u8)g_Alpha[0];

        RenderBlobShadow(params.vPosition, p, params.nPartitionIndex, NULL, &colour);
        g_AntiFlimmer = oldAntiFlimmer;
    }

    if (g_bShadowBounds)
    {
        nlMatrix4 mLight;
        mLight.SetIdentity();
        mLight.m41 = vLight.x;
        mLight.m42 = vLight.y;
        mLight.m43 = vLight.z;
        mLight.m44 = 1.0f;

        c.c[0] = 0xFF;
        c.c[1] = 0xFF;
        c.c[2] = 0x40;
        c.c[3] = 0xFF;
        g_ShapeRenderer.DrawSpherePrimitive(mLight, 0.5f, c);
    }
}
