#include "NL/gl/glRenderList.h"
#include "NL/nlAVLTree.h"
#include "NL/nlDLListContainer.h"

class TextureTreeCompare
{
public:
    int operator()(const glModelPacket* const& b, const glModelPacket* const& a) const
    {
        const glModelPacket* keyB = b;
        const glModelPacket* keyA = a;
        if (keyA->state.program < keyB->state.program)
            return -1;
        if (keyA->state.program > keyB->state.program)
            return 1;
        if (keyA->state.texconfig < keyB->state.texconfig)
            return -1;
        if (keyA->state.texconfig > keyB->state.texconfig)
            return 1;
        if (keyA->state.texture[0] < keyB->state.texture[0])
            return -1;
        if (keyA->state.texture[0] > keyB->state.texture[0])
            return 1;
        if (keyA->userData < keyB->userData)
            return -1;
        if (keyA->userData > keyB->userData)
            return 1;
        if (keyA < keyB)
            return -1;
        if (keyA > keyB)
            return 1;
        return 0;
    }
};

struct DepthPacketPair
{
    /* 0x0 */ unsigned long sortKey;
    /* 0x4 */ const glModelPacket* packet;
}; // total size: 0x8

inline bool operator==(const DepthPacketPair& a, const DepthPacketPair& b)
{
    return a.sortKey == b.sortKey;
}

inline bool operator<(const DepthPacketPair& a, const DepthPacketPair& b)
{
    return a.sortKey < b.sortKey;
}

class DepthTreeCompare
{
public:
    int operator()(const DepthPacketPair& b, const DepthPacketPair& a) const
    {
        if (b.sortKey < a.sortKey)
            return -1;
        if (b.sortKey > a.sortKey)
            return 1;
        if (a.packet < b.packet)
            return -1;
        if (a.packet > b.packet)
            return 1;
        return 0;
    }
};

class GLTexturePacketTree : public nlAVLTreeSlotPool<const glModelPacket*, unsigned int, TextureTreeCompare>
{
public:
    GLTexturePacketTree()
        : nlAVLTreeSlotPool<const glModelPacket*, unsigned int, TextureTreeCompare>()
    {
    }

    GLTexturePacketTree(int initial, int delta)
        : nlAVLTreeSlotPool<const glModelPacket*, unsigned int, TextureTreeCompare>(initial, delta)
    {
    }
};

class GLDepthPacketTree : public nlAVLTreeSlotPool<DepthPacketPair, unsigned int, DepthTreeCompare>
{
public:
    GLDepthPacketTree()
        : nlAVLTreeSlotPool<DepthPacketPair, unsigned int, DepthTreeCompare>()
    {
    }

    GLDepthPacketTree(int initial, int delta)
        : nlAVLTreeSlotPool<DepthPacketPair, unsigned int, DepthTreeCompare>(initial, delta)
    {
    }
};

class GLPacketList : public nlDLListSlotPool<const glModelPacket*>
{
};

class PacketCallbackManager
{
public:
    /* 0x00 */ eGLView m_View;
    /* 0x04 */ void (*m_Cb)(eGLView, unsigned long, const glModelPacket*);
    /* 0x08 */ unsigned long m_LastProgram;
    /* 0x0C */ unsigned long m_LastRaster;
    /* 0x10 */ unsigned long long m_LastTextureState;
    /* 0x18 */ unsigned long m_LastMatrix;
    /* 0x1C */ unsigned long m_LastTexconfig;
    /* 0x20 */ unsigned long m_LastUserdata;
    /* 0x24 */ unsigned long m_LastNumStreams;
    /* 0x28 */ glModelStream* m_LastStreams;
    /* 0x2C */ unsigned long m_LastTexture[6];
    /* 0x44 */ unsigned long m_LastUserStateKey;
    /* 0x48 */ unsigned long m_LastMaterialSet;

    glModelStream* GetLastStreams() const { return m_LastStreams; }

    void ListCallback(const glModelPacket**);
    void DepthCallback(const DepthPacketPair&, unsigned int*);
    void TexCallback(const glModelPacket* const&, unsigned int*);
    void DoCallback(const glModelPacket*, unsigned int);
};

#include "NL/WalkHelper.h"
#include "Game/GL/GLRenderBuffer.h"
#include "NL/gl/glAppAttach.h"
#include "NL/gl/glMatrix.h"
#include "port/endian.h"
#include "NL/gl/glModify.h"
#include "NL/platvmath.h"
#include "NL/nlSlotPool.h"

static inline unsigned long PackMatrixDepth(const glModelPacket* pPacket, const nlMatrix4& view, unsigned long seq)
{
    nlMatrix4 packetMatrix;
    nlVector3 out;
    glGetMatrix(pPacket->state.matrix, packetMatrix);
    nlMultPosVectorMatrix(out, *(nlVector3*)&packetMatrix.e2[3][0], view);
    return ((unsigned long)(int)(-out.z * 100.0f) << 12) | (seq & 0xFFF);
}

static inline unsigned long PackStreamDepth(const glModelPacket* pPacket, const nlMatrix4& view)
{
    nlMatrix4 packetMatrix;
    nlVector3 out;
    glGetMatrix(pPacket->state.matrix, packetMatrix);
    // PORT: the stream may be big-endian and may be s16, neither of which this cast survives.
    const glModelStream* pStream = pPacket->streams;
    const u8* q = (const u8*)pStream->address;
    nlVector3 first;
    if (pStream->stride == 12)
    {
        if (pStream->beData)
        {
            first.x = port_bef32(q + 0);
            first.y = port_bef32(q + 4);
            first.z = port_bef32(q + 8);
        }
        else
        {
            first = *(const nlVector3*)q;
        }
    }
    else
    {
        const s16* s = (const s16*)q;
        first.x = (float)(pStream->beData ? (s16)port_be16(q + 0) : s[0]);
        first.y = (float)(pStream->beData ? (s16)port_be16(q + 2) : s[1]);
        first.z = (float)(pStream->beData ? (s16)port_be16(q + 4) : s[2]);
    }
    nlMultPosVectorMatrix(out, first, view);
    return (unsigned long)(int)(-out.z * 2147483648.0f);
}

extern GLRenderBuffer glRenderBuffer;

struct PacketCallbackManagerLayout
{
    /* 0x00 */ eGLView m_View;
    /* 0x04 */ glViewPacketCallback m_Cb;
    /* 0x08 */ unsigned long m_LastProgram;
    /* 0x0C */ unsigned long m_LastRaster;
    /* 0x10 */ unsigned long long m_LastTextureState;
    /* 0x18 */ unsigned long m_LastMatrix;
    /* 0x1C */ unsigned long m_LastTexconfig;
    /* 0x20 */ unsigned long m_LastUserdata;
    /* 0x24 */ unsigned long m_LastNumStreams;
    /* 0x28 */ glModelStream* m_LastStreams;
    /* 0x2C */ unsigned long m_LastTexture[6];
    /* 0x44 */ unsigned long m_LastUserStateKey;
    /* 0x48 */ unsigned long m_LastMaterialSet;
}; // total size: 0x50

static const unsigned long glv_UserDataChanged = 0x100;
static const unsigned long glv_MaterialChanged = 0x400;
static const unsigned long glv_UserStateKeyChanged = 0x200;
static const unsigned long glv_RasterChanged = 0x08;
static const unsigned long glv_TextureStateChanged = 0x10;
static const unsigned long glv_MatrixChanged = 0x20;
static const unsigned long glv_TexConfigChanged = 0x80;
static const unsigned long glv_TextureChanged = 0x4;
static const unsigned long glv_StreamsChanged = 0x40;

typedef WalkHelper<const glModelPacket*, DLListEntry<const glModelPacket*>, PacketCallbackManager> GLRenderPacketWalkHelper;
typedef void (GLRenderPacketWalkHelper::*GLRenderPacketWalkCallback)(DLListEntry<const glModelPacket*>*);
typedef void (PacketCallbackManager::*GLRenderPacketListCallback)(const glModelPacket**);

static unsigned char StreamsDiffer(const glModelPacket* packet, unsigned long num_prev, const glModelStream* prev)
{
    if (num_prev != packet->numStreams)
    {
        return 1;
    }

    const glModelStream* cur = packet->streams;
    const glModelStream* last = prev;

    for (int i = 0; i < packet->numStreams; i++)
    {
        if (cur->address != last->address)
        {
            return 1;
        }
        last++;
        cur++;
    }

    return 0;
}

/**
 * Offset/Address/Size: 0xC08 | 0x801D9EC8 | size: 0x1B0
 */
GLRenderList::GLRenderList()
{
    int layer;
    uDepthInsertNumber = 0;

    for (layer = 0; layer < 7; layer++)
    {
        GLTexturePacketTree* tree = new (8, false) GLTexturePacketTree(16, 16);
        texPacketTree[layer] = tree;
    }

    GLDepthPacketTree* dTree = new (8, false) GLDepthPacketTree(16, 16);
    depthPacketTree = dTree;

    GLPacketList* list = new (8, false) GLPacketList();
    packetList = list;
}

GLRenderList::~GLRenderList()
{
    for (int layer = 0; layer < 7; layer++)
    {
        delete texPacketTree[layer];
    }

    delete depthPacketTree;
    delete packetList;
}

/**
 * Offset/Address/Size: 0xB68 | 0x801D9E28 | size: 0xA0
 */
typedef DLListContainerBase<const glModelPacket*, BasicSlotPool<DLListEntry<const glModelPacket*> > > GLPacketListBase;

/**
 * Offset/Address/Size: 0xB68 | 0x801D9E28 | size: 0xA0
 */
void GLRenderList::Clear()
{
    GLPacketListBase* pList;
    int i;
    for (i = 0; i < 7; i++)
    {
        texPacketTree[i]->Clear();
    }
    depthPacketTree->Clear();

    pList = packetList;
    nlWalkDLRing(pList->m_Head, pList, &GLPacketListBase::DeleteEntry);
    pList->m_Head = NULL;
    uDepthInsertNumber = 0;
}

/**
 * Offset/Address/Size: 0xAE8 | 0x801D9DA8 | size: 0x80
 */
void GLRenderList::Compact()
{
    Clear();

    for (int i = 0; i < 7; i++)
    {
        texPacketTree[i]->m_Allocator.FreeBlocks();
    }

    depthPacketTree->m_Allocator.FreeBlocks();
    packetList->m_Allocator.FreeBlocks();
}

/**
 * Offset/Address/Size: 0x9E0 | 0x801D9CA0 | size: 0x108
 */
bool GLRenderList::IsEmpty() const
{
    if (sortMode == GLVSort_Texture)
    {
        for (int layer = 0; layer < 7; layer++)
        {
            if (texPacketTree[layer]->m_Root != NULL)
            {
                return false;
            }
        }
        return true;
    }
    if (sortMode == GLVSort_TransformedDepth || sortMode == GLVSort_TransformedMatrixDepth)
    {
        return depthPacketTree->m_Root == NULL;
    }
    return packetList->m_Head == NULL;
}

/**
 * Offset/Address/Size: 0x748 | 0x801D9A08 | size: 0x298
 */
void PacketCallbackManager::DoCallback(const glModelPacket* p, unsigned int count)
{
    unsigned long flags = 0;

    if (p->state.program != m_LastProgram)
    {
        flags |= 0x86;
        m_LastProgram = p->state.program;
    }

    if (p->userData == 0)
    {
        if (m_LastUserdata != 0)
        {
            m_LastUserdata = p->userData;
            flags |= glv_UserDataChanged;
        }
    }
    else
    {
        m_LastUserdata = p->userData;
        flags |= glv_UserDataChanged;
    }

    if (p->materialset != m_LastMaterialSet)
    {
        flags |= glv_MaterialChanged;
    }

    if (p->state.userStateKey != m_LastUserStateKey)
    {
        m_LastUserStateKey = p->state.userStateKey;
        flags |= glv_UserStateKeyChanged;
    }

    if (p->state.raster != m_LastRaster)
    {
        m_LastRaster = p->state.raster;
        flags |= glv_RasterChanged;
    }

    if (p->state.texturestate != m_LastTextureState)
    {
        m_LastTextureState = p->state.texturestate;
        flags |= glv_TextureStateChanged;
    }

    if (p->state.matrix != m_LastMatrix)
    {
        m_LastMatrix = p->state.matrix;
        flags |= glv_MatrixChanged;
    }

    if (p->state.texconfig != m_LastTexconfig)
    {
        flags |= glv_TexConfigChanged;
        m_LastTexconfig = p->state.texconfig;
        flags |= glv_TextureChanged;
    }

    for (int i = 0; i < 6; i++)
    {
        if (m_LastTexture[i] != p->state.texture[i])
        {
            m_LastTexture[i] = p->state.texture[i];
            flags |= glv_TextureChanged;
        }
    }

    if (StreamsDiffer(p, m_LastNumStreams, GetLastStreams()))
    {
        m_LastNumStreams = p->numStreams;
        m_LastStreams = p->streams;
        flags |= glv_StreamsChanged;
    }

    unsigned int stage = flags | 0x800;

    while (count != 0)
    {
        m_Cb(m_View, stage, p);
        count--;
    }
}

/**
 * Offset/Address/Size: 0x720 | 0x801D99E0 | size: 0x28
 */
WEAKFUNC void PacketCallbackManager::TexCallback(const glModelPacket* const& key, unsigned int* count)
{
    DoCallback(key, *count);
}

/**
 * Offset/Address/Size: 0x6F8 | 0x801D99B8 | size: 0x28
 */
WEAKFUNC void PacketCallbackManager::DepthCallback(const DepthPacketPair& key, unsigned int* count)
{
    DoCallback(key.packet, *count);
}

/**
 * Offset/Address/Size: 0x6D0 | 0x801D9990 | size: 0x28
 */
WEAKFUNC void PacketCallbackManager::ListCallback(const glModelPacket** pPacketPtr)
{
    DoCallback(*pPacketPtr, 1);
}

static inline void WalkPacketList(
    GLPacketList*, PacketCallbackManager&, GLRenderPacketListCallback);

/**
 * Offset/Address/Size: 0x440 | 0x801D9700 | size: 0x290
 */
void GLRenderList::Iterate(eGLView view, void (*cb)(eGLView, unsigned long, const glModelPacket*))
{
    typedef void (PacketCallbackManager::*TexCallbackType)(const glModelPacket* const&, unsigned int*);
    typedef void (PacketCallbackManager::*DepthCallbackType)(const DepthPacketPair&, unsigned int*);

    PacketCallbackManager pkCallback;
    TexCallbackType texCb;
    DepthCallbackType depthCb;

    pkCallback.m_View = view;
    pkCallback.m_Cb = cb;
    pkCallback.m_LastProgram = (unsigned long)-1;
    pkCallback.m_LastRaster = (unsigned long)-1;
    pkCallback.m_LastTextureState = 0xFFFFFFFF;
    pkCallback.m_LastMatrix = (unsigned long)-1;
    pkCallback.m_LastTexconfig = (unsigned long)-1;
    pkCallback.m_LastUserdata = (unsigned long)-1;
    pkCallback.m_LastUserStateKey = (unsigned long)-1;
    pkCallback.m_LastNumStreams = 0;
    pkCallback.m_LastStreams = NULL;
    pkCallback.m_LastMaterialSet = (unsigned long)-1;
    pkCallback.m_LastTexture[0] = (unsigned long)-1;
    pkCallback.m_LastTexture[1] = (unsigned long)-1;
    pkCallback.m_LastTexture[2] = (unsigned long)-1;
    pkCallback.m_LastTexture[3] = (unsigned long)-1;
    pkCallback.m_LastTexture[4] = (unsigned long)-1;
    pkCallback.m_LastTexture[5] = (unsigned long)-1;

    if (sortMode == GLVSort_Texture)
    {
        for (int layer = 0; layer < 7; layer++)
        {
            if (texPacketTree[layer]->m_Root != NULL)
            {
                cb(view, 1, NULL);
                texCb = &PacketCallbackManager::TexCallback;
                texPacketTree[layer]->Walk(&pkCallback, texCb);
            }
        }
    }
    else if (sortMode == GLVSort_TransformedDepth || sortMode == GLVSort_TransformedMatrixDepth)
    {
        if (depthPacketTree->m_Root != NULL)
        {
            cb(view, 1, NULL);
            depthCb = &PacketCallbackManager::DepthCallback;
            depthPacketTree->Walk(&pkCallback, depthCb);
        }
    }
    else if (sortMode == GLVSort_Reverse)
    {
        if (packetList->m_Head != NULL)
        {
            cb(view, 1, NULL);
            WalkPacketList(packetList, pkCallback, &PacketCallbackManager::ListCallback);
        }
    }
    else
    {
        if (packetList->m_Head != NULL)
        {
            cb(view, 1, NULL);
            WalkPacketList(packetList, pkCallback, &PacketCallbackManager::ListCallback);
        }
    }
}

static inline void WalkPacketList(
    GLPacketList* list, PacketCallbackManager& pkCallback, GLRenderPacketListCallback callback)
{
    GLRenderPacketWalkHelper helper;
    GLRenderPacketWalkCallback walkCb;
    helper.m_CBClass = &pkCallback;
    helper.m_CB = callback;
    walkCb = &GLRenderPacketWalkHelper::Callback;
    nlWalkDLRing(list->m_Head, &helper, walkCb);
}

void GLRenderList::AttachPacket(unsigned long layer, const glModelPacket* pPacket)
{
    const glModelPacket* pKey = pPacket;

    if (view < GLV_FrontEnd && glRenderBuffer.m_bEnabled && glRenderBuffer.m_bExclusive && !glRenderBuffer.m_bSending)
    {
        return;
    }

    GLTexturePacketTree* pTree = texPacketTree[layer];
    unsigned int* pCount = pTree->Add(pKey, 1);

    if (pCount != NULL)
    {
        (*pCount)++;
    }
}

/**
 * Offset/Address/Size: 0x36C | 0x801D962C | size: 0xD4
 */
void gl_ViewAttachPacket(eGLView view, unsigned long layer, const glModelPacket* pPacket)
{
    GLRenderList* pList = gl_ViewGetRenderList(view);
    pList->AttachPacket(layer, pPacket);
}

/**
 * Offset/Address/Size: 0x0 | 0x801D92C0 | size: 0x36C
 */
bool GLRenderList::AttachModel(const glModel* pModel, unsigned long layer)
{
    unsigned long index;
    glModelPacket* pPacket;
    glModelPacket* newPacket;
    DepthPacketPair pair;
    nlMatrix4 m;

    if (view < GLV_FrontEnd && glRenderBuffer.m_bEnabled && glRenderBuffer.m_bExclusive && !glRenderBuffer.m_bSending)
    {
        return true;
    }

    pPacket = pModel->packets;

    if (sortMode == GLVSort_Texture)
    {
        if ((s32)gl_ModifyGetNum() > 0)
        {
            for (index = 0; index < pModel->numPackets; index++, pPacket++)
            {
                newPacket = gl_Modify(pPacket);
                glplatAttachPacket(view, layer, newPacket == NULL ? pPacket : newPacket);
            }
        }
        else
        {
            for (index = 0; index < pModel->numPackets; index++, pPacket++)
            {
                glplatAttachPacket(view, layer, pPacket);
            }
        }
    }
    else if (sortMode == GLVSort_TransformedDepth || sortMode == GLVSort_TransformedMatrixDepth)
    {
        glGetIdentityMatrix();
        glGetMatrix((uintptr_t)glViewGetViewMatrix(view), m);
        for (index = 0; index < pModel->numPackets; index++, pPacket++)
        {
            pair.packet = pPacket;
            if (sortMode == GLVSort_TransformedMatrixDepth)
            {
                pair.sortKey = PackMatrixDepth(pPacket, m, uDepthInsertNumber);
                uDepthInsertNumber++;
            }
            else
            {
                pair.sortKey = PackStreamDepth(pPacket, m);
            }

            unsigned int* pCount = depthPacketTree->Add(pair, 1);
            if (pCount != NULL)
            {
                *pCount = *pCount + 1;
            }
        }
    }
    else if (sortMode == GLVSort_Reverse)
    {
        for (index = 0; index < pModel->numPackets; index++, pPacket++)
        {
            glModelPacket* modified = glplatModifyPacket(view, pPacket);
            packetList->AddStart(modified);
        }
    }
    else if (sortMode == GLVSort_None)
    {
        for (index = 0; index < pModel->numPackets; index++, pPacket++)
        {
            glModelPacket* modified = glplatModifyPacket(view, pPacket);
            packetList->AddEnd(modified);
        }
    }

    return true;
}
