#ifndef _BASICSTRING_H_
#define _BASICSTRING_H_

#include "types.h"
#include "NL/nlMemory.h"
#include "NL/nlVector.h"
#include <string.h>
#include "NL/nlString.h"

// class Vector
// {
//     // total size: 0xC
//     char* mData;   // offset 0x0, size 0x4
//     int mSize;     // offset 0x4, size 0x4
//     int mCapacity; // offset 0x8, size 0x4
// };

// class Data
// {
//     // total size: 0x10
//     Vector mData;  // offset 0x0, size 0xC
//     int mRefCount; // offset 0xC, size 0x4
// };

// class BasicString
// {
//     // total size: 0x4
//     Data* mData; // offset 0x0, size 0x4
// };

// Forward declarations
namespace Detail
{
class TempStringAllocator;
} // namespace Detail

template <typename CharT, typename Allocator>
class BasicString;

// BasicString template class - total size: 0x4 (pointer to Data)
template <typename CharT, typename Allocator>
class BasicString
{
public:
    typedef CharT value_type;

    class Data
    {
    public:
        Data(const CharT* string)
            : mData(string)
            , mRefCount(1)
        {
        }

        Data(const CharT* begin, const CharT* end)
            : mData((int)(end - begin) + 1, 0)
            , mRefCount(1)
        {
            for (int i = 0; i < mData.mSize - 1; i++)
            {
                mData[i] = begin[i];
            }
        }

        Data(const Data& other)
            : mData(other.mData, 0)
            , mRefCount(1)
        {
        }

        void reserve(int capacity);
        void insertRange(CharT* at, const CharT* begin, const CharT* end);

        void erase(const CharT* begin, const CharT* end);

        Data* AddRef()
        {
            ++mRefCount;
            return this;
        }

        void* operator new(size_t size)
        {
            return Allocator::Alloc(size);
        }

        void operator delete(void* ptr)
        {
            if (ptr)
            {
                Allocator::Free(ptr);
            }
        }

        void DecRef() const
        {
            if (!--mRefCount)
            {
                delete this;
            }
        }

        CharT& operator[](int index)
        {
            return mData[index];
        }

        const CharT& operator[](int index) const
        {
            return mData[index];
        }

        CharT* begin()
        {
            return mData.mData;
        }

        CharT* end()
        {
            return mData.mData + mData.mSize - 1;
        }

        Data* Cow()
        {
            if (mRefCount == 1)
            {
                return this;
            }
            Data* data = new Data(*this);
            DecRef();
            return data;
        }

        Vector<CharT, Allocator> mData;
        mutable int mRefCount;
    };

    Data* mData; // offset 0x0

    BasicString()
        : mData(0)
    {
    }

    BasicString(const CharT* string)
    {
        void* storage = Allocator::Alloc(sizeof(Data));
        Data* data = ::new (storage) Data(string);
        mData = data;
    }

    BasicString(const CharT* begin, const CharT* end);

    BasicString(Data* p)
        : mData(p)
    {
    }

    BasicString(const BasicString& other)
        : mData(other.mData != 0 ? other.mData->AddRef() : 0)
    {
    }

    ~BasicString()
    {
        if (mData)
        {
            Data* data = mData;
            data->DecRef();
        }
    }

    BasicString& operator=(BasicString other)
    {
        Data* tmp = mData;
        mData = other.mData;
        other.mData = tmp;
        return *this;
    }

    BasicString& AppendInPlace(const CharT* str);

    template <typename OtherAllocator>
    inline BasicString& AppendInPlace(const BasicString<CharT, OtherAllocator>& rhs);

    const CharT* c_str() const
    {
        static CharT emptyString = '\0';
        return mData ? mData->mData.mData : &emptyString;
    }

    int size() const
    {
        return mData ? mData->mData.mSize - 1 : 0;
    }

    CharT* begin()
    {
        Cow();
        if (mData)
        {
            return GetData().begin();
        }
        return (CharT*)0;
    }

    CharT* end()
    {
        Cow();
        if (mData)
        {
            return GetData().end();
        }
        return (CharT*)0;
    }

    const CharT* begin() const
    {
        return mData ? mData->mData.mData : (const CharT*)0;
    }

    const CharT* end() const
    {
        return mData ? mData->mData.mData + mData->mData.mSize - 1 : (const CharT*)0;
    }

    const CharT& operator[](int index) const
    {
        return mData->mData.mData[index];
    }

    Data& GetData()
    {
        return *mData;
    }

    const Data& GetData() const
    {
        return *mData;
    }

    void Cow()
    {
        if (!mData)
        {
            mData = new Data((const CharT*)0, (const CharT*)0);
        }
        else
        {
            mData = mData->Cow();
        }
    }

    CharT& operator[](int index)
    {
        Cow();
        return GetData()[index];
    }

    template <typename OtherAllocator>
    void insert(CharT* at, const BasicString<CharT, OtherAllocator>& rhs);

    void insert(CharT* at, const CharT* begin, const CharT* end);

    void erase(const CharT* begin, const CharT* end);

    inline void TrimInPlace(const CharT* chars);

    inline BasicString Trim(const CharT* chars) const;

    BasicString Append(const CharT* rhs) const;

    template <typename OtherAllocator>
    inline BasicString Append(const BasicString<CharT, OtherAllocator>& rhs) const;
};

template <typename CharT, typename Allocator>
inline void BasicString<CharT, Allocator>::Data::erase(
    const CharT* begin,
    const CharT* end)
{
    mData.erase(begin, end);
}

template <typename CharT, typename Allocator>
inline BasicString<CharT, Allocator>::BasicString(const CharT* begin, const CharT* end)
    : mData(new Data(begin, end))
{
}

template <typename CharT, typename Allocator>
inline void BasicString<CharT, Allocator>::Data::reserve(int capacity)
{
    if (mData.mCapacity < capacity)
    {
        Vector<CharT, Allocator> newVec(capacity, 0);
        int i = 0;
        for (; i < mData.mSize; i++)
        {
            newVec.mData[i] = mData.mData[i];
        }
        newVec.mSize = mData.mSize;

        int value;
        CharT* data;

        value = mData.mSize;
        mData.mSize = newVec.mSize;
        newVec.mSize = value;

        value = mData.mCapacity;
        mData.mCapacity = newVec.mCapacity;
        newVec.mCapacity = value;

        data = mData.mData;
        mData.mData = newVec.mData;
        newVec.mData = data;
    }
}

template <typename CharT, typename Allocator>
inline void BasicString<CharT, Allocator>::Data::insertRange(CharT* at, const CharT* begin, const CharT* end)
{
    int size = end - begin;
    int offset = at - mData.mData;
    reserve(mData.mSize + size);

    at = mData.mData + offset;
    CharT* t = mData.mData + mData.mSize - 1;
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
    mData.mSize += size;
}

template <typename CharT, typename Allocator>
inline void BasicString<CharT, Allocator>::erase(const CharT* begin, const CharT* end)
{
    Cow();
    GetData().erase(begin, end);
}

template <typename CharT, typename Allocator>
inline void BasicString<CharT, Allocator>::insert(CharT* at, const CharT* begin, const CharT* end)
{
    CharT* oldData = this->begin();
    int offset = at - oldData;
    Cow();
    CharT* dataPtr = this->begin();
    mData->insertRange(dataPtr + offset, begin, end);
}

template <typename CharT, typename Allocator>
template <typename OtherAllocator>
inline void BasicString<CharT, Allocator>::insert(CharT* at, const BasicString<CharT, OtherAllocator>& rhs)
{
    typename BasicString<CharT, OtherAllocator>::Data* data = rhs.mData;
    const CharT* begin;
    if (data)
    {
        begin = data->mData.mData;
    }
    else
    {
        begin = 0;
    }
    insert(at, begin, data ? data->mData.mData + data->mData.mSize - 1 : (CharT*)0);
}

template <typename CharT, typename Allocator>
template <typename OtherAllocator>
BasicString<CharT, Allocator>& BasicString<CharT, Allocator>::AppendInPlace(const BasicString<CharT, OtherAllocator>& rhs)
{
    (*this)[0];

    CharT* at;
    Data* currentData = mData;
    if (currentData != 0)
    {
        at = currentData->mData.mData + currentData->mData.mSize - 1;
    }
    else
    {
        at = 0;
    }

    typename BasicString<CharT, OtherAllocator>::Data* rhsData = rhs.mData;
    const CharT* begin;
    if (rhsData != 0)
    {
        begin = rhsData->mData.mData;
    }
    else
    {
        begin = 0;
    }

    insert(at, begin, rhsData != 0 ? rhsData->mData.mData + rhsData->mData.mSize - 1 : 0);
    return *this;
}

template <typename CharT, typename Allocator>
inline BasicString<CharT, Allocator>& BasicString<CharT, Allocator>::AppendInPlace(const CharT* str)
{
    const CharT* rhsEnd = str;
    while (*rhsEnd != 0)
    {
        rhsEnd++;
    }

    (*this)[0];

    CharT* at;
    if (mData != 0)
    {
        at = mData->mData.mData + mData->mData.mSize - 1;
    }
    else
    {
        at = 0;
    }

    insert(at, str, rhsEnd);
    return *this;
}

template <typename CharT, typename Allocator>
template <typename OtherAllocator>
BasicString<CharT, Allocator> BasicString<CharT, Allocator>::Append(const BasicString<CharT, OtherAllocator>& rhs) const
{
    BasicString r(*this);
    r.AppendInPlace(rhs);
    Data* data = r.mData;
    if (data != 0)
    {
        data->mRefCount++;
    }
    else
    {
        data = 0;
    }
    return BasicString(data);
}

template <typename CharT, typename Allocator>
BasicString<CharT, Allocator> BasicString<CharT, Allocator>::Append(const CharT* rhs) const
{
    BasicString r(*this);
    r.AppendInPlace(rhs);
    Data* data = r.mData;
    if (data != 0)
    {
        data->mRefCount++;
    }
    else
    {
        data = 0;
    }
    return BasicString(data);
}

template <typename CharT, typename Allocator>
BasicString<CharT, Allocator> BasicString<CharT, Allocator>::Trim(const CharT* chars) const
{
    BasicString r(*this);
    r.TrimInPlace(chars);
    Data* data = r.mData;
    if (data != 0)
    {
        data->mRefCount++;
    }
    else
    {
        data = 0;
    }
    return BasicString(data);
}

template <typename CharT, typename Allocator>
void BasicString<CharT, Allocator>::TrimInPlace(const CharT* chars)
{
    int i = 0;
    while (i < size())
    {
        const CharT* c;
        for (c = chars; *c != 0; ++c)
        {
            if (*c == (*this)[i])
                break;
        }
        if (*c == 0)
            break;
        ++i;
    }
    erase(begin(), begin() + i);

    i = size() - 1;
    while (i >= 0)
    {
        const CharT* c;
        for (c = chars; *c != 0; ++c)
        {
            if (*c == (*this)[i])
                break;
        }
        if (*c == 0)
            break;
        --i;
    }
    erase(begin() + i + 1, end());
}

template <typename CharT, typename Allocator>
bool operator==(const BasicString<CharT, Allocator>& lhs, const char* rhs)
{
    unsigned int c;
    typename BasicString<CharT, Allocator>::Data* data = lhs.mData;
    int i = 0;

    while (i < (data != 0 ? data->mData.mSize - 1 : 0))
    {
        c = (u8)*rhs;
        if ((CharT)c == 0)
        {
            return false;
        }
        if ((CharT)c != (CharT)data->mData.mData[i])
        {
            return false;
        }
        rhs++;
        i++;
    }

    return *rhs == '\0';
}


// PORT: defined once in ReplayCamera.cpp. Declaring it here suppresses implicit instantiations of the generic member template in other TUs.
template <>
template <>
BasicString<char, Detail::TempStringAllocator>&
BasicString<char, Detail::TempStringAllocator>::AppendInPlace<Detail::TempStringAllocator>(
    const BasicString<char, Detail::TempStringAllocator>& rhs);

#endif
