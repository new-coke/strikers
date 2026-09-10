#ifndef _ROOTSCRIPT_H_
#define _ROOTSCRIPT_H_

#include "Game/AI/DecisionEntity.h"
#include "Game/AI/Fuzzy.h"
#include "Game/AI/FuzzyVariant.h"

FuzzyVariant StrategyChoosePlay(cDecisionEntity* pDecision);
int GetNumDecisionEntities();

#endif // _ROOTSCRIPT_H_
