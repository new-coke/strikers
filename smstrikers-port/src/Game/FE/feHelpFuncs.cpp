#include "port/region.h"  // PORT: one binary, three discs
#include "types.h"
#include "strtold.h"
#include "NL/nlBasicString.h"
#include "NL/nlPrint.h"

namespace TakeGameMemSnapshot
{
namespace Detail
{
using ::Detail::TempStringAllocator;
}

#include "NL/nlFormat.h"
} // namespace TakeGameMemSnapshot

using TakeGameMemSnapshot::LexicalCast;

#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/feText.h"

#include "Game/FE/FEAudio.h"
#include "Game/FE/feAsyncImage.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feInput.h"
#include "Game/FE/tlInstance.h"
#include "Game/FE/tlSlide.h"
#include "Game/FE/tlTextInstance.h"
#include "Game/GameInfo.h"
#include "Game/DB/UserOptions.h"
#include "NL/nlColour.h"
#include "NL/nlMath.h"
#include "NL/nlPrint.h"
#include "NL/nlString.h"
#include "NL/nlBasicString.h"
#include "NL/MemAlloc.h"
#include <stdio.h>
#include "Game/main.h"

namespace DoubleHighlite
{
static bool TEMPDISABLESOUND;
} // namespace DoubleHighlite

namespace SingleHighlite
{
bool TEMPDISABLESOUND;
} // namespace SingleHighlite

static unsigned long CCToStringName[13] = {
    0x35D90F6F,
    0x35F8B919,
    0x11761DAE,
    0x4298E9EF,
    0x367F2AF9,
    0x36947319,
    0x369BBA97,
    0x36D3EC80,
    0x477D2527,
    0x4DD51191,
    0x3750AFA1,
    0x377C920B,
    0x848890BC,
};

static unsigned long ModeToStringName[10] = {
    0xF17CCCFC,
    0x3F93A700,
    0x68141335,
    0x7902D2A0,
    0xE384D5B8,
    0x88EAF9AE,
    0xFC7AE263,
    0x3D6FFE4E,
    0x77EBA4E6,
    0x1A97A58C,
};

static unsigned long ModeToStandingsStringName[10] = {
    0xF17CCCFC,
    0x9B697E0A,
    0x61949FBF,
    0xF302B4AA,
    0xDD056242,
    0xDB4C7038,
    0x6E192E6D,
    0xE617BFD8,
    0xE989F0F0,
    0x14183216,
};

static unsigned long TrophyTypeToStringName[13] = {
    0x3F93A700,
    0x68141335,
    0x7902D2A0,
    0xE384D5B8,
    0x88EAF9AE,
    0xFC7AE263,
    0x3D6FFE4E,
    0x77EBA4E6,
    0x51C47AC0,
    0x09FB969C,
    0xB520A8AF,
    0x185EC17B,
    0x99FCEA51,
};

static const long TeamID2CharacterClassTable[9][2] = {
    { 0x00000000, 0x00000001 },
    { 0x00000001, 0x00000002 },
    { 0x00000002, 0x00000005 },
    { 0x00000003, 0x00000006 },
    { 0x00000004, 0x00000007 },
    { 0x00000005, 0x00000009 },
    { 0x00000006, 0x0000000A },
    { 0x00000007, 0x0000000B },
    { 0x00000008, 0x0000000C },
};

NameIDEntry NameTeamTable[9] = {
    { "daisy", TEAM_DAISY },
    { "donkeykong", TEAM_DONKEYKONG },
    { "luigi", TEAM_LUIGI },
    { "mario", TEAM_MARIO },
    { "peach", TEAM_PEACH },
    { "waluigi", TEAM_WALUIGI },
    { "wario", TEAM_WARIO },
    { "yoshi", TEAM_YOSHI },
    { "mystery", TEAM_MYSTERY },
};

NameIDEntry NameSidekickTable[4] = {
    { "toad", SK_TOAD },
    { "koopa", SK_KOOPA },
    { "hammerbro", SK_HAMMERBROS },
    { "birdo", SK_BIRDO },
};

const unsigned char PAD_COLOURS[4][3] = {
    { 0xFA, 0xE3, 0x35 },
    { 0x40, 0xC4, 0xE4 },
    { 0x08, 0xDD, 0x3F },
    { 0xB8, 0x49, 0x4C },
};

u32 CONTROLLER_TEXT[4] = { 0xA60, 0xA61, 0xA62, 0xA63 };   // PORT: u32, as its reader declares it

static unsigned long RankToRankString[14] = {
    0x006AF952, 0x2BCC86A6, 0x01679D85, 0x0E3857E2, 0x006AC65B, 0x0156517A, 0xA6AFB3A7, 0x0B6EC463, 0x00FBAC6B, 0x01660DED, 0x856B1465, 0x1C76DFC8, 0x969F5329, 0xA08874EE
};

nlColour MenuHighliteColour = { 0xFF, 0xFF, 0xFF, 0xB2 };
nlColour SubMenuHighliteColour = { 0xFF, 0xA8, 0x00, 0xFF };
nlColour SubMenuUnhighliteColour = { 0xFF, 0xFF, 0xFF, 0xFF };

/**
 * Offset/Address/Size: 0x1358 | 0x800A4414 | size: 0x80
 */
unsigned long GetStadiumStringID(eStadiumID stadiumID)
{
    switch (stadiumID)
    {
    case STAD_MARIO_STADIUM:
        return 0x8FFCBC02;
    case STAD_PEACH_TOAD_STADIUM:
        return 0x9034EDEB;
    case STAD_DK_DAISY:
        return 0xBDD19EF9;
    case STAD_WARIO_STADIUM:
        return 0x90B1B10C;
    case STAD_YOSHI_STADIUM:
        return 0x90DD9376;
    case STAD_SUPER_STADIUM:
        return 0x90743D99;
    case STAD_FORBIDDEN_DOME:
        return 0xC7B104A1;
    default:
        return 0x094D126F;
    }
}

static unsigned long GetLOCCharacterName(eCharacterClass cc)
{
    return CCToStringName[cc];
}

/**
 * Offset/Address/Size: 0x127C | 0x800A4338 | size: 0xDC
 */
unsigned long GetLOCCharacterName(eTeamID teamid, bool useShortSuperTeam, bool useLockedSuperTeam)
{
    if (useShortSuperTeam && teamid == 8)
    {
        return 0xFA6F322B;
    }
    if (useLockedSuperTeam && teamid == 8 && !GameInfoManager::Instance()->IsSuperTeamUnlocked())
    {
        return 0x387952CD;
    }

    return GetLOCTeamName(teamid);
}

/**
 * Offset/Address/Size: 0x11F4 | 0x800A42B0 | size: 0x88
 */
unsigned long GetLOCSidekickName(eSidekickID sidekickid)
{
    s32 cc;

    switch (sidekickid)
    {
    case 3:
        cc = 0;
        break;
    case 2:
        cc = 3;
        break;
    case 1:
        cc = 4;
        break;
    case 0:
        cc = 8;
        break;
    case -2:
        cc = 0xC;
        break;
    default:
        cc = -1;
        break;
    }
    if (cc != -1)
    {
        return CCToStringName[cc];
    }
    return 0x094D126F;
}

static inline eCharacterClass TeamIDToCharacterClass(const eTeamID& teamID)
{
    s32 i;

    // PORT: was walked through a u32* stepping two words per row, which is 8 bytes, a row on console, half a row here.
    for (i = 0; i < 9; i++)
    {
        if ((s32)teamID == (s32)TeamID2CharacterClassTable[i][0])
        {
            return (eCharacterClass)TeamID2CharacterClassTable[i][1];
        }
    }
    return CHARACTER_CLASS_INVALID;
}

/**
 * Offset/Address/Size: 0x1184 | 0x800A4240 | size: 0x70
 */
unsigned long GetLOCTeamName(eTeamID teamID)
{
    eCharacterClass cc = TeamIDToCharacterClass(teamID);
    if (cc != CHARACTER_CLASS_INVALID)
    {
        return CCToStringName[cc];
    }
    return 0x094D126F;
}

/**
 * Offset/Address/Size: 0x1170 | 0x800A422C | size: 0x14
 */
unsigned long GetLOCModeName(GameInfoManager::eGameModes mode)
{
    return ModeToStringName[mode];
}

/**
 * Offset/Address/Size: 0x115C | 0x800A4218 | size: 0x14
 */
unsigned long GetLOCStandingsName(GameInfoManager::eGameModes mode)
{
    return ModeToStandingsStringName[mode];
}

/**
 * Offset/Address/Size: 0x1148 | 0x800A4204 | size: 0x14
 */
unsigned long GetLOCTrophyName(eTrophyType trophyType)
{
    return TrophyTypeToStringName[trophyType];
}

/**
 * Offset/Address/Size: 0x10E4 | 0x800A41A0 | size: 0x64
 */
unsigned long GetLOCDifficultyName(GameplaySettings::eSkillLevel difficulty)
{
    switch (difficulty)
    {
    case GameplaySettings::ROOKIE:
        return 0xBBB858E3;
    case GameplaySettings::PROFESSIONAL:
        return 0xCAED019A;
    case GameplaySettings::SUPERSTAR:
        return 0xA7596E77;
    case GameplaySettings::LEGEND:
        return 0x00015F30;
    default:
        return 0xBBB858E3;
    }
}

/**
 * Offset/Address/Size: 0x1098 | 0x800A4154 | size: 0x4C
 */
eCharacterClass ConvertToCharacterClass(eTeamID teamID)
{
    for (int i = 0; i < 9; i++)
    {
        if (teamID == TeamID2CharacterClassTable[i][0])
        {
            return (eCharacterClass)TeamID2CharacterClassTable[i][1];
        }
    }
    return CHARACTER_CLASS_INVALID;
}

/**
 * Offset/Address/Size: 0x1034 | 0x800A40F0 | size: 0x64
 */
eCharacterClass ConvertToCharacterClass(eSidekickID sidekickID)
{
    switch (sidekickID)
    {
    case SK_BIRDO:
        return BIRDO;
    case SK_HAMMERBROS:
        return HAMMERBROS;
    case SK_KOOPA:
        return KOOPA;
    case SK_TOAD:
        return TOAD;
    case SK_MYSTERY:
        return MYSTERY;
    default:
        return CHARACTER_CLASS_INVALID;
    }
}

static eTeamID ConvertToTeamID(eCharacterClass cc)
{
    for (int i = 0; i < 9; i++)
    {
        if (cc == TeamID2CharacterClassTable[i][1])
        {
            return (eTeamID)TeamID2CharacterClassTable[i][0];
        }
    }
    return TEAM_INVALID;
}

/**
 * Offset/Address/Size: 0x1020 | 0x800A40DC | size: 0x14
 */
const char* GetTeamName(eTeamID teamID)
{
    return NameTeamTable[teamID].mName;
}

/**
 * Offset/Address/Size: 0xFF8 | 0x800A40B4 | size: 0x28
 */
const char* GetSidekickName(eSidekickID sidekickID)
{
    if (sidekickID == SK_MYSTERY)
    {
        return "myst_sidekick";
    }
    return NameSidekickTable[sidekickID].mName;
}

/**
 * Offset/Address/Size: 0xF74 | 0x800A4030 | size: 0x84
 */
eTeamID ConvertToTeamID(const char* name)
{
    for (int i = 0; i < 9; i++)
    {
        if (nlStrICmp(NameTeamTable[i].mName, name) == 0)
        {
            return (eTeamID)NameTeamTable[i].mID;
        }
    }
    return TEAM_INVALID;
}

/**
 * Offset/Address/Size: 0xEF0 | 0x800A3FAC | size: 0x84
 */
eSidekickID ConvertToSidekickID(const char* name)
{
    for (int i = 0; i < 4; i++)
    {
        if (nlStrICmp(NameSidekickTable[i].mName, name) == 0)
        {
            return (eSidekickID)NameSidekickTable[i].mID;
        }
    }
    return SK_INVALID;
}

/**
 * Offset/Address/Size: 0xEDC | 0x800A3F98 | size: 0x14
 */
unsigned long GetLOCRank(int rank)
{
    return RankToRankString[rank];
}

/**
 * Offset/Address/Size: 0xE70 | 0x800A3F2C | size: 0x6C
 */
TLInstance* FindComponent(TLSlide* slide, const char* name)
{
    TLInstance* head = slide->m_instances;
    TLInstance* inst = head;
    unsigned long hash = nlStringLowerHash(name);
    while (inst)
    {
        if (hash == inst->m_hash)
        {
            break;
        }
        inst = inst->m_next;
        if (inst == head)
        {
            inst = NULL;
            break;
        }
    }
    return inst;
}

/**
 * Offset/Address/Size: 0xE64 | 0x800A3F20 | size: 0xC
 */
void DoubleHighlite::TempDisableSound()
{
    TEMPDISABLESOUND = true;
}

/**
 * Offset/Address/Size: 0xCB4 | 0x800A3D70 | size: 0x1B0
 */
void DoubleHighlite::OpenItem(TLComponentInstance* component)
{
    TLInstance* highliteimage;

    component->SetActiveSlide("in");

    highliteimage = FindComponent(component->GetActiveSlide(), "highlite");
    highliteimage->m_bVisible = true;
    ((TLComponentInstance*)highliteimage)->SetActiveSlide("in");

    FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        ((TLComponentInstance*)highliteimage)->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("may_highlite")))
        ->SetAssetColour(MenuHighliteColour);

    FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        ((TLComponentInstance*)highliteimage)->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("may_highlite2")))
        ->SetAssetColour(MenuHighliteColour);

    ((TLComponentInstance*)highliteimage)->Update(0.0f);
    component->Update(0.0f);

    if (TEMPDISABLESOUND == false)
    {
        FEAudio::PlayAnimAudioEvent("sfx_menu_highlight_open", false);
    }

    TEMPDISABLESOUND = false;
}

/**
 * Offset/Address/Size: 0xC00 | 0x800A3CBC | size: 0xB4
 */
void DoubleHighlite::CloseItem(TLComponentInstance* component)
{
    TLComponentInstance* highlite;

    component->SetActiveSlide("out");

    highlite = (TLComponentInstance*)FindComponent(component->GetActiveSlide(), "highlite");
    highlite->m_bVisible = true;
    highlite->SetActiveSlide("out");
    highlite->SetAssetColour(MenuHighliteColour);
    component->Update(0.0f);
}

/**
 * Offset/Address/Size: 0xBF4 | 0x800A3CB0 | size: 0xC
 */
void SingleHighlite::TempDisableSound()
{
    TEMPDISABLESOUND = true;
}

/**
 * Offset/Address/Size: 0xAB0 | 0x800A3B6C | size: 0x144
 */
void SingleHighlite::OpenItem(TLComponentInstance* component)
{
    TLComponentInstance* highlight;
    TLInstance* highlightimage;

    component->SetActiveSlide("in");
    component->Update(0.0f);

    highlight = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        component->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("high")));

    highlight->SetActiveSlide("in");
    highlight->Update(0.0f);

    highlightimage = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        highlight->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("may_highlite")));
    highlightimage->SetAssetColour(MenuHighliteColour);

    if (TEMPDISABLESOUND == false)
    {
        FEAudio::PlayAnimAudioEvent("sfx_menu_highlight_open", false);
    }

    TEMPDISABLESOUND = false;
}

/**
 * Offset/Address/Size: 0x990 | 0x800A3A4C | size: 0x120
 */
void SingleHighlite::CloseItem(TLComponentInstance* component)
{
    TLComponentInstance* highlight;
    TLInstance* highlightimage;

    component->SetActiveSlide("out");
    component->Update(0.0f);

    highlight = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        component->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("high")));

    highlight->SetActiveSlide("out");
    highlight->Update(0.0f);

    highlightimage = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        highlight->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("may_highlite")));
    highlightimage->SetAssetColour(MenuHighliteColour);
}

/**
 * Offset/Address/Size: 0x7DC | 0x800A3898 | size: 0x1B4
 */
void CaptainSidekickFilename::Build(CaptainSidekickFilename::Type type, char* buf, int size, int id, int flag)
{
    char suffix = 'r';
    if (!flag)
    {
        suffix = 'l';
    }

    if (type == TYPE_SIDEKICK || type == TYPE_SIDEKICK_OUTLINE)
    {
        const char* name = (id == -2) ? "myst_sidekick" : NameSidekickTable[id].mName;
        switch (type)
        {
        case TYPE_SIDEKICK:
            nlSNPrintf(buf, size, "fe/loadingscreens/%s_%c", name, suffix);
            break;
        case TYPE_SIDEKICK_OUTLINE:
            nlSNPrintf(buf, size, "fe/loadingscreens/%s_%c_bg", name, suffix);
            break;
        default:
            nlSNPrintf(buf, size, "fe/loadingscreens/%s_%c", name, suffix);
            break;
        }
    }
    else
    {
        const char* name = (char*)NameTeamTable[id].mName;
        suffix = (flag == 0) ? 'l' : 'r';
        switch (type)
        {
        case TYPE_CAPTAIN:
            nlSNPrintf(buf, size, "fe/loadingscreens/%s_%c", name, suffix);
            break;
        case TYPE_CAPTAIN_FLASH:
            nlSNPrintf(buf, size, "fe/loadingscreens/%s_%c_white", name, suffix);
            break;
        case TYPE_CAPTAIN_OUTLINE:
            nlSNPrintf(buf, size, "fe/loadingscreens/%s_%c_bg", name, suffix);
            break;
        default:
            nlSNPrintf(buf, size, "fe/loadingscreens/%s_%c", name, suffix);
            break;
        }
    }
}

/**
 * Offset/Address/Size: 0x748 | 0x800A3804 | size: 0x94
 */
void EnableAutoPressed()
{
    g_pFEInput->Reset();
    g_pFEInput->EnableAnalogToDPadMapping(FE_ALL_PADS, true);
    g_pFEInput->SetAutoRepeatParams(FE_ALL_PADS, 0xe, 0.7f, 0.3f);
    g_pFEInput->SetAutoRepeatParams(FE_ALL_PADS, 0xd, 0.7f, 0.3f);
    g_pFEInput->SetAutoRepeatParams(FE_ALL_PADS, 0xb, 0.7f, 0.3f);
    g_pFEInput->SetAutoRepeatParams(FE_ALL_PADS, 0xc, 0.7f, 0.3f);
}

/**
 * Offset/Address/Size: 0x700 | 0x800A37BC | size: 0x48
 */
const char* FECharacterSound::PlayCaptainName(eTeamID teamID)
{
    static char* CHARACTER_ACCEPT_SOUNDS[9] = {
        "sfx_accept_daisy",
        "sfx_accept_dk",
        "sfx_accept_luigi",
        "sfx_accept_mario",
        "sfx_accept_peach",
        "sfx_accept_waluigi",
        "sfx_accept_wario",
        "sfx_accept_yoshi",
        "sfx_accept_mystery",
    };

    FEAudio::PlayAnimAudioEvent(CHARACTER_ACCEPT_SOUNDS[teamID], true);
    return CHARACTER_ACCEPT_SOUNDS[teamID];
}

/**
 * Offset/Address/Size: 0x6CC | 0x800A3788 | size: 0x34
 */
void FECharacterSound::PlaySidekickName(eSidekickID sidekickID)
{
    static char* SIDEKICK_SOUNDS[4] = {
        "sfx_accept_toad",
        "sfx_accept_koopa",
        "sfx_accept_ham",
        "sfx_accept_birdo",
    };

    FEAudio::PlayAnimAudioEvent(SIDEKICK_SOUNDS[sidekickID], false);
}

/**
 * Offset/Address/Size: 0x698 | 0x800A3754 | size: 0x34
 */
void FECharacterSound::PlayCaptainSlideIn(eTeamID teamID)
{
    static char* CAPTAIN_SLIDE_SOUNDS[9] = {
        "sfx_focus_daisy",
        "sfx_focus_dk",
        "sfx_focus_luigi",
        "sfx_focus_mario",
        "sfx_focus_peach",
        "sfx_focus_waluigi",
        "sfx_focus_wario",
        "sfx_focus_yoshi",
        "sfx_focus_mystery",
    };

    FEAudio::PlayAnimAudioEvent(CAPTAIN_SLIDE_SOUNDS[teamID], false);
}

static unsigned long GetLargestFreeBlock()
{
    return StandardAllocator.LargestFreeBlock();
}

/**
 * Offset/Address/Size: 0x648 | 0x800A3704 | size: 0x50
 */
void TakeGameMemSnapshot::Update(float dt)
{
    if (gTakenSnapshot)
    {
        return;
    }
    gTimeElapsed += dt;
    if (gTimeElapsed >= 5.0f)
    {
        WriteToDisk();
        gTakenSnapshot = 1;
    }
}

namespace TakeGameMemSnapshot
{
unsigned char gTakenSnapshot;
float gTimeElapsed;
} // namespace TakeGameMemSnapshot

/**
 * Offset/Address/Size: 0x634 | 0x800A36F0 | size: 0x14
 */
void TakeGameMemSnapshot::ResetTimers()
{
    gTakenSnapshot = 0;
    gTimeElapsed = 0.0f;
}

/**
 * Offset/Address/Size: 0x130 | 0x800A31EC | size: 0x504
 */
void TakeGameMemSnapshot::WriteToDisk()
{
    static const char* StadiumNames[7] = {
        "pipeline",
        "palace",
        "konga",
        "underground",
        "biopod",
        "super",
        "battledome",
    };

    const char* filename = "gamesnapshot.txt";
    FILE* pFile = fopen(filename, "r");

    if (!pFile)
    {
        pFile = fopen(filename, "wt");
        BasicString<char, ::Detail::TempStringAllocator> header;
        header.AppendInPlace("hcaptain,hsidekick,acaptain,asidekick,stadium,largestfree,freevm,largestfreevm\n");
        fwrite(header.c_str(), 1, header.size(), pFile);
    }
    fclose(pFile);

    pFile = fopen(filename, "at");

    BasicString<char, ::Detail::TempStringAllocator> data;
    data.AppendInPlace(NameTeamTable[GameInfoManager::GetInstance()->GetTeam(0)].mName);
    data.AppendInPlace(",");

    eSidekickID sk0 = GameInfoManager::GetInstance()->GetSidekick(0);
    data.AppendInPlace((sk0 == (eSidekickID)-2) ? "myst_sidekick" : NameSidekickTable[sk0].mName);
    data.AppendInPlace(",");

    data.AppendInPlace(NameTeamTable[GameInfoManager::GetInstance()->GetTeam(1)].mName);
    data.AppendInPlace(",");

    eSidekickID sk1 = GameInfoManager::GetInstance()->GetSidekick(1);
    data.AppendInPlace((sk1 == (eSidekickID)-2) ? "myst_sidekick" : NameSidekickTable[sk1].mName);
    data.AppendInPlace(",");

    data.AppendInPlace(StadiumNames[GameInfoManager::GetInstance()->GetStadium()]);
    data.AppendInPlace(",");

    fwrite(data.c_str(), 1, data.size(), pFile);

    BasicString<char, ::Detail::TempStringAllocator> stats;
    {
        BasicString<char, ::Detail::TempStringAllocator> fmt("{0},{1},{2}\n");
        unsigned long largestFree;
        unsigned int freeVM;
        unsigned int largestFreeVM;

        largestFreeVM = nlVirtualLargestBlock();
        freeVM = nlVirtualTotalFree();
        largestFree = GetLargestFreeBlock();

        stats = Format<BasicString<char, ::Detail::TempStringAllocator>, unsigned long, unsigned int, unsigned int>(fmt, largestFree, freeVM, largestFreeVM);
    }

    fwrite(stats.c_str(), 1, stats.size(), pFile);
    fclose(pFile);
}

/**
 * Offset/Address/Size: 0xF0 | 0x800A31AC | size: 0x40
 */
const char* GetMemCardTitle()
{
    switch (g_Language)
    {
    case nlLocalization::LangEnglish:
        return "Super Mario Strikers";
    case nlLocalization::LangJapanese:
        return "Super Mario Strikers";
    default:
        return "Mario Smash Football";
    }
}

/**
 * Offset/Address/Size: 0x6C | 0x800A3128 | size: 0x84
 */
const char* GetMemCardDescription()
{
    switch (g_Language)
    {
    case nlLocalization::LangEnglish:
        return "Save File";
    case nlLocalization::LangFrench:
        return "Fichier de Sauvegarde";
    case nlLocalization::LangGerman:
        return "Spieldatei";
    case nlLocalization::LangSpanish:
        return "Archivo Guardado";
    case nlLocalization::LangItalian:
        return "File di Dati";
    case nlLocalization::LangJapanese:
        return port_region() == PORT_REGION_JAPAN
                   ? "\x83\x5A\x81\x5B\x83\x75\x83\x66\x81\x5B\x83\x5E"  // SJIS: "save data"
                   : "\x83\x74\x83\x40\x83\x8B\x82\xF0\x8F\x9C\x82\xAF\x82\xCE";  // SJIS: "delete file"
    case nlLocalization::LangUKEnglish:
        return "Save File";
    default:
        return "Save File";
    }
}

/**
 * Offset/Address/Size: 0x58 | 0x800A3114 | size: 0x14
 */
const char* GetCupStreamName(eTrophyType trophyType)
{
    static char* TrophyStreamNames[8] = {
        "FE_Cup_Win_Battle_Mush",
        "FE_Cup_Win_Battle_Flow",
        "FE_Cup_Win_Battle_Star",
        "FE_Cup_Win_Battle_Bows",
        "FE_Cup_Win_Super_Mush",
        "FE_Cup_Win_Super_Flow",
        "FE_Cup_Win_Super_Star",
        "FE_Cup_Win_Super_Bows",
    };

    return TrophyStreamNames[trophyType];
}

/**
 * Offset/Address/Size: 0x0 | 0x800A30BC | size: 0x58
 */
void MakeTextBoxReallyWide(TLTextInstance& textInstance)
{
    // PORT: a text instance's component is an FEText; read its BoxSize through that type rather than through TLComponent's phantom member.
    nlVector2& boxSize = ((textInstance.m_OverloadFlags & 0x4) != 0)
                             ? textInstance.m_OverloadedAttributes.BoxSize
                             : ((FEText*)textInstance.m_component)->m_TextAttributes.BoxSize;
    nlVector2 bb = boxSize;
    bb.x = 999.9f;
    textInstance.m_OverloadedAttributes.BoxSize = bb;
    textInstance.m_OverloadFlags |= 0x4;
}
