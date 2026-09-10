#include "NL/nlFont.h"
#include "NL/nlAlgorithm.h"
#include "NL/nlColour.h"
#include "NL/nlList.h"
#include "NL/nlString.h"

#include "NL/nlPrint.h"
#include "NL/nlTextBox.h"
#include "NL/nlTextEscape.h"
#include "NL/gl/glDraw2.h"
#include "NL/gl/glState.h"
#include "NL/gl/glStruct.h"
#include "NL/gl/glUserData.h"

/**
 * Offset/Address/Size: 0x125C | 0x80211B98 | size: 0x3C
 */
nlFont::nlFont()
{
    m_bIsJapanese = false;
    memset(m_GlyphLookup, 0xFF, sizeof(m_GlyphLookup));
}

/**
 * Offset/Address/Size: 0x11F0 | 0x80211B2C | size: 0x6C
 */
nlFont::~nlFont()
{
    ::operator delete[](m_pKernTable);
    m_pKernTable = NULL;
    if (m_pExtendedGlyphs != NULL)
    {
        ::operator delete[](m_pExtendedGlyphs);
    }
}

static inline nlFont::GlyphInfo* AddExtendedGlyph(nlFont* self, nlListSlotPoolHigh<nlFont::GlyphInfo>& GlyphList)
{
    ListEntry<nlFont::GlyphInfo>* pEntry = GlyphList.Allocate(nlFont::GlyphInfo());
    nlListAddStart<ListEntry<nlFont::GlyphInfo> >(&GlyphList.m_Head, pEntry, &GlyphList.m_Tail);
    self->m_ExtendedGlyphCount++;
    return &pEntry->entry;
}

static inline void ParseKernPairs(nlFont* self, nlFont::GlyphInfo* pInfo, char* pToken, unsigned short Base, nlListSlotPoolHigh<nlFont::KernPair>& KernList)
{
    pInfo->HasKernPairs = 1;
    nlFont::KernPair kp;
    char* pKernToken = nlStrChr(pToken, ' ');
    ListEntry<nlFont::KernPair>** pKernHead;
    ListEntry<nlFont::KernPair>** pKernTail;
    pKernTail = &KernList.m_Tail;
    pKernHead = &KernList.m_Head;
    pKernToken++;
    while ((uintptr_t)pKernToken != 1)
    {
        kp.s.A = Base;
        int nB;
        if (pKernToken[1] != ' ')
        {
            nB = atoi(pKernToken);
        }
        else
        {
            nB = pKernToken[0];
        }
        kp.s.B = (unsigned short)nB;
        pKernToken = nlStrChr(pKernToken, ' ') + 1;
        kp.Kern = atoi(pKernToken);
        ListEntry<nlFont::KernPair>* pEntry = KernList.Allocate(kp);
        nlListAddStart<ListEntry<nlFont::KernPair> >(pKernHead, pEntry, pKernTail);
        self->m_KernTableSize++;
        pKernToken = nlStrChr(pKernToken, ' ') + 1;
    }
}

/**
 * Offset/Address/Size: 0x830 | 0x8021116C | size: 0x9C0
 */
unsigned char nlFont::Load(const char* szFontName, char* pFontDescData, unsigned long HashId)
{
    char* pCurrentLine;

    nlStrNCpy(m_FontName, szFontName, 0x20);

    pCurrentLine = pFontDescData;

    nlListSlotPoolHigh<nlFont::KernPair> KernList(0x10, 0x10);
    m_KernTableSize = 0;

    nlListSlotPoolHigh<nlFont::GlyphInfo> ExtendedGlyphList(0x10, 0x10);
    m_ExtendedGlyphCount = 0;

    unsigned long CurrentPage;
    unsigned long CurrentTexelX;
    unsigned long CurrentTexelY;
    char* pEOL;
    char* pToken;
    unsigned short Character;
    nlFont::GlyphInfo* pInfo;
    unsigned short Base;
    nlFont::KernPair* pCurKP;
    nlFont::KernPair* pKP;

    CurrentPage = 0;
    CurrentTexelX = 0;
    CurrentTexelY = 0;
    m_bScissorBox = false;
    m_Metrics.FontName = HashId;

    for (;;)
    {
        if (nlToUpper(*pCurrentLine) == 'E')
            break;
        pEOL = nlStrChr(pCurrentLine, '\r');
        if (pEOL != NULL)
        {
            *pEOL = 0;
        }

        pToken = nlStrChr(pCurrentLine, ' ') + 1;

        switch (nlToUpper(*pCurrentLine))
        {
        case 'P':
        {
            m_PageSize = atoi(pToken);
            m_InvTexSize = 1.0f / (float)m_PageSize;

            pCurrentLine = nlStrChr(pToken, ' ');
            pCurrentLine++;
            pCurrentLine = nlStrChr(pCurrentLine, ' ') + 1;
            m_PageCount = atoi(pCurrentLine);

            pCurrentLine = nlStrChr(pCurrentLine, ' ');
            pCurrentLine++;
            pCurrentLine = nlStrChr(pCurrentLine, ' ') + 1;
            switch (nlToLower(*pCurrentLine))
            {
            case 'c':
                m_TextureType = Colour;
                break;
            case 'g':
                m_TextureType = Greyscale;
                break;
            case 's':
                m_TextureType = SplitFX;
                break;
            default:
                break;
            }

            pCurrentLine = nlStrChr(pCurrentLine, ' ');
            pCurrentLine++;
            pCurrentLine = nlStrChr(pCurrentLine, ' ');
            switch (nlToLower(pCurrentLine[1]))
            {
            case 'e':
                m_Distribution = English;
                break;
            case 'i':
                m_Distribution = InOrder;
                break;
            default:
                break;
            }
            break;
        }

        case 'H':
        {
            m_Metrics.Height = (unsigned short)atoi(pToken);

            pCurrentLine = nlStrChr(pToken, ' ');
            pCurrentLine++;
            pCurrentLine = nlStrChr(pCurrentLine, ' ') + 1;
            m_Metrics.RenderHeight = (unsigned short)atoi(pCurrentLine);

            pCurrentLine = nlStrChr(pCurrentLine, ' ');
            pCurrentLine++;
            pCurrentLine = nlStrChr(pCurrentLine, ' ') + 1;
            m_Metrics.Ascent = (unsigned short)atoi(pCurrentLine);

            pCurrentLine = nlStrChr(pCurrentLine, ' ');
            pCurrentLine++;
            pCurrentLine = nlStrChr(pCurrentLine, ' ') + 1;
            m_Metrics.RenderAscent = (unsigned short)atoi(pCurrentLine);

            pCurrentLine = nlStrChr(pCurrentLine, ' ');
            pCurrentLine++;
            pCurrentLine = nlStrChr(pCurrentLine, ' ') + 1;
            m_Metrics.InternalLeading = (unsigned short)atoi(pCurrentLine);
            break;
        }

        case 'C':
        {
            m_Metrics.Spacing = (float)atoi(pToken) / 100.0f;

            pCurrentLine = nlStrChr(pToken, ' ');
            pCurrentLine++;
            pCurrentLine = nlStrChr(pCurrentLine, ' ') + 1;
            m_Metrics.LineHeight = (float)atoi(pCurrentLine) / 100.0f;
            break;
        }

        case 'G':
        {
            int nChar;
            if (pToken[1] != ' ')
            {
                nChar = atoi(pToken);
            }
            else
            {
                nChar = pToken[0];
            }
            Character = (unsigned short)nChar;

            if (Character < 0x7F)
            {
                pInfo = m_GlyphLookup + Character - 0x20;
            }
            else
            {
                pInfo = AddExtendedGlyph(this, ExtendedGlyphList);
            }

            pInfo->UnicodeChar = Character;
            pInfo->HasKernPairs = 0;

            pCurrentLine = nlStrChr(pToken, ' ');
            pCurrentLine++;
            pCurrentLine = nlStrChr(pCurrentLine, ' ') + 1;
            pInfo->Advance = (unsigned char)atoi(pCurrentLine);

            pCurrentLine = nlStrChr(pCurrentLine, ' ') + 1;
            pInfo->RenderWidth = (unsigned char)atoi(pCurrentLine);

            pCurrentLine = nlStrChr(pCurrentLine, ' ') + 1;
            pInfo->Offset = (signed char)atoi(pCurrentLine);

            if ((CurrentTexelX + pInfo->RenderWidth) > m_PageSize)
            {
                CurrentTexelX = 0;
                CurrentTexelY += m_Metrics.RenderHeight;
                if ((CurrentTexelY + m_Metrics.RenderHeight) > m_PageSize)
                {
                    CurrentTexelX = 0;
                    CurrentTexelY = 0;
                    CurrentPage++;
                }
            }

            pInfo->Page = CurrentPage;
            nlVec2Set(pInfo->uv, (float)CurrentTexelX * m_InvTexSize, (float)CurrentTexelY * m_InvTexSize);
            CurrentTexelX += pInfo->RenderWidth;
            break;
        }

        case 'K':
        {
            int nBase;
            if (pToken[1] != ' ')
            {
                nBase = atoi(pToken);
            }
            else
            {
                nBase = pToken[0];
            }
            Base = (unsigned short)nBase;

            if (Base > 0x7F)
            {
                pInfo = &m_pExtendedGlyphs[Base - 0x80];
            }
            else
            {
                pInfo = &m_GlyphLookup[Base - 0x20];
            }

            ParseKernPairs(this, pInfo, pToken, Base, KernList);
            break;
        }

        case 'V':
            break;

        default:
            break;
        }

        pCurrentLine = pEOL + 2;
    }

    if (m_KernTableSize != 0)
    {
        m_pKernTable = (KernPair*)nlMalloc(m_KernTableSize * sizeof(KernPair), 8, false);
        pKP = m_pKernTable;

        while (KernList.m_Head != NULL)
        {
            pCurKP = pKP;
            pKP++;
            ListEntry<nlFont::KernPair>* pEntry = nlListRemoveStart<ListEntry<nlFont::KernPair> >(&KernList.m_Head, &KernList.m_Tail);
            if (pCurKP != NULL)
            {
                pCurKP->s.A = pEntry->entry.s.A;
                pCurKP->s.B = pEntry->entry.s.B;
                pCurKP->Kern = pEntry->entry.Kern;
            }

            pEntry->next = (ListEntry<nlFont::KernPair>*)KernList.m_Allocator.m_FreeList;
            KernList.m_Allocator.m_FreeList = (SlotPoolEntry*)pEntry;
        }

        nlQSort<nlFont::KernPair>(m_pKernTable, m_KernTableSize, nlFont::KernPair::SortProc);
    }
    else
    {
        m_pKernTable = NULL;
    }

    if (m_ExtendedGlyphCount != 0)
    {
        m_pExtendedGlyphs = (GlyphInfo*)nlMalloc(m_ExtendedGlyphCount * sizeof(GlyphInfo), 8, false);
        pInfo = m_pExtendedGlyphs;

        while (ExtendedGlyphList.m_Head != NULL)
        {
            ListEntry<nlFont::GlyphInfo>* pEntry = nlListRemoveStart<ListEntry<nlFont::GlyphInfo> >(&ExtendedGlyphList.m_Head, &ExtendedGlyphList.m_Tail);
            if (pInfo != NULL)
            {
                *pInfo = pEntry->entry;
            }

            pEntry->next = (ListEntry<nlFont::GlyphInfo>*)ExtendedGlyphList.m_Allocator.m_FreeList;
            ExtendedGlyphList.m_Allocator.m_FreeList = (SlotPoolEntry*)pEntry;
            pInfo++;
        }

        nlQSort<nlFont::GlyphInfo>(m_pExtendedGlyphs, m_ExtendedGlyphCount, nlFont::GlyphInfo::SortProc);
    }
    else
    {
        m_pExtendedGlyphs = NULL;
    }

    char sHashFontName[265] = { 0 };
    unsigned long page;
    for (page = 0; page < m_PageCount; page++)
    {
        // PORT: was staged into sHashFontName and then formatted from it, and a buffer passed as its own %s argument is undefined: Apple's libc yields "<name>_1", glibc yields "_1", whose hash matches no file in the font bundle.
        nlSNPrintf(sHashFontName, 0x109, "%s_%d", szFontName, page + 1);
        m_TextureHandles[page] = nlStringHash(sHashFontName);

        if (m_TextureType == SplitFX)
        {
            nlStrNCat(sHashFontName, sHashFontName, "e", 0x109);
            m_EffectTextureHandles[page] = nlStringHash(sHashFontName);
        }
    }

    return 1;
}

void nlFont::Unload()
{
    ::operator delete[](m_pKernTable);
    m_pKernTable = NULL;
    if (m_pExtendedGlyphs != NULL)
    {
        ::operator delete[](m_pExtendedGlyphs);
    }
}

/**
 * Offset/Address/Size: 0x17C | 0x80210AB8 | size: 0x6B4
 */
void nlFont::DrawString(eGLView View, const FontCharString& Text, const nlVector2& Position, const nlColour& Colour, const nlColour& EffectColour, int Length, nlFont::TextPass Passes, bool FlipY, uintptr_t* pMatrix, nlColour* pOverrideColour) const
{
    float PositionY = Position.y;
    float CurrentY;
    float CurrentX;
    float StartingX;
    int renderAscent;
    if (FlipY)
    {
        renderAscent = -m_Metrics.RenderAscent;
    }
    else
    {
        renderAscent = m_Metrics.RenderAscent;
    }

    StartingX = Position.x;
    CurrentY = PositionY - (float)renderAscent;

    unsigned short EscapeBegin;
    int StringLength;
    if (Length == -1)
    {
        StringLength = nlStrLen(Text.m_pString);
    }
    else
    {
        StringLength = Length;
    }

    const GlyphInfo* pGlyph;
    glPoly2* pQuads = (glPoly2*)__builtin_alloca((unsigned long)(StringLength * sizeof(glPoly2)));
    glPoly2* pCurrentQuad = pQuads;

    gl_ScreenInfo* pScreenInfo = glGetScreenInfo();
    float PixelCenter = pScreenInfo->PixelCentre;
    unsigned long HandledChars = 0;
    unsigned long CurrentPage = 0;
    const unsigned short* pCurrentChar;
    StartingX += PixelCenter;
    CurrentY += PixelCenter;

    glSetDefaultState(false);
    glSetRasterState(GLS_AlphaBlend, 1);
    glSetRasterState(GLS_AlphaTest, 1);
    glSetRasterState(GLS_AlphaTestRef, 0);
    glSetRasterState(GLS_Culling, 0);
    glSetCurrentRasterState(glHandleizeRasterState());
    glSetCurrentProgram(glGetProgram("2d unlit diffuse"));

    void* pUserData = 0;
    if (m_bScissorBox)
    {
        struct GLScissorUserData
        {
            unsigned short xOrig;
            unsigned short yOrig;
            unsigned short wd;
            unsigned short ht;
        };

        pUserData = glUserAlloc(GLUD_Scissor, sizeof(GLScissorUserData), false);
        GLScissorUserData* pScissor = (GLScissorUserData*)glUserGetData(pUserData);
        pScissor->xOrig = m_scissorBox.X;
        pScissor->yOrig = m_scissorBox.Y;
        pScissor->wd = m_scissorBox.Width;
        pScissor->ht = m_scissorBox.Height;
    }

    float GlyphRenderHeightVOffset = (float)m_Metrics.RenderHeight * m_InvTexSize;

    nlColour OverrideColour = Colour;
    if (pOverrideColour != 0 && pOverrideColour->c[3] != 0)
    {
        OverrideColour = *pOverrideColour;
    }

    EscapeBegin = nlEscapeSequence::ESCAPE_BEGIN;
    const unsigned short* PushColourIndex = 0;
    nlColour LastPushedColour = OverrideColour;

    while (HandledChars < (unsigned long)StringLength)
    {
        bool useEffectTextures = false;
        if (Passes == PASS_Effect && m_TextureType == SplitFX)
        {
            useEffectTextures = true;
        }

        const unsigned long* textureHandles = useEffectTextures ? m_EffectTextureHandles : m_TextureHandles;
        glSetCurrentTexture(textureHandles[CurrentPage], GLTT_Diffuse);

        CurrentX = StartingX;
        unsigned long i = 0;
        pCurrentChar = Text.m_pString;

        while (*pCurrentChar != 0 && HandledChars < (unsigned long)StringLength && i < (unsigned long)StringLength)
        {
            unsigned long Char = *pCurrentChar;
            if (Char == EscapeBegin)
            {
                nlEscapeSequence escape(pCurrentChar);
                switch (escape.m_Type)
                {
                case ESC_COLOUR:
                    OverrideColour = escape.GetExtendedColour();

                    if (PushColourIndex < pCurrentChar)
                    {
                        if (OverrideColour.c[3] == 0)
                        {
                            LastPushedColour = Colour;
                            LastPushedColour.c[3] = 0;
                        }
                        else
                        {
                            LastPushedColour = OverrideColour;
                            LastPushedColour.c[3] = 0xFF;
                        }
                        PushColourIndex = pCurrentChar;
                    }

                    if (OverrideColour.c[3] == 0)
                    {
                        OverrideColour = Colour;
                    }
                    break;

                case ESC_NON_BREAKING_SPACE:
                    if (!m_bIsJapanese)
                    {
                        CurrentX += (float)((int)m_GlyphLookup[0].Offset + (int)m_GlyphLookup[0].Advance);
                    }
                    break;
                default:
                    break;
                }

                int consumed = (int)(escape.m_pEnd - pCurrentChar);
                i = consumed + i;
                i -= 1;
                if (CurrentPage == 0)
                {
                    HandledChars += consumed;
                }
                pCurrentChar = escape.m_pEnd - 1;
            }
            else
            {
                pGlyph = (Char > 0x7F) ? &m_pExtendedGlyphs[Char - 0x80] : &m_GlyphLookup[Char - 0x20];

                unsigned long Page = pGlyph->Page;
                if (Page > 0x10)
                {
                    nlPrintf("Font missing requested character");
                }
                else
                {
                    CurrentX += (float)pGlyph->Offset;
                    if (Page == CurrentPage)
                    {
                        pCurrentQuad->m_pos[1].x = CurrentX;
                        pCurrentQuad->m_pos[0].x = CurrentX;

                        float EndX = CurrentX + (float)pGlyph->RenderWidth;
                        pCurrentQuad->m_pos[3].x = EndX;
                        pCurrentQuad->m_pos[2].x = EndX;

                        pCurrentQuad->m_pos[3].y = CurrentY;
                        pCurrentQuad->m_pos[0].y = CurrentY;

                        int renderHeight;
                        if (FlipY)
                        {
                            renderHeight = -m_Metrics.RenderHeight;
                        }
                        else
                        {
                            renderHeight = m_Metrics.RenderHeight;
                        }

                        float EndY = CurrentY + (float)renderHeight;
                        pCurrentQuad->m_pos[2].y = EndY;
                        pCurrentQuad->m_pos[1].y = EndY;

                        pCurrentQuad->depth = 0.0f;

                        float GlyphU = pGlyph->uv.x;
                        pCurrentQuad->m_uv[1].x = GlyphU;
                        pCurrentQuad->m_uv[0].x = GlyphU;

                        float EndU = pGlyph->uv.x + (0.999f * ((float)pGlyph->RenderWidth * m_InvTexSize));
                        pCurrentQuad->m_uv[3].x = EndU;
                        pCurrentQuad->m_uv[2].x = EndU;

                        float GlyphV = pGlyph->uv.y;
                        pCurrentQuad->m_uv[3].y = GlyphV;
                        pCurrentQuad->m_uv[0].y = GlyphV;

                        float EndV = pGlyph->uv.y + GlyphRenderHeightVOffset;
                        pCurrentQuad->m_uv[2].y = EndV;
                        pCurrentQuad->m_uv[1].y = EndV;

                        pCurrentQuad->SetColour(OverrideColour);

                        pCurrentQuad++;
                        HandledChars++;
                    }

                    int FinalAdvance = (int)pGlyph->Advance;
                    if (pGlyph->HasKernPairs && pCurrentChar[1] != 0)
                    {
                        unsigned short* pKern = (unsigned short*)pCurrentChar;
                        KernPair kp = { { pKern[0], pKern[1] }, 0 };
                        KernPair* pValidKp = nlBSearch<KernPair, KernPair>(kp, m_pKernTable, m_KernTableSize);
                        if (pValidKp != 0)
                        {
                            FinalAdvance += pValidKp->Kern;
                        }
                    }

                    CurrentX += (float)FinalAdvance * m_Metrics.Spacing;
                }
            }

            pCurrentChar++;
            i++;
        }

        if (pOverrideColour != 0 && pOverrideColour->c[3] != 0)
        {
            OverrideColour = *pOverrideColour;
        }
        else
        {
            OverrideColour = Colour;
        }

        unsigned long UsedQuads = (unsigned long)(pCurrentQuad - pQuads);
        if (UsedQuads != 0)
        {
            if (m_bScissorBox)
            {
                glAttachPoly2(View, UsedQuads, pQuads, pMatrix, pUserData);
            }
            else
            {
                glAttachPoly2(View, UsedQuads, pQuads, pMatrix, 0);
            }
        }

        pCurrentQuad = pQuads;
        CurrentPage++;
    }

    if (m_TextureType == SplitFX && Passes == PASS_TextAndEffect)
    {
        DrawString(View, Text, Position, EffectColour, EffectColour, Length, PASS_Effect, View != 0, pMatrix, 0);
    }

    if (pOverrideColour != 0)
    {
        if (PushColourIndex != 0 && LastPushedColour.c[3] != 0)
        {
            *pOverrideColour = LastPushedColour;
            pOverrideColour->c[3] = 0xFF;
        }
        else
        {
            pOverrideColour->c[3] = 0;
        }
    }
}

/**
 * Offset/Address/Size: 0x150 | 0x80210A8C | size: 0x2C
 */
void nlFont::SetScissorBox(const ScissorBox& other) const
{
    m_scissorBox = other;
    m_bScissorBox = true;
}

/**
 * Offset/Address/Size: 0x144 | 0x80210A80 | size: 0xC
 */
void nlFont::DisableScissorBox() const
{
    m_bScissorBox = false;
}

/**
 * Offset/Address/Size: 0x0 | 0x8021093C | size: 0x144
 */
unsigned long nlFont::GetCharWidth(unsigned short FontChar, unsigned short PrevFontChar) const
{
    const GlyphInfo* pGlyph;
    unsigned short c = FontChar;

    if (c > 0x7F)
    {
        pGlyph = &m_pExtendedGlyphs[c - 0x80];
    }
    else
    {
        pGlyph = &m_GlyphLookup[c - 0x20];
    }

    if (pGlyph->UnicodeChar == 0xFFFF)
    {
        nlPrintf("Tried to use character that's not in the exported font (%c)\n", (unsigned short)FontChar);
    }

    signed char offset = pGlyph->Offset;
    unsigned short prevChar = PrevFontChar;
    unsigned char advance = pGlyph->Advance;
    unsigned long ret = advance + offset;

    if (prevChar != 0)
    {
        const GlyphInfo* pPrevGlyph;
        if (prevChar > 0x7F)
        {
            pPrevGlyph = &m_pExtendedGlyphs[prevChar - 0x80];
        }
        else
        {
            pPrevGlyph = &m_GlyphLookup[prevChar - 0x20];
        }

        if (pPrevGlyph->HasKernPairs)
        {
            KernPair kp = { { PrevFontChar, FontChar }, 0 };
            KernPair* pFoundKP = nlBSearch<KernPair, KernPair>(kp, m_pKernTable, m_KernTableSize);
            if (pFoundKP != NULL)
            {
                ret += pFoundKP->Kern;
            }
        }
    }

    return (unsigned long)(ret * m_Metrics.Spacing);
}
