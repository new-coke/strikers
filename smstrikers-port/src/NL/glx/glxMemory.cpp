#include "dolphin/types.h"
#include "NL/nlMemory.h"
#include "NL/nlConfig.h"
#include "NL/glx/glxMemory.h"
#include "port/overlay.h"   // PORT: PortGfxArenaStats
#include <string.h>
#include <stdint.h>

// PORT: multiplier on the console's graphics memory budgets.
#define PORT_GFX_ARENA_SCALE 8u
#include "NL/glx/glxTexture.h"
#include "NL/gc/gcSwizzler.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glConstant.h"
#include "NL/nlDebug.h"
#include "dolphin/os.h"
#include "dolphin/gx/GXGeometry.h"
#include "dolphin/gx/GXTexture.h"
#include "Game/Sys/debug.h"
#include "../../Game/Sys/tweak.h"
#include "Game/GL/GLInventory.h"

static u8 glx_MemoryDump;
static u32 ResourceMemSize = MB(12) - KB(12);
static u32 FrameMemSizeReal = KB(896);
static u32 FrameMemSizeVirt = KB(640);
static uintptr_t p_phys;   // PORT: an address
static u32 n_phys;
static int i_frame;
static u32 glx_mem0;
static int g_uResourceMarker;

static uintptr_t p_frame[2][2];   // PORT: addresses, not offsets
static u32 n_frame[2][2];

Tweakable glx_memory_tweaks[] = {
    { TWEAK_Title, NULL, "Engine", 1.0f, 0.0f, 0.0f, NULL },
    { TWEAK_Title, NULL, "GLX Memory", 2.0f, 0.0f, 0.0f, NULL },
    { TWEAK_Bool, &glx_MemoryDump, "glx_MemoryDump", 0.0f, 0.0f, 0.0f, NULL },
};

static u32 FrameMemSizes[2] = { FrameMemSizeReal, FrameMemSizeVirt };

static GLXMemoryInfo g_uResourceAlloc[16];

static char* szMemoryNames[6] = {
    "header",
    "matrix",
    "index",
    "vert",
    "tex",
    "target"
};

/**
 * Offset/Address/Size: 0x90C | 0x801B7234 | size: 0x24
 */
GLXMemoryInfo::GLXMemoryInfo()
{
    for (int i = 0; i < GLM_Num; i++)
    {
        m_uBytes[i] = 0;
    }
    m_uTexBundle = 0;
}

/**
 * Offset/Address/Size: 0x8FC | 0x801B7224 | size: 0x10
 */
u32 glx_GetFreeMemory()
{
    return ResourceMemSize - n_phys;
}

/**
 * Offset/Address/Size: 0x8E8 | 0x801B7210 | size: 0x14
 */
void glx_FreeMemory0()
{
    glx_mem0 = ResourceMemSize - n_phys;
}

/**
 * Offset/Address/Size: 0x8E4 | 0x801B720C | size: 0x4
 */
void glx_FreeMemory1(const char* filename)
{
}

static u32 GetFromConfig(const char* szConfigString, u32 uDefault)
{
    if (Config::Global().Exists(szConfigString))
    {
        f32 fValue = GetConfigFloat(Config::Global(), szConfigString, 0.0f);
        return (u32)(1024.0f * (1024.0f * fValue));
    }
    return uDefault;
}

/**
 * Offset/Address/Size: 0x4F4 | 0x801B6E1C | size: 0x3F0
 */
bool glxInitMemory()
{
    bool bDeveloper = GetConfigBool(Config::Global(), "e3_build", false);
    const char* szResourceKey = bDeveloper ? "e3 resource total memory" : "resource total memory";

    ResourceMemSize = GetFromConfig(szResourceKey, ResourceMemSize);
    // PORT: see the note on, host structures are wider.
    ResourceMemSize *= PORT_GFX_ARENA_SCALE;

    uintptr_t pMem = (uintptr_t)nlMalloc(ResourceMemSize, 32, false);
    if (pMem == 0)
        return false;
    p_phys = pMem;

    FrameMemSizeReal = GetFromConfig("frame vertex memory", FrameMemSizeReal);
    FrameMemSizeReal *= PORT_GFX_ARENA_SCALE;

    pMem = (uintptr_t)nlMalloc(FrameMemSizeReal * 2, 32, false);
    if (pMem == 0)
        return false;
    p_frame[0][0] = pMem;
    p_frame[1][0] = pMem + FrameMemSizeReal;

    FrameMemSizeVirt = GetFromConfig("frame header memory", FrameMemSizeVirt);
    FrameMemSizeVirt *= PORT_GFX_ARENA_SCALE;

    uintptr_t pVirt = (uintptr_t)nlVirtualAlloc(FrameMemSizeVirt * 2, false);
    if (pVirt == 0)
        return false;

    p_frame[0][1] = pVirt;
    p_frame[1][1] = pVirt + FrameMemSizeVirt;
    i_frame = 0;
    n_phys = 0;
    n_frame[1][0] = 0;
    n_frame[0][0] = 0;
    n_frame[1][1] = 0;
    n_frame[0][1] = 0;
    FrameMemSizes[0] = FrameMemSizeReal;
    FrameMemSizes[1] = FrameMemSizeVirt;

    return true;
}

inline void GLXMemoryInfo::Clear()
{
    for (int i = 0; i < GLM_Num; i++)
    {
        m_uBytes[i] = 0;
    }
    m_uTexBundle = 0;
}

inline u32 GLXMemoryInfo::GetTotal() const
{
    u32 total = 0;
    for (u32 mem = 0; mem < GLM_Num; mem++)
    {
        total += m_uBytes[mem];
    }
    return total;
}

inline u32 GLXMemoryInfo::GetTexDiff() const
{
    return m_uBytes[GLM_TextureData] - m_uTexBundle;
}

inline void GLXMemoryInfo::Print(unsigned long level) const
{
    tDebugPrintManager::Print(DC_GLPLAT, "%u : [ ", level);
    for (u32 mem = 0; mem < GLM_Num; mem++)
    {
        tDebugPrintManager::Print(DC_GLPLAT, "%s %uKB ", szMemoryNames[mem], m_uBytes[mem] >> 10);
    }
    tDebugPrintManager::Print(DC_GLPLAT, "]");
    tDebugPrintManager::Print(DC_GLPLAT, ": ");
    tDebugPrintManager::Print(DC_GLPLAT, "%uKB .. %uKB\n", GetTotal() >> 10, GetTexDiff() >> 10);
}

static bool glplatIsMemoryReadable(eGLMemory memType)
{
    return true;
}

static bool glplatIsMemoryWriteable(eGLMemory memType)
{
    return true;
}

static void glx_TrackTextureBundleAlloc(const GXTextureHeader* pHeader)
{
    int size = GCTextureSize(pHeader->format, pHeader->width, pHeader->height, pHeader->numLevels, -1);
    g_uResourceAlloc[g_uResourceMarker].m_uTexBundle += size;
}

// PORT: The console printed this to the on-screen debug console; nothing draws that here.
static void port_ReportResourceArena(const char* why)
{
    OSReport("[gfxmem] %s: arena %u/%u KB used, marker level %d\n",
             why, n_phys >> 10, ResourceMemSize >> 10, g_uResourceMarker);
    for (s32 level = 0; level <= g_uResourceMarker; level++)
    {
        u32 total = g_uResourceAlloc[level].GetTotal();
        if (total == 0)
        {
            continue;
        }
        OSReport("[gfxmem]   level %d: %u KB total", level, total >> 10);
        for (u32 mem = 0; mem < GLM_Num; mem++)
        {
            if (g_uResourceAlloc[level].m_uBytes[mem] != 0)
            {
                OSReport("  %s %u KB", szMemoryNames[mem],
                         g_uResourceAlloc[level].m_uBytes[mem] >> 10);
            }
        }
        OSReport("\n");
    }
}

// PORT: the same table, for the debug menu.
extern "C" int PortGfxArenaStats(PortGfxArena* out)
{
    if (out == NULL)
        return 0;
    memset(out, 0, sizeof *out);
    out->used = n_phys;
    out->size = ResourceMemSize;
    out->markerLevel = (int)g_uResourceMarker;
    out->typeCount = (GLM_Num < 8) ? (int)GLM_Num : 8;
    for (int mem = 0; mem < out->typeCount; mem++)
        strncpy(out->typeNames[mem], szMemoryNames[mem], sizeof out->typeNames[mem] - 1);
    for (s32 level = 0; level <= g_uResourceMarker && level < 8; level++)
        for (int mem = 0; mem < out->typeCount; mem++)
            out->bytes[level][mem] = g_uResourceAlloc[level].m_uBytes[mem];
    return 1;
}

static void ResourceMarkerPrint(unsigned long level)
{
    g_uResourceAlloc[level].Print(level);
}

static void ResourceAllocTotal()
{
    u32 uTotal = 0;
    u32 uTexDiff = uTotal;
    u32 level;
    for (level = 0; level <= (u32)g_uResourceMarker; level++)
    {
        u32 levelTotal = 0;
        for (u32 mem = 0; mem < GLM_Num; mem++)
        {
            levelTotal += g_uResourceAlloc[level].m_uBytes[mem];
        }
        uTotal += levelTotal;
        uTexDiff += g_uResourceAlloc[level].m_uBytes[GLM_TextureData] - g_uResourceAlloc[level].m_uTexBundle;
    }

    f32 fConst = 1.0f / 1024.0f;
    f32 fMB = (f32)uTotal * fConst;
    tDebugPrintManager::Print(DC_GLPLAT, "total : (%0.3fMB) %uKB .. %uKB dTex\n", fMB * fConst, uTotal >> 10, uTexDiff >> 10);
}

static void ResourceAllocMark()
{
    s32 level = g_uResourceMarker + 1;
    g_uResourceMarker = level;
    g_uResourceAlloc[level].Clear();
    ResourceAllocTotal();
}

static void ResourceAllocRelease(int level)
{
    while ((s32)g_uResourceMarker != level)
    {
        g_uResourceMarker--;
    }
    ResourceAllocTotal();
}

/**
 * Offset/Address/Size: 0x440 | 0x801B6D68 | size: 0xB4
 */
void* glplatResourceAlloc(unsigned long size, eGLMemory memType)
{
    uintptr_t base = p_phys;
    // PORT: mask in pointer width, ~0x1FU would clear the top half.
    uintptr_t aligned = (base + n_phys + 0x1F) & ~(uintptr_t)0x1F;
    n_phys = size + (aligned - base);
    if (n_phys > ResourceMemSize)
    {
        OSReport("out of resource memory (%s)\n", szMemoryNames[memType]);
        // PORT: the arena is one bump pointer, so the type that trips it is not the type that filled it.
        OSReport("[gfxmem] failing request: %u bytes (%u KB) of %s\n",
                 (u32)size, (u32)(size >> 10), szMemoryNames[memType]);
        port_ReportResourceArena("exhausted");
        nlBreak();
    }
    g_uResourceAlloc[g_uResourceMarker].m_uBytes[memType] += size;
    return (void*)aligned;
}

/**
 * Offset/Address/Size: 0x310 | 0x801B6C38 | size: 0x130
 */
unsigned long long glplatResourceMark()
{
    unsigned long long marker = glx_GetTexMarkerLevel() | ((unsigned long long)n_phys << 32);

    glx_AdvanceTexMarkerLevel();
    gl_ConstantMarkerAdvance();
    glInventory.ResourceMark();
    ResourceAllocMark();

    return marker;
}

/**
 * Offset/Address/Size: 0x200 | 0x801B6B28 | size: 0x110
 */
void glplatResourceRelease(unsigned long long marker)
{
    int level = (int)(marker & 0xFFFFFFFF);

    n_phys = (u32)(marker >> 32);

    glx_BackupTexMarkerLevel(level);
    gl_ConstantMarkerBackup(level);
    glInventory.ResourceRelease(level);
    ResourceAllocRelease(level);
}

inline u32 RealOrVirtual(eGLMemory memType)
{
    switch (memType)
    {
    case GLM_Header:
    case GLM_Matrix:
    case GLM_IndexData:
        return 1;
    default:
        return 0;
    }
}

inline uintptr_t AlignUp(uintptr_t value, uintptr_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

void* glx_FrameAlloc(unsigned long size, eGLMemory memType, bool bCanReturnNULL)
{
    u32 isLow = RealOrVirtual(memType);
    uintptr_t newTop = AlignUp(p_frame[i_frame][isLow] + n_frame[i_frame][isLow], 32);
    size += newTop - p_frame[i_frame][isLow];

    if (size > FrameMemSizes[isLow])
    {
        if (bCanReturnNULL)
        {
            return NULL;
        }
        OSReport("out of frame memory (%s)\n", szMemoryNames[memType]);
        nlBreak();
        return NULL;
    }

    n_frame[i_frame][isLow] = size;
    return (void*)newTop;
}

/**
 * Offset/Address/Size: 0x140 | 0x801B6A68 | size: 0xC0
 */
void* glplatFrameAlloc(unsigned long size, eGLMemory memType)
{
    return glx_FrameAlloc(size, memType, false);
}

/**
 * Offset/Address/Size: 0x40 | 0x801B6968 | size: 0x100
 */
void glplatFrameAllocNextFrame()
{
    if (glx_MemoryDump)
    {
        tDebugPrintManager::Print(DC_GLPLAT, "memory used: %uKB resource, %uKB frame real, %uKB frame virt\n", n_phys >> 10, n_frame[i_frame][0] >> 10, n_frame[i_frame][1] >> 10);

        tDebugPrintManager::Print(DC_GLPLAT, "       free: %uKB resource, %uKB frame real, %uKB frame virt\n", (ResourceMemSize - n_phys) >> 10, (FrameMemSizes[0] - n_frame[i_frame][0]) >> 10, (FrameMemSizes[1] - n_frame[i_frame][1]) >> 10);

        glx_MemoryDump = false;
    }

    int newFrame = i_frame ^ 1;
    i_frame = newFrame;
    n_frame[newFrame][0] = 0;
    n_frame[newFrame][1] = 0;

    GXInvalidateVtxCache();
    GXInvalidateTexAll();
}

/**
 * Offset/Address/Size: 0x20 | 0x801B6948 | size: 0x20
 */
void glplatGetMatrix(uintptr_t matrix, nlMatrix4& m)
{
    GLMatrix* matrixPtr = (GLMatrix*)matrix;
    matrixPtr->Get(m);
}

/**
 * Offset/Address/Size: 0x0 | 0x801B6928 | size: 0x20
 */
void glplatSetMatrix(uintptr_t matrix, const nlMatrix4& m)
{
    GLMatrix* matrixPtr = (GLMatrix*)matrix;
    matrixPtr->Set(m);
}
