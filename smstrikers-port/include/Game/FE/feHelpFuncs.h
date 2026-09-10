#ifndef _FEHELPFUNCS_H_
#define _FEHELPFUNCS_H_

#include "Game/GameInfo.h"
#include "Game/Team.h"
#include "Game/TrophyInfo.h"
#include "Game/DB/Cup.h"
#include "Game/DB/UserOptions.h"

#include "Game/FE/tlComponentInstance.h"

#include "Game/FE/tlTextInstance.h"
#include "Game/Render/Nis.h"

struct NameIDEntry
{
    const char* mName;
    int mID;
};

extern NameIDEntry NameTeamTable[9];
extern NameIDEntry NameSidekickTable[4];

void MakeTextBoxReallyWide(TLTextInstance& textInstance);
const char* GetCupStreamName(eTrophyType trophyType);
const char* GetMemCardDescription();
const char* GetMemCardTitle();
void EnableAutoPressed();
TLInstance* FindComponent(TLSlide* slide, const char* name);
unsigned long GetLOCRank(int rank);
eSidekickID ConvertToSidekickID(const char* name);
eTeamID ConvertToTeamID(const char* name);
const char* GetSidekickName(eSidekickID sidekickID);
const char* GetTeamName(eTeamID teamID);
eCharacterClass ConvertToCharacterClass(eSidekickID sidekickID);
eCharacterClass ConvertToCharacterClass(eTeamID teamID);
unsigned long GetLOCTrophyName(eTrophyType trophyType);
unsigned long GetLOCStandingsName(GameInfoManager::eGameModes mode);
unsigned long GetLOCModeName(GameInfoManager::eGameModes mode);
unsigned long GetLOCTeamName(eTeamID teamID);
unsigned long GetLOCSidekickName(eSidekickID sidekickid);
unsigned long GetLOCCharacterName(eTeamID teamid, bool useShortSuperTeam, bool useLockedSuperTeam);
unsigned long GetLOCDifficultyName(GameplaySettings::eSkillLevel difficulty);
unsigned long GetStadiumStringID(eStadiumID stadiumID);

namespace TakeGameMemSnapshot
{
extern unsigned char gTakenSnapshot;
extern float gTimeElapsed;
void WriteToDisk();
void ResetTimers();
void Update(float dt);
} // namespace TakeGameMemSnapshot

class FECharacterSound
{
public:
    static void PlayCaptainSlideIn(eTeamID teamID);
    static void PlaySidekickName(eSidekickID sidekickID);
    static const char* PlayCaptainName(eTeamID teamID);
};

class CaptainSidekickFilename
{
public:
    enum Type
    {
        TYPE_INVALID = -1,
        TYPE_CAPTAIN = 0,
        TYPE_CAPTAIN_OUTLINE = 1,
        TYPE_CAPTAIN_FLASH = 2,
        TYPE_SIDEKICK = 3,
        TYPE_SIDEKICK_OUTLINE = 4,
    };
    static void Build(Type type, char* buf, int size, int id, int flag);
};

extern const unsigned char PAD_COLOURS[4][3];

namespace DoubleHighlite
{
static const char* SLIDE_IN = "in";
static const char* SLIDE_OUT = "out";

void CloseItem(TLComponentInstance* component);
void OpenItem(TLComponentInstance* component);
void TempDisableSound();
} // namespace DoubleHighlite

namespace SingleHighlite
{
static const char* SLIDE_IN = "in";
static const char* SLIDE_OUT = "out";

void CloseItem(TLComponentInstance* component);
void OpenItem(TLComponentInstance* component);
void TempDisableSound();

extern bool TEMPDISABLESOUND;
} // namespace SingleHighlite

#endif // _FEHELPFUNCS_H_
