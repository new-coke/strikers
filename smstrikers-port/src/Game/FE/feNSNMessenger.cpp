#include "Game/FE/feNSNMessenger.h"
#include "Game/FE/feFinder.h"

#include "NL/gl/glStruct.h"
#include "NL/nlLocalization.h"
#include "NL/nlTask.h"

static float MESSAGE_DISPLAY_TIME = 3.0f;

/**
 * Offset/Address/Size: 0xAC0 | 0x800A1DDC | size: 0x84
 */
NSNMessengerScene::NSNMessengerScene()
    : BaseOverlayHandler(3, POSITION_BOTTOM)
{
    m_curState = MS_INVALID;
    m_messageDisplayTime = 0.0f;
    m_scrollText = NULL;
}

/**
 * Offset/Address/Size: 0x940 | 0x800A1C5C | size: 0x180
 */
NSNMessengerScene::~NSNMessengerScene()
{
    if (m_scrollText != NULL)
    {
        delete m_scrollText;
    }
}

/**
 * Offset/Address/Size: 0x914 | 0x800A1C30 | size: 0x2C
 */
void NSNMessengerScene::SceneCreated()
{
    CloseMessengerNow();
}

/**
 * Offset/Address/Size: 0x7B4 | 0x800A1AD0 | size: 0x160
 */
void NSNMessengerScene::Update(float fDeltaT)
{
    BaseOverlayHandler::Update(fDeltaT);

    if (m_messageDisplaying)
    {
        m_messageDisplayTime += fDeltaT;
        if (m_messageDisplayTime > MESSAGE_DISPLAY_TIME && m_scrollText == NULL
            && m_messageFinishedCB)
        {
            m_messageDisplaying = false;
            m_messageFinishedCB();
        }

        if (m_scrollText != NULL)
        {
            m_scrollText->Update(fDeltaT);
        }
    }

    FEPresentation* pres = m_pFEScene->m_pFEPackage->GetPresentation();
    TLSlide* slide = pres->m_currentSlide;

    if (m_curState == MS_OPENING || m_curState == MS_CLOSING)
    {
        if (slide->m_time >= slide->m_start + slide->m_duration)
        {
            switch (m_curState)
            {
            case MS_OPENING:
                OpenMessengerNow();
                break;
            case MS_OPEN:
                break;
            case MS_CLOSING:
                CloseMessengerNow();
                break;
            default:
                break;
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x728 | 0x800A1A44 | size: 0x8C
 */
void NSNMessengerScene::OpenMessenger()
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

    if (mVisibilityMask & nlTaskManager::m_pInstance->m_CurrState)
    {
        SetVisible(true);
    }

    presentation->SetActiveSlide("Intro");
    presentation->m_currentSlide->Update(0.0f);
    m_curState = MS_OPENING;
}

/**
 * Offset/Address/Size: 0x6A8 | 0x800A19C4 | size: 0x80
 */
void NSNMessengerScene::OpenMessengerNow()
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

    if (mVisibilityMask & nlTaskManager::m_pInstance->m_CurrState)
    {
        SetVisible(true);
    }

    presentation->SetActiveSlide("Normal");
    m_curState = MS_OPEN;
}

/**
 * Offset/Address/Size: 0x638 | 0x800A1954 | size: 0x70
 */
void NSNMessengerScene::ForceMessengerVisibleNow()
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    SetVisible(true);
    presentation->SetActiveSlide("Normal");
    m_curState = MS_OPEN;
}

/**
 * Offset/Address/Size: 0x418 | 0x800A1734 | size: 0x220
 */
void NSNMessengerScene::SetDisplayMessage(const BasicString<unsigned short, Detail::TempStringAllocator>& theMessage)
{
    m_messageDisplaying = true;
    m_messageDisplayTime = 0.0f;

    const unsigned short* str = theMessage.c_str();
    memcpy(m_displayMessage, str, 0x1FE);

    FEPresentation* pres = m_pFEScene->m_pFEPackage->GetPresentation();

    TLTextInstance* textBox;

    textBox = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        pres,
        InlineHasher(nlStringLowerHash("Normal")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Group")),
        InlineHasher(nlStringLowerHash("Text")));
    textBox->SetString(m_displayMessage);

    textBox = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        pres,
        InlineHasher(nlStringLowerHash("Intro")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Group")),
        InlineHasher(nlStringLowerHash("Text")));
    textBox->SetString(m_displayMessage);

    textBox = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        pres,
        InlineHasher(nlStringLowerHash("Outro")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Group")),
        InlineHasher(nlStringLowerHash("Text")));
    textBox->SetString(m_displayMessage);

    if (m_scrollText != NULL)
    {
        m_scrollText->SetDisplayMessage(theMessage);
    }
}

static inline const unsigned short* LookupLocText(const char* locMessage)
{
    unsigned long hash = nlStringLowerHash(locMessage);
    nlLocalization* loc = g_pLocalization;
    if (loc->m_LookupTable == 0)
    {
        return LocalizationTableNotFound;
    }
    nlLocalization::StringLookup* lookup = nlBSearch<nlLocalization::StringLookup, unsigned long>(hash, loc->m_LookupTable, loc->m_pFile->StringCount);
    if (lookup != 0)
    {
        return loc->m_FirstString + lookup->StringOffset;
    }
    return MissingLocString;
}

/**
 * Offset/Address/Size: 0x274 | 0x800A1590 | size: 0x1A4
 */
void NSNMessengerScene::SetDisplayMessage(const char* locMessage)
{
    const unsigned short* text = LookupLocText(locMessage);
    SetDisplayMessage(BasicString<unsigned short, Detail::TempStringAllocator>(text));
}

/**
 * Offset/Address/Size: 0x210 | 0x800A152C | size: 0x64
 */
void NSNMessengerScene::CloseMessenger()
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

    presentation->SetActiveSlide("Outro");
    presentation->m_currentSlide->m_time = 0.0f;
    presentation->m_currentSlide->Update(0.0f);

    m_curState = MS_CLOSING;
}

/**
 * Offset/Address/Size: 0x1C8 | 0x800A14E4 | size: 0x48
 */
void NSNMessengerScene::CloseMessengerNow()
{
    SetVisible(false);
    m_curState = MS_CLOSED;
}

/**
 * Offset/Address/Size: 0x1C0 | 0x800A14DC | size: 0x8
 */
bool NSNMessengerScene::IsMessengerOpen() const
{
    return m_bVisible;
}

/**
 * Offset/Address/Size: 0x0 | 0x800A131C | size: 0x1C0
 */
void NSNMessengerScene::EnableScrolling(bool state)
{
    if (state)
    {
        TLTextInstance* textinstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            m_pFEPresentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("Group")),
            InlineHasher(nlStringLowerHash("Text")));

        if (m_scrollText == NULL)
        {
            gl_ScreenInfo* screeninfo = glGetScreenInfo();
            FEScrollText* scrolltext = new (nlMalloc(sizeof(FEScrollText), 8, false)) FEScrollText(textinstance, 0, screeninfo->ScreenWidth);
            m_scrollText = scrolltext;
        }
        else
        {
            m_scrollText->ApplyNewTextInstancePointer(textinstance, 8000.0f, 100.0f);
        }
    }
    else if (m_scrollText != NULL)
    {
        delete m_scrollText;
        m_scrollText = NULL;
    }
}
