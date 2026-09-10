#include "Game/FE/feScrollText.h"
#include "Game/FE/feFontResource.h"
#include "NL/gl/glStruct.h"
#include "NL/nlTextEscape.h"
#include "NL/nlAlgorithm.h"
#include "NL/nlLocalization.h"

/**
 * Offset/Address/Size: 0x0 | 0x800C89D4 | size: 0x38
 */
void FEScrollText::ApplyNewTextInstancePointer(TLTextInstance* controltext, float boxwidth, float boxheight)
{
    nlVector2 boxSize;
    boxSize.y = boxheight;

    this->m_controlText = controltext;
    TLTextInstance* text = this->m_controlText;

    boxSize.x = boxwidth;

    text->m_OverloadedAttributes.BoxSize = boxSize;
    text->m_OverloadFlags |= 0x4;
}

static float TEXT_TIME = 4.5f;

/**
 * Offset/Address/Size: 0x38 | 0x800C8A0C | size: 0x190
 */
void FEScrollText::Update(float fDeltaT)
{
    if (m_textFont == NULL)
    {
        SetDisplayMessage(m_message);
        if (m_textFont == NULL)
            return;
    }

    m_controlText->m_bVisible = true;
    m_msgTime += fDeltaT;

    float pixPerSec = (float)m_width / TEXT_TIME;

    feVector3 pos = m_controlText->GetPosition();

    float x = (float)(m_pos + m_width / 2);

    x = x - m_msgTime * pixPerSec;

    m_controlText->SetAssetPosition(x, pos.f.y, pos.f.z);

    if (x + (float)m_messageWidth < m_leftEdge)
    {
        if (m_messageFinishedCB)
        {
            m_messageFinishedCB();
        }
        else
        {
            m_msgTime = 0.0f;
        }
    }
}

static inline const unsigned short* LookupLocTextChar(nlLocalization* loc, unsigned long hash)
{
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
 * Offset/Address/Size: 0x1C8 | 0x800C8B9C | size: 0x198
 */
void FEScrollText::SetDisplayMessage(const char* locMessage)
{
    nlLocalization* loc = g_pLocalization;
    unsigned long hash = nlStringLowerHash(locMessage);
    const unsigned short* text = LookupLocTextChar(loc, hash);
    SetDisplayMessage(BasicString<unsigned short, Detail::TempStringAllocator>(text));
}

static inline const unsigned short* LookupLocText(unsigned long hash)
{
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
 * Offset/Address/Size: 0x360 | 0x800C8D34 | size: 0x190
 */
void FEScrollText::SetDisplayMessage(unsigned long hash)
{
    const unsigned short* text = LookupLocText(hash);
    SetDisplayMessage(BasicString<unsigned short, Detail::TempStringAllocator>(text));
}

static inline unsigned int MapCharToFontChar(nlFont* font, unsigned int ch)
{
    if (ch <= 0x7F)
        return ch;

    nlFont::GlyphInfo key;
    key.UnicodeChar = ch;
    nlFont::GlyphInfo* result;
    if (font->m_pExtendedGlyphs != NULL && font->m_ExtendedGlyphCount != 0 && (result = nlBSearch<nlFont::GlyphInfo, nlFont::GlyphInfo>(key, font->m_pExtendedGlyphs, font->m_ExtendedGlyphCount)) != NULL)
    {
        return (unsigned short)((result - font->m_pExtendedGlyphs) + 0x80);
    }
    ch = 0x30;
    while (((unsigned short)ch > 0x7F ? font->m_pExtendedGlyphs[(unsigned short)ch - 0x80] : font->m_GlyphLookup[(unsigned short)ch - 0x20]).UnicodeChar == 0xFFFF)
    {
        ch++;
    }
    return ch;
}

static inline void BuildFontCharStringForScroll(FontCharString& fontcharstring, FEScrollText* self)
{
    fontcharstring.m_pString = self->m_textBuffer;
    fontcharstring.m_InternalBuffer = 0;

    const unsigned short* src = self->m_message.c_str();
    nlFont* font = self->m_textFont;
    unsigned short* dest = fontcharstring.m_pString;
    unsigned short escBegin = nlEscapeSequence::ESCAPE_BEGIN;

    while (*src != 0)
    {
        unsigned int ch = *src;
        if (ch == escBegin)
        {
            nlEscapeSequence EscSeq(src);
            const unsigned short* end = EscSeq.m_pEnd;
            while (src < end)
            {
                *dest++ = *src++;
            }
        }
        else
        {
            ch = MapCharToFontChar(font, ch);
            *dest++ = ch;
            src++;
        }
    }

    *dest = 0;
}

/**
 * Offset/Address/Size: 0x4F0 | 0x800C8EC4 | size: 0x578
 */
void FEScrollText::SetDisplayMessage(const BasicString<unsigned short, Detail::TempStringAllocator>& theMessage)
{
    m_message = theMessage;
    m_msgTime = 0.0f;

    if (m_textFont == NULL)
    {
        m_textFont = ((FEFontResource*)m_controlText->m_component->pChildren)->m_pFontReference;
    }

    if (m_textFont == NULL)
    {
        return;
    }

    FontCharString fontcharstring;
    BuildFontCharStringForScroll(fontcharstring, this);
    m_messageWidth = 0;

    int i = 0;
    while (i < (m_message.mData != 0 ? m_message.mData->mData.mSize - 1 : 0))
    {
        const unsigned short* str = m_message.c_str();
        unsigned short* charPtr = (unsigned short*)(str) + i;
        unsigned short origCh = *charPtr;

        if (origCh == nlEscapeSequence::ESCAPE_BEGIN)
        {
            nlEscapeSequence EscSeq(charPtr);
            uintptr_t skipCount = ((uintptr_t)EscSeq.m_pEnd - (uintptr_t)charPtr) / 2;
            i += skipCount - 1;
        }
        else
        {
            unsigned short* fontBuffer = fontcharstring.m_pString;
            unsigned short mappedCh = fontBuffer[i];
            int charWidth;
            if (i != 0)
            {
                int prev = i - 1;
                unsigned short prevChar = fontBuffer[prev];
                charWidth = (int)m_textFont->GetCharWidth(mappedCh, prevChar);
            }
            else
            {
                charWidth = (int)m_textFont->GetCharWidth(mappedCh, 0);
            }
            m_messageWidth += charWidth;
        }
        i++;
    }

    const unsigned short* finalStr = m_message.c_str();
    memcpy(m_textBuffer, finalStr, sizeof(m_textBuffer));
    m_controlText->SetString(m_textBuffer);

    m_messageWidth = (int)(m_controlText->GetScale().f.x * (float)m_messageWidth);

    Update(0.0f);
}

void FEScrollText::SetMetrics(int pos, int width)
{
    const gl_ScreenInfo* screenInfo = glGetScreenInfo();
    int boxX = pos + screenInfo->ScreenWidth / 2 - width / 2;
    int boxWidth = width;
    if (boxX < 0)
        boxX = 0;
    if (width + boxX >= screenInfo->ScreenWidth)
        boxWidth = screenInfo->ScreenWidth - pos - 1;
    m_controlText->SetScissorBox((u16)boxX, 0, (u16)boxWidth, (u16)screenInfo->ScreenHeight);
}

/**
 * Offset/Address/Size: 0xA68 | 0x800C943C | size: 0x1E8
 */
FEScrollText::FEScrollText(TLTextInstance* controlText, int pos, int width)
    : m_controlText(controlText)
    , m_message((const unsigned short*)L"")
    , m_messageWidth(0)
    , m_msgTime(0.0f)
    , m_textFont(NULL)
{
    nlVector2 boxSize;
    boxSize.x = 8000.0f;

    boxSize.y = 100.0f;

    TLTextInstance* text = m_controlText;
    text->m_OverloadedAttributes.BoxSize = boxSize;
    text->m_OverloadFlags |= 0x4;

    m_controlText->m_bVisible = false;

    m_pos = pos;
    m_width = width;

    SetMetrics(pos, width);

    m_leftEdge = (float)(m_pos - m_width / 2);
}
