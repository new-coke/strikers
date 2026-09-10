#ifndef _TLTEXTINSTANCE_H_
#define _TLTEXTINSTANCE_H_

#include "types.h"
#include "NL/nlColour.h"
#include "NL/gl/gl.h"
#include "NL/nlFont.h"
#include "NL/nlMath.h"
#include "NL/nlTextBox.h"

#include "Game/FE/tlInstance.h"

struct FETextLibObjectAttributes
{
    /* 0x0 */ nlColour EffectColour;
    /* 0x4 */ nlVector2 BoxSize;
}; // total size: 0xC

class TLTextInstance : public TLInstance
{
public:
    void SetScissorBox(u16 left, u16 top, u16 width, u16 height);
    void SetString(const unsigned short* utf16);
    void DisableScissorBox();
    void SetStringId(const char* id);
    const unsigned short* GetString() const;
    void SetMatrix(nlMatrix4* pMatrix)
    {
        m_DrawInfo.pMatrix = pMatrix;
    }
    void Render(eGLView view, const nlColour& colour) const;

    /* 0x80 */ unsigned long m_LocStrId;
    /* 0x84 */ FETextLibObjectAttributes m_OverloadedAttributes;
    /* 0x90 */ unsigned long m_OverloadFlags;
    /* 0x94 */ mutable nlTextBox::StringDrawInfo m_DrawInfo;
    /* 0xEC */ FontCharString* m_pFontString;
    /* 0xF0 */ unsigned long m_DrawOptions;
    /* 0xF4 */ const unsigned short* m_wcUserString;
    /* 0xF8 */ bool m_UseScissorRect;
    /* 0xFA */ nlFont::ScissorBox m_ScissorRect;

}; // total size: 0x104

#endif // _TLTEXTINSTANCE_H_
