#include "Game/FE/feButtonComponent.h"
#include "NL/nlLocalization.h"
#include "NL/nlString.h"

#include "Game/FE/feAsyncImage.h"
#include "Game/FE/feFontResource.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/feFinder.h"
#include "NL/glx/glxTexture.h"

int ButtonComponent::GetRenderedStringLength(const unsigned short* pString, const nlFont* pFont)
{
    int returnValue;
    unsigned char firstChar;
    const unsigned short* pLastChar;
    unsigned short* pCurrentChar;
    unsigned long charWidth;

    returnValue = 0;
    pLastChar = 0;
    firstChar = true;
    {
        FontCharString fcs(pString, pFont, (unsigned short*)0);
        pCurrentChar = fcs.m_pString;

        while (*pCurrentChar != 0)
        {
            charWidth = pFont->GetCharWidth(*pCurrentChar, firstChar ? 0 : *pLastChar);
            returnValue += charWidth;
            pLastChar = pCurrentChar;
            firstChar = false;
            pCurrentChar++;
        }
    }

    return returnValue;
}

/**
 * Offset/Address/Size: 0x0 | 0x8010DC04 | size: 0x3F8
 */
void ButtonComponent::CentreButtons()
{
    float totalLength;
    int i;
    PlatTexture* texture;
    float buttonWidth;
    feVector3 imagePosition;
    feVector3 textPosition;
    unsigned short buffer[128];
    feVector3 labelScale;
    feVector3 componentPosition;

    if (mAlreadyCentred || mButtonInstance == NULL)
    {
        return;
    }

    totalLength = 0.0f;

    for (i = 0; i < mNumButtons; i++)
    {
        int renderedLength;

        texture = glx_GetTex(mButtonImages[i]->m_pTextureResource->m_glTextureHandle, true, true);
        buttonWidth = (float)texture->m_Width;

        imagePosition = mButtonImages[i]->GetAssetPosition();
        float halfWidth = buttonWidth / 2.0f;
        mButtonImages[i]->SetAssetPosition(totalLength + halfWidth, imagePosition.f.y, imagePosition.f.z);
        totalLength += buttonWidth;

        textPosition = mButtonLabels[i]->GetAssetPosition();
        mButtonLabels[i]->SetAssetPosition(totalLength, textPosition.f.y, textPosition.f.z);

        const nlFont* pFont = ((FEFontResource*)mButtonLabels[i]->m_component->pChildren)->m_pFontReference;

        nlStrNCpy<unsigned short>(buffer, mButtonLabels[i]->GetString(), 0x80);
        buffer[127] = 0;

        labelScale = mButtonLabels[i]->GetAssetScale();

        renderedLength = GetRenderedStringLength(buffer, pFont);

        totalLength += (float)(int)(labelScale.f.x * (float)renderedLength);
        totalLength += 32.0f;
    }

    totalLength -= 32.0f;

    componentPosition = mButtonInstance->GetAssetPosition();
    mButtonInstance->SetAssetPosition(-(totalLength / 2.0f), componentPosition.f.y, componentPosition.f.z);
    mButtonInstance->m_bVisible = true;
    mAlreadyCentred = true;

    TLComponentInstance* pComponent = (TLComponentInstance*)FindComponent(mButtonInstance->GetActiveSlide(), "Component");
    if (pComponent != NULL)
    {
        TLImageInstance* pImage = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            pComponent->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("Group")),
            InlineHasher(nlStringLowerHash("darkblue_frame2")));

        if (pImage != 0)
        {
            pImage->SetAssetScale(1024.0f, 1.0f, 1.0f);
        }

        pImage = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            pComponent->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("Group")),
            InlineHasher(nlStringLowerHash("darkblue_frame")));

        if (pImage != 0)
        {
            pImage->SetAssetScale(1024.0f, 1.0f, 1.0f);
        }
    }
}

/**
 * Offset/Address/Size: 0x3F8 | 0x8010DFFC | size: 0x8D8
 */
void ButtonComponent::SetState(ButtonComponent::ButtonState buttonstate)
{
    if (mButtonInstance != NULL)
    {
        mAlreadyCentred = false;

        switch (buttonstate)
        {
        case BS_A_AND_B:
            mButtonInstance->SetActiveSlide("a and b");
            mNumButtons = 2;
            mButtonImages[1] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("A_button")));
            mButtonLabels[1] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("accept")));
            mButtonImages[0] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("B_button")));
            mButtonLabels[0] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("back")));
            break;

        case BS_A_ONLY:
            mButtonInstance->SetActiveSlide("a");
            mNumButtons = 1;
            mButtonImages[0] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("A_button")));
            mButtonLabels[0] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("accept")));
            break;

        case BS_B_ONLY:
            mButtonInstance->SetActiveSlide("b");
            mNumButtons = 1;
            mButtonImages[0] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("B_button")));
            mButtonLabels[0] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("back")));
            break;

        case BS_A_AND_B_AND_Y:
            mButtonInstance->SetActiveSlide("a b y");
            mNumButtons = 3;
            mButtonImages[1] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("A_button")));
            mButtonLabels[1] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("accept")));
            mButtonImages[0] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("B_button")));
            mButtonLabels[0] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("back")));
            mButtonImages[2] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("y_button")));
            mButtonLabels[2] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("OPTIONS")));
            break;

        case 4:
            mButtonInstance->SetActiveSlide("a and b and start");
            mNumButtons = 3;
            mButtonImages[0] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("start_button")));
            mButtonLabels[0] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("start")));
            mButtonImages[1] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("B_button")));
            mButtonLabels[1] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("back")));
            mButtonImages[2] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("A_button")));
            mButtonLabels[2] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(mButtonInstance->GetActiveSlide(), InlineHasher(nlStringLowerHash("accept")));
            break;

        default:
            break;
        }
    }

    if (g_pLocalization->m_CurrentLanguage == nlLocalization::LangJapanese)
    {
        for (int i = 0; i < mNumButtons; i++)
        {
            mButtonLabels[i]->SetAssetScale(0.9f, 0.9f, 1.0f);
        }
    }
}

/**
 * Offset/Address/Size: 0xCD0 | 0x8010E8D4 | size: 0x3C
 */
ButtonComponent::~ButtonComponent()
{
}

/**
 * Offset/Address/Size: 0xD0C | 0x8010E910 | size: 0x14
 */
ButtonComponent::ButtonComponent()
{
    mButtonInstance = 0;
    mNumButtons = 0;
    mAlreadyCentred = false;
}
