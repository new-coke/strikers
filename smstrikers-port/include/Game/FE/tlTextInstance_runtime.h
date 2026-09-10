#ifndef _TLTEXTINSTANCE_RUNTIME_H_
#define _TLTEXTINSTANCE_RUNTIME_H_

#include "types.h"

#include "NL/gl/gl.h"

#include "NL/nlColour.h"
#include "NL/nlMath.h"

#include "Game/FE/tlInstance.h"

#include "NL/nlFont.h"

struct Row
{
    /* 0x00 */ unsigned short XOffset;
    /* 0x02 */ unsigned short FirstChar;
}; // total size: 0x4

struct StringDrawInfo
{
    /* 0x00 */ const nlFont* pFont;
    /* 0x04 */ const unsigned short* String;
    /* 0x08 */ nlMatrix4* pMatrix;
    /* 0x0C */ unsigned long DrawOptions;
    /* 0x10 */ unsigned short RowCount;
    /* 0x12 */ signed short YOffset;
    /* 0x14 */ Row Rows[17];
}; // total size: 0x58

struct FETextLibObjectAttributes
{
    /* 0x0 */ nlColour EffectColour;
    /* 0x4 */ nlVector2 BoxSize;
}; // total size: 0xC

class TLTextInstance : public TLInstance
{
public:
    void SetScissorBox(unsigned short, unsigned short, unsigned short, unsigned short);
    void SetString(const unsigned short*);
    void DisableScissorBox();
    void Render(eGLView, const nlColour&) const;
    unsigned short* GetString() const;

    /* 0x80 */ unsigned long m_LocStrId;
    /* 0x84 */ FETextLibObjectAttributes m_OverloadedAttributes;
    /* 0x90 */ unsigned long m_OverloadFlags;
    /* 0x94 */ StringDrawInfo m_DrawInfo;
    /* 0xEC */ FontCharString* m_pFontString;
    /* 0xF0 */ unsigned long m_DrawOptions;
    /* 0xF4 */ const unsigned short* m_wcUserString;
    /* 0xF8 */ bool m_UseScissorRect;
    /* 0xFA */ ScissorBox m_ScissorRect;
}; // total size: 0x104

#endif // _TLTEXTINSTANCE_RUNTIME_H_
