#ifndef PORT_ENDIAN_H
#define PORT_ENDIAN_H
// Byte-swap helpers for GameCube assets. Functions over a pointer into the buffer rather than a
// BE<T> field wrapper, so sites convert one at a time.

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint16_t port_be16(const void* p)
{
    uint16_t v;
    memcpy(&v, p, sizeof v);      // memcpy, not a cast: the source may be
    return __builtin_bswap16(v);  // unaligned within a packed asset
}

static inline uint32_t port_be32(const void* p)
{
    uint32_t v;
    memcpy(&v, p, sizeof v);
    return __builtin_bswap32(v);
}

static inline float port_bef32(const void* p)
{
    uint32_t v = port_be32(p);
    float f;
    memcpy(&f, &v, sizeof f);
    return f;
}

static inline void port_be16_array(void* p, unsigned long count)
{
    unsigned char* b = (unsigned char*)p;
    for (unsigned long i = 0; i < count; i++, b += 2)
    {
        uint16_t v = port_be16(b);
        memcpy(b, &v, sizeof v);
    }
}

static inline void port_be32_array(void* p, unsigned long count)
{
    unsigned char* b = (unsigned char*)p;
    for (unsigned long i = 0; i < count; i++, b += 4)
    {
        uint32_t v = port_be32(b);
        memcpy(b, &v, sizeof v);
    }
}

// An nlChunk's payload and the bytes of it inside the chunk: bits 24-30 of the id are an alignment
// exponent (nlChunk::GetAlignedData), the padding it costs is inside the chunk's size, and a chunk
// aligned past its own end, or with an exponent no asset uses (past 64 KB), yields NULL.
static inline unsigned char* port_chunk_payload(unsigned char* chunk, uint32_t id,
                                                uint32_t size, unsigned long* len)
{
    uintptr_t addr = (uintptr_t)(chunk + 8);
    uintptr_t end = addr + size;
    unsigned shift = (id & 0x7F000000u) >> 24;

    if (shift != 0)
    {
        uintptr_t alignment;
        if (shift > 16)
            return NULL;
        alignment = (uintptr_t)1 << shift;
        addr = (addr + alignment - 1) & ~(alignment - 1);
    }
    if (addr > end)
        return NULL;
    *len = (unsigned long)(end - addr);
    return (unsigned char*)addr;
}

#ifdef __cplusplus
}
#endif

#endif // PORT_ENDIAN_H
