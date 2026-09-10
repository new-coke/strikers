#ifndef _NLLIST_H_
#define _NLLIST_H_

#include "types.h"

#include "NL/NewAdapter.h"
#include "NL/nlDLRing.h"
#include "NL/nlSlotPool.h"

template <typename T>
class ListEntry
{
public:
    /* 0x00 */ ListEntry<T>* next;
    /* 0x04 */ T entry;

    ListEntry()
        : next(nullptr)
        , entry(nullptr)
    {
    }

    ListEntry(const T& data)
        : next(nullptr)
        , entry(data)
    {
    }

    const T& Entry() const
    {
        return entry;
    }
};

template <typename T>
ListEntry<T>* nlListRemoveStart(ListEntry<T>** head, ListEntry<T>** tail)
{
    ListEntry<T>* first = *head;
    if (first == 0)
        return 0;

    if (tail != 0)
    {
        if (*tail == first)
            *tail = 0;
    }

    // Remove first element
    ListEntry<T>* tmp = *head;
    *head = tmp->next;
    return tmp;
}

template <typename T>
void nlListAddStart(T** head, T* newNode, T** prev)
{
    if ((prev != NULL) && ((T*)*head == NULL))
    {
        *prev = newNode;
    }
    newNode->next = *head;
    *head = newNode;
}

template <typename T>
void nlListAddEnd(T** head, T** tail, T* node)
{
    node->next = NULL;
    T* temp_r6 = *tail;
    if (temp_r6 != NULL)
    {
        temp_r6->next = node;
        *tail = node;
        return;
    }
    *tail = node;
    *head = node;
}

template <typename T>
T* nlListRemoveStart(T** head, T** tail)
{
    T* current_node = *head;
    if (current_node == NULL)
    {
        return NULL;
    }
    if ((tail != NULL) && (*tail == current_node))
    {
        *tail = NULL;
    }
    T* node_to_remove = *head;
    *head = node_to_remove->next;
    return node_to_remove;
}

template <typename T>
class nlList
{
public:
    nlList(T* start, T* end)
    {
        m_pEnd = end;
        m_pStart = start;
    }
    nlList() { }
    T* m_pStart; // offset 0x0, size 0x4
    T* m_pEnd;   // offset 0x4, size 0x4
}; // total size: 0x8

template <typename T>
T* nlListRemoveElement(T** head, T* element, T** tail)
{
    if (head == NULL)
        return NULL;

    if (*head == element)
    {
        if (tail != NULL && *tail == *head)
        {
            *tail = NULL;
        }
        T* first = *head;
        *head = first->next;
        return NULL;
    }

    T* prev = *head;
    T* current = prev->next;
    while (current != NULL)
    {
        if (current == element)
        {
            prev->next = current->next;
            if (tail == NULL)
                return prev;
            if (*tail == current)
            {
                *tail = prev;
            }
            return prev;
        }
        prev = current;
        current = current->next;
    }
    return NULL;
}
// Defined after nlListRemoveElement: MWCC emits linkonce template
// instantiations in reverse definition order, and the original DOL has
// nlDeleteList<T> before nlListRemoveElement<T> in every TU that uses both.
template <typename T>
void nlDeleteList(T** head)
{
    T* next;
    while (*head != NULL)
    {
        next = (*head)->next;
        delete *head;
        *head = next;
    }
    *head = NULL;
}

template <typename T>
inline void nlDeleteList(nlList<T>& list)
{
    while (list.m_pStart != NULL)
    {
        delete nlListRemoveStart<T>(&list.m_pStart, &list.m_pEnd);
    }
}

template <typename T>
s32 nlListCountElements(T* head)
{
    T* node = head;
    s32 count = 0;
    while (node != NULL)
    {
        node = node->next;
        count += 1;
    }
    return count;
}

template <typename EntryT, typename ContainerT>
void nlWalkList(EntryT* list, ContainerT* cbClass, void (ContainerT::*cb)(EntryT*))
{
    while (list != NULL)
    {
        EntryT* next = list->next;
        (cbClass->*cb)(list);
        list = next;
    }
}

// ListContainerBase / nlListContainer bodies live in their original home
// header (linkonce grouping is keyed by the body's file).
#include "NL/nlListContainer.h"

#include "NL/WalkHelper.h"

#endif
