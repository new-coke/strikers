// Byte order for MusyX data files; SDIR_DATA is 0x20 on disc against 0x28 here, so it is rebuilt.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>

extern "C" {
#include "musyx/musyx.h"
#include "musyx/hardware.h"
#include "musyx/stream.h"
#include "musyx/synthdata.h"
}

namespace {

inline u16 rd16(const u8* p) { return (u16)((p[0] << 8) | p[1]); }
inline u32 rd32(const u8* p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}
inline void sw16(u8* p) {
    u8 t = p[0];
    p[0] = p[1];
    p[1] = t;
}
inline void sw32(u8* p) {
    u8 t = p[0];
    p[0] = p[3];
    p[3] = t;
    t = p[1];
    p[1] = p[2];
    p[2] = t;
}

bool logging() {
    static int on = -1;
    if (on < 0) {
        const char* e = getenv("STRIKERS_LOG_AUDIO");
        on = (e != nullptr && *e != '\0') ? 1 : 0;
    }
    return on != 0;
}

// A 0xFFFF-terminated list of u16 ids. Confined to [lo, hi) so a shared table is swapped once.
void swapIdList(u8* b, u32 off, u32 lo, u32 hi) {
    if (off < lo || off >= hi)
        return;
    for (u32 p = off; p + 2 <= hi; p += 2) {
        const u16 v = rd16(b + p);
        sw16(b + p);
        if (v == 0xFFFF)
            break;
    }
}

// On-disc sizes. The host structs differ where they hold a pointer.
constexpr u32 kGroupBytes = 0x28;      // GROUP_DATA, no pointers
constexpr u32 kSdirEntryBytes = 0x20;  // SDIR_DATA_INTER
constexpr u32 kSdirTerminatorBytes = 4;
constexpr u32 kFxTabBytes = 0xA;       // FX_TAB
constexpr u32 kKeymapBytes = 8;        // KEYMAP
constexpr u32 kLayerBytes = 0xC;       // LAYER
constexpr u32 kAdpcmInfoBytes = 0x28;  // SNDADPCMinfo, and DSPADPCMplusInfo's head
constexpr u32 kAdpcmBlockBytes = 6;    // DSPADPCMblock

} // namespace

extern "C" void PortMusyxSwapProj(void* buf, unsigned long size) {
    u8* b = (u8*)buf;
    u32 groups = 0, songs = 0;
    u32 o = 0;
    while (o + kGroupBytes <= size) {
        const u32 next = rd32(b + o);
        sw32(b + o);
        if (next == 0xFFFFFFFF)
            break;
        const u32 hi = next <= size ? next : (u32)size;

        sw16(b + o + 4);                    // id
        const u16 type = rd16(b + o + 6);
        sw16(b + o + 6);

        // macroOff, sampleOff, curveOff, keymapOff, layerOff: id lists, offsets from the file.
        for (u32 f = 8; f < 0x1C; f += 4) {
            const u32 off = rd32(b + o + f);
            sw32(b + o + f);
            swapIdList(b, off, o, hi);
        }

        if (type == 1) {
            // data.fx.tableOff: FX_DATA { u16 num; u16 reserved; FX_TAB fx[num]; }
            const u32 tab = rd32(b + o + 0x1C);
            sw32(b + o + 0x1C);
            if (tab >= o && tab + 4 <= hi) {
                const u16 num = rd16(b + tab);
                sw16(b + tab);
                sw16(b + tab + 2);
                for (u32 k = 0; k < num; ++k) {
                    u8* e = b + tab + 4 + k * kFxTabBytes;
                    if (e + kFxTabBytes > b + hi)
                        break;
                    sw16(e);        // id
                    sw16(e + 2);    // macro; the rest are bytes
                }
            }
        } else {
            // data.song: three offsets; the tables behind them are left alone.
            for (u32 f = 0x1C; f < 0x28; f += 4)
                sw32(b + o + f);
            ++songs;
        }
        ++groups;
        o = next;
    }
    if (logging())
        std::fprintf(stderr, "[port] musyx: project: %u groups swapped, %u song groups left\n",
                     groups, songs);
}

extern "C" void PortMusyxSwapPool(void* buf, unsigned long size) {
    u8* b = (u8*)buf;
    if (size < sizeof(POOL_DATA))
        return;

    // POOL_DATA: macroOff, curveOff, keymapOff, layerOff. 0 is absent; a repeat is swapped once.
    u32 offs[4];
    for (int i = 0; i < 4; ++i) {
        offs[i] = rd32(b + 4 * i);
        sw32(b + 4 * i);
    }

    u32 counts[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        const u32 start = offs[i];
        if (start == 0 || start >= size)
            continue;
        bool seen = false;
        for (int j = 0; j < i; ++j)
            seen = seen || offs[j] == start;
        if (seen)
            continue;

        // MEM_DATA: u32 nextOff from this entry, u16 id, u16 reserved, data. FFFFFFFF ends it.
        u32 p = start;
        while (p + 8 <= size) {
            const u32 next = rd32(b + p);
            sw32(b + p);
            if (next == 0xFFFFFFFF)
                break;
            sw16(b + p + 4);
            sw16(b + p + 6);
            u8* d = b + p + 8;
            u32 dlen = next > 8 ? next - 8 : 0;
            if (p + 8 + dlen > size)
                dlen = (u32)size - p - 8;

            switch (i) {
            case 0:     // macro: MSTEP { u32 para[2]; } steps
                for (u32 k = 0; k + 4 <= dlen; k += 4)
                    sw32(d + k);
                break;
            case 1:     // curve: bytes
                break;
            case 2:     // keymap: KEYMAP { u16 id; s8; u8; s16 prioOffset; u8[2]; } x128
                for (u32 k = 0; k + kKeymapBytes <= dlen; k += kKeymapBytes) {
                    sw16(d + k);
                    sw16(d + k + 4);
                }
                break;
            case 3: {   // layer: u32 num; LAYER { u16 id; u8 x4; s16 prioOffset; u8[4]; } x num
                if (dlen < 4)
                    break;
                const u32 num = rd32(d);
                sw32(d);
                for (u32 k = 0; k < num; ++k) {
                    u8* e = d + 4 + k * kLayerBytes;
                    if (e + kLayerBytes > d + dlen)
                        break;
                    sw16(e);
                    sw16(e + 6);
                }
            } break;
            }
            ++counts[i];
            if (next < 8)
                break;
            p += next;
        }
    }
    if (logging())
        std::fprintf(stderr, "[port] musyx: pool: %u macros, %u curves, %u keymaps, %u layers\n",
                     counts[0], counts[1], counts[2], counts[3]);
}

extern "C" void* PortMusyxConvertSDir(const void* buf, unsigned long size, unsigned long* outSize) {
    const u8* b = (const u8*)buf;
    if (outSize != nullptr)
        *outSize = 0;

    u32 n = 0;
    while ((unsigned long)n * kSdirEntryBytes + 2 <= size && rd16(b + n * kSdirEntryBytes) != 0xFFFF)
        ++n;

    // The extra data begins after the terminator, which is an id and padding only.
    u32 tailStart = n * kSdirEntryBytes + kSdirTerminatorBytes;
    for (u32 i = 0; i < n; ++i) {
        const u32 ex = rd32(b + i * kSdirEntryBytes + 0x1C);
        if (ex != 0 && ex < tailStart)
            tailStart = ex;
    }
    if (tailStart > size)
        tailStart = (u32)size;

    const size_t tableBytes = (size_t)(n + 1) * sizeof(SDIR_DATA);
    const size_t tailBytes = size - tailStart;
    u8* out = (u8*)std::calloc(1, tableBytes + tailBytes);
    if (out == nullptr)
        return nullptr;

    SDIR_DATA* t = (SDIR_DATA*)out;
    for (u32 i = 0; i < n; ++i) {
        const u8* e = b + i * kSdirEntryBytes;
        t[i].id = rd16(e);
        t[i].ref_cnt = rd16(e + 2);
        t[i].offset = rd32(e + 4);
        t[i].addr = nullptr;                 // the disc's u32 here is always 0
        t[i].header.info = rd32(e + 0xC);
        t[i].header.length = rd32(e + 0x10);
        t[i].header.loopOffset = rd32(e + 0x14);
        t[i].header.loopLength = rd32(e + 0x18);
        const u32 ex = rd32(e + 0x1C);
        // Relative to the table's start, as dataGetSample resolves it.
        t[i].extraData = (ex != 0 && ex >= tailStart) ? (u32)(ex - tailStart + tableBytes) : 0;
    }
    t[n].id = 0xFFFF;
    std::memcpy(out + tableBytes, b + tailStart, tailBytes);

    // SNDADPCMinfo, or for type 1 the same head plus one DSPADPCMblock per 14-sample block.
    std::set<u32> done;
    const u8* end = out + tableBytes + tailBytes;
    u32 infos = 0;
    for (u32 i = 0; i < n; ++i) {
        const u32 ex = t[i].extraData;
        if (ex == 0 || !done.insert(ex).second)
            continue;
        const u8 type = (u8)(t[i].header.length >> 24);
        if (type != 0 && type != 1)
            continue;
        u8* x = out + ex;
        if (x + kAdpcmInfoBytes > end)
            continue;
        sw16(x);                             // numCoef; initialPS and loopPS are bytes
        sw16(x + 4);                         // loopY0
        sw16(x + 6);                         // loopY1
        for (u32 k = 0; k < 16; ++k)
            sw16(x + 8 + 2 * k);             // coefTab[8][2]
        if (type == 1) {
            const u32 samples = t[i].header.length & 0xFFFFFF;
            const u32 blocks = (samples + 13) / 14;
            for (u32 k = 0; k < blocks; ++k) {
                u8* blk = x + kAdpcmInfoBytes + k * kAdpcmBlockBytes;
                if (blk + kAdpcmBlockBytes > end)
                    break;
                sw16(blk);                   // Y0
                sw16(blk + 2);               // Y1; PS is a byte
            }
        }
        ++infos;
    }

    if (logging())
        std::fprintf(stderr,
                     "[port] musyx: sample directory: %u entries rebuilt at %u bytes each "
                     "(disc %u), %u ADPCM info blocks swapped, %lu bytes\n",
                     n, (unsigned)sizeof(SDIR_DATA), kSdirEntryBytes, infos,
                     (unsigned long)(tableBytes + tailBytes));
    if (outSize != nullptr)
        *outSize = tableBytes + tailBytes;
    return out;
}
