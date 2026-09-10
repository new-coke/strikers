#ifndef _NLRING_H_
#define _NLRING_H_

// Singly Linked Ring

#include "types.h"

template <typename T>
void nlRingAddStart(T** list, T* item)
{
    T* head = *list;
    if (head == nullptr)
    {
        *list = item;
        item->m_next = item;
        return;
    }

    item->m_next = head->m_next;
    head = *list;
    head->m_next = item;
}

template <typename T>
void nlRingAddEnd(T** list, T* item)
{
    nlRingAddStart(list, item);
    *list = item;
}

template <typename T>
T* nlRingGetStart(T* head)
{
    if (head == NULL)
    {
        return NULL;
    }
    return head->m_next;
}

template <typename T>
bool nlRingIsEnd(T* head, T* element)
{
    if (head == NULL)
    {
        return true;
    }
    return head == element;
}

template <typename T>
u32 nlRingCountElements(T* head)
{
    T* current;
    u32 count = 0;

    if (head == NULL)
    {
        return 0;
    }

    current = head->m_next;
    while (true)
    {
        T* next = current->m_next;
        count++;
        if (current == head)
        {
            break;
        }
        current = next;
    }
    return count;
}

template <typename T>
void nlDeleteRing(T** head)
{
    T* current;
    T* next;

    T* headPtr = *head;
    if (headPtr != NULL)
    {
        current = headPtr->m_next;
        for (;;)
        {
            next = current->m_next;
            delete current;
            if (current != *head)
            {
                current = next;
            }
            else
            {
                break;
            }
        }
        *head = NULL;
    }
}
template <typename T, typename CallbackType>
void nlWalkRing(T* head, CallbackType* callback, void (CallbackType::*callbackFunc)(T*))
{
    if (head == 0)
    {
        return;
    }

    T* current = head->m_next;
    while (true)
    {
        T* next = current->m_next;
        (callback->*callbackFunc)(current);
        if (current == head)
        {
            break;
        }
        current = next;
    }
}

#endif // _NLRING_H_
