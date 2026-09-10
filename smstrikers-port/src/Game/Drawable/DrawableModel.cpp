#include "Game/Drawable/DrawableModel.h"
#include "Game/Debug/ShapeRender.h"
#include "Game/Render/RenderShadow.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glDraw3.h"
#include "NL/gl/glState.h"
#include "NL/gl/glAppAttach.h"
#include "NL/gl/glUserData.h"
#include "NL/glx/glxDisplayList.h"
#include "port/endian.h"
#include "NL/nlString.h"
#include "Game/GameObjectLighting.h"
#include "Game/GL/GLInventory.h"
#include "Game/GL/GLVertexAnim.h"
#include "Game/Render/Jumbotron.h"
#include "Game/Render/CrowdManager.h"
#include "Game/WorldManager.h"

const u32 GLTT_BumpLocal_bit = 1 << (int)GLTT_BumpLocal;

static const unsigned long UnlitProgram = glGetProgram("3d unlit");
static const unsigned long LitProgram = glGetProgram("3d pointlit");
static const unsigned long LightTexture = glGetTexture("global/lightramp");
static const unsigned long BlackTexture = glGetTexture("global/black");
static const unsigned long WhiteTexture = glGetTexture("global/white");
static const unsigned long UnlitCrowdProgram = glGetProgram("3d crowd");
static const unsigned long LitCrowdProgram = glGetProgram("3d crowd lit");

// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
bool g_bShadowVolumes;
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
unsigned char g_bDrawBoundingBoxes;

// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
bool g_bEnableDrawableModel = true;
static bool g_bLightDynamicObjects = true;
static bool g_bDrawLitObjects = true;
static bool g_bDrawSpecularObjects = true;
static bool g_bCalculateFresnel = true;
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
bool g_bDrawPlanarShadows = true;
static bool g_bDrawObjectsWithPlanarShadows = true;
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
bool g_bBallGlow = true;
static float g_fBallGlowH = 1.0f;
static float g_fBallGlowR0 = 2.0f;
static int g_nBallGlowA0 = 100;
static int g_nBallGlowA1 = 20;
static int g_nBallGlowRed = 255;
static int g_nBallGlowGreen = 255;
static int g_nBallGlowBlue = 255;
bool g_bCoPlanarPerObject = true;
static float sfPlanarShadowOpacity = 0.3f;
static float sfCoPlanarZ = 0.1f;
static float sfCoPlanar0Z = 0.041666668f;
static float g_fBallShadowH = 3.0f;
static float g_fBallShadowR0 = 0.275f;
static float g_fBallShadowR1 = 0.625f;
static int g_nBallShadowA0 = 128;
static int g_nBallShadowA1 = 72;
static float g_fBallGlowR1;
static unsigned char g_bCoPlanarReferenceVis;
static unsigned char g_bCoPlanarIgnoreIdentity;
unsigned char DrawableModel::sbBallShadowDisabled;

static nlAVLTreeSlotPool<unsigned long, AABBDimensions, DefaultKeyCompare<unsigned long> > boundingBoxCache(16, 16);

static const unsigned long BallModelID = nlStringHash("gameplay/ball");

static void DrawBallShadow(const nlVector3& vPosition, const BallShadowParams& p, bool bGlow);

/**
 * Offset/Address/Size: 0x1DD4 | 0x80121BE0 | size: 0x214
 */
static void DrawBallShadow(const nlVector3& vPosition, const BallShadowParams& p, bool bGlow)
{
    f32 frac = vPosition.z / p.fReferenceHeight;
    if (frac < 0.0f)
    {
        frac = 0.0f;
    }
    if (frac > 1.0f)
    {
        frac = 1.0f;
    }

    f32 fX0, fY0, fX1, fY1;
    f32 half_dim = (1.0f - frac) * p.fRadius0 + frac * p.fRadius1;
    f32 fAlpha = (1.0f - frac) * (f32)p.nAlpha0 + frac * (f32)p.nAlpha1;
    s32 alpha = (s32)fAlpha;
    if (alpha < 0)
    {
        alpha = 0;
    }
    if (alpha > 255)
    {
        alpha = 255;
    }

    nlColour c = p.colour;
    fY0 = vPosition.y - half_dim;
    fX0 = vPosition.x - half_dim;

    glQuad3 quad;
    c.c[3] = (u8)alpha;

    fY1 = vPosition.y + half_dim;
    fX1 = vPosition.x + half_dim;

    quad.m_pos[0].x = fX0;
    quad.m_pos[0].y = fY0;
    quad.m_pos[0].z = 1.0f / 64.0f;
    quad.m_pos[1].x = fX0;
    quad.m_pos[1].y = fY1;
    quad.m_pos[1].z = 1.0f / 64.0f;
    quad.m_pos[2].x = fX1;
    quad.m_pos[2].y = fY1;
    quad.m_pos[2].z = 1.0f / 64.0f;
    quad.m_pos[3].x = fX1;
    quad.m_pos[3].y = fY0;
    quad.m_pos[3].z = 1.0f / 64.0f;

    quad.m_uv[0].x = 1.0f;
    quad.m_uv[0].y = 1.0f;
    quad.m_uv[1].x = 0.0f;
    quad.m_uv[1].y = 1.0f;
    quad.m_uv[2].x = 0.0f;
    quad.m_uv[2].y = 0.0f;
    quad.m_uv[3].x = 1.0f;
    quad.m_uv[3].y = 0.0f;

    quad.m_colour[3] = c;
    quad.m_colour[2] = c;
    quad.m_colour[1] = c;
    quad.m_colour[0] = c;

    glSetDefaultState(true);

    glSetRasterState(GLS_AlphaBlend, bGlow ? 3 : 1);
    glSetRasterState(GLS_AlphaTest, 1);
    glSetRasterState(GLS_Culling, 0);
    glSetRasterState(GLS_DepthWrite, 0);
    glSetCurrentRasterState(glHandleizeRasterState());

    glSetCurrentTexture(glGetTexture(bGlow ? "global/light_blob" : "global/ball_shadow"), GLTT_Diffuse);
    glSetTextureState(GLTS_DiffuseWrap, 3);
    glSetCurrentTextureState(glHandleizeTextureState());

    const glModel* pModel = quad.GetModel(true);
    void* pUserData = glAppGetNoFogUserData();
    glUserAttach(pUserData, pModel->packets, false);
    glViewAttachModel(GLV_Unshadowed, pModel);
}

static inline void* FindStream(glModelPacket* pPacket, int streamID)
{
    for (int i = 0; i < pPacket->numStreams; i++)
    {
        if (pPacket->streams[i].id == streamID)
        {
            return (void*)pPacket->streams[i].address;
        }
    }
    return NULL;
}

/**
 * Offset/Address/Size: 0x183C | 0x80121648 | size: 0x598
 */
static void Fresnelify(glModelPacket* pPacket, eGLView view)
{
    nlMatrix4 viewMat;
    nlMatrix4 objectMat;
    nlMatrix4 modelview;

    u32 glossInt = (u8)glGetTextureState(pPacket->state.texturestate, GLTS_GlossLevel);
    f32 glossLevel = (f32)glossInt * (1.0f / 63.0f);
    if (glossLevel > 0.85f)
    {
        return;
    }

    s8* pNormals = (s8*)FindStream(pPacket, 1);
    nlColour* pColours = (nlColour*)FindStream(pPacket, 2);

    glViewGetViewMatrix(view, viewMat);
    glGetMatrix(pPacket->state.matrix, objectMat);
    nlMultMatrices(modelview, objectMat, viewMat);
    nlInvertMatrix(modelview, modelview);
    nlTransposeMatrix(modelview, modelview);

    DisplayList* pList = dlGetStruct(pPacket->indexBuffer);
    s32 index = 0;
    s32 numVerts = pPacket->numVertices;

    if (glossLevel > 0.5f)
    {
        while (index < numVerts)
        {
            u16* pVert;
            if (((u16*)&pList->indices)[1] != 0)
            {
                u16 ns = ((u16*)&pList->indices)[0];
                int stride = (ns - 1) * 2 + 1;
                int offset = stride * index;
                pVert = (u16*)((u8*)pList->list + offset);
                pVert += 2;
            }
            else
            {
                u16 ns = ((u16*)&pList->indices)[0];
                int stride = ns * 2;
                int offset = index * stride;
                u8* ptr8 = (u8*)pList->list;
                ptr8 += offset;
                pVert = (u16*)ptr8;
                ptr8 = (u8*)pVert;
                ptr8 += 3;
                pVert = (u16*)ptr8;
            }
            // PORT: FIFO indices are big-endian.
            int vertIndex = port_be16(pVert);
            s8* pNormal = pNormals + vertIndex * 3;
            nlColour* pColour = pColours + vertIndex;
            nlVector3 normal = { 0 };
            normal.x = (f32)pNormal[0] * (1.0f / 64.0f);
            normal.y = (f32)pNormal[1] * (1.0f / 64.0f);
            normal.z = (f32)pNormal[2] * (1.0f / 64.0f);
            f32 dot = normal.x * modelview.e2[0][2] + normal.y * modelview.e2[1][2] + normal.z * modelview.e2[2][2];
            if (dot < 0.0f)
            {
                dot = 0.0f;
            }
            f32 fresnel = 1.0f - dot;
            fresnel = fresnel * fresnel * 255.5f;
            pColour->c[3] = (u8)(s32)fresnel;
            index++;
        }
    }
    else if (glossLevel > 0.25f)
    {
        while (index < numVerts)
        {
            u16* pVert;
            if (((u16*)&pList->indices)[1] != 0)
            {
                u16 ns = ((u16*)&pList->indices)[0];
                int stride = (ns - 1) * 2 + 1;
                int offset = stride * index;
                pVert = (u16*)((u8*)pList->list + offset);
                pVert += 2;
            }
            else
            {
                u16 ns = ((u16*)&pList->indices)[0];
                int stride = ns * 2;
                int offset = index * stride;
                u8* ptr8 = (u8*)pList->list;
                ptr8 += offset;
                pVert = (u16*)ptr8;
                ptr8 = (u8*)pVert;
                ptr8 += 3;
                pVert = (u16*)ptr8;
            }
            // PORT: FIFO indices are big-endian.
            int vertIndex = port_be16(pVert);
            s8* pNormal = pNormals + vertIndex * 3;
            nlColour* pColour = pColours + vertIndex;
            nlVector3 normal = { 0 };
            normal.x = (f32)pNormal[0] * (1.0f / 64.0f);
            normal.y = (f32)pNormal[1] * (1.0f / 64.0f);
            normal.z = (f32)pNormal[2] * (1.0f / 64.0f);
            f32 dot = normal.x * modelview.e2[0][2] + normal.y * modelview.e2[1][2] + normal.z * modelview.e2[2][2];
            if (dot < 0.0f)
            {
                dot = 0.0f;
            }
            f32 fresnel = 1.0f - dot;
            fresnel *= fresnel * fresnel;
            pColour->c[3] = (u8)(s32)(255.5f * fresnel);
            index++;
        }
    }
    else
    {
        while (index < numVerts)
        {
            u16* pVert;
            if (((u16*)&pList->indices)[1] != 0)
            {
                u16 ns = ((u16*)&pList->indices)[0];
                int stride = (ns - 1) * 2 + 1;
                int offset = stride * index;
                pVert = (u16*)((u8*)pList->list + offset);
                pVert += 2;
            }
            else
            {
                u16 ns = ((u16*)&pList->indices)[0];
                int stride = ns * 2;
                int offset = index * stride;
                u8* ptr8 = (u8*)pList->list;
                ptr8 += offset;
                pVert = (u16*)ptr8;
                ptr8 = (u8*)pVert;
                ptr8 += 3;
                pVert = (u16*)ptr8;
            }
            // PORT: FIFO indices are big-endian.
            int vertIndex = port_be16(pVert);
            s8* pNormal = pNormals + vertIndex * 3;
            nlColour* pColour = pColours + vertIndex;
            nlVector3 normal = { 0 };
            normal.x = (f32)pNormal[0] * (1.0f / 64.0f);
            normal.y = (f32)pNormal[1] * (1.0f / 64.0f);
            normal.z = (f32)pNormal[2] * (1.0f / 64.0f);
            f32 dot = normal.x * modelview.e2[0][2] + normal.y * modelview.e2[1][2] + normal.z * modelview.e2[2][2];
            if (dot < 0.0f)
            {
                dot = 0.0f;
            }
            f32 fresnel = 1.0f - dot;
            fresnel *= fresnel * fresnel * fresnel;
            pColour->c[3] = (u8)(s32)(255.5f * fresnel);
            index++;
        }
    }
}

/**
 * Offset/Address/Size: 0x12A4 | 0x801210B0 | size: 0x598
 */
void DrawableModel::DrawModel(const nlMatrix4& worldMatrix)
{
    void* pLightData = NULL;
    void* pSpecularData = NULL;
    void* pEnvData = NULL;
    void* pTransData = NULL;
    void* pNoFogData = NULL;
    uintptr_t LightTexture;
    unsigned char bJumbotron;
    unsigned long bCrowd;
    unsigned long litProgram;
    unsigned long unlitProgram;
    unsigned int bLight;
    unsigned char bSpec;
    glModel* newModel;

    if (!g_bEnableDrawableModel)
        return;

    if (!g_bDrawObjectsWithPlanarShadows && m_bRenderPlanarShadow)
        return;

    if (m_uObjectCreationFlags & 0x00100000)
    {
        pNoFogData = glAppGetNoFogUserData();
    }

    unsigned char bTransparent = (m_uObjectCreationFlags & 0x0000F000) != 0;
    if (bTransparent)
    {
        float f31 = m_translucency;
        if (f31 == 0.0f)
            return;
        if (f31 == 1.0f)
            bTransparent = 0;
        if (bTransparent)
        {
            pTransData = glUserAlloc(GLUD_Translucent, 4, false);
            float* pFloat = (float*)glUserGetData(pTransData);
            *pFloat = f31;
        }
    }

    eGLView view;
    if (m_uObjectFlags & 0x00000008)
    {
        view = (eGLView)0x16;
    }
    else
    {
        eGLView v = (eGLView)3;
        if (m_uObjectCreationFlags & 0x00000004)
            v = (eGLView)7;
        view = v;
        if (m_uObjectCreationFlags & 0x00000100)
            v = (eGLView)2;
        view = v;
        if (m_uObjectFlags & 0x00000040)
            v = (eGLView)0x13;
        view = v;
    }

    bCrowd = (m_uObjectCreationFlags >> 17) & 1;
    bJumbotron = (m_uObjectCreationFlags >> 16) & 1;

    if (bCrowd)
    {
        litProgram = LitCrowdProgram;
        unlitProgram = UnlitCrowdProgram;
    }
    else
    {
        litProgram = LitProgram;
        unlitProgram = UnlitProgram;
    }

    if (m_uObjectFlags & 0x00000100)
    {
        LightTexture = GetGameObjectLightRamp();
    }
    else
    {
        LightTexture = m_pWorldContext->m_LightRampTexA;
    }

    bLight = ((m_uObjectCreationFlags >> 7) & 1) ^ 1;
    if ((unsigned char)bLight)
    {
        unsigned int temp = (m_uObjectFlags & 0x00000004) ? g_bLightDynamicObjects : 1;
        bLight = temp;
    }
    if ((unsigned char)bLight)
    {
        if (!g_bDrawLitObjects)
            return;
    }

    bSpec = (m_uObjectCreationFlags >> 3) & 1;
    if (bSpec)
    {
        if (!g_bDrawSpecularObjects)
            return;
    }

    if (m_bVertexAnimated)
    {
        GLVertexAnim* pAnim = glInventory.GetVertexAnim(m_pModel->id);
        newModel = pAnim->GetModel(-1);
    }
    else
    {
        newModel = glModelDupNoStreams(m_pModel, true, false);
    }

    if (m_pModel->id == BallModelID && !sbBallShadowDisabled && worldMatrix.e2[3][2] >= 0.0f)
    {
        BallShadowParams p;
        p.fReferenceHeight = g_fBallShadowH;
        p.fRadius0 = g_fBallShadowR0;
        p.fRadius1 = g_fBallShadowR1;
        p.nAlpha0 = g_nBallShadowA0;
        p.nAlpha1 = g_nBallShadowA1;
        p.colour.c[0] = 0xFF;
        p.colour.c[1] = 0xFF;
        p.colour.c[2] = 0xFF;
        p.colour.c[3] = 0xFF;
        DrawBallShadow(*(const nlVector3*)&worldMatrix.e2[3][0], p, false);

        if (g_bBallGlow)
        {
            p.fReferenceHeight = g_fBallGlowH;
            p.fRadius0 = g_fBallGlowR0;
            p.fRadius1 = g_fBallGlowR1;
            p.nAlpha0 = g_nBallGlowA0;
            p.nAlpha1 = g_nBallGlowA1;
            p.colour.c[0] = g_nBallGlowRed;
            p.colour.c[1] = g_nBallGlowGreen;
            p.colour.c[2] = g_nBallGlowBlue;
            p.colour.c[3] = 0xFF;
            DrawBallShadow(*(const nlVector3*)&worldMatrix.e2[3][0], p, true);
        }
    }

    if ((m_uObjectCreationFlags & 0x00000010) && g_bCalculateFresnel)
    {
        pEnvData = glUserAlloc(GLUD_EnvDiffuse, 0, false);
    }

    if ((unsigned char)bLight)
    {
        if (m_uObjectFlags & 0x00000100)
        {
            pLightData = GetInGameLightData();
        }
        else
        {
            pLightData = m_pWorldContext->m_pIntensityPerm;
        }

        if (m_uObjectCreationFlags & 0x00000008)
        {
            pSpecularData = m_pWorldContext->m_pSTSIntensity;
        }
    }
    else
    {
        if (bSpec)
        {
            pSpecularData = m_pWorldContext->m_pSTSIntensity;
        }
    }

    uintptr_t matHandle = glAllocMatrix();
    if ((u32)(matHandle + 0x10000) != 0xFFFF)
    {
        glSetMatrix(matHandle, worldMatrix);
    }

    if (m_uObjectCreationFlags & 0x00200000)
    {
        for (glModelPacket* pPacket = newModel->packets; pPacket < newModel->packets + newModel->numPackets; pPacket++)
        {
            void* fresnelData = glAppGetOnePassFresnelUserData();
            glUserAttach(fresnelData, pPacket, false);
        }
    }

    for (glModelPacket* pPacket = newModel->packets; pPacket < newModel->packets + newModel->numPackets; pPacket++)
    {
        pPacket->state.matrix = matHandle;

        if (bJumbotron)
        {
            u32 tex = Jumbotron::instance.m_CurrentTexture;
            if ((u32)(tex + 0x10000) != 0xFFFF)
            {
                pPacket->state.texture[0] = tex;
            }
        }

        if (bCrowd)
        {
            u32 tex = CrowdManager::instance.GetTextureHandle(newModel->id);
            if ((u32)(tex + 0x10000) != 0xFFFF)
            {
                pPacket->state.texture[0] = tex;
            }
        }

        if (pNoFogData)
            glUserAttach(pNoFogData, pPacket, false);
        if (pTransData)
            glUserAttach(pTransData, pPacket, false);
        if (pEnvData)
        {
            glUserAttach(pEnvData, pPacket, false);
            Fresnelify(pPacket, view);
        }

        pPacket->state.program = unlitProgram;
        if (pLightData)
        {
            pPacket->state.program = litProgram;
            glUserAttach(pLightData, pPacket, false);

            if (!(pPacket->state.texconfig & 0x20))
            {
                pPacket->state.texture[5] = LightTexture;
                pPacket->state.texconfig |= GLTT_BumpLocal_bit;
            }

            if (pSpecularData && (pPacket->state.texconfig & 0x10))
            {
                void* specData = m_pWorldContext->GetCustomSpecularData(pPacket, false);
                glUserAttach(specData, pPacket, false);
            }
        }
        else
        {
            if (pSpecularData && (pPacket->state.texconfig & 0x10))
            {
                void* specData = m_pWorldContext->GetCustomSpecularData(pPacket, false);
                glUserAttach(specData, pPacket, false);
            }
        }
    }

    if (m_CB)
    {
        newModel = m_CB(newModel, view, m_uRenderLayer);
        if (!newModel)
            return;
    }

    glViewAttachModel(view, m_uRenderLayer, newModel);

    if (m_bRenderPlanarShadow)
    {
        DrawPlanarShadow();
    }
}

/**
 * Offset/Address/Size: 0x126C | 0x80121078 | size: 0x38
 */
void DrawableModel::Draw()
{
    DrawModel(GetWorldMatrix());
}

/**
 * Offset/Address/Size: 0x10A4 | 0x80120EB0 | size: 0x1C8
 */
DrawableObject* DrawableModel::Clone() const
{
    DrawableObject* pClone = new (nlMalloc(sizeof(DrawableModel), 8, false)) DrawableModel(*this);

    pClone->m_uObjectFlags |= 0x1;
    pClone->m_uObjectFlags |= 0x4;

    return pClone;
}

/**
 * Offset/Address/Size: 0xD18 | 0x80120B24 | size: 0x38C
 */
void GetAABBDimensions(const glModel* model, AABBDimensions& dimensions, uintptr_t boundingBoxCacheKey)
{
    AABBDimensions* foundValue = NULL;
    u8* packets;
    int packetOffset;
    AVLTreeNode** root = (AVLTreeNode**)&boundingBoxCache.m_Root;
    AVLTreeEntry<unsigned long, AABBDimensions>* node = (AVLTreeEntry<unsigned long, AABBDimensions>*)*root;

    while (node != NULL)
    {
        DefaultKeyCompare<unsigned long> compare;
        int cmpResult = compare(node->key, boundingBoxCacheKey);

        if (cmpResult == 0)
        {
            if (&foundValue != NULL)
            {
                foundValue = &node->value;
            }
            break;
        }

        if (cmpResult < 0)
        {
            node = (AVLTreeEntry<unsigned long, AABBDimensions>*)node->node.left;
        }
        else
        {
            node = (AVLTreeEntry<unsigned long, AABBDimensions>*)node->node.right;
        }
    }

    if (foundValue != NULL)
    {
        dimensions = *foundValue;
        return;
    }

    packets = (u8*)model->packets;
    unsigned char first = 1;
    unsigned int packetIndex = 0;
    glModelPacket* packet;
    int vertexIndex;
    packetOffset = 0;
    nlVector3 min;
    nlVector3 max;

    while (packetIndex < model->numPackets)
    {
        packet = (glModelPacket*)(packets + packetOffset);
        DisplayList* list = dlGetStruct(packet->indexBuffer);
        vertexIndex = 0;

        while (vertexIndex < packet->numVertices)
        {
            u16* pVert;
            if (((u16*)&list->indices)[1] != 0)
            {
                u16 ns = ((u16*)&list->indices)[0];
                int stride = (ns - 1) * 2 + 1;
                int offset = stride * vertexIndex;
                pVert = (u16*)((u8*)list->list + offset);
                pVert += 2;
            }
            else
            {
                u16 ns = ((u16*)&list->indices)[0];
                int stride = ns * 2;
                int offset = vertexIndex * stride;
                u8* ptr8 = (u8*)list->list;
                ptr8 += offset;
                pVert = (u16*)ptr8;
                ptr8 = (u8*)pVert;
                ptr8 += 3;
                pVert = (u16*)ptr8;
            }

            glModelStream* stream = packet->streams;
            // PORT: FIFO indices are big-endian.
            u16 vert = port_be16(pVert);
            u8 stride = stream->stride;
            nlVector3 point;

            // PORT: a stream out of a model file is big-endian; unswapped, the byte swap and the fixed 1/256 cancel and every extent comes back as its raw s16.
            const u8* pSrc = (const u8*)stream->address + vert * stride;
            if (stride == 12)
            {
                if (stream->beData)
                {
                    point.x = port_bef32(pSrc + 0);
                    point.y = port_bef32(pSrc + 4);
                    point.z = port_bef32(pSrc + 8);
                }
                else
                {
                    memcpy(&point, pSrc, 12);
                }
            }
            else
            {
                const s16* src = (const s16*)pSrc;
                float scale = 1.0f / 256.0f;
                s16 sx = stream->beData ? (s16)port_be16(pSrc + 0) : src[0];
                s16 sy = stream->beData ? (s16)port_be16(pSrc + 2) : src[1];
                s16 sz = stream->beData ? (s16)port_be16(pSrc + 4) : src[2];

                point.x = (float)sx * scale;
                point.y = (float)sy * scale;
                point.z = (float)sz * scale;
            }

            if (point.x < min.x || first)
            {
                min.x = point.x;
            }

            if (point.y < min.y || first)
            {
                min.y = point.y;
            }

            if (point.z < min.z || first)
            {
                min.z = point.z;
            }

            if (point.x > max.x || first)
            {
                max.x = point.x;
            }

            if (point.y > max.y || first)
            {
                max.y = point.y;
            }

            if (point.z > max.z || first)
            {
                max.z = point.z;
            }

            first = 0;
            vertexIndex++;
        }

        packetOffset += sizeof(glModelPacket);
        packetIndex++;
    }

    dimensions.mMin = min;
    dimensions.mMax = max;
    float dz = dimensions.mMax.z - dimensions.mMin.z;
    float dy = dimensions.mMax.y - dimensions.mMin.y;
    float dx = dimensions.mMax.x - dimensions.mMin.x;
    dimensions.mDim.x = dx;
    dimensions.mDim.y = dy;
    dimensions.mDim.z = dz;

    if (boundingBoxCacheKey != 0)
    {
        AVLTreeNode* existingNode;

        boundingBoxCache.AddAVLNode(root, &boundingBoxCacheKey, &dimensions, &existingNode, boundingBoxCache.m_NumElements);

        if (existingNode == NULL)
        {
            boundingBoxCache.m_NumElements++;
        }
    }
}

/**
 * Offset/Address/Size: 0xCF0 | 0x80120AFC | size: 0x28
 */
void DrawableModel::GetAABBDimensions(AABBDimensions& dims, bool param) const
{
    ::GetAABBDimensions(m_pModel, dims, 0);
}

/**
 * Offset/Address/Size: 0xC78 | 0x80120A84 | size: 0x78
 */
void DrawableShadow::Draw()
{
    if (g_bShadowVolumes)
    {
        uintptr_t mtx;                 // PORT: a matrix handle
        nlMatrix4& worldMtx = GetWorldMatrix();
        mtx = glAllocMatrix();
        if ((u32)(mtx + 0x10000) != 0xFFFF)
        {
            glSetMatrix(mtx, worldMtx);
        }
        RenderShadowModel(2, m_pModel, mtx);
    }
}

/**
 * Offset/Address/Size: 0xBF4 | 0x80120A00 | size: 0x84
 */
void DrawableModel::DrawPlanarShadow()
{
    glModel* model = glModelDupNoStreams(m_pModel, true, false);
    f32 opacity = sfPlanarShadowOpacity;
    const nlMatrix4& worldMtx = GetWorldMatrix();
    ::DrawPlanarShadow(model, worldMtx, opacity * m_translucency, true, false, true, (uintptr_t)this);
}

static void GetShadowBoundingSquare(const glModel* model, const nlMatrix4& matrix, float& x0, float& x1, float& y0, float& y1, uintptr_t userData);

static inline void MakePlanarShadowMatrix(nlMatrix4& shadowMatrix, const nlMatrix4& objectToWorldMatrix)
{
    // PORT: was World+0x138 read as a u32, plus 4.
    const nlVector3& lightVector =
        WorldManager::s_World->m_pShadowLight->m_worldPosition;
    float xOverZ = -lightVector.x / lightVector.z;
    float yOverZ = -lightVector.y / lightVector.z;

    shadowMatrix.m11 = objectToWorldMatrix.m11 + xOverZ * objectToWorldMatrix.m13;
    shadowMatrix.m21 = objectToWorldMatrix.m21 + xOverZ * objectToWorldMatrix.m23;
    shadowMatrix.m31 = objectToWorldMatrix.m31 + xOverZ * objectToWorldMatrix.m33;
    shadowMatrix.m41 = objectToWorldMatrix.m41 + xOverZ * objectToWorldMatrix.m43;
    shadowMatrix.m12 = objectToWorldMatrix.m12 + yOverZ * objectToWorldMatrix.m13;
    shadowMatrix.m22 = objectToWorldMatrix.m22 + yOverZ * objectToWorldMatrix.m23;
    shadowMatrix.m32 = objectToWorldMatrix.m32 + yOverZ * objectToWorldMatrix.m33;
    shadowMatrix.m42 = objectToWorldMatrix.m42 + yOverZ * objectToWorldMatrix.m43;
    shadowMatrix.m13 = 0.0f;
    shadowMatrix.m23 = 0.0f;
    shadowMatrix.m33 = 0.0f;
    shadowMatrix.m43 = 0.0f;
    shadowMatrix.m14 = 0.0f;
    shadowMatrix.m24 = 0.0f;
    shadowMatrix.m34 = 0.0f;
    shadowMatrix.m44 = 1.0f;
}

/**
 * Offset/Address/Size: 0x964 | 0x80120770 | size: 0x290
 */
static void GetShadowBoundingSquare(const glModel* model, const nlMatrix4& matrix, float& x0, float& x1, float& y0, float& y1, uintptr_t userData)
{
    AABBDimensions dimensions;
    GetAABBDimensions(model, dimensions, userData);

    const nlVector3& max = dimensions.mMax;
    nlVector4 p[8];

    nlVec4Set(p[0], dimensions.mMin.x, dimensions.mMin.y, dimensions.mMin.z, 1.0f);
    nlVec4Set(p[1], dimensions.mMin.x, dimensions.mMin.y, max.z, 1.0f);
    nlVec4Set(p[2], dimensions.mMin.x, max.y, dimensions.mMin.z, 1.0f);
    nlVec4Set(p[3], dimensions.mMin.x, max.y, max.z, 1.0f);
    nlVec4Set(p[4], max.x, dimensions.mMin.y, dimensions.mMin.z, 1.0f);
    nlVec4Set(p[5], max.x, dimensions.mMin.y, max.z, 1.0f);
    nlVec4Set(p[6], max.x, max.y, dimensions.mMin.z, 1.0f);
    nlVec4Set(p[7], max.x, max.y, max.z, 1.0f);

    nlMatrix4 shadowMatrix;
    MakePlanarShadowMatrix(shadowMatrix, matrix);

    int i;
    for (i = 0; i < 8; i++)
    {
        nlMultVectorMatrix(p[i], p[i], shadowMatrix);

        if (i == 0 || p[i].x < x0)
        {
            x0 = p[i].x;
        }

        if (i == 0 || p[i].x > x1)
        {
            x1 = p[i].x;
        }

        if (i == 0 || p[i].y < y0)
        {
            y0 = p[i].y;
        }

        if (i == 0 || p[i].y > y1)
        {
            y1 = p[i].y;
        }
    }
}

/**
 * Offset/Address/Size: 0x730 | 0x8012053C | size: 0x234
 */
static void DrawCoPlanarReference(eGLView view, const glModel& model, const nlMatrix4& mtx, uintptr_t userData)
{

    float z;
    if (view == GLV_CoPlanar0)
    {
        z = sfCoPlanar0Z;
    }
    else
    {
        z = sfCoPlanarZ;
    }

    float x0;
    float x1;
    float y0;
    float y1;
    GetShadowBoundingSquare(&model, mtx, x0, x1, y0, y1, userData);

    nlVector3 points[4] = { };
    points[0].x = x0;
    points[0].y = y0;
    points[0].z = z;
    points[1].x = x1;
    points[1].y = y0;
    points[1].z = z;
    points[2].x = x1;
    points[2].y = y1;
    points[2].z = z;
    points[3].x = x0;
    points[3].y = y1;
    points[3].z = z;

    if (x0 + x1 < 0.0f)
    {
        points[0].x = x1;
        points[1].x = x0;
        points[2].x = x0;
        points[3].x = x1;
    }

    glSetDefaultState(false);

    glSetRasterState(GLS_Culling, g_bCoPlanarReferenceVis ? GX_CULL_NONE : GX_CULL_ALL);
    glSetRasterState(GLS_DepthTest, 0);
    glSetRasterState(GLS_DepthWrite, 0);

    glSetCurrentRasterState(glHandleizeRasterState());
    glSetCurrentTexture((uintptr_t)ResolvedWhiteTexture, GLTT_Diffuse);

    glQuad3 quad;
    quad.m_pos[0] = points[0];
    quad.m_uv[0].x = 0.0f;
    quad.m_uv[0].y = 0.0f;

    quad.m_pos[1] = points[1];
    quad.m_uv[1].x = 0.0f;
    quad.m_uv[1].y = 0.0f;

    quad.m_pos[2] = points[2];
    quad.m_uv[2].x = 0.0f;
    quad.m_uv[2].y = 0.0f;

    quad.m_pos[3] = points[3];
    quad.m_uv[3].x = 0.0f;
    quad.m_uv[3].y = 0.0f;

    quad.SetColour(0xAA, 0xAA, 0xAA, 0xFF);
    quad.Attach(view, 0, true);
}

/**
 * Offset/Address/Size: 0x2D0 | 0x801200DC | size: 0x460
 */
void DrawPlanarShadow(const glModel* model, const nlMatrix4& worldMatrix, float shadowTranslucency, bool ignorePacketMatrices, bool isModelPosed, bool bFieldOnlyShadow, uintptr_t boundingBoxCacheKey)
{

    nlMatrix4 packetMatrix;
    nlMatrix4 packetShadowMatrix;
    nlMatrix4 transformedPacketMatrix;
    nlMatrix4 packetMat;
    nlMatrix4 mat;
    eGLView view;
    glModelPacket* pPacket;
    void* pTransData;

    if (g_bDrawBoundingBoxes)
    {
        RenderBoundingBox(model, worldMatrix);
    }

    if (!g_bDrawPlanarShadows)
    {
        return;
    }

    view = GLV_CoPlanar;
    if (bFieldOnlyShadow)
    {
        view = GLV_CoPlanar0;
    }

    pTransData = glUserAlloc(GLUD_Translucent, 4, false);
    *(float*)glUserGetData(pTransData) = shadowTranslucency;

    if (g_bCoPlanarPerObject)
    {
        if (g_bCoPlanarIgnoreIdentity && worldMatrix.m41 == 0.0f && worldMatrix.m42 == 0.0f && worldMatrix.m43 == 0.0f)
        {
            return;
        }

        if (ignorePacketMatrices)
        {
            mat = worldMatrix;
        }
        else
        {
            glGetMatrix(model->packets->state.matrix, packetMat);
            nlMultMatrices(mat, worldMatrix, packetMat);
        }

        DrawCoPlanarReference(view, *model, mat, boundingBoxCacheKey);
    }

    pPacket = model->packets;

    while (pPacket < model->packets + model->numPackets)
    {
        if (!ignorePacketMatrices)
        {
            glGetMatrix(pPacket->state.matrix, packetMatrix);

            if (isModelPosed)
            {
                transformedPacketMatrix = packetMatrix;
            }
            else
            {
                nlMultMatrices(transformedPacketMatrix, worldMatrix, packetMatrix);
            }

            MakePlanarShadowMatrix(packetShadowMatrix, transformedPacketMatrix);
        }
        else
        {
            MakePlanarShadowMatrix(packetShadowMatrix, worldMatrix);
        }

        // PORT: GX co-planar mode gave these pixels the depth of the reference quad drawn just before them.
        packetShadowMatrix.m43 = (view == GLV_CoPlanar0) ? sfCoPlanar0Z : sfCoPlanarZ;

        uintptr_t shadowMatrix = glAllocMatrix();
        if ((u32)(shadowMatrix + 0x10000) != 0xFFFF)
        {
            glSetMatrix(shadowMatrix, packetShadowMatrix);
        }

        pPacket->state.matrix = shadowMatrix;
        pPacket->state.texture[0] = (uintptr_t)ResolvedBlackTexture;
        pPacket->state.program = UnlitProgram;

        glUserAttach(pTransData, pPacket, false);
        glUserDetach(GLUD_Light, pPacket);
        glSetRasterState(pPacket->state.raster, GLS_AlphaBlend, 1);

        pPacket++;
    }

    // PORT: the two passes co-planar mode made one.
    {
        void* pCoPlanar = glAppGetCoPlanarUserData();
        glModel* pDepth = glModelDup(model, true);
        glModelPacket* pD = pDepth->packets;
        while (pD < pDepth->packets + pDepth->numPackets)
        {
            glUserAttach(pCoPlanar, pD, false);
            glSetRasterState(pD->state.raster, GLS_ColourWrite, 0);
            glSetRasterState(pD->state.raster, GLS_AlphaBlend, 0);
            glSetRasterState(pD->state.raster, GLS_DepthTest, 1);
            glSetRasterState(pD->state.raster, GLS_DepthWrite, 1);
            glSetRasterState(pD->state.raster, GLS_DepthFunc, 3);
            pD++;
        }
        glViewAttachModel(GLV_CoPlanar0, pDepth);

        pPacket = model->packets;
        while (pPacket < model->packets + model->numPackets)
        {
            glUserAttach(pCoPlanar, pPacket, false);
            glSetRasterState(pPacket->state.raster, GLS_DepthTest, 1);
            glSetRasterState(pPacket->state.raster, GLS_DepthWrite, 0);
            glSetRasterState(pPacket->state.raster, GLS_DepthFunc, 2);
            pPacket++;
        }
        glViewAttachModel(GLV_CoPlanar, model);
    }
}

/**
 * Offset/Address/Size: 0x2C8 | 0x801200D4 | size: 0x8
 */
float GetPlanarShadowOpacity()
{
    return sfPlanarShadowOpacity;
}

/**
 * Offset/Address/Size: 0x2C0 | 0x801200CC | size: 0x8
 */
void SetPlanarShadowOpacity(float opacity)
{
    sfPlanarShadowOpacity = opacity;
}

/**
 * Offset/Address/Size: 0x2B8 | 0x801200C4 | size: 0x8
 */
float GetCoPlanarZ()
{
    return sfCoPlanarZ;
}

/**
 * Offset/Address/Size: 0x2B0 | 0x801200BC | size: 0x8
 */
void SetCoPlanarZ(float z)
{
    sfCoPlanarZ = z;
}

/**
 * Offset/Address/Size: 0x2A8 | 0x801200B4 | size: 0x8
 */
float GetCoPlanar0Z()
{
    return sfCoPlanar0Z;
}

/**
 * Offset/Address/Size: 0x3C | 0x8011FE48 | size: 0x26C
 */
void RenderBoundingBox(const glModel* model, const nlMatrix4& matrix)
{
    AABBDimensions dimensions;
    GetAABBDimensions(model, dimensions, 0);

    nlVector4 points[8];
    float minx = dimensions.mMin.x;
    float miny = dimensions.mMin.y;
    float minz = dimensions.mMin.z;

    nlVector4* p1 = &points[1];
    nlVector4* p2 = &points[2];
    nlVector4* p3 = &points[3];
    nlVector4* p5;
    nlVector4* p4;
    p4 = &points[4];
    p5 = &points[5];
    nlVector4* p6 = &points[6];
    nlVector4* p7 = &points[7];
    nlVector4* p0 = &points[0];
    int i = 0;

    nlVec4Set(points[0], minx, miny, minz, 1.0f);
    nlVec4Set(*p1, minx, miny, dimensions.mMax.z, 1.0f);
    nlVec4Set(*p2, minx, dimensions.mMax.y, dimensions.mMax.z, 1.0f);
    nlVec4Set(*p3, minx, dimensions.mMax.y, minz, 1.0f);
    nlVec4Set(*p4, dimensions.mMax.x, miny, minz, 1.0f);
    nlVec4Set(*p5, dimensions.mMax.x, miny, dimensions.mMax.z, 1.0f);
    nlVec4Set(*p6, dimensions.mMax.x, dimensions.mMax.y, dimensions.mMax.z, 1.0f);
    nlVec4Set(*p7, dimensions.mMax.x, dimensions.mMax.y, minz, 1.0f);

    for (; i < 8; i++, p0++)
    {
        nlMultVectorMatrix(*p0, *p0, matrix);
    }

    const nlColour blue = { 0, 0, 0xFF, 0 };

    g_ShapeRenderer.DrawLine3D((nlVector3&)points[0], (nlVector3&)*p1, blue, true);
    g_ShapeRenderer.DrawLine3D((nlVector3&)*p1, (nlVector3&)*p2, blue, true);
    g_ShapeRenderer.DrawLine3D((nlVector3&)*p2, (nlVector3&)*p3, blue, true);
    g_ShapeRenderer.DrawLine3D((nlVector3&)*p3, (nlVector3&)points[0], blue, true);

    g_ShapeRenderer.DrawLine3D((nlVector3&)*p4, (nlVector3&)*p5, blue, true);
    g_ShapeRenderer.DrawLine3D((nlVector3&)*p5, (nlVector3&)*p6, blue, true);
    g_ShapeRenderer.DrawLine3D((nlVector3&)*p6, (nlVector3&)*p7, blue, true);
    g_ShapeRenderer.DrawLine3D((nlVector3&)*p7, (nlVector3&)*p4, blue, true);

    g_ShapeRenderer.DrawLine3D((nlVector3&)*p4, (nlVector3&)points[0], blue, true);
    g_ShapeRenderer.DrawLine3D((nlVector3&)*p5, (nlVector3&)*p1, blue, true);
    g_ShapeRenderer.DrawLine3D((nlVector3&)*p6, (nlVector3&)*p2, blue, true);
    g_ShapeRenderer.DrawLine3D((nlVector3&)*p7, (nlVector3&)*p3, blue, true);
}

/**
 * Offset/Address/Size: 0x0 | 0x8011FE0C | size: 0x3C
 */
void CleanBoundingBoxCache()
{
    boundingBoxCache.Clear();
    boundingBoxCache.m_Allocator.FreeBlocks();
}
