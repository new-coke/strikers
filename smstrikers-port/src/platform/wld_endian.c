// Byte-swap pass for .wld world objects, which World::LoadObjectData casts structs over.

#include <stdint.h>
#include <string.h>

#include "port/endian.h"

#define WLD_CHUNK_ROOT      0x19000
#define WLD_CHUNK_OBJECT    0x19003
#define WLD_CHUNK_LIGHT     0x19005
#define WLD_CHUNK_EMITTER   0x19101
#define WLD_CHUNK_HELPER    0x19201
#define WLD_CHUNK_PHYSICS   0x1D000
#define WLD_CHUNK_PHYS_NUM  0x1D001
#define WLD_CHUNK_PHYS_ELEM 0x1D002

// Offsets from world.cpp's structs, whose unsigned long fields the port narrows to u32.
struct wld_record
{
    uint32_t type;
    uint32_t wordOffset;   // bytes; everything before this is a name
    uint32_t wordCount;
    uint32_t size;         // total record size, as a sanity check
};

static const struct wld_record wld_records[] = {
    { WLD_CHUNK_OBJECT,  0x80, 24, 0xE0 },
    { WLD_CHUNK_LIGHT,   0x40, 24, 0xA0 },
    { WLD_CHUNK_EMITTER, 0x40, 24, 0xA0 },
    { WLD_CHUNK_HELPER,  0x3C, 17, 0x80 },
};

// CharacterPhysicsElement is 0xA0 with two name arrays in the middle, so it takes three runs.
static const struct { uint32_t off, words; } wld_phys_elem_runs[] = {
    { 0x00, 16 },   // matLocalToParent
    { 0x60,  1 },   // uHashID          (szName at 0x40 is bytes)
    { 0x84,  7 },   // uParentHashID .. uReserved (szParentName at 0x64 is bytes)
};
#define WLD_PHYS_ELEM_SIZE 0xA0

static void wld_swap_phys_elements(uint8_t* data, uint32_t size)
{
    uint32_t n = size / WLD_PHYS_ELEM_SIZE;
    uint32_t i;
    unsigned long r;

    for (i = 0; i < n; i++)
    {
        uint8_t* e = data + i * WLD_PHYS_ELEM_SIZE;
        for (r = 0; r < sizeof wld_phys_elem_runs / sizeof wld_phys_elem_runs[0]; r++)
        {
            port_be32_array(e + wld_phys_elem_runs[r].off,
                            wld_phys_elem_runs[r].words);
        }
    }
}

// The count chunks (0x19001, 0x19002, 0x19004, 0x19100, 0x19200) are one u32 each.
static int wld_is_count_chunk(uint32_t type)
{
    return type == 0x19001 || type == 0x19002 || type == 0x19004
        || type == 0x19100 || type == 0x19200;
}

static void wld_swap_payload(uint8_t* data, uint32_t size, uint32_t type)
{
    unsigned long i;

    if (type == WLD_CHUNK_PHYS_ELEM)
    {
        wld_swap_phys_elements(data, size);
        return;
    }

    if ((wld_is_count_chunk(type) || type == WLD_CHUNK_PHYS_NUM) && size >= 4)
    {
        port_be32_array(data, 1);
        return;
    }

    for (i = 0; i < sizeof wld_records / sizeof wld_records[0]; i++)
    {
        const struct wld_record* r = &wld_records[i];
        if (r->type != type)
            continue;
        // A record of the wrong size is not the record the struct describes.
        if (size != r->size)
            return;
        port_be32_array(data + r->wordOffset, r->wordCount);
        return;
    }
}

// Convert a run of sibling chunks in place: the physics block inside a .wld, or the same container
// as a file of its own. Adds to *n; returns 0 on a chunk sized or aligned past its end.
static int wld_swap_children(uint8_t* p, uint32_t size, unsigned long* n)
{
    uint8_t* end = p + size;

    while (p + 8 <= end)
    {
        uint32_t id = port_be32(p);
        uint32_t chunkSize = port_be32(p + 4);
        unsigned long len;
        uint8_t* data;

        if (chunkSize > (uint32_t)(end - p - 8) || ((id & 0x7F000000u) >> 24) > 16)
            return 0;

        memcpy(p, &id, 4);
        memcpy(p + 4, &chunkSize, 4);
        (*n)++;

        // The payload length, not the chunk size: an aligned chunk's padding is inside its size.
        data = port_chunk_payload(p, id, chunkSize, &len);
        if (data == NULL)
            return 0;
        wld_swap_payload(data, (uint32_t)len, id & 0x00FFFFFFu);
        p += 8 + chunkSize;
    }

    return 1;
}

// A character's physics elements are the same 0x1D000 container, written as its own file.
unsigned long port_phys_swap(void* data, unsigned long size)
{
    uint8_t* base = (uint8_t*)data;
    uint32_t rootId, rootSize;

    if (data == NULL || size < 8)
        return 0;

    rootId = port_be32(base);
    rootSize = port_be32(base + 4);
    if ((rootId & 0x00FFFFFFu) != WLD_CHUNK_PHYSICS || rootSize > size - 8)
        return 0;

    memcpy(base, &rootId, 4);
    memcpy(base + 4, &rootSize, 4);

    {
        unsigned long n = 1;
        return wld_swap_children(base + 8, rootSize, &n) ? n : 0;
    }
}

// Returns the number of chunks converted. 0 means the buffer is not to be
// walked: not a .wld, or a chunk sized or aligned past its end.
unsigned long port_wld_swap(void* data, unsigned long size)
{
    uint8_t* base = (uint8_t*)data;
    uint32_t rootId, rootSize;
    uint8_t* p;
    uint8_t* end;
    unsigned long n = 0;

    if (data == NULL || size < 8)
        return 0;

    rootId = port_be32(base);
    rootSize = port_be32(base + 4);
    if ((rootId & 0x00FFFFFFu) != WLD_CHUNK_ROOT || rootSize > size - 8)
        return 0;

    memcpy(base, &rootId, 4);
    memcpy(base + 4, &rootSize, 4);
    n++;

    p = base + 8;
    end = p + rootSize;
    while (p + 8 <= end)
    {
        uint32_t id = port_be32(p);
        uint32_t chunkSize = port_be32(p + 4);

        // A size that runs past the end means this is no longer a chunk
        // header; the game's own walk would read past the buffer, so refuse.
        if (chunkSize > (uint32_t)(end - p - 8) || ((id & 0x7F000000u) >> 24) > 16)
            return 0;

        memcpy(p, &id, 4);
        memcpy(p + 4, &chunkSize, 4);
        n++;

        // The physics block is the one child that is itself a container.
        if ((id & 0x00FFFFFFu) == WLD_CHUNK_PHYSICS)
        {
            if (!wld_swap_children(p + 8, chunkSize, &n))
                return 0;
        }
        else
        {
            unsigned long len;
            uint8_t* data = port_chunk_payload(p, id, chunkSize, &len);
            if (data == NULL)
                return 0;
            wld_swap_payload(data, (uint32_t)len, id & 0x00FFFFFFu);
        }
        p += 8 + chunkSize;
    }

    return n;
}
