#include "Game/Transitions/ScreenTransitionManager.h"

#include "Game/Transitions/TransitionSequence.h"
#include "Game/Transitions/ColourBlendScreenTransition.h"
#include "Game/Transitions/ScriptedTransition.h"
#include "Game/Transitions/ModelTransition.h"

#include "Game/Sys/simpleparser.h"

#include "string.h"

#include "NL/nlMath.h"
#include "NL/nlString.h"

template <>
ScreenTransitionManager* nlSingleton<ScreenTransitionManager>::s_pInstance = 0;

/**
 * Offset/Address/Size: 0x9DC | 0x80205ACC | size: 0xA4
 */
ScreenTransitionManager::ScreenTransitionManager()
    : m_pActiveTransition(nullptr)
    , m_TransitionMap()
    , m_eView(GLV_Transitions)
    , m_pCallback(nullptr)
    , m_SelectedTransition(nullptr)
    , m_fCurrentTime(0.0f)
{
    m_Transitions.mData = nullptr;
    m_Transitions.mSize = 0;
    m_Transitions.mCapacity = 0;
    m_Cut = false;
    m_Transitions.reserve(16);
}

/**
 * Offset/Address/Size: 0x8FC | 0x802059EC | size: 0x80
 */
ScreenTransitionManager::~ScreenTransitionManager()
{
    DeleteAllTransitions();
}

/**
 * Offset/Address/Size: 0x7F4 | 0x802058E4 | size: 0x108
 */
void ScreenTransitionManager::Render(float dt)
{
    if (m_pActiveTransition)
    {
        m_pActiveTransition->Update(dt);

        if (m_pActiveTransition->IsFinished())
        {
            m_pActiveTransition->Cancel();
            m_pActiveTransition = 0;

            if (m_pCallback)
            {
                m_pCallback->TransitionFinished();
            }
        }
        else
        {
            m_fCurrentTime += dt;

            if (m_pCallback)
            {
                float curProgress = m_fCurrentLength;
                float progress = 0.0f;
                if (curProgress > 0.0f)
                {
                    progress = m_fCurrentTime / curProgress;
                }

                m_pCallback->TransitionProgressed(progress);
            }

            m_pActiveTransition->Render(m_eView);
        }
    }
}

/**
 * Offset/Address/Size: 0x78C | 0x8020587C | size: 0x68
 */
void ScreenTransitionManager::CancelAllTransitions()
{
    if (m_pActiveTransition != nullptr)
    {
        m_pActiveTransition->Cancel();

        if (m_pCallback != nullptr)
        {
            m_pCallback->TransitionFinished();
        }
    }
    m_pActiveTransition = nullptr;
}

/**
 * Offset/Address/Size: 0x71C | 0x8020580C | size: 0x70
 */
void ScreenTransitionManager::DeleteAllTransitions()
{
    if (m_pActiveTransition != nullptr)
    {
        m_pActiveTransition->Cancel();

        if (m_pCallback != nullptr)
        {
            m_pCallback->TransitionFinished();
        }
    }

    m_pActiveTransition = nullptr;
    m_TransitionMap.DeleteValues();
}

/**
 * Offset/Address/Size: 0x5A4 | 0x80205694 | size: 0x178
 */
void ScreenTransitionManager::AddTransitionToMap(char* name, ScreenTransition* pTransition)
{
    u32 transitionHash = glHash(name);
    m_TransitionMap.Add(transitionHash, pTransition);

    BasicString<char, Detail::TempStringAllocator> nameString(name);
    m_Transitions.push_back(nameString);
}

/**
 * Offset/Address/Size: 0x504 | 0x802055F4 | size: 0xA0
 */
void ScreenTransitionManager::EnableRandomTransition(const char* filter)
{
    SelectRandomTransition(filter);

    if (m_pActiveTransition != nullptr)
    {
        m_pActiveTransition->Cancel();
    }

    if (m_SelectedTransition != nullptr)
    {
        m_SelectedTransition->Reset();
        m_pActiveTransition = m_SelectedTransition;
        m_SelectedTransition = nullptr;
        m_eView = static_cast<eGLView>(29);
        m_fCurrentTime = 0.0f;
        m_Cut = false;
        m_fCurrentLength = m_pActiveTransition->GetTransitionLength();
    }
}

/**
 * Offset/Address/Size: 0x33C | 0x8020542C | size: 0x1C8
 */
void ScreenTransitionManager::SelectRandomTransition(const char* filter)
{
    Vector<BasicString<char, Detail::TempStringAllocator>, DefaultAllocator> candidates;
    candidates.mData = nullptr;
    candidates.mSize = 0;
    candidates.mCapacity = 0;
    candidates.reserve(8);

    for (int i = 0; i < m_Transitions.mSize; i++)
    {
        const char* transitionName = m_Transitions.mData[i].c_str();

        if (strstr(transitionName, filter) != nullptr)
        {
            candidates.push_back(m_Transitions.mData[i]);
        }
    }

    m_SelectedTransition = nullptr;

    if (candidates.mSize > 0)
    {
        int randomIndex = nlRandom(candidates.mSize, &nlDefaultSeed);
        const char* selectedName = candidates.mData[randomIndex].c_str();

        unsigned long transitionHash = glHash(selectedName);
        ScreenTransition** foundTransition = nullptr;
        bool found = m_TransitionMap.FindGet(transitionHash, &foundTransition);

        if (found)
        {
            m_SelectedTransition = *foundTransition;
        }
    }
}

/**
 * Offset/Address/Size: 0x2FC | 0x802053EC | size: 0x40
 */
float ScreenTransitionManager::GetSelectedTransitionCutTime() const
{
    if (m_SelectedTransition != nullptr)
    {
        return m_SelectedTransition->CutTime();
    }
    return 0.0f;
}

/**
 * Offset/Address/Size: 0x260 | 0x80205350 | size: 0x9C
 */
void ScreenTransitionManager::EnableSelectedTransition()
{
    if (m_pActiveTransition != nullptr)
    {
        m_pActiveTransition->Cancel();
    }

    if (m_SelectedTransition != nullptr)
    {
        m_SelectedTransition->Reset();
        m_pActiveTransition = m_SelectedTransition;
        m_SelectedTransition = nullptr;
        m_eView = GLV_Transitions;
        m_fCurrentTime = 0.0f;
        m_Cut = false;

        m_fCurrentLength = m_pActiveTransition->GetTransitionLength();
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x802050F0 | size: 0x260
 */
void ScreenTransitionManager::AddTransitions(char* loadedData, unsigned long fileSize)
{
    SimpleParser parser;
    char szNameBuffer[64];

    parser.StartParsing(loadedData, fileSize, true);
    char* pToken = parser.NextToken(true);

    while (pToken != nullptr)
    {
        if (nlStrCmp<char>(pToken, "colourblend") == 0)
        {
            pToken = parser.NextTokenOnLine(true);
            nlStrNCpy<char>(szNameBuffer, pToken, 0x40);
            AddTransitionToMap(szNameBuffer, ColourBlendScreenTransition::GetFromParser(&parser));
        }
        else if (nlStrCmp<char>(pToken, "sequence") == 0)
        {
            nlStrNCpy<char>(szNameBuffer, parser.NextTokenOnLine(true), 0x40);
            TransitionSequence* transitionSequence = new (nlMalloc(sizeof(TransitionSequence), 8, 0)) TransitionSequence();
            transitionSequence->Initialize(&parser);
            AddTransitionToMap(szNameBuffer, transitionSequence);
        }
        else if (nlStrCmp<char>(pToken, "transition") == 0)
        {
            nlStrNCpy<char>(szNameBuffer, parser.NextTokenOnLine(true), 0x40);
            ScriptedScreenTransition* scriptedTransition = new (nlMalloc(sizeof(ScriptedScreenTransition), 8, 0)) ScriptedScreenTransition();
            scriptedTransition->InitializeFromParser(&parser);
            AddTransitionToMap(szNameBuffer, scriptedTransition);
        }
        else if (nlStrCmp<char>(pToken, "model") == 0)
        {
            nlStrNCpy<char>(szNameBuffer, parser.NextTokenOnLine(true), 0x40);
            ModeledScreenTransition* modeledTransition = new (nlMalloc(sizeof(ModeledScreenTransition), 8, 0)) ModeledScreenTransition();
            modeledTransition->LoadFromParser(&parser);
            AddTransitionToMap(szNameBuffer, modeledTransition);
        }
        else if (*pToken == 0x23)
        {
            while (pToken != nullptr)
            {
                pToken = parser.NextTokenOnLine(true);
            }
        }
        pToken = parser.NextToken(true);
    }
}
