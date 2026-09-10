#include "Game/GL/GLInventory.h"
#include <stdlib.h>   // PORT: getenv
#include "dolphin/os.h" // PORT: OSReport, likewise
#include "NL/glx/glxLoadModel.h"

inline GLInventory::GLInventory()
{
    m_bCreated = false;
    for (int i = 0; i < 16; i++)
    {
        m_pFileData[i] = NULL;
        m_pModels[i] = NULL;
        m_pSkinData[i] = NULL;
        m_pShadowVolumes[i] = NULL;
        m_pTextureAnims[i] = NULL;
        m_pVertexAnims[i] = NULL;
        m_pMaterialLists[i] = NULL;
    }
    m_nLevel = 0;
}

GLInventory glInventory;

/**
 * Offset/Address/Size: 0x1058 | 0x801E32F0 | size: 0x50
 */
GLInventory::~GLInventory()
{
    Delete();
}

/**
 * Offset/Address/Size: 0xD54 | 0x801E2FEC | size: 0x304
 */
void GLInventory::Create()
{
    m_bCreated = true;

    nlListContainer<void*>** current = m_pFileData;
    int i = 0;

    for (; i < 16; i++, current++)
    {
        nlListContainer<void*>* fileData = new (8, false) nlListContainer<void*>();
        *current = fileData;

        freeing_GLInventory<nlChunk>* pSkinData = (freeing_GLInventory<nlChunk>*)nlMalloc(sizeof(freeing_GLInventory<nlChunk>), 8, false);
        if (pSkinData != NULL)
        {
            pSkinData->m_pItems = new (nlMalloc(sizeof(nlAVLTree<unsigned long, nlChunk*, DefaultKeyCompare<unsigned long> >), 8, false))
                nlAVLTree<unsigned long, nlChunk*, DefaultKeyCompare<unsigned long> >();
        }
        ((freeing_GLInventory<nlChunk>**)current)[16] = pSkinData;

        clearing_GLInventory<glModel>* pModels = (clearing_GLInventory<glModel>*)nlMalloc(sizeof(clearing_GLInventory<glModel>), 8, false);
        if (pModels != NULL)
        {
            pModels->m_pItems = new (nlMalloc(sizeof(nlAVLTree<unsigned long, glModel*, DefaultKeyCompare<unsigned long> >), 8, false))
                nlAVLTree<unsigned long, glModel*, DefaultKeyCompare<unsigned long> >();
        }
        ((clearing_GLInventory<glModel>**)current)[32] = pModels;

        deleting_GLInventory<GLShadowVolume>* pShadowVolumes = (deleting_GLInventory<GLShadowVolume>*)nlMalloc(sizeof(deleting_GLInventory<GLShadowVolume>), 8, false);
        if (pShadowVolumes != NULL)
        {
            pShadowVolumes->m_pItems = new (nlMalloc(sizeof(nlAVLTree<unsigned long, GLShadowVolume*, DefaultKeyCompare<unsigned long> >), 8, false))
                nlAVLTree<unsigned long, GLShadowVolume*, DefaultKeyCompare<unsigned long> >();
        }
        ((deleting_GLInventory<GLShadowVolume>**)current)[48] = pShadowVolumes;

        deleting_GLInventory<GLTextureAnim>* pTextureAnims = (deleting_GLInventory<GLTextureAnim>*)nlMalloc(sizeof(deleting_GLInventory<GLTextureAnim>), 8, false);
        if (pTextureAnims != NULL)
        {
            pTextureAnims->m_pItems = new (nlMalloc(sizeof(nlAVLTree<unsigned long, GLTextureAnim*, DefaultKeyCompare<unsigned long> >), 8, false))
                nlAVLTree<unsigned long, GLTextureAnim*, DefaultKeyCompare<unsigned long> >();
        }
        ((deleting_GLInventory<GLTextureAnim>**)current)[64] = pTextureAnims;

        deleting_GLInventory<GLVertexAnim>* pVertexAnims = (deleting_GLInventory<GLVertexAnim>*)nlMalloc(sizeof(deleting_GLInventory<GLVertexAnim>), 8, false);
        if (pVertexAnims != NULL)
        {
            pVertexAnims->m_pItems = new (nlMalloc(sizeof(nlAVLTree<unsigned long, GLVertexAnim*, DefaultKeyCompare<unsigned long> >), 8, false))
                nlAVLTree<unsigned long, GLVertexAnim*, DefaultKeyCompare<unsigned long> >();
        }
        ((deleting_GLInventory<GLVertexAnim>**)current)[80] = pVertexAnims;

        deleting_GLInventory<GLMaterialList>* pMaterialLists = (deleting_GLInventory<GLMaterialList>*)nlMalloc(sizeof(deleting_GLInventory<GLMaterialList>), 8, false);
        if (pMaterialLists != NULL)
        {
            pMaterialLists->m_pItems = new (nlMalloc(sizeof(nlAVLTree<unsigned long, GLMaterialList*, DefaultKeyCompare<unsigned long> >), 8, false))
                nlAVLTree<unsigned long, GLMaterialList*, DefaultKeyCompare<unsigned long> >();
        }
        ((deleting_GLInventory<GLMaterialList>**)current)[96] = pMaterialLists;
    }
}

/**
 * Offset/Address/Size: 0xC4C | 0x801E2EE4 | size: 0x108
 */
void GLInventory::Delete()
{
    m_bCreated = false;

    nlListContainer<void*>* fileData = NULL;
    nlListContainer<void*>** current = m_pFileData;
    int i = 0;

    for (; i < 16; i++, current++)
    {
        ReleaseLevel(i);

        fileData = *current;
        delete fileData;

        delete ((freeing_GLInventory<nlChunk>**)current)[16];
        delete ((clearing_GLInventory<glModel>**)current)[32];
        delete ((deleting_GLInventory<GLShadowVolume>**)current)[48];
        delete ((deleting_GLInventory<GLTextureAnim>**)current)[64];
        delete ((deleting_GLInventory<GLVertexAnim>**)current)[80];
        delete ((deleting_GLInventory<GLMaterialList>**)current)[96];
    }
}

static inline void DeleteFileEntries(ListEntry<void*>* current)
{
    while (current != NULL)
    {
        delete current->entry;
        current = current->next;
    }
}

/**
 * Offset/Address/Size: 0xB68 | 0x801E2E00 | size: 0xE4
 */
void GLInventory::ReleaseLevel(int nLevel)
{
    DeleteFileEntries(m_pFileData[nLevel]->m_Head);
    m_pFileData[nLevel]->Clear();

    m_pSkinData[nLevel]->Release();
    m_pModels[nLevel]->Release();
    m_pShadowVolumes[nLevel]->Release();
    m_pTextureAnims[nLevel]->Release();
    m_pVertexAnims[nLevel]->Release();
    m_pMaterialLists[nLevel]->Release();
}

/**
 * Offset/Address/Size: 0xB58 | 0x801E2DF0 | size: 0x10
 */
void GLInventory::ResourceMark()
{
    m_nLevel++;
}

/**
 * Offset/Address/Size: 0xB00 | 0x801E2D98 | size: 0x58
 */
void GLInventory::ResourceRelease(int nLevel)
{
    while (m_nLevel != nLevel)
    {
        ReleaseLevel(m_nLevel);
        m_nLevel--;
    }
}

/**
 * Offset/Address/Size: 0xA8C | 0x801E2D24 | size: 0x74
 */
void GLInventory::AddModel(unsigned long key, glModel* model)
{
    unsigned long k = key;
    glModel* value = model;
    nlAVLTree<unsigned long, glModel*, DefaultKeyCompare<unsigned long> >* pTree = m_pModels[m_nLevel]->m_pItems;
    pTree->Add(k, value);
}

/**
 * Offset/Address/Size: 0x9C4 | 0x801E2C5C | size: 0xC8
 */
glModel* GLInventory::GetModel(unsigned long id)
{
    for (int i = m_nLevel; i >= 0; i--)
    {
        glModel** pResult;
        bool found = m_pModels[i]->m_pItems->FindGet(id, &pResult);
        glModel* result;
        if (found)
        {
            result = *pResult;
        }
        else
        {
            result = nullptr;
        }
        if (result != nullptr)
        {
            return result;
        }
    }
    return nullptr;
}

/**
 * Offset/Address/Size: 0x8FC | 0x801E2B94 | size: 0xC8
 */
GLShadowVolume* GLInventory::GetShadowVolume(unsigned long id)
{
    for (int i = m_nLevel; i >= 0; i--)
    {
        GLShadowVolume** pResult;
        bool found = m_pShadowVolumes[i]->m_pItems->FindGet(id, &pResult);
        GLShadowVolume* result;
        if (found)
        {
            result = *pResult;
        }
        else
        {
            result = nullptr;
        }
        if (result != nullptr)
        {
            return result;
        }
    }
    return nullptr;
}

/**
 * Offset/Address/Size: 0x888 | 0x801E2B20 | size: 0x74
 */
void GLInventory::AddTextureAnim(unsigned long key, GLTextureAnim* anim)
{
    unsigned long k = key;
    GLTextureAnim* value = anim;
    nlAVLTree<unsigned long, GLTextureAnim*, DefaultKeyCompare<unsigned long> >* pTree = m_pTextureAnims[m_nLevel]->m_pItems;
    pTree->Add(k, value);
}


/**
 * Offset/Address/Size: 0x7C0 | 0x801E2A58 | size: 0xC8
 */
GLTextureAnim* GLInventory::GetTextureAnim(uintptr_t id)
{
    for (int i = m_nLevel; i >= 0; i--)
    {
        GLTextureAnim** foundValue;
        // PORT: was a stand-in struct reaching the tree's root at +8, the console offset. nlAVLTree starts with a vtable pointer.
        bool found = m_pTextureAnims[i]->m_pItems->FindGet(id, &foundValue);
        GLTextureAnim* result;
        if (found)
            result = *foundValue;
        else
            result = nullptr;
        if (result != nullptr)
            return result;
    }
    return nullptr;
}

/**
 * Offset/Address/Size: 0x74C | 0x801E29E4 | size: 0x74
 */
void GLInventory::AddVertexAnim(unsigned long key, GLVertexAnim* vertexAnim)
{
    unsigned long k = key;
    GLVertexAnim* value = vertexAnim;
    nlAVLTree<unsigned long, GLVertexAnim*, DefaultKeyCompare<unsigned long> >* pTree = m_pVertexAnims[m_nLevel]->m_pItems;
    pTree->Add(k, value);
}

/**
 * Offset/Address/Size: 0x684 | 0x801E291C | size: 0xC8
 */
GLVertexAnim* GLInventory::GetVertexAnim(unsigned long id)
{
    for (int i = m_nLevel; i >= 0; i--)
    {
        GLVertexAnim** pResult;
        bool found = m_pVertexAnims[i]->m_pItems->FindGet(id, &pResult);
        GLVertexAnim* result;
        if (found)
        {
            result = *pResult;
        }
        else
        {
            result = nullptr;
        }
        if (result != nullptr)
        {
            return result;
        }
    }
    return nullptr;
}

/**
 * Offset/Address/Size: 0x610 | 0x801E28A8 | size: 0x74
 */
void GLInventory::AddMaterialList(unsigned long key, GLMaterialList* materialList)
{
    unsigned long k = key;
    GLMaterialList* value = materialList;
    nlAVLTree<unsigned long, GLMaterialList*, DefaultKeyCompare<unsigned long> >* pItems = m_pMaterialLists[m_nLevel]->m_pItems;

    pItems->Add(k, value);
}

/**
 * Offset/Address/Size: 0x548 | 0x801E27E0 | size: 0xC8
 */
GLMaterialList* GLInventory::GetMaterialList(unsigned long id)
{
    for (int i = m_nLevel; i >= 0; i--)
    {
        GLMaterialList** pResult;
        bool found = m_pMaterialLists[i]->m_pItems->FindGet(id, &pResult);
        GLMaterialList* result;
        if (found)
        {
            result = *pResult;
        }
        else
        {
            result = nullptr;
        }
        if (result != nullptr)
        {
            return result;
        }
    }
    return nullptr;
}

/**
 * Offset/Address/Size: 0x4D4 | 0x801E276C | size: 0x74
 */
void GLInventory::AddSkinData(unsigned long key, nlChunk* skinData)
{
    unsigned long key2 = key;
    nlChunk* skinData2 = skinData;
    freeing_GLInventory<nlChunk>* pSkinData = m_pSkinData[m_nLevel];
    nlAVLTree<unsigned long, nlChunk*, DefaultKeyCompare<unsigned long> >* tree = pSkinData->m_pItems;

    tree->Add(key2, skinData2);
}

GLSkinMesh* GLInventory::MakeSkinMesh(nlChunk* pChunk, glModel* pModel)
{
    return glx_MakeSkinMesh(pChunk, pModel);
}

/**
 * Offset/Address/Size: 0x334 | 0x801E25CC | size: 0x1A0
 */
GLSkinMesh* GLInventory::MakeSkinMesh(unsigned long hashID)
{
    struct SkinDataHelper
    {
        static inline nlChunk* Get(GLInventory* self, unsigned long id)
        {
            for (int i = self->m_nLevel; i >= 0; i--)
            {
                nlChunk** pResult;
                bool found = self->m_pSkinData[i]->m_pItems->FindGet(id, &pResult);
                nlChunk* result = found ? *pResult : nullptr;

                if (result != nullptr)
                {
                    return result;
                }
            }

            return nullptr;
        }
    };

    struct ModelHelper
    {
        static inline glModel* Get(GLInventory* self, unsigned long id)
        {
            for (int i = self->m_nLevel; i >= 0; i--)
            {
                glModel** pResult;
                bool found = self->m_pModels[i]->m_pItems->FindGet(id, &pResult);
                glModel* result = found ? *pResult : nullptr;

                if (result != nullptr)
                {
                    return result;
                }
            }

            return nullptr;
        }
    };

    nlChunk* foundChunk = SkinDataHelper::Get(this, hashID);
    nlChunk* pChunk = foundChunk;
    glModel* pModel = ModelHelper::Get(this, hashID);

    // PORT: A NULL chunk here is a crash one call later, with nothing to say which key missed.
    if (getenv("STRIKERS_PROBE_SKIN"))
        OSReport("[skin] LOOKUP id=%lu chunk=%p model=%p level=%d\n",
                 (unsigned long)hashID, (void*)pChunk, (void*)pModel,
                 (int)m_nLevel);

    return MakeSkinMesh(pChunk, pModel);
}

struct NodeStack
{
    AVLTreeNode** m_Stack;
    unsigned int m_Count;
};

static inline void UpdateTextureAnims(GLInventory* self, float dt)
{
    NodeStack* stack = NULL;
    for (int i = self->m_nLevel; i >= 0; i--)
    {
        nlAVLTree<unsigned long, GLTextureAnim*, DefaultKeyCompare<unsigned long> >* tree = self->m_pTextureAnims[i]->m_pItems;

        if (tree->m_Root != NULL)
        {
            stack = (NodeStack*)nlMalloc(sizeof(NodeStack), 8, false);
            if (stack != NULL)
            {
                unsigned int numElements = tree->m_NumElements;
                AVLTreeEntry<unsigned long, GLTextureAnim*>* node = tree->m_Root;

                stack->m_Stack = (AVLTreeNode**)nlMalloc((numElements + 1) * sizeof(AVLTreeEntry<unsigned long, GLTextureAnim*>*), 8, false);
                stack->m_Count = 0;

                if (node != NULL)
                {
                    while (node->node.left != NULL)
                    {
                        stack->m_Stack[stack->m_Count] = (AVLTreeNode*)node;
                        stack->m_Count++;
                        node = (AVLTreeEntry<unsigned long, GLTextureAnim*>*)node->node.left;
                    }

                    stack->m_Stack[stack->m_Count] = (AVLTreeNode*)node;
                    stack->m_Count++;
                }
            }

            while (stack->m_Count != 0)
            {
                ((AVLTreeEntry<unsigned long, GLTextureAnim*>*)stack->m_Stack[stack->m_Count - 1])->value->Update(dt);
                stack->m_Count--;

                AVLTreeEntry<unsigned long, GLTextureAnim*>* right = (AVLTreeEntry<unsigned long, GLTextureAnim*>*)((AVLTreeEntry<unsigned long, GLTextureAnim*>*)stack->m_Stack[stack->m_Count])->node.right;
                if (right != NULL)
                {
                    while (right->node.left != NULL)
                    {
                        stack->m_Stack[stack->m_Count] = (AVLTreeNode*)right;
                        stack->m_Count++;
                        right = (AVLTreeEntry<unsigned long, GLTextureAnim*>*)right->node.left;
                    }

                    stack->m_Stack[stack->m_Count] = (AVLTreeNode*)right;
                    stack->m_Count++;
                }
            }

            if (stack != NULL)
            {
                delete[] stack->m_Stack;
                delete stack;
            }
        }
    }
}

static inline void UpdateVertexAnims(GLInventory* self, float dt)
{
    int i;
    NodeStack* stack = NULL;
    for (i = self->m_nLevel; i >= 0; i--)
    {
        nlAVLTree<unsigned long, GLVertexAnim*, DefaultKeyCompare<unsigned long> >* tree = self->m_pVertexAnims[i]->m_pItems;

        if (tree->m_Root != NULL)
        {
            stack = (NodeStack*)nlMalloc(sizeof(NodeStack), 8, false);
            if (stack != NULL)
            {
                unsigned int numElements = tree->m_NumElements;
                AVLTreeEntry<unsigned long, GLVertexAnim*>* node = tree->m_Root;

                stack->m_Stack = (AVLTreeNode**)nlMalloc((numElements + 1) * sizeof(AVLTreeEntry<unsigned long, GLVertexAnim*>*), 8, false);
                stack->m_Count = 0;

                if (node != NULL)
                {
                    while (node->node.left != NULL)
                    {
                        stack->m_Stack[stack->m_Count] = (AVLTreeNode*)node;
                        stack->m_Count++;
                        node = (AVLTreeEntry<unsigned long, GLVertexAnim*>*)node->node.left;
                    }

                    stack->m_Stack[stack->m_Count] = (AVLTreeNode*)node;
                    stack->m_Count++;
                }
            }

            while (stack->m_Count != 0)
            {
                ((AVLTreeEntry<unsigned long, GLVertexAnim*>*)stack->m_Stack[stack->m_Count - 1])->value->Update(dt);
                stack->m_Count--;

                AVLTreeEntry<unsigned long, GLVertexAnim*>* right = (AVLTreeEntry<unsigned long, GLVertexAnim*>*)((AVLTreeEntry<unsigned long, GLVertexAnim*>*)stack->m_Stack[stack->m_Count])->node.right;
                if (right != NULL)
                {
                    while (right->node.left != NULL)
                    {
                        stack->m_Stack[stack->m_Count] = (AVLTreeNode*)right;
                        stack->m_Count++;
                        right = (AVLTreeEntry<unsigned long, GLVertexAnim*>*)right->node.left;
                    }

                    stack->m_Stack[stack->m_Count] = (AVLTreeNode*)right;
                    stack->m_Count++;
                }
            }

            if (stack != NULL)
            {
                delete[] stack->m_Stack;
                delete stack;
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x801E2298 | size: 0x334
 */
void GLInventory::Update(float deltaTime)
{
    UpdateTextureAnims(this, deltaTime);
    UpdateVertexAnims(this, deltaTime);
}
