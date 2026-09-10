#include "Game/AI/Scripts/ScriptDefines.h"

#include "Game/AI/Scripts/ScriptCaching.h"

#include "Game/AI/Fielder.h"
#include "Game/Team.h"
#include "Game/Ball.h"
#include "types.h"

cFielder* g_pScriptCurrentFielder;
cFielder* g_pScriptCurrentMark;
cFielder* g_pScriptBallOwner;
cTeam* g_pScriptCurrentTeam;
cTeam* g_pScriptOtherTeam;
cBall* g_pScriptBall;

extern cTeam* g_pTeams[];

/**
 * Offset/Address/Size: 0xB8 | 0x8007EA18 | size: 0x70
 */
void FuzzyScriptSetCurrentFielder(cFielder* pCurrentFielder)
{
    if (pCurrentFielder == NULL)
    {
        FuzzyScriptClearGlobals();
        return;
    }

    g_pScriptCurrentFielder = pCurrentFielder;
    g_pScriptBall = g_pBall;
    g_pScriptCurrentMark = pCurrentFielder->m_pMark;
    g_pScriptBallOwner = g_pBall->GetOwnerFielder();
    g_pScriptCurrentTeam = pCurrentFielder->m_pTeam;
    g_pScriptOtherTeam = (g_pTeams[0] == g_pScriptCurrentTeam) ? g_pTeams[1] : g_pTeams[0];
}

/**
 * Offset/Address/Size: 0x48 | 0x8007E9A8 | size: 0x70
 */
void FuzzyScriptSetCurrentTeam(cTeam* pCurrentTeam)
{
    if (pCurrentTeam == NULL)
    {
        FuzzyScriptClearGlobals();
        return;
    }

    g_pScriptCurrentFielder = NULL;
    g_pScriptBall = g_pBall;
    g_pScriptCurrentMark = NULL;
    g_pScriptBallOwner = (cFielder*)g_pBall->GetOwnerFielder();
    g_pScriptCurrentTeam = pCurrentTeam;
    g_pScriptOtherTeam = (g_pTeams[0] == pCurrentTeam) ? g_pTeams[1] : g_pTeams[0];
}

/**
 * Offset/Address/Size: 0x0 | 0x8007E960 | size: 0x48
 */
void FuzzyScriptClearGlobals()
{
    g_pScriptCurrentTeam = NULL;
    g_pScriptCurrentFielder = NULL;
    g_pScriptCurrentMark = 0;
    g_pScriptBallOwner = 0;
    g_pScriptCurrentTeam = NULL;
    g_pScriptOtherTeam = 0;
    g_pScriptBall = NULL;

    if (ScriptQuestionCache::Instance() != NULL)
    {
        ScriptQuestionCache::Instance()->Clear();
    }
}
