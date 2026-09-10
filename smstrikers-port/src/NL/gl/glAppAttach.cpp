#include "NL/gl/glAppAttach.h"

class WorldDarkening
{
public:
    static WorldDarkening& Instance();
    /* 0x0 */ float mRate;
    /* 0x4 */ float mPos;
    /* 0x8 */ float mTo;
    /* 0xC */ bool mActive;
};

#include "NL/gl/glMemory.h"
#include "NL/gl/glState.h"

const u32 UnlitProgram = glGetProgram("3d unlit");
const u32 LitProgram = glGetProgram("3d pointlit");
const u32 LightTexture = glGetTexture("global/lightramp");
const u32 BlackTexture = glGetTexture("global/black");
const u32 WhiteTexture = glGetTexture("global/white");

static void* glapp_CoPlanarUserData;
static void* glapp_NoFogUserData;
static void* glapp_NoRasterizedAlphaUserData;
static void* glapp_OnePassFresnelUserData;

const u32 MovieTexture_Y = glGetTexture("movie");

const u32 MovieTexture_U = glGetTexture("movie_u");
const u32 MovieTexture_V = glGetTexture("movie_v");
const u32 MovieProgram = glGetProgram("2d movie");

const char* view_names[0x22] = {
    "ShadowTexture",       // @378
    "GrabTexture",         // @379
    "Skybox",              // @380
    "Shadowed",            // @381
    "Shadow0",             // @382
    "ShadowBlend0",        // @383
    "WorldShadowed",       // @384
    "Unshadowed",          // @385
    "BigBlackPolygon",     // @386
    "Warble",              // @387
    "WarbleBlend",         // @388
    "Characters",          // @389
    "CoPlanar0",           // @390
    "CoPlanar",            // @391
    "Shadow1",             // @392
    "ShadowBlend1",        // @393
    "UnsortedPerspective", // @394
    "UnsortedOrtho",       // @395
    "DepthOfField",        // @396
    "LingeringParticles",  // @397
    "Particles",           // @398
    "InvisiblePlane",      // @399
    "ElectricFence",       // @400
    "CameraSpace",         // @401
    "ScreenBlur",          // @402
    "ScreenBlur2",         // @403
    "ScreenGrab",          // @404
    "FrontEnd",            // @405
    "Transitions",         // @406
    "Transitions3D",       // @407
    "Anark3DBG",           // @408
    "Anark",               // @409
    "Anark3DFG",           // @410
    "Debug"                // @411
};

PlatTexture* ResolvedBlackTexture = (PlatTexture*)0xFFFFFFFF;
PlatTexture* ResolvedWhiteTexture = (PlatTexture*)0xFFFFFFFF;

/**
 * Offset/Address/Size: 0x0 | 0x80191948 | size: 0x41C
 */
void glplatAttachPacket(eGLView view, unsigned long layer, const glModelPacket* pPacket)
{
    extern void gl_ViewAttachPacket(eGLView, unsigned long, const glModelPacket*);

    unsigned long newLayer = layer;
    eGLView newView = view;
    s32 desiredMode;
    glModelPacket* p;

    if ((s32)newView >= 0x13)
    {
        unsigned long l = newLayer;
        if (l >= 7)
            l = 6;
        gl_ViewAttachPacket(newView, l, pPacket);
        return;
    }

    if (glUserHasType((eGLUserData)5, pPacket))
    {
        switch ((s32)glGetRasterState(pPacket->state.raster, (eGLState)5))
        {
        case 0:
            desiredMode = 1;
            break;
        case 2:
            desiredMode = 3;
            break;
        default:
            desiredMode = 0;
            break;
        }

        if (desiredMode != 0)
        {
            p = glModelPacketDup(pPacket, true);
            glSetRasterState(p->state.raster, (eGLState)5, desiredMode);
            pPacket = p;
        }

        if ((pPacket->state.texconfig == 0x39) || (pPacket->state.texconfig == 0x31))
        {
            glModelPacket* p0 = glModelPacketDup(pPacket, true);
            p0->state.texconfig &= ~0x18;
            if (glUserHasType((eGLUserData)6, p0))
            {
                glUserDetach((eGLUserData)6, p0);
            }
            pPacket = p0;
        }
    }

    if (pPacket->state.texconfig == 0x33)
    {
        glModelPacket* p0 = glModelPacketDup(pPacket, true);
        glModelPacket* p1 = glModelPacketDup(pPacket, true);

        p0->state.texconfig &= ~0x10;
        glUserDetach((eGLUserData)3, p0);

        {
            unsigned long l = newLayer;
            if (l >= 7)
                l = 6;
            gl_ViewAttachPacket(newView, l, p0);
        }

        if (glUserHasType((eGLUserData)1, p1))
        {
            glUserDetach((eGLUserData)1, p1);
        }

        p1->state.texconfig &= ~0x02;
        p1->state.texconfig &= ~0x20;
        p1->state.texture[0] = (uintptr_t)ResolvedBlackTexture;
        glSetRasterState(p1->state.raster, (eGLState)5, 2);

        pPacket = p1;
        newLayer++;
    }

    if ((pPacket->state.texconfig == 0x17) || (pPacket->state.texconfig == 0x37))
    {
        glModelPacket* p0 = glModelPacketDup(pPacket, true);
        glModelPacket* p1 = glModelPacketDup(pPacket, true);

        if (glUserHasType((eGLUserData)1, p0))
        {
            glUserDetach((eGLUserData)1, p0);
        }

        p0->state.texconfig = 0x11;
        p0->state.texture[0] = (uintptr_t)ResolvedBlackTexture;
        glSetRasterState(p0->state.raster, (eGLState)5, 2);

        p1->state.texconfig &= ~0x10;
        glUserDetach((eGLUserData)3, p1);

        {
            unsigned long l = newLayer;
            if (l >= 7)
                l = 6;
            gl_ViewAttachPacket(newView, l, p1);
        }

        pPacket = p0;
        newLayer++;
    }

    switch ((s32)newView)
    {
    case 0xB:
        if (glGetRasterState(pPacket->state.raster, (eGLState)5) != 0)
        {
            newLayer++;
        }
        break;
    case 3:
    case 6:
    case 7:
        if (glGetRasterState(pPacket->state.raster, (eGLState)5) != 0)
        {
            if (WorldDarkening::Instance().mActive)
            {
                newView = (eGLView)7;
                newLayer++;
            }
            else
            {
                newView = (eGLView)11;
                newLayer += 2;
            }
        }
        break;
    }

    if (glUserHasType((eGLUserData)6, pPacket))
    {
        glModelPacket* p0 = glModelPacketDup(pPacket, true);
        p0->state.texture[4] = (uintptr_t)ResolvedBlackTexture;
        p0->state.texture[3] = (uintptr_t)ResolvedBlackTexture;
        glUserDetach((eGLUserData)6, p0);
        glUserAttach(glapp_NoRasterizedAlphaUserData, p0, false);

        {
            unsigned long l = newLayer;
            if (l >= 7)
                l = 6;
            gl_ViewAttachPacket(newView, l, p0);
        }

        {
            glModelPacket* p1 = glModelPacketDup(pPacket, true);
            p1->state.texture[0] = (uintptr_t)ResolvedBlackTexture;
            glSetRasterState(p1->state.raster, (eGLState)5, 3);
            glUserDetach((eGLUserData)1, p1);

            if (!glUserHasType((eGLUserData)15, p1))
            {
                glUserAttach(glapp_NoFogUserData, p1, false);
            }

            pPacket = p1;
        }

        newLayer++;
    }

    {
        unsigned long l = newLayer;
        if (l >= 7)
            l = 6;
        gl_ViewAttachPacket(newView, l, pPacket);
    }
}

inline glModelStream* getDetail(glModelStream* start, const glModelStream* end)
{
    while (start < end)
    {
        if ((s32)start->id == 3)
        {
            return start;
        }
        start++;
    }
    return NULL;
}

/**
 * Offset/Address/Size: 0x41C | 0x80191D64 | size: 0x210
 */
glModelPacket* glplatModifyPacket(eGLView view, const glModelPacket* pPacket)
{
    glModelPacket* pFinal = (glModelPacket*)pPacket;
    glModelPacket* p;

    if ((((s32)view == 0xC) || ((s32)view == 0xD)) && ((u32)pPacket->indexBuffer != 0U) && (glUserHasType((eGLUserData)0x10, pPacket) == 0))
    {
        pFinal = glModelPacketDup(pPacket, true);
        glUserAttach(glapp_CoPlanarUserData, pFinal, 0);
        glSetRasterState(pFinal->state.raster, (eGLState)2, 3U);
        glSetRasterState(pFinal->state.raster, (eGLState)0, 1U);
        glSetRasterState(pFinal->state.raster, (eGLState)1, 1U);
    }

    if ((u32)pFinal->state.texture[0] == (u32)MovieTexture_Y)
    {
        p = glModelPacketDup(pFinal, true);
        glSetRasterState(p->state.raster, (eGLState)3, 0U);
        glSetRasterState(p->state.raster, (eGLState)5, 0U);
        p->state.program = MovieProgram;
        p->state.texture[0] = MovieTexture_Y;
        p->state.texture[1] = MovieTexture_U;
        p->state.texture[2] = MovieTexture_V;
        p->state.texconfig = 7;

        glSetTextureState(p->state.texturestate, (eGLTextureState)0, 3U);
        glSetTextureState(p->state.texturestate, (eGLTextureState)1, 3U);
        glSetTextureState(p->state.texturestate, (eGLTextureState)2, 3U);
        glSetTextureState(p->state.texturestate, (eGLTextureState)6, 0U);
        glSetTextureState(p->state.texturestate, (eGLTextureState)7, 0U);
        glSetTextureState(p->state.texturestate, (eGLTextureState)8, 0U);

        u32 numOldStreams = p->numStreams;
        u32 numNewStreams = numOldStreams + 1;
        glModelStream* streams = (glModelStream*)glFrameAlloc(numNewStreams * sizeof(glModelStream), (eGLMemory)0);
        memcpy(streams, pFinal->streams, numOldStreams * sizeof(glModelStream));

        glModelStream* detail = getDetail(pFinal->streams, &pFinal->streams[pFinal->numStreams]);
        glModelStream* newStream = &streams[numOldStreams];
        memcpy(newStream, detail, sizeof(glModelStream));
        pFinal = p;
        newStream->id = 4;
        p->streams = streams;
        p->numStreams = numNewStreams;
    }

    return pFinal;
}

/**
 * Offset/Address/Size: 0x62C | 0x80191F74 | size: 0x94
 */
void glAppStartup()
{
    glapp_CoPlanarUserData = glUserAlloc((eGLUserData)0x10, 0U, 1);
    glapp_NoFogUserData = glUserAlloc((eGLUserData)0xF, 0U, 1);
    glapp_NoRasterizedAlphaUserData = glUserAlloc((eGLUserData)0xD, 0U, 1);
    glapp_OnePassFresnelUserData = glUserAlloc((eGLUserData)0x11, 0U, 1);
    ResolvedBlackTexture = glx_GetTex((unsigned long)BlackTexture, 1, 1);
    ResolvedWhiteTexture = glx_GetTex((u32)WhiteTexture, 1, 1);
}

/**
 * Offset/Address/Size: 0x6C0 | 0x80192008 | size: 0x8
 */
void* glAppGetOnePassFresnelUserData()
{
    return glapp_OnePassFresnelUserData;
}

/**
 * Offset/Address/Size: 0x6C8 | 0x80192010 | size: 0x8
 */
void* glAppGetNoFogUserData()
{
    return glapp_NoFogUserData;
}

/**
 * Offset/Address/Size: 0x6D0 | 0x80192018 | size: 0x8
 */
void* glAppGetCoPlanarUserData()
{
    return glapp_CoPlanarUserData;
}

/**
 * Offset/Address/Size: 0x6D8 | 0x80192020 | size: 0x30
 */
char* gld_ViewName(int view)
{
    if ((view < 0) || (view > 0x22))
    {
        return "[bad view]";
    }
    return (char*)view_names[view];
}
