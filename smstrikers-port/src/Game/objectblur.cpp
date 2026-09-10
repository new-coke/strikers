#include "Game/ObjectBlur.h"

#include "NL/nlMath.h"
#include "NL/nlString.h"
#include "NL/nlMemory.h"

#include "Game/Camera/CameraMan.h"
#include "Game/GL/gluMeshWriter.h"

#include "NL/gl/glState.h"
#include "NL/gl/glMatrix.h"

#include "Game/ReplayManager.h"

BlurHandler* BlurManager::m_activeBlurHandler = NULL;

SlotPool<BlurHandler> BlurHandler::m_BlurHandlerSlotPool(0x10, 0x10);

static f32 fFlimmerOffset = 0.01f;

static const nlColour kWhite = { 0xFF, 0xFF, 0xFF, 0xFF };

/**
 * Offset/Address/Size: 0x0 | 0x801627D4 | size: 0x3C
 */
void BlurManager::Shutdown()
{
    nlDeleteDLRing<BlurHandler>(&m_activeBlurHandler);
    m_activeBlurHandler = NULL;
    SlotPoolBase::BaseFreeBlocks(&BlurHandler::m_BlurHandlerSlotPool, sizeof(BlurHandler));
}

/**
 * Offset/Address/Size: 0x3C | 0x80162810 | size: 0x128
 */
void BlurManager::Update(float deltaTime)
{
    if (m_activeBlurHandler == NULL)
        return;

    bool isEnd;
    BlurHandler* current = nlDLRingGetStart<BlurHandler>(m_activeBlurHandler);
    do
    {
        isEnd = nlDLRingIsEnd<BlurHandler>(m_activeBlurHandler, current);
        BlurHandler* next = current->m_next;

        if (current->m_bDying)
        {
            current->m_fDyingTimer -= deltaTime;
            if (current->m_fDyingTimer <= 0.0f)
            {
                current->m_fDyingTimer = 0.0f;
            }
        }

        bool shouldRemove;
        if (current->m_bDying && current->m_fDyingTimer <= 0.0f)
        {
            shouldRemove = true;
        }
        else
        {
            shouldRemove = false;
        }

        if (shouldRemove)
        {
            nlDLRingRemove<BlurHandler>(&m_activeBlurHandler, current);
            if (current != NULL)
            {
                delete[] current->m_pointRingBuffer;
                BlurHandler::m_BlurHandlerSlotPool.Free(current);
            }
        }
        else
        {
            current->RenderMesh(current->m_uTexHashID);
        }

        current = next;
    } while (!isEnd);
}

/**
 * Offset/Address/Size: 0x164 | 0x80162938 | size: 0x78
 */
void BlurManager::DestroyHandler(BlurHandler* handler, float timeToDie)
{
    if (timeToDie == 0.0f)
    {
        nlDLRingRemove<BlurHandler>(&m_activeBlurHandler, handler);
        if (handler != NULL)
        {
            delete[] handler->m_pointRingBuffer;
            BlurHandler::m_BlurHandlerSlotPool.Free(handler);
        }
    }
    else
    {
        handler->m_bDying = true;
        handler->m_fDyingTimer = timeToDie;
        handler->m_fTimeToDie = timeToDie;
    }
}

/**
 * Offset/Address/Size: 0x1DC | 0x801629B0 | size: 0xD0
 */
BlurHandler* BlurManager::GetNewHandler(const char* szTextureName, float fLineWidth, int maxPositionEntries, bool bAdditive)
{
    BlurHandler* handler = nullptr;

    BlurHandler::m_BlurHandlerSlotPool.Allocate(handler);

    if (handler != nullptr)
    {
        handler->m_bAdditive = bAdditive;
        handler->m_fLineWidth = fLineWidth;
        handler->m_pLastPoint = nullptr;
        handler->m_bDying = false;
        handler->m_maxPositionEntries = maxPositionEntries;
        handler->m_nInsertIndex = 0;
        handler->m_nTrailEndPointer = 0;
        handler->m_uTexHashID = nlStringLowerHash(szTextureName);
        handler->m_pointRingBuffer = (BlurPointEntry*)nlMalloc(maxPositionEntries * sizeof(BlurPointEntry), 8, false);
    }

    nlDLRingAddEnd<BlurHandler>(&m_activeBlurHandler, handler);

    return handler;
}

/**
 * Offset/Address/Size: 0x2AC | 0x80162A80 | size: 0x514
 */
void BlurHandler::RenderMesh(unsigned long uTexID)
{
    u32 count;
    u32 uPointIndex;
    uintptr_t matHandle;   // PORT: a GLMatrix address
    f32 nonAdditiveAlpha;
    BlurPointEntry* BPEntry;

    count = 0;
    if (m_nTrailEndPointer != m_nInsertIndex)
    {
        if (m_nTrailEndPointer < m_nInsertIndex)
        {
            count = m_nInsertIndex - m_nTrailEndPointer;
        }
        else
        {
            count = m_maxPositionEntries - m_nTrailEndPointer;
            count += m_nInsertIndex;
        }
    }

    if (count < 2)
    {
        return;
    }

    eGLStream stream_decl[3] = { GLStream_Position, GLStream_Colour, GLStream_Diffuse };

    GLMeshWriter mesh;

    glSetDefaultState(true);

    nlMatrix4 matWorld;
    matWorld.SetIdentity();
    matWorld.m43 += fFlimmerOffset;

    matHandle = glAllocMatrix();
    if (matHandle != 0xFFFFFFFF)   // PORT: see glAllocMatrix
    {
        glSetMatrix(matHandle, matWorld);
    }
    glSetCurrentMatrix(matHandle);

    if (m_bAdditive)
    {
        glSetRasterState(GLS_AlphaBlend, 2);
    }
    else
    {
        glSetRasterState(GLS_AlphaBlend, 1);
    }
    glSetRasterState(GLS_AlphaTest, 1);
    glSetRasterState(GLS_AlphaTestRef, 0);
    glSetRasterState(GLS_DepthWrite, 0);
    glSetRasterState(GLS_Culling, 0);
    glSetCurrentRasterState(glHandleizeRasterState());
    glSetCurrentTexture(uTexID, GLTT_Diffuse);
    glSetCurrentProgram(glGetProgram("3d unlit"));

    nlColour colour = kWhite;
    nonAdditiveAlpha = 0.0f;

    if (m_bAdditive)
    {
        nlColourSet(colour, 0xFF, 0xFF, 0xFF, 0xFF);
        if (m_bDying)
        {
            int fadeVal = 255.0f * (m_fDyingTimer / m_fTimeToDie);
            nlColourSeReversed(colour, fadeVal, fadeVal, fadeVal, fadeVal);
        }
    }
    else if (m_bDying)
    {
        nonAdditiveAlpha = 90.0f * (m_fDyingTimer / m_fTimeToDie);
    }
    else
    {
        nonAdditiveAlpha = 90.0f;
    }

    uPointIndex = m_nTrailEndPointer;
    f32 texU = 0.0f;
    f32 texUIncrement = 1.0f / (f32)count;

    if (mesh.Begin(count * 2, GLP_TriStrip, 3, stream_decl, false))
    {
        for (u32 i = 0; i < count; i++)
        {
            BPEntry = &m_pointRingBuffer[uPointIndex];
            if (!m_bAdditive)
            {
                colour.c[3] = nonAdditiveAlpha * texU;
            }

            if ((i == 0 || i == count - 1) && !m_bDying)
            {
                int pointIndexA, pointIndexB;
                if (i == 0)
                {
                    pointIndexB = uPointIndex + 1;
                    pointIndexA = uPointIndex;
                    if (pointIndexB == m_maxPositionEntries)
                    {
                        pointIndexB = 0;
                    }
                }
                else
                {
                    pointIndexA = uPointIndex - 1;
                    pointIndexB = uPointIndex;
                    if (pointIndexA < 0)
                    {
                        pointIndexA = m_maxPositionEntries - 1;
                    }
                }

                f32 blendPct = ReplayManager::Instance()->mRender->mFrameBlendPercent;
                f32 invBlend = 1.0f - blendPct;

                nlVector3 v3Top;
                nlVector3 v3Bottom;
                nlVec3WeightedSum(v3Top, invBlend, m_pointRingBuffer[pointIndexA].v3Top, blendPct, m_pointRingBuffer[pointIndexB].v3Top);
                nlVec3WeightedSum(v3Bottom, invBlend, m_pointRingBuffer[pointIndexA].v3Bottom, blendPct, m_pointRingBuffer[pointIndexB].v3Bottom);

                mesh.Colour(colour);
                nlVector2 tc0;
                tc0.x = texU;
                tc0.y = 0.0f;
                ((GLMeshWriterCore*)&mesh)->Texcoord(tc0);
                mesh.Vertex(v3Top);

                mesh.Colour(colour);
                nlVector2 tc1;
                tc1.x = texU;
                tc1.y = 1.0f;
                ((GLMeshWriterCore*)&mesh)->Texcoord(tc1);
                mesh.Vertex(v3Bottom);
            }
            else
            {
                mesh.Colour(colour);
                nlVector2 tc0;
                tc0.x = texU;
                tc0.y = 0.0f;
                ((GLMeshWriterCore*)&mesh)->Texcoord(tc0);
                mesh.Vertex(BPEntry->v3Top);

                mesh.Colour(colour);
                nlVector2 tc1;
                tc1.x = texU;
                tc1.y = 1.0f;
                ((GLMeshWriterCore*)&mesh)->Texcoord(tc1);
                mesh.Vertex(BPEntry->v3Bottom);
            }

            uPointIndex++;
            if (uPointIndex == (u32)m_maxPositionEntries)
            {
                uPointIndex = 0;
            }
            texU += texUIncrement;
        }

        if (!mesh.End())
        {
            return;
        }
        glViewAttachModel(GLV_Particles, 5, mesh.GetModel());
    }
}

/**
 * Offset/Address/Size: 0x7C0 | 0x80162F94 | size: 0x14
 */
void BlurHandler::Die(float timeToDie)
{
    m_bDying = true;
    m_fDyingTimer = timeToDie;
    m_fTimeToDie = timeToDie;
}

/**
 * Offset/Address/Size: 0x7D4 | 0x80162FA8 | size: 0x1B0
 */
void BlurHandler::AddViewOrientedPoint(const nlVector3& position, const nlVector3& forwardVector)
{
    nlVector3 topPoint, bottomPoint;

    if (ConstructViewOrientedPoints(topPoint, bottomPoint, position, forwardVector))
    {
        m_pointFinal.v3Top = topPoint;
        m_pointFinal.v3Bottom = bottomPoint;

        if (m_pLastPoint != nullptr)
        {
            nlVector3 delta;
            nlVec3Set(delta, m_pLastPoint->v3Top.x - m_pointFinal.v3Top.x, m_pLastPoint->v3Top.y - m_pointFinal.v3Top.y, m_pLastPoint->v3Top.z - m_pointFinal.v3Top.z);

            if ((delta.x * delta.x) + (delta.y * delta.y) + (delta.z * delta.z) < 0.0025000002f)
            {
                return;
            }
        }

        BlurPointEntry* entry = &m_pointRingBuffer[m_nInsertIndex];
        entry->v3Top = m_pointFinal.v3Top;
        entry->v3Bottom = m_pointFinal.v3Bottom;

        m_pLastPoint = &m_pointRingBuffer[m_nInsertIndex];
        m_nInsertIndex = m_nInsertIndex + 1;
        m_nInsertIndex %= m_maxPositionEntries;

        if (m_nInsertIndex == m_nTrailEndPointer)
        {
            m_nTrailEndPointer = m_nTrailEndPointer + 1;
            m_nTrailEndPointer %= m_maxPositionEntries;
        }
    }
    else
    {
        if (m_nInsertIndex != m_nTrailEndPointer)
        {
            m_nTrailEndPointer = m_nTrailEndPointer + 1;
            m_nTrailEndPointer %= m_maxPositionEntries;
        }
    }
}

/**
 * Offset/Address/Size: 0x984 | 0x80163158 | size: 0x1C4
 */
bool BlurHandler::ConstructViewOrientedPoints(nlVector3& topPoint, nlVector3& bottomPoint, nlVector3 position, const nlVector3& forwardVector)
{
    nlVector3 viewVector;
    float perpX;
    float perpY;
    float perpZ;

    float sLen1 = nlVec3LengthSquared(forwardVector);
    if (sLen1 < 0.01f)
    {
        return false;
    }

    float invLen = nlRecipSqrt(sLen1, 1);
    float normX;
    float normZ;
    float normY;
    normZ = invLen * forwardVector.z;
    normY = invLen * forwardVector.y;
    normX = invLen * forwardVector.x;

    cCameraManager::GetViewVector(viewVector);
    if (viewVector.x * normX + viewVector.y * normY + viewVector.z * normZ < 0.99f)
    {
        float crossX = (normY * viewVector.z) - (normZ * viewVector.y);
        float crossY = (-normX * viewVector.z) + (normZ * viewVector.x);
        float crossZ = (normX * viewVector.y) - (normY * viewVector.x);

        float invLen2 = nlRecipSqrt((crossZ * crossZ) + ((crossX * crossX) + (crossY * crossY)), 1);

        float width = m_fLineWidth;
        perpX = width * (invLen2 * crossX);
        perpY = width * (invLen2 * crossY);
        perpZ = width * (invLen2 * crossZ);
    }
    else
    {
        if (m_pLastPoint != NULL)
        {
            perpX = 0.5f * (m_pLastPoint->v3Top.x - m_pLastPoint->v3Bottom.x);
            perpY = 0.5f * (m_pLastPoint->v3Top.y - m_pLastPoint->v3Bottom.y);
            perpZ = 0.5f * (m_pLastPoint->v3Top.z - m_pLastPoint->v3Bottom.z);
        }
        else
        {
            return false;
        }
    }

    nlVec3Set(topPoint, position.x + perpX, position.y + perpY, position.z + perpZ);
    nlVec3Set(bottomPoint, position.x - perpX, position.y - perpY, position.z - perpZ);

    return true;
}
