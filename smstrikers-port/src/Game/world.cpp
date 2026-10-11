#include <stdio.h>
#include <stdlib.h>
#include "port/input.h"
#include "port/benchmark.h"
#include "NL/nlString.h"
#include "NL/vmath.h"
#include "Game/World.h"
#include "port/endian.h"
#include "Game/LightObject.h"
#include "Game/Render/FlareHandler.h"
#include "Game/Camera/CameraMan.h"
#include "Game/Camera/MatrixEffectCam.h"

#include "string.h"

#include "NL/nlPrint.h"
#include "NL/nlDebug.h"
#include "NL/gl/gl.h"
#include "NL/gl/glModel.h"
#include "NL/gl/glState.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glFont.h"
#include "NL/gl/glView.h"
#include "NL/gl/glUserData.h"
#include "NL/gl/glTexture.h"
#include "NL/glx/glxTexture.h"
#include "Game/Sys/debug.h"
#include "Game/Game.h"
#include "Game/Ball.h"
#include "Game/Drawable/DrawableObj.h"
#include "Game/Drawable/DrawableModel.h"
#include "Game/Drawable/DrawableSkinModel.h"
#include "Game/Drawable/DrawableTmModel.h"
#include "Game/GL/GLInventory.h"
#include "Game/Effects/EmissionManager.h"

#include "NL/gl/glLightUserData.h"
#include "NL/nlFile.h"
#include "Game/SAnim.h"
#include "Game/Physics/CharacterPhysicsElement.h"
#include "ctype_api.h"
#include "Game/Drawable/DrawableCharacter.h"
#if defined(PORT_VITA)
#include <aurora_vita_backend.hpp>
#include <vector>
#endif

// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
unsigned char g_bClipToFrustum = 1;
u32 World::m_uCurrentFrameCount = 0xFFFFFFFF;
static float g_fTransAdjustOccluded = 1.0f;
static float g_fTransAdjustNotOccluded = 0.125f;
static float g_fExponentScale = 128.0f;
static float g_fExponentBase = 8.0f;

// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
unsigned char g_bDrawBoundingSphere;
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
unsigned char g_bFreezeFrustum;
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
unsigned char g_bDrawCullingInfo;
static unsigned char g_bDebugEqualsSide;
static unsigned char g_bDebugEqualsEnd;
static unsigned char g_bDrawFrustum;
static const unsigned long WhiteTexture = glGetTexture("global/white50percent");
static unsigned long BallModelID = nlStringHash("gameplay/ball");
static unsigned long HammerModelID = nlStringHash("gameplay/hammer");
unsigned long SpecificModelID = nlStringLowerHash("The_Palace/Pod_Metal_42");
static float g_fTransMinimum = 0.0f;
bool World::sbIsHyperShootToScoreRenderingEnabled;
bool World::sbShowPositiveXNetDuringHyperStrike;
bool World::sbStadiumRenderingDisabled;
bool World::sbSkyboxRenderingDisabled;
static unsigned char g_bFreezeSideCam;
static unsigned char g_bFreezeEndCam;
static unsigned char sbDontRenderObjectsThatCantBeTransparent;
static unsigned char sbRenderObjectsWithVisibilityFlag;
static unsigned char sbRenderSideAlwaysTransObjects;
static unsigned char sbAllObjectsCanBeTransparent;
static unsigned char sbOnlyRenderSpecificObject;
static unsigned char sbPretendWereNotInGameplayCam;

static LightObject fxLightObjects[4];

/**
 * Offset/Address/Size: 0x3DB4 | 0x80198A78 | size: 0x19C
 */
World::World(const char* szWorldName)
    : m_pWorldAnimManager(NULL)
    , m_Locked(false)
    , m_pModels(NULL)
    , m_uNumModels(0)
    , m_animControllerList(0)
    , m_pLightData(NULL)
    , m_pPlayerNISLightData(NULL)
    , m_pIntensityPerm(NULL)
    , m_pIntensityData(NULL)
    , m_pSTSIntensity(NULL)
    , m_pPhysicsData(NULL)
    , m_pShadowLight(NULL)
{
    m_WorldNameLength = nlStrLen<char>(szWorldName);
    nlStrNCpy<char>(m_WorldNamePrefix, szWorldName, MAX_NAME_LENGTH);

    m_WorldNamePrefix[m_WorldNameLength++] = '/';

    m_pWorldAnimManager = new (nlMalloc(sizeof(WorldAnimManager), 8, false)) WorldAnimManager();

    m_LightRampTexA = glGetTexture("global/lightramp");
    m_LightRampTexB = m_LightRampTexA;

    m_PlayerLightRampTex = glGetTexture("global/playerlightramp");
    if (glTextureLoad(m_PlayerLightRampTex) == false)
    {
        m_PlayerLightRampTex = m_LightRampTexA;
    }

    m_GlobalLightRampSTSTex = m_LightRampTexA;
}

/**
 * Offset/Address/Size: 0x37D4 | 0x80198498 | size: 0x580
 */
World::~World()
{
    void* iterator;

    {
        typedef nlAVLTreeIterator<unsigned long, DrawableObject*, DefaultKeyCompare<unsigned long> >
            DrawableIterator;
        iterator = NewAdapter<DrawableIterator>().Allocate();
        iterator = new (iterator) DrawableIterator(m_drawableMap);
        while (((DrawableIterator*)iterator)->IsValid())
        {
            delete ((DrawableIterator*)iterator)->Current()->value;
            ((DrawableIterator*)iterator)->Next();
        }
        m_drawableMap.Clear();
        m_hyperSTSDrawableMap.Clear();
        delete (DrawableIterator*)iterator;
    }

    {
        typedef nlAVLTreeIterator<unsigned long, LightObject*, DefaultKeyCompare<unsigned long> >
            LightIterator;
        iterator = NewAdapter<LightIterator>().Allocate();
        iterator = new (iterator) LightIterator(m_lightMap);
        while (((LightIterator*)iterator)->IsValid())
        {
            delete ((LightIterator*)iterator)->Current()->value;
            ((LightIterator*)iterator)->Next();
        }
        m_lightMap.Clear();
        delete (LightIterator*)iterator;
    }

    {
        nlDLListIterator<WorldAnimController*> iterator(
            m_animControllerList.m_Head,
            nlDLRingGetStart(m_animControllerList.m_Head));
        while (iterator.hasNext())
        {
            delete iterator.m_Curr->entry;
            iterator.next();
        }
    }

    {
        typedef nlAVLTreeIterator<unsigned long, HelperObject*, DefaultKeyCompare<unsigned long> >
            HelperIterator;
        iterator = NewAdapter<HelperIterator>().Allocate();
        iterator = new (iterator) HelperIterator(m_helperMap);
        while (((HelperIterator*)iterator)->IsValid())
        {
            delete ((HelperIterator*)iterator)->Current()->value;
            ((HelperIterator*)iterator)->Next();
        }
        delete (HelperIterator*)iterator;
        m_helperMap.Clear();
    }
    delete m_pWorldAnimManager;
    delete m_pPhysicsData;
}

struct WorldObjectChunkData
{
    /* 0x00 */ char m_szName[64];
    /* 0x40 */ char m_szModelName[64];
    /* 0x80 */ u32 m_uHashID;
    /* 0x84 */ u32 m_uModelHashID;
    /* 0x88 */ u32 m_uPhyGeomHashID;
    /* 0x8C */ u32 m_uShadowHashID;
    /* 0x90 */ u32 m_uObjectCreationFlags;
    /* 0x94 */ u32 m_uRenderLayer;
    /* 0x98 */ float m_fRadius;
    /* 0x9C */ u32 m_uPadding1;
    /* 0xA0 */ nlMatrix4 m_worldMatrix;
}; // total size: 0xE0

struct WorldObjectData
{
    /* 0x00 */ unsigned long m_uObjectCreationFlags;
    /* 0x04 */ unsigned long m_uHashID;
    /* 0x08 */ unsigned long m_uModelID;
    /* 0x0C */ unsigned long m_uShadowHashID;
    /* 0x10 */ unsigned long m_uRenderLayer;
    /* 0x14 */ nlMatrix4 m_worldMatrix;
    /* 0x54 */ float m_fRadius;
    /* 0x58 */ nlVector3 m_v3Offset;
}; // total size: 0x64

static const int LF_NOLIGHT = 4;

struct WorldLightChunkData
{
    /* 0x00 */ char m_szName[64];
    /* 0x40 */ u32 m_uHashID;
    /* 0x44 */ float m_fIntensity;
    /* 0x48 */ float m_fFarAttenuationStart;
    /* 0x4C */ float m_fFarAttenuationEnd;
    /* 0x50 */ nlVector3 m_colour;
    /* 0x5C */ u32 m_emitFlags;
    /* 0x60 */ nlMatrix4 m_worldMatrix;
}; // total size: 0xA0

struct WorldEmitterChunkData
{
    /* 0x00 */ char m_szName[64];
    /* 0x40 */ u32 m_uHashID;
    /* 0x44 */ float m_fPadding0;
    /* 0x48 */ float m_fPadding1;
    /* 0x4C */ float m_fPadding2;
    /* 0x50 */ float m_fPadding3;
    /* 0x54 */ float m_fPadding4;
    /* 0x58 */ float m_fPadding5;
    /* 0x5C */ float m_fPadding6;
    /* 0x60 */ nlMatrix4 m_worldMatrix;
}; // total size: 0xA0

// WLD records are serialized GameCube structures. The byte/string prefix is
// endian-neutral; everything from wordOffset onward is a run of 32-bit scalar
// fields (including float/matrix bit patterns). Decode into a host record and
// leave the disc buffer immutable.
static bool DecodeWorldRecordBE(const nlChunk* rawChunk, u32 expectedType,
                                void* dst, u32 dstSize, u32 wordOffset)
{
    if (rawChunk == NULL || dst == NULL || wordOffset > dstSize || (wordOffset & 3u) != 0)
        return false;

    const u8* raw = (const u8*)rawChunk;
    const u32 rawSize = port_be32(raw + 4);
    PortBEChunkView chunk;
    if (!port_be_chunk_read(raw, raw + sizeof(nlChunk) + rawSize, &chunk) ||
        (chunk.id & 0x80FFFFFFu) != expectedType || chunk.payload_len != dstSize)
        return false;

    memcpy(dst, chunk.payload, wordOffset);
    for (u32 off = wordOffset; off < dstSize; off += 4)
    {
        const u32 value = port_be32(chunk.payload + off);
        memcpy((u8*)dst + off, &value, sizeof(value));
    }
    return true;
}

static void CopyWorldPhysicsElementsBE(CharacterPhysicsElement* dst, const u8* src, u32 count)
{
    static const struct
    {
        u32 offset;
        u32 words;
    } runs[] = {
        { 0x00, 16 }, // matLocalToParent
        { 0x60, 1 },  // uHashID; names are byte arrays
        { 0x84, 7 },  // uParentHashID through uReserved
    };

    for (u32 n = 0; n < count; ++n)
    {
        const u8* in = src + n * sizeof(CharacterPhysicsElement);
        u8* out = (u8*)&dst[n];
        memcpy(out, in, sizeof(CharacterPhysicsElement));
        for (u32 r = 0; r < sizeof(runs) / sizeof(runs[0]); ++r)
        {
            for (u32 w = 0; w < runs[r].words; ++w)
            {
                const u32 value = port_be32(in + runs[r].offset + w * 4);
                memcpy(out + runs[r].offset + w * 4, &value, sizeof(value));
            }
        }
    }
}

// PORT: was a second declaration of FlareHandler, padded to the console's 0x70.

static const unsigned long eOC_SHINY = 0x00000008;

static const unsigned long eOC_ENV_SHINY = 0x00000010;

/**
 * Offset/Address/Size: 0x376C | 0x80198430 | size: 0x68
 */
bool World::Load(bool forfe)
{
    bool ret = DoLoad();
    if (forfe == 0)
    {
        ret = DoInitialize();
    }
    m_Locked = 1;
    return ret;
}
/**
 * Offset/Address/Size: 0x3694 | 0x80198358 | size: 0xD8
 */
bool World::LoadGeometry(const char* szWorldName, bool bMakeDrawables, bool keepTransform, unsigned long* pDrawableObjectHashes, int* pNumObjectsLoaded)
{
    char buffer[256];

    nlSNPrintf(buffer, sizeof(buffer) - 1, "%s.glt", szWorldName);
    tDebugPrintManager::Print(DC_RENDER, "Loading world texture file: %s\n", buffer);
    glLoadTextureBundle(buffer);

    nlSNPrintf(buffer, 0xFF, "%s.glg", szWorldName);
    tDebugPrintManager::Print(DC_RENDER, "Loading world geometry file: %s\n", buffer);

    m_pModels = glLoadModel(buffer, &m_uNumModels);

    return LoadGeometry(m_pModels, m_uNumModels, bMakeDrawables, keepTransform, pDrawableObjectHashes, pNumObjectsLoaded, false);
}

static inline float World_SelectBoundingRadius(AABBDimensions& aabb)
{
    float dimX = aabb.mDim.x;
    float dimY = aabb.mDim.y;
    if (dimX >= dimY && dimX > aabb.mDim.z)
    {
        return dimX;
    }
    else
    {
        dimX = aabb.mDim.z;
        if (dimY >= dimX)
        {
            dimX = dimY;
        }
        return dimX;
    }
}

/**
 * Offset/Address/Size: 0x3420 | 0x801980E4 | size: 0x274
 */
bool World::LoadGeometry(glModel* gModel, unsigned long uNumModels, bool bMakeDrawables, bool keepTransform, unsigned long* pDrawableObjectHashes, int* pNumObjectsLoaded, bool bTrophy)
{
    WorldObjectData data;
    AABBDimensions aabb;
    DrawableObject* pObject;

    m_pModels = gModel;
    m_uNumModels = uNumModels;

    if (bMakeDrawables)
    {
        unsigned long* pHash = pDrawableObjectHashes;
        glModel* pModelStart = m_pModels;
        nlMatrix4* pWorldMtx = &data.m_worldMatrix;
        glModel* pModel = pModelStart;
        bool keep = keepTransform;
        glModel* pEndModel = pModelStart + m_uNumModels;
        bool bIsTrophy = bTrophy;
        int count = 0;
        float radius = 1.0f;

        while (pModel < pEndModel)
        {
            nlZeroMemory(&data, sizeof(WorldObjectData));

            data.m_uHashID = pModel->id;
            data.m_uModelID = pModel->id;
            data.m_uShadowHashID = (unsigned long)-1;

            if (keep)
            {
                glGetMatrix(pModel->packets->state.matrix, *pWorldMtx);
            }
            else
            {
                pWorldMtx->SetIdentity();
            }

            data.m_fRadius = radius;
            HandleObjectCreation(&data);

            pObject = FindDrawableObject(pModel->id);

            pObject->m_uObjectFlags |= 0x4;

            if (bIsTrophy)
            {
                DrawableModel* model = pObject->AsDrawableModel();
                if (model != NULL)
                {
                    glModelPacket* pPacket = model->m_pModel->packets;
                    while (pPacket < model->m_pModel->packets + model->m_pModel->numPackets)
                    {
                        if (pPacket->state.texconfig & 0x10)
                        {
                            pObject->m_uObjectCreationFlags |= eOC_ENV_SHINY;
                        }
                        pPacket++;
                    }
                }
            }

            pObject->GetAABBDimensions(aabb, false);

            pObject->m_fBoundingRadius = World_SelectBoundingRadius(aabb);

            if (pDrawableObjectHashes != NULL)
            {
                *pHash = pObject->m_uHashID;
            }

            pHash++;
            count++;
            pModel++;
        }

        if (pNumObjectsLoaded != NULL)
        {
            *pNumObjectsLoaded = count;
        }
    }

    return m_pModels != NULL;
}

GLShadowVolume* World::FindShadowVolumeByID(unsigned long uHashID)
{
    return glInventory.GetShadowVolume(uHashID);
}

glModel* World::FindModelByID(unsigned long uHashID)
{
    return glInventory.GetModel(uHashID);
}

/**
 * Offset/Address/Size: 0x3084 | 0x80197D48 | size: 0x39C
 */
u8 World::HandleObjectCreation(WorldObjectData* pObjectData)
{
    WorldObjectData* pData = pObjectData;
    DrawableObject* pDrawable = NULL;
    glModel* pModel = glInventory.GetModel(pData->m_uModelID);

    if (pModel == NULL)
    {
        return 1;
    }

    if ((pData->m_uObjectCreationFlags & 0x10) == 0)
    {
        glModelPacket* pPacketEnd;
        glModelPacket* pPacket = pModel->packets;
        pPacketEnd = pPacket + pModel->numPackets;

        while (pPacket < pPacketEnd)
        {
            if (pPacket->state.texconfig & 0x10)
            {
                pData->m_uObjectCreationFlags |= eOC_SHINY;
                break;
            }
            pPacket++;
        }
    }

    if (pData->m_uObjectCreationFlags & 0x2)
    {
        DrawableSkinModel* pSkin = new (nlMalloc(sizeof(DrawableSkinModel), 8, false)) DrawableSkinModel();
        pSkin->m_pModel = pModel;
        pSkin->m_pShadowVolume = glInventory.GetShadowVolume(pData->m_uShadowHashID);
        pSkin->m_uObjectFlags |= 0x4;
        pDrawable = pSkin;
    }
    else if (pData->m_uObjectCreationFlags & 0x20)
    {
        DrawableTmModel* pTm = new (nlMalloc(sizeof(DrawableTmModel), 8, false)) DrawableTmModel();
        pTm->m_pModel = pModel;
        pTm->m_pShadowVolume = glInventory.GetShadowVolume(pData->m_uShadowHashID);
        pTm->m_uObjectFlags |= 0x4;
        pDrawable = pTm;
    }
    else if (pData->m_uObjectCreationFlags & 0x1)
    {
        DrawableShadow* pShadow = new (nlMalloc(sizeof(DrawableShadow), 8, false)) DrawableShadow();
        pShadow->m_pModel = pModel;
        pShadow->m_uObjectFlags &= ~0x4;
        pDrawable = pShadow;
    }
    else
    {
        DrawableModel* pModelDrawable = new (nlMalloc(sizeof(DrawableModel), 8, false)) DrawableModel();
        pModelDrawable->m_pModel = pModel;
        pModelDrawable->m_pShadowVolume = glInventory.GetShadowVolume(pData->m_uShadowHashID);
        pModelDrawable->m_bVertexAnimated = glInventory.GetVertexAnim(pModel->id) ? true : false;
        pModelDrawable->m_uObjectFlags &= ~0x4;
        pDrawable = pModelDrawable;
    }

    if (pDrawable == NULL)
    {
        return 0;
    }

    pDrawable->m_pWorldContext = this;
    pDrawable->m_uObjectFlags |= 0x1;
    pDrawable->m_uHashID = pData->m_uHashID;
    pDrawable->m_uObjectCreationFlags = pData->m_uObjectCreationFlags;
    pDrawable->m_uRenderLayer = pData->m_uRenderLayer;
    pDrawable->m_worldMatrix = pData->m_worldMatrix;
    pDrawable->m_fBoundingRadius = pData->m_fRadius;

    m_drawableMap.Add(pDrawable->m_uHashID, pDrawable);
    return 1;
}

/**
 * Offset/Address/Size: 0x3020 | 0x80197CE4 | size: 0x64
 */
void World::AddToHyperSTSDrawables(unsigned long key, DrawableModel* pDrawableModel)
{
    m_hyperSTSDrawableMap.Add(key, pDrawableModel);
}

void World::AssignLightBitmasks()
{
    typedef nlAVLTreeIterator<unsigned long, LightObject*, DefaultKeyCompare<unsigned long> > LightIterator;

    LightIterator* iterator = new (nlMalloc(sizeof(LightIterator), 8, false))
        LightIterator(m_lightMap.m_Root, m_lightMap.m_NumElements);
    unsigned long bit = 1;
    while (iterator->IsValid())
    {
        iterator->Current()->value->m_bit = bit;
        bit <<= 1;
        iterator->Next();
    }
    delete iterator;
}

bool World::LoadPhysicsPrimitives(nlChunk* pChunk)
{
    if (pChunk == NULL)
        return false;

    const u8* raw = (const u8*)pChunk;
    const u32 rawSize = port_be32(raw + 4);
    PortBEChunkView root;
    if (!port_be_chunk_read(raw, raw + sizeof(nlChunk) + rawSize, &root) ||
        (root.id & 0x00FFFFFFu) != 0x0001D000u)
        return false;

    bool haveCount = false;
    bool haveElements = false;
    u32 count = 0;
    const u8* elements = NULL;
    unsigned long elementsLen = 0;

    const u8* cursor = root.raw + sizeof(nlChunk);
    while (cursor < root.next)
    {
        PortBEChunkView child;
        if (!port_be_chunk_read(cursor, root.next, &child))
            return false;
        cursor = child.next;

        switch (child.id & 0x80FFFFFFu)
        {
        case 0x0001D001:
            if (child.payload_len < sizeof(u32))
                return false;
            count = port_be32(child.payload);
            if (count > 0x10000u)
                return false;
            haveCount = true;
            break;
        case 0x0001D002:
            elements = child.payload;
            elementsLen = child.payload_len;
            haveElements = true;
            break;
        default:
            break;
        }
    }

    if (!haveCount || !haveElements ||
        count > elementsLen / sizeof(CharacterPhysicsElement))
        return false;

    CharacterPhysicsData* physics =
        new (nlMalloc(sizeof(CharacterPhysicsData), 8, false)) CharacterPhysicsData();
    if (physics == NULL)
        return false;
    physics->physicsElementCount = count;
    physics->pPhysicsElements = NULL;
    if (count != 0)
    {
        physics->pPhysicsElements = (CharacterPhysicsElement*)nlMalloc(
            count * sizeof(CharacterPhysicsElement), 8, false);
        if (physics->pPhysicsElements == NULL)
        {
            delete physics;
            return false;
        }
        CopyWorldPhysicsElementsBE(physics->pPhysicsElements, elements, count);
    }

    delete m_pPhysicsData;
    m_pPhysicsData = physics;
    return true;
}

/**
 * Offset/Address/Size: 0x264C | 0x80197310 | size: 0x9D4
 */
bool World::LoadObjectData(const char* szWorldName)
{
    char szFullFileName[255];
    void* pWorldData;

    nlSNPrintf(szFullFileName, sizeof(szFullFileName), "art/%s.wld", szWorldName);
    tDebugPrintManager::Print(DC_RENDER, "Loading world object file: %s\n", szFullFileName);

    unsigned long uWorldDataSize = 0;
    pWorldData = nlLoadEntireFile(szFullFileName, &uWorldDataSize, 0x20, AllocateEnd);
    if (pWorldData == NULL)
    {
        nlPrintf("Error: Failed to load world object data '%s'\n", szFullFileName);
        return false;
    }

    const u8* fileBegin = (const u8*)pWorldData;
    const u8* fileEnd = fileBegin + uWorldDataSize;
    PortBEChunkView root;
    if (!port_be_chunk_read(fileBegin, fileEnd, &root) ||
        (root.id & 0x00FFFFFFu) != 0x00019000u)
    {
        OSReport("Error: '%s' is not a well-formed world object file\n", szFullFileName);
        nlFree(pWorldData);
        return false;
    }

    const u8* cursor = root.raw + sizeof(nlChunk);
    while (cursor < root.next)
    {
        PortBEChunkView chunk;
        if (!port_be_chunk_read(cursor, root.next, &chunk))
        {
            OSReport("Error: malformed WLD child in '%s' at offset %lu\n",
                     szFullFileName, (unsigned long)(cursor - fileBegin));
            nlFree(pWorldData);
            return false;
        }
        cursor = chunk.next;

        nlChunk* rawChunk = (nlChunk*)chunk.raw;
        switch (chunk.id & 0x80FFFFFFu)
        {
        case 0x19001:
            break;
        case 0x19002:
            break;
        case 0x19003:
            CreateWorldObjFromChunk(rawChunk);
            break;
        case 0x19004:
            break;
        case 0x19005:
            CreateLightObjFromChunk(rawChunk);
            break;
        case 0x19100:
            break;
        case 0x19101:
            CreateEmitterObjFromChunk(rawChunk);
            break;
        case 0x19200:
            break;
        case 0x19201:
            CreateHelperObjFromChunk(rawChunk);
            break;
        case (u32)0x0001D000:
        case (u32)0x8001D000:
            if (!LoadPhysicsPrimitives(rawChunk))
            {
                OSReport("Error: malformed WLD physics block in '%s'\n", szFullFileName);
                nlFree(pWorldData);
                return false;
            }
            break;
        default:
            break;
        }
    }

    nlFree(pWorldData);
    AssignLightBitmasks();
    return true;
}

static inline nlFloatColour MakeIntensityColour(float r, float g, float b, float a)
{
    nlFloatColour colour;
    nlFloatColourSet(colour, r, g, b, a);
    return colour;
}

/**
 * Offset/Address/Size: 0x1CA8 | 0x8019696C | size: 0x9A4
 */
void World::CreateLightUserData()
{
    nlListContainer<LightObject*> lightList;
    nlListContainer<LightObject*> specList;
    int numLights = 0;
    int numSpecLights = 0;
    LightObject* pLight;
    {
        typedef nlAVLTreeIterator<unsigned long, LightObject*, DefaultKeyCompare<unsigned long> >
            LightIterator;
        LightIterator* iterator = m_lightMap.GetIterator();

        while (iterator->IsValid())
        {
            pLight = iterator->CurrentValue();
            if ((pLight->m_emitFlags & 0x4) == 0)
            {
                numLights++;
                lightList.AddEntry(pLight);
            }

            if ((pLight->m_emitFlags & 0x2) != 0)
            {
                numSpecLights++;
                specList.AddEntry(pLight);
            }
            iterator->Next();
        }

        delete iterator;
    }

    pLight = fxLightObjects;
    int i;
    int numExtra = 0;
    {
        for (i = 0; i < EmissionManager::GetNumLights(); i++)
        {
            const EffectsLight* pLight = EmissionManager::GetLight(i);

            fxLightObjects[i].m_worldPosition = pLight->m_v3Position;
            fxLightObjects[i].m_fIntensity = (2.0f * (f32)pLight->m_Colour.c[3]) / 255.0f;
            fxLightObjects[i].m_fFarAttenuationStart = 0.0f;
            fxLightObjects[i].m_fFarAttenuationEnd = pLight->m_fRadius;
            fxLightObjects[i].m_colour.c[0] = 0.003921569f * (f32)pLight->m_Colour.c[0];
            fxLightObjects[i].m_colour.c[1] = 0.003921569f * (f32)pLight->m_Colour.c[1];
            fxLightObjects[i].m_colour.c[2] = 0.003921569f * (f32)pLight->m_Colour.c[2];
            fxLightObjects[i].m_colour.c[3] = 0.003921569f * (f32)pLight->m_Colour.c[3];
            fxLightObjects[i].m_fRadiusSquared = pLight->m_fRadius * pLight->m_fRadius;
            numExtra++;
        }
    }
    if (numLights == 0)
    {
        m_pLightData = NULL;
        m_pIntensityPerm = NULL;
    }
    else
    {
        int totalLights = numLights + numExtra;
        u32* pIntensityPermData;
        unsigned long size = totalLights * sizeof(GLLightUserData) + 4;
        m_pLightData = glUserAlloc(GLUD_Light, size, false);

        u32* p32 = (u32*)glUserGetData(m_pLightData);
        *p32 = totalLights;
        GLLightUserData* glLight = (GLLightUserData*)(p32 + 1);

        nlListIterator<LightObject*> lightIterator = lightList.Begin();
        while (lightIterator.IsValid())
        {
            LightObject* pLight = lightIterator.Current();
            glLight->colour = pLight->m_colour;
            glLight->worldPosition = pLight->m_worldPosition;
            glLight->intensity = pLight->m_fIntensity;
            if (pLight->m_emitFlags & 0x8)
            {
                glLight->innerRadius = 0.0f;
                glLight->outerRadius = 0.0f;
            }
            else
            {
                glLight->innerRadius = pLight->m_fFarAttenuationStart;
                glLight->outerRadius = pLight->m_fFarAttenuationEnd;
            }
            lightIterator.Next();
            glLight++;
        }
        for (int i = 0; i < numExtra; i++)
        {
            glLight->colour = pLight[i].m_colour;
            glLight->worldPosition = pLight[i].m_worldPosition;
            glLight->intensity = pLight[i].m_fIntensity;
            glLight->innerRadius = pLight[i].m_fFarAttenuationStart;
            glLight->outerRadius = pLight[i].m_fFarAttenuationEnd;
            glLight++;
        }

        m_pIntensityPerm = glUserAlloc(GLUD_Light, size, false);
        p32 = (u32*)glUserGetData(m_pIntensityPerm);
        *p32 = totalLights;
        pIntensityPermData = p32;
        glLight = (GLLightUserData*)(p32 + 1);
        const float fBlueWeight = 0.11f;
        const float fRedWeight = 0.3f;
        const float fGreenWeight = 0.59f;
        const float fZero = 0.0f;
        const float fOne = 1.0f;

        nlListIterator<LightObject*> lightIterator2 = lightList.Begin();
        while (lightIterator2.IsValid())
        {
            LightObject* pLight = lightIterator2.Current();
            glLight->colour = MakeIntensityColour(
                fBlueWeight * pLight->m_colour.c[2]
                    + (fRedWeight * pLight->m_colour.c[0]
                        + fGreenWeight * pLight->m_colour.c[1]),
                fZero,
                fZero,
                fOne);
            glLight->worldPosition = pLight->m_worldPosition;
            glLight->intensity = pLight->m_fIntensity;
            if (pLight->m_emitFlags & 0x8)
            {
                glLight->innerRadius = 0.0f;
                glLight->outerRadius = 0.0f;
            }
            else
            {
                glLight->innerRadius = pLight->m_fFarAttenuationStart;
                glLight->outerRadius = pLight->m_fFarAttenuationEnd;
            }
            lightIterator2.Next();
            glLight++;
        }
        for (int i = numExtra; i > 0; i--)
        {
            glLight->colour = pLight->m_colour;
            glLight->worldPosition = pLight->m_worldPosition;
            glLight->intensity = pLight->m_fIntensity;
            glLight->innerRadius = pLight->m_fFarAttenuationStart;
            glLight->outerRadius = pLight->m_fFarAttenuationEnd;
            pLight = (LightObject*)((u8*)pLight + sizeof(LightObject));
            glLight++;
        }

        if (m_pPlayerNISLightData == NULL)
        {
            size = numLights * sizeof(GLLightUserData) + 4;
            m_pPlayerNISLightData = glUserAlloc(GLUD_Light, size, true);
            memcpy(
                glUserGetData(m_pPlayerNISLightData),
                pIntensityPermData,
                size);
        }
    }

    if (numSpecLights == 0)
    {
        m_pSTSIntensity = NULL;
    }
    else
    {
        nlVector3 origin = { { 0.0f, 0.0f, 0.0f } };
        unsigned long size = numLights * sizeof(GLSpecularUserData) + 4;
        m_pSTSIntensity = glUserAlloc(GLUD_Specular, size, false);
        u32* p32 = (u32*)glUserGetData(m_pSTSIntensity);
        *p32 = numSpecLights;
        GLSpecularUserData* pSpec = (GLSpecularUserData*)(p32 + 1);
        nlListIterator<LightObject*> specIterator = specList.Begin();
        while (specIterator.IsValid())
        {
            LightObject* pLight = specIterator.Current();
            pSpec->colour = pLight->m_colour;
            pSpec->exponent = 64.0f;
            pSpec->intensity = pLight->m_fIntensity;
            nlVec3Sub(pSpec->worldDirection, origin, pLight->m_worldPosition);
            pSpec++;
            specIterator.Next();
        }
    }

    m_pIntensityData = glUserAlloc(GLUD_Light, 0x2C, false);
    {
        u32* p32 = (u32*)glUserGetData(m_pIntensityData);
        *p32 = 1;
        GLLightUserData* glLight = (GLLightUserData*)(p32 + 1);
        nlZeroMemory(glLight, sizeof(GLLightUserData));
        static nlVector3 vLightDirection = { { 0.0f, 0.0f, -1.0f } };
        glLight->worldPosition = vLightDirection;
        glLight->colour.c[0] = 1.0f;
        glLight->colour.c[1] = 1.0f;
        glLight->colour.c[2] = 1.0f;
        glLight->colour.c[3] = 1.0f;
        glLight->intensity = 1.0f;
        glLight->innerRadius = 0.0f;
        glLight->outerRadius = 0.0f;
    }
}

/**
 * Offset/Address/Size: 0x1B7C | 0x80196840 | size: 0x12C
 */
void* World::GetCustomSpecularData(glModelPacket* pPacket, bool bPerm)
{
    GLSpecularUserData* pCursor;
    u32* p32;
    int numLights;
    void* pSTSIntensity = GetSTSIntensity();

    u8 glossLevel = (u8)glGetTextureState(pPacket->state.texturestate, GLTS_GlossLevel);

    f32 fGloss = (f32)glossLevel;
    f32 fNorm = fGloss * 0.015873017f;
    f32 fInv = 1.0f - fNorm;
    f32 fExponent = g_fExponentScale * fInv + g_fExponentBase;

    p32 = (u32*)glUserGetData(pSTSIntensity);
    numLights = *p32;
    void* pNewData = glUserAlloc(GLUD_Specular, numLights * sizeof(GLSpecularUserData) + 4, bPerm);

    p32 = (u32*)glUserGetData(pNewData);
    pCursor = (GLSpecularUserData*)((int*)p32 + 1);
    memcpy(p32, glUserGetData(pSTSIntensity), numLights * sizeof(GLSpecularUserData) + 4);

    for (int i = numLights; i > 0; i--)
    {
        pCursor->exponent = fExponent;
        pCursor++;
    }

    return pNewData;
}

/**
 * Offset/Address/Size: 0x191C | 0x801965E0 | size: 0x260
 */
void World::CreateHelperObjFromChunk(nlChunk* chunk)
{
    static int flareLen;
    static signed char init;

    HelperObject* pHelper;
    WorldHelperChunkData worldHelperChunkData;
    WorldHelperChunkData* pWorldHelperChunkData = &worldHelperChunkData;
    char* substring;
    const char* flashString;
    const char* flareTag;
    char flareName[64];

    if (!DecodeWorldRecordBE(chunk, 0x00019201u, pWorldHelperChunkData,
                             sizeof(*pWorldHelperChunkData), 0x3C))
    {
        OSReport("Error: malformed WLD helper record\n");
        return;
    }

    pHelper = (HelperObject*)nlMalloc(sizeof(HelperObject), 8, false);
    pHelper->m_uHashID = pWorldHelperChunkData->m_uHashID;
    pHelper->m_worldMatrix = pWorldHelperChunkData->m_worldMatrix;

    substring = nlStrChr<char>(pWorldHelperChunkData->m_szName, '/');
    flashString = "fx_camera_flash";
    if (nlStrNICmp<char>(substring + 1, flashString, nlStrLen<char>(flashString)) == 0)
    {
        nlStrNCpy<char>(pHelper->m_szName, substring + 1, sizeof(pHelper->m_szName));
    }
    else
    {
        flareTag = "flare_";

        if (!init)
        {
            flareLen = nlStrLen<char>(flareTag);
            init = 1;
        }

        substring = strstr(pWorldHelperChunkData->m_szName, flareTag);
        if (substring != NULL)
        {
            nlStrNCpy<char>(flareName, substring + flareLen, sizeof(flareName));

            substring = strstr(flareName, "_");
            if (substring != NULL)
            {
                *substring = '\0';
            }

            if (nlToLower<char>(flareName[0]) == 'h')
            {
                FlareHandler::instance.AddHalo(pWorldHelperChunkData->m_worldMatrix);
            }
            else
            {
                FlareHandler::instance.AddGlow(pWorldHelperChunkData->m_worldMatrix);
            }

            delete pHelper;
            return;
        }

        nlStrNCpy<char>(pHelper->m_szName, pWorldHelperChunkData->m_szName, sizeof(pHelper->m_szName));
    }

    m_helperMap.Add(pHelper->m_uHashID, pHelper);
}

void World::CreateEmitterObjFromChunk(nlChunk* pChunk)
{
    WorldEmitterChunkData emitterData;
    WorldEmitterChunkData* pEmitterData = &emitterData;
    const char* pPersistentEffectsTag;
    char fxName[256];
    int i;
    EffectsGroup* pEffectsGroup;
    EmissionController* pEmissionController;
    HelperObject* pHelper;

    if (!DecodeWorldRecordBE(pChunk, 0x00019101u, pEmitterData,
                             sizeof(*pEmitterData), 0x40))
    {
        OSReport("Error: malformed WLD emitter record\n");
        return;
    }
    pPersistentEffectsTag = "fx_persistent_";
    static int persistentLen = nlStrLen<char>(pPersistentEffectsTag);

    pPersistentEffectsTag = strstr(pEmitterData->m_szName, pPersistentEffectsTag);
    if (pPersistentEffectsTag != NULL)
    {
        nlStrNCpy<char>(fxName, pPersistentEffectsTag + persistentLen, sizeof(fxName));

        i = strlen(fxName);
        if (__ctype_map[(unsigned char)fxName[i - 1]] & __digit)
        {
            i = strlen(fxName);
            while (i > 0)
            {
                if (fxName[i] == '_')
                {
                    fxName[i] = '\0';
                    break;
                }
                i--;
            }
        }

        pEffectsGroup = fxGetGroup(fxName);
        if (pEffectsGroup != NULL)
        {
            pEmissionController = EmissionManager::Create(pEffectsGroup, 0);
            pEmissionController->SetPosition(pEmitterData->m_worldMatrix.GetTranslation());
            pEmissionController->m_uUserData = 0xDEADBEEF;
        }
    }
    else
    {
        pHelper = (HelperObject*)nlMalloc(sizeof(HelperObject), 8, false);
        pHelper->m_uHashID = pEmitterData->m_uHashID;
        pHelper->m_worldMatrix = pEmitterData->m_worldMatrix;
        nlStrNCpy<char>(pHelper->m_szName, pEmitterData->m_szName, sizeof(pHelper->m_szName));
        m_helperMap.Add(pHelper->m_uHashID, pHelper);
    }
}

void World::CreateLightObjFromChunk(nlChunk* pChunk)
{
    LightObject* pLightObj;
    WorldLightChunkData lightData;
    WorldLightChunkData* pWorldLightChunkData = &lightData;
    if (!DecodeWorldRecordBE(pChunk, 0x00019005u, pWorldLightChunkData,
                             sizeof(*pWorldLightChunkData), 0x40))
    {
        OSReport("Error: malformed WLD light record\n");
        return;
    }

    pLightObj = (LightObject*)nlMalloc(sizeof(LightObject), 8, false);
    pLightObj->m_uHashID = pWorldLightChunkData->m_uHashID;
    pLightObj->m_worldPosition = pWorldLightChunkData->m_worldMatrix.GetTranslation();
    pLightObj->m_fIntensity = pWorldLightChunkData->m_fIntensity;
    pLightObj->m_fFarAttenuationStart = pWorldLightChunkData->m_fFarAttenuationStart;
    pLightObj->m_fFarAttenuationEnd = pWorldLightChunkData->m_fFarAttenuationEnd;
    pLightObj->m_emitFlags = pWorldLightChunkData->m_emitFlags;

    if (pLightObj->m_fIntensity < 0.01f)
    {
        pLightObj->m_emitFlags |= LF_NOLIGHT;
    }

    nlFloatColourSet(
        pLightObj->m_colour,
        pWorldLightChunkData->m_colour.x,
        pWorldLightChunkData->m_colour.y,
        pWorldLightChunkData->m_colour.z,
        0.0f);
    pLightObj->m_bit = 0;
    m_lightMap.Add(pLightObj->m_uHashID, pLightObj);
}

void World::CreateWorldObjFromChunk(nlChunk* pChunk)
{
    WorldObjectChunkData worldObjectChunkData;
    WorldObjectChunkData* pWorldObjectChunkData = &worldObjectChunkData;
    WorldObjectData objectData;
    memset(&objectData, 0, sizeof(WorldObjectData));
    if (!DecodeWorldRecordBE(pChunk, 0x00019003u, pWorldObjectChunkData,
                             sizeof(*pWorldObjectChunkData), 0x80))
    {
        OSReport("Error: malformed WLD object record\n");
        return;
    }

    objectData.m_uObjectCreationFlags = pWorldObjectChunkData->m_uObjectCreationFlags;
    objectData.m_uHashID = pWorldObjectChunkData->m_uHashID;
    objectData.m_worldMatrix = pWorldObjectChunkData->m_worldMatrix;
    objectData.m_uShadowHashID = pWorldObjectChunkData->m_uShadowHashID;
    objectData.m_uRenderLayer = pWorldObjectChunkData->m_uRenderLayer;
    objectData.m_uModelID = pWorldObjectChunkData->m_uModelHashID;
    objectData.m_fRadius = pWorldObjectChunkData->m_fRadius;

    HandleObjectCreation(&objectData);
}

/**
 * Offset/Address/Size: 0x1884 | 0x80196548 | size: 0x98
 */
void World::Update(float fDeltaT)
{
    DLListEntry<WorldAnimController*>* start = nlDLRingGetStart(m_animControllerList.m_Head);
    DLListEntry<WorldAnimController*>* head = m_animControllerList.m_Head;
    DLListEntry<WorldAnimController*>* current = start;

    while (current != NULL)
    {
        current->entry->Update(fDeltaT);

        if ((nlDLRingIsEnd(head, current) != 0) || (current == NULL))
        {
            current = NULL;
        }
        else
        {
            current = current->m_next;
        }
    }
}

/**
 * Offset/Address/Size: 0x1880 | 0x80196544 | size: 0x4
 */
void World::UpdateInReplay(float fTimeDelta)
{
}


/**
 * Offset/Address/Size: 0x1428 | 0x801960EC | size: 0x458
 */
void World::ExtractFrustumPlanes()
{
    nlMatrix4 viewProjection;
    nlMatrix4 projection = *glViewGetProjectionMatrix(GLV_Unshadowed);

    float m33 = projection.m33;
    float m34 = projection.m34;
    float m43 = projection.m43;
    projection.m43 = m34;
    projection.m34 = m43;
    projection.m33 = m33 - 1.0f;

    nlMultMatrices(viewProjection, *glViewGetViewMatrix(GLV_Unshadowed), projection);

    m_frustumPlane[0].x = viewProjection.m14 - viewProjection.m11;
    m_frustumPlane[0].y = viewProjection.m24 - viewProjection.m21;
    m_frustumPlane[0].z = viewProjection.m34 - viewProjection.m31;
    m_frustumPlane[0].w = viewProjection.m44 - viewProjection.m41;
    {
        float length = nlSqrt(
            m_frustumPlane[0].x * m_frustumPlane[0].x + m_frustumPlane[0].y * m_frustumPlane[0].y + m_frustumPlane[0].z * m_frustumPlane[0].z,
            true);
        m_frustumPlane[0].x /= length;
        m_frustumPlane[0].y /= length;
        m_frustumPlane[0].z /= length;
        m_frustumPlane[0].w /= length;
    }

    m_frustumPlane[1].x = viewProjection.m14 + viewProjection.m11;
    m_frustumPlane[1].y = viewProjection.m24 + viewProjection.m21;
    m_frustumPlane[1].z = viewProjection.m34 + viewProjection.m31;
    m_frustumPlane[1].w = viewProjection.m44 + viewProjection.m41;
    {
        float length = nlSqrt(
            m_frustumPlane[1].x * m_frustumPlane[1].x + m_frustumPlane[1].y * m_frustumPlane[1].y + m_frustumPlane[1].z * m_frustumPlane[1].z,
            true);
        m_frustumPlane[1].x /= length;
        m_frustumPlane[1].y /= length;
        m_frustumPlane[1].z /= length;
        m_frustumPlane[1].w /= length;
    }

    m_frustumPlane[2].x = viewProjection.m14 + viewProjection.m12;
    m_frustumPlane[2].y = viewProjection.m24 + viewProjection.m22;
    m_frustumPlane[2].z = viewProjection.m34 + viewProjection.m32;
    m_frustumPlane[2].w = viewProjection.m44 + viewProjection.m42;
    {
        float length = nlSqrt(
            m_frustumPlane[2].x * m_frustumPlane[2].x + m_frustumPlane[2].y * m_frustumPlane[2].y + m_frustumPlane[2].z * m_frustumPlane[2].z,
            true);
        m_frustumPlane[2].x /= length;
        m_frustumPlane[2].y /= length;
        m_frustumPlane[2].z /= length;
        m_frustumPlane[2].w /= length;
    }

    m_frustumPlane[3].x = viewProjection.m14 - viewProjection.m12;
    m_frustumPlane[3].y = viewProjection.m24 - viewProjection.m22;
    m_frustumPlane[3].z = viewProjection.m34 - viewProjection.m32;
    m_frustumPlane[3].w = viewProjection.m44 - viewProjection.m42;
    {
        float length = nlSqrt(
            m_frustumPlane[3].x * m_frustumPlane[3].x + m_frustumPlane[3].y * m_frustumPlane[3].y + m_frustumPlane[3].z * m_frustumPlane[3].z,
            true);
        m_frustumPlane[3].x /= length;
        m_frustumPlane[3].y /= length;
        m_frustumPlane[3].z /= length;
        m_frustumPlane[3].w /= length;
    }

    m_frustumPlane[4].x = viewProjection.m13;
    m_frustumPlane[4].y = viewProjection.m23;
    m_frustumPlane[4].z = viewProjection.m33;
    m_frustumPlane[4].w = viewProjection.m43;
    {
        float length = nlSqrt(
            m_frustumPlane[4].x * m_frustumPlane[4].x + m_frustumPlane[4].y * m_frustumPlane[4].y + m_frustumPlane[4].z * m_frustumPlane[4].z,
            true);
        m_frustumPlane[4].x /= length;
        m_frustumPlane[4].y /= length;
        m_frustumPlane[4].z /= length;
        m_frustumPlane[4].w /= length;
    }

    m_frustumPlane[5].x = viewProjection.m14 - viewProjection.m13;
    m_frustumPlane[5].y = viewProjection.m24 - viewProjection.m23;
    m_frustumPlane[5].z = viewProjection.m34 - viewProjection.m33;
    m_frustumPlane[5].w = viewProjection.m44 - viewProjection.m43;
    {
        float length = nlSqrt(
            m_frustumPlane[5].x * m_frustumPlane[5].x + m_frustumPlane[5].y * m_frustumPlane[5].y + m_frustumPlane[5].z * m_frustumPlane[5].z,
            true);
        m_frustumPlane[5].x /= length;
        m_frustumPlane[5].y /= length;
        m_frustumPlane[5].z /= length;
        m_frustumPlane[5].w /= length;
    }
}

/**
 * Offset/Address/Size: 0x1340 | 0x80196004 | size: 0xE8
 */
bool World::IsSphereInFrustum(const nlMatrix4& mat, float radius)
{
    nlVector3 v3Position = mat.GetTranslation();

    f32 posX = v3Position.x;
    f32 posY = v3Position.y;
    f32 posZ = v3Position.z;
    f32 negRadius = -radius;

    nlVector4* pPlanes = m_frustumPlane;
    World* self = this;

    for (int i = 0; i < 6; i++)
    {
        f32 dot = posX * pPlanes[i].x + posY * pPlanes[i].y + posZ * pPlanes[i].z + self->m_frustumPlane[i].w;
        if (dot < negRadius)
            return false;
    }
    return true;
}

unsigned char World::IsSphereInFrustum(const nlVector4* pPlanes, const nlMatrix4& mWorld, float fRadius)
{
    nlVector3 v3Position = mWorld.GetTranslation();
    f32 negRadius = -fRadius;
    for (int i = 0; i < 6; i++)
    {
        if (nlPlaneDot(pPlanes[i], v3Position) < negRadius)
            return false;
    }
    return true;
}

/**
 * Offset/Address/Size: 0x10F4 | 0x80195DB8 | size: 0x24C
 */
void DoTranslucency(DrawableObject* pObject)
{
    int cameraType = cCameraManager::m_pBeginFrameCameraType;

    if (cameraType == 4)
    {
        pObject->m_translucency = 1.0f;
        if (pObject->m_translucency < 0.0f)
        {
            pObject->m_translucency = 0.0f;
        }
        if (pObject->m_translucency > 1.0f)
        {
            pObject->m_translucency = 1.0f;
        }
        return;
    }

    float fTrans = pObject->m_translucency;
    bool inGameplayCamera = false;
    bool transitioningOutOfGameplayCamera = false;
    bool canBeTransparent = false;

    if ((cameraType == 7) || (cameraType == 10))
    {
        inGameplayCamera = true;
    }

    if (sbPretendWereNotInGameplayCam)
    {
        inGameplayCamera = false;
    }

    if (nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack)->GetType() == 8)
    {
        if (((MatrixEffectCam*)nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack))->mbUseGameplayTransparencyFlags)
        {
            transitioningOutOfGameplayCamera = true;
        }
    }

    unsigned long objectCreationFlags = pObject->m_uObjectCreationFlags;

    if (objectCreationFlags & 0x2000)
    {
        if (inGameplayCamera)
        {
            pObject->m_translucency = 0.0f;
            if (pObject->m_translucency < 0.0f)
            {
                pObject->m_translucency = 0.0f;
            }
            if (pObject->m_translucency > 1.0f)
            {
                pObject->m_translucency = 1.0f;
            }
            return;
        }

        if (transitioningOutOfGameplayCamera)
        {
            canBeTransparent = true;
        }
    }

    if (objectCreationFlags & 0x8000)
    {
        if (inGameplayCamera)
        {
            pObject->m_translucency = 0.0f;
            if (pObject->m_translucency < 0.0f)
            {
                pObject->m_translucency = 0.0f;
            }
            if (pObject->m_translucency > 1.0f)
            {
                pObject->m_translucency = 1.0f;
            }
            return;
        }
    }

    if (!(objectCreationFlags & 0x1000) && !canBeTransparent && !sbAllObjectsCanBeTransparent)
    {
        pObject->m_translucency = 1.0f;
        if (pObject->m_translucency < 0.0f)
        {
            pObject->m_translucency = 0.0f;
        }
        if (pObject->m_translucency > 1.0f)
        {
            pObject->m_translucency = 1.0f;
        }
        return;
    }

    if (cCameraManager::IsObjectOccludingField(pObject))
    {
        fTrans -= g_fTransAdjustOccluded;
    }
    else
    {
        fTrans += g_fTransAdjustNotOccluded;
    }

    if (fTrans > 1.0f)
    {
        fTrans = 1.0f;
    }

    if (fTrans < g_fTransMinimum)
    {
        fTrans = g_fTransMinimum;
    }

    pObject->m_translucency = fTrans;
    if (pObject->m_translucency < 0.0f)
    {
        pObject->m_translucency = 0.0f;
    }
    if (pObject->m_translucency > 1.0f)
    {
        pObject->m_translucency = 1.0f;
    }
}

/**
 * Offset/Address/Size: 0xF54 | 0x80195C18 | size: 0x1A0
 */
void World::HandleCameraSwitch()
{
    typedef nlAVLTreeIterator<unsigned long, DrawableObject*, DefaultKeyCompare<unsigned long> > DrawableIterator;
    DrawableIterator* iterator = m_drawableMap.GetIterator();

    f32 maxVal = 1.0f;
    f32 minVal = 0.0f;

    while (iterator->IsValid())
    {
        DrawableObject* pObject = iterator->CurrentValue();

        pObject->m_translucency = maxVal;
        if (pObject->m_translucency < minVal)
        {
            pObject->m_translucency = minVal;
        }
        if (pObject->m_translucency > maxVal)
        {
            pObject->m_translucency = maxVal;
        }

        iterator->Next();
    }

    delete iterator;
}

void DoTranslucency(DrawableObject* pObject);

static inline u8 World_IsSphereInFrustumInline(World* pWorld, const nlMatrix4& mat, f32 radius)
{
    nlVector3 v3Position = mat.GetTranslation();

    f32 posX = v3Position.x;
    f32 negRadius = -radius;
    f32 posZ = v3Position.z;
    f32 posY = v3Position.y;

    nlVector4* pPlanes = pWorld->m_frustumPlane;
    World* self = pWorld;

    for (int i = 0; i < 6; i++)
    {
        f32 dot = posX * pPlanes[i].x + posY * pPlanes[i].y + posZ * pPlanes[i].z + self->m_frustumPlane[i].w;
        if (dot < negRadius)
            return false;
    }
    return true;
}

#if defined(PORT_VITA)
namespace
{
struct VitaCullEntry
{
    DrawableObject* object;
    nlVector3 position;
    float radius;
    u8 forceVisible;
    u8 visible;
};

struct VitaCullJob
{
    VitaCullEntry* entries;
    nlVector4 planes[6];
};

bool VitaCullRange(void* opaque, size_t begin, size_t end, uint32_t) noexcept
{
    VitaCullJob& job = *static_cast<VitaCullJob*>(opaque);
    for (size_t i = begin; i < end; ++i)
    {
        VitaCullEntry& entry = job.entries[i];
        if (entry.forceVisible)
        {
            entry.visible = 1;
            continue;
        }

        const float negRadius = -entry.radius;
        u8 visible = 1;
        for (int plane = 0; plane < 6; ++plane)
        {
            const nlVector4& p = job.planes[plane];
            const float dot = entry.position.x * p.x + entry.position.y * p.y +
                              entry.position.z * p.z + p.w;
            if (dot < negRadius)
            {
                visible = 0;
                break;
            }
        }
        entry.visible = visible;
    }
    return true;
}

bool VitaWorldParallelEnabled()
{
    static const bool enabled = [] {
        const char* value = getenv("STRIKERS_VITA_GAME_PARALLEL");
        return value == NULL || value[0] != '0';
    }();
    return enabled;
}
}
#endif

static void RenderBoundingSphere(const nlMatrix4& matWorld, f32 fRadius);

/**
 * Offset/Address/Size: 0x434 | 0x801950F8 | size: 0xB20
 */
void World::Render()
{
    typedef nlAVLTreeIterator<unsigned long, DrawableObject*, DefaultKeyCompare<unsigned long> > DrawableIterator;

    int nSubmitted;
    int nDrawn;
    nSubmitted = 0;
    nDrawn = 0;
    u8 bFreezeSide = g_bFreezeSideCam;
    if (bFreezeSide && g_bFreezeEndCam)
        g_bFreezeEndCam = 0;
    u8 bFreezeEnd = g_bFreezeEndCam;
    g_bDebugEqualsSide = bFreezeSide;
    g_bDebugEqualsEnd = bFreezeEnd;
    if (!g_bFreezeFrustum && !bFreezeSide && !bFreezeEnd)
        ExtractFrustumPlanes();
    u8 gameFlag = IsCaptainShootToScorePresentationOn();
    if (!gameFlag)
        DrawableCharacter::sSTSLighting = false;
    CreateLightUserData();

    DrawableIterator* iter;
    World* pWorld = this;
    if (!sbIsHyperShootToScoreRenderingEnabled)
    {
        iter = pWorld->m_drawableMap.GetIterator();
    }
    else
    {
        iter = pWorld->m_hyperSTSDrawableMap.GetIterator();
    }

    if (g_bClipToFrustum)
    {
#if defined(PORT_VITA)
        if (VitaWorldParallelEnabled() && aurora::vita::worker_threads() != 0)
        {
            // Keep the AVL traversal, lazy world-matrix materialization and all
            // draw/state mutation on CPU0. CPU2 receives only immutable sphere
            // positions/radii plus a snapshot of the six frustum planes.
            static std::vector<VitaCullEntry> cullEntries;
            cullEntries.clear();
            if (cullEntries.capacity() < 256)
                cullEntries.reserve(256);

            static const bool bNoCull = getenv("STRIKERS_NO_CULL") != NULL;
            while (iter->IsValid())
            {
                DrawableObject* pObject = iter->CurrentValue();
                if (sbIsHyperShootToScoreRenderingEnabled)
                {
                    const float x = pObject->GetWorldMatrix().e2[3][0];
                    if ((x < 0.0f && sbShowPositiveXNetDuringHyperStrike)
                        || (x > 0.0f && !sbShowPositiveXNetDuringHyperStrike))
                    {
                        iter->Next();
                        continue;
                    }
                }

                bool bHammer = false;
                bool bBall = false;
                if (pObject->IsDrawableModel())
                {
                    bHammer = (pObject->AsDrawableModel()->m_pModel->id == HammerModelID);
                    bBall = (pObject->AsDrawableModel()->m_pModel->id == BallModelID);
                }
                DrawableObject* ballDrawable = g_pBall->GetDrawableBall();
                if ((DrawableObject*)pObject->AsDrawableModel() == ballDrawable)
                {
                    iter->Next();
                    continue;
                }

                const u32 objectFlags = pObject->m_uObjectFlags;
                if (objectFlags & 0x80)
                {
                    iter->Next();
                    continue;
                }
                const bool bSkybox = !sbSkyboxRenderingDisabled && (pObject->m_uObjectCreationFlags & 0x100);
                if (sbStadiumRenderingDisabled && !bBall && !bHammer && !bSkybox)
                {
                    iter->Next();
                    continue;
                }

                if (objectFlags & 0x1)
                {
                    const nlMatrix4& world = pObject->GetWorldMatrix();
                    VitaCullEntry entry;
                    entry.object = pObject;
                    entry.position = world.GetTranslation();
                    entry.radius = pObject->m_fBoundingRadius;
                    entry.forceVisible = (objectFlags & 0x10) || bNoCull;
                    entry.visible = entry.forceVisible;
                    cullEntries.push_back(entry);
                    ++nSubmitted;
                }
                else
                {
                    pObject->m_translucency = 1.0f;
                }
                iter->Next();
            }

            if (!cullEntries.empty() && !bNoCull)
            {
                VitaCullJob job;
                job.entries = cullEntries.data();
                for (int plane = 0; plane < 6; ++plane)
                    job.planes[plane] = pWorld->m_frustumPlane[plane];
                (void)aurora::vita::parallel_for(cullEntries.size(), 24, VitaCullRange, &job);
            }

            for (size_t i = 0; i < cullEntries.size(); ++i)
            {
                VitaCullEntry& entry = cullEntries[i];
                DrawableObject* pObject = entry.object;
                if (entry.visible)
                {
                    if (pObject->m_uObjectCreationFlags & 0xF000)
                        DoTranslucency(pObject);
                    pObject->Draw();
                    if (g_bDrawBoundingSphere)
                        RenderBoundingSphere(pObject->GetWorldMatrix(), pObject->m_fBoundingRadius);
                    ++nDrawn;
                }
                else
                {
                    pObject->m_translucency = 1.0f;
                }
            }
            delete iter;
            iter = NULL;
        }
        else
        {
#endif
        while (iter->IsValid())
        {
            DrawableObject* pObject = iter->CurrentValue();
            if (sbIsHyperShootToScoreRenderingEnabled)
            {
                if ((pObject->GetWorldMatrix().e2[3][0] < 0.0f && sbShowPositiveXNetDuringHyperStrike)
                    || (pObject->GetWorldMatrix().e2[3][0] > 0.0f && !sbShowPositiveXNetDuringHyperStrike))
                {
                    iter->Next();
                    continue;
                }
            }
            {
                bool bHammer = false;
                DrawableObject* ballDrawable;
                bool bBall = false;
                if (pObject->IsDrawableModel())
                {
                    bHammer = (pObject->AsDrawableModel()->m_pModel->id == HammerModelID);
                    bBall = (pObject->AsDrawableModel()->m_pModel->id == BallModelID);
                }
                {
                    ballDrawable = g_pBall->GetDrawableBall();
                    if ((DrawableObject*)pObject->AsDrawableModel() == ballDrawable)
                    {
                        iter->Next();
                        continue;
                    }
                }
                {
                    u32 objectFlags = pObject->m_uObjectFlags;
                    if (objectFlags & 0x80)
                    {
                        iter->Next();
                        continue;
                    }
                    u8 bSkybox = 0;
                    if (!sbSkyboxRenderingDisabled && (pObject->m_uObjectCreationFlags & 0x100))
                        bSkybox = 1;
                    if (sbStadiumRenderingDisabled && !bBall && !bHammer && !bSkybox)
                    {
                        iter->Next();
                        continue;
                    }
                    if (objectFlags & 0x1)
                    {
                        static const bool bNoCull = getenv("STRIKERS_NO_CULL") != NULL;
                        bool bPassed = (objectFlags & 0x10)
                            || World_IsSphereInFrustumInline(pWorld, pObject->GetWorldMatrix(), pObject->m_fBoundingRadius);
                        if (bNoCull) bPassed = true;
                        if (bPassed)
                        {
                            if (pObject->m_uObjectCreationFlags & 0xF000)
                                DoTranslucency(pObject);
                            pObject->Draw();
                            if (g_bDrawBoundingSphere)
                            {
                                RenderBoundingSphere(pObject->GetWorldMatrix(), pObject->m_fBoundingRadius);
                            }
                            nDrawn++;
                        }
                        else
                        {
                            pObject->m_translucency = 1.0f;
                            if (pObject->m_translucency < 0.0f)
                                pObject->m_translucency = 0.0f;
                            if (pObject->m_translucency > 1.0f)
                                pObject->m_translucency = 1.0f;
                        }
                        nSubmitted++;
                    }
                    else
                    {
                        pObject->m_translucency = 1.0f;
                        if (pObject->m_translucency < 0.0f)
                            pObject->m_translucency = 0.0f;
                        if (pObject->m_translucency > 1.0f)
                            pObject->m_translucency = 1.0f;
                    }
                }
            }
            iter->Next();
        }
#if defined(PORT_VITA)
        }
#endif
    }
    else
    {
        while (iter->IsValid())
        {
            DrawableObject* pObject = iter->CurrentValue();
            if (pObject->m_uObjectFlags & 0x1)
            {
                if (pObject->m_uObjectCreationFlags & 0xF000)
                    DoTranslucency(pObject);
                pObject->Draw();
                if (g_bDrawBoundingSphere)
                {
                    RenderBoundingSphere(pObject->GetWorldMatrix(), pObject->m_fBoundingRadius);
                }
                nDrawn++;
            }
            else
            {
                pObject->m_translucency = 1.0f;
                if (pObject->m_translucency < 0.0f)
                    pObject->m_translucency = 0.0f;
                if (pObject->m_translucency > 1.0f)
                    pObject->m_translucency = 1.0f;
            }
            nSubmitted++;
            iter->Next();
        }
    }

    delete iter;

    PortBenchSetCullStats((unsigned int)nSubmitted,
                          (unsigned int)(nSubmitted >= nDrawn ? nSubmitted - nDrawn : 0));

    if (g_bDrawCullingInfo)
    {
        DrawCullingInformation(nSubmitted, nDrawn);
    }
}

static void RenderBoundingSphere(const nlMatrix4& matWorld, f32 fRadius)
{
    glModel* pSphere = glInventory.GetModel(nlStringHash("debug/sphere"));
    glModel* pNewModel = glModelDup(pSphere, true);
    nlMatrix4 m;
    m.SetIdentity();
    m.e2[3][0] = matWorld.e2[3][0];
    m.e2[3][1] = matWorld.e2[3][1];
    m.e2[3][2] = matWorld.e2[3][2];
    m.e2[3][3] = 1.0f;
    m.e2[0][0] = fRadius;
    m.e2[1][1] = fRadius;
    m.e2[2][2] = fRadius;
    glModelPacket* pPacket = pNewModel->packets;
    while (pPacket < (glModelPacket*)((u8*)pNewModel->packets + pNewModel->numPackets * 0x4A))
    {
        glSetRasterState(pPacket->state.raster, (eGLState)5, 1);
        uintptr_t matID = glAllocMatrix();
        if ((u32)(matID + 0x10000) != 0xFFFF)
            glSetMatrix(matID, m);
        pPacket->state.matrix = matID;
        pPacket->state.texture[0] = WhiteTexture;
        pPacket = (glModelPacket*)((u8*)pPacket + sizeof(glModelPacket));
    }
    glViewAttachModel((eGLView)7, pNewModel);
}

void World::DrawAdditionalBalls(DrawableObject* pObject)
{
    nlVector3 currentPosition;
    nlVector3 otherPosition;

    currentPosition = pObject->GetWorldMatrix().GetTranslation();
    otherPosition = currentPosition;
    otherPosition.x = -currentPosition.x;
    if (currentPosition.x > 0.0f)
    {
        pObject->Draw();
    }
    pObject->GetWorldMatrix().SetTranslation(otherPosition);
    pObject->Draw();
    pObject->GetWorldMatrix().GetTranslation() = currentPosition;
}

void World::DrawCullingInformation(int nNumSubmitted, int nNumDrawn)
{
    char szBuffer[255];
    f32 drawnPct = 100.0f * ((f32)nNumDrawn / (f32)nNumSubmitted);
    nlSNPrintf(szBuffer, sizeof(szBuffer), "%d submitted, %d culled, %d ( %0.2f%% )drawn", nNumSubmitted, nNumSubmitted - nNumDrawn, nNumDrawn, drawnPct);
    static int x = 10;
    static int y = 0;
    nlColour OtherColour = { 0xFF, 0xFF, 0xFF, 0xFF };
    glStateBundle state;
    glStateSave(state);
    glFontBegin(false);
    glFontPrint((eGLView)0x21, x, y, OtherColour, szBuffer);
    glFontEnd();
    glStateRestore(state);
}

/**
 * Offset/Address/Size: 0x3A8 | 0x8019506C | size: 0x8C
 */
DrawableObject* World::FindDrawableObject(unsigned long uHashID)
{
    DrawableObject** foundValue;
    bool found = m_drawableMap.FindGet(uHashID, &foundValue);
    if (found)
        return *foundValue;
    return NULL;
}

DrawableObject* World::FindDrawableObject(unsigned long uHashID, unsigned long uOCMask)
{
    DrawableObject* pObject = FindDrawableObject(uHashID);
    if (pObject != NULL && (pObject->m_uObjectCreationFlags & uOCMask) != 0)
    {
        return pObject;
    }
    return NULL;
}

/**
 * Offset/Address/Size: 0x31C | 0x80194FE0 | size: 0x8C
 */
HelperObject* World::FindHelperObject(unsigned long uHashID)
{
    HelperObject** foundValue;
    bool found = m_helperMap.FindGet(uHashID, &foundValue);
    if (found)
        return *foundValue;
    return NULL;
}

SkinnedAnimController* World::CreateUniqueSkinnedController(const char* szObjName, const char* szHierarchy)
{
    SkinnedAnimController* pController = new (nlMalloc(sizeof(SkinnedAnimController), 8, false)) SkinnedAnimController(szHierarchy, this);
    if (!AssignSkinnedController(pController, szObjName))
    {
        return NULL;
    }
    m_animControllerList.AddEnd(pController);
    return pController;
}

SkinnedAnimController* World::CreateGangedSkinnedController(const char* szHierarchy)
{
    SkinnedAnimController* pController = new (nlMalloc(sizeof(SkinnedAnimController), 8, false)) SkinnedAnimController(szHierarchy, this);
    pController->m_bIsGanged = true;
    m_animControllerList.AddEnd(pController);
    return pController;
}

unsigned char World::AssignSkinnedController(SkinnedAnimController* pController, const char* szObjName)
{
    DrawableSkinModel* pSkinModelObj = (DrawableSkinModel*)FindDrawableObject(GetHashIdForGenericName(szObjName));
    unsigned char result = false;
    if (pSkinModelObj != NULL && pController != NULL)
    {
        pSkinModelObj->m_pAnimController = pController;
        pController->m_pSkinModel = pSkinModelObj;
        pController->CreateGLSkinMesh(pSkinModelObj->m_pModel);
        result = true;
    }
    return result;
}

TMAnimController* World::CreateUniqueTransformController(DrawableObject* pRootObject, const char* szHierarchy)
{
    TMAnimController* pController = new (nlMalloc(sizeof(TMAnimController), 8, false)) TMAnimController(szHierarchy, this);
    pController->m_pRootModel = pRootObject;
    m_animControllerList.AddEnd(pController);
    return pController;
}

TMAnimController* World::CreateUniqueTransformController(const char* szObjName, const char* szHierarchy)
{
    DrawableObject* pRootObject = FindDrawableObject(GetHashIdForGenericName(szObjName), 0x20);
    TMAnimController* pController = NULL;
    if (pRootObject != NULL && szHierarchy != NULL)
    {
        pController = CreateUniqueTransformController(pRootObject, szHierarchy);
    }
    return pController;
}

/**
 * Offset/Address/Size: 0x290 | 0x80194F54 | size: 0x8C
 */
bool World::AddDrawableObject(unsigned long uHashID, DrawableObject* pDrawableObject)
{
    DrawableObject** ppValue = m_drawableMap.Add(uHashID, pDrawableObject);

    if (ppValue == NULL)
    {
        pDrawableObject->m_pWorldContext = this;
        return true;
    }
    else
    {
        return false;
    }
}

unsigned char World::RemoveDrawableObject(DrawableObject* pObject)
{
    World* pWorld = pObject->m_pWorldContext;
    if (pWorld == NULL)
    {
        return false;
    }
    unsigned long uHashID = pObject->GetHashID();
    AVLTreeNode* removedNode = pWorld->m_drawableMap.RemoveAVLNode(
        (AVLTreeNode**)&pWorld->m_drawableMap.m_Root, &uHashID, pWorld->m_drawableMap.m_NumElements);
    if (removedNode != NULL)
    {
        pWorld->m_drawableMap.DeleteEntry(pWorld->m_drawableMap.CastUp(removedNode));
        pWorld->m_drawableMap.m_NumElements--;
        return true;
    }
    return false;
}

/**
 * Offset/Address/Size: 0xA0 | 0x80194D64 | size: 0x1F0
 */
LightObject* World::GetShadowLight(const nlVector3& vPosition, float fRadius)
{
    typedef nlAVLTreeIterator<unsigned long, LightObject*, DefaultKeyCompare<unsigned long> > LightIterator;
    LightObject* pClosest = NULL;
    float fDistance = 1e30f;
    LightObject* pLight;

    LightIterator* iterator = m_lightMap.GetIterator();

    while (iterator->IsValid())
    {
        pLight = iterator->CurrentValue();

        if (pLight->m_emitFlags & 1)
        {
            nlVector3 vDelta;
            nlVec3Sub(vDelta, vPosition, pLight->m_worldPosition);
            float distSq = nlVec3LengthSquared(vDelta);
            if (distSq < fDistance)
            {
                fDistance = distSq;
                pClosest = pLight;
            }
        }

        iterator->Next();
    }

    delete iterator;
    return pClosest;
}

/**
 * Offset/Address/Size: 0x5C | 0x80194D20 | size: 0x44
 */
unsigned long World::GetHashIdForGenericName(const char* name) const
{
    nlStrNCpy<char>(
        m_WorldNamePrefix + m_WorldNameLength,
        name,
        (unsigned long)(sizeof(m_WorldNamePrefix) - m_WorldNameLength));
    return nlStringLowerHash(m_WorldNamePrefix);
}

/**
 * Offset/Address/Size: 0x0 | 0x80194CC4 | size: 0x5C
 */
int World::CompareNameToGenericName(const char* objName, const char* genericName)
{
    size_t genericNameLength = strlen(genericName);
    return nlStrNCmp<char>(objName + m_WorldNameLength, genericName, genericNameLength);
}

bool World::IsCaptainShootToScorePresentationOn() const
{
    if (g_pGame == NULL)
        return false;
    return g_pGame->mbCaptainShotToScoreOn;
}
