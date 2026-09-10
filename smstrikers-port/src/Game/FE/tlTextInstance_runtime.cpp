#include "Game/FE/tlTextInstance.h"

#include "NL/nlAlgorithm.h"
#include "NL/nlFont.h"
#include "NL/nlLocalization.h"
#include "NL/nlTextBox.h"
#include "Game/FE/feFontResource.h"

static inline const nlFont* GetFontFromComponent(TLComponent* comp)
{
    // PORT: was a local struct with a 0x14-byte pad standing in for FEFontResource.
    return ((const FEFontResource*)comp->pChildren)->m_pFontReference;
}

/**
 * Offset/Address/Size: 0x0 | 0x802101D8 | size: 0x1C
 */
void TLTextInstance::SetScissorBox(u16 left, u16 top, u16 width, u16 height)
{
    m_UseScissorRect = true;
    m_ScissorRect.X = left;
    m_ScissorRect.Y = top;
    m_ScissorRect.Width = width;
    m_ScissorRect.Height = height;
}

/**
 * Offset/Address/Size: 0x1C | 0x802101F4 | size: 0x1C
 */
void TLTextInstance::SetString(const unsigned short* utf16)
{
    m_wcUserString = utf16;
    m_pFontString = NULL;
    m_OverloadFlags &= 0xFFFFFFF7;
}

/**
 * Offset/Address/Size: 0x38 | 0x80210210 | size: 0x250
 */
void TLTextInstance::Render(eGLView view, const nlColour& colour) const
{
    nlVector2 drawAt;
    const nlFont* pFont;

    GetPosition();

    TLComponent* component = m_component;
    // PORT: as above, the real type.
    const FEFontResource* resource = (const FEFontResource*)component->pChildren;
    if (!resource->m_bValid)
    {
        return;
    }

    const unsigned short* pWideTextString;
    if (m_OverloadFlags & 0x8)
    {
        unsigned long key = m_LocStrId;
        nlLocalization* loc = g_pLocalization;
        const unsigned short* locString;

        if (loc->m_LookupTable == NULL)
        {
            locString = LocalizationTableNotFound;
        }
        else
        {
            nlLocalization::StringLookup* result = nlBSearch<nlLocalization::StringLookup, unsigned long>(key, loc->m_LookupTable, loc->m_pFile->StringCount);
            if (result != NULL)
            {
                locString = loc->m_FirstString + result->StringOffset;
            }
            else
            {
                locString = MissingLocString;
            }
        }

        pWideTextString = locString;
    }
    else
    {
        pWideTextString = m_wcUserString;
    }

    if (pWideTextString != NULL)
    {
        unsigned short* buffer = (unsigned short*)__builtin_alloca((nlStrLen<unsigned short>(pWideTextString) + 1) * sizeof(unsigned short));

        pFont = GetFontFromComponent(component);
        FontCharString charString(pWideTextString, pFont, buffer);

        m_DrawInfo.String = charString.m_pString;
        nlTextBox::ProcessString(&charString, pFont, m_OverloadedAttributes.BoxSize, m_DrawOptions | 0x800, m_DrawInfo.pMatrix, m_DrawInfo);
    }
    else if (m_pFontString == NULL)
    {
        return;
    }

    float x;
    switch (m_DrawOptions & 0xF)
    {
    case 0:
        x = 0.0f;
        break;
    case 1:
    {
        float half = 0.5f;
        float bx = m_OverloadedAttributes.BoxSize.x;
        x = -bx * half;
        break;
    }
    case 2:
        x = -m_OverloadedAttributes.BoxSize.x;
        break;
    }

    float y;
    switch (m_DrawOptions & 0xF0)
    {
    case 0:
        y = 0.0f;
        break;
    case 0x10:
    {
        float half = 0.5f;
        float by = m_OverloadedAttributes.BoxSize.y;
        y = by * half;
        break;
    }
    case 0x20:
        y = m_OverloadedAttributes.BoxSize.y;
        break;
    }

    drawAt.x = x;
    drawAt.y = y;

    if (m_UseScissorRect)
    {
        m_DrawInfo.pFont->SetScissorBox(m_ScissorRect);
    }

    nlTextBox::DrawString(m_DrawInfo, drawAt, colour, view);

    if (m_UseScissorRect)
    {
        m_DrawInfo.pFont->DisableScissorBox();
    }
}

void TLTextInstance::DisableScissorBox()
{
    m_UseScissorRect = false;
}

/**
 * Offset/Address/Size: 0x288 | 0x80210460 | size: 0x94
 */
const unsigned short* TLTextInstance::GetString() const
{
    const unsigned short* pWideTextString;
    if (m_OverloadFlags & 0x8)
    {
        unsigned long key = m_LocStrId;
        nlLocalization* loc = g_pLocalization;
        const unsigned short* locString;

        if (loc->m_LookupTable == NULL)
        {
            locString = LocalizationTableNotFound;
        }
        else
        {
            nlLocalization::StringLookup* result = nlBSearch<nlLocalization::StringLookup, unsigned long>(key, loc->m_LookupTable, loc->m_pFile->StringCount);
            if (result != NULL)
            {
                locString = loc->m_FirstString + result->StringOffset;
            }
            else
            {
                locString = MissingLocString;
            }
        }
        pWideTextString = locString;
    }
    else
    {
        pWideTextString = m_wcUserString;
    }

    return pWideTextString;
}
