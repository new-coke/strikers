#ifndef _SIMMER_H_
#define _SIMMER_H_

#include "NL/nlTokenizer.h"

struct StatsPair
{
    /* 0x000 */ float mMean;
    /* 0x004 */ float mStandardDeviation;
}; // total size: 0x8

class Simulator
{
public:
    Simulator();
    void InitializeStats();

    /* 0x000 */ StatsPair mStatistics[23];
};

#endif // _SIMMER_H_
