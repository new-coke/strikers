#include "NL/nlLocalization.h"
#include "port/endian.h"
#if defined(STRIKERS_VITA) && defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "port/native_prompt_text.hpp"
#endif
#include "NL/nlMemory.h"
#include "NL/nlFile.h"
#include "NL/nlPrint.h"
#include <stdint.h>
#include <string.h>

extern const unsigned short LocalizationTableNotFound[] = { 'L', 'o', 'c', 'a', 'l', 'i', 'z', 'a', 't', 'i', 'o', 'n', ' ', 'T', 'a', 'b', 'l', 'e', ' ', 'N', 'o', 't', ' ', 'F', 'o', 'u', 'n', 'd', 0 };
extern const unsigned short MissingLocString[] = { 'm', 'i', 's', 's', 'i', 'n', 'g', ' ', 'l', 'o', 'c', ' ', 's', 't', 'r', 'i', 'n', 'g', 0 };

const unsigned long nlLocalization::LanguageId[] = {
    0x7A947B29,
    0xA93C2035,
    0xAAAD26B9,
    0xB482A4B5,
    0xBC0FCCA1,
    0x95F1D726,
    0x5F2F5E69,
    0x983D29BB,
    0x00012332,
};

char* nlLocalization::LanguageName[] = {
    "English",
    "French",
    "German",
    "Spanish",
    "Italian",
    "Japanese",
    "UKEnglish",
    "Longest",
    "Bob",
};

const char nlLocalization::Thumbprint[4] = { 'N', 'L', 'O', 'C' };

nlLocalization* g_pLocalization;

/**
 * Offset/Address/Size: 0x0 | 0x802107AC | size: 0x148
 */
unsigned char nlLocalization::Load(nlLanguage Language, bool ingameloc)
{
    m_pFile = NULL;
    m_LookupTable = NULL;
    m_FirstString = NULL;

    if ((unsigned int)Language >= (unsigned int)LangEnd)
        return 0;

    m_CurrentLanguage = Language;

    char Filename[64];
    if (ingameloc)
    {
        nlSNPrintf(Filename, 64, "art/fe/%s_game.loc", LanguageName[Language]);
    }
    else
    {
        nlSNPrintf(Filename, 64, "art/fe/%s.loc", LanguageName[Language]);
    }

    unsigned long FileSize = 0;
    u8* rawFile = (u8*)nlLoadEntireFile(Filename, &FileSize, 32, AllocateStart);

    if (rawFile == NULL)
        return 0;

    if (FileSize < sizeof(LOCHeader) || memcmp(rawFile, Thumbprint, 4) != 0)
    {
        nlFree(rawFile);
        return 0;
    }

    // The disc image remains big-endian. Validate every extent using decoded
    // locals before allocating/materializing the host representation.
    const u32 version = port_be32(rawFile + 0x04);
    const u32 language = port_be32(rawFile + 0x08);
    const u32 stringCount = port_be32(rawFile + 0x0C);
    const u32 flags = port_be32(rawFile + 0x10);
    const uint64_t lookupBytes = (uint64_t)stringCount * sizeof(StringLookup);
    const uint64_t stringsOffset = sizeof(LOCHeader) + lookupBytes;

    if (version != 1 || language != LanguageId[Language] || stringsOffset > FileSize ||
        ((FileSize - (unsigned long)stringsOffset) & 1u) != 0)
    {
        nlFree(rawFile);
        return 0;
    }

    const unsigned long stringWords =
        (FileSize - (unsigned long)stringsOffset) / sizeof(u16);
    const u8* rawLookup = rawFile + sizeof(LOCHeader);
    const u8* rawStrings = rawFile + (unsigned long)stringsOffset;

    // Every StringOffset is later added directly to m_FirstString. Prove not
    // only that it lands in the UTF-16 area, but that a terminator exists before
    // EOF so a malformed table cannot turn a text lookup into an OOB scan.
    for (u32 i = 0; i < stringCount; ++i)
    {
        const u32 stringOffset = port_be32(rawLookup + i * sizeof(StringLookup) + 4);
        if (stringOffset >= stringWords)
        {
            nlFree(rawFile);
            return 0;
        }

        bool terminated = false;
        for (unsigned long w = stringOffset; w < stringWords; ++w)
        {
            if (port_be16(rawStrings + w * sizeof(u16)) == 0)
            {
                terminated = true;
                break;
            }
        }
        if (!terminated)
        {
            nlFree(rawFile);
            return 0;
        }
    }

    size_t hostBytes = FileSize;
#if defined(STRIKERS_VITA) && defined(STRIKERS_VITA_NATIVE_RUNTIME)
    auto promptPlan = port::prompttext::plan(Language, rawLookup, stringCount, rawStrings, stringWords);
    if (promptPlan.extraWords <= (SIZE_MAX - hostBytes) / sizeof(u16))
        hostBytes += size_t(promptPlan.extraWords) * sizeof(u16);
    else
        promptPlan = {}; // Keep the original strings if the final allocation would overflow.
#endif
    u8* hostFile = (u8*)nlMalloc(hostBytes, 32, false);
    if (hostFile == NULL)
    {
        nlFree(rawFile);
        return 0;
    }
    memcpy(hostFile, rawFile, FileSize);

    LOCHeader* hostHeader = (LOCHeader*)hostFile;
    hostHeader->Version = version;
    hostHeader->Language = language;
    hostHeader->StringCount = stringCount;
    hostHeader->Flags = flags;

    StringLookup* hostLookup = (StringLookup*)(hostFile + sizeof(LOCHeader));
    for (u32 i = 0; i < stringCount; ++i)
    {
        hostLookup[i].hash = port_be32(rawLookup + i * sizeof(StringLookup));
        hostLookup[i].StringOffset =
            port_be32(rawLookup + i * sizeof(StringLookup) + 4);
    }

    u16* hostStrings = (u16*)(hostFile + (unsigned long)stringsOffset);
    for (unsigned long w = 0; w < stringWords; ++w)
        hostStrings[w] = port_be16(rawStrings + w * sizeof(u16));

#if defined(STRIKERS_VITA) && defined(STRIKERS_VITA_NATIVE_RUNTIME)
    port::prompttext::apply(promptPlan, hostLookup, hostStrings, stringWords);
#endif
    nlFree(rawFile);
    m_pFile = hostHeader;
    m_LookupTable = hostLookup;
    m_FirstString = hostStrings;
    return 1;
}

/**
 * Offset/Address/Size: 0x148 | 0x802108F4 | size: 0x48
 */
void nlLocalization::Initialize()
{
    nlLocalization* pLocalization = (nlLocalization*)nlMalloc(sizeof(nlLocalization), 8, false);
    if (pLocalization != NULL)
    {
        pLocalization->m_pFile = NULL;
        pLocalization->m_LookupTable = NULL;
        pLocalization->m_FirstString = NULL;
    }
    g_pLocalization = pLocalization;
}
