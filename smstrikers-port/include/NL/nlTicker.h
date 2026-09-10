#ifndef _NLTICKER_H_
#define _NLTICKER_H_

#include "types.h"

f32 nlGetTickerDifference(uint startTick, uint endTick);
uint nlSubtractTicks(uint startTick, uint endTick);
u32 nlGetTicker();
void nlInitTicker();

#endif // _NLTICKER_H_
