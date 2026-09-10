#include "Game/Transitions/ModelTransition.h"

#include "Game/SHierarchy.h"
#include "port/endian.h"
#include "NL/nlMath.h"
#include "NL/vmath.h"
#include "NL/nlMemory.h"
#include "NL/nlSlotPool.h"
#include "NL/nlFile.h"
#include "NL/nlPrint.h"
#include "NL/nlString.h"
#include "NL/nlAVLTree.h"

#include <stdlib.h>
#include "Game/SAnim/pnSAnimController.h"
#include "Game/Effects/EffectsGroup.h"
#include "Game/Effects/EmissionManager.h"
#include "NL/gl/gl.h"
#include "NL/gl/glModel.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glState.h"
#include "NL/gl/glView.h"
#include "NL/glx/glxLoadModel.h"
#include "NL/glx/glxDisplayList.h"
#include "Game/GL/gluMeshWriter.h"

eGLView ModeledScreenTransition::s_3DView = GLV_Transitions;
nlAVLTree<unsigned long, TransitionModelStore, DefaultKeyCompare<unsigned long> > ModeledScreenTransition::g_ModelInventory;

/**
 * Offset/Address/Size: 0x1E60 | 0x80203F24 | size: 0x31C
 */
static int GetNumLeafNodesInHierarchy(cSHierarchy& h, int node, int ret)
{
    if (h.GetNumChildren(node) == 0)
    {
        return ret + 1;
    }

    for (int i = 0; i < h.GetNumChildren(node); i++)
    {
        ret = GetNumLeafNodesInHierarchy(h, h.GetChild(node, i), ret);
    }

    return ret;
}

/**
 * Offset/Address/Size: 0x1BCC | 0x80203C90 | size: 0x294
 */
static void ShuffleIntoOutline(Vector<nlVector3, DefaultAllocator>& polygon)
{
    float min = 9999.0f;

    for (int i = 1; i < polygon.mSize; i++)
    {
        float dist = nlGetLengthSquared3D(polygon.mData[0].x - polygon.mData[i].x, polygon.mData[0].y - polygon.mData[i].y, polygon.mData[0].z - polygon.mData[i].z);

        if (dist < min)
        {
            nlVector3 tmp = polygon.mData[i];
            min = dist;
            polygon.mData[i] = polygon.mData[1];
            polygon.mData[1] = tmp;
        }
    }

    nlVector3 dir;

    for (int i = 1; i < polygon.mSize - 1; i++)
    {
        float max = 1.0f;
        nlRecipSqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z, true);

        int prev = i;
        prev -= 1;
        nlVec3Set(dir, polygon.mData[i].x - polygon.mData[prev].x, polygon.mData[i].y - polygon.mData[prev].y, polygon.mData[i].z - polygon.mData[prev].z);

        for (int j = i + 1; j < polygon.mSize; j++)
        {
            nlVector3 delta;
            nlVec3Sub(delta, polygon[i], polygon[j]);
            float recip = nlRecipSqrt(delta.GetLengthSq3D(), true);
            nlVec3Scale(delta, recip);
            float dot = nlVec3DotProduct(dir, delta);

            if (dot <= max)
            {
                max = dot;
                nlVector3 tmp = polygon[i + 1];
                polygon[i + 1] = polygon[j];
                polygon[j] = tmp;
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x188C | 0x80203950 | size: 0x340
 */
static int UpdateEffectsFromLeafNodes(cPoseAccumulator& pa, EmissionController** ecs, cSHierarchy& skeleton, int leaf, int node)
{
    if (skeleton.GetNumChildren(node) == 0)
    {
        EmissionController* ec = ecs[leaf];
        if (ec != NULL)
        {
            if (EmissionManager::IsStillAlive(ec))
            {
                ecs[leaf]->SetPosition(pa.GetNodeMatrix(node).GetTranslation());
            }
            else
            {
                ecs[leaf] = NULL;
            }
        }
        leaf++;
    }
    else
    {
        for (int i = 0; i < skeleton.GetNumChildren(node); i++)
        {
            leaf = UpdateEffectsFromLeafNodes(pa, ecs, skeleton, leaf, skeleton.GetChild(node, i));
        }
    }

    return leaf;
}

/**
 * Offset/Address/Size: 0x17E0 | 0x802038A4 | size: 0xAC
 */
ModeledScreenTransition::ModeledScreenTransition()
{
    m_pModels = NULL;
    m_nModels = 0;
    m_pSkeleton = NULL;
    m_pAnim = NULL;
    m_pSkelFile = NULL;
    m_pAnimFile = NULL;
    m_nTexture = 0xFFFFFFFF;
    m_pPoseAccumulator = NULL;
    m_pPoseTree = NULL;
    m_bScreenGrab = false;
    m_bEnableGrab = false;
    m_pModelMap = NULL;
    m_pLight = NULL;
    m_nProgram = glGetProgram("3d unlit");
    m_Effects = NULL;
    m_RenderOutline = false;
    m_mWorldMatrix.SetIdentity();
    m_EffectName[0] = '\0';
}

/**
 * Offset/Address/Size: 0x15C8 | 0x8020368C | size: 0x218
 */
ModeledScreenTransition::~ModeledScreenTransition()
{
    if (m_pPoseAccumulator != NULL)
    {
        delete m_pPoseAccumulator;
        m_pPoseAccumulator = NULL;
    }

    if (m_pLight != NULL)
    {
        for (u32 i = 0; i < m_nModels; i++)
        {
            m_pLight->DetachFromModel(&m_pModels[i]);
        }

        if (m_pLight != NULL)
        {
            delete m_pLight;
        }
        m_pLight = NULL;
    }

    if (m_pPoseTree != NULL)
    {
        delete m_pPoseTree;
        m_pPoseTree = NULL;
    }

    if (m_pSkeleton != NULL)
    {
        m_pSkeleton = NULL;
        delete[] m_pSkelFile;
        m_pSkelFile = NULL;
    }

    if (m_pAnim != NULL)
    {
        m_pAnim->Destroy();
        m_pAnim = NULL;
        delete[] m_pAnimFile;
        m_pAnimFile = NULL;
    }

    if (m_pModelMap != NULL)
    {
        delete[] m_pModelMap;
        m_pModelMap = NULL;
    }

    if (m_Effects != NULL)
    {
        delete[] m_Effects;
    }

    m_pModels = NULL;
    m_nModels = 0;
}

/**
 * Offset/Address/Size: 0x15C4 | 0x80203688 | size: 0x4
 */
void ModeledScreenTransition::DoSanityCheck()
{
}

/**
 * Offset/Address/Size: 0x1520 | 0x802035E4 | size: 0xA4
 */
void ModeledScreenTransition::Update(float deltaTime)
{
    if (m_pPoseTree != NULL)
    {
        bool skipUpdate = false;
        if (m_pPoseTree->m_ePlayMode == PM_HOLD && m_pPoseTree->m_fTime == 1.0f)
        {
            skipUpdate = true;
        }

        if (!skipUpdate)
        {
            m_pPoseTree->Update(deltaTime);
            m_pPoseAccumulator->Pose(*m_pPoseTree, m_mWorldMatrix);
        }
    }

    if (m_EffectName[0] != '\0')
    {
        UpdateEffectsFromLeafNodes(*m_pPoseAccumulator, m_Effects, *m_pSkeleton, 0, 0);
    }
}

static inline uintptr_t glAllocSetMatrix(const nlMatrix4& matrix)
{
    uintptr_t handle = glAllocMatrix();
    if (handle != 0xFFFFFFFF)
    {
        glSetMatrix(handle, matrix);
    }
    return handle;
}

/**
 * Offset/Address/Size: 0x13EC | 0x802034B0 | size: 0x134
 */
void ModeledScreenTransition::Render(eGLView view)
{
    if (m_pLight != NULL && m_pPoseTree != NULL)
    {
        m_pLight->ApplyLight(m_pPoseTree->m_fTime);
    }

    for (u32 i = 0; i < m_nModels; i++)
    {
        for (u32 j = 0; j < m_pModels[i].numPackets; j++)
        {
            m_pModels[i].packets[j].state.matrix = glAllocSetMatrix(m_pPoseAccumulator->GetNodeMatrix(m_pModelMap[i]));
        }

        glViewAttachModel(s_3DView, &m_pModels[i]);
    }

    if (m_bEnableGrab)
    {
        glViewSetFilter(GLV_ScreenGrab, GLFilter_Blt);
        glViewSetFilterSource(GLV_ScreenGrab, GLTG_Main);
        m_bEnableGrab = false;
    }
    else
    {
        glViewSetFilter(GLV_ScreenGrab, GLFilter_None);
    }

    if (m_RenderOutline)
    {
        RenderOutline();
    }
}

static inline void ClearOutline(Vector<nlVector3, DefaultAllocator>& outline);

static inline void ReserveOutline(Vector<nlVector3, DefaultAllocator>& outline, int capacity)
{
    if (outline.mCapacity < capacity)
    {
        Vector<nlVector3, DefaultAllocator> other(capacity, 0);
        for (int i = 0; i < outline.mSize; i++)
        {
            other.mData[i] = outline.mData[i];
        }
        other.mSize = outline.mSize;
        outline.Swap(other);
    }
}

static inline void InsertOutline(
    Vector<nlVector3, DefaultAllocator>& outline,
    nlVector3* at,
    const nlVector3* begin,
    const nlVector3* end)
{
    int size = end - begin;
    int offset = at - outline.mData;
    ReserveOutline(outline, outline.mSize + size);
    at = outline.mData + offset;
    nlVector3* t = outline.mData + outline.mSize - 1;
    while (t >= at)
    {
        *(t + size) = *t;
        t--;
    }
    while (begin != end)
    {
        *at = *begin;
        begin++;
        at++;
    }
    outline.mSize += size;
}

/**
 * Offset/Address/Size: 0x918 | 0x802029DC | size: 0xAD4
 */
void ModeledScreenTransition::RenderOutline() const
{
    Vector<nlVector3, DefaultAllocator> outline;
    nlVector3 current;
    outline.mData = NULL;
    outline.mSize = 0;
    outline.mCapacity = 0;
    ReserveOutline(outline, 8);

    for (int i = 0; (u32)i < m_nModels; i++)
    {
        for (int iPacket = 0; (u32)iPacket < m_pModels[i].numPackets; iPacket++)
        {
            const glModelPacket& packet = m_pModels[i].packets[iPacket];
            DisplayList* pList = dlGetStruct(packet.indexBuffer);

            for (int iVertex = 0; iVertex < (int)packet.numVertices; iVertex++)
            {
                const u16* p;
                if (((u16*)&pList->indices)[1] != 0)
                {
                    u16 ns = ((u16*)&pList->indices)[0];
                    int stride = (ns - 1) * 2 + 1;
                    int offset = stride * iVertex;
                    p = (u16*)((u8*)pList->list + offset);
                    p += 2;
                }
                else
                {
                    u16 ns = ((u16*)&pList->indices)[0];
                    int stride = ns * 2;
                    int offset = iVertex * stride;
                    u8* ptr8 = (u8*)pList->list;
                    ptr8 += offset;
                    p = (u16*)ptr8;
                    ptr8 = (u8*)p;
                    ptr8 += 3;
                    p = (u16*)ptr8;
                }

                // PORT: FIFO indices are big-endian.
                u16 vertex = port_be16(p);
                int index = vertex;
                const glModelStream& stream = packet.streams[0];

                if (stream.stride == 12)
                {
                    const u8* q = (const u8*)stream.address + index * stream.stride;
                    if (stream.beData)
                    {
                        current.x = port_bef32(q + 0);
                        current.y = port_bef32(q + 4);
                        current.z = port_bef32(q + 8);
                    }
                    else
                    {
                        memcpy(&current, (const void*)q, sizeof(nlVector3));
                    }
                }
                else
                {
                    // PORT: big-endian in a model file.
                    const u8* q = (const u8*)stream.address + index * stream.stride;
                    const s16* s = (const s16*)q;
                    s16 sx = stream.beData ? (s16)port_be16(q + 0) : s[0];
                    s16 sy = stream.beData ? (s16)port_be16(q + 2) : s[1];
                    s16 sz = stream.beData ? (s16)port_be16(q + 4) : s[2];
                    current.x = sx / 128.0f;
                    current.y = sy / 128.0f;
                    current.z = sz / 128.0f;
                }

                nlMultPosVectorMatrix(current, current, m_pPoseAccumulator->GetNodeMatrix(m_pModelMap[i]));

                InsertOutline(outline, outline.mData + outline.mSize, &current, &current + 1);
            }

            ShuffleIntoOutline(outline);

            GLMeshWriter mesh;
            glSetDefaultState(true);
            glSetCurrentMatrix(glGetIdentityMatrix());
            glSetCurrentTexture(glGetTexture("global/white"), GLTT_Diffuse);
            glSetCurrentProgram(glGetProgram("3d unlit"));
            glSetRasterState((eGLState)5, 1);
            glSetCurrentRasterState(glHandleizeRasterState());

            eGLStream streamDecl[3] = {
                GLStream_Position,
                GLStream_Colour,
                GLStream_Diffuse,
            };

            mesh.Begin(outline.mSize + 1, GLP_LineStrip, 3, streamDecl, false);

            for (int k = 0; k < outline.mSize; k++)
            {
                mesh.Colour(m_OutlineColour);
                nlVector2 uv;
                uv.x = 0.0f;
                uv.y = 0.0f;
                ((GLMeshWriterCore*)&mesh)->Texcoord(uv);
                mesh.Vertex(outline.mData[k]);
            }

            mesh.Colour(m_OutlineColour);
            nlVector2 uv;
            uv.x = 0.0f;
            uv.y = 0.0f;
            ((GLMeshWriterCore*)&mesh)->Texcoord(uv);
            mesh.Vertex(outline.mData[0]);

            if (mesh.End())
            {
                glViewAttachModel(GLV_Transitions3D, 2, mesh.GetModel());
            }

            ClearOutline(outline);
        }
    }
}

static inline void ClearOutline(Vector<nlVector3, DefaultAllocator>& outline)
{
    for (int i = 0; i < outline.mSize; i++)
    {
        outline.mData[i] = nlVector3();
    }
    outline.mSize = 0;
}

/**
 * Offset/Address/Size: 0x8DC | 0x802029A0 | size: 0x3C
 */
bool ModeledScreenTransition::IsFinished()
{
    if (!m_pPoseTree)
    {
        return true;
    }

    return m_pPoseTree->m_ePlayMode == PM_HOLD && m_pPoseTree->m_fTime == 1.0f;
}

/**
 * Offset/Address/Size: 0x8C0 | 0x80202984 | size: 0x1C
 */
float ModeledScreenTransition::Time() const
{
    if (m_pPoseTree != NULL)
    {
        return m_pPoseTree->m_fTime;
    }
    return 0.0f;
}

/**
 * Offset/Address/Size: 0x76C | 0x80202830 | size: 0x154
 */
void ModeledScreenTransition::Reset()
{
    cPN_SAnimController* poseTree;
    cPN_SAnimController* controller;

    if (m_pPoseTree == NULL && m_pAnim != NULL)
    {
        controller = AllocateSAnimController();
        controller = ::new (controller) cPN_SAnimController(m_pAnim, NULL, PM_HOLD, NULL, 0, false);
        m_pPoseTree = controller;
    }

    poseTree = m_pPoseTree;
    if (poseTree != NULL)
    {
        poseTree->m_fPrevTime = poseTree->m_fTime;
        poseTree->m_fTime = 0.0f;
    }

    m_bEnableGrab = m_bScreenGrab;

    if (m_EffectName[0] != '\0')
    {
        const int numLeafNodes = GetNumLeafNodesInHierarchy(*m_pSkeleton, 0, 0);
        if (m_Effects == NULL)
        {
            m_Effects = (EmissionController**)nlMalloc(numLeafNodes * sizeof(EmissionController*), 8, false);
        }

        for (int i = 0; i < numLeafNodes; i++)
        {
            m_Effects[i] = EmissionManager::Create(fxGetGroup(m_EffectName), 0);
            m_Effects[i]->m_GlView = GLV_Transitions3D;
        }
    }
}

/**
 * Offset/Address/Size: 0x70C | 0x802027D0 | size: 0x60
 */
void ModeledScreenTransition::Cancel()
{
    delete m_pPoseTree;
    m_pPoseTree = NULL;

    delete[] m_Effects;
    m_Effects = NULL;
}

static unsigned long GetParsedProgram(const char* pToken)
{
    char name[128];
    int i;
    nlStrNCpy(name, pToken, 128);
    for (i = 0; i < 128; i++)
    {
        if (name[i] == '\0')
            break;
        if (name[i] == '_')
            name[i] = ' ';
    }
    return glGetProgram(name);
}

/**
 * Offset/Address/Size: 0x44 | 0x80202108 | size: 0x6C8
 */
ModeledScreenTransition* ModeledScreenTransition::LoadFromParser(SimpleParser* parser)
{
    char* pToken = parser->NextToken(true);

    while (pToken != NULL)
    {
        if (nlStrCmp(pToken, "texture") == 0)
        {
            m_nTexture = glHash(parser->NextTokenOnLine(true));
        }
        else if (nlStrCmp(pToken, "name") == 0)
        {
            pToken = parser->NextTokenOnLine(true);
            Load(pToken);
        }
        else if (nlStrCmp(pToken, "screengrab") == 0)
        {
            m_bScreenGrab = true;
        }
        else if (nlStrCmp(pToken, "effect") == 0)
        {
            char* effect = parser->NextTokenOnLine(true);
            nlStrNCpy(m_EffectName, effect, 64);
        }
        else if (nlStrCmp(pToken, "outline") == 0)
        {
            m_RenderOutline = true;
        }
        else if (nlStrCmp(pToken, "outline_colour") == 0)
        {
            m_OutlineColour.c[0] = atoi(parser->NextTokenOnLine(true));
            m_OutlineColour.c[1] = atoi(parser->NextTokenOnLine(true));
            m_OutlineColour.c[2] = atoi(parser->NextTokenOnLine(true));
            m_OutlineColour.c[3] = atoi(parser->NextTokenOnLine(true));
        }
        else if (nlStrCmp(pToken, "program") == 0)
        {
            m_nProgram = GetParsedProgram(parser->NextTokenOnLine(true));
        }
        else if (nlStrCmp(pToken, "light") == 0)
        {
            m_pLight = new (nlMalloc(sizeof(TransitionLight), 8, false)) TransitionLight();
            m_pLight->LoadFromParser(parser);
        }
        else if (nlStrCmp(pToken, "end") == 0)
        {
            break;
        }

        pToken = parser->NextToken(true);
    }

    m_pPoseAccumulator = new (nlMalloc(sizeof(cPoseAccumulator), 8, false)) cPoseAccumulator(m_pSkeleton, false);

    FixupModel();

    // PORT: the decomp gave this a return type its body never writes.
    return this;
}

void ModeledScreenTransition::CreateInstance(TransitionModelStore& modelInfo)
{
    m_nModels = modelInfo.nModels;
    m_pModels = glModelDupArrayNoStreams(modelInfo.pModels, modelInfo.nModels, false, true);

    for (unsigned long j = 0; j < m_nModels; j++)
    {
        for (unsigned long i = 0; i < m_pModels[i].numPackets; i++)
        {
            m_pModels[j].packets[i].userData = 0;
        }
    }
}

void ModeledScreenTransition::Load(const char* szName)
{
    unsigned long fileSize = 0;
    TransitionModelStore* pModelStore;
    u32 hash;
    TransitionModelStore newStore;
    char buf[128];
    hash = glHash(szName);

    if (ModeledScreenTransition::g_ModelInventory.FindGet(hash, &pModelStore))
    {
        CreateInstance(*pModelStore);
    }
    else
    {
        glSetIgnoreDuplicateModels(true);

        nlSNPrintf(buf, 128, "transitions/%s.glg", szName);
        m_pModels = glLoadModel(buf, &m_nModels);

        glSetIgnoreDuplicateModels(false);

        newStore.pModels = m_pModels;
        newStore.nModels = m_nModels;
        ModeledScreenTransition::g_ModelInventory.Add(hash, newStore);
    }

    nlSNPrintf(buf, 128, "art/transitions/%s.sanim", szName);
    m_pAnimFile = (char*)nlLoadEntireFile(buf, &fileSize, 0x20, AllocateStart);
    m_pAnim = cSAnim::Initialize((nlChunk*)m_pAnimFile);

    nlSNPrintf(buf, 128, "art/transitions/%s.shier", szName);
    m_pSkelFile = (char*)nlLoadEntireFile(buf, &fileSize, 0x20, AllocateStart);
    m_pSkeleton = cSHierarchy::Initialize((nlChunk*)m_pSkelFile);

    m_pModelMap = (int*)nlMalloc(m_nModels * 4, 8, false);
    for (u32 i = 0; i < m_nModels; i++)
    {
        m_pModelMap[i] = m_pSkeleton->GetNodeIndexByID(m_pModels[i].id);
    }
}

void ModeledScreenTransition::FixupModel()
{
    u64 savedTextureState = glGetCurrentTextureState();
    u32 savedRasterState = glGetCurrentRasterState();

    for (u32 i = 0; i < m_nModels; i++)
    {
        if (m_pLight != NULL)
        {
            m_pLight->AttachToModel(&m_pModels[i]);
        }

        for (u32 j = 0; j < m_pModels[i].numPackets; j++)
        {
            glSetCurrentTextureState(m_pModels[i].packets[j].state.texturestate);
            glSetTextureState(GLTS_DiffuseWrap, 3);

            glSetCurrentRasterState(m_pModels[i].packets[j].state.raster);
            glSetRasterState(GLS_AlphaBlend, 0);
            glSetRasterState(GLS_Culling, 0);
            glSetRasterState(GLS_DepthTest, 1);
            glSetRasterState(GLS_DepthWrite, 1);

            glSetCurrentProgram(m_nProgram);

            if (m_nTexture != 0xFFFFFFFF)
            {
                m_pModels[i].packets[j].state.texture[0] = m_nTexture;
            }

            m_pModels[i].packets[j].state.raster = glHandleizeRasterState();

            u64 texHandle = glHandleizeTextureState();
            m_pModels[i].packets[j].state.texturestate = texHandle;

            m_pModels[i].packets[j].state.program = m_nProgram;
        }
    }

    glSetCurrentTextureState(savedTextureState);
    glSetCurrentRasterState(savedRasterState);
}

/**
 * Offset/Address/Size: 0x0 | 0x802020C4 | size: 0x44
 */
float ModeledScreenTransition::GetTransitionLength()
{
    if (m_pAnim == NULL)
    {
        return 0.0f;
    }
    return m_pAnim->m_nNumKeys / 30.0f;
}
