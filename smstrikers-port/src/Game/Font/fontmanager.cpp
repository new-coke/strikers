#include "Game/Font/fontmanager.h"
#include "NL/nlBundleFile.h"
#include "NL/nlMemory.h"
#include "NL/nlString.h"
#include "NL/gl/glTexture.h"
#include "NL/nlBundleFile.h"

template <>
FontManager* nlSingleton<FontManager>::s_pInstance = 0;

/**
 * Offset/Address/Size: 0x4CC | 0x80209B60 | size: 0x70
 */
FontManager::FontManager()
    : m_fonts(8)
{
}

/**
 * Offset/Address/Size: 0x374 | 0x80209A08 | size: 0x158
 */
FontManager::~FontManager()
{
    DLListEntry<nlFont*>* head;
    DLListEntry<nlFont*>* current = nlDLRingGetStart(m_fonts.m_Head);
    head = m_fonts.m_Head;

    while (current != NULL)
    {
        delete current->entry;

        if (nlDLRingIsEnd(head, current) || current == NULL)
        {
            current = NULL;
        }
        else
        {
            current = current->m_next;
        }
    }

    m_fonts.Clear();
}

/**
 * Offset/Address/Size: 0x2A8 | 0x8020993C | size: 0xCC
 */
nlFont* FontManager::GetFontByHashID(unsigned long hashID)
{
    DLListEntry<nlFont*>* head;
    DLListEntry<nlFont*>* entry = nlDLRingGetStart(m_fonts.m_Head);
    head = m_fonts.m_Head;

    while (entry != NULL)
    {
        nlFont* font = entry->entry;
        if (hashID == font->m_Metrics.FontName)
        {
            return font;
        }

        if (nlDLRingIsEnd(head, entry) || entry == NULL)
        {
            entry = NULL;
        }
        else
        {
            entry = entry->m_next;
        }
    }

    nlPrintf("FontManager: Warning, failed to find font 0x%08x\n", hashID);

    DLListEntry<nlFont*>* start = nlDLRingGetStart(m_fonts.m_Head);
    if (start == NULL)
    {
        return NULL;
    }
    return start->entry;
}

static inline bool LoadFontDescription(BundleFile& fileBundle, unsigned long fileHashID, const char* szFontFileName, const char* szFontName, nlFont** pNewFont)
{
    BundleFileDirectoryEntry entry;
    if (!fileBundle.GetFileInfo(fileHashID, &entry, true))
    {
        return false;
    }

    char* fileData = (char*)nlMalloc(entry.m_length, 0x20, true);
    fileBundle.ReadFile(fileHashID, fileData, entry.m_length);

    *pNewFont = new (nlMalloc(sizeof(nlFont), 0x8, false)) nlFont();
    (*pNewFont)->Load(szFontName, fileData, nlStringHash(szFontFileName));

    delete[] fileData;
    return true;
}

static inline void LoadFontTexture(BundleFile& fileBundle, unsigned long fileHashID)
{
    BundleFileDirectoryEntry entry;
    if (fileBundle.GetFileInfo(fileHashID, &entry, true))
    {
        char* textureData = (char*)nlMalloc(entry.m_length, 0x20, true);
        fileBundle.ReadFile(fileHashID, textureData, entry.m_length);
        glTextureAdd(fileHashID, textureData, entry.m_length);
        delete[] textureData;
    }
}

static inline void AddFontEntry(BasicSlotPool<DLListEntry<nlFont*> >& alloc, DLListEntry<nlFont*>** head, nlFont* newFont)
{
    DLListEntry<nlFont*>* entry = NULL;
    alloc.Allocate(entry);

    if (entry != NULL)
    {
        entry->m_next = NULL;
        entry->m_prev = NULL;
        entry->entry = newFont;
    }

    nlDLRingAddEnd(head, entry);
}

/**
 * Offset/Address/Size: 0x0 | 0x80209694 | size: 0x2A8
 */
bool FontManager::LoadFont(const char* bundlePath, const char* fontName, const char* fontFileName)
{
    BundleFile bundleFile;
    BundleFileDirectoryEntry entry;
    char nameBuffer[0xFF];
    unsigned long hashID;
    nlFont* newFont = NULL;

    nlStrNCpy(nameBuffer, fontFileName, 0xFF);
    nlToLower(nameBuffer);

    bundleFile.Open(bundlePath);

    hashID = nlStringHash(fontName);
    if (!LoadFontDescription(bundleFile, hashID, nameBuffer, fontName, &newFont))
    {
        return false;
    }

    AddFontEntry(m_fonts.m_Allocator, &m_fonts.m_Head, newFont);

    for (unsigned long i = 0; i < newFont->m_PageCount; i++)
    {
        hashID = newFont->m_TextureHandles[i];
        bundleFile.GetFileInfo(hashID, &entry, true);
        LoadFontTexture(bundleFile, hashID);

        if (newFont->m_TextureType == SplitFX)
        {
            hashID = newFont->m_EffectTextureHandles[i];
            bundleFile.GetFileInfo(hashID, &entry, true);
            LoadFontTexture(bundleFile, hashID);
        }
    }

    bundleFile.Close();
    return true;
}
