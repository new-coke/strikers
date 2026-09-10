#ifndef _VARIANT_H_
#define _VARIANT_H_

// void 0x8028D2AC..0x8028D2B0 | size : 0x4;
#include "NL/nlBasicString.h"
#include "NL/nlMath.h"
#include "Game/Player.h"
#include "Game/Team.h"

enum eVariantType
{
    FT_UNSPECIFIED = -1,
    FT_BOOL = 0,
    FT_CHAR = 1,
    FT_SHORT = 2,
    FT_INT = 3,
    FT_U32 = 4,
    FT_FLOAT = 5,
    FT_PLAYER = 6,
    FT_TEAM = 7,
    FT_VECTOR = 8,
    NUM_V_TYPES = 9,
};

inline eVariantType VariantTypeOf(bool)
{
    return FT_BOOL;
}
inline eVariantType VariantTypeOf(char)
{
    return FT_CHAR;
}
inline eVariantType VariantTypeOf(signed short)
{
    return FT_SHORT;
}
inline eVariantType VariantTypeOf(int)
{
    return FT_INT;
}
inline eVariantType VariantTypeOf(unsigned long)
{
    return FT_U32;
}
inline eVariantType VariantTypeOf(float)
{
    return FT_FLOAT;
}
inline eVariantType VariantTypeOf(cPlayer*)
{
    return FT_PLAYER;
}
inline eVariantType VariantTypeOf(cTeam*)
{
    return FT_TEAM;
}

class Variant
{
public:
    Variant() { Reset(); };

    Variant(eVariantType type, const nlVector3& value)
    {
        Reset();
        mData.vector = value;
        mType = type;
    }

    template <typename T>
    Variant(eVariantType type, T value)
    {
        Reset();
        *(T*)&mData = value;
        mType = type;
    }

    virtual void Reset()
    {
        mType = FT_UNSPECIFIED;
        mData.vector.x = 0.0f;
        mData.vector.y = 0.0f;
        mData.vector.z = 0.0f;
    };

    // PORT: uintptr_t, because the FT_PLAYER and FT_TEAM cases return a pointer.
    virtual uintptr_t GetHash() const;
    virtual BasicString<char, Detail::TempStringAllocator> ToString() const;

    cPlayer* GetPlayer() const
    {
        return mData.pPlayer;
    }

    inline bool operator==(const Variant& other) const
    {
        bool bEqual = (mType == other.mType);
        if (bEqual)
        {
            switch (mType)
            {
            case FT_UNSPECIFIED:
                break;
            case FT_BOOL:
                bEqual = (mData.b == other.mData.b);
                break;
            case FT_CHAR:
                bEqual = (mData.c == other.mData.c);
                break;
            case FT_SHORT:
                bEqual = (mData.s == other.mData.s);
                break;
            case FT_INT:
                bEqual = (mData.i == other.mData.i);
                break;
            case FT_U32:
                bEqual = (mData.u == other.mData.u);
                break;
            case FT_FLOAT:
                bEqual = (mData.f == other.mData.f);
                break;
            case FT_PLAYER:
                bEqual = (mData.pPlayer == other.mData.pPlayer);
                break;
            case FT_TEAM:
                bEqual = (mData.pTeam == other.mData.pTeam);
                break;
            case FT_VECTOR:
                bEqual = (other.mData.vector.x == mData.vector.x && other.mData.vector.y == mData.vector.y && other.mData.vector.z == mData.vector.z);
                break;
            }
        }
        return bEqual;
    }

    /* 0x4 */ enum eVariantType mType;
    /* 0x8 */ union
    {
        bool b;
        char c;
        signed short s;
        float f;
        int i;
        // PORT: pointer-width, not `unsigned long`.
        uintptr_t u;
        cPlayer* pPlayer;
        cTeam* pTeam;
        nlVector3 vector;
    } mData;
}; // total size: 0x14

// class Format < BasicString < char, Detail
// {
// public:
//     void TempStringAllocator >, float > (const BasicString<char, Detail::TempStringAllocator>&, const float&);
//     void TempStringAllocator >, unsigned long > (const BasicString<char, Detail::TempStringAllocator>&, const unsigned long&);
//     void TempStringAllocator >, char > (const BasicString<char, Detail::TempStringAllocator>&, const char&);
//     void TempStringAllocator >, float, float, float > (const BasicString<char, Detail::TempStringAllocator>&, const float&, const float&, const float&);
// };

// class FormatImpl < BasicString < char, Detail
// {
// public:
//     void TempStringAllocator >> ::operator% <unsigned long>(const unsigned long&);
//     void TempStringAllocator >> ::operator% <char>(const char&);
//     void TempStringAllocator >> ::operator% <float>(const float&);
// };

// class LexicalCast < BasicString < char, Detail
// {
// public:
//     void TempStringAllocator >, unsigned long > (const unsigned long&);
//     void TempStringAllocator >, char > (const char&);
// };

// class Detail
// {
// public:
//     void LexicalCastImpl<BasicString<char, Detail::TempStringAllocator>, unsigned long>::Do(unsigned long);
//     void LexicalCastImpl<BasicString<char, Detail::TempStringAllocator>, char>::Do(char);
// };

#endif // _VARIANT_H_
