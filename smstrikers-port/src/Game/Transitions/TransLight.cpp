#include "Game/Transitions/TransLight.h"

#include "NL/nlString.h"

/**
 * Offset/Address/Size: 0x658 | 0x8020956C | size: 0x128
 */
TransitionLight::TransitionLight()
{
    m_fStartScale = 0.0f;
    m_fEndScale = 0.0f;
    m_nDirLights = 0;
    m_nPointLights = 0;
    m_nDirType = 0xA;
    m_nAmbientType = 8;
    m_nPointType = 1;

    nlFloatColourSet(m_ScaledAmbient, 0.0f, 0.0f, 0.0f, 1.0f);
    nlFloatColourSet(m_Ambient, 0.0f, 0.0f, 0.0f, 1.0f);

    for (int i = 0; i < 3; i++)
    {
        nlVec3Set(m_Directionals[i].direction, 0.0f, 0.0f, 0.0f);
        nlFloatColourSet(m_Directionals[i].colour, 0.0f, 0.0f, 0.0f, 1.0f);
    }

    for (int i = 0; i < 3; i++)
    {
        nlVec3Set(m_PointLights[i].worldPosition, 0.0f, 0.0f, 0.0f);
        nlFloatColourSet(m_PointLights[i].colour, 0.0f, 0.0f, 0.0f, 1.0f);
        m_PointLights[i].intensity = 1.0f;
        m_PointLights[i].innerRadius = 0.0f;
        m_PointLights[i].outerRadius = 200.0f;
    }
}

inline float clamp_ge0(float x)
{
    if (x >= 0.0f)
        return x;
    else
        return 0.0f; // forces an else body -> unconditional branch to skip it
}

inline float clamp_le1(float x)
{
    if (x <= 1.0f)
        return x;
    else
        return 1.0f;
}

/**
 * Offset/Address/Size: 0x5E4 | 0x802094F8 | size: 0x74
 */
void TransitionLight::ApplyLight(float t)
{
    float cur = (m_fStartScale * (1.0f - t)) + (m_fEndScale * t);
    float fCurrentScale = clamp_le1(cur);
    fCurrentScale = clamp_ge0(fCurrentScale);
    float fInvCurScale = 1.0f - fCurrentScale;

    m_ScaledAmbient.c[0] = (m_Ambient.c[0] * fCurrentScale) + fInvCurScale;
    m_ScaledAmbient.c[1] = (m_Ambient.c[1] * fCurrentScale) + fInvCurScale;
    m_ScaledAmbient.c[2] = (m_Ambient.c[2] * fCurrentScale) + fInvCurScale;
    m_ScaledAmbient.c[3] = 1.0f;
}

/**
 * Offset/Address/Size: 0x164 | 0x80209078 | size: 0x480
 */
void TransitionLight::LoadFromParser(SimpleParser* parser)
{
    m_fStartScale = 1.0f;
    m_fEndScale = 1.0f;

    char* pToken = parser->NextToken(true);
    while (pToken != nullptr)
    {
        if (nlStrCmp<char>(pToken, "end") == 0)
            break;

        if (nlStrCmp<char>(pToken, "ambient") == 0)
        {
            pToken = parser->NextTokenOnLine(true);
            if (pToken)
            {
                float val = atof(pToken);
                m_Ambient.c[0] = val / 255.f;
            }

            pToken = parser->NextTokenOnLine(true);
            if (pToken)
            {
                float val = atof(pToken);
                m_Ambient.c[1] = val / 255.f;
            }

            pToken = parser->NextTokenOnLine(true);
            if (pToken)
            {
                float val = atof(pToken);
                m_Ambient.c[2] = val / 255.f;
            }
        }
        else if (nlStrCmp<char>(pToken, "directional") == 0)
        {
            pToken = parser->NextTokenOnLine(true);
            if (pToken)
                m_Directionals[m_nDirLights].direction.x = (float)atof(pToken);

            pToken = parser->NextTokenOnLine(true);
            if (pToken)
                m_Directionals[m_nDirLights].direction.y = (float)atof(pToken);

            pToken = parser->NextTokenOnLine(true);
            if (pToken)
                m_Directionals[m_nDirLights].direction.z = (float)atof(pToken);

            pToken = parser->NextTokenOnLine(true);
            if (pToken)
            {
                float val = atof(pToken);
                m_Directionals[m_nDirLights].colour.c[0] = val / 255.f;
            }

            pToken = parser->NextTokenOnLine(true);
            if (pToken)
            {
                float val = atof(pToken);
                m_Directionals[m_nDirLights].colour.c[1] = val / 255.f;
            }

            pToken = parser->NextTokenOnLine(true);
            if (pToken)
            {
                float val = atof(pToken);
                m_Directionals[m_nDirLights].colour.c[2] = val / 255.f;
            }

            nlVector3& d = m_Directionals[m_nDirLights].direction;
            float invLen = nlRecipSqrt((d.x * d.x) + (d.y * d.y) + (d.z * d.z), 1);
            nlVec3Set(d,
                invLen * d.x,
                invLen * d.y,
                invLen * d.z);

            m_nDirLights += 1;
        }
        else if (nlStrCmp<char>(pToken, "point") == 0)
        {

            pToken = parser->NextTokenOnLine(true);
            if (pToken)
                m_PointLights[m_nPointLights].worldPosition.x = (float)atof(pToken);

            pToken = parser->NextTokenOnLine(true);
            if (pToken)
                m_PointLights[m_nPointLights].worldPosition.y = (float)atof(pToken);

            pToken = parser->NextTokenOnLine(true);
            if (pToken)
                m_PointLights[m_nPointLights].worldPosition.z = (float)atof(pToken);

            // colour (scaled by 255.0)
            pToken = parser->NextTokenOnLine(true);
            if (pToken)
            {
                float val = atof(pToken);
                m_PointLights[m_nPointLights].colour.c[0] = val / 255.f;
            }

            pToken = parser->NextTokenOnLine(true);
            if (pToken)
            {
                float val = atof(pToken);
                m_PointLights[m_nPointLights].colour.c[1] = val / 255.f;
            }

            pToken = parser->NextTokenOnLine(true);
            if (pToken)
            {
                float val = atof(pToken);
                m_PointLights[m_nPointLights].colour.c[2] = val / 255.f;
            }

            pToken = parser->NextTokenOnLine(true);
            if (pToken)
                m_PointLights[m_nPointLights].outerRadius = atof(pToken);

            m_nPointLights += 1;
        }
        else if (nlStrCmp<char>(pToken, "scale") == 0)
        {
            pToken = parser->NextTokenOnLine(true);
            if (pToken)
                m_fStartScale = atof(pToken);

            pToken = parser->NextTokenOnLine(true);
            if (pToken)
                m_fEndScale = atof(pToken);
        }

        pToken = parser->NextToken(true);
    }
}

/**
 * Offset/Address/Size: 0xAC | 0x80208FC0 | size: 0xB8
 */
void TransitionLight::AttachToModel(glModel* pModel)
{
    for (u32 i = 0; i < pModel->numPackets; i++)
    {
        glUserAttach(&m_nAmbientType, &pModel->packets[i], true);
        if (m_nDirLights != 0)
        {
            glUserAttach(&m_nDirType, &pModel->packets[i], true);
        }
        if (m_nPointLights != 0)
        {
            glUserAttach(&m_nPointType, &pModel->packets[i], true);
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80208F14 | size: 0xAC
 */
void TransitionLight::DetachFromModel(glModel* pModel)
{
    for (u32 i = 0; i < pModel->numPackets; i++)
    {
        glUserDetach((eGLUserData)m_nAmbientType, &pModel->packets[i]);

        if (m_nDirLights != 0)
        {
            glUserDetach((eGLUserData)m_nDirType, &pModel->packets[i]);
        }

        if (m_nPointLights != 0)
        {
            glUserDetach((eGLUserData)m_nPointType, &pModel->packets[i]);
        }
    }
}
