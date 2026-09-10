#ifndef _EFLIST_H_
#define _EFLIST_H_

#include "types.h"

struct efBaseNode
{
public:
    efBaseNode()
        : m_nextNode(nullptr)
        , m_prevNode(nullptr)
    {
    }

    /* 0x0, */ efBaseNode* m_nextNode;
    /* 0x4, */ efBaseNode* m_prevNode;
};

class efNode : public efBaseNode
{
public:
    // todo: ...
}; // size: 0x8

class efBaseList
{
public:
    efBaseList()
        : m_headNode(nullptr)
        , m_tailNode(nullptr)
        , m_numNodes(0)
    {
    }

    void Insert(efBaseNode* node);
    void Append(efBaseNode* node);
    efBaseNode* Remove();
    efBaseNode* Remove(efBaseNode* node);

    /* 0x0, */ struct efBaseNode* m_headNode;
    /* 0x4, */ struct efBaseNode* m_tailNode;
    /* 0x8, */ int m_numNodes;
};

class efList : public efBaseList
{
public:
    inline efNode* Remove()
    {
        return (efNode*)efBaseList::Remove();
    }

    inline efNode* Remove(efNode* node)
    {
        return (efNode*)efBaseList::Remove(node);
    }

    inline efNode* Head() const
    {
        return (efNode*)m_headNode;
    }
}; // size: 0xC

#endif // _EFLIST_H_
