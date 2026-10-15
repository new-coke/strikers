// PlatTexture uses 0x50544558 (ASCII "PTEX") as an in-memory type tag.
// Constructors initialize the tag, and glx_GetTex checks it when a texture handle may be a direct pointer.
// It is not an on-disk texture file signature.

#include "NL/nlString.h"
#include "NL/nlDebug.h"
#include "NL/glx/glxTexture.h"
#include "port/endian.h"
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "port/native_match_runtime.hpp"
#include "port/native_graphics.hpp"
#include <algorithm>
#endif
#include "dolphin/os.h"
#include "port/texture_packs.h"
#include <stdlib.h>
extern "C" int port_region_owns(const void*);  // src/platform/memalloc.cpp

#include "NL/nlAVLTree.h"
#include "NL/nlList.h"
#include "NL/nlAlgorithm.h"
#include "NL/nlFile.h"
#include "NL/nlFileGC.h"
#include "NL/gl/glTexture.h"
#include "NL/gl/glMemory.h"
#include "NL/glx/glxMemory.h"
#include "NL/gc/gcSwizzler.h"
#include "dolphin/gx/GXTexture.h"
#include "Game/GL/GLInventory.h"
#include "Game/Sys/debug.h"

// PORT: the definition lives here.
#include "NL/nlWare.h"
extern GLInventory glInventory;

static glxTextureLoadCallback_t glxTextureLoad_cb;
static int currentMarkerLevel = 0;
static bool glx_bGridMode = false;

static nlAVLTree<unsigned long, PlatTexture*, DefaultKeyCompare<unsigned long> >* textures[16];
static nlListContainer<PlatTexture*> gridTextures;
static unsigned long nGridMemory;
static PlatTexture texobj;

static PlatTexture* glx_MakeGridTexture(int width, int height);

static inline void (TexDestructor::* glx_GetTexDestructorCallback())(const unsigned long&, PlatTexture**)
{
    return &TexDestructor::CallDestructor;
}

#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
static bool glxApplyNativeTexture(PlatTexture* texture,const void* bytes,size_t size)
{
    port::nativegxm::TextureUpload image;uint32_t semantic;uint8_t bits[4];bool missing;
    if(!texture || !port::assets::texture({static_cast<const uint8_t*>(bytes),size},image,&semantic,bits,&missing))return false;
    texture->m_Width=image.width;texture->m_Height=image.height;
    texture->m_Levels=texture->m_MaxLevel=image.mipCount;texture->m_Format=eGXTextureFormat(semantic);
    texture->m_nPaletteEntries=0;texture->m_PaletteData=NULL;texture->m_bMissingTexture=missing;
    texture->m_SwizzledData=NULL;texture->m_NativeDataSize=size;
    memcpy(texture->m_Bits,bits,4);return port::directmatch::admit_texture(texture,image);
}
static bool glxLoadNativeBundle(nlFile* file,unsigned size)
{
    uint8_t header[16];
    if(size<16)return false;
    nlRead(file,header,16);
    if(static_cast<GCFile*>(file)->GetReadStatus()!=DVD_STATE_END || memcmp(header,"NSTB0001",8) || port::assets::word(header+8)!=size)return false;
    const size_t count=port::assets::word(header+12);
    if(count>(size-16)/16)return false;
    std::vector<uint8_t> dictionary(count*16);
    nlRead(file,dictionary.data(),dictionary.size());
    if(static_cast<GCFile*>(file)->GetReadStatus()!=DVD_STATE_END)return false;
    size_t previous=16+dictionary.size(),largest=0;
    for(size_t i=0;i<count;++i) {
        const auto* entry=dictionary.data()+i*16;
        const size_t at=port::assets::word(entry+4),n=port::assets::word(entry+8);
        if(port::assets::word(entry+12) || at%16 || at<previous || at>size || n>size-at || n<64 || n>16u*1024u*1024u+64)return false;
        previous=at+n;largest=std::max(largest,n);
    }
    std::vector<uint8_t> scratch(largest);
    for(size_t i=0;i<count;++i) {
        const auto* entry=dictionary.data()+i*16;
        const auto ident=port::assets::word(entry),at=port::assets::word(entry+4),n=port::assets::word(entry+8);
        nlSeek(file,at,0);
        nlRead(file,scratch.data(),n);
        if(static_cast<GCFile*>(file)->GetReadStatus()!=DVD_STATE_END)return false;
        const auto handle=glxTextureLoad_cb?glxTextureLoad_cb(ident):ident;
        if(handle==uint32_t(-1))continue;
        if(glTextureLoad(handle))glplatTextureReplace(handle,scratch.data(),n);
        else glplatTextureAdd(handle,scratch.data(),n);
        if(!glx_GetTex(handle,false,false))return false;
    }
    return true;
}
#endif

/**
 * Offset/Address/Size: 0x19DC | 0x801B8C98 | size: 0x10
 */
glxTextureLoadCallback_t glx_SetLoadCallback(glxTextureLoadCallback_t callback)
{
    unsigned long (*oldCallback)(unsigned long) = glxTextureLoad_cb;
    glxTextureLoad_cb = callback;
    return oldCallback;
}

/**
 * Offset/Address/Size: 0x19D4 | 0x801B8C90 | size: 0x8
 */
int glx_GetTexMarkerLevel()
{
    return currentMarkerLevel;
}

/**
 * Offset/Address/Size: 0x19C4 | 0x801B8C80 | size: 0x10
 */
void glx_AdvanceTexMarkerLevel()
{
    currentMarkerLevel += 1;
}

/**
 * Offset/Address/Size: 0x197C | 0x801B8C38 | size: 0x48
 */
inline void TexDestructor::CallDestructor(const unsigned long& index, PlatTexture** tex)
{
    PlatTexture* pTex;
    void* linearData;

    pTex = *tex;
    // PORT: the tree can hold records whose arena storage was already reclaimed, check the 'PTEX' tag before trusting the fields.
    if (pTex != NULL && pTex->m_Magic == 0x50544558)
    {
        linearData = pTex->m_LinearData;
        if (linearData != NULL)
        {
            nlFree(linearData);
            pTex->m_LinearData = NULL;
        }
    }
}

/**
 * Offset/Address/Size: 0x18DC | 0x801B8B98 | size: 0xA0
 */
void glx_BackupTexMarkerLevel(int level)
{
    #if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    port::directmatch::on_texture_release(level);
    #endif
    while (currentMarkerLevel != level)
    {
        TexDestructor texDtor;
        textures[currentMarkerLevel]->Walk<TexDestructor>(&texDtor, glx_GetTexDestructorCallback());
        textures[currentMarkerLevel]->Clear();
        currentMarkerLevel -= 1;
    }
}

/**
 * Offset/Address/Size: 0x1838 | 0x801B8AF4 | size: 0xA4
 */
void glxInitTex()
{
    currentMarkerLevel = -1;
    for (int level = 0; level < 16; level++)
    {
        textures[level] = new (nlMalloc(sizeof(nlAVLTree<unsigned long, PlatTexture*, DefaultKeyCompare<unsigned long> >), 8, 0)) nlAVLTree<unsigned long, PlatTexture*, DefaultKeyCompare<unsigned long> >();
    }
    currentMarkerLevel += 1;
}

/**
 * Offset/Address/Size: 0x1828 | 0x801B8AE4 | size: 0x10
 */
bool glx_SetGridMode(bool bGrid)
{
    bool prevGridMode = glx_bGridMode;
    glx_bGridMode = bGrid;
    return prevGridMode;
}

/**
 * Offset/Address/Size: 0x12EC | 0x801B85A8 | size: 0x53C
 */
static PlatTexture* glx_MakeGridTexture(int w, int h)
{
    u8 bits[4] = { 5, 6, 5, 0 };
    PlatTexture* pTex = new (nlMalloc(sizeof(PlatTexture), 8, false)) PlatTexture();
    pTex->Create(w, h, GXTex_RGB565, 1, true, true);
    memcpy(pTex->m_Bits, bits, sizeof(pTex->m_Bits));
    u16 gridColor = 0xFFFF;
    int x;
    int y;
    u16* pData = (u16*)pTex->m_LinearData;

    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            int gridx = x / 4;
            if ((y / 4) & 1)
            {
                pData[y * w + x] = (gridx & 1) == 0 ? 0 : gridColor;
            }
            else
            {
                pData[y * w + x] = (gridx & 1) ? 0 : gridColor;
            }
        }
    }

    // Update grid memory counter - includes sizeof(PlatTexture)
    nGridMemory += h * (w << 1) + 0x50;
    tDebugPrintManager::Print(DC_GL, "grid [%d %d] now using %uKB\n", w, h, nGridMemory / 1024);

    pTex->Swizzle(true);
    pTex->Prepare();

    return pTex;
}

inline bool IsValidTextureDimension(int dimension)
{
    bool validDimensions = false;
    switch (dimension)
    {
    case 4:
    case 8:
    case 16:
    case 32:
    case 64:
    case 128:
    case 256:
    case 512:
        validDimensions = true;
        break;
    default:
        validDimensions = false;
        break;
    }
    return validDimensions;
}

#pragma dont_inline on
/**
 * Offset/Address/Size: 0x1128 | 0x801B83E4 | size: 0x1C4
 */
static PlatTexture* glx_GetGridTexture(int width, int height)
{
    if (!IsValidTextureDimension(width) || !IsValidTextureDimension(height))
    {
        return nullptr;
    }

    if (width < 8 || height < 8)
    {
        return nullptr;
    }

    ListEntry<PlatTexture*>* current = gridTextures.m_Head;
    while (current != nullptr)
    {
        PlatTexture* texture = current->entry;
        if (width == texture->m_Width && height == texture->m_Height)
        {
            return texture;
        }
        current = current->next;
    }

    PlatTexture* newTexture = glx_MakeGridTexture(width, height);
    ListEntry<PlatTexture*>* listEntry = new (nlMalloc(sizeof(ListEntry<PlatTexture*>), 8, false)) ListEntry<PlatTexture*>(newTexture);

    nlListAddStart<ListEntry<PlatTexture*> >(&gridTextures.m_Head, listEntry, &gridTextures.m_Tail);
    return gridTextures.m_Head->entry;
}
#pragma dont_inline off

#pragma dont_inline on
/**
 * Offset/Address/Size: 0xF94 | 0x801B8250 | size: 0x194
 */
PlatTexture* glx_GetTex(uintptr_t handle, bool bMissingFatal, bool bAllowGrids)
{
    PlatTexture** tex;
    GLTextureAnim* pAnim;
    int index;

    // PORT: was `(handle & 0xFF000000) + 0x80000000 == 0`, cached MEM1 in the console memory map.
    bool directPointer = port_region_owns((const void*)handle) != 0;
#if defined(STRIKERS_VITA)
    directPointer = directPointer || glxVitaResourceArenaOwns((const void*)handle);
#endif
    if (directPointer)
    {
        // PORT: and `*(unsigned long*)handle` read four bytes on console and reads eight here.
        if (*(const u32*)handle == 0x50544558)
        {
            return (PlatTexture*)handle;
        }
    }

    pAnim = glInventory.GetTextureAnim(handle);
    if (pAnim != NULL)
    {
        handle = pAnim->GetTexture(-1)->textureHandle;
    }

    for (index = currentMarkerLevel; index >= 0; index--)
    {
        if (textures[index]->FindGet(handle, &tex))
        {
            if (bAllowGrids && glx_bGridMode)
            {
                index = (u32)(u16)(*tex)->m_Height >> 2;
                PlatTexture* pTex = glx_GetGridTexture((u32)(*tex)->m_Width >> 2, index);
                if (pTex != NULL)
                {
                    return pTex;
                }
            }
            return *tex;
        }
    }

    if (bMissingFatal)
    {
        tDebugPrintManager::Print(DC_GLPLAT, "texture 0x%08X not found\n", handle);
    }
    return NULL;
}
#pragma dont_inline off

/**
 * Offset/Address/Size: 0xF00 | 0x801B81BC | size: 0x94
 */
bool glx_AddTex(uintptr_t handle, PlatTexture* pTex)
{
    nlAVLTree<unsigned long, PlatTexture*, DefaultKeyCompare<unsigned long> >* textureTree;

    if ((pTex == NULL) || (handle == -1))
    {
        return false;
    }
    textureTree = textures[currentMarkerLevel];

    textureTree->Add(handle, pTex);
    return true;
}

/**
 * Offset/Address/Size: 0xBD4 | 0x801B7E90 | size: 0x32C
 */
PlatTexture* glx_MakeTexture(GXTextureHeader* header, uintptr_t texhandle, unsigned long fileSize)
{
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    PlatTexture* texture=glx_CreatePlatTexture();
    if(!glxApplyNativeTexture(texture,header,fileSize)) {
        OSReport("[native-texture] converted texture required or invalid: %08lx\n",(unsigned long)texhandle);
        return NULL;
    }
    return texture;
#else

    if (header == NULL || fileSize < sizeof(GXTextureHeader))
    {
        OSReport("[texture] invalid bundle entry: handle=%08lX size=%lu (header=%u)\n",
                 (unsigned long)texhandle, fileSize, (unsigned)sizeof(GXTextureHeader));
        return NULL;
    }

    // PORT: the header is big-endian, straight off the disc, but decode it into locals rather than rewriting it.
    const u32 hdrNumLevels = port_be32(&header->numLevels);
    const eGXTextureFormat hdrFormat = (eGXTextureFormat)port_be32(&header->format);
    const u16 hdrWidth = port_be16(&header->width);
    const u16 hdrHeight = port_be16(&header->height);
    const u32 hdrNumEntries = port_be32(&header->numEntries);

    if (hdrFormat >= GXTex_Num || hdrWidth == 0 || hdrHeight == 0 || hdrNumLevels == 0 || hdrNumLevels > 16)
    {
        OSReport("[texture] invalid header: handle=%08lX fmt=%u %ux%u levels=%u file=%lu\n",
                 (unsigned long)texhandle, (unsigned)hdrFormat, (unsigned)hdrWidth,
                 (unsigned)hdrHeight, (unsigned)hdrNumLevels, fileSize);
        return NULL;
    }

    const u32 paletteBytes = hdrNumEntries <= 0x7FFFu ? hdrNumEntries * 2u : 0xFFFFFFFFu;
    const u32 available = (u32)(fileSize - sizeof(GXTextureHeader));
    const u32 encodedSize = GCTextureEncodedSize(hdrFormat, hdrWidth, hdrHeight, hdrNumLevels);
    if (paletteBytes == 0xFFFFFFFFu || encodedSize == 0 || encodedSize > available || paletteBytes > available - encodedSize)
    {
        const u32 legacySize = GCTextureSize(hdrFormat, hdrWidth, hdrHeight, hdrNumLevels, texhandle);
        OSReport("[texture] invalid payload: handle=%08lX fmt=%u %ux%u levels=%u "
                 "encoded=%u legacy=%u palette=%u available=%u file=%lu\n",
                 (unsigned long)texhandle, (unsigned)hdrFormat, (unsigned)hdrWidth,
                 (unsigned)hdrHeight, (unsigned)hdrNumLevels, encodedSize, legacySize,
                 paletteBytes, available, fileSize);
        return NULL;
    }

    PlatTexture* pTex;
    u16 width;
    u16 height;
    eGXTextureFormat format;
    unsigned long numLevels;
    unsigned long numEntries;
    const u32 textureSize = encodedSize;

    // Allocate PlatTexture (inline version of glx_CreatePlatTexture)
    pTex = (PlatTexture*)glResourceAlloc(sizeof(PlatTexture), GLM_Header);
    if (pTex != NULL)
    {
        pTex->m_Magic = 0x50544558;
        pTex->m_Width = 0;
        pTex->m_Height = 0;
        pTex->m_Levels = 0;
        pTex->m_MaxLevel = 0;
        pTex->m_Format = GXTex_Num;
        pTex->m_nPaletteEntries = 0;
        pTex->m_bMissingTexture = false;
        pTex->m_SwizzledData = NULL;
        pTex->m_LinearData = NULL;
        pTex->m_PaletteData = NULL;
        pTex->m_NativeDataSize = 0;
        pTex->m_OwnerTextureLevel=currentMarkerLevel;
        pTex->m_OwnerResourceLevel=glx_GetResourceMarkerLevel();
        memset(&pTex->m_TexObj, 0, sizeof(pTex->m_TexObj));
        memset(&pTex->m_TlutObj, 0, sizeof(pTex->m_TlutObj));
        memset(pTex->m_Bits, 0xFF, sizeof(pTex->m_Bits));
    }

    numLevels = hdrNumLevels;
    format = hdrFormat;
    height = hdrHeight;
    width = hdrWidth;

    if (pTex->m_LinearData != NULL)
    {
        nlFree(pTex->m_LinearData);
        pTex->m_LinearData = NULL;
    }

    pTex->m_Width = width;
    pTex->m_Height = height;
    pTex->m_Levels = (u8)numLevels;
    pTex->m_MaxLevel = (u8)numLevels;
    pTex->m_Format = format;

    pTex->m_SwizzledData = glResourceAlloc(textureSize, GLM_TextureData);
    pTex->m_LinearData = NULL;

    memcpy(pTex->m_Bits, header->numBits, sizeof(pTex->m_Bits));

    pTex->m_bMissingTexture = header->missingTexture ? true : false;

    numEntries = hdrNumEntries;
    if (numEntries != 0)
    {
        pTex->m_PaletteData = (u16*)glResourceAlloc(paletteBytes, GLM_TextureData);
        pTex->m_nPaletteEntries = (s16)numEntries;
        memcpy(pTex->m_PaletteData, (u8*)&header[1] + textureSize, paletteBytes);
    }

    memcpy(pTex->m_SwizzledData, (const u8*)header + 0x20, textureSize);

    pTex->Prepare();
    PortTextureCreated(&pTex->m_TexObj, pTex->m_nPaletteEntries > 0 ? &pTex->m_TlutObj : NULL); // PORT: texture packs

    return pTex;
#endif
}

/**
 * Offset/Address/Size: 0xBC4 | 0x801B7E80 | size: 0x10
 */
inline int BundleSortProc(const glTexBundleDict* a, const glTexBundleDict* b)
{
    return a->offset - b->offset;
}

/**
 * Offset/Address/Size: 0x9F4 | 0x801B7CB0 | size: 0x1D0
 */
bool glplatLoadTextureBundle(const char* filename)
{
    char fullFilename[256];
    unsigned long uBaseOffset;
    nlFile* pFile;
    glTexBundleDict* pDictionary;
    glTexBundleHeader* pHeader;
    unsigned char* pData;
    unsigned long uNumFiles;
    unsigned long uSize;

    glx_FreeMemory0();
    nlStrNCat<char>(fullFilename, "art/", filename, 0x100);
    PortTextureDumpFrom(fullFilename); // PORT: texture dumps

    pFile = nlOpen(fullFilename);
    if (pFile == NULL)
    {
        nlPrintf("file '%s' not found\n", filename);
        return false;
    }

    unsigned int fileAllocSize = 0;
    const unsigned int bundleFileSize = nlFileSize(pFile, &fileAllocSize);
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    const bool nativeLoaded=glxLoadNativeBundle(pFile,bundleFileSize);
    nlClose(pFile);glx_FreeMemory1(filename);
    if(!nativeLoaded)OSReport("[native-texture] invalid native bundle %s\n",filename);
    return nativeLoaded;
#else
    if (bundleFileSize < sizeof(glTexBundleHeader))
    {
        OSReport("[texture] bundle %s is truncated: %u bytes\n", filename, bundleFileSize);
        nlClose(pFile);
        return false;
    }

    pHeader = (glTexBundleHeader*)nlMalloc(sizeof(glTexBundleHeader), 0x20, 1);
    nlRead(pFile, pHeader, 0x20);
    // PORT: eight big-endian u32s, magic, count, and six pad words.
    port_be32_array(pHeader, 8);

    uNumFiles = pHeader->numTextures;
    if (uNumFiles > (bundleFileSize - sizeof(glTexBundleHeader)) / sizeof(glTexBundleDict))
    {
        OSReport("[texture] bundle %s has invalid dictionary count: %lu (file=%u)\n",
                 filename, uNumFiles, bundleFileSize);
        nlFree(pHeader);
        nlClose(pFile);
        return false;
    }
    uSize = uNumFiles * sizeof(glTexBundleDict);
    // Keep dictionarySize as its own copy: passing uSize directly instead costs
    const unsigned long dictionarySize = uSize;

    pDictionary = (glTexBundleDict*)nlMalloc((uBaseOffset = dictionarySize), 0x20, 1);
    nlRead(pFile, pDictionary, dictionarySize);
    // PORT: four big-endian u32s per entry, hash, offset, size, pad.
    port_be32_array(pDictionary, uNumFiles * 4);

    nlQSort<glTexBundleDict>(pDictionary, uNumFiles, BundleSortProc);

    uBaseOffset = uSize + 0x20;

    unsigned long largestEntry = 0;
    bool dictionaryValid = uBaseOffset <= bundleFileSize;
    for (unsigned long i = 0; dictionaryValid && i < uNumFiles; ++i)
    {
        const unsigned long offset = pDictionary[i].offset;
        const unsigned long entrySize = pDictionary[i].fileSize;
        const unsigned long dataBytes = bundleFileSize - uBaseOffset;
        if (offset > dataBytes || entrySize > dataBytes - offset || entrySize < sizeof(GXTextureHeader))
        {
            OSReport("[texture] bundle %s invalid entry %lu: offset=%lu size=%lu data=%lu\n",
                     filename, i, offset, entrySize, dataBytes);
            dictionaryValid = false;
            break;
        }
        if (entrySize > largestEntry)
            largestEntry = entrySize;
    }

    if (!dictionaryValid || largestEntry == 0)
    {
        nlFree(pDictionary);
        nlFree(pHeader);
        nlClose(pFile);
        return false;
    }

    // The retail code used a fixed 0x40800 scratch buffer because every retail
    // entry fit in it. Allocate from the validated dictionary size on the port:
    // a malformed/foreign bundle can no longer overflow the scratch buffer.
    pData = (unsigned char*)nlMalloc(largestEntry, 0x20, 1);
    if (pData == NULL)
    {
        OSReport("[texture] bundle %s scratch allocation failed: %lu bytes\n",
                 filename, largestEntry);
        nlFree(pDictionary);
        nlFree(pHeader);
        nlClose(pFile);
        return false;
    }

    for (unsigned long i = 0; i < uNumFiles; i++)
    {
        nlSeek(pFile, uBaseOffset + pDictionary[i].offset, 0);
        nlRead(pFile, pData, pDictionary[i].fileSize);
        if (glxTextureLoad_cb == NULL)
        {
            glplatTextureAdd(pDictionary[i].hash, pData, pDictionary[i].fileSize);
        }
        else
        {
            unsigned long newHash = glxTextureLoad_cb(pDictionary[i].hash);
            if (newHash != -1)
            {
                if (glTextureLoad(newHash) != 0)
                {
                    glplatTextureReplace(newHash, pData, pDictionary[i].fileSize);
                }
                else
                {
                    glplatTextureAdd(newHash, pData, pDictionary[i].fileSize);
                }
            }
        }
    }

    PortTextureDumpFrom(NULL); // PORT: texture dumps
    nlFree(pData);
    nlFree(pDictionary);
    nlFree(pHeader);
    nlClose(pFile);
#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
    GXInvalidateTexAll();
#endif
    glx_FreeMemory1(filename);

    return true;
#endif
}

#pragma dont_inline on
/**
 * Offset/Address/Size: 0x93C | 0x801B7BF8 | size: 0xB8
 */
static bool glxParseTextureBundle(const char* filedata, unsigned long fileSize,
                                glxTextureLoadCallback_t remap=glxTextureLoad_cb)
{
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    const auto bytes=std::span(reinterpret_cast<const uint8_t*>(filedata),size_t(fileSize));
    if(!filedata)return false;
    return port::assets::admit_texture_bundle(bytes,
        [remap](uint32_t ident){return remap?uint32_t(remap(ident)):ident;},
        [](uint32_t handle,std::span<const uint8_t> image){
            if(glTextureLoad(handle))glplatTextureReplace(handle,image.data(),image.size());
            else glplatTextureAdd(handle,image.data(),image.size());
            return glx_GetTex(handle,false,false)!=NULL;
        });
#else

    if (filedata == NULL || fileSize < sizeof(glTexBundleHeader))
    {
        OSReport("[texture] async bundle is truncated: %lu bytes\n", fileSize);
        return false;
    }

    const u32 numTextures = port_be32(filedata + 4);
    const unsigned long headerSize = sizeof(glTexBundleHeader);
    if (numTextures > (fileSize - headerSize) / sizeof(glTexBundleDict))
    {
        OSReport("[texture] async bundle has invalid dictionary count: %u (file=%lu)\n",
                 numTextures, fileSize);
        return false;
    }

    const unsigned long dictionarySize =
        (unsigned long)numTextures * sizeof(glTexBundleDict);
    const unsigned long dataOffset = headerSize + dictionarySize;
    const unsigned long dataBytes = fileSize - dataOffset;
    const u8* dict = (const u8*)filedata + headerSize;
    const u8* textureData = (const u8*)filedata + dataOffset;

    for (u32 i = 0; i < numTextures; i++)
    {
        const u8* entry = dict + i * sizeof(glTexBundleDict);
        const u32 hash = port_be32(entry + 0x00);
        const u32 offset = port_be32(entry + 0x04);
        const u32 entrySize = port_be32(entry + 0x08);
        if (offset > dataBytes || entrySize > dataBytes - offset ||
            entrySize < sizeof(GXTextureHeader))
        {
            OSReport("[texture] async bundle invalid entry %u: offset=%u size=%u data=%lu\n",
                     i, offset, entrySize, dataBytes);
            return false;
        }

        const GXTextureHeader* currentTextureHeader =
            (const GXTextureHeader*)(textureData + offset);

        if (glxTextureLoad_cb == NULL)
        {
            glplatTextureAdd(hash, currentTextureHeader, entrySize);
        }
        else
        {
            unsigned long newHash = glxTextureLoad_cb(hash);
            if (newHash != -1 && glTextureLoad(newHash) != 0)
            {
                glplatTextureReplace(newHash, currentTextureHeader, entrySize);
            }
        }
    }

#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
    GXInvalidateTexAll();
#endif
    return true;
#endif
}
#pragma dont_inline off

/**
 * Offset/Address/Size: 0x894 | 0x801B7B50 | size: 0xA8
 */
bool glplatBeginLoadTextureBundle(const char* filename, void (*callback)(void*, unsigned long, void*), void* param)
{
    char fullname[256];
    nlStrNCat<char>(fullname, "art/", filename, 0x100);
    PortTextureDumpExpect(param, fullname); // PORT: texture dumps; the file arrives in `param` when it is set
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    if(param!=NULL) {
        struct Completion {LoadAsyncCallback callback;glxTextureLoadCallback_t remap;};
        // Remapping belongs to this request. The caller restores the global
        // hook before the asynchronous read completes.
        auto* completion=new Completion{callback,glxTextureLoad_cb};
        const auto completed=[](void* bytes,unsigned long size,void* context) {
            auto* c=static_cast<Completion*>(context);
            const bool loaded=glxParseTextureBundle(static_cast<const char*>(bytes),size,c->remap);
            nlFree(bytes);const auto original=c->callback;delete c;
            if(!loaded){OSReport("[native-texture] async native bundle failed\n");nlBreak();return;}
            if(original)original(NULL,size,NULL);
        };
        if(!nlLoadEntireFileAsync(fullname,completed,completion,0x20,AllocateEnd)){delete completion;return false;}
        return true;
    }
#endif
    if (param == NULL)
    {
        if (nlLoadEntireFileAsync(fullname, callback, param, 0x20, AllocateEnd) == 0)
        {
            return false;
        }
    }
    else if (nlLoadEntireFileAsync(fullname, callback, param, 0x20, (eAllocType)0x17) == 0)
    {
        return false;
    }
    return true;
}

/**
 * Offset/Address/Size: 0x874 | 0x801B7B30 | size: 0x20
 */
bool glplatEndLoadTextureBundle(void* data, unsigned long size)
{
    // PORT: texture dumps
    PortTextureDumpFromBuffer(data);
    const bool parsed = glxParseTextureBundle((const char*)data, size);
    PortTextureDumpFrom(NULL);
    return parsed;
}

/**
 * Offset/Address/Size: 0x810 | 0x801B7ACC | size: 0x64
 */
bool glplatTextureLoad(uintptr_t texture)
{
    PlatTexture* pTex = glx_GetTex(texture, 0, 0);
    if (pTex == NULL)
    {
        memset(&texobj, 0, sizeof(texobj));
        return false;
    }
    memcpy(&texobj, pTex, sizeof(texobj));
    return true;
}

/**
 * Offset/Address/Size: 0x800 | 0x801B7ABC | size: 0x10
 */
u32 glplatTextureGetWidth()
{
    return texobj.m_Width;
}

/**
 * Offset/Address/Size: 0x7F0 | 0x801B7AAC | size: 0x10
 */
u32 glplatTextureGetHeight()
{
    return texobj.m_Height;
}

/**
 * Offset/Address/Size: 0x6EC | 0x801B79A8 | size: 0x104
 */
int glplatTextureGetNumBits(int component)
{
    if (texobj.m_Bits[component] == 0xFF)
    {
        u32 format = texobj.m_Format;
        u8 bits[4] = { 0, 0, 0, 0 };

        switch (format)
        {
        case GXTex_RGB565:
            bits[0] = 5;
            bits[1] = 6;
            bits[2] = 5;
            bits[3] = 0;
            break;

        case GXTex_RGB5A3:
            bits[0] = 5;
            bits[1] = 5;
            bits[2] = 5;
            bits[3] = 3;
            break;

        case GXTex_CMPR:
            break;

        case GXTex_RGBA8:
            bits[0] = 8;
            bits[1] = 8;
            bits[2] = 8;
            bits[3] = 8;
            break;

        case GXTex_I8:
            bits[0] = 8;
            bits[1] = 0;
            bits[2] = 0;
            bits[3] = 0;
            break;

        case GXTex_I4:
            bits[0] = 4;
            bits[1] = 0;
            bits[2] = 0;
            bits[3] = 0;
            break;

        case GXTex_A8:
            break;

        case GXTex_IA8:
            bits[0] = 8;
            bits[1] = 0;
            bits[2] = 0;
            bits[3] = 8;
            break;
        }

        return bits[component];
    }

    return texobj.m_Bits[component];
}

/**
 * Offset/Address/Size: 0x644 | 0x801B7900 | size: 0xA8
 */
PlatTexture* glx_CreatePlatTexture()
{
    PlatTexture* pTex = (PlatTexture*)glResourceAlloc(sizeof(PlatTexture), GLM_Header);
    if (pTex != NULL)
    {
        pTex->m_Magic = 0x50544558;
        pTex->m_Width = 0;
        pTex->m_Height = 0;
        pTex->m_Levels = 0;
        pTex->m_MaxLevel = 0;
        pTex->m_Format = GXTex_Num;
        pTex->m_nPaletteEntries = 0;
        pTex->m_bMissingTexture = false;
        pTex->m_SwizzledData = NULL;
        pTex->m_LinearData = NULL;
        pTex->m_PaletteData = NULL;
        pTex->m_NativeDataSize = 0;
        pTex->m_OwnerTextureLevel=currentMarkerLevel;
        pTex->m_OwnerResourceLevel=glx_GetResourceMarkerLevel();
        memset(&pTex->m_TexObj, 0, sizeof(pTex->m_TexObj));
        memset(&pTex->m_TlutObj, 0, sizeof(pTex->m_TlutObj));
        memset(pTex->m_Bits, 0xFF, sizeof(pTex->m_Bits));
    }
    return pTex;
}

/**
 * Offset/Address/Size: 0x5E0 | 0x801B789C | size: 0x64
 */
PlatTexture::~PlatTexture()
{
    if (m_LinearData != NULL)
    {
        nlFree(m_LinearData);
        m_LinearData = NULL;
    }
}

/**
 * Offset/Address/Size: 0x5BC | 0x801B7878 | size: 0x24
 */
void PlatTexture::CreateWithMemory(int width, int height, eGXTextureFormat format, int numLevels, const void* pTextureData)
{
    m_NativeDataSize = 0;
    m_Width = width;
    m_Height = height;
    m_Levels = (u8)numLevels;
    m_MaxLevel = (u8)numLevels;
    m_Format = format;
    m_SwizzledData = (void*)pTextureData;
    m_LinearData = NULL;
}

/**
 * Offset/Address/Size: 0x4E4 | 0x801B77A0 | size: 0xD8
 */
void PlatTexture::Create(int width, int height, eGXTextureFormat format, int numLevels, bool bLinearData, bool bNewResourceMemory)
{
    if (m_LinearData != NULL)
    {
        nlFree(m_LinearData);
        m_LinearData = NULL;
    }

    m_NativeDataSize = 0;
    m_Width = width;
    m_Height = height;
    m_Levels = (u8)numLevels;
    m_MaxLevel = (u8)numLevels;
    m_Format = format;

    u32 textureSize = GCTextureSize(format, width, height, numLevels, -1);

    if (bNewResourceMemory)
    {
        m_SwizzledData = nlMalloc(textureSize, 0x20, false);
    }
    else
    {
        m_SwizzledData = glResourceAlloc(textureSize, GLM_TextureData);
    }

    if (bLinearData)
    {
        m_LinearData = nlMalloc(textureSize, 0x20, false);
    }
    else
    {
        m_LinearData = NULL;
    }
}

/**
 * Offset/Address/Size: 0x47C | 0x801B7738 | size: 0x68
 */
void PlatTexture::Swizzle(bool bDeleteLinear)
{
    GCSwizzle(m_SwizzledData, m_LinearData, m_Width, m_Height, m_Format, false);

    if (bDeleteLinear)
    {
        nlFree(m_LinearData);
        m_LinearData = NULL;
    }
}

static inline GXTexFmt* glx_GetGXFormatTable()
{
    static GXTexFmt gx_format[9] = {
        GX_TF_RGB565,
        GX_TF_RGB5A3,
        GX_TF_CMPR,
        GX_TF_RGBA8,
        GX_TF_I8,
        GX_TF_I4,
        GX_TF_I8,
        GX_TF_IA8,
        (GXTexFmt)0x9
    };

    return gx_format;
}

/**
 * Offset/Address/Size: 0x2C8 | 0x801B7584 | size: 0x1B4
 */
void PlatTexture::Prepare()
{
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    DCStoreRange(m_SwizzledData,m_NativeDataSize?m_NativeDataSize:GCTextureSize(m_Format,m_Width,m_Height,m_Levels,-1));
    if(m_PaletteData&&m_nPaletteEntries>0)DCStoreRange(m_PaletteData,m_nPaletteEntries*2);
#else

    DCStoreRange(m_SwizzledData, GCTextureSize(m_Format, m_Width, m_Height, m_Levels, -1));
    if (m_nPaletteEntries > 0)
    {
        DCStoreRange(m_PaletteData, m_nPaletteEntries * 2);
        GXInitTlutObj(&m_TlutObj, m_PaletteData, GX_TL_RGB5A3, m_nPaletteEntries);
    }
    if (m_Format == GXTex_CI8)
    {
        GXInitTexObjCI(&m_TexObj, m_SwizzledData, m_Width, m_Height, (GXCITexFmt)glx_GetGXFormatTable()[m_Format], GX_CLAMP, GX_CLAMP, m_Levels > 1 ? 1 : 0, 0);
        GXInitTexObjLOD(&m_TexObj, (m_Levels == 1) ? GX_LINEAR : GX_LIN_MIP_NEAR, GX_LINEAR, 0.0f, (float)(m_MaxLevel - 1), 0.0f, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
        return;
    }
    GXInitTexObj(&m_TexObj, m_SwizzledData, m_Width, m_Height, glx_GetGXFormatTable()[m_Format], GX_CLAMP, GX_CLAMP, m_Levels > 1 ? 1 : 0);
    GXInitTexObjLOD(&m_TexObj, (m_Levels == 1) ? GX_LINEAR : GX_LIN_MIP_LIN, GX_LINEAR, 0.0f, (float)(m_MaxLevel - 1), 0.0f, GX_DISABLE, GX_DISABLE, GX_ANISO_1);

#endif
}

#pragma dont_inline on
/**
 * Offset/Address/Size: 0x230 | 0x801B74EC | size: 0x98
 */
void glplatTextureAdd(uintptr_t handle, const void* textureData, unsigned long size)
{
    unsigned long handleCopy;
    PlatTexture* pTex = glx_MakeTexture((GXTextureHeader*)textureData, handle, size);
    nlAVLTree<unsigned long, PlatTexture*, DefaultKeyCompare<unsigned long> >* textureTree;
    handleCopy = handle;

    if ((pTex != NULL) && (handle != -1))
    {
        textureTree = textures[currentMarkerLevel];

        textureTree->Add(handleCopy, pTex);
    }
}
#pragma dont_inline off

#pragma dont_inline on
/**
 * Offset/Address/Size: 0x0 | 0x801B72BC | size: 0x230
 */
void glplatTextureReplace(uintptr_t handle, const void* textureData, unsigned long size)
{
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    if(!glxApplyNativeTexture(glx_GetTex(handle,false,false),textureData,size))
        OSReport("[native-texture] invalid native replacement: %08lx\n",(unsigned long)handle);
#else

    // PORT: the header is NOT swapped in place here.
    const GXTextureHeader* pHeader = (GXTextureHeader*)textureData;
    PlatTexture* pTex = glx_GetTex(handle, false, false);
    if (pTex == NULL)
    {
        // PORT: the handle names a texture that is not loaded.
        OSReport("glplatTextureReplace: handle %lu is not a loaded texture\n",
                 (unsigned long)handle);
        return;
    }

    // PORT: the inventory can miss, and this never checked.
    if (pTex == NULL || pTex->m_Magic != 0x50544558
        || pTex->m_Width == 0 || pTex->m_Height == 0
        || pTex->m_Format >= GXTex_Num)
        return;

    // PORT: a slot with no pixel storage is not ours to fill.
    if (pTex->m_SwizzledData == NULL)
        return;

    // PORT: big-endian in the file; the palette copy below is sized from it.
    const u32 numEntries = port_be32(&pHeader->numEntries);

    {
        // PORT: clamp to what the caller actually handed us.
        unsigned long wanted = GCTextureSize(pTex->m_Format, pTex->m_Width,
                                             pTex->m_Height, pTex->m_Levels, -1);
        unsigned long avail = size > sizeof(GXTextureHeader)
                                  ? size - sizeof(GXTextureHeader) : 0;
        if (wanted > avail)
            wanted = avail;
        memcpy(pTex->m_SwizzledData,
               ((u8*)textureData) + sizeof(GXTextureHeader), wanted);
    }

    if (numEntries != 0 && pTex->m_PaletteData != NULL)
    {
        const u8* src = (const u8*)textureData;
        src += GCTextureSize(pTex->m_Format, pTex->m_Width, pTex->m_Height, pTex->m_Levels, -1);
        src += sizeof(GXTextureHeader);

        // PORT: clamp to the palette that was actually allocated.
        if (pTex->m_nPaletteEntries <= 0)
        {
            static int nPalSkip = 0;
            if (nPalSkip++ < 4)
                OSReport("[texpal] palette replace skipped: destination "
                         "capacity unknown (m_nPaletteEntries=%d)\n",
                         (int)pTex->m_nPaletteEntries);
        }
        else
        {
            u32 palEntries = numEntries;
            if (palEntries > (u32)pTex->m_nPaletteEntries)
            {
                static int nPalWarn = 0;
                if (nPalWarn++ < 4)
                {
                    OSReport("[texpal] replacement claims %u palette entries, "
                             "allocation holds %d; clamping\n",
                             (unsigned)palEntries, (int)pTex->m_nPaletteEntries);
                }
                palEntries = (u32)pTex->m_nPaletteEntries;
            }
            memcpy(pTex->m_PaletteData, src, palEntries * 2);
        }
    }
    DCStoreRange(pTex->m_SwizzledData, GCTextureSize(pTex->m_Format, pTex->m_Width, pTex->m_Height, pTex->m_Levels, -1));

#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    pTex->Prepare();
#else
    if (pTex->m_nPaletteEntries > 0)
    {
        DCStoreRange(pTex->m_PaletteData, pTex->m_nPaletteEntries * 2);
        GXInitTlutObj(&pTex->m_TlutObj, pTex->m_PaletteData, GX_TL_RGB5A3, pTex->m_nPaletteEntries);
    }

    if (pTex->m_Format == GXTex_CI8)
    {
        GXInitTexObjCI(&pTex->m_TexObj, pTex->m_SwizzledData, pTex->m_Width, pTex->m_Height, (GXCITexFmt)glx_GetGXFormatTable()[pTex->m_Format], GX_CLAMP, GX_CLAMP, pTex->m_Levels > 1 ? 1 : 0, 0);
        GXInitTexObjLOD(&pTex->m_TexObj, (pTex->m_Levels == 1) ? GX_LINEAR : GX_LIN_MIP_NEAR, GX_LINEAR, 0.0f, (float)(pTex->m_MaxLevel - 1), 0.0f, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
        PortTextureCreated(&pTex->m_TexObj, pTex->m_nPaletteEntries > 0 ? &pTex->m_TlutObj : NULL); // PORT: texture packs
        return;
    }

    GXInitTexObj(&pTex->m_TexObj, pTex->m_SwizzledData, pTex->m_Width, pTex->m_Height, glx_GetGXFormatTable()[pTex->m_Format], GX_CLAMP, GX_CLAMP, pTex->m_Levels > 1 ? 1 : 0);
    GXInitTexObjLOD(&pTex->m_TexObj, (pTex->m_Levels == 1) ? GX_LINEAR : GX_LIN_MIP_LIN, GX_LINEAR, 0.0f, (float)(pTex->m_MaxLevel - 1), 0.0f, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
    PortTextureCreated(&pTex->m_TexObj, NULL); // PORT: texture packs
#endif
#endif
}
#pragma dont_inline off
