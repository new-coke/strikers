#ifndef NEW_ADAPTER_H
#define NEW_ADAPTER_H

#include "NL/nlMemory.h"

template <typename T>
class NewAdapter
{
public:
    // For AVL Tree interface
    T* Allocate() { return (T*)nlMalloc(sizeof(T), 8, false); }
    void Allocate(T*& out) { out = (T*)nlMalloc(sizeof(T), 8, false); }
    T* New(const T& data) { return new (8, false) T(data); }
    void Free(T* ptr) { delete ptr; }
    void Delete(T* ptr);

    // For List interface
    typedef T EntryType;
    static void DeleteEntry(T* entry) { delete entry; }
};

template <typename T>
void NewAdapter<T>::Delete(T* ptr)
{
    delete ptr;
}

#endif // NEW_ADAPTER_H
