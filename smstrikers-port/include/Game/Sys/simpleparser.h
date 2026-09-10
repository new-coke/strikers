#ifndef _SIMPLEPARSER_H_
#define _SIMPLEPARSER_H_

#include "types.h"

class SimpleParser
{
public:
    SimpleParser();
    bool StartParsing(char* data, int size, bool bSpacesAreWhitespace);
    char* NextToken(bool bToLower);
    char* NextTokenOnLine(bool bToLower);
    bool AdvanceLine();
    bool SkipToEOL();
    bool AdvanceEnd();
    bool NextChar();
    bool IsWhitespace(char nCharacter);

    /* 0x000 */ char m_TokenBuffer[5][256];
    /* 0x500 */ char* m_CurrPos;
    /* 0x504 */ char* m_EndOfLine;
    /* 0x508 */ int m_AmountLeft;
    /* 0x50C */ int m_CurrentBuffer;
    /* 0x510 */ bool m_bSpacesAreWhitespace;
}; // size: 0x514

#endif // _SIMPLEPARSER_H_
