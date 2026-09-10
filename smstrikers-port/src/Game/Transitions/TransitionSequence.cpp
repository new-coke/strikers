#include "Game/Transitions/TransitionSequence.h"

#include "strtold.h"

#include "NL/nlString.h"

#include "Game/Sys/audio.h"

#include "Game/Transitions/ScriptedTransition.h"
#include "Game/Transitions/ColourBlendScreenTransition.h"
#include "Game/Transitions/ModelTransition.h"

/**
 * Offset/Address/Size: 0xB0C | 0x80208ED4 | size: 0x38
 */
TransitionSequence::TransitionSequence()
{
    m_pTransitions = NULL;
    m_nNumTransitions = 0;
    m_pPlaying = NULL;
    m_pSound = NULL;
    m_CutAt = -1.0f;
}

/**
 * Offset/Address/Size: 0x9EC | 0x80208DB4 | size: 0x120
 */
TransitionSequence::~TransitionSequence()
{
    if (m_pTransitions != nullptr)
    {
        for (int i = 0; i < m_nNumTransitions; i++)
        {
            delete m_pTransitions[i];
            m_pTransitions[i] = nullptr;

            delete[] m_pSound[i].soundStr;
            m_pSound[i].soundStr = nullptr;
        }

        delete[] m_pTransitions;
        m_pTransitions = nullptr;

        delete[] m_pPlaying;
        m_pPlaying = nullptr;

        delete[] m_pEarly;
        m_pEarly = nullptr;

        delete[] m_pSound;
        m_pSound = nullptr;
    }
}

/**
 * Offset/Address/Size: 0x974 | 0x80208D3C | size: 0x78
 */
void TransitionSequence::DoSanityCheck()
{
    for (int i = 0; i < m_nNumTransitions; i++)
    {
        if (m_pTransitions[i] != nullptr)
        {
            m_pTransitions[i]->DoSanityCheck();
        }
    }
}

/**
 * Offset/Address/Size: 0x718 | 0x80208AE0 | size: 0x25C
 */
void TransitionSequence::Update(float dt)
{
    for (int i = 0; i < m_nNumTransitions; i++)
    {
        if (m_pPlaying[i] == 1)
        {
            m_pTransitions[i]->Update(dt);

            int next = i + 1;
            bool isFinished = m_pTransitions[i]->IsFinished();
            if (!isFinished)
            {
                if (next >= m_nNumTransitions)
                {
                    continue;
                }

                if (!((1.0f - m_pEarly[next]) < m_pTransitions[i]->Time()))
                {
                    continue;
                }
            }

            isFinished = m_pTransitions[i]->IsFinished();

            if (isFinished)
            {
                m_pTransitions[i]->Cancel();
                m_pPlaying[i] = 2;
            }

            if (next < m_nNumTransitions)
            {
                ScreenTransitionManager* manager = ScreenTransitionManager::s_pInstance;
                if (manager->m_pCallback != nullptr)
                {
                    manager->m_pCallback->SequenceSwitch();
                }

                if (m_pPlaying[next] == 0)
                {
                    m_pTransitions[next]->Reset();
                    m_pTransitions[next]->Update(0.0f);
                    m_pPlaying[next] = 1;

                    if (m_pSound[next].soundStr != nullptr)
                    {
                        Audio::PlayWorldSFXbyStr(
                            m_pSound[next].soundStr,
                            100.0f,
                            -1.0f,
                            false,
                            true,
                            nullptr,
                            nullptr,
                            nullptr);
                    }
                }
            }
        }
    }

    float progress = m_Time / GetTransitionLength();

    m_Time += dt;

    float newProgress = m_Time / GetTransitionLength();

    if ((progress <= m_CutAt) && (newProgress > m_CutAt))
    {
        ScreenTransitionManager::Instance()->m_Cut = true;
    }
}

/**
 * Offset/Address/Size: 0x650 | 0x80208A18 | size: 0xC8
 */
void TransitionSequence::Reset()
{
    for (int i = 0; i < m_nNumTransitions; i++)
    {
        m_pPlaying[i] = 0;
        m_pTransitions[i]->Reset();
    }

    if (m_pSound[0].soundStr != nullptr)
    {
        Audio::PlayWorldSFXbyStr(m_pSound[0].soundStr, 100.0f, -1.0f, false, true, nullptr, nullptr, nullptr);
    }

    m_pPlaying[0] = 1;
    m_Time = 0.0f;
}

/**
 * Offset/Address/Size: 0x5BC | 0x80208984 | size: 0x94
 */
void TransitionSequence::Render(eGLView glView)
{
    for (int i = 0; i < m_nNumTransitions; i++)
    {
        if (m_pPlaying[i] == 1)
        {
            m_pTransitions[i]->Render(glView);
        }
    }
}

/**
 * Offset/Address/Size: 0x518 | 0x802088E0 | size: 0xA4
 */
void TransitionSequence::Cancel()
{
    for (int i = 0; i < m_nNumTransitions; i++)
    {
        m_pTransitions[i]->Cancel();

        if (m_pSound[i].soundStr != nullptr && m_pSound[i].stopAtEnd)
        {
            Audio::StopWorldSFXbyStr(m_pSound[i].soundStr);
        }
    }
}

/**
 * Offset/Address/Size: 0x480 | 0x80208848 | size: 0x98
 */
float TransitionSequence::GetTransitionLength()
{
    float totalLength = 0.0f;

    for (int i = 0; i < m_nNumTransitions; i++)
    {
        float transitionLength = m_pTransitions[i]->GetTransitionLength();
        totalLength += transitionLength;

        totalLength -= m_pEarly[i];
    }

    return totalLength;
}

/**
 * Offset/Address/Size: 0x478 | 0x80208840 | size: 0x8
 */
float TransitionSequence::Time() const
{
    return 0.0f;
}

/**
 * Offset/Address/Size: 0x438 | 0x80208800 | size: 0x40
 */
bool TransitionSequence::IsFinished()
{
    for (int i = 0; i < m_nNumTransitions; i++)
    {
        if (m_pPlaying[i] != 2)
        {
            return false;
        }
    }

    return true;
}
/**
 * Offset/Address/Size: 0x0 | 0x802083C8 | size: 0x438
 */
void TransitionSequence::Initialize(SimpleParser* parser)
{
    ScreenTransition* pTrans[20];
    float pEarly[20] = { };
    TransitionSounds sound[20] = { };

    m_nNumTransitions = 0;

    char* token = parser->NextToken(true);
    while (token != nullptr)
    {
        if (nlStrCmp<char>(token, "end") == 0)
            break;

        if (nlStrCmp<char>(token, "colourblend") == 0)
        {
            ColourBlendScreenTransition* transition = ColourBlendScreenTransition::GetFromParser(parser);
            pTrans[m_nNumTransitions] = transition;
            m_nNumTransitions++;
        }
        else if (nlStrCmp<char>(token, "transition") == 0)
        {
            ScriptedScreenTransition* transition = new (nlMalloc(sizeof(ScriptedScreenTransition), 8, false)) ScriptedScreenTransition();
            transition->InitializeFromParser(parser);
            pTrans[m_nNumTransitions] = transition;
            m_nNumTransitions++;
        }
        else if (nlStrCmp<char>(token, "early") == 0)
        {
            pEarly[m_nNumTransitions] = atof(parser->NextTokenOnLine(true));
        }
        else if (nlStrCmp<char>(token, "sound") == 0 || nlStrCmp<char>(token, "soundstopatend") == 0)
        {
            char* soundName = parser->NextTokenOnLine(true);
            int soundNameLen = nlStrLen<char>(soundName);

            sound[m_nNumTransitions].soundStr = (char*)nlMalloc(soundNameLen + 1, 8, false);
            nlStrNCpy<char>(sound[m_nNumTransitions].soundStr, soundName, soundNameLen + 1);

            if (nlStrCmp<char>(token, "soundstopatend") == 0)
            {
                sound[m_nNumTransitions].stopAtEnd = true;
            }
            else
            {
                sound[m_nNumTransitions].stopAtEnd = false;
            }
        }
        else if (nlStrCmp<char>(token, "model") == 0)
        {
            ModeledScreenTransition* transition = new (nlMalloc(sizeof(ModeledScreenTransition), 8, false)) ModeledScreenTransition();
            transition->LoadFromParser(parser);
            pTrans[m_nNumTransitions] = transition;
            m_nNumTransitions++;
        }
        else if (nlStrCmp<char>(token, "cut_at") == 0)
        {
            m_CutAt = atof(parser->NextTokenOnLine(true));
        }

        token = parser->NextToken(true);
    }

    m_pTransitions = (ScreenTransition**)nlMalloc(m_nNumTransitions * sizeof(ScreenTransition*), 8, false);
    memcpy(m_pTransitions, pTrans, m_nNumTransitions * sizeof(ScreenTransition*));
    m_pEarly = (float*)nlMalloc(m_nNumTransitions * sizeof(float), 8, false);
    memcpy(m_pEarly, pEarly, m_nNumTransitions * sizeof(float));
    m_pSound = (TransitionSounds*)nlMalloc(m_nNumTransitions * sizeof(TransitionSounds), 8, false);

    for (int i = 0; i < m_nNumTransitions; i++)
    {
        m_pSound[i].soundStr = nullptr;
        s32 len = nlStrLen<char>(sound[i].soundStr);
        if (len > 0)
        {
            m_pSound[i].soundStr = (char*)nlMalloc(len + 1, 8, false);
            nlStrNCpy<char>(m_pSound[i].soundStr, sound[i].soundStr, len + 1);
            m_pSound[i].stopAtEnd = sound[i].stopAtEnd;
            delete sound[i].soundStr;
        }
    }

    m_pPlaying = (char*)nlMalloc(m_nNumTransitions, 8, false);
    for (int i = 0; i < m_nNumTransitions; i++)
    {
        m_pPlaying[i] = 0;
    }
}
