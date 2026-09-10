#include "Game/Drawable/DrawableSkinModel.h"
#include "Game/World/worldanim.h"
#include "Game/World.h"
#include "NL/gl/glModel.h"
#include "NL/gl/gl.h"
#include "NL/gl/glState.h"

// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
bool g_bEnableDrawableSkinModel = true;
static bool g_bSkinModelTextureLighting = true;
const u32 GLTT_BumpLocal_bit = 1 << (int)GLTT_BumpLocal;

const unsigned long UnlitProgram = glGetProgram("3d unlit");
const unsigned long LitProgram = glGetProgram("3d pointlit");
const unsigned long LightTexture = glGetTexture("global/lightramp");
const unsigned long BlackTexture = glGetTexture("global/black");
const unsigned long WhiteTexture = glGetTexture("global/white");

uintptr_t glAllocMatrix();
void glSetMatrix(uintptr_t matrix, const nlMatrix4& m);   // PORT: matches NL/gl/glMatrix.h

static inline void ApplySkinModelTextureLighting(glModelPacket* pDup)
{
    pDup->state.texconfig |= GLTT_BumpLocal_bit;
    pDup->state.texture[5] = LightTexture;
}

/**
 * Offset/Address/Size: 0x0 | 0x80122D5C | size: 0x60
 */
DrawableSkinModel::~DrawableSkinModel()
{
}

/**
 * Offset/Address/Size: 0x60 | 0x80122DBC | size: 0x170
 */
void DrawableSkinModel::Draw()
{
    if (!g_bEnableDrawableSkinModel)
    {
        return;
    }

    if (m_pAnimController == NULL)
    {
        return;
    }

    glModel* pModel;
    u8 isLit = (u8)((m_uObjectCreationFlags >> 7) & 1) ^ 1;
    if (isLit)
    {
        void* pLightData;
        if (g_bSkinModelTextureLighting)
        {
            pLightData = m_pWorldContext->m_pIntensityPerm;
        }
        else
        {
            pLightData = m_pWorldContext->m_pLightData;
        }
        pModel = m_pAnimController->GetUpdatedModel(LitProgram, pLightData);
    }
    else
    {
        pModel = m_pAnimController->GetUpdatedModel(UnlitProgram, NULL);
    }

    uintptr_t matrixHandle;        // PORT: a matrix handle
    if (m_pAnimController->m_bIsGanged)
    {
        const nlMatrix4& worldMat = GetWorldMatrix();
        matrixHandle = glAllocMatrix();
        if ((u32)(matrixHandle + 0x10000) != 0xFFFF)
        {
            glSetMatrix(matrixHandle, worldMat);
        }
    }

    glModelPacket* pPacket = pModel->packets;
    while (pPacket < pModel->packets + pModel->numPackets)
    {
        glModelPacket* pDup = glModelPacketDup(pPacket, true);

        if (m_pAnimController->m_bIsGanged)
        {
            pDup->state.matrix = matrixHandle;
        }

        if (g_bSkinModelTextureLighting)
        {
            ApplySkinModelTextureLighting(pDup);
        }

        glViewAttachPacket((eGLView)6, pDup);
        pPacket++;
    }
}

/**
 * Offset/Address/Size: 0x1D0 | 0x80122F2C | size: 0x24
 */
void DrawableSkinModel::SetAnimation(const char* szAnimationName, unsigned long playMode)
{
    m_pAnimController->SetAnimation(szAnimationName, (ePlayMode)playMode);
}

/**
 * Offset/Address/Size: 0x1F4 | 0x80122F50 | size: 0xC
 */
void DrawableSkinModel::SetAnimationSpeed(float fSpeed)
{
    m_pAnimController->m_fSpeed = fSpeed;
}

/**
 * Offset/Address/Size: 0x200 | 0x80122F5C | size: 0x24
 */
float DrawableSkinModel::GetAnimationTime()
{
    return m_pAnimController->GetAnimationTime();
}

/**
 * Offset/Address/Size: 0x224 | 0x80122F80 | size: 0x24
 */
void DrawableSkinModel::SetAnimationTime(float fTime)
{
    m_pAnimController->SetAnimationTime(fTime);
}

/**
 * Offset/Address/Size: 0x248 | 0x80122FA4 | size: 0x24
 */
float DrawableSkinModel::GetAnimationDuration()
{
    return m_pAnimController->GetAnimationDuration();
}
