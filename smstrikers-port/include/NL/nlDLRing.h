#ifndef _NLDLRING_H_
#define _NLDLRING_H_

// Doubly Linked Ring

#include "types.h"
#include "NL/nlRing.h"

template <typename T>
class DLListEntry
{
public:
    /* 0x0 */ DLListEntry<T>* m_next;
    /* 0x4 */ DLListEntry<T>* m_prev;
    /* 0x8 */ T entry;

    DLListEntry()
        : m_next(nullptr)
        , m_prev(nullptr)
        , entry()
    {
    }

    DLListEntry(T* data)
        : m_next(nullptr)
        , m_prev(nullptr)
        , entry(data)
    {
    }

    DLListEntry(const T& data)
        : m_next(nullptr)
        , m_prev(nullptr)
        , entry(data)
    {
    }
}; // total size: 0xC

template <typename T>
void nlDLRingInsert(T** head, T* afterNode, T* newNode)
{

    afterNode->m_next->m_prev = newNode;
    newNode->m_next = afterNode->m_next;
    newNode->m_prev = afterNode;
    afterNode->m_next = newNode;
    if ((void*)*head == afterNode)
    {
        *head = newNode;
    }
}

template <typename T>
void nlDLRingAddStart(T** head, T* newNode)
{
    T* temp;

    temp = *head;
    if (temp == NULL)
    {
        *head = newNode;
        newNode->m_next = newNode;
        newNode->m_prev = newNode;
        return;
    }

    temp->m_next->m_prev = newNode;
    newNode->m_next = temp->m_next;
    newNode->m_prev = temp;
    temp->m_next = newNode;
}

template <typename T>
void nlDLRingAddEnd(T** head, T* newNode)
{
    nlDLRingAddStart(head, newNode);
    *head = newNode;
}

template <typename T>
void nlDLRingRemove(T** head, T* current)
{
    T* tmp_node = current->m_next;

    if (tmp_node == current)
    {
        *head = NULL;
        return;
    }

    current->m_prev->m_next = tmp_node;
    current->m_next->m_prev = current->m_prev;

    if (*head == current)
    {
        *head = current->m_prev;
    }
}

template <typename T>
T* nlDLRingGetStart(T* current)
{
    if (current == NULL)
    {
        return NULL;
    }
    return current->m_next;
}

template <typename T>
bool nlDLRingValidateContainsElement(T* head, const T* node)
{
    if (head == 0)
        return false;

    T* cur = head->m_next;

    for (;;)
    {
        if (node == cur)
            return true;

        T* next = cur->m_next;

        if (cur == head)
            break;

        cur = next;
    }

    return false;
}

template <typename T>
bool nlDLRingRemoveSafely(T** head, const T* node)
{
    T* next;

    if (nlDLRingValidateContainsElement(*head, node) == 0)
    {
        return false;
    }

    next = node->m_next;
    if (next == node)
    {
        *head = NULL;
        return 1;
    }
    node->m_prev->m_next = next;
    node->m_next->m_prev = node->m_prev;
    if (*head == node)
    {
        *head = node->m_prev;
        return true;
    }

    return true;
}

template <typename T>
T* nlDLRingGetEnd(T* current)
{
    if (current != 0)
    {
        return current;
    }
    return NULL;
}

template <typename T>
bool nlDLRingIsEnd(T* head, T* current)
{
    if (head == NULL)
    {
        return true;
    }
    return head == current;
}

template <typename T>
u32 nlDLRingCountElements(T* head)
{
    return nlRingCountElements(head);
}

template <typename T>
bool nlDLRingIsStart(T* head, T* current)
{
    if (head == NULL)
    {
        return false;
    }
    return head->m_next == current;
}

template <typename T>
T* nlDLRingRemoveStart(T** current)
{
    T* temp_r31;
    temp_r31 = (*current)->m_next;
    nlDLRingRemove<T>(current, temp_r31);
    return temp_r31;
}

template <typename T>
T* nlDLRingRemoveEnd(T** head)
{
    T* node = *head;
    nlDLRingRemove<T>(head, node);
    return node;
}

template <typename T>
void nlDLRingAppendRing(T** head, T* current)
{
    T* currentHead;
    T* nextAfterHead;

    if (current != NULL)
    {
        currentHead = *head;
        if (currentHead == NULL)
        {
            *head = current;
            return;
        }

        nextAfterHead = currentHead->m_next;
        currentHead->m_next = current->m_next;
        current->m_next->m_prev = *head;
        current->m_next = nextAfterHead;
        nextAfterHead->m_prev = current;
        *head = current;
    }
}

template <typename T>
void nlDeleteDLRing(T** head)
{
    nlDeleteRing<T>(head);
}
template <typename T, typename CallbackType>
void nlWalkDLRing(T* head, CallbackType* callback, void (CallbackType::*callbackFunc)(T*))
{
    void (CallbackType::*func)(T*) = callbackFunc;
    nlWalkRing(head, callback, func);
}

// =======================================================
// Iterator for nlDLRing
// =======================================================

template <typename T>
class nlDLListIterator
{
public:
    typedef T ValueType;
    typedef DLListEntry<T>* Pointer;
    typedef T& Reference;

    nlDLListIterator() { }

    nlDLListIterator(Pointer head)
    {
        m_Head = head;
        m_Curr = nlDLRingGetStart(head);
    }

    nlDLListIterator(Pointer head, Pointer current)
    {
        m_Curr = current;
        m_Head = head;
    }

    Reference operator*() const { return m_Curr->entry; }

    Pointer CurrentEntry() const { return m_Curr; }

    bool hasNext() const { return m_Curr != 0; }

    Pointer next()
    {
        Pointer ret = m_Curr;
        if ((nlDLRingIsEnd(m_Head, m_Curr) != 0) || (m_Curr == NULL))
            m_Curr = NULL;
        else
            m_Curr = m_Curr->m_next;
        return ret;
    }

    void reset()
    {
        m_Curr = nlDLRingGetStart(m_Head);
    }

    Pointer m_Head;
    Pointer m_Curr;
};

#endif // _NLDLRING_H_
