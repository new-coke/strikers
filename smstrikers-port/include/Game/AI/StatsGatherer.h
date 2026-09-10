#ifndef _STATSGATHERER_H_
#define _STATSGATHERER_H_

#include "Game/Formation.h"
#include "Game/GameTweaks.h"
#include "Game/InterpreterCore.h"
#include "Game/Team.h"
#include "NL/nlTask.h"

class StatsGatherer : private InterpreterCore, public nlTask
{
public:
    StatsGatherer();
    virtual ~StatsGatherer();

    void ReloadStadium();
    void SwapHomeAway(bool);
    void SetTeam(eTeamSide, const char*);
    void SetSidekick(eTeamSide, const char*);
    void SetTestName(const char* name);
    void SetGameLength(float);
    void ResetClock();
    void ResetStats();
    void ResetFormations(eTeamSide);
    void ResetSkill(eTeamSide);
    void SetBallFormationSet(eTeamSide, eFormationSet);
    void SetOffensiveFormation(eTeamSide, eFormation);
    void SetDefensiveFormation(eTeamSide, eFormation);
    void SetSkillHomeAway(eDifficultyID, eDifficultyID);
    void SetSkill(eTeamSide, eDifficultyID);
    void WriteStats(const char*);
    void RestartMatch();
    void RunTillGameEnd();
    void RunTillGameTime(float);
    virtual void Run(float);
    void Initialize();
    void Stop();
    void Start();
    virtual const char* GetName();
    virtual void DoFunctionCall(unsigned int);

    /* 0x3C */ BasicString<char, Detail::TempStringAllocator> m_testName;
    /* 0x40 */ void* m_pByteCode;
    /* 0x44 */ bool m_bRunning;
    /* 0x45 */ bool m_bReloadingStadium;
    /* 0x48 */ eTeamSide m_eTeamSide[2];
}; // total size: 0x50

#endif // _STATSGATHERER_H_
