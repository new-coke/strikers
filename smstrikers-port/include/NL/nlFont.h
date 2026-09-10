#ifndef _NLFONT_H_
#define _NLFONT_H_

#include "NL/nlMath.h"
#include "NL/gl/gl.h"
#include "NL/nlAlgorithm.h"
#include "NL/nlMemory.h"
#include "NL/nlString.h"
#include "NL/nlTextEscape.h"

struct nlColour;
class FontCharString;

enum TextureType
{
    InvalidTextureType = 0,
    Colour = 1,
    Greyscale = 2,
    SplitFX = 3,
};

enum Distribution
{
    InvalidDistribution = 0,
    English = 1,
    InOrder = 2,
};

struct TextMetrics
{
    /* 0x00 */ unsigned long FontName;
    /* 0x04 */ unsigned short Height;
    /* 0x06 */ unsigned short RenderHeight;
    /* 0x08 */ unsigned short Ascent;
    /* 0x0A */ unsigned short RenderAscent;
    /* 0x0C */ unsigned short InternalLeading;
    /* 0x10 */ float Spacing;
    /* 0x14 */ float LineHeight;
}; // total size: 0x18

class nlFont
{
public:
    enum TextPass
    {
        PASS_None = 0,
        PASS_Text = 1,
        PASS_Effect = 2,
        PASS_TextAndEffect = 3,
    };

    struct ScissorBox
    {
        /* 0x0 */ unsigned short X;
        /* 0x2 */ unsigned short Y;
        /* 0x4 */ unsigned short Width;
        /* 0x6 */ unsigned short Height;
    }; // total size: 0x8

    struct GlyphInfo
    {
        /* 0x0 */ nlVector2 uv;
        /* 0x8 */ unsigned char Advance;
        /* 0x9 */ unsigned char RenderWidth;
        /* 0xA */ signed char Offset;
        /* 0xB */ unsigned char Page : 4;
        /* 0xB */ unsigned char HasKernPairs : 1;
        union
        {
            /* 0xC */ unsigned short UnicodeChar;
            /* 0xC */ unsigned short hash;
        };

        operator unsigned long() const { return UnicodeChar; }
        static int SortProc(const GlyphInfo* pa, const GlyphInfo* pb) { return pa->UnicodeChar - pb->UnicodeChar; }
    }; // total size: 0x10

    struct KernPair
    {
        union
        {
            struct
            {
                /* 0x0 */ unsigned short A;
                /* 0x2 */ unsigned short B;
            } s;
            /* 0x0 */ unsigned long hash;
        };
        /* 0x4 */ int Kern;

        operator unsigned long() const { return hash; }
        static int SortProc(const KernPair* pa, const KernPair* pb) { return pa->hash - pb->hash; }
    }; // total size: 0x8

    unsigned long GetCharWidth(unsigned short FontChar, unsigned short PrevFontChar) const;
    void DisableScissorBox() const;
    void SetScissorBox(const ScissorBox& other) const;
    void DrawString(eGLView View, const FontCharString& Text, const nlVector2& Position, const nlColour& Colour, const nlColour& EffectColour, int Length, nlFont::TextPass Passes, bool FlipY, uintptr_t* pMatrix, nlColour* pOverrideColour) const;
    unsigned char Load(const char* szFontName, char* pFontDescData, unsigned long HashId);
    void Unload();
    bool IsJapanese() const { return m_bIsJapanese; }
    void SetIsJapanese(bool isJapanese) { m_bIsJapanese = isJapanese; }

    ~nlFont();
    nlFont();

    /* 0x000 */ unsigned long m_PageCount;
    /* 0x004 */ unsigned long m_TextureHandles[16];
    /* 0x044 */ unsigned long m_EffectTextureHandles[16];
    /* 0x084 */ TextureType m_TextureType;
    /* 0x088 */ mutable unsigned char m_bScissorBox;
    /* 0x08A */ mutable ScissorBox m_scissorBox;
    /* 0x094 */ Distribution m_Distribution;
    /* 0x098 */ unsigned long m_CharacterSet;
    /* 0x09C */ unsigned long m_PageSize;
    /* 0x0A0 */ char m_FontName[32];
    /* 0x0C0 */ TextMetrics m_Metrics;
    /* 0x0D8 */ float m_InvTexSize;
    /* 0x0DC */ GlyphInfo m_GlyphLookup[95];
    /* 0x6CC */ GlyphInfo* m_pExtendedGlyphs;
    /* 0x6D0 */ unsigned long m_ExtendedGlyphCount;
    /* 0x6D4 */ KernPair* m_pKernTable;
    /* 0x6D8 */ unsigned long m_KernTableSize;
private:
    /* 0x6DC */ bool m_bIsJapanese;
}; // total size: 0x6DC (G4QE01), 0x6E0 (G4QJ01)

class FontCharString
{
public:
    FontCharString() { }
    ~FontCharString()
    {
        if (m_InternalBuffer != 0)
        {
            delete[] m_pString;
        }
    }
    template <typename T>
    FontCharString(const T*, const nlFont*, T*);

    /* 0x0 */ unsigned short* m_pString;
    /* 0x4 */ unsigned char m_InternalBuffer;
}; // total size: 0x8

template <typename T>
FontCharString::FontCharString(const T* Source, const nlFont* pFont, T* pBuffer)
{
    m_InternalBuffer = 0;
    if (pBuffer == 0)
    {
        m_pString = (unsigned short*)nlMalloc((nlStrLen<T>(Source) + 1) * sizeof(T), 8, false);
        m_InternalBuffer = 1;
    }
    else
    {
        m_pString = pBuffer;
    }

    unsigned short* dest = m_pString;
    const T* src = Source;
    unsigned short escBegin = nlEscapeSequence::ESCAPE_BEGIN;
    unsigned int ch;

    while ((ch = *src) != 0)
    {
        if (ch == escBegin)
        {
            nlEscapeSequence EscSeq(src);
            const T* end = EscSeq.m_pEnd;
            while (src < end)
            {
                *dest++ = *src++;
            }
        }
        else
        {
            if (ch <= 0x7F)
            {
                ch &= 0xFFFF;
            }
            else
            {
                nlFont::GlyphInfo key;
                key.UnicodeChar = ch;
                nlFont::GlyphInfo* result;
                if (pFont->m_pExtendedGlyphs != 0 && pFont->m_ExtendedGlyphCount != 0 && (result = nlBSearch<nlFont::GlyphInfo, nlFont::GlyphInfo>(key, pFont->m_pExtendedGlyphs, pFont->m_ExtendedGlyphCount)) != 0)
                {
                    ch = ((result - pFont->m_pExtendedGlyphs) + 0x80) & 0xFFFF;
                }
                else
                {
                    ch = 0x30;
                    while (((unsigned short)ch > 0x7F ? pFont->m_pExtendedGlyphs[(unsigned short)ch - 0x80] : pFont->m_GlyphLookup[(unsigned short)ch - 0x20]).UnicodeChar == 0xFFFF)
                    {
                        ch++;
                    }
                }
            }
            *dest++ = ch;
            src++;
        }
    }

    *dest = 0;
}

#endif // _NLFONT_H_
