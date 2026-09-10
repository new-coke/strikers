#ifndef _FUZZYVARIANT_H_
#define _FUZZYVARIANT_H_

#include "Game/AI/Variant.h"

class FuzzyVariant : public Variant
{
public:
    FuzzyVariant() { Reset(); };
    FuzzyVariant(const FuzzyVariant& other)
    {
        Reset();
        *this = other;
    }

    FuzzyVariant(bool& value)
        : Variant(VariantTypeOf(value), value)
    {
        ExtraData.Reset();
        Confidence = 0.0f;
        SelectionChance = 1.0f;
    }

    template <typename T>
    FuzzyVariant(const T& value)
        : Variant(VariantTypeOf(value), value)
    {
        ExtraData.Reset();
        Confidence = 0.0f;
        SelectionChance = 1.0f;
    }

    virtual void Reset()
    {
        mType = FT_UNSPECIFIED;
        mData.vector.x = 0.0f;
        mData.vector.y = 0.0f;
        mData.vector.z = 0.0f;
        ExtraData.Reset();
        Confidence = 0.0f;
        SelectionChance = 1.0f;
    };

    float GetFloat() const { return mData.f; }

    float Confidence;      // offset 0x14, size 0x4
    float SelectionChance; // offset 0x18, size 0x4
    Variant ExtraData;     // offset 0x1C, size 0x14
}; // total size: 0x30

extern FuzzyVariant fvNotSet;

#endif // _FUZZYVARIANT_H_
