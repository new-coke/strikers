#include "Game/FE/feRender.h"
#include "NL/glx/glxTexture.h"   // PORT: glx_GetTex
#include <stdlib.h>              // PORT: getenv, for the probes
#include "NL/vmath.h"
#include "Game/FE/tlInstance.h"
#include "Game/FE/tlImageInstance.h"
#include "Game/FE/tlTextInstance.h"
#include "Game/FE/tlComponentInstance.h"
#include "Game/FE/feScene.h"
#include "Game/FE/fePackage.h"
#include "Game/FE/fePresentation.h"
#include "Game/FE/feImage.h"
#include "Game/FE/feTextureResource.h"
#include "Game/GL/gluMeshWriter.h"
#include "NL/gl/glMatrixStack.h"
#include "NL/gl/glState.h"
#include "NL/gl/glMatrix.h"
#include "NL/nlMemory.h"
#include "NL/nlColour.h"
#include "NL/platvmath.h"
#include "NL/gl/gl.h"

static nlFloatColour s_currentAssetColour;
static unsigned long drawQuadProgram = glGetProgram("2d unlit");
FEScene* FERender::m_pRenderScene = nullptr;
GLMatrixStack* FERender::m_pMatrixStack = nullptr;
static const unsigned long grabTex = nlStringLowerHash("target/grab_texture");
static const unsigned long portMovieTex = nlStringLowerHash("movie");

/**
 * Offset/Address/Size: 0x16D8 | 0x8020B960 | size: 0xB4
 */
void FERender::CalculateCurrentAssetColour(const TLInstance* instance)
{
    for (u32 i = 0; i < 4; i++)
    {
        s_currentAssetColour.c[i] = (instance->GetColour().c[i] * s_currentAssetColour.c[i]) / 255.0f;
    }
}

/**
 * Offset/Address/Size: 0x16A0 | 0x8020B928 | size: 0x38
 */
void FERender::Cleanup()
{
    if (m_pMatrixStack != nullptr)
    {
        delete m_pMatrixStack;
        m_pMatrixStack = nullptr;
    }
}

/**
 * Offset/Address/Size: 0x1650 | 0x8020B8D8 | size: 0x50
 */
void FERender::Initialize()
{
    if (m_pMatrixStack == nullptr)
    {
        m_pMatrixStack = new (8, false) GLMatrixStack(16);
    }
}

/**
 * Offset/Address/Size: 0x155C | 0x8020B7E4 | size: 0xF4
 */
void FERender::PushTransformMatrix(const TLInstance* instance)
{
    nlMatrix4 combinedMatrix;
    nlMatrix4 scaleMatrix;
    nlMatrix4 rotationMatrix;

    nlMakeRotationMatrixEulerAngles(rotationMatrix,
        instance->GetRotation().f.x,
        instance->GetRotation().f.y,
        instance->GetRotation().f.z);

    nlMakeScaleMatrix(scaleMatrix,
        instance->GetScale().f.x,
        instance->GetScale().f.y,
        instance->GetScale().f.z);

    nlMultMatrices(combinedMatrix, scaleMatrix, rotationMatrix);

    const feVector3& tlPosition = instance->GetPosition();
    nlVector3 v3Pos;
    tlPosition.GetNLVector3(v3Pos);
    combinedMatrix.SetTranslation(v3Pos);
    combinedMatrix.m43 *= -1.0f;

    m_pMatrixStack->PushMatrix();
    m_pMatrixStack->MultMatrix(combinedMatrix);
}

/**
 * Offset/Address/Size: 0x1538 | 0x8020B7C0 | size: 0x24
 */
void FERender::PopTransformMatrix()
{
    m_pMatrixStack->PopMatrix();
}

/**
 * Offset/Address/Size: 0xD90 | 0x8020B018 | size: 0x7A8
 */
void FERender::RenderTimeLineAsset(TLInstance* pTLInstance, float fCurrentTime)
{
    if (!pTLInstance->IsValidAtTime(fCurrentTime))
    {
        return;
    }

    if (!pTLInstance->IsVisible())
    {
        return;
    }

    PushTransformMatrix(pTLInstance);
    CalculateCurrentAssetColour(pTLInstance);

    switch (pTLInstance->GetType())
    {
    case TLAT_IMAGE:
        RenderImageInstance((const TLImageInstance*)pTLInstance);
        break;
    case TLAT_TEXT:
        RenderTextInstance((TLTextInstance*)pTLInstance);
        break;
    case TLAT_COMPONENT:
        RenderComponentInstance((TLComponentInstance*)pTLInstance);
        break;
    default:
        break;
    }

    if (pTLInstance->pChildren)
    {
        TLInstance* curr = pTLInstance->pChildren->m_next;
        TLInstance* next;

        while (true)
        {
            next = curr->m_next;
            nlFloatColour colour = s_currentAssetColour;

            RenderTimeLineAsset(curr, fCurrentTime);

            s_currentAssetColour = colour;

            if (curr == pTLInstance->pChildren)
            {
                break;
            }

            curr = next;
        }
    }

    PopTransformMatrix();
}

/**
 * Offset/Address/Size: 0x978 | 0x8020AC00 | size: 0x418
 */
void FERender::RenderSlide(const TLSlide* pTLSlide)
{
    if (!pTLSlide)
    {
        return;
    }

    if (!pTLSlide->m_instances)
    {
        return;
    }

    {
        TLInstance* curr = pTLSlide->m_instances->m_next;
        TLInstance* next;

        while (true)
        {
            next = curr->m_next;
            nlFloatColour colour = s_currentAssetColour;

            RenderTimeLineAsset(curr, pTLSlide->GetCurrentTime());

            s_currentAssetColour = colour;

            if (curr == pTLSlide->m_instances)
            {
                break;
            }

            curr = next;
        }
    }
}

/**
 * Offset/Address/Size: 0x528 | 0x8020A7B0 | size: 0x42C
 */
void FERender::RenderComponentInstance(TLComponentInstance* componentInstance)
{
    TLComponent* component = (TLComponent*)componentInstance->GetLibRefObject();
    if (!component)
    {
        return;
    }

    if (!component->GetActiveSlide())
    {
        return;
    }

    RenderSlide(component->GetActiveSlide());
}

/**
 * Offset/Address/Size: 0x4F0 | 0x8020A778 | size: 0x38
 */
void FERender::RenderPresentation(const FEPresentation* presentation)
{
    if (presentation == nullptr)
    {
        return;
    }

    if (presentation->m_slides == nullptr)
    {
        return;
    }

    RenderSlide(presentation->m_currentSlide);
}

/**
 * Offset/Address/Size: 0x48C | 0x8020A714 | size: 0x64
 */
void FERender::RenderScene(FEScene* scene)
{
    if (scene == nullptr)
    {
        return;
    }

    m_pRenderScene = scene;
    m_pMatrixStack->LoadIdentity();

    s_currentAssetColour.c[0] = 1.0f;
    s_currentAssetColour.c[1] = 1.0f;
    s_currentAssetColour.c[2] = 1.0f;
    s_currentAssetColour.c[3] = 1.0f;

    FEPresentation* presentation = scene->m_pFEPackage->GetPresentation();
    RenderPresentation(presentation);

    m_pRenderScene = nullptr;
}

/**
 * Offset/Address/Size: 0x3BC | 0x8020A644 | size: 0xD0
 */
void FERender::RenderTextInstance(TLTextInstance* textInstance)
{
    nlMatrix4 combinedMatrix;

    m_pMatrixStack->GetTop(combinedMatrix);

    nlMultMatrices(combinedMatrix, combinedMatrix, m_pRenderScene->GetCameraMatrix());

    textInstance->SetMatrix(&combinedMatrix);

    nlColour colour;
    ConvertColour(colour, s_currentAssetColour);

    textInstance->Render(m_pRenderScene->GetRenderView(), colour);
}

static inline uintptr_t glAllocSetMatrix(const nlMatrix4& matrix)
{
    uintptr_t handle = glAllocMatrix();
    if (handle != 0xFFFFFFFF)
    {
        glSetMatrix(handle, matrix);
    }
    return handle;
}

/**
 * Offset/Address/Size: 0x0 | 0x8020A288 | size: 0x3BC
 */
unsigned char FERender::RenderImageInstance(const TLImageInstance* pTLImageInstance)
{
    uintptr_t textureHandle;

    nlColour colour = pTLImageInstance->GetAssetColour();

    const FEImage* pFEImage = (const FEImage*)pTLImageInstance->m_component;
    FETextureResource* pTexRes = pFEImage->m_pFeTextureResource;

    if (!pTexRes->IsValid())
        return 1;

    ConvertColour(colour, s_currentAssetColour);

    textureHandle = pTexRes->GetTextureHandle();
    if (!textureHandle)
        return 1;

    nlMatrix4 matTM;
    m_pMatrixStack->GetTop(matTM);

    // PORT: widen the elements that were authored to span the frame.
    float portEdgeExtend = 1.0f;
    {
        const float designWidth = glGetOrthographicHeight() * (4.0f / 3.0f);
        const float frameWidth = glGetOrthographicWidth();
        if (frameWidth > designWidth && designWidth > 0.0f)
        {
            // The quad below is +/-50 in local space, so this instance covers 100 * xScale units of the 2D space.
            float xScale = matTM.e2[0][0];
            if (xScale < 0.0f)
                xScale = -xScale;

            // Only STRIKERS_PROBE_WIDEN reads the height, but it reads it for
            // every instance the rule judges: whether an element is a picture
            // or a bar is not visible in the width alone, and that question
            // has been asked twice now.
            float yScale = matTM.e2[1][1];
            if (yScale < 0.0f)
                yScale = -yScale;

            if (100.0f * xScale >= designWidth * 0.99f)
            {
                // ... and only when there is nothing left in the texture to distort.
                const PlatTexture* pTex = glx_GetTex(textureHandle, false, false);
                const float texWidth = (pTex != NULL) ? (float)pTex->m_Width : 0.0f;
                const bool alreadyMagnified =
                    texWidth <= 0.0f || 100.0f * xScale > 2.0f * texWidth;


                // STRIKERS_PROBE_WIDEN names every instance this rule judges and which way it went.
                {
                    static int s_probe = -1;
                    if (s_probe < 0)
                        s_probe = getenv("STRIKERS_PROBE_WIDEN") != NULL;
                    if (s_probe)
                    {
                        enum { kMax = 128 };
                        static float s_seen[kMax][2];
                        static int s_count;
                        const float w = 100.0f * xScale;
                        // Rounded, because two instances of the same background differ in the last bits of the float and would otherwise each get a line.
                        const float wKey = (float)(int)(w + 0.5f);
                        int i;
                        int seen = 0;

                        for (i = 0; i < s_count; ++i)
                            if (s_seen[i][0] == wKey && s_seen[i][1] == texWidth)
                                seen = 1;
                        if (!seen && s_count < kMax)
                        {
                            s_seen[s_count][0] = wKey;
                            s_seen[s_count][1] = texWidth;
                            s_count++;
                            OSReport("[widen] quad %.0f x %.0f from tex %.0f"
                                     " = %.2fx, %s\n",
                                     (double)w, (double)(100.0f * yScale),
                                     (double)texWidth,
                                     (double)(texWidth > 0.0f ? w / texWidth : 0.0f),
                                     alreadyMagnified ? "stretched" : "centred");
                        }
                    }
                }

                if (alreadyMagnified)
                {
                    const float widen = frameWidth / designWidth;
                    matTM.e2[0][0] *= widen;
                    matTM.e2[0][1] *= widen;
                    matTM.e2[0][2] *= widen;
                }
                else if (textureHandle != portMovieTex)
                {
                    // Centred, and the rest of the frame filled with this image's own edge colour rather than left black.
                    portEdgeExtend = frameWidth / designWidth;
                }
                // ... except for a decoded video frame, which is picture all the way to its own edge.
            }
        }
    }

    nlMultMatrices(matTM, matTM, m_pRenderScene->m_matView);

    uintptr_t matrixHandle = glAllocSetMatrix(matTM);

    nlVector2 pos[4];
    nlVector2 uv[4];

    // PORT: x and u are extended together by portEdgeExtend (1 unless this instance is being centred in a wider frame).
    const float portU0 = 0.5f + (0.0078125f - 0.5f) * portEdgeExtend;
    const float portU1 = 0.5f + (0.9921875f - 0.5f) * portEdgeExtend;
    const float portX = 50.0f * portEdgeExtend;

    uv[0].e[0] = portU0;
    uv[0].e[1] = 0.0078125f;
    uv[1].e[0] = portU0;
    uv[1].e[1] = 0.9921875f;
    uv[2].e[0] = portU1;
    uv[2].e[1] = 0.9921875f;
    uv[3].e[0] = portU1;
    uv[3].e[1] = 0.0078125f;

    pos[0].e[0] = -portX;
    pos[0].e[1] = 50.0f;
    pos[1].e[0] = -portX;
    pos[1].e[1] = -50.0f;
    pos[2].e[0] = portX;
    pos[2].e[1] = -50.0f;
    pos[3].e[0] = portX;
    pos[3].e[1] = 50.0f;

    glSetDefaultState(false);

    static unsigned char bAlpha;
    static signed char init;
    if (!init)
    {
        bAlpha = 1;
        init = 1;
    }

    glSetRasterState(GLS_Culling, 0);

    if (textureHandle != grabTex && bAlpha)
    {
        glSetRasterState(GLS_AlphaBlend, 1);
        glSetRasterState(GLS_AlphaTest, 1);
        glSetRasterState(GLS_AlphaTestRef, 0);
    }

    glSetTextureState(GLTS_DiffuseWrap, 3);

    glSetCurrentRasterState(glHandleizeRasterState());
    glSetCurrentTextureState(glHandleizeTextureState());

    glSetCurrentTexture(textureHandle, GLTT_Diffuse);

    static int stripmap[4] = { 3, 0, 2, 1 };
    static int quadmap[4] = { 0, 1, 2, 3 };

    eGLStream streams[] = { GLStream_Position, GLStream_Colour, GLStream_Diffuse };

    GLMeshWriter meshWriter;

    unsigned long program;
    uintptr_t matrix;              // PORT: a matrix handle
    eGLPrimitive prim;
    int* pMap;
    unsigned long texconfig;

    texconfig = gl_GetCurrentStateBundle()->texconfig;
    program = glSetCurrentProgram(drawQuadProgram);
    matrix = glSetCurrentMatrix(matrixHandle);

    if (glHasQuads())
    {
        pMap = quadmap;
        prim = GLP_QuadList;
    }
    else
    {
        pMap = stripmap;
        prim = GLP_TriStrip;
    }

    if (meshWriter.Begin(4, prim, texconfig + 2, streams, false))
    {
        for (int i = 0; i < 4; i++)
        {
            int index = pMap[i];
            meshWriter.Colour(colour);
            if (texconfig)
                meshWriter.Texcoord(uv[index]);
            meshWriter.Position(pos[index].e[0], pos[index].e[1], 0.0f);
        }

        if (!meshWriter.End())
            return 0;

        glViewAttachModel(m_pRenderScene->GetRenderView(), 0, meshWriter.GetModel());
    }
    else
    {
        return 0;
    }

    glSetCurrentProgram(program);
    glSetCurrentMatrix(matrix);

    return 1;
}
