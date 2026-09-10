#ifndef _FESCROLLTEXT_H_
#define _FESCROLLTEXT_H_

#include "NL/nlBasicString.h"
#include "NL/nlFont.h"
#include "NL/nlFunction.h"

#include "Game/FE/tlTextInstance.h"

/* total size: 0x22C */
class FEScrollText
{
public:
    FEScrollText(TLTextInstance* controlText, int pos, int width);
    ~FEScrollText() { }

    void ApplyNewTextInstancePointer(TLTextInstance* controltext, float boxwidth, float boxheight);
    void Update(float fDeltaT);
    void SetDisplayMessage(const char* locMessage);
    void SetDisplayMessage(unsigned long hash);
    void SetDisplayMessage(const BasicString<unsigned short, Detail::TempStringAllocator>& theMessage);
    void SetMetrics(int pos, int width);

    /* 0x0,  */ TLTextInstance* m_controlText;
    /* 0x4,  */ BasicString<unsigned short, Detail::TempStringAllocator> m_message;
    /* 0x8,  */ int m_messageWidth;
    /* 0xC,  */ int m_pos;
    /* 0x10, */ int m_width;
    /* 0x14, */ float m_leftEdge;
    /* 0x18, */ float m_msgTime;
    /* 0x1C, */ unsigned short m_textBuffer[256];
    /* 0x21C */ Function<FnVoidVoid> m_messageFinishedCB;
    /* 0x224 */ unsigned char m_visible;
    /* 0x228 */ nlFont* m_textFont;
}; // total size: 0x22C

#endif // _FESCROLLTEXT_H_
