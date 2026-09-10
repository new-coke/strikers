// Byte-swap pass for BMD models: glxLoadModel walks the file in place, so the chunk tree is
// converted before it sees it and payloads per type as it reaches them.

// nlChunk header: u32 ID, type in the low 24 bits and payload alignment in bits 24..30, then u32
// size in payload bytes excluding the header.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "port/endian.h"

static int chunk_is_container(uint32_t type)
{
    switch (type)
    {
    // BMD (.glg)
    case 0x1B100:  // file container
    case 0x1B000:  // model container
    case 0x1B008:  // SKIN
    // Not 0x1B003 (MODELS): its payload is glModel records, not chunks, converted by the loader;
    // listed, a record's words read as a header with an impossible size and refuse the tree.
    // SAnim (.sanim)
    case 0x17000:  // animation container
    case 0x17100:  // per-node container, holding rot/trans/scale sub-chunks
    // SHierarchy (.shier), from cSHierarchy::IsValidChunkID
    case 0x18000:  // skeleton container
    // Animation retargeting (.rtg), from AnimRetargetList::IsValidChunkID
    case 0x17104:  // retarget list: the list, the retarget array, then one
                   // bone-map chunk per retarget
        return 1;
    default:
        return 0;
    }
}

// Adds the chunks converted to *n. Returns 0 on a chunk sized past its container, an aligned
// payload past the chunk's own end (GetData applies the id's alignment), or a tree deeper than
// any real file; the game's own walk would read past the buffer on any of them.
static int swap_range(uint8_t* p, uint8_t* end, int depth, unsigned long* n)
{
    if (depth > 8)
        return 0;

    while (p + 8 <= end)
    {
        uint32_t id = port_be32(p);
        uint32_t size = port_be32(p + 4);
        unsigned long len;

        if (size > (uint32_t)(end - p - 8) || port_chunk_payload(p, id, size, &len) == NULL)
            return 0;

        memcpy(p, &id, 4);          // now in host order for the loader
        memcpy(p + 4, &size, 4);
        (*n)++;

        if (chunk_is_container(id & 0x00FFFFFFu)
            && !swap_range(p + 8, p + 8 + size, depth + 1, n))
            return 0;

        p += 8 + size;
    }
    return 1;
}

// Called before anything inspects a chunk-format asset; serves .sanim too, same nlChunk layout.
// Returns the number of chunks converted; 0 means the tree is not to be walked.
unsigned long port_bmd_swap_headers(void* data, unsigned long size)
{
    unsigned long n = 0;
    if (data == NULL || size < 8)
        return 0;
    return swap_range((uint8_t*)data, (uint8_t*)data + size, 0, &n) ? n : 0;
}

// Packet and stream conversion: these records hold 4-byte disc pointers and 8-byte host ones, so
// they are re-laid out field by field rather than swapped in place.

// glStateBundle is 0x36 (54) on disc and 82 on host because `matrix` and `texture[6]` are
// addresses: a GLMatrix in the frame arena, and a texture id or PlatTexture*.

#define BMD_DISC_PACKET_SIZE 0x4A
#define BMD_DISC_STREAM_SIZE 0x06

unsigned long port_bmd_packet_count(unsigned long chunkSize)
{
    return chunkSize / BMD_DISC_PACKET_SIZE;
}

unsigned long port_bmd_stream_count(unsigned long chunkSize)
{
    return chunkSize / BMD_DISC_STREAM_SIZE;
}

#pragma pack(push, 1)
// Mirrors glStateBundle; on disc its last two fields are 4 bytes each, not 8.
typedef struct
{
    uint64_t texturestate;
    uint32_t materialstate;
    uint32_t program;
    uint32_t raster;
    uintptr_t matrix;
    uintptr_t texture[6];
    uint8_t texconfig;
    uint8_t pad;
    uint32_t userStateKey;
} PortStateBundle;

typedef struct
{
    uintptr_t userData;
    uintptr_t indexBuffer;
    uint16_t numVertices;
    uint8_t primType;
    uint8_t numStreams;
    void* streams;
    PortStateBundle state;
    uint32_t materialset;
} PortPacket;

typedef struct
{
    uintptr_t address;
    uint8_t id;
    uint8_t stride;
    uint8_t beData;    // host-only; see glModelStream in NL/gl/glUserData.h
    uint32_t dataSize; // host-only; the loader fills it in at relocation
} PortStream;
#pragma pack(pop)

_Static_assert(sizeof(PortStateBundle) == 82,
               "PortStateBundle must match glStateBundle");
_Static_assert(sizeof(PortPacket) == 114, "PortPacket must match glModelPacket");
_Static_assert(sizeof(PortStream) == 15, "PortStream must match glModelStream");

void port_bmd_convert_packets(void* dst, const void* src, unsigned long count)
{
    PortPacket* out = (PortPacket*)dst;
    const uint8_t* in = (const uint8_t*)src;

    for (unsigned long i = 0; i < count; i++, in += BMD_DISC_PACKET_SIZE, out++)
    {
        out->userData = port_be32(in + 0x00);
        out->indexBuffer = port_be32(in + 0x04);
        out->numVertices = port_be16(in + 0x08);
        out->primType = in[0x0A];
        out->numStreams = in[0x0B];
        {
            uint32_t off = port_be32(in + 0x0C);
            out->streams = (void*)(uintptr_t)(
                (off / BMD_DISC_STREAM_SIZE) * sizeof(PortStream));
        }

        // The embedded state bundle at its disc offsets: u64 at 0x00, u32s through 0x2F, two bytes
        // at 0x30, u32 at 0x32, all relative to the packet's 0x10.
        {
            const uint8_t* st = in + 0x10;
            unsigned long t;
            out->state.texturestate =
                ((uint64_t)port_be32(st + 0x00) << 32) | port_be32(st + 0x04);
            out->state.materialstate = port_be32(st + 0x08);
            out->state.program = port_be32(st + 0x0C);
            out->state.raster = port_be32(st + 0x10);
            // Matrix and texture handles on disc; the renderer resolves them to pointers.
            out->state.matrix = port_be32(st + 0x14);
            for (t = 0; t < 6; t++)
                out->state.texture[t] = port_be32(st + 0x18 + t * 4);
            out->state.texconfig = st[0x30];
            out->state.pad = st[0x31];
            out->state.userStateKey = port_be32(st + 0x32);
        }

        out->materialset = port_be32(in + 0x46);
    }
}

void port_bmd_convert_streams(void* dst, const void* src, unsigned long count)
{
    PortStream* out = (PortStream*)dst;
    const uint8_t* in = (const uint8_t*)src;

    for (unsigned long i = 0; i < count; i++, in += BMD_DISC_STREAM_SIZE, out++)
    {
        out->address = port_be32(in + 0x00);
        out->id = in[0x04];
        out->stride = in[0x05];
        // Inside the model file, so big-endian; anything built at runtime leaves this 0.
        out->beData = 1;
        out->dataSize = 0;
    }
}

// INDEX_DATA is big-endian u16 indices. Nothing converted it, which survived only while
// dlMakeDisplayList copied it verbatim into a big-endian FIFO; an index buffer built at runtime is
// host order, and the swap belongs where the FIFO is written.
void port_bmd_swap_indices(void* data, unsigned long bytes)
{
    uint8_t* p = (uint8_t*)data;
    unsigned long i;

    for (i = 0; i + 1 < bytes; i += 2)
    {
        uint8_t hi = p[i];
        p[i] = p[i + 1];
        p[i + 1] = hi;
    }
}

static int port_cmp_u32(const void* a, const void* b)
{
    uint32_t x = *(const uint32_t*)a;
    uint32_t y = *(const uint32_t*)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

// Console GXSetArray takes no length and the file records none, so an array runs from its own
// offset to the next one's in the concatenated DISPLAY_LIST chunk.

// Bounding each at the end of the chunk instead makes several streams claim most of it, and Aurora
// aborts in ByteBuffer::resize. Called before the loader relocates.
void port_bmd_stream_sizes(void* streams, unsigned long count,
                           unsigned long vertexDataSize)
{
    PortStream* s = (PortStream*)streams;
    uint32_t* bounds;
    unsigned long n = 0;
    unsigned long i;

    if (count == 0)
    {
        return;
    }

    bounds = (uint32_t*)malloc(count * sizeof(uint32_t));
    if (bounds == NULL)
    {
        return;
    }

    for (i = 0; i < count; i++)
    {
        bounds[n++] = (uint32_t)s[i].address;
    }
    qsort(bounds, n, sizeof(uint32_t), port_cmp_u32);

    for (i = 0; i < count; i++)
    {
        uint32_t off = (uint32_t)s[i].address;
        uint32_t end = (uint32_t)vertexDataSize;
        unsigned long lo = 0;
        unsigned long hi = n;

        while (lo < hi)
        {
            unsigned long mid = lo + (hi - lo) / 2;
            if (bounds[mid] > off)
            {
                hi = mid;
            }
            else
            {
                lo = mid + 1;
            }
        }
        if (lo < n && bounds[lo] < end)
        {
            end = bounds[lo];
        }
        s[i].dataSize = off < end ? end - off : 0;
    }

    free(bounds);
}

// glModel is 0x10 (16) on disc and 24 on host; its `packets` byte offset is in on-disc units and is
// rescaled to the host stride.

#define BMD_DISC_MODEL_SIZE 0x10

unsigned long port_bmd_model_count(unsigned long chunkSize)
{
    return chunkSize / BMD_DISC_MODEL_SIZE;
}

void port_bmd_convert_models(void* dst, const void* src, unsigned long count,
                             unsigned long hostModelSize)
{
    const uint8_t* in = (const uint8_t*)src;
    uint8_t* out = (uint8_t*)dst;

    for (unsigned long i = 0; i < count;
         i++, in += BMD_DISC_MODEL_SIZE, out += hostModelSize)
    {
        uint32_t numPackets = port_be32(in + 0x00);
        uint32_t id = port_be32(in + 0x04);
        uint32_t pad = port_be32(in + 0x08);
        uint32_t off = port_be32(in + 0x0C);

        memcpy(out + 0x00, &numPackets, 4);
        memcpy(out + 0x04, &id, 4);
        memcpy(out + 0x08, &pad, 4);

        uintptr_t scaled =
            (uintptr_t)(off / BMD_DISC_PACKET_SIZE) * sizeof(PortPacket);
        const unsigned long ptrOff =
            (12 + sizeof(void*) - 1) & ~(unsigned long)(sizeof(void*) - 1);
        memcpy(out + ptrOff, &scaled, sizeof scaled);
    }
}
