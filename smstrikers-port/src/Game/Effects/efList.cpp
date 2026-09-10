#include "Game/Effects/efList.h"

/**
 * Offset/Address/Size: 0xE8 | 0x801FA93C | size: 0x44
 */
void efBaseList::Insert(efBaseNode* node)
{
    efBaseNode* head = m_headNode;
    if (head != nullptr)
    {
        head->m_prevNode = node;
    }

    node->m_nextNode = m_headNode;
    node->m_prevNode = nullptr;
    m_headNode = node;

    if (m_tailNode == nullptr)
    {
        m_tailNode = node;
    }

    m_numNodes++;
}

/**
 * Offset/Address/Size: 0xA4 | 0x801FA8F8 | size: 0x44
 */
void efBaseList::Append(efBaseNode* node)
{
    efBaseNode* tail = m_tailNode;
    if (tail != nullptr)
    {
        tail->m_nextNode = node;
    }

    node->m_prevNode = m_tailNode;
    node->m_nextNode = nullptr;
    m_tailNode = node;

    if (m_headNode == nullptr)
    {
        m_headNode = node;
    }

    m_numNodes++;
}

/**
 * Offset/Address/Size: 0x54 | 0x801FA8A8 | size: 0x50
 */
efBaseNode* efBaseList::Remove()
{
    efBaseNode* new_head;
    efBaseNode* prev_head;

    prev_head = m_headNode;
    if (prev_head == NULL)
    {
        return NULL;
    }
    m_headNode = prev_head->m_nextNode;
    new_head = m_headNode;
    if (new_head == NULL)
    {
        m_tailNode = NULL;
    }
    else
    {
        new_head->m_prevNode = NULL;
    }
    m_numNodes--;
    return prev_head;
}

/**
 * Offset/Address/Size: 0x0 | 0x801FA854 | size: 0x54
 */
efBaseNode* efBaseList::Remove(efBaseNode* node)
{
    efBaseNode* prev = node->m_prevNode;
    if (prev != nullptr)
    {
        prev->m_nextNode = node->m_nextNode;
    }
    else
    {
        m_headNode = node->m_nextNode;
    }

    efBaseNode* next = node->m_nextNode;
    if (next != nullptr)
    {
        next->m_prevNode = node->m_prevNode;
    }
    else
    {
        m_tailNode = node->m_prevNode;
    }

    m_numNodes--;
    return node;
}
