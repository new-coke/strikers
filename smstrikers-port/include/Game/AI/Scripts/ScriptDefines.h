#ifndef _SCRIPTDEFINES_H_
#define _SCRIPTDEFINES_H_

class cFielder;
class cTeam;

void FuzzyScriptClearGlobals();
void FuzzyScriptSetCurrentTeam(cTeam* pCurrentTeam);
void FuzzyScriptSetCurrentFielder(cFielder* pCurrentFielder);

#endif // _SCRIPTDEFINES_H_
