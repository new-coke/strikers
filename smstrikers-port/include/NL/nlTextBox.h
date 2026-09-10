#ifndef _NLTEXTBOX_H_
#define _NLTEXTBOX_H_

#include "NL/nlMath.h"
#include "NL/nlFont.h"
#include "NL/nlColour.h"
#include "NL/gl/gl.h"
#include "NL/gl/glMatrix.h"

struct Row
{
    /* 0x0 */ unsigned short XOffset;
    /* 0x2 */ unsigned short FirstChar;
}; // total size: 0x4

class nlTextBox
{
public:
    enum DrawOptions
    {
        AlignLeft = 0,
        AlignCenter = 1,
        AlignRight = 2,
        VAlignTop = 0,
        VAlignCenter = 0x10,
        VAlignBottom = 0x20,
        ClipBottom = 0x100,
        UseFullHeight = 0x200,
        DontWrapOnSpaces = 0x400,
        FlipY = 0x800,
    };

    struct StringDrawInfo
    {
        static const unsigned long MAX_ROWS = 16;

        /* 0x00 */ const nlFont* pFont;
        /* 0x04 */ const unsigned short* String;
        /* 0x08 */ const nlMatrix4* pMatrix;
        /* 0x0C */ unsigned long DrawOptions;
        /* 0x10 */ unsigned short RowCount;
        /* 0x12 */ signed short YOffset;
        /* 0x14 */ Row Rows[MAX_ROWS + 1];
    }; // total size: 0x58

    nlTextBox();

    static void DrawString(const nlTextBox::StringDrawInfo& DrawInfo, const nlVector2& DrawAt, const nlColour& Color, eGLView View);
    static void ProcessString(const FontCharString* pString, const nlFont* pFont, const nlVector2& BoxSize, unsigned long DrawOptions, const nlMatrix4* pMatrix, nlTextBox::StringDrawInfo& DrawInfo);

private:
    /* 0x00 */ nlFont* m_pFont;
    /* 0x04 */ nlVector2 m_Position;
    /* 0x0C */ nlVector2 m_Size;
    /* 0x14 */ unsigned long m_DrawOptions;
    /* 0x18 */ nlColour m_Colour;
    /* 0x1C */ nlMatrix4* m_pMatrix;
    /* 0x20 */ FontCharString* m_FontString;
    /* 0x24 */ StringDrawInfo m_DrawInfo;
}; // total size: 0x7C

#endif // _NLTEXTBOX_H_
