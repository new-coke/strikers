#include "Game/Blinker.h"

#include "Game/NisPlayer.h"

#include "Game/Sys/debug.h"
#include "string.h"

#include "NL/nlPrint.h"
#include "NL/gl/glState.h"
#include "NL/gl/glTexture.h"

extern float RandomizedValue(float, float);

const float BlinkTimes[4] = { 1.25f, 0.016666667f, 0.033333335f, 0.016666667f };

/**
 * Offset/Address/Size: 0x0 | 0x8016A968 | size: 0x74
 */
void Blinker::Blink(glModel* model)
{
    const int id = (model->id == m_uModel0Hash) ? 0 : 1;
    if (m_pEyes[id])
    {
        for (u32 i = 0; i < m_pEyes[id]->numPackets; ++i)
        {
            int pkt = i + m_pEyes[id]->packetIndex;
            model->packets[pkt].state.texture[0] = m_Textures[(unsigned long)m_State];
        }
    }
}

/**
 * Offset/Address/Size: 0x74 | 0x8016A9DC | size: 0x130
 */
void Blinker::Update(float fDeltaT)
{
    float deltaTime = fDeltaT;
    if (NisPlayer::Instance()->WorldIsFrozen())
    {
        deltaTime = 0.0f;
    }
    m_fTime += deltaTime;

    if (m_fTime > m_fBlinkTimes[m_State])
    {
        m_fTime = 0.0f;
        switch (m_State)
        {
        case Blink_Open:
            m_State = Blink_HalfClosed;
            if (m_bJustDoubleBlinked == 0)
            {
                if (RandomizedValue(0.5f, 1.0f) < 0.2f)
                {
                    m_fBlinkTimes[0] = RandomizedValue(0.06666667f, 0.06666667f);
                    m_bJustDoubleBlinked = 1;
                    return;
                }
            }
            m_fBlinkTimes[0] = RandomizedValue(1.2f, 1.0f);
            m_bJustDoubleBlinked = 0;
            break;
        case Blink_HalfClosed:
            m_State = Blink_Closed;
            break;
        case Blink_Closed:
            m_State = Blink_HalfOpen;
            break;
        case Blink_HalfOpen:
            m_State = Blink_Open;
            break;
        }
    }
}

/**
 * Offset/Address/Size: 0x1A4 | 0x8016AB0C | size: 0x454
 */
Blinker::Blinker(const char* szBaseName, unsigned long model0Hash, GLMaterialList* mats0, GLMaterialList* mats1, unsigned long eyesHash)
{
    int pattern; // r28
    bool base0;  // r27
    bool found;  // r30

    m_bValid = 0;
    m_uModel0Hash = model0Hash;
    memcpy(&m_fBlinkTimes, BlinkTimes, sizeof(m_fBlinkTimes));

    m_pMats[0] = mats0;
    m_pMats[1] = mats1;
    m_uEyesHash = eyesHash;

    m_pEyes[0] = mats0->FindMaterial(eyesHash);

    if (m_pEyes[0] == 0)
    {
        tDebugPrintManager::Print(DC_RENDER, "%s has no eyes on material 0\n", szBaseName);
        return;
    }

    if (mats1 == 0)
    {
        m_pEyes[1] = 0;
    }
    else
    {
        m_pEyes[1] = mats1->FindMaterial(eyesHash);
        if (m_pEyes[1] == 0)
        {
            m_pMats[1] = 0;
        }
    }

    m_Textures[0] = -1;
    m_Textures[1] = -1;
    m_Textures[2] = -1;
    m_Textures[3] = -1;

    found = false;
    pattern = 0;

    for (;;)
    {
        char szTmpBuffer_1[0x80];
        switch (pattern)
        {
        case 0:
            nlSNPrintf(szTmpBuffer_1, 0x80, "%s/%s_eye_%d", szBaseName, szBaseName, 0);
            break;
        case 1:
            nlSNPrintf(szTmpBuffer_1, 0x80, "%s/%s_eye%d", szBaseName, szBaseName, 0);
            break;
        case 2:
            nlSNPrintf(szTmpBuffer_1, 0x80, "%s/%seye_%d", szBaseName, szBaseName, 0);
            break;
        }

        m_Textures[0] = glGetTexture(szTmpBuffer_1);
        if (glTextureLoad(m_Textures[0]))
        {
            base0 = true;
            found = true;
            break;
        }

        char szTmpBuffer_2[0x80];
        switch (pattern)
        {
        case 0:
            nlSNPrintf(szTmpBuffer_2, 0x80, "%s/%s_eye_%d", szBaseName, szBaseName, 1);
            break;
        case 1:
            nlSNPrintf(szTmpBuffer_2, 0x80, "%s/%s_eye%d", szBaseName, szBaseName, 1);
            break;
        case 2:
            nlSNPrintf(szTmpBuffer_2, 0x80, "%s/%seye_%d", szBaseName, szBaseName, 1);
            break;
        }

        m_Textures[0] = glGetTexture(szTmpBuffer_2);
        if (glTextureLoad(m_Textures[0]))
        {
            base0 = false;
            found = true;
            break;
        }

        ++pattern;
        if (pattern >= 3)
            break;
    }

    if (found)
    {
        int idx2 = (base0 ? -1 : 0) + 2;

        char szTmpBuffer_1[0x80];
        switch (pattern)
        {
        case 0:
            nlSNPrintf(szTmpBuffer_1, 0x80, "%s/%s_eye_%d", szBaseName, szBaseName, idx2);
            break;
        case 1:
            nlSNPrintf(szTmpBuffer_1, 0x80, "%s/%s_eye%d", szBaseName, szBaseName, idx2);
            break;
        case 2:
            nlSNPrintf(szTmpBuffer_1, 0x80, "%s/%seye_%d", szBaseName, szBaseName, idx2);
            break;
        }

        m_Textures[1] = glGetTexture(szTmpBuffer_1);

        int idx3 = (base0 ? -1 : 0) + 3;

        char szTmpBuffer_2[0x80];
        switch (pattern)
        {
        case 0:
            nlSNPrintf(szTmpBuffer_2, 0x80, "%s/%s_eye_%d", szBaseName, szBaseName, idx3);
            break;
        case 1:
            nlSNPrintf(szTmpBuffer_2, 0x80, "%s/%s_eye%d", szBaseName, szBaseName, idx3);
            break;
        case 2:
            nlSNPrintf(szTmpBuffer_2, 0x80, "%s/%seye_%d", szBaseName, szBaseName, idx3);
            break;
        }
        m_Textures[2] = glGetTexture(szTmpBuffer_2);
    }

    if (!found || !glTextureLoad(m_Textures[1]) || !glTextureLoad(m_Textures[2]))
    {
        tDebugPrintManager::Print(DC_RENDER, "Error: %s not properly configured for blinking.\n", szBaseName);
        return;
    }

    m_Textures[3] = m_Textures[1];
    m_fTime = 0.0f;
    m_State = Blink_Open;
    m_bJustDoubleBlinked = 0;
    m_bValid = 1;
}
