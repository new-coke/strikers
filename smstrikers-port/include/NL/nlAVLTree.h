#ifndef _NLAVLTREE_H_
#define _NLAVLTREE_H_

#include "NL/NewAdapter.h"
#include "NL/nlMemory.h"
#include "NL/nlSlotPool.h"

#include "dolphin/types.h"
#include "types.h"

struct AVLTreeNode
{
    /* 0x0 */ AVLTreeNode* left;
    /* 0x4 */ AVLTreeNode* right;
    /* 0x8 */ s8 heavy;
}; // total size: 0xC

class AVLTreeUntemplated
{
public:
    virtual int CompareNodes(AVLTreeNode* node1, AVLTreeNode* node2) = 0;
    virtual int CompareKey(void* key, AVLTreeNode* n) = 0;
    virtual AVLTreeNode* AllocateEntry(void* key, void* value) = 0;

    AVLTreeNode* RemoveAVLNode(AVLTreeNode** root, void* key, unsigned int height);
    u32 AddAVLNode(AVLTreeNode** rootNode, void* key, void* value, AVLTreeNode** existingNode, unsigned int height);
};

template <typename KeyType, typename ValueType>
class AVLTreeEntry
{
public:
    AVLTreeNode node; // offset 0x0, size 0xC
    KeyType key;      // offset 0xC, size 0x4
    ValueType value;  // offset 0x10, size 0x4
}; // total size: 0x14

// DefaultKeyCompare template implementation
template <typename KeyType>
class DefaultKeyCompare
{
public:
    int operator()(const KeyType& key2, const KeyType& key1) const
    {
        int result;
        if (key1 == key2)
            result = 0;
        else if (key1 < key2)
            result = -1;
        else
            result = 1;
        return result;
    }
};

template <typename KeyType, typename ValueType, typename CompareType>
class nlAVLTreeIterator;

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
class AVLTreeBase : public AVLTreeUntemplated
{
public:
    AVLTreeBase()
    {
        m_NumElements = 0;
        m_Root = nullptr;
        m_Compare = nullptr;
    }

    ~AVLTreeBase();

    void Clear();

    void DeleteValues();

    inline void Find(const KeyType& key, ValueType** foundValue,
        KeyType** foundKey) const;
    inline ValueType* FindGet(const KeyType& key) const;
    inline bool FindGet(const KeyType& key, ValueType** foundValue) const;
    inline ValueType* Add(const KeyType& key, const ValueType& value);
    inline void Remove(const KeyType& key);

    inline nlAVLTreeIterator<KeyType, ValueType, CompareType>* GetIterator();

    typedef void (AVLTreeBase::*ENTRY_DELETE_FUNC)(AVLTreeEntry<KeyType, ValueType>*);

    template <typename CallbackType>
    void Walk(CallbackType* cbClass,
        void (CallbackType::*cb)(const KeyType&, ValueType*))
    {
        InorderWalk(m_Root, cbClass, cb);
    }

    void DeleteEntry(AVLTreeEntry<KeyType, ValueType>* entry);
    void DeleteValue(AVLTreeEntry<KeyType, ValueType>* entry);
    void DestroyTree(ENTRY_DELETE_FUNC deleteFunc);
    AVLTreeEntry<KeyType, ValueType>* CastUp(AVLTreeNode* node) const;

    template <typename CallbackType>
    void InorderWalk(AVLTreeEntry<KeyType, ValueType>* curr,
        CallbackType* cbClass,
        void (CallbackType::*cb)(const KeyType&, ValueType*))
    {
        while (curr != nullptr)
        {
            InorderWalk(CastUp(curr->node.left), cbClass, cb);
            (cbClass->*cb)(curr->key, &curr->value);
            curr = CastUp(curr->node.right);
        }
    }

    void PostorderTraversal(AVLTreeEntry<KeyType, ValueType>* curr,
        ENTRY_DELETE_FUNC cb);

    virtual int CompareNodes(AVLTreeNode* a, AVLTreeNode* b);
    virtual int CompareKey(void* key, AVLTreeNode* node);
    virtual AVLTreeNode* AllocateEntry(void* key, void* value);

public:
    /* 0x04 */ AllocatorType m_Allocator;
    /* 0x08 */ AVLTreeEntry<KeyType, ValueType>* m_Root;
    /* 0x0C */ CompareType* m_Compare;
    /* 0x10 */ unsigned int m_NumElements;
}; // total size: 0x14

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline void AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::Find(
    const KeyType& key, ValueType** foundValue, KeyType** foundKey) const
{
    AVLTreeEntry<KeyType, ValueType>* node = m_Root;

    while (node != nullptr)
    {
        const KeyType& nodeKey = node->key;
        CompareType compare;
        int cmpResult = compare(nodeKey, key);

        if (cmpResult == 0)
        {
            if (foundValue != nullptr)
            {
                *foundValue = &node->value;
            }
            if (foundKey != nullptr)
            {
                *foundKey = &node->key;
            }
            break;
        }
        else if (cmpResult < 0)
        {
            node = (AVLTreeEntry<KeyType, ValueType>*)node->node.left;
        }
        else
        {
            node = (AVLTreeEntry<KeyType, ValueType>*)node->node.right;
        }
    }
}

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline ValueType* AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::Add(
    const KeyType& key, const ValueType& value)
{
    AVLTreeNode* existingNode;
    AddAVLNode((AVLTreeNode**)&m_Root, (void*)&key, (void*)&value, &existingNode, m_NumElements);
    if (existingNode == NULL)
    {
        m_NumElements++;
        return NULL;
    }
    return &CastUp(existingNode)->value;
}

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline void AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::Remove(
    const KeyType& key)
{
    AVLTreeNode* removedNode = RemoveAVLNode((AVLTreeNode**)&m_Root, (void*)&key, m_NumElements);
    if (removedNode != NULL)
    {
        m_Allocator.Free(CastUp(removedNode));
        m_NumElements--;
    }
}

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::~AVLTreeBase()
{
    Clear();
}

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline void AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::Clear()
{
    DestroyTree(&AVLTreeBase::DeleteEntry);
    m_NumElements = 0;
}

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline void AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::DeleteValues()
{
    DestroyTree(&AVLTreeBase::DeleteValue);
    m_NumElements = 0;
}

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline ValueType* AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::FindGet(
    const KeyType& key) const
{
    ValueType* foundValue;
    AVLTreeEntry<KeyType, ValueType>* node = m_Root;

    while (node != nullptr)
    {
        int cmpResult;
        if (key == node->key)
        {
            cmpResult = 0;
        }
        else if (key < node->key)
        {
            cmpResult = -1;
        }
        else
        {
            cmpResult = 1;
        }

        if (cmpResult == 0)
        {
            if (&foundValue != nullptr)
            {
                foundValue = &node->value;
            }
            break;
        }
        else if (cmpResult < 0)
        {
            node = (AVLTreeEntry<KeyType, ValueType>*)node->node.left;
        }
        else
        {
            node = (AVLTreeEntry<KeyType, ValueType>*)node->node.right;
        }
    }

    return foundValue;
}

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline bool AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::FindGet(
    const KeyType& key, ValueType** foundValue) const
{
    AVLTreeEntry<KeyType, ValueType>* node = m_Root;

    while (node != nullptr)
    {
        const KeyType& nodeKey = node->key;
        CompareType compare;
        int cmpResult = compare(nodeKey, key);

        if (cmpResult == 0)
        {
            if (foundValue != nullptr)
            {
                *foundValue = &node->value;
            }
            return true;
        }
        else if (cmpResult < 0)
        {
            node = (AVLTreeEntry<KeyType, ValueType>*)node->node.left;
        }
        else
        {
            node = (AVLTreeEntry<KeyType, ValueType>*)node->node.right;
        }
    }

    return false;
}

template <typename KeyType, typename ValueType, typename CompareType>
class nlAVLTree
    : public AVLTreeBase<KeyType, ValueType,
          NewAdapter<AVLTreeEntry<KeyType, ValueType> >, CompareType>
{
};

template <typename KeyType, typename ValueType, typename CompareType>
class nlAVLTreeSlotPool
    : public AVLTreeBase<KeyType, ValueType,
          BasicSlotPool<AVLTreeEntry<KeyType, ValueType> >, CompareType>
{
public:
    nlAVLTreeSlotPool()
    {
    }

    nlAVLTreeSlotPool(int initial, int delta)
    {
        this->m_Allocator.Initialize(initial, delta);
    }
};

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline void AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::DeleteEntry(
    AVLTreeEntry<KeyType, ValueType>* entry)
{
    m_Allocator.Free(entry);
}

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline void AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::DeleteValue(
    AVLTreeEntry<KeyType, ValueType>* entry)
{
    delete entry->value;
    m_Allocator.Delete(entry);
}

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline void AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::DestroyTree(
    ENTRY_DELETE_FUNC deleteFunc)
{
    if (m_Root != NULL)
    {
        PostorderTraversal(m_Root, deleteFunc);
        m_Root = nullptr;
        m_NumElements = 0;
    }
}

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline AVLTreeEntry<KeyType, ValueType>*
AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::CastUp(
    AVLTreeNode* node) const
{
    return (AVLTreeEntry<KeyType, ValueType>*)node;
}

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline void AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::PostorderTraversal(
    AVLTreeEntry<KeyType, ValueType>* curr,
    ENTRY_DELETE_FUNC cb)
{
    if (curr->node.left != NULL)
        PostorderTraversal(CastUp(curr->node.left), cb);
    if (curr->node.right != NULL)
        PostorderTraversal(CastUp(curr->node.right), cb);
    (this->*cb)(curr);
}

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline int AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::CompareNodes(
    AVLTreeNode* a, AVLTreeNode* b)
{
    const KeyType& keyA = ((AVLTreeEntry<KeyType, ValueType>*)a)->key;
    const KeyType& keyB = ((AVLTreeEntry<KeyType, ValueType>*)b)->key;
    CompareType compare;
    return compare(keyB, keyA);
}

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline int AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::CompareKey(
    void* key, AVLTreeNode* node)
{
    const KeyType& k = *(KeyType*)key;
    AVLTreeEntry<KeyType, ValueType>* entry = (AVLTreeEntry<KeyType, ValueType>*)node;
    CompareType compare;
    return compare(entry->key, k);
}

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline AVLTreeNode* AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::AllocateEntry(
    void* key, void* value)
{
    AVLTreeEntry<KeyType, ValueType>* newNode = NULL;
    m_Allocator.Allocate(newNode);
    newNode->node.left = NULL;
    newNode->node.right = NULL;
    newNode->node.heavy = 0;
    newNode->key = *(KeyType*)key;
    newNode->value = *(ValueType*)value;
    return (AVLTreeNode*)newNode;
}

template <typename KeyType, typename ValueType, typename CompareType>
class nlAVLTreeIterator
{
public:
    typedef AVLTreeEntry<KeyType, ValueType> Entry;

    template <typename AllocatorType>
    nlAVLTreeIterator(
        AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>& tree)
    {
        Entry* node = tree.m_Root;
        m_Stack = (Entry**)nlMalloc((tree.m_NumElements + 1) * sizeof(Entry*), 8, false);
        m_NumStackEntries = 0;
        if (node != NULL)
        {
            while (node->node.left != NULL)
            {
                m_Stack[m_NumStackEntries] = node;
                m_NumStackEntries++;
                node = (Entry*)node->node.left;
            }
            m_Stack[m_NumStackEntries] = node;
            m_NumStackEntries++;
        }
    }

    nlAVLTreeIterator(Entry* node, unsigned int numElements)
    {
        m_Stack = (Entry**)nlMalloc((numElements + 1) * sizeof(Entry*), 8, false);
        m_NumStackEntries = 0;
        if (node != NULL)
        {
            PushLeft(node);
        }
    }

    ~nlAVLTreeIterator()
    {
        delete[] m_Stack;
    }

    void Next()
    {
        m_NumStackEntries--;
        Entry* entry = m_Stack[m_NumStackEntries];
        Entry* right = (Entry*)entry->node.right;
        if (right != NULL)
        {
            PushLeft(right);
        }
    }

    inline void PushLeft(Entry* entry);

    bool IsValid() const
    {
        return m_NumStackEntries > 0;
    }

    Entry* Current() const
    {
        return m_Stack[m_NumStackEntries - 1];
    }

    ValueType& CurrentValue() const
    {
        return m_Stack[m_NumStackEntries - 1]->value;
    }

    /* 0x0 */ Entry** m_Stack;
    /* 0x4 */ u32 m_NumStackEntries;
}; // total size: 0x8

template <typename KeyType, typename ValueType, typename CompareType>
inline void nlAVLTreeIterator<KeyType, ValueType, CompareType>::PushLeft(
    AVLTreeEntry<KeyType, ValueType>* entry)
{
    while (entry->node.left != NULL)
    {
        m_Stack[m_NumStackEntries] = entry;
        m_NumStackEntries++;
        entry = (AVLTreeEntry<KeyType, ValueType>*)entry->node.left;
    }
    m_Stack[m_NumStackEntries] = entry;
    m_NumStackEntries++;
}

template <typename KeyType, typename ValueType, typename AllocatorType, typename CompareType>
inline nlAVLTreeIterator<KeyType, ValueType, CompareType>*
AVLTreeBase<KeyType, ValueType, AllocatorType, CompareType>::GetIterator()
{
    typedef nlAVLTreeIterator<KeyType, ValueType, CompareType> Iterator;
    return new (8, false) Iterator(m_Root, m_NumElements);
}

#endif // _NLAVLTREE_H_
