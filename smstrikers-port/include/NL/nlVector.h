#ifndef _NLVECTOR_H_
#define _NLVECTOR_H_

#include "NL/nlMath.h"
#include "NL/nlMemory.h"

class DefaultAllocator
{
public:
    enum
    {
        kAtEnd = false
    };

    template <typename T>
    static T* New(int count, const char* name)
    {
        return NL_PORT_ARRAY_NEW(T, count, 8, kAtEnd, name);
    }

    template <typename T>
    static void Delete(T* ptr)
    {
        // PORT: paired with the placement new[] above, rather than relying on ::operator delete[] being nlFree.
        NL_PORT_ARRAY_DELETE(T, ptr);
    }

    static void* Alloc(int size)
    {
        return nlMalloc(size, 8, kAtEnd);
    }

    static void Free(void* ptr)
    {
        nlFree(ptr);
    }
};

template <typename T, typename Allocator = DefaultAllocator>
class Vector
{
public:
    Vector() { }
    Vector(const T* string)
    {
        mData = 0;
        mSize = 0;
        mCapacity = 0;

        const T* scan = string;
        while (*scan++ != 0)
        {
            mSize++;
        }

        mSize++;
        mData = Allocator::template New<T>(mSize + 1, 0);
        mCapacity = mSize;

        for (int i = 0; i < mSize; i++)
        {
            mData[i] = *string++;
        }
    }
    Vector(const Vector& other, const char* name = NULL)
    {
        mData = Allocator::template New<T>(other.mSize, name);
        mSize = other.mSize;
        mCapacity = other.mSize;
        for (int i = 0; i < mSize; i++)
        {
            mData[i] = other.mData[i];
        }
    }
    Vector(int count, const char* name)
    {
        mData = Allocator::template New<T>(count, name);
        mSize = count;
        mCapacity = count;
        for (int i = 0; i < count; i++)
        {
            mData[i] = T();
        }
    }
    ~Vector();
    Vector& operator=(const Vector& other)
    {
        if (mSize >= other.mSize)
        {
            for (int i = 0; i < other.mSize; i++)
            {
                mData[i] = other.mData[i];
            }
            mSize = other.mSize;
        }
        else
        {
            Vector tmp(other);
            Swap(tmp);
        }
        return *this;
    }
    void Swap(Vector& other);
    void push_back(const T& value)
    {
        insert(mData + mSize, &value, &value + 1);
    }
    void insert(T* at, const T* begin, const T* end)
    {
        int size = end - begin;
        int offset = at - mData;
        reserve(mSize + size);
        at = mData + offset;
        T* t = mData + mSize - 1;
        while (t >= at)
        {
            *(t + size) = *t;
            t--;
        }
        while (begin != end)
        {
            *at = *begin;
            begin++;
            at++;
        }
        mSize += size;
    }
    void reserve(int capacity)
    {
        FORCE_DONT_INLINE;
        if (mCapacity < capacity)
        {
            Vector<T, Allocator> other(capacity, 0);
            for (int i = 0; i < mSize; i++)
            {
                other.mData[i] = mData[i];
            }
            other.mSize = mSize;
            Swap(other);
        }
    }
    void resize(int size)
    {
        if (size > mSize)
        {
            reserve(size);
            for (int i = mSize; i < size; i++)
            {
                T temp;
                mData[i] = temp;
            }
            mSize = size;
        }
    }
    inline void erase(const T* first, const T* last);
    T& operator[](int index)
    {
        return mData[index];
    }

    /* 0x0 */ T* mData;
    /* 0x4 */ int mSize;
    /* 0x8 */ int mCapacity;
}; // total size: 0xC

template <typename T, typename Allocator>
inline Vector<T, Allocator>::~Vector()
{
    Allocator::template Delete<T>(mData);
}

template <typename T, typename Allocator>
inline void Vector<T, Allocator>::erase(const T* begin, const T* end)
{
    const T* eraseEnd = end;
    int size = end - begin;
    T* at;
    int offset = begin - mData;
    at = mData + offset;
    while (eraseEnd != mData + mSize)
    {
        *at++ = *eraseEnd++;
    }
    mSize -= size;
}

template <typename T, typename Allocator>
inline void Vector<T, Allocator>::Swap(Vector<T, Allocator>& other)
{
    int oldSize = mSize;
    mSize = other.mSize;
    other.mSize = oldSize;
    int oldCapacity = mCapacity;
    mCapacity = other.mCapacity;
    other.mCapacity = oldCapacity;
    T* oldData = mData;
    mData = other.mData;
    other.mData = oldData;
}

#endif // _NLVECTOR_H_
