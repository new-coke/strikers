#include "Game/Physics/NetMeshModelLoader.h"
#include "port/endian.h"
#include <stdlib.h>
#include "Game/WorldManager.h"
#include "Game/Drawable/DrawableModel.h"
#include "NL/glx/glxDisplayList.h"

// Use typedefs from header for convenience
typedef NetMeshModelLoader::NetMeshVertex NetMeshVertex;
typedef NetMeshModelLoader::NetMeshEdge NetMeshEdge;
typedef NetMeshModelLoader::VertexTree VertexTree;
typedef NetMeshModelLoader::EdgeTree EdgeTree;
typedef NetMeshModelLoader::VertexEntry VertexEntry;
typedef NetMeshModelLoader::EdgeEntry EdgeEntry;

static int s_initialEdgeCount = 1;
static int s_initialVertexCount = 1;
static unsigned char sbPullGoalsOut;

/**
 * Offset/Address/Size: 0x1400 | 0x80131558 | size: 0x54
 */
NetMeshModelLoader::NetMeshModelLoader(NetMesh& netMesh, unsigned long netMeshDrawableObjectID)
    : m_NetMesh(netMesh)
    , m_NetMeshDrawableObjectID(netMeshDrawableObjectID)
    , m_NumParticles(0)
    , m_TriStripIndices(NULL)
    , m_CurrentTriStripIndex(0)
{
    LoadGeometryFromModel();
}

/**
 * Offset/Address/Size: 0x12C8 | 0x80131420 | size: 0x78
 */
NetMeshModelLoader::~NetMeshModelLoader()
{
    delete m_EdgeList;
    delete m_VertexList;
    delete m_TriStripIndices;
}

/**
 * Offset/Address/Size: 0xE78 | 0x80130FD0 | size: 0x450
 */
void NetMeshModelLoader::LoadGeometryFromModel()
{
    m_EdgeList = new (nlMalloc(sizeof(EdgeTree), 8, false)) EdgeTree(0x10, 0x10);
    m_VertexList = new (nlMalloc(sizeof(VertexTree), 8, false)) VertexTree(0x10, 0x10);

    DrawableModel* pDrawable = (DrawableModel*)WorldManager::s_World->FindDrawableObject(m_NetMeshDrawableObjectID);
    u16 maxVertex = 0;
    u16 vertexOffset = 0;
    s32 packetOffset;
    u16 numPackets = (u16)pDrawable->m_pModel->numPackets;
    s32 packetCount = 0;
    s32 packetSumIndex = 0;

    m_NumTriStripIndices = packetCount;
    m_CurrentTriStripIndex = packetCount;

    for (packetSumIndex = 0; packetSumIndex < numPackets; packetSumIndex++)
    {
        m_NumTriStripIndices += pDrawable->m_pModel->packets[packetSumIndex].numVertices;
    }

    m_TriStripIndices = (u16*)nlMalloc((unsigned long)(m_NumTriStripIndices * 2), 8, false);
    for (s32 i = 0; i < m_NumTriStripIndices; i++)
    {
        m_TriStripIndices[i] = 0xFFFF;
    }

    s32 packetIndex = 0;
    packetOffset = 0;
    while (packetIndex < numPackets)
    {
        maxVertex = 0;
        glModelPacket* pPacket = (glModelPacket*)((u8*)pDrawable->m_pModel->packets + packetOffset);
        vertexOffset = (u16)m_NumParticles;

        DisplayList* pList = dlGetStruct(pPacket->indexBuffer);
        struct TriStripIV
        {
            s32 index;
            s32 offset;
        };
        TriStripIV iv;
        iv.index = 0;
        iv.offset = iv.index;

        while (iv.index < pPacket->numVertices)
        {
            u16* ptr;
            if (((u16*)&pList->indices)[1] != 0)
            {
                u16 ns = ((u16*)&pList->indices)[0];
                s32 stride = (ns - 1) * 2 + 1;
                s32 offset = stride * iv.index;
                u8* ptr8 = (u8*)pList->list + offset;
                ptr = (u16*)ptr8;
                ptr8 = (u8*)ptr;
                ptr8 += 4;
                ptr = (u16*)ptr8;
            }
            else
            {
                u16 ns = ((u16*)&pList->indices)[0];
                s32 stride = ns * 2;
                s32 offset = iv.index * stride;
                u8* ptr8 = (u8*)pList->list;
                ptr8 += offset;
                ptr = (u16*)ptr8;
                ptr = (u16*)((u8*)ptr + 3);
            }

            // PORT: FIFO indices are big-endian.
            s32 idx = port_be16(ptr);
            if (idx > maxVertex)
            {
                maxVertex = idx;
            }

            *(u16*)((u8*)m_TriStripIndices + iv.offset) = idx + vertexOffset;
            iv.index++;
            iv.offset += 2;
            m_CurrentTriStripIndex++;
        }

        m_NetMesh.SetTexture(pPacket->state.texture[0]);

        u16 maxV = maxVertex;
        s32 i2 = 0;
        while (i2 <= maxV)
        {
            NetMeshVertex vertex;
            vertex.mpPacket = pPacket;
            vertex.mIndex = i2;
            vertex.mParticleIndex = m_NumParticles;

            VertexTree* vertexList = m_VertexList;
            vertexList->Add(vertex, s_initialVertexCount);

            m_NumParticles++;
            i2++;
        }

        ReadEdgesFromGeometryPacket(*pPacket);
        packetOffset += sizeof(glModelPacket);
        packetIndex++;
    }

    CreateNetMeshFromVertexList();
}

/**
 * Offset/Address/Size: 0xC44 | 0x80130D9C | size: 0x234
 */
void NetMeshModelLoader::AddEdge(const glModelPacket& packet, unsigned short idx1, unsigned short idx2)
{
    NetMeshEdge edge;
    edge.mpPacket = NULL;
    edge.mpVertex1 = NULL;
    edge.mpVertex2 = NULL;

    NetMeshVertex* pVertex1;
    NetMeshVertex* pVertex2;
    int* pValue;

    m_VertexList->Find(NetMeshVertex(&packet, idx1), &pValue, &pVertex1);
    m_VertexList->Find(NetMeshVertex(&packet, idx2), &pValue, &pVertex2);

    // Construct edge with vertices ordered by mIndex (smaller first)
    edge.mpPacket = &packet;

    NetMeshVertex* v2 = pVertex2;
    if (pVertex1->mIndex < v2->mIndex)
    {
        edge.mpVertex1 = pVertex1;
        edge.mpVertex2 = v2;
    }
    else
    {
        edge.mpVertex1 = v2;
        edge.mpVertex2 = pVertex1;
    }

    int* pRefCount = m_EdgeList->Add(edge, s_initialEdgeCount);

    if (pRefCount != NULL)
    {
        (*pRefCount)++;
    }
}

/**
 * Offset/Address/Size: 0xB90 | 0x80130CE8 | size: 0xB4
 */
void NetMeshModelLoader::AddTriangleFromGeometry(const glModelPacket& packet, unsigned short* vertexIndices)
{
    unsigned char isThin = 0;

    if (vertexIndices[0] == vertexIndices[1] || vertexIndices[1] == vertexIndices[2] || vertexIndices[0] == vertexIndices[2])
    {
        isThin = 1;
    }

    if (!isThin)
    {
        for (int j = 0; j < 3; j++)
        {
            AddEdge(packet, vertexIndices[j], vertexIndices[(j + 1) % 3]);
        }
    }
}

/**
 * Offset/Address/Size: 0xA80 | 0x80130BD8 | size: 0x110
 */
void NetMeshModelLoader::ReadEdgesFromGeometryPacket(const glModelPacket& packet)
{
    u16 maxVertex = 0;

    if (packet.primType != 1)
        return;

    DisplayList* pList = dlGetStruct(packet.indexBuffer);

    s32 i = 2;
    while (i < packet.numVertices)
    {
        u16 vertexIndices[3];
        u16* pVtx = vertexIndices;
        u16* ptr;
        u16 ns;
        s32 vertOff;
        s32 stride;
        s32 offset;
        u8* ptr8;

        for (s32 j = 0; j < 3; j++)
        {
            if (((u16*)&pList->indices)[1] != 0)
            {
                s32 base = i - 2;
                ns = ((u16*)&pList->indices)[0];
                vertOff = base + j;
                stride = (ns - 1) * 2 + 1;
                offset = stride * vertOff;
                ptr8 = (u8*)pList->list + offset;
                ptr = (u16*)ptr8;
                ptr8 = (u8*)ptr;
                ptr8 += 4;
                ptr = (u16*)ptr8;
            }
            else
            {
                ns = ((u16*)&pList->indices)[0];
                vertOff = i;
                vertOff += j;
                vertOff -= 2;
                stride = ns * 2;
                offset = vertOff * stride;
                ptr8 = (u8*)pList->list + offset;
                ptr = (u16*)ptr8;
                ptr8 = (u8*)ptr;
                ptr8 += 3;
                ptr = (u16*)ptr8;
            }

            // PORT: FIFO indices are big-endian.
            *pVtx = port_be16(ptr);
            if (*pVtx > maxVertex)
                maxVertex = *pVtx;
            pVtx++;
        }

        AddTriangleFromGeometry(packet, vertexIndices);
        i++;
    }

    ProcessEdges(packet, (s32)maxVertex);
}

/**
 * Offset/Address/Size: 0x780 | 0x801308D8 | size: 0x300
 */
void NetMeshModelLoader::ProcessEdges(const glModelPacket& packet, int maxVertex)
{
    extern void* nlMalloc(size_t, unsigned int, bool);

    struct EdgeIter
    {
        EdgeEntry** m_Stack;
        unsigned int m_NumStackEntries;
    };

    EdgeIter* iter;
    unsigned char* allReady;
    unsigned char* readyIndicator;

    allReady = (unsigned char*)nlMalloc(maxVertex + 1, 8, false);
    {
        int i = 0;
        while (i < maxVertex)
        {
            allReady[i] = 0;
            i++;
        }
    }

    readyIndicator = (unsigned char*)nlMalloc(maxVertex + 1, 8, false);
    {
        int i = 0;
        while (i < maxVertex)
        {
            readyIndicator[i] = 0;
            i++;
        }
    }

    EdgeTree* edgeTree = m_EdgeList;
    iter = (EdgeIter*)nlMalloc(sizeof(EdgeIter), 8, false);

    if (iter != NULL)
    {
        unsigned int numEntries = edgeTree->m_NumElements;
        EdgeEntry* node = edgeTree->m_Root;

        iter->m_Stack = (EdgeEntry**)nlMalloc((numEntries + 1) * sizeof(void*), 8, false);
        iter->m_NumStackEntries = 0;

        if (node != NULL)
        {
            while (node->node.left != NULL)
            {
                iter->m_Stack[iter->m_NumStackEntries] = node;
                iter->m_NumStackEntries++;
                node = (EdgeEntry*)node->node.left;
            }

            iter->m_Stack[iter->m_NumStackEntries] = node;
            iter->m_NumStackEntries++;
        }
    }

    while (iter->m_NumStackEntries != 0)
    {
        unsigned short index1;
        unsigned short index2;
        EdgeEntry* edgeEntry = iter->m_Stack[iter->m_NumStackEntries - 1];

        if (edgeEntry->key.mpPacket == &packet)
        {
            index1 = edgeEntry->key.mpVertex1->mIndex;
            index2 = edgeEntry->key.mpVertex2->mIndex;

            if (edgeEntry->value == 1)
            {
                allReady[index1] = 1;
                allReady[index2] = 1;
                edgeEntry->key.mpVertex1->mbIsConstrained = 1;
                edgeEntry->key.mpVertex2->mbIsConstrained = 1;
            }

            readyIndicator[index1] = 1;
            readyIndicator[index2] = 1;
        }

        iter->m_NumStackEntries--;
        edgeEntry = iter->m_Stack[iter->m_NumStackEntries];

        EdgeEntry* right = (EdgeEntry*)edgeEntry->node.right;
        if (right != NULL)
        {
            while (right->node.left != NULL)
            {
                iter->m_Stack[iter->m_NumStackEntries] = right;
                iter->m_NumStackEntries++;
                right = (EdgeEntry*)right->node.left;
            }

            iter->m_Stack[iter->m_NumStackEntries] = right;
            iter->m_NumStackEntries++;
        }
    }

    if (iter != NULL)
    {
        delete[] iter->m_Stack;
        delete iter;
    }

    delete readyIndicator;
    delete allReady;
}

/**
 * Offset/Address/Size: 0x0 | 0x80130158 | size: 0x780
 */
void NetMeshModelLoader::CreateNetMeshFromVertexList()
{
    extern void* nlMalloc(size_t, unsigned int, bool);

    struct VertexIter
    {
        VertexEntry** m_Stack;
        unsigned int m_NumStackEntries;
    };

    struct EdgeIter
    {
        EdgeEntry** m_Stack;
        unsigned int m_NumStackEntries;
    };

    VertexIter* vertexIter;
    VertexTree* vertexTree;
    VertexEntry* node;
    EdgeEntry* edgeNode;
    NetMeshVertex* vertex;
    int numEdges = 0;
    int numVertices = 0;
    int numConstrainedVertices;

    vertexTree = m_VertexList;
    numConstrainedVertices = 0;
    vertexIter = (VertexIter*)nlMalloc(sizeof(VertexIter), 8, false);
    if (vertexIter != NULL)
    {
        unsigned int numEntries = vertexTree->m_NumElements;
        node = vertexTree->m_Root;

        vertexIter->m_Stack = (VertexEntry**)nlMalloc((numEntries + 1) * sizeof(void*), 8, false);
        vertexIter->m_NumStackEntries = 0;

        if (node != NULL)
        {
            while (node->node.left != NULL)
            {
                vertexIter->m_Stack[vertexIter->m_NumStackEntries] = node;
                vertexIter->m_NumStackEntries++;
                node = (VertexEntry*)node->node.left;
            }

            vertexIter->m_Stack[vertexIter->m_NumStackEntries] = node;
            vertexIter->m_NumStackEntries++;
        }
    }

    while (vertexIter->m_NumStackEntries != 0)
    {
        VertexEntry* vertexEntry = vertexIter->m_Stack[vertexIter->m_NumStackEntries - 1];
        numVertices++;

        if (vertexEntry->key.mbIsConstrained != 0)
        {
            numConstrainedVertices++;
        }

        vertexIter->m_NumStackEntries--;
        vertexEntry = vertexIter->m_Stack[vertexIter->m_NumStackEntries];

        VertexEntry* right = (VertexEntry*)vertexEntry->node.right;
        if (right != NULL)
        {
            while (right->node.left != NULL)
            {
                vertexIter->m_Stack[vertexIter->m_NumStackEntries] = right;
                vertexIter->m_NumStackEntries++;
                right = (VertexEntry*)right->node.left;
            }

            vertexIter->m_Stack[vertexIter->m_NumStackEntries] = right;
            vertexIter->m_NumStackEntries++;
        }
    }

    if (vertexIter != NULL)
    {
        delete[] vertexIter->m_Stack;
        delete vertexIter;
    }

    EdgeTree* edgeTree = m_EdgeList;
    EdgeIter* edgeIter = (EdgeIter*)nlMalloc(sizeof(EdgeIter), 8, false);
    if (edgeIter != NULL)
    {
        unsigned int numEntries = edgeTree->m_NumElements;
        edgeNode = edgeTree->m_Root;

        edgeIter->m_Stack = (EdgeEntry**)nlMalloc((numEntries + 1) * sizeof(void*), 8, false);
        edgeIter->m_NumStackEntries = 0;

        if (edgeNode != NULL)
        {
            while (edgeNode->node.left != NULL)
            {
                edgeIter->m_Stack[edgeIter->m_NumStackEntries] = edgeNode;
                edgeIter->m_NumStackEntries++;
                edgeNode = (EdgeEntry*)edgeNode->node.left;
            }

            edgeIter->m_Stack[edgeIter->m_NumStackEntries] = edgeNode;
            edgeIter->m_NumStackEntries++;
        }
    }

    while (edgeIter->m_NumStackEntries != 0)
    {
        edgeIter->m_NumStackEntries--;
        numEdges++;

        EdgeEntry* edgeEntry = edgeIter->m_Stack[edgeIter->m_NumStackEntries];
        EdgeEntry* right = (EdgeEntry*)edgeEntry->node.right;
        if (right != NULL)
        {
            while (right->node.left != NULL)
            {
                edgeIter->m_Stack[edgeIter->m_NumStackEntries] = right;
                edgeIter->m_NumStackEntries++;
                right = (EdgeEntry*)right->node.left;
            }

            edgeIter->m_Stack[edgeIter->m_NumStackEntries] = right;
            edgeIter->m_NumStackEntries++;
        }
    }

    if (edgeIter != NULL)
    {
        delete[] edgeIter->m_Stack;
        delete edgeIter;
    }

    m_NumParticles = numVertices;
    if (getenv("STRIKERS_LOG_NIS") != NULL)
    {
        OSReport("[net] verts=%d edges=%d constrained=%d stripIndices=%d\n",
                 numVertices, numEdges, numConstrainedVertices,
                 (int)m_NumTriStripIndices);
    }
    m_NetMesh.Allocate(m_NumParticles, numEdges, numConstrainedVertices);

    DrawableModel* pObject = (DrawableModel*)WorldManager::s_World->FindDrawableObject(m_NetMeshDrawableObjectID);

    vertexTree = m_VertexList;
    vertexIter = (VertexIter*)nlMalloc(sizeof(VertexIter), 8, false);
    if (vertexIter != NULL)
    {
        unsigned int numEntries = vertexTree->m_NumElements;
        node = vertexTree->m_Root;

        vertexIter->m_Stack = (VertexEntry**)nlMalloc((numEntries + 1) * sizeof(void*), 8, false);
        vertexIter->m_NumStackEntries = 0;

        if (node != NULL)
        {
            while (node->node.left != NULL)
            {
                vertexIter->m_Stack[vertexIter->m_NumStackEntries] = node;
                vertexIter->m_NumStackEntries++;
                node = (VertexEntry*)node->node.left;
            }

            vertexIter->m_Stack[vertexIter->m_NumStackEntries] = node;
            vertexIter->m_NumStackEntries++;
        }
    }

    float scale = 1024.0f;
    int index = 0;

    while (vertexIter->m_NumStackEntries != 0)
    {
        VertexEntry* vertexEntry = vertexIter->m_Stack[vertexIter->m_NumStackEntries - 1];
        vertex = &vertexEntry->key;

        nlVector3 position = *vertex->GetPosition();
        nlVector3 normal;
        nlVector2 texCoord;
        shortVector2 shortCoord;

        vertex->GetNormal(normal);
        vertex->GetTextureCoord(texCoord);

        shortCoord.e[0] = (s16)(scale * texCoord.x);
        shortCoord.e[1] = (s16)(scale * texCoord.y);

        if (sbPullGoalsOut != 0)
        {
            position.x = position.x - 5.0f;
        }

        nlMultPosVectorMatrix(position, position, pObject->GetWorldMatrix());
        nlMultDirVectorMatrix(normal, normal, pObject->GetWorldMatrix());

        int particle = m_NetMesh.m_NumParticles;
        NetMesh* pNetMesh = &m_NetMesh;
        pNetMesh->m_v3Position[particle] = position;
        pNetMesh->m_v3Normal[particle] = normal;
        pNetMesh->m_v2TextureCoords[particle] = shortCoord;
        particle = pNetMesh->m_NumParticles;
        pNetMesh->m_NumParticles = particle + 1;

        if (vertex->mbIsConstrained != 0)
        {
            m_NetMesh.SetPositionConstraint(index, position);
        }

        vertex->mParticleIndex = index;

        vertexIter->m_NumStackEntries--;
        vertexEntry = vertexIter->m_Stack[vertexIter->m_NumStackEntries];

        VertexEntry* right = (VertexEntry*)vertexEntry->node.right;
        if (right != NULL)
        {
            while (right->node.left != NULL)
            {
                vertexIter->m_Stack[vertexIter->m_NumStackEntries] = right;
                vertexIter->m_NumStackEntries++;
                right = (VertexEntry*)right->node.left;
            }

            vertexIter->m_Stack[vertexIter->m_NumStackEntries] = right;
            vertexIter->m_NumStackEntries++;
        }

        index++;
    }

    if (vertexIter != NULL)
    {
        delete[] vertexIter->m_Stack;
        delete vertexIter;
    }

    edgeTree = m_EdgeList;
    edgeIter = (EdgeIter*)nlMalloc(sizeof(EdgeIter), 8, false);
    if (edgeIter != NULL)
    {
        unsigned int numEntries = edgeTree->m_NumElements;
        edgeNode = edgeTree->m_Root;

        edgeIter->m_Stack = (EdgeEntry**)nlMalloc((numEntries + 1) * sizeof(void*), 8, false);
        edgeIter->m_NumStackEntries = 0;

        if (edgeNode != NULL)
        {
            while (edgeNode->node.left != NULL)
            {
                edgeIter->m_Stack[edgeIter->m_NumStackEntries] = edgeNode;
                edgeIter->m_NumStackEntries++;
                edgeNode = (EdgeEntry*)edgeNode->node.left;
            }

            edgeIter->m_Stack[edgeIter->m_NumStackEntries] = edgeNode;
            edgeIter->m_NumStackEntries++;
        }
    }

    int index1;
    int index2;
    EdgeEntry* edgeEntry;

    while (edgeIter->m_NumStackEntries != 0)
    {
        edgeEntry = edgeIter->m_Stack[edgeIter->m_NumStackEntries - 1];

        if (edgeEntry->value > 1)
        {
            index1 = edgeEntry->key.mpVertex1->mParticleIndex;
            index2 = edgeEntry->key.mpVertex2->mParticleIndex;

            nlVector3 position1 = *edgeEntry->key.mpVertex1->GetPosition();
            nlVector3 position2 = *edgeEntry->key.mpVertex2->GetPosition();

            float dx = position1.x - position2.x;
            float dy = position1.y - position2.y;
            float dz = position1.z - position2.z;
            float distance = nlSqrt(dx * dx + dy * dy + dz * dz, true);

            m_NetMesh.SetDistanceConstraint(index1, index2, distance);
        }

        edgeIter->m_NumStackEntries--;
        edgeEntry = edgeIter->m_Stack[edgeIter->m_NumStackEntries];

        EdgeEntry* right = (EdgeEntry*)edgeEntry->node.right;
        if (right != NULL)
        {
            while (right->node.left != NULL)
            {
                edgeIter->m_Stack[edgeIter->m_NumStackEntries] = right;
                edgeIter->m_NumStackEntries++;
                right = (EdgeEntry*)right->node.left;
            }

            edgeIter->m_Stack[edgeIter->m_NumStackEntries] = right;
            edgeIter->m_NumStackEntries++;
        }
    }

    if (edgeIter != NULL)
    {
        delete[] edgeIter->m_Stack;
        delete edgeIter;
    }

    m_NetMesh.SetTriStripIndices(m_NumTriStripIndices, m_TriStripIndices);
}
