// The chunk converters against files shaped to walk them off the buffer, and the well-formed
// cases that keep the fix from being "convert nothing".

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port/endian.h"

unsigned long port_cam_swap(void* data, unsigned long size);
unsigned long port_wld_swap(void* data, unsigned long size);
unsigned long port_phys_swap(void* data, unsigned long size);
unsigned long port_skin_swap(void* outerChunk);
unsigned long port_bmd_swap_headers(void* data, unsigned long size);

static int failures;

static void check(int ok, const char* what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        failures++;
}

static void put32(unsigned char* p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static uint32_t host32(const unsigned char* p)
{
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

// Exactly `size` bytes, so a sanitiser sees the first byte past the end.
static unsigned char* exact(size_t size)
{
    unsigned char* p = malloc(size);
    memset(p, 0, size);
    return p;
}

// A buffer on a 32-byte boundary, as nlLoadEntireFile hands the game. Aligned by hand: the
// Windows C runtime has no aligned_alloc.
static unsigned char* s_aligned_block;
static unsigned char* aligned64(void)
{
    unsigned char* p;
    free(s_aligned_block);
    s_aligned_block = malloc(64 + 32);
    p = (unsigned char*)(((uintptr_t)s_aligned_block + 31) & ~(uintptr_t)31);
    memset(p, 0, 64);
    return p;
}

int main(void)
{
    {
        unsigned char chunk[24];
        unsigned long len = 99;
        memset(chunk, 0, sizeof chunk);
        check(port_chunk_payload(chunk, 0x15508, 16, &len) == chunk + 8 && len == 16,
              "helper: an unaligned chunk's payload starts after the header");
        check(port_chunk_payload(chunk, (17u << 24) | 0x15508, 16, &len) == NULL,
              "helper: an alignment no asset uses is refused");
        check(port_chunk_payload(chunk, (16u << 24) | 0x15508, 8, &len) == NULL,
              "helper: an alignment past the chunk's end is refused");
    }

    {
        unsigned char* buf = exact(24);
        put32(buf, 0x15501);
        put32(buf + 4, 16);
        put32(buf + 8, (16u << 24) | 0x15508);
        put32(buf + 12, 8);
        put32(buf + 16, 1);
        check(port_cam_swap(buf, 24) == 0, "cam: a chunk aligned past its end refuses the file");
        check(buf[16] == 0 && buf[19] == 1, "cam: ...and its payload is left alone");
        free(buf);
    }

    {
        unsigned char* buf = exact(16);
        put32(buf, 0x15501);
        put32(buf + 4, 8);
        put32(buf + 8, 0x15508);
        put32(buf + 12, 0);
        check(port_cam_swap(buf, 16) == 0, "cam: an empty key-count chunk refuses the file");
        free(buf);
    }

    {
        unsigned char* buf = exact(48);
        put32(buf, 0x15501);
        put32(buf + 4, 32);
        put32(buf + 8, 0x15508);
        put32(buf + 12, 4);
        put32(buf + 16, 2);
        put32(buf + 20, 0x15509);
        put32(buf + 24, 12);
        check(port_cam_swap(buf, 40) == 0, "cam: a key array shorter than the count refuses the file");
        put32(buf, 0x15501);
        put32(buf + 4, 32);
        put32(buf + 8, 0x15508);
        put32(buf + 12, 4);
        put32(buf + 16, 1);
        put32(buf + 20, 0x15509);
        put32(buf + 24, 12);
        check(port_cam_swap(buf, 40) == 3, "cam: a key array holding the count's keys is accepted");
        free(buf);
    }

    {
        unsigned char* buf = exact(32);
        put32(buf, 0x15501);
        put32(buf + 4, 24);
        put32(buf + 8, 0x15509);
        put32(buf + 12, 16);
        check(port_cam_swap(buf, 32) == 0, "cam: a file with no key count refuses");
        free(buf);
    }

    {
        unsigned char* buf = exact(24);
        put32(buf, 0x15501);
        put32(buf + 4, 16);
        put32(buf + 8, 0x15508);
        put32(buf + 12, 4000);
        check(port_cam_swap(buf, 24) == 0, "cam: a chunk sized past the buffer refuses the file");
        free(buf);
    }

    {
        unsigned char* buf = exact(24);
        put32(buf, 0x15501);
        put32(buf + 4, 16);
        put32(buf + 8, (127u << 24) | 0x15508);
        put32(buf + 12, 8);
        put32(buf + 16, 1);
        check(port_cam_swap(buf, 24) == 0, "cam: a 127 alignment exponent refuses the file");
        check(buf[19] == 1, "cam: ...and converts nothing");
        free(buf);
    }

    {
        unsigned char* buf = aligned64();
        unsigned i;
        put32(buf, 0x15501);
        put32(buf + 4, 48);
        put32(buf + 8, 0x15508);
        put32(buf + 12, 8);
        put32(buf + 16, 2);
        // The position chunk's header ends at 32, 8-aligned already, so nothing is padding.
        put32(buf + 24, (3u << 24) | 0x15509);
        put32(buf + 28, 24);
        for (i = 0; i < 6; i++)
            put32(buf + 32 + i * 4, i + 1);
        check(port_cam_swap(buf, 56) == 3, "cam: a well-formed file converts its chunks");
        check(host32(buf + 32) == 1 && host32(buf + 52) == 6,
              "cam: the aligned payload is in host order to the chunk's end");
        
    }

    {
        unsigned char* buf = exact(24);
        put32(buf, 0x19000);
        put32(buf + 4, 16);
        put32(buf + 8, (16u << 24) | 0x19003);
        put32(buf + 12, 8);
        put32(buf + 16, 1);
        check(port_wld_swap(buf, 24) == 0, "wld: a record aligned past its end refuses the file");
        check(buf[19] == 1, "wld: ...and its payload is left alone");
        free(buf);
    }

    {
        unsigned char* buf = exact(32);
        put32(buf, 0x19000);
        put32(buf + 4, 24);
        put32(buf + 8, 0x1D000);
        put32(buf + 12, 16);
        put32(buf + 16, (16u << 24) | 0x1D001);
        put32(buf + 20, 8);
        check(port_wld_swap(buf, 32) == 0, "wld: a bad chunk inside the physics block refuses the file");
        put32(buf, 0x19000);
        put32(buf + 4, 24);
        put32(buf + 8, 0x1D000);
        put32(buf + 12, 16);
        put32(buf + 16, (16u << 24) | 0x1D001);
        put32(buf + 20, 8);
        check(port_phys_swap(buf + 8, 24) == 0, "phys: the same block as a file of its own is refused");
        free(buf);
    }

    {
        unsigned char* buf = exact(32);
        put32(buf, 0x8001B000u);
        put32(buf + 4, 24);
        put32(buf + 8, 0x8001B008u);
        put32(buf + 12, 16);
        put32(buf + 16, (127u << 24) | 0x1B00Eu);
        put32(buf + 20, 8);
        check(port_bmd_swap_headers(buf, 32) == 0, "bmd: an alignment exponent the loader cannot shift by refuses the tree");
        put32(buf, 0x8001B000u);
        put32(buf + 4, 24);
        put32(buf + 8, 0x8001B008u);
        put32(buf + 12, 16);
        put32(buf + 16, (3u << 24) | 0x1B00Eu);
        put32(buf + 20, 8);
        check(port_bmd_swap_headers(buf, 32) == 3, "bmd: an 8-byte alignment is accepted");
        free(buf);
    }

    {
        unsigned char* buf = aligned64();
        put32(buf, 0x8001B000u);
        put32(buf + 4, 20);
        put32(buf + 8, (5u << 24) | 0x1B004u);
        put32(buf + 12, 12);
        check(port_bmd_swap_headers(buf, 28) == 0, "bmd: an aligned payload past the chunk's end refuses the tree");
        put32(buf, 0x8001B000u);
        put32(buf + 4, 40);
        put32(buf + 8, (5u << 24) | 0x1B004u);
        put32(buf + 12, 32);
        check(port_bmd_swap_headers(buf, 48) == 2, "bmd: an aligned payload inside the chunk is accepted");
    }

    {
        unsigned char* buf = exact(40);
        put32(buf, 0x15501);
        put32(buf + 4, 32);
        put32(buf + 8, 0x15508);
        put32(buf + 12, 4);
        put32(buf + 16, 0x40000000u);
        put32(buf + 20, 0x15509);
        put32(buf + 24, 12);
        check(port_cam_swap(buf, 40) == 0, "cam: a count whose byte product would wrap still refuses");
        free(buf);
    }

    {
        unsigned char* buf = exact(24);
        put32(buf, 0x8001B100u);
        put32(buf + 4, 16);
        put32(buf + 8, 0x1B002);
        put32(buf + 12, 4000);
        check(port_bmd_swap_headers(buf, 24) == 0, "bmd: a chunk sized past its container refuses the tree");
        put32(buf, 0x8001B100u);
        put32(buf + 4, 16);
        put32(buf + 8, 0x1B002);
        put32(buf + 12, 8);
        check(port_bmd_swap_headers(buf, 24) == 2, "bmd: a well-formed tree reports its chunk count");
        free(buf);
    }

    {
        unsigned char* buf = aligned64();
        put32(buf, 0x19000);
        put32(buf + 4, 24);
        put32(buf + 8, (3u << 24) | 0x19001);
        put32(buf + 12, 16);
        put32(buf + 16, 7);
        check(port_wld_swap(buf, 32) == 2, "wld: a well-formed file converts its chunks");
        check(host32(buf + 16) == 7, "wld: the aligned count is in host order");
        
    }

    // The SKIN chunk arrives with its headers already in host order; the model pass did those.
    {
        unsigned char* buf = exact(24);
        uint32_t v;
        v = 0x1B008;                      memcpy(buf, &v, 4);
        v = 16;                           memcpy(buf + 4, &v, 4);
        v = (16u << 24) | 0x1B00E;        memcpy(buf + 8, &v, 4);
        v = 8;                            memcpy(buf + 12, &v, 4);
        buf[16] = 0; buf[17] = 1;
        check(port_skin_swap(buf) == 0, "skin: a child aligned past its end refuses the chunk");
        check(buf[16] == 0 && buf[17] == 1, "skin: ...and its payload is left alone");
        free(buf);
    }

    {
        unsigned char* buf = aligned64();
        uint32_t v;
        v = 0x1B008;                      memcpy(buf, &v, 4);
        v = 32;                           memcpy(buf + 4, &v, 4);
        v = (3u << 24) | 0x1B00E;         memcpy(buf + 8, &v, 4);
        v = 24;                           memcpy(buf + 12, &v, 4);
        buf[16] = 0; buf[17] = 1;
        buf[38] = 0; buf[39] = 2;
        check(port_skin_swap(buf) == 1, "skin: a well-formed chunk is converted");
        check(buf[16] == 1 && buf[17] == 0 && buf[38] == 2 && buf[39] == 0,
              "skin: the pairs are in host order to the chunk's end");
        
    }

    if (failures)
    {
        printf("\n%d check(s) failed\n", failures);
        return 1;
    }
    printf("\nall checks passed\n");
    return 0;
}
