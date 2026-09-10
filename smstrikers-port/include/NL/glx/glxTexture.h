#ifndef _GLXTEXTURE_H_
#define _GLXTEXTURE_H_

#include "types.h"
#include "dolphin/gx/GXEnum.h"
#include "dolphin/gx/GXStruct.h"

#include "NL/nlMemory.h"

typedef unsigned long (*glxTextureLoadCallback_t)(unsigned long);

enum eGXTextureFormat
{
    GXTex_RGB565 = 0,
    GXTex_RGB5A3 = 1,
    GXTex_CMPR = 2,
    GXTex_RGBA8 = 3,
    GXTex_I8 = 4,
    GXTex_I4 = 5,
    GXTex_A8 = 6,
    GXTex_IA8 = 7,
    GXTex_CI8 = 8,
    GXTex_Num = 9,
};

enum eGLTextureType
{
    GLTT_Diffuse = 0,
    GLTT_Detail = 1,
    GLTT_Shadow = 2,
    GLTT_SelfIllum = 3,
    GLTT_Gloss = 4,
    GLTT_BumpLocal = 5,
    GLTT_Num = 6,
};

enum eGLTextureState
{
    GLTS_DiffuseWrap = 0,
    GLTS_DetailWrap = 1,
    GLTS_ShadowWrap = 2,
    GLTS_SelfIllumWrap = 3,
    GLTS_GlossWrap = 4,
    GLTS_BumpLocalWrap = 5,
    GLTS_DiffuseFilter = 6,
    GLTS_DetailFilter = 7,
    GLTS_ShadowFilter = 8,
    GLTS_SelfIllumFilter = 9,
    GLTS_GlossFilter = 10,
    GLTS_BumpLocalFilter = 11,
    GLTS_DiffuseLevel = 12,
    GLTS_DetailLevel = 13,
    GLTS_ShadowLevel = 14,
    GLTS_SelfIllumLevel = 15,
    GLTS_GlossLevel = 16,
    GLTS_BumpLocalLevel = 17,
    GLTS_Num = 18,
};

enum eGLTextureMode
{
    GLTM_WrapWrap = 0,
    GLTM_WrapClamp = 1,
    GLTM_ClampWrap = 2,
    GLTM_ClampClamp = 3,
    GLTM_Num = 4,
};

enum eGLTextureFilter
{
    GLTF_Linear = 0,
    GLTF_Point = 1,
    GLTF_Num = 2,
};

enum eGLTextureFormatType
{
    eGLTextureFormatType_0,
};

struct BundleEntry
{
    /* 0x0 */ u32 hash;
    /* 0x4 */ u32 offset;
    /* 0x8 */ u32 fileSize;
    /* 0xC */ u32 pad;
}; // total size: 0x10

struct glTexBundleDict : public BundleEntry
{
}; // total size: 0x10

struct BundleHeader
{
    /* 0x0 */ u32 magic;
    /* 0x4 */ u32 numTextures;
    /* 0x8 */ u32 pad1;
    /* 0xC */ u32 pad2;
}; // total size: 0x10

struct glTexBundleHeader : public BundleHeader
{
    /* 0x10 */ u32 pad[4]; // size 0x10
}; // total size: 0x20

struct GXTextureHeader
{
    /* 0x00 */ u32 numLevels;
    /* 0x04 */ eGXTextureFormat format;
    /* 0x08 */ unsigned char numBits[4];
    /* 0x0C */ unsigned char missingTexture;
    /* 0x0E */ unsigned short width;
    /* 0x10 */ unsigned short height;
    /* 0x14 */ u32 numEntries;
    /* 0x18 */ u32 pad[2];
}; // total size: 0x20

class PlatTexture
{
public:
    PlatTexture()
        : m_Magic(0x50544558)
        , m_Width(0)
        , m_Height(0)
        , m_Levels(0)
        , m_MaxLevel(0)
        , m_Format(GXTex_Num)
        , m_nPaletteEntries(0)
        , m_bMissingTexture(false)
        , m_SwizzledData(NULL)
        , m_LinearData(NULL)
        , m_PaletteData(NULL)
    {
        memset(&m_TexObj, 0, sizeof(m_TexObj));
        memset(&m_TlutObj, 0, sizeof(m_TlutObj));
        memset(m_Bits, 0xFF, sizeof(m_Bits));
    }
    ~PlatTexture();

    void Prepare();
    void Swizzle(bool bDeleteLinear);
    void Create(int width, int height, eGXTextureFormat format, int numLevels, bool bLinearData, bool bNewResourceMemory);
    void CreateWithMemory(int width, int height, eGXTextureFormat format, int numLevels, const void* pTextureData);

    /* 0x00 */ int m_Magic;
    /* 0x04 */ u16 m_Width;
    /* 0x06 */ u16 m_Height;
    /* 0x08 */ u8 m_Levels;
    /* 0x09 */ u8 m_MaxLevel;
    /* 0x0C */ eGXTextureFormat m_Format;
    /* 0x10 */ s16 m_nPaletteEntries;
    /* 0x12 */ bool m_bMissingTexture;
    /* 0x14 */ void* m_SwizzledData;
    /* 0x18 */ void* m_LinearData;
    /* 0x1C */ u16* m_PaletteData;
    /* 0x20 */ u8 m_Bits[4];
    /* 0x24 */ _GXTexObj m_TexObj;   // size 0x20
    /* 0x44 */ _GXTlutObj m_TlutObj; // size 0xC
}; // total size: 0x50

void glplatTextureReplace(uintptr_t handle, const void* textureData, unsigned long size);
void glplatTextureAdd(uintptr_t handle, const void* textureData, unsigned long size);
PlatTexture* glx_CreatePlatTexture();
int glplatTextureGetNumBits(int component);
u32 glplatTextureGetHeight();
u32 glplatTextureGetWidth();
bool glplatTextureLoad(uintptr_t texture);
bool glplatEndLoadTextureBundle(void* data, unsigned long size);
bool glplatBeginLoadTextureBundle(const char* filename, void (*callback)(void*, unsigned long, void*), void* param);
bool glplatLoadTextureBundle(const char* filename);
PlatTexture* glx_MakeTexture(GXTextureHeader* header, uintptr_t texhandle);
bool glx_AddTex(uintptr_t handle, PlatTexture* pTex);
PlatTexture* glx_GetTex(uintptr_t handle, bool bMissingFatal, bool bAllowGrids);
bool glx_SetGridMode(bool bGrid);
void glxInitTex();
void glx_BackupTexMarkerLevel(int level);
void glx_AdvanceTexMarkerLevel();
int glx_GetTexMarkerLevel();
glxTextureLoadCallback_t glx_SetLoadCallback(glxTextureLoadCallback_t callback);

class TexDestructor
{
public:
    void CallDestructor(const unsigned long& index, PlatTexture** tex);
};

#endif // _GLXTEXTURE_H_
