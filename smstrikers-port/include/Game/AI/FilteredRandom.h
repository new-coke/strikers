#ifndef _FILTEREDRANDOM_H_
#define _FILTEREDRANDOM_H_

#include "types.h"

class FilteredRandomRange
{
public:
    FilteredRandomRange();
    ~FilteredRandomRange() { }
    int genrand(int range);

    /* 0x00 */ u32 m_repeatingRunLength; // size 0x4
    /* 0x04 */ s32 m_hist[10];           // size 0x28
}; // total size: 0x2C

class FilteredRandomChance
{
public:
    FilteredRandomChance();
    ~FilteredRandomChance() { }
    bool genrand(float chance);

    /* 0x00 */ u8 m_hist[20]; // size 0x14
}; // total size: 0x14

class FilteredRandomReal
{
public:
    FilteredRandomReal();
    ~FilteredRandomReal() { }
    float genrand();

    /* 0x00 */ bool m_change;                             // size 0x1
    /* 0x04 */ float m_hist[5];                           // size 0x14
    /* 0x18 */ FilteredRandomRange m_filteredRandomRange; // size 0x2C
}; // total size: 0x44

#endif // _FILTEREDRANDOM_H_
