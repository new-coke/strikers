#include "NL/nlTextEscape.h"

#include "NL/nlAlgorithm.h"


const unsigned long nlEscapeSequence::ESCAPE_DEFN[ESC_COUNT] = {
    0x00000000,
    0x7B000000,
    0x6E627300,
    0x636C7200,
    0x70000000,
};

nlEscapeSequence::ESCAPE_LOOKUP nlEscapeSequence::s_EscapeLookup[ESC_COUNT];

const unsigned short nlEscapeSequence::ESCAPE_BEGIN = 0x007B;

class EscapeSorter
{
public:
    EscapeSorter()
    {
        for (int i = 0; i < ESC_COUNT; i++)
        {
            nlEscapeSequence::s_EscapeLookup[i].hash = nlEscapeSequence::ESCAPE_DEFN[i];
            nlEscapeSequence::s_EscapeLookup[i].type = (ESCAPE_TYPE)i;
        }
        nlQSort<nlEscapeSequence::ESCAPE_LOOKUP>(nlEscapeSequence::s_EscapeLookup, ESC_COUNT, &nlDefaultQSortComparer<nlEscapeSequence::ESCAPE_LOOKUP>);
    }
};

static EscapeSorter s_EscapeSorter;

/**
 * Offset/Address/Size: 0xB0 | 0x802125CC | size: 0x174
 */
nlEscapeSequence::nlEscapeSequence(const unsigned short* str)
{
    char Seq[4] = { 0, 0, 0, 0 };
    const unsigned short* ExtendedStart = 0;
    const unsigned short* p = str;
    char* pSeq = Seq;
    unsigned long Char = 0;
    unsigned long key;
    ESCAPE_LOOKUP* pEscape;
    ESCAPE_TYPE type;

    while (true)
    {
        unsigned long ch = p[1];

        if (ch == '}')
        {
            break;
        }

        if (ch == ':')
        {
            const unsigned short* pExtended = str;
            pExtended += Char;
            ExtendedStart = pExtended + 2;
            break;
        }

        if (Char < 4)
        {
            *pSeq = (char)ch;
        }

        p++;
        pSeq++;
        Char++;
    }

    if (ExtendedStart != 0)
    {
        nlEscapeSequence* pSelf = this;

        for (Char = 0; Char < 15; Char++)
        {
            unsigned long ch = ExtendedStart[Char];
            if (ch == '}')
            {
                break;
            }

            pSelf->m_Extended[Char] = (unsigned short)ch;
        }
    }

    m_Extended[Char] = 0;
    m_pEnd = (ExtendedStart ? ExtendedStart : str + 1) + Char + 1;

    // PORT: was *(unsigned long*)Seq, eight bytes off a four-byte buffer.
    key = ((unsigned long)(unsigned char)Seq[0] << 24)
        | ((unsigned long)(unsigned char)Seq[1] << 16)
        | ((unsigned long)(unsigned char)Seq[2] << 8)
        | ((unsigned long)(unsigned char)Seq[3]);
    pEscape = nlBSearch(key, s_EscapeLookup, ESC_COUNT);
    if (pEscape != 0)
    {
        type = pEscape->type;
    }
    else
    {
        type = ESC_UNKNOWN;
    }
    m_Type = type;
}

/**
 * Offset/Address/Size: 0x0 | 0x8021251C | size: 0xB0
 */
nlColour nlEscapeSequence::GetExtendedColour()
{
    unsigned short str[3] = { 0, 0, 0 };
    nlColour colour;

    if ((int)m_Extended[0] == 0x70)
    {
        colour.c[3] = 0;
        return colour;
    }

    int channel = 0;
    for (channel = 0; channel < 3; channel++)
    {
        str[0] = m_Extended[channel * 2];
        str[1] = m_Extended[channel * 2 + 1];
        // PORT: was (u8)wcstoul(str, 0, 16). str is unsigned short[3] and the tree is built -fshort-wchar.
        unsigned long value = 0;
        for (int digit = 0; digit < 2; digit++)
        {
            const unsigned short ch = str[digit];
            unsigned long d;
            if (ch >= '0' && ch <= '9')
                d = (unsigned long)(ch - '0');
            else if (ch >= 'a' && ch <= 'f')
                d = (unsigned long)(ch - 'a') + 10;
            else if (ch >= 'A' && ch <= 'F')
                d = (unsigned long)(ch - 'A') + 10;
            else
                break;
            value = value * 16 + d;
        }
        colour.c[channel] = (u8)value;
    }
    colour.c[3] = 0xFF;
    return colour;
}
