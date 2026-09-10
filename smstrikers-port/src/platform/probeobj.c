// Object probe for STRIKERS_PROBE_OBJ; inert until something registers.

#include "port/probeobj.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROBEOBJ_MAX 8

struct probeobj_entry
{
    const unsigned char* base;
    const unsigned char* end;
    unsigned stride;
    const void* keys[64];
    int numKeys;
    char name[128];
};

static struct probeobj_entry g_entries[PROBEOBJ_MAX];
static int g_numEntries;

void PortProbeObjRegister(const void* base, int count, unsigned stride,
                          const char* name)
{
    int i;
    if (base == 0 || count <= 0 || stride == 0 || name == 0)
        return;
    for (i = 0; i < g_numEntries; i++)
    {
        if (strncmp(g_entries[i].name, name, sizeof(g_entries[i].name)) == 0)
            break;
    }
    if (i == g_numEntries)
    {
        if (g_numEntries == PROBEOBJ_MAX)
            return;
        g_numEntries++;
    }
    g_entries[i].base = (const unsigned char*)base;
    g_entries[i].end = (const unsigned char*)base + (unsigned)count * stride;
    g_entries[i].stride = stride;
    g_entries[i].numKeys = 0;
    strncpy(g_entries[i].name, name, sizeof(g_entries[i].name) - 1);
    g_entries[i].name[sizeof(g_entries[i].name) - 1] = '\0';
}

void PortProbeObjRegisterKey(const char* name, int index, const void* key)
{
    int i;
    if (name == 0 || key == 0 || index < 0 || index >= 64)
        return;
    for (i = 0; i < g_numEntries; i++)
    {
        if (strncmp(g_entries[i].name, name, sizeof(g_entries[i].name)) == 0)
        {
            g_entries[i].keys[index] = key;
            if (index >= g_entries[i].numKeys)
                g_entries[i].numKeys = index + 1;
            return;
        }
    }
}

int PortProbeObjIndex(const void* p, const void* key, const char** nameOut)
{
    const unsigned char* q = (const unsigned char*)p;
    int i, k;
    for (i = 0; i < g_numEntries; i++)
    {
        if (q >= g_entries[i].base && q < g_entries[i].end)
        {
            if (nameOut)
                *nameOut = g_entries[i].name;
            return (int)((unsigned long)(q - g_entries[i].base)
                         / g_entries[i].stride);
        }
        if (key != 0)
        {
            for (k = 0; k < g_entries[i].numKeys; k++)
            {
                if (g_entries[i].keys[k] == key)
                {
                    if (nameOut)
                        *nameOut = g_entries[i].name;
                    return k;
                }
            }
        }
    }
    return -1;
}

int PortProbeObjShouldLog(unsigned long frame)
{
    static unsigned long lastFrame = (unsigned long)-1;
    static unsigned long nDistinct;
    if (frame != lastFrame)
    {
        lastFrame = frame;
        nDistinct++;
    }
    return nDistinct <= 3 || (nDistinct % 500) == 0;
}

// CMPR to PPM: 8x8 tiles of four 4x4 sub-blocks, RGB565 c0 and c1 then 32 bits of 2-bit indices.
static void cmpr_colour(unsigned v, unsigned char* rgb)
{
    rgb[0] = (unsigned char)(((v >> 11) & 0x1F) * 255 / 31);
    rgb[1] = (unsigned char)(((v >> 5) & 0x3F) * 255 / 63);
    rgb[2] = (unsigned char)((v & 0x1F) * 255 / 31);
}

void PortProbeDumpCMPR(const char* tag, int index, const void* data,
                       unsigned w, unsigned h)
{
    const char* dir = getenv("STRIKERS_PROBE_TEXDUMP");
    if (dir == 0 || data == 0 || w == 0 || h == 0)
        return;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s_%d_%ux%u.ppm", dir, tag, index, w, h);
    FILE* f = fopen(path, "wb");
    if (f == 0)
        return;
    unsigned char* img = (unsigned char*)malloc((size_t)w * h * 3);
    if (img == 0)
    {
        fclose(f);
        return;
    }
    const unsigned char* src = (const unsigned char*)data;
    unsigned tilesAcross = w / 8;
    for (unsigned ty = 0; ty < h / 8; ty++)
        for (unsigned tx = 0; tx < tilesAcross; tx++)
            for (unsigned sb = 0; sb < 4; sb++)
            {
                const unsigned char* b =
                    src + ((ty * tilesAcross + tx) * 4 + sb) * 8;
                unsigned c0 = ((unsigned)b[0] << 8) | b[1];
                unsigned c1 = ((unsigned)b[2] << 8) | b[3];
                unsigned char pal[4][3];
                cmpr_colour(c0, pal[0]);
                cmpr_colour(c1, pal[1]);
                if (c0 > c1)
                    for (int ch = 0; ch < 3; ch++)
                    {
                        pal[2][ch] = (unsigned char)((2 * pal[0][ch] + pal[1][ch]) / 3);
                        pal[3][ch] = (unsigned char)((pal[0][ch] + 2 * pal[1][ch]) / 3);
                    }
                else
                    for (int ch = 0; ch < 3; ch++)
                    {
                        pal[2][ch] = (unsigned char)((pal[0][ch] + pal[1][ch]) / 2);
                        pal[3][ch] = 0;
                    }
                unsigned bx = tx * 8 + (sb & 1) * 4;
                unsigned by = ty * 8 + (sb >> 1) * 4;
                for (unsigned yy = 0; yy < 4; yy++)
                {
                    unsigned char row = b[4 + yy];
                    for (unsigned xx = 0; xx < 4; xx++)
                    {
                        unsigned idx = (row >> (6 - 2 * xx)) & 3;
                        unsigned char* d =
                            img + ((by + yy) * w + bx + xx) * 3;
                        d[0] = pal[idx][0];
                        d[1] = pal[idx][1];
                        d[2] = pal[idx][2];
                    }
                }
            }
    fprintf(f, "P6\n%u %u\n255\n", w, h);
    fwrite(img, 1, (size_t)w * h * 3, f);
    free(img);
    fclose(f);
}

// Write a GC-swizzled RGB565 image