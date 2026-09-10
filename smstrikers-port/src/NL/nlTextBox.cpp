#include "NL/nlTextBox.h"
#include "NL/nlColour.h"
#include "NL/nlTextEscape.h"
#include "NL/nlString.h"

nlTextBox::nlTextBox()
{
    nlVec2Set(m_Position, 0.0f, 0.0f);
    nlVec2Set(m_Size, 640.0f, 480.0f);
    m_DrawOptions = AlignLeft;
}

/**
 * Offset/Address/Size: 0x0 | 0x80211F28 | size: 0x244
 */
void nlTextBox::DrawString(const nlTextBox::StringDrawInfo& DrawInfo, const nlVector2& DrawAt, const nlColour& Color, eGLView View)
{
    int yDir = 1;
    const nlFont* pFont = DrawInfo.pFont;
    nlVector2 CurrentPos = DrawAt;
    if (DrawInfo.DrawOptions & FlipY)
    {
        yDir = -1;
    }
    float yWithOffset = CurrentPos.y + (float)(yDir * DrawInfo.YOffset);
    CurrentPos.y = yWithOffset;

    int ascentAdj;
    if (DrawInfo.DrawOptions & UseFullHeight)
    {
        ascentAdj = pFont->m_Metrics.InternalLeading;
    }
    else
    {
        ascentAdj = 0;
    }

    const nlMatrix4* pMatrix = DrawInfo.pMatrix;
    int vertOffset = yDir * (int)pFont->m_Metrics.Ascent - ascentAdj;
    nlColour overridecolour;
    overridecolour = Color;
    uintptr_t hMatrix;             // PORT: a matrix handle

    unsigned long row = 0;
    CurrentPos.y = yWithOffset + (float)vertOffset;
    overridecolour.c[3] = 0;

    while (row < DrawInfo.RowCount)
    {
        // PORT: 0x14 is offsetof(StringDrawInfo, Rows) on console.
        const Row& CurrentRow = DrawInfo.Rows[row];
        CurrentPos.x = DrawAt.x + (float)CurrentRow.XOffset;

        if (pMatrix)
        {
            uintptr_t h = glAllocMatrix();
            if ((u32)(h + 0x10000) != 0xFFFF)
            {
                glSetMatrix(h, *pMatrix);
            }
            hMatrix = h;
        }

        unsigned short startIdx = CurrentRow.FirstChar;
        {
            FontCharString fontCharStr;
            fontCharStr.m_pString = (unsigned short*)(DrawInfo.String + startIdx);
            fontCharStr.m_InternalBuffer = 0;
            uintptr_t* matArg = pMatrix ? &hMatrix : 0;

            int length = (&CurrentRow + 1)->FirstChar - startIdx;

            DrawInfo.pFont->DrawString(View, fontCharStr, CurrentPos, Color, Color, length, nlFont::PASS_TextAndEffect, (DrawInfo.DrawOptions & FlipY) != 0, matArg, &overridecolour);
        }

        if (overridecolour.c[3] == 0)
        {
            overridecolour = Color;
        }

        row++;
        CurrentPos.y += (float)(yDir * pFont->m_Metrics.Height);
    }
}

/**
 * Offset/Address/Size: 0x244 | 0x8021216C | size: 0x3B0
 */
void nlTextBox::ProcessString(const FontCharString* pString, const nlFont* pFont, const nlVector2& BoxSize, unsigned long DrawOptions, const nlMatrix4* pMatrix, nlTextBox::StringDrawInfo& DrawInfo)
{
    unsigned long CurrentRowWidth = 0;
    const unsigned short* pLastSpace = 0;
    const unsigned short* pLastNonEsc = 0;
    unsigned long WidthAtLastSpace = 0;
    unsigned char FirstChar = 1;
    unsigned char IsNewParagraph = 0;
    unsigned char IsNonBreakingSpace = 0;
    const unsigned short* pCurrentChar;
    unsigned long CharWidth;

    DrawInfo.pFont = pFont;
    pCurrentChar = DrawInfo.String = pString->m_pString;
    DrawInfo.DrawOptions = DrawOptions;
    DrawInfo.RowCount = 0;
    DrawInfo.pMatrix = pMatrix;

    while (*pCurrentChar != 0)
    {
        if (*pCurrentChar == nlEscapeSequence::ESCAPE_BEGIN)
        {
            nlEscapeSequence esc(pCurrentChar);

            if (esc.m_Type == ESC_NON_BREAKING_SPACE)
            {
                if (pFont->IsJapanese())
                {
                    CharWidth = 0;
                    IsNonBreakingSpace = 1;
                }
                else
                {
                    unsigned long prevChar = FirstChar ? 0 : (pLastNonEsc != 0 ? (unsigned long)*pLastNonEsc : 0);
                    CharWidth = pFont->GetCharWidth(' ', (unsigned short)prevChar);
                }
            }
            else if (esc.m_Type == ESC_PARAGRAPH)
            {
                CharWidth = (unsigned long)(1.0f + BoxSize.x);
                WidthAtLastSpace = CurrentRowWidth;
                IsNewParagraph = 1;
                pLastSpace = esc.m_pEnd - 1;
            }
            else
            {
                CharWidth = 0;
            }
            pCurrentChar = esc.m_pEnd - 1;
        }
        else if (!IsNonBreakingSpace)
        {
            unsigned long prevChar = FirstChar ? 0 : (pLastNonEsc != 0 ? (unsigned long)*pLastNonEsc : 0);
            CharWidth = pFont->GetCharWidth(*pCurrentChar, (unsigned short)prevChar);
            pLastNonEsc = pCurrentChar;
            if (*pCurrentChar == ' ')
            {
                pLastSpace = pCurrentChar;
                WidthAtLastSpace = CurrentRowWidth;
            }
        }

        FirstChar = 0;

        if ((float)(CurrentRowWidth + CharWidth) > BoxSize.x
            && !IsNonBreakingSpace
        )
        {
            if (!(DrawOptions & DontWrapOnSpaces) && pLastSpace != 0)
            {
                CurrentRowWidth = WidthAtLastSpace;
                pCurrentChar = pLastSpace + 1;
                pLastSpace = 0;
                WidthAtLastSpace = 0;
            }

            int xOffset;
            if (DrawOptions & 0x3)
            {
                int remaining = (int)(BoxSize.x - (float)CurrentRowWidth);
                xOffset = remaining >> ((DrawOptions & AlignCenter) ? 1u : 0u);
            }
            else
            {
                xOffset = 0;
            }

            DrawInfo.Rows[DrawInfo.RowCount].XOffset = xOffset;
            unsigned short charIdx = (unsigned short)(pCurrentChar - pString->m_pString);
            DrawInfo.RowCount++;
            DrawInfo.Rows[DrawInfo.RowCount].FirstChar = charIdx;

            if (*pCurrentChar != ' ')
            {
                pCurrentChar--;
            }
            if (*pCurrentChar == ' ')
            {
                CharWidth = 0;
            }
            CurrentRowWidth = 0;
        }

        if (IsNonBreakingSpace)
        {
            IsNonBreakingSpace = 0;
        }

        CurrentRowWidth += CharWidth;

        if (IsNewParagraph)
        {
            CurrentRowWidth = 0;
            IsNewParagraph = 0;
        }

        pCurrentChar++;
    }

    int xOffset;
    if (DrawOptions & 0x3)
    {
        int remaining = (int)(BoxSize.x - (float)CurrentRowWidth);
        xOffset = remaining >> ((DrawOptions & AlignCenter) ? 1u : 0u);
    }
    else
    {
        xOffset = 0;
    }

    DrawInfo.Rows[DrawInfo.RowCount].XOffset = xOffset;
    DrawInfo.RowCount++;

    s32 strLen = nlStrLen(pString->m_pString);
    DrawInfo.Rows[DrawInfo.RowCount].FirstChar = (unsigned short)strLen;
    DrawInfo.Rows[0].FirstChar = 0;

    if (DrawOptions & 0x30)
    {
        int TotalHeight = (int)DrawInfo.RowCount * (int)pFont->m_Metrics.Height;
        if ((float)TotalHeight > BoxSize.y)
        {
            DrawInfo.YOffset = 0;
        }
        else if (DrawOptions & VAlignCenter)
        {
            DrawInfo.YOffset = (signed short)((int)(BoxSize.y / 2.0f) - (TotalHeight >> 1));
        }
        else
        {
            DrawInfo.YOffset = (signed short)((int)BoxSize.y - TotalHeight);
        }
    }
    else
    {
        DrawInfo.YOffset = 0;
    }
}
