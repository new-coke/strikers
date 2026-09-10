#include "NL/glx/glxLoadModel.h"
#include "port/endian.h"
#include "NL/nlWare.h"
#include "dolphin/os.h"
extern "C" {
unsigned long port_bmd_swap_headers(void*, unsigned long);
unsigned long port_skin_swap(void*);
unsigned long port_bmd_packet_count(unsigned long);
unsigned long port_bmd_stream_count(unsigned long);
void port_bmd_convert_packets(void*, const void*, unsigned long);
void port_bmd_convert_streams(void*, const void*, unsigned long);
void port_bmd_stream_sizes(void*, unsigned long, unsigned long);
void port_bmd_swap_indices(void*, unsigned long);
unsigned long port_bmd_model_count(unsigned long);
void port_bmd_convert_models(void*, const void*, unsigned long, unsigned long);
}
#include "NL/glx/glxMemory.h"
#include "NL/gl/glMemory.h"
#include "NL/gl/glModel.h"
#include "NL/gl/glState.h"
#include "NL/gl/glTexture.h"
#include "NL/gl/glUserData.h"
#include "NL/glx/glxDisplayList.h"
#include "NL/glx/glxTexture.h"
#include "NL/nlFile.h"
#include "NL/nlFileGC.h"
#include "NL/nlString.h"
#include "NL/nlDLRing.h"
#include "NL/nlMemory.h"
#include "NL/platvmath.h"
#include "Game/GL/GLInventory.h"
#include "Game/GL/GLTextureAnim.h"
#include "Game/GL/GLVertexAnim.h"
#include "Game/GL/GLMaterial.h"
#include "Game/GL/ShaderSkinMesh.h"
#include "Game/SAnim.h"
#include "Game/Sys/debug.h"
#include "dolphin/os/OSCache.h"
#include <string.h>

extern GLInventory glInventory;

static bool glIgnoreDuplicateModels;

static glModel* glxLoadModelFromMemory(char* data, int size, unsigned long* pNumModels, bool bLoadTextures);

// BMD model chunk type IDs.
enum BMDChunkType
{
    BMD_CHUNK_FILE_INFO = 0x1B001,
    BMD_CHUNK_REF_DATA = 0x1B002,
    BMD_CHUNK_MODELS = 0x1B003,
    BMD_CHUNK_PACKETS = 0x1B004,
    BMD_CHUNK_STREAMS = 0x1B005,
    BMD_CHUNK_DISPLAY_LIST = 0x1B006,
    BMD_CHUNK_INDEX_DATA = 0x1B007,
    BMD_CHUNK_SKIN = (int)0x8001B008u,
    BMD_CHUNK_TEXTURE_ANIM = 0x1B00F,
    BMD_CHUNK_VERTEX_ANIM = 0x1B011,
    BMD_CHUNK_MATERIAL_LIST = 0x1B012,
};

static const int gl_stream_stride[15] = {
    12, 3, 4, 4, 4, 4, 4, 4, 4, 12, 12, 12, 1, 16, 16
};

static void* NLVIRTUALALLOC(unsigned long size)
{
    if (nlVirtualLargestBlock() >= size + 0x100)
    {
        return nlVirtualAlloc(size, false);
    }

    OSReport("VIRTUAL MEMORY WARNING ~ NLVIRTUALALLOC had to fall back to MRAM\nLargest block: %d Total free: %d\n",
        nlVirtualLargestBlock(),
        nlVirtualTotalFree());
    return nlMalloc(size, 0x20, false);
}

// Skin mesh chunk type IDs (lower 24 bits of m_ID); switch index = (type - 0x1B009).
enum SkinChunkType
{
    SKIN_CHUNK_0x1B009 = 0x1B009,
    SKIN_CHUNK_BONE_MATRICES = 0x1B00A,
    SKIN_CHUNK_BONE_MAP_LIST = 0x1B00B,
    SKIN_CHUNK_MORPH = 0x1B00C,
    SKIN_CHUNK_SOFTWARE_VERTICES = 0x1B00D,
    SKIN_CHUNK_SKIN_PAIRS = 0x1B00E,
    SKIN_CHUNK_0x1B00F = 0x1B00F,
    SKIN_CHUNK_STITCHING = 0x1B010,
};

/**
 * Offset/Address/Size: 0xF08 | 0x801C0B28 | size: 0x8
 */
void glSetIgnoreDuplicateModels(bool ignore)
{
    glIgnoreDuplicateModels = ignore;
}

/**
 * Offset/Address/Size: 0xC08 | 0x801C0828 | size: 0x2A0
 */
GLSkinMesh* glx_MakeSkinMesh(nlChunk* outerChunk, glModel* models)
{
    ShaderSkinMesh* mesh = new (nlMalloc(sizeof(ShaderSkinMesh), 8, false)) ShaderSkinMesh();

    mesh->pModel = models;

    u32 align;
    u32 i;
    u32 count;
    nlChunk* chunkEnd = (nlChunk*)((u8*)outerChunk + outerChunk->m_Size + 8);
    u32 chunkSize;
    u8* data;

    for (nlChunk* chunk = (nlChunk*)((u8*)outerChunk + 8); chunk != chunkEnd; chunk = (nlChunk*)((u8*)chunk + chunk->m_Size + 8))
    {
        u32 id = chunk->m_ID;
        chunkSize = chunk->m_Size;
        u32 alignBits = id & 0x7F000000;
        u32 chunkType = id & ~0x7F000000u;

        u8* result;
        if (((-alignBits | alignBits) >> 31) != 0)
        {
            align = 1 << (alignBits >> 24);
            uintptr_t ptr = (uintptr_t)chunk;
            ptr += align;
            ptr += 7;
            result = (u8*)(ptr & ~(uintptr_t)(align - 1));
        }
        else
        {
            result = (u8*)chunk + 8;
        }
        data = result;

        switch (chunkType)
        {
        case 0x1B009:
            break;
        case 0x1B00A:
        {
            count = chunkSize / 0x44;
            for (i = 0; i < count; i++)
            {
                u32 boneID = *(u32*)data;
                nlMatrix4 src;
                nlMatrix4 inv;
                memcpy(&src, data + 4, sizeof(nlMatrix4));
                data += 0x44;
                nlInvertMatrix(inv, src);
                mesh->SetBoneMatrix(boneID, &inv);
            }
            break;
        }
        case 0x1B00B:
        {
            BoneMapList* node = new (nlMalloc(sizeof(BoneMapList), 8, false)) BoneMapList;

            count = chunkSize >> 3;
            node->m_next = NULL;
            for (i = 0; i < count; i++)
            {
                unsigned long key = *(u32*)(data + 0);
                unsigned long value = *(u32*)(data + 4);
                data += 8;
                node->boneMap.Add(key, value);
            }
            nlRingAddEnd<BoneMapList>(&mesh->boneMaps, node);
            break;
        }
        case 0x1B00D:
            mesh->SetSoftwareVertices((int)(chunkSize >> 4), (const SkinVertex*)data);
            break;
        case 0x1B00E:
            mesh->AppendSkinPairList((int)(chunkSize >> 2), (const SkinPair*)data);
            break;
        case 0x1B00C:
        {
            u32 numMorphs = *(u32*)(data + 0);
            mesh->numMorphs = (int)numMorphs;
            mesh->numBaseVerts = *(u32*)(data + 4);
            data += 8;
            mesh->SetMorphIDs((const u32*)data);
            data += numMorphs * 4;
            mesh->SetMorphNumDeltas((const u32*)data);
            data += numMorphs * 4;
            mesh->SetMorphDeltas(*(int*)data, (const MorphDelta*)(data + 4));
            break;
        }
        case 0x1B00F:
            break;
        case 0x1B010:
            mesh->AppendStitchingInfo(*(int*)(data + 4), *(int*)(data + 0), (int)chunkSize - 8, data + 8);
            break;
        }
    }

    mesh->StitchModel();
    return mesh;
}

/**
 * Offset/Address/Size: 0x1D0 | 0x801BFDF0 | size: 0xA38
 */
static glModel* glxLoadModelFromMemory(char* data, int size, unsigned long* pNumModels, bool bLoadTextures)
{
    // PORT: big-endian; a chunk tree bmd_endian.c refuses would be walked out of bounds.
    if (port_bmd_swap_headers(data, (unsigned long)size) == 0)
    {
        OSReport("Error: model data is not a well-formed chunk tree\n");
        return NULL;
    }

    bool hasBmdHeader = false;
    nlChunk* innerEnd;
    u8* currentOuter;
    nlChunk* outerChunkPtr;
    nlChunk* outerEnd;
    nlChunk* chunkStart;
    nlChunk* chunkEnd;
    u32 numModels;
    int numPacketEntries;
    int numStreamEntries;
    uintptr_t refDataPtr;   // PORT: an address
    glModel* pModels;
    glModelPacket* pPackets;
    u8* pStreamData;
    u8* pDisplayListData;
    u32 vertexDataSize = 0;
    u8* pIndexData;
    bool hasSkinData;
    nlChunk* chunk;

    outerChunkPtr = (nlChunk*)data;
    outerEnd = (nlChunk*)(data + size);
    chunkStart = outerChunkPtr;
    chunkEnd = outerEnd;

    if ((*(u32*)outerChunkPtr & ~0x7F000000u) == 0x8001B100u)
    {
        u32 innerSize = outerChunkPtr->m_Size;
        chunkStart = (nlChunk*)((u8*)outerChunkPtr + 8);
        chunkEnd = (nlChunk*)((u8*)outerChunkPtr + innerSize + 8);
        hasBmdHeader = true;
    }

    hasSkinData = false;

    while (chunkStart < chunkEnd)
    {
        if (hasBmdHeader)
        {
            nlChunk* topChunk = (nlChunk*)chunkStart;
            outerChunkPtr = chunkStart;
            outerEnd = (nlChunk*)((u8*)chunkStart + topChunk->m_Size + 8);
        }

        while (outerChunkPtr < outerEnd)
        {
            currentOuter = (u8*)outerChunkPtr;
            chunk = (nlChunk*)(currentOuter + 8);
            innerEnd = (nlChunk*)(currentOuter + outerChunkPtr->m_Size + 8);

            while (chunk != innerEnd)
            {
                u32 rawId = chunk->m_ID;
                u32 chunkSize = chunk->m_Size;
                u32 alignBits = rawId & 0x7F000000u;
                int id = (int)(rawId & ~0x7F000000u);
                u8* chunkData;
                if (((-alignBits | alignBits) >> 31) != 0)
                {
                    u32 align = 1u << (alignBits >> 24);
                    chunkData = (u8*)nlAlignUp((uintptr_t)(chunk + 1), align);
                }
                else
                {
                    chunkData = (u8*)chunk + 8;
                }

                switch (id)
                {
                case BMD_CHUNK_FILE_INFO:
                    break;
                case BMD_CHUNK_REF_DATA:
                {
                    void* p = glResourceAlloc(chunkSize, GLM_Matrix);
                    refDataPtr = (uintptr_t)p;
                    memcpy(p, chunkData, chunkSize);
                    // PORT: matrices, and the file is big-endian.
                    port_be32_array(p, chunkSize / 4);
                    break;
                }
                case BMD_CHUNK_MODELS:
                {
                    numModels = chunkSize >> 4;
                    if (pNumModels != NULL)
                        *pNumModels = numModels;
                    pModels = (glModel*)glResourceAlloc(
                        numModels * sizeof(glModel), GLM_Header);
                    port_bmd_convert_models(pModels, chunkData, numModels,
                                            sizeof(glModel));
                    {
                        glModel* pEnt = pModels;
                        glModel* pEntEnd = pModels + numModels;
                        for (; pEnt < pEntEnd; pEnt++)
                        {
                            if (glIgnoreDuplicateModels)
                            {
                                if (glInventory.GetModel(pEnt->id) != NULL)
                                {
                                    pEnt++;
                                    continue;
                                }
                            }
                            glInventory.AddModel(pEnt->id, pEnt);
                        }
                    }
                    break;
                }
                case BMD_CHUNK_PACKETS:
                {
                    numPacketEntries = (int)port_bmd_packet_count(chunkSize);
                    pPackets = (glModelPacket*)glResourceAlloc(
                        numPacketEntries * sizeof(glModelPacket), GLM_Header);
                    port_bmd_convert_packets(pPackets, chunkData, numPacketEntries);
                    break;
                }
                case BMD_CHUNK_STREAMS:
                {
                    numStreamEntries = (int)port_bmd_stream_count(chunkSize);
                    pStreamData = (u8*)glResourceAlloc(
                        numStreamEntries * sizeof(glModelStream), GLM_Header);
                    port_bmd_convert_streams(pStreamData, chunkData, numStreamEntries);
                    break;
                }
                case BMD_CHUNK_DISPLAY_LIST:
                {
                    vertexDataSize = chunkSize;
                    pDisplayListData = (u8*)glResourceAlloc(chunkSize, GLM_VertexData);
                    memcpy(pDisplayListData, chunkData, chunkSize);
                    DCFlushRange(pDisplayListData, chunkSize);
                    break;
                }
                case BMD_CHUNK_INDEX_DATA:
                {
                    pIndexData = (u8*)nlMalloc(chunkSize, 8, true);
                    memcpy(pIndexData, chunkData, chunkSize);
                    port_bmd_swap_indices(pIndexData, chunkSize);
                    DCFlushRange(pIndexData, chunkSize);
                    break;
                }
                case BMD_CHUNK_TEXTURE_ANIM:
                {
                    // PORT: 4-byte big-endian fields.
                    const u8* p32 = (const u8*)chunkData;
                    unsigned long canonID = port_be32(p32);
                    p32 += 4;
                    if (glInventory.GetTextureAnim(canonID) == NULL)
                    {
                        unsigned long num = port_be32(p32);
                        p32 += 4;
                        unsigned long mode = port_be32(p32);
                        p32 += 4;
                        float start = port_bef32(p32);
                        p32 += 4;
                        GLTextureAnim* pAnim = new (nlMalloc(sizeof(GLTextureAnim), 8, false)) GLTextureAnim();
                        pAnim->m_unk_0x00 = (s32)canonID;
                        pAnim->SetNumTextures(num);
                        pAnim->m_mode = mode;
                        pAnim->SetFrame((int)start);
                        unsigned long index;
                        GLAnimTex animTex;
                        for (index = 0; index < num; index++)
                        {
                            unsigned long hashID = port_be32(p32);
                            p32 += 4;
                            animTex.textureHandle = hashID;
                            animTex.time = port_bef32(p32);
                            p32 += 4;
                            pAnim->SetTexture(index, animTex);
                        }
                        glInventory.AddTextureAnim(canonID, pAnim);
                    }
                    else
                    {
                        tDebugPrintManager::Print(DC_LOADER, "skipping duplicate texanim 0x%08X\n", canonID);
                    }
                    break;
                }
                case BMD_CHUNK_VERTEX_ANIM:
                {
                    // PORT: 4-byte big-endian fields.
                    const u8* p32 = (const u8*)chunkData;
                    unsigned long hashID = port_be32(p32 + 0);
                    unsigned long numFrames = port_be32(p32 + 4);
                    unsigned long numVerts = port_be32(p32 + 8);
                    unsigned long fps = port_be32(p32 + 12);
                    p32 += 16;
                    GLVertexAnim* pAnim = new (nlMalloc(sizeof(GLVertexAnim), 8, false)) GLVertexAnim();
                    pAnim->m_uHashID = hashID;
                    pAnim->m_nNumFrames = numFrames;
                    pAnim->m_nNumVertices = numVerts;
                    pAnim->m_fFrameRate = (float)fps;
                    unsigned long size = numFrames * 12 * numVerts;
                    nlVector3* pVertices = (nlVector3*)glResourceAlloc(size, GLM_VertexData);
                    memcpy(pVertices, p32, size);
                    // PORT: and the vertices themselves are big-endian floats.
                    port_be32_array(pVertices, size / 4);
                    DCFlushRange(pVertices, size);
                    pAnim->m_pVertices = pVertices;
                    pAnim->m_pModel = glInventory.GetModel(hashID);
                    pAnim->Reset();
                    glInventory.AddVertexAnim(hashID, pAnim);
                    break;
                }
                case BMD_CHUNK_MATERIAL_LIST:
                {
                    // PORT: 4-byte big-endian fields.
                    u8* p32 = (u8*)chunkData;
                    unsigned long modelID = port_be32(p32 + 0);
                    unsigned long numMats = port_be32(p32 + 4);
                    p32 += 8;
                    // GLMaterialEntry is three u32s on both sides, so the array needs converting but not reshaping.
                    port_be32_array(p32, numMats * 3);
                    GLMaterialList* pList = new (nlMalloc(sizeof(GLMaterialList), 8, false)) GLMaterialList();
                    pList->m_uHashID = modelID;
                    pList->SetMaterials(numMats, (const GLMaterialEntry*)p32);
                    glInventory.AddMaterialList(modelID, pList);
                    break;
                }
                case BMD_CHUNK_SKIN:
                {
                    u32 skinSize = chunkSize + 8;
                    nlChunk* pSkinChunk = (nlChunk*)NLVIRTUALALLOC(skinSize);
                    memcpy(pSkinChunk, chunk, skinSize);
                    // PORT: big-endian payloads, converted once where the chunk is copied; a chunk
                    // skin_endian.c refuses would be read out of bounds, so the model has no skin data.
                    if (port_skin_swap(pSkinChunk) == 0)
                    {
                        OSReport("Error: SKIN chunk of model %lu is not well-formed; ignored\n", (unsigned long)pModels->id);
                        break;
                    }
                    // PORT: see rw_glinventory_skinprobe.
                    if (getenv("STRIKERS_PROBE_SKIN"))
                        OSReport("[skin] ADD id=%lu pModels=%p numModels=%lu\n",
                                 (unsigned long)pModels->id, (void*)pModels,
                                 (unsigned long)numModels);
                    glInventory.AddSkinData(pModels->id, pSkinChunk);
                    hasSkinData = true;
                    break;
                }
                default:
                    break;
                }

                chunk = (nlChunk*)((u8*)chunk + chunk->m_Size + 8);
            }

            outerChunkPtr = (nlChunk*)(currentOuter + outerChunkPtr->m_Size + 8);

            {
                int count = numModels;
                glModel* pM = pModels;
                while (count > 0)
                {
                    pM->packets = (glModelPacket*)((uintptr_t)pM->packets + (uintptr_t)pPackets);
                    pM++;
                    count--;
                }
            }

            {
                int i;
                glModelPacket* pPkt = pPackets;
                for (i = 0; i < numPacketEntries; i++)
                {
                    if (glGetRasterState(pPkt->state.raster, (eGLState)5) == 0)
                    {
                        if (glTextureLoad(pPkt->state.texture[0]))
                        {
                            glUnHandleizeRasterState(pPkt->state.raster);
                            int bits = glTextureGetNumBits(3);
                            if (bits == 1)
                            {
                                glSetRasterState((eGLState)5, 0);
                                glSetRasterState((eGLState)3, 1);
                                glSetRasterState((eGLState)4, 0x40);
                            }
                            else if (bits > 1)
                            {
                                glSetRasterState((eGLState)5, 1);
                                glSetRasterState((eGLState)3, 1);
                                glSetRasterState((eGLState)4, 0);
                                glSetRasterState((eGLState)1, 0);
                            }
                            pPkt->state.raster = glHandleizeRasterState();
                        }
                    }
                    pPkt->streams = (glModelStream*)((uintptr_t)pPkt->streams + (uintptr_t)pStreamData);
                    pPkt->indexBuffer += (uintptr_t)pIndexData;
                    pPkt->state.matrix += refDataPtr;
                    pPkt = (glModelPacket*)((u8*)pPkt + sizeof(glModelPacket));
                }
            }

            {
                // PORT: was `*(u32*)p += ...; p += 6`, glModelStream's on-disc layout.
                glModelStream* pStream = (glModelStream*)pStreamData;
                // PORT: while the addresses are still offsets.
                port_bmd_stream_sizes(pStream, numStreamEntries, vertexDataSize);
                for (int count = 0; count < numStreamEntries; count++)
                {
                    pStream[count].address += (uintptr_t)pDisplayListData;
                }
            }

            {
                glModel* pModel = pModels;
                // PORT: was bounded by numModels shifted left 4, glModel's size on disc; the host record is wider.
                glModel* pModelEnd = pModels + numModels;
                while (pModel < pModelEnd)
                {
                    glModelPacket* pPacket = pModel->packets;
                    while (pPacket < pModel->packets + pModel->numPackets)
                    {
                        if (hasSkinData)
                        {
                            if (glGetRasterState(pPacket->state.raster, (eGLState)8) == 1)
                            {
                                int oldNumStreams = pPacket->numStreams;
                                int newNum = oldNumStreams + 1;
                                glModelStream* streams = (glModelStream*)glResourceAlloc(newNum * sizeof(glModelStream), GLM_Header);
                                memcpy(streams, pPacket->streams, oldNumStreams * sizeof(glModelStream));
                                streams[oldNumStreams].id = 12;
                                streams[oldNumStreams].address = 0;
                                streams[oldNumStreams].stride = (u8)gl_stream_stride[12];
                                pPacket->numStreams = (u8)(pPacket->numStreams + 1);
                                pPacket->streams = streams;
                            }
                        }
                        if (pPacket->indexBuffer != 0)
                        {
                            pPacket->indexBuffer = (uintptr_t)dlMakeDisplayList(pPacket, true);
                        }
                        if (bLoadTextures)
                        {
                            for (int s = 0; s < 6; s++)
                            {
                                if (pPacket->state.texconfig & (1 << s))
                                {
                                    uintptr_t texHandle = pPacket->state.texture[s];
                                    // PORT: a fallback reported at draw time is just a hash; here it can still be tied to the model that asked for it.
                                    {
                                        static const bool bProbe = getenv("STRIKERS_PROBE_TEX") != NULL;
                                        if (bProbe && !glTextureLoad(texHandle))
                                        {
                                            static u32 seen[64];
                                            static int nSeen = 0;
                                            bool bNew = true;
                                            for (int q = 0; q < nSeen; q++)
                                                if (seen[q] == texHandle)
                                                    bNew = false;
                                            if (bNew && nSeen < 64)
                                            {
                                                seen[nSeen++] = texHandle;
                                                OSReport("[texload] model id 0x%08x wants texture "
                                                         "0x%08x (stage %d), not loaded\n",
                                                         (unsigned)pModel->id,
                                                         (unsigned)texHandle, s);
                                            }
                                        }
                                    }
                                    if (glInventory.GetTextureAnim(texHandle) == NULL)
                                    {
                                        if (glTextureLoad(texHandle))
                                        {
                                            pPacket->state.texture[s] = (uintptr_t)glx_GetTex(texHandle, true, true);
                                        }
                                    }
                                }
                            }
                        }
                        pPacket = (glModelPacket*)((u8*)pPacket + sizeof(glModelPacket));
                    }
                    pModel++;
                }
                nlFree(pIndexData);
            }
        }

        if (!hasBmdHeader)
            break;

        {
            nlChunk* topChunk = (nlChunk*)chunkStart;
            chunkStart = (nlChunk*)((u8*)chunkStart + topChunk->m_Size + 8);
        }
    }

    nlFree(data);
    return pModels;
}

static glModel* glxLoadModelFromDisk(const char* filename, unsigned long* pNumModels)
{
    unsigned int alignSize;
    unsigned int fileSize;
    char fullname[256];
    glModel* retval;
    nlFile* f;

    glx_FreeMemory0();
    nlStrNCat(fullname, "art/", filename, 256);

    f = nlOpen(fullname);
    if (f == NULL)
    {
        retval = NULL;
    }
    else
    {
        fileSize = nlFileSize(f, &alignSize);
        nlClose(f);
        if (fileSize == 0)
        {
            retval = NULL;
        }
        else
        {
            char* data = (char*)nlLoadEntireFileToVirtualMemory(fullname, (int*)&fileSize, 0x80000, NULL, AllocateEnd);
            if (data == NULL)
            {
                retval = NULL;
            }
            else
            {
                retval = glxLoadModelFromMemory(data, fileSize, pNumModels, true);
            }
        }
    }

    glx_FreeMemory1(filename);
    return retval;
}

/**
 * Offset/Address/Size: 0xCC | 0x801BFCEC | size: 0x104
 */
glModel* glplatLoadModel(const char* filename, unsigned long* pNumModels)
{
    unsigned int alignSize;
    unsigned int fileSize;
    char fullname[256];
    char lowerName[256];
    glModel* retval;
    nlFile* f;
    bool bSkinned;

    glx_FreeMemory0();
    nlStrNCat(fullname, "art/", filename, 256);
    nlStrNCpy(lowerName, fullname, 256);
    nlToLower(lowerName);

    bSkinned = (strstr(lowerName, "characters") == NULL);

    f = nlOpen(fullname);
    if (f == NULL)
    {
        retval = NULL;
    }
    else
    {
        fileSize = nlFileSize(f, &alignSize);
        nlClose(f);
        if (fileSize == 0)
        {
            retval = NULL;
        }
        else
        {
            char* data = (char*)nlLoadEntireFileToVirtualMemory(fullname, (int*)&fileSize, 0x80000, NULL, AllocateEnd);
            retval = glxLoadModelFromMemory(data, fileSize, pNumModels, bSkinned);
        }
    }

    glx_FreeMemory1(filename);
    return retval;
}

/**
 * Offset/Address/Size: 0x24 | 0x801BFC44 | size: 0xA8
 */
bool glplatBeginLoadModel(const char* filename, void (*callback)(void*, unsigned long, void*), void* userData)
{
    char fullname[256];
    nlStrNCat(fullname, "art/", filename, 256);

    if (userData == NULL)
    {
        if (!nlLoadEntireFileAsync(fullname, callback, userData, 32, AllocateEnd))
        {
            return false;
        }
    }
    else
    {
        if (!nlLoadEntireFileAsync(fullname, callback, userData, 32, (eAllocType)23))
        {
            return false;
        }
    }
    return true;
}

/**
 * Offset/Address/Size: 0x0 | 0x801BFC20 | size: 0x24
 */
glModel* glplatEndLoadModel(void* data, unsigned long size, unsigned long* pNumModels)
{
    return glxLoadModelFromMemory((char*)data, size, pNumModels, false);
}
