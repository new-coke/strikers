#include "NL/gl/glFont.h"
#include "NL/gl/glState.h"
#include "NL/nlPrint.h"
#include "NL/nlString.h"
#include "NL/glx/glxFont.h"

#include "NL/gl/glDraw2.h"

#include "font_data.h"

static glPoly2 g_poly[128];
static float font_z;
static void* handle;
static bool bInsideBegin;
static bool bDrop;
static bool bEnabled;
static bool bVirtualCoords;

/**
 * Offset/Address/Size: 0x0 | 0x801D8398 | size: 0xE8
 */
int glFontPrintf(eGLView view, int x, int y, const nlColour& col, const char* format, ...)
{
    char string[0x80];
    va_list args;

    if (bEnabled == false)
    {
        return false;
    }

    va_start(args, format);
    nlVSNPrintf(string, 0x80, format, args);
    va_end(args);

    return glFontPrint(view, x, y, col, string);
}

/**
 * Offset/Address/Size: 0xE8 | 0x801D8480 | size: 0xF4
 */
int glFontPrintf(eGLView view, int x, int y, const char* format, ...)
{
    char string[0x80];
    va_list args;

    if (bEnabled == false)
    {
        return false;
    }

    va_start(args, format);
    nlVSNPrintf(string, 0x80, format, args);
    va_end(args);

    nlColour col;
    col.c[0] = 255;
    col.c[1] = 255;
    col.c[2] = 255;
    col.c[3] = 255;

    return glFontPrint(view, x, y, col, string);
}

static inline void _Putchar(glPoly2& poly, float sx, float sy, int charIndex, const nlColour& colour)
{
    float s = (float)((charIndex % 8) * 8);
    float t = (float)((charIndex / 8) * 8);
    poly.m_uv[0].x = s / 64.0f;
    poly.m_uv[0].y = t / 128.0f;
    poly.m_uv[1].x = s / 64.0f;
    poly.m_uv[1].y = (t + 8.0f) / 128.0f;
    poly.m_uv[2].x = (s + 8.0f) / 64.0f;
    poly.m_uv[2].y = (t + 8.0f) / 128.0f;
    poly.m_uv[3].x = (s + 8.0f) / 64.0f;
    poly.m_uv[3].y = t / 128.0f;
    poly.m_pos[0].x = sx;
    poly.m_pos[0].y = sy;
    poly.m_pos[1].x = sx;
    poly.m_pos[1].y = 10.0f + sy;
    poly.m_pos[2].x = 10.0f + sx;
    poly.m_pos[2].y = 10.0f + sy;
    poly.m_pos[3].x = 10.0f + sx;
    poly.m_pos[3].y = sy;
    poly.depth = font_z;
    poly.m_colour[0] = colour;
    poly.m_colour[1] = colour;
    poly.m_colour[2] = colour;
    poly.m_colour[3] = colour;
}

/**
 * Offset/Address/Size: 0x1DC | 0x801D8574 | size: 0x3EC
 */
int glFontPrint(eGLView view, int virtual_x, int virtual_y, const nlColour& colour, const char* str)
{
    if (nlStrLen(str) == 0)
    {
        return 0;
    }
    if (bEnabled == false)
    {
        return 0;
    }
    int screen_x = virtual_x;
    int screen_y = virtual_y;
    if ((u8)bVirtualCoords != 0)
    {
        screen_x = (screen_x * 10) + 40;
        screen_y = (screen_y * 11) + 32;
    }
    nlStrLen(str);
    int numChars = 0;
    int i;
    int j;
    glPoly2* pPoly = g_poly;
    const char* cp = str;
    while (*cp != '\0')
    {
        if ((*cp >= 0x20) && (*cp <= 0x7E))
        {
            _Putchar(*pPoly, screen_x, screen_y, *cp - 0x20, colour);
            pPoly++;
            numChars++;
        }
        else if (*cp == '\n')
        {
            screen_x = 30;
            screen_y += 11;
        }
        screen_x += 10;
        cp++;
    }
    if (bDrop != false)
    {
        pPoly = g_poly;
        for (i = 0; i < numChars; i++)
        {
            for (j = 0; j < 4; j++)
            {
                pPoly[i].m_colour[j].c[0] = 0;
                pPoly[i].m_colour[j].c[1] = 0;
                pPoly[i].m_colour[j].c[2] = 0;
                pPoly[i].m_colour[j].c[3] = 0xFF;
                pPoly[i].m_pos[j].x += 3.0f;
                pPoly[i].m_pos[j].y += 3.0f;
            }
            pPoly[i].depth += -0.001f;
        }
        glAttachPoly2(view, numChars, g_poly, 0, 0);
        float fz = font_z;
        for (i = 0; i < numChars; i++)
        {
            for (j = 0; j < 4; j++)
            {
                *(u32*)&pPoly[i].m_colour[j].c[0] = *(u32*)&colour.c[0];
                pPoly[i].m_pos[j].x -= 3.0f;
                pPoly[i].m_pos[j].y -= 3.0f;
            }
            pPoly[i].depth = fz;
        }
    }
    glAttachPoly2(view, numChars, g_poly, 0, 0);
    return numChars;
}

/**
 * Offset/Address/Size: 0x5C8 | 0x801D8960 | size: 0x18
 */
void glFontEnd()
{
    if (bEnabled != false)
    {
        bInsideBegin = false;
    }
}

/**
 * Offset/Address/Size: 0x5E0 | 0x801D8978 | size: 0x7C
 */
void glFontBegin(bool drop)
{
    if (bEnabled != false)
    {
        glSetDefaultState(0);
        glSetCurrentTexture((uintptr_t)handle, GLTT_Diffuse);
        glSetRasterState(GLS_AlphaTest, 1);
        glSetCurrentRasterState(glHandleizeRasterState());
        glSetTextureState(GLTS_DiffuseFilter, 1);
        glSetCurrentTextureState(glHandleizeTextureState());
        bDrop = drop;
        bInsideBegin = true;
    }
}

/**
 * Offset/Address/Size: 0x65C | 0x801D89F4 | size: 0x24
 */
void glFontVirtualPosToScreenCoordPos(float x, float y, float& outX, float& outY)
{
    outX = (10.0f * x) + 40.0f;
    outY = (11.0f * y) + 32.0f;
}

/**
 * Offset/Address/Size: 0x680 | 0x801D8A18 | size: 0x5C
 */
void gl_FontStartup()
{
    handle = (void*)glGetTexture("font/fixed8x8");
    glplatCreateFont(0x40, 0x80, _fontData, (uintptr_t)handle);
    bInsideBegin = 0;
    bEnabled = 1;
    bDrop = 0;
    bVirtualCoords = 1;
}
