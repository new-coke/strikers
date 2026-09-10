#ifndef _NLLISTCONTAINER_H_
#define _NLLISTCONTAINER_H_

// Home header for ListContainerBase and nlListContainer. The original compiler
// grouped weak instantiations into
// linkonce sections keyed by the BODY's file; DWARF attributes
// ListContainerBase::DeleteEntry and ~ListContainerBase to
// core/nlListContainer.h, so their bodies live here rather than nlList.h for
// a TU's .text section grouping to match the target.
//
// Include via NL/nlList.h (which includes this at its end) - ListEntry and
// nlWalkList must be visible first.

template <typename T>
class nlListIterator;

template <typename T>
class nlListConstIterator;

template <typename T, typename Adapter>
class ListContainerBase
{
public:
    ListContainerBase()
        : m_Head(NULL)
        , m_Tail(NULL)
    {
    }

    typedef void (ListContainerBase::*ENTRY_FUNC)(ListEntry<T>*);

    ~ListContainerBase()
    {
        Clear();
    }

    void Clear()
    {
        ENTRY_FUNC func = &ListContainerBase::DeleteEntry;
        nlWalkList(m_Head, this, func);
        m_Head = NULL;
        m_Tail = NULL;
    }

    void DeleteEntry(ListEntry<T>* entry);

    void RemoveEntry(const T& data)
    {
        ListEntry<T>* currentEntry = m_Head;
        if (currentEntry != NULL)
        {
            if (currentEntry->Entry() == data)
            {
                ListEntry<T>* newHead;
                if (currentEntry == m_Tail)
                    m_Tail = newHead = NULL;
                else
                    newHead = currentEntry->next;
                DeleteEntry(m_Head);
                m_Head = newHead;
            }
            else
            {
                ListEntry<T>* previousEntry = currentEntry;
                nlListIterator<T> iterator(currentEntry->next);
                while (iterator.IsValid())
                {
                    ListEntry<T>* nextEntry = iterator.CurrentEntry();
                    if (iterator.Current() == data)
                    {
                        previousEntry->next = nextEntry->next;
                        if (nextEntry == m_Tail)
                            m_Tail = previousEntry;
                        DeleteEntry(nextEntry);
                        break;
                    }
                    previousEntry = nextEntry;
                    iterator.Next();
                }
            }
        }
    }

    ListEntry<T>* Allocate(const T& data)
    {
        ListEntry<T> localEntry(data);
        ListEntry<T>* entry;
        m_Allocator.AllocateForReturn(entry);
        if (entry != NULL)
        {
            *entry = localEntry;
        }
        return entry;
    }

    void AddEntry(const T& data)
    {
        ListEntry<T> value(data);
        ListEntry<T>* entry = m_Allocator.New(value);
        nlListAddStart<ListEntry<T> >(&m_Head, entry, &m_Tail);
    }

    void AddStart(const T& data)
    {
        ListEntry<T> value(data);
        ListEntry<T>* entry = m_Allocator.Allocate();
        if (entry != NULL)
        {
            *entry = value;
        }
        nlListAddStart<ListEntry<T> >(&m_Head, entry, &m_Tail);
    }

    void AddEnd(const T& data)
    {
        ListEntry<T>* entry = m_Allocator.New(ListEntry<T>(data));
        nlListAddEnd<ListEntry<T> >(&m_Head, &m_Tail, entry);
    }

    ListEntry<T>* RemoveStart()
    {
        return nlListRemoveStart<ListEntry<T> >(&m_Head, &m_Tail);
    }

    nlListIterator<T> Begin();
    nlListConstIterator<T> Begin() const;

    // offsets and sizes are dependent on the adapter
    /* 0x0 */ Adapter m_Allocator;
    ListEntry<T>* m_Head;
    ListEntry<T>* m_Tail;
};

template <typename T>
class nlListConstIterator
{
public:
    nlListConstIterator(ListEntry<T>* current)
        : m_Curr(current)
    {
    }

    bool IsValid() const
    {
        return m_Curr != NULL;
    }

    T Current() const
    {
        return m_Curr->entry;
    }

    void Next()
    {
        m_Curr = m_Curr->next;
    }

    ListEntry<T>* CurrentEntry() const
    {
        return m_Curr;
    }

private:
    /* 0x0 */ ListEntry<T>* m_Curr;
};

template <typename T>
class nlListIterator
{
public:
    nlListIterator(ListEntry<T>* current)
        : m_Curr(current)
    {
    }

    bool IsValid() const
    {
        return m_Curr != NULL;
    }

    T& Current() const
    {
        return m_Curr->entry;
    }

    void Next()
    {
        m_Curr = m_Curr->next;
    }

    ListEntry<T>* CurrentEntry() const
    {
        return m_Curr;
    }

private:
    /* 0x0 */ ListEntry<T>* m_Curr;
};

template <typename T, typename Adapter>
inline nlListIterator<T> ListContainerBase<T, Adapter>::Begin()
{
    return nlListIterator<T>(m_Head);
}

template <typename T, typename Adapter>
inline nlListConstIterator<T> ListContainerBase<T, Adapter>::Begin() const
{
    return nlListConstIterator<T>(m_Head);
}

template <typename T, typename Adapter>
inline void ListContainerBase<T, Adapter>::DeleteEntry(ListEntry<T>* entry)
{
    m_Allocator.DeleteEntry(entry);
}

template <typename T>
class nlListContainer : public ListContainerBase<T, NewAdapter<ListEntry<T> > >
{
}; // total size: 0xC

template <typename T>
class nlListSlotPool : public ListContainerBase<T, BasicSlotPool<ListEntry<T> > >
{
public:
    nlListSlotPool()
        : ListContainerBase<T, BasicSlotPool<ListEntry<T> > >()
    {
    }

    nlListSlotPool(int initial, int delta)
        : ListContainerBase<T, BasicSlotPool<ListEntry<T> > >()
    {
        this->m_Allocator.m_Initial = initial;
        SlotPoolBase::BaseAddNewBlock(&this->m_Allocator, sizeof(ListEntry<T>));
        this->m_Allocator.m_Delta = delta;
    }
};

template <typename T>
class nlListSlotPoolHigh
    : public ListContainerBase<T, BasicSlotPoolHigh<ListEntry<T> > >
{
public:
    nlListSlotPoolHigh()
        : ListContainerBase<T, BasicSlotPoolHigh<ListEntry<T> > >()
    {
    }

    nlListSlotPoolHigh(int initial, int delta)
        : ListContainerBase<T, BasicSlotPoolHigh<ListEntry<T> > >()
    {
        this->m_Allocator.m_Initial = initial;
        SlotPoolBase::BaseAddNewBlock(
            &this->m_Allocator, sizeof(ListEntry<T>));
        this->m_Allocator.m_Delta = delta;
    }
};

#endif // _NLLISTCONTAINER_H_
