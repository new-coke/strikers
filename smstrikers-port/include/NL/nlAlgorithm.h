#ifndef _NLALGORITHM_H_
#define _NLALGORITHM_H_

#include "stdlib.h"

static unsigned long nlCountBits(unsigned long bits)
{
    unsigned long count = 0;
    while (bits != 0)
    {
        bits &= bits - 1;
        count++;
    }
    return count;
}

// Home header for the generic search/sort algorithm templates. The original
// compiler grouped weak instantiations into linkonce sections keyed by the
// BODY's file; DWARF attributes nlBSearch/nlQSort/nlDefaultQSortComparer to
// core/nlAlgorithm.h, so their bodies must live together here for a TU's
// .text section grouping to match the target link layout.

/**
 * Offset/Address/Size: 0x0 | 0x80213820 | size: 0x8C
 */
template <typename T, typename Key>
T* nlBSearch(const Key& key, T* array, int size)
{
    int high = size - 1;
    int low = -1;
    while (high - low > 1)
    {
        int probe = (high + low) / 2;
        if ((unsigned long)array[probe] > (unsigned long)key)
            high = probe;
        else
            low = probe;
    }
    unsigned long highValue = (unsigned long)array[high];
    if (highValue == (unsigned long)key)
        return &array[high];
    if (low == -1)
        return NULL;
    unsigned long lowValue = (unsigned long)array[low];
    if (lowValue == (unsigned long)key)
        return &array[low];
    return NULL;
}

template <typename T>
int nlDefaultQSortComparer(const T* pa, const T* pb)
{
    if ((unsigned long)*pa > (unsigned long)*pb)
        return 1;
    if ((unsigned long)*pa == (unsigned long)*pb)
        return 0;
    return -1;
}

template <typename T>
void nlQSort(T* array, int count, int (*comparefunc)(const T*, const T*))
{
    qsort(array, count, sizeof(T), (int (*)(const void*, const void*))comparefunc);
}

#endif // _NLALGORITHM_H_
