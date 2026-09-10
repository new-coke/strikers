#ifndef _NLLOCALIZATION_H_
#define _NLLOCALIZATION_H_

#include "types.h"

struct LOCHeader
{
    char Thumbprint[4];
    u32 Version;
    u32 Language;
    u32 StringCount;
    u32 Flags;
};

class nlLocalization
{
public:
    struct StringLookup
    {
        u32 hash;
        u32 StringOffset;

        operator unsigned long() const { return hash; }
    };

    enum nlLanguage
    {
        LangEnglish = 0,
        LangFrench = 1,
        LangGerman = 2,
        LangSpanish = 3,
        LangItalian = 4,
        LangJapanese = 5,
        LangUKEnglish = 6,
        LangLongestStrings = 7,
        LangBob = 8,
        LangEnd = 9,
    };

    unsigned char Load(nlLanguage Language, bool ingameloc);
    void Destroy();
    static void Initialize();

    LOCHeader* m_pFile;
    StringLookup* m_LookupTable;
    unsigned short* m_FirstString;
    nlLanguage m_CurrentLanguage;

    static char* LanguageName[];
    static const unsigned long LanguageId[];
    static const char Thumbprint[4];
};

extern nlLocalization* g_pLocalization;
extern const unsigned short LocalizationTableNotFound[];
extern const unsigned short MissingLocString[];

#endif // _NLLOCALIZATION_H_
