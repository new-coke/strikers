#include "dolphin/types.h"
#include "NL/nlMemory.h"
#include "NL/nlConfig.h"
#include "NL/glx/glxMemory.h"
#include "port/overlay.h"   // PORT: PortGfxArenaStats
#if defined(PORT_VITA) && defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "port/native_match_runtime.hpp"
#endif
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#if defined(STRIKERS_VITA)
#include <psp2/kernel/sysmem.h>
#endif

// PORT: the desktop port widens pointers and several GL bookkeeping objects,
// so it needs extra room versus the original 32-bit GameCube layouts.  Vita
// is 32-bit again; carrying the desktop 8x multiplier there asks the game's
// allocator for ~96 MiB during glStartup and guarantees an early boot failure.
#if defined(STRIKERS_VITA)
#define PORT_GFX_ARENA_SCALE 1u
#else
#define PORT_GFX_ARENA_SCALE 8u
#endif
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

u32 glx_GetResourceMarkerLevel()
{
    return static_cast<u32>(g_uResourceMarker);
}
#if defined(STRIKERS_VITA)
struct VitaResourceSegment
{
    SceUID uid;
    uintptr_t base;
    u32 logicalStart;
    u32 size;
};

// Prefer one large early reservation so the GameCube-style linear arena stays
// truly linear.  If CDRAM is already fragmented, fall back to 16 MiB chunks;
// this is large enough for the biggest retail textures while wasting far less
// tail space than the old 8 MiB segmentation.
static const u32 kVitaResourceSegmentSize = MB(16);
static const int kVitaResourceMaxSegments = 8;
static VitaResourceSegment s_vitaResourceSegments[kVitaResourceMaxSegments];
static int s_vitaResourceSegmentCount;
static u32 s_vitaResourceReservedSize;
#endif

// PORT: addresses, not offsets. vita_async_send needs a third frame buffer
// because some packets reuse the previous frame's allocations.
static uintptr_t p_frame[3][2];
static u32 n_frame[3][2];
static int s_frameBuffers = 2;

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
#if defined(STRIKERS_VITA)
    const u32 freeNow = ResourceMemSize > n_phys ? ResourceMemSize - n_phys : 0;
    const u32 consumed = glx_mem0 > freeNow ? glx_mem0 - freeNow : 0;
    OSReport("[gfxmem] bundle %s: +%u KB, used=%u/%u KB, free=%u KB\n",
             filename != NULL ? filename : "?", consumed >> 10, n_phys >> 10,
             ResourceMemSize >> 10, freeNow >> 10);
#else
    (void)filename;
#endif
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

#if defined(STRIKERS_VITA)
static bool VitaResourceCached()
{
    const char* setting = getenv("STRIKERS_GFX_RESOURCE_CACHED");
    // Changing the memory domain also changes CPU-heap headroom. Keep the
    // existing allocation profile until a port explicitly budgets cached RAM.
    return setting != NULL && setting[0] == '1';
}

static u32 VitaResourceTargetSize()
{
    const u32 desiredDefault = MB(48);
    const u32 vitaGlSafetyReserve = MB(48);
    u32 desired = desiredDefault;
    const char* overrideMb = getenv("STRIKERS_GFX_RESOURCE_MB");
    if (overrideMb != NULL && *overrideMb != '\0')
    {
        const unsigned long mb = strtoul(overrideMb, NULL, 10);
        if (mb >= 32 && mb <= 64)
            desired = (u32)mb * MB(1);
        else
            OSReport("[gfxmem] ignoring STRIKERS_GFX_RESOURCE_MB=%s (expected 32..64)\n",
                     overrideMb);
    }

    // GX source meshes/display lists are decoded by the CPU every frame. Keep
    // them cacheable; only converted GPU resources belong in vitaGL's CDRAM.
    // Preserve headroom for the renderer and compiler in either memory domain.
    SceKernelFreeMemorySizeInfo info = {};
    info.size = sizeof(info);
    if (sceKernelGetFreeMemorySize(&info) >= 0)
    {
        const u32 freeCdram = VitaResourceCached() ? (u32)info.size_user : (u32)info.size_cdram;
        const u32 safeMaximum = freeCdram > vitaGlSafetyReserve
            ? freeCdram - vitaGlSafetyReserve : 0;
        u32 target = desired < safeMaximum ? desired : safeMaximum;
        target &= ~(MB(1) - 1);
        OSReport("[gfxmem] %s plan: free=%u KB GLX=%u KB reserve=%u KB desired=%u KB\n",
                 VitaResourceCached() ? "cached RAM" : "CDRAM", freeCdram >> 10, target >> 10, vitaGlSafetyReserve >> 10,
                 desired >> 10);
        return target;
    }

    return desired;
}

bool glxVitaReserveResourceArena()
{
    if (s_vitaResourceSegmentCount != 0)
        return true;

    const u32 target = VitaResourceTargetSize();
    const SceKernelMemBlockType memoryType = VitaResourceCached()
        ? SCE_KERNEL_MEMBLOCK_TYPE_USER_RW : SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW;
    const char* memoryName = VitaResourceCached() ? "cached RAM" : "CDRAM";
    if (target < MB(32))
    {
        OSReport("[gfxmem] not enough %s for stable GLX arena: %u KB target\n",
                 memoryName, target >> 10);
        return false;
    }

    u32 reserved = 0;

    // Best case: one contiguous virtual memblock. We reserve before
    // vitaGL starts, so this normally succeeds and eliminates all segment-tail
    // waste from the GameCube bump allocator.
    {
        const SceUID uid = sceKernelAllocMemBlock(
            "strikersGLX", memoryType,
            (SceSize)target, NULL);
        if (uid >= 0)
        {
            void* base = NULL;
            const int baseRc = sceKernelGetMemBlockBase(uid, &base);
            if (baseRc >= 0 && base != NULL)
            {
                VitaResourceSegment& segment = s_vitaResourceSegments[0];
                segment.uid = uid;
                segment.base = (uintptr_t)base;
                segment.logicalStart = 0;
                segment.size = target;
                s_vitaResourceSegmentCount = 1;
                s_vitaResourceReservedSize = target;
                ResourceMemSize = target;
                p_phys = segment.base;
                OSReport("[gfxmem] early %s resource reservation: %u KB contiguous at %p\n",
                         memoryName, target >> 10, base);
                return true;
            }
            sceKernelFreeMemBlock(uid);
        }
        OSReport("[gfxmem] contiguous %s reservation unavailable; using 16 MiB segments\n", memoryName);
    }

    while (reserved < target && s_vitaResourceSegmentCount < kVitaResourceMaxSegments)
    {
        const u32 remaining = target - reserved;
        const u32 chunk = remaining < kVitaResourceSegmentSize ? remaining : kVitaResourceSegmentSize;
        char name[32];
        snprintf(name, sizeof(name), "strikersGLX%u", (unsigned int)s_vitaResourceSegmentCount);

        const SceUID uid = sceKernelAllocMemBlock(
            name, memoryType, (SceSize)chunk, NULL);
        if (uid < 0)
        {
            OSReport("[gfxmem] early %s segment %d allocation failed: %u KB rc=0x%08X\n",
                     memoryName, s_vitaResourceSegmentCount, chunk >> 10, (u32)uid);
            break;
        }

        void* base = NULL;
        const int baseRc = sceKernelGetMemBlockBase(uid, &base);
        if (baseRc < 0 || base == NULL)
        {
            OSReport("[gfxmem] early %s segment %d base lookup failed rc=0x%08X\n",
                     memoryName, s_vitaResourceSegmentCount, (u32)baseRc);
            sceKernelFreeMemBlock(uid);
            break;
        }

        VitaResourceSegment& segment = s_vitaResourceSegments[s_vitaResourceSegmentCount++];
        segment.uid = uid;
        segment.base = (uintptr_t)base;
        segment.logicalStart = reserved;
        segment.size = chunk;
        reserved += chunk;
    }

    s_vitaResourceReservedSize = reserved;
    if (reserved < MB(32))
    {
        OSReport("[gfxmem] early %s reservation insufficient: %u/%u KB\n",
                 memoryName, reserved >> 10, target >> 10);
        return false;
    }

    ResourceMemSize = reserved;
    p_phys = s_vitaResourceSegments[0].base;
    OSReport("[gfxmem] early %s resource reservation: %u/%u KB in %d x <=16MiB segment(s)\n",
             memoryName, reserved >> 10, target >> 10, s_vitaResourceSegmentCount);
    return true;
}

bool glxVitaResourceArenaOwns(const void* p)
{
    const uintptr_t addr = (uintptr_t)p;
    if (addr == 0)
        return false;

    for (int i = 0; i < s_vitaResourceSegmentCount; ++i)
    {
        const VitaResourceSegment& segment = s_vitaResourceSegments[i];
        if (addr >= segment.base && addr - segment.base < segment.size)
            return true;
    }
    return false;
}
#endif

/**
 * Offset/Address/Size: 0x4F4 | 0x801B6E1C | size: 0x3F0
 */
bool glxInitMemory()
{
    bool bDeveloper = GetConfigBool(Config::Global(), "e3_build", false);
    const char* szResourceKey = bDeveloper ? "e3 resource total memory" : "resource total memory";

    ResourceMemSize = GetFromConfig(szResourceKey, ResourceMemSize);
    // PORT: the desktop port needs a large multiplier for widened host-side
    // structures. Vita is 32-bit again, but the PAL global texture bundle plus
    // the permanent GL targets already exceed the retail 12 MiB arena. Reserve
    // the long-lived resource pool in small CDRAM segments before Aurora/vitaGL
    // fragments that heap, while leaving the per-frame pools at their native
    // 32-bit sizes. strikers.ini can override the total as
    // `gfx_resource_mb=<n>` for hardware profiling without another build.
#if defined(STRIKERS_VITA)
    {
        // Use a separate OS memblock rather than consume the game's own heap.
        // These are CPU-readable source resources, not native GPU allocations.
        if (s_vitaResourceSegmentCount == 0)
            (void)glxVitaReserveResourceArena();
        if (s_vitaResourceReservedSize != 0)
            ResourceMemSize = s_vitaResourceReservedSize;
    }
#else
    // PORT: see the note on, host structures are wider.
    ResourceMemSize *= PORT_GFX_ARENA_SCALE;
#endif

    uintptr_t pMem = p_phys;
#if defined(STRIKERS_VITA)
    if (s_vitaResourceSegmentCount != 0)
        pMem = s_vitaResourceSegments[0].base;
#endif
    if (pMem == 0)
        pMem = (uintptr_t)nlMalloc(ResourceMemSize, 32, false);
    if (pMem == 0)
    {
        OSReport("[gfxmem] resource arena allocation failed: %u KB requested\n",
                 ResourceMemSize >> 10);
        return false;
    }
    p_phys = pMem;

    FrameMemSizeReal = GetFromConfig("frame vertex memory", FrameMemSizeReal);
    FrameMemSizeReal *= PORT_GFX_ARENA_SCALE;

#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    {
        const char* asyncSend = getenv("STRIKERS_VITA_ASYNC_SEND");
        s_frameBuffers = asyncSend && strcmp(asyncSend, "1") == 0 ? 3 : 2;
    }
#endif
    pMem = (uintptr_t)nlMalloc(FrameMemSizeReal * s_frameBuffers, 32, false);
    if (pMem == 0 && s_frameBuffers == 3)
    {
        OSReport("[gfxmem] no room for a third frame buffer; asynchronous send off\n");
        s_frameBuffers = 2;
        pMem = (uintptr_t)nlMalloc(FrameMemSizeReal * s_frameBuffers, 32, false);
    }
    if (pMem == 0)
    {
        OSReport("[gfxmem] frame vertex allocation failed: %u KB requested\n",
                 (FrameMemSizeReal * s_frameBuffers) >> 10);
        return false;
    }
    for (int i = 0; i < s_frameBuffers; ++i)
        p_frame[i][0] = pMem + FrameMemSizeReal * i;

    FrameMemSizeVirt = GetFromConfig("frame header memory", FrameMemSizeVirt);
    FrameMemSizeVirt *= PORT_GFX_ARENA_SCALE;

    uintptr_t pVirt = (uintptr_t)nlVirtualAlloc(FrameMemSizeVirt * s_frameBuffers, false);
    if (pVirt == 0)
    {
        OSReport("[gfxmem] frame header allocation failed: %u KB requested\n",
                 (FrameMemSizeVirt * s_frameBuffers) >> 10);
        return false;
    }

    for (int i = 0; i < s_frameBuffers; ++i)
    {
        p_frame[i][1] = pVirt + FrameMemSizeVirt * i;
        n_frame[i][0] = 0;
        n_frame[i][1] = 0;
    }
    i_frame = 0;
    n_phys = 0;
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
#if defined(STRIKERS_VITA)
extern "C" void strikers_native_prepare_memory_write(const void* address, size_t bytes) noexcept;
extern "C" void strikers_native_notify_memory_write(const void* address, size_t bytes) noexcept;
static void* glplatResourceAllocRaw(unsigned long size, eGLMemory memType);
/* Resource and frame arenas are reused (marker release, double-buffered
 * frames). Publish every handed-out range as written so Aurora never serves
 * a cached display list or stable geometry from a previous occupant. */
void* glplatResourceAlloc(unsigned long size, eGLMemory memType)
{
    void* block = glplatResourceAllocRaw(size, memType);
    if (block != NULL && size != 0)
    {
        strikers_native_prepare_memory_write(block, size);
        strikers_native_notify_memory_write(block, size);
    }
    return block;
}
static void* glplatResourceAllocRaw(unsigned long size, eGLMemory memType)
#else
void* glplatResourceAlloc(unsigned long size, eGLMemory memType)
#endif
{
#if defined(STRIKERS_VITA)
    if (s_vitaResourceSegmentCount != 0)
    {
        u32 logical = (n_phys + 0x1F) & ~0x1Fu;
        VitaResourceSegment* chosen = NULL;
        u32 offset = 0;

        for (int i = 0; i < s_vitaResourceSegmentCount; ++i)
        {
            VitaResourceSegment& segment = s_vitaResourceSegments[i];
            const u32 segEnd = segment.logicalStart + segment.size;
            if (logical < segment.logicalStart)
                logical = segment.logicalStart;
            if (logical >= segEnd)
                continue;

            offset = logical - segment.logicalStart;
            if (size <= segment.size - offset)
            {
                chosen = &segment;
                break;
            }

            // Keep each individual allocation physically contiguous. The
            // logical bump pointer simply skips the tail of this CDRAM block.
            logical = segEnd;
        }

        const u32 newUsed = chosen != NULL ? logical + (u32)size : ResourceMemSize + 1;
        if (chosen == NULL || newUsed > ResourceMemSize)
        {
            u32 payload = 0;
            for (s32 level = 0; level <= g_uResourceMarker; ++level)
                payload += g_uResourceAlloc[level].GetTotal();
            const u32 packingWaste = n_phys > payload ? n_phys - payload : 0;
            OSReport("out of resource memory (%s)\n", szMemoryNames[memType]);
            OSReport("[gfxmem] segmented request failed: %u bytes (%u KB) of %s; used=%u bytes required=%u bytes budget=%u bytes segments=%d packing_waste=%u bytes\n",
                     (u32)size, (u32)(size >> 10), szMemoryNames[memType], n_phys,
                     newUsed, ResourceMemSize, s_vitaResourceSegmentCount, packingWaste);
            port_ReportResourceArena("exhausted");
            nlBreak();
        }

        n_phys = newUsed;
        g_uResourceAlloc[g_uResourceMarker].m_uBytes[memType] += size;
        return (void*)(chosen->base + offset);
    }
#endif

    uintptr_t base = p_phys;
    // PORT: mask in pointer width, ~0x1FU would clear the top half.
    uintptr_t aligned = (base + n_phys + 0x1F) & ~(uintptr_t)0x1F;
    const u32 newUsed = (u32)(size + (aligned - base));
    if (newUsed > ResourceMemSize)
    {
        OSReport("out of resource memory (%s)\n", szMemoryNames[memType]);
        // PORT: the arena is one bump pointer, so the type that trips it is not the type that filled it.
        OSReport("[gfxmem] failing request: %u bytes (%u KB) of %s; used=%u KB required=%u KB budget=%u KB\n",
                 (u32)size, (u32)(size >> 10), szMemoryNames[memType], n_phys >> 10,
                 newUsed >> 10, ResourceMemSize >> 10);
        port_ReportResourceArena("exhausted");
        nlBreak();
    }
    n_phys = newUsed;
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

#if defined(PORT_VITA) && defined(STRIKERS_VITA_NATIVE_RUNTIME)
    // Remove native mesh bindings before the bump allocator reuses these
    // DisplayList addresses (ABA-safe resource generations).
    port::directmatch::on_resource_release(static_cast<uint32_t>(level));
#endif


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

    int newFrame = (i_frame + 1) % s_frameBuffers;
    i_frame = newFrame;
#if defined(STRIKERS_VITA)
    // Everything the new frame half held is rewritten this frame.
    if (n_frame[newFrame][0] != 0)
    {
        strikers_native_prepare_memory_write((const void*)p_frame[newFrame][0], n_frame[newFrame][0]);
        strikers_native_notify_memory_write((const void*)p_frame[newFrame][0], n_frame[newFrame][0]);
    }
    if (n_frame[newFrame][1] != 0)
    {
        strikers_native_prepare_memory_write((const void*)p_frame[newFrame][1], n_frame[newFrame][1]);
        strikers_native_notify_memory_write((const void*)p_frame[newFrame][1], n_frame[newFrame][1]);
    }
#endif
    n_frame[newFrame][0] = 0;
    n_frame[newFrame][1] = 0;

#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
    GXInvalidateVtxCache();
    GXInvalidateTexAll();
#endif
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

int glxFrameBufferCount() { return s_frameBuffers; }
