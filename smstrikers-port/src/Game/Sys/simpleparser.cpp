#include "Game/Sys/simpleparser.h"

#include "ctype.h"
#include "NL/nlString.h"

/**
 * Offset/Address/Size: 0x988 | 0x801D732C | size: 0x10
 */
SimpleParser::SimpleParser()
{
    m_EndOfLine = nullptr;
    m_CurrentBuffer = 0;
}

/**
 * Offset/Address/Size: 0x81C | 0x801D71C0 | size: 0x16C
 */
bool SimpleParser::StartParsing(char* data, int size, bool bSpacesAreWhitespace)
{
    m_EndOfLine = data;
    m_AmountLeft = size;
    m_bSpacesAreWhitespace = bSpacesAreWhitespace;
    return AdvanceLine();
}

/**
 * Offset/Address/Size: 0x33C | 0x801D6CE0 | size: 0x4E0
 */
char* SimpleParser::NextToken(bool bToLower)
{
    char* retval = NextTokenOnLine(bToLower);
    if (retval == NULL)
    {
        if (!AdvanceLine())
            return NULL;
        retval = NextTokenOnLine(bToLower);
    }
    return retval;
}

/**
 * Offset/Address/Size: 0x160 | 0x801D6B04 | size: 0x1DC
 */
char* SimpleParser::NextTokenOnLine(bool bToLower)
{
    if (m_CurrPos == m_EndOfLine)
        return NULL;

    for (;;)
    {
        if (!IsWhitespace(*m_CurrPos))
            break;
        if (!NextChar())
            return NULL;
    }

    unsigned int tokenSize = 0;
    do
    {
        if (bToLower)
            m_TokenBuffer[m_CurrentBuffer][tokenSize] = nlToLower<char>(*m_CurrPos);
        else
            m_TokenBuffer[m_CurrentBuffer][tokenSize] = *m_CurrPos;
        tokenSize++;
    } while (NextChar() && !IsWhitespace(*m_CurrPos));

    m_TokenBuffer[m_CurrentBuffer][tokenSize] = 0;
    int nPrevBuffer = m_CurrentBuffer;
    m_CurrentBuffer = (nPrevBuffer + 1) % 5;
    return m_TokenBuffer[nPrevBuffer];
}

bool SimpleParser::IsWhitespace(char nCharacter)
{
    bool isWhitespace = false;
    int nChar = (s8)nCharacter;
    if ((__ctype_map[(u8)nChar] & 0x6) != 0 && (m_bSpacesAreWhitespace || nChar != ' '))
        isWhitespace = true;
    return isWhitespace;
}

bool SimpleParser::NextChar()
{
    if (m_CurrPos == m_EndOfLine)
        return false;
    m_CurrPos++;
    return true;
}

bool SimpleParser::AdvanceEnd()
{
    if (m_AmountLeft <= 1)
        return false;
    m_EndOfLine++;
    m_AmountLeft--;
    return true;
}

bool SimpleParser::SkipToEOL()
{
    do
    {
        if (!AdvanceEnd())
            return false;
    } while (*m_EndOfLine != '\n');
    return true;
}

/**
 * Offset/Address/Size: 0x0 | 0x801D69A4 | size: 0x160
 */
bool SimpleParser::AdvanceLine()
{
    if (m_AmountLeft <= 1)
        return false;

    for (;;)
    {
        if (IsWhitespace(*m_EndOfLine))
        {
            if (!AdvanceEnd())
                return false;
        }
        else if (*m_EndOfLine == '#')
        {
            if (!SkipToEOL())
                return false;
        }
        else
        {
            break;
        }
    }

    m_CurrPos = m_EndOfLine;
    while (AdvanceEnd() && *m_EndOfLine != '\n')
    {
    }
    return true;
}
