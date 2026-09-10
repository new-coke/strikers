#include "types.h"

#ifndef _INT32
typedef int _INT32;
typedef unsigned int _UINT32;
#endif

_UINT32 __float_nan[] = { 0x7FFFFFFF };
_UINT32 __float_huge[] = { 0x7F800000 };
_UINT32 __double_min[] = { 0x00100000, 0 };
_UINT32 __double_max[] = { 0x7FEFFFFF, 0xFFFFFFFF };
_UINT32 __double_epsilon[] = { 0x3CB00000, 0 };
_UINT32 __double_tiny[] = { 0, 0x00000001 };
_UINT32 __double_huge[] = { 0x7FF00000, 0 };
_UINT32 __double_nan[] = { 0x7FFFFFFF, 0xFFFFFFFF };
_UINT32 __extended_min[] = { 0x00100000, 0 };
_UINT32 __extended_max[] = { 0x7FEFFFFF, 0xFFFFFFFF };
_UINT32 __extended_epsilon[] = { 0x3CB00000, 0 };
_UINT32 __extended_tiny[] = { 0, 0x00000001 };
_UINT32 __extended_huge[] = { 0x7FF00000, 0 };
_UINT32 __extended_nan[] = { 0x7FFFFFFF, 0xFFFFFFFF };
_UINT32 __float_min[] = { 0x00800000 };
_UINT32 __float_max[] = { 0x7F7FFFFF };
_UINT32 __float_epsilon[] = { 0x34000000 };
