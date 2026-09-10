#include "Game/CharacterTemplate.h"
#include "Game/SHierarchy.h"
#include "Game/SAnim/AnimRetargeter.h"
#include "Game/Player.h"
#include "Game/AI/Fielder.h"
#include "Game/CharacterTweaks.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/Goalie.h"
#include "Game/AI/ScriptAction.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/Sys/GCStream.h"
#include "Game/Audio/AudioStream.h"
#include "Game/AnimInventory.h"
#include "Game/Physics/CharacterPhysicsElement.h"
#include "Game/Triggers/AnimTrigger.h"
#include "Game/Triggers/SebringAnimScript.h"
#include "NL/nlFile.h"
#include "NL/nlFileGC.h"
#include "NL/nlPrint.h"
#include "NL/nlString.h"
#include "NL/nlMemory.h"
#include "NL/gl/gl.h"
#include "NL/gl/glRenderList.h"
#include "NL/gl/glTexture.h"
#include "NL/glx/glxTexture.h"

extern SoundPropAccessor* gpBIRDOSoundPropAccessor;
extern SoundPropAccessor* gpDAISYSoundPropAccessor;
extern SoundPropAccessor* gpDKSoundPropAccessor;
extern SoundPropAccessor* gpHAMBROSSoundPropAccessor;
extern SoundPropAccessor* gpKOOPASoundPropAccessor;
extern SoundPropAccessor* gpLUIGISoundPropAccessor;
extern SoundPropAccessor* gpMARIOSoundPropAccessor;
extern SoundPropAccessor* gpPEACHSoundPropAccessor;
extern SoundPropAccessor* gpTOADSoundPropAccessor;
extern SoundPropAccessor* gpWALUIGISoundPropAccessor;
extern SoundPropAccessor* gpWARIOSoundPropAccessor;
extern SoundPropAccessor* gpYOSHISoundPropAccessor;
extern SoundPropAccessor* gpSUPERSoundPropAccessor;
extern SoundPropAccessor* gpCRITTERSoundPropAccessor;

SebringAnimTagScriptInterpreter* g_pAnimScriptInterp;
class cCharacter;
// PORT: cCharacter*, matching the three files that declare it extern.
cCharacter* g_pCurrentlyUpdatingCharacter;

extern AnimProperties GLOBALAnimProperties[];
extern AnimProperties GOALIEAnimProperties[];
void AnimTriggerCallback_MARIO(unsigned int);

cCharacter* g_pCharacters[10];
static tCharacterTemplateInfo g_aCharacterTemplateInfo[13] = {
    { "birdo", FIELDER, "characters/birdo/birdo.glg", "characters/birdo/birdo_blend.glg", "characters/birdo/birdo.glt", "art/animation/birdo.trg", AnimTriggerCallback_MARIO, "art/animation/birdo.shier", "birdo", GLOBALAnimProperties, 119, "art/animation/birdo.sanim", "mario", gpBIRDOSoundPropAccessor, "art/animation/birdo.cph", "birdo.ini", "art/characters/birdo/animretarget/birdo.bin" },
    { "daisy", FIELDER, "characters/daisy/daisy.glg", "characters/daisy/daisy_blend.glg", "characters/daisy/daisy.glt", "art/animation/daisy.trg", AnimTriggerCallback_MARIO, "art/animation/daisy.shier", "daisy", GLOBALAnimProperties, 119, "art/animation/daisy.sanim", "daisy", gpDAISYSoundPropAccessor, "art/animation/daisy.cph", "daisy.ini", "art/characters/daisy/animretarget/daisy.bin" },
    { "donkeykong", FIELDER, "characters/donkeykong/donkeykong.glg", "characters/donkeykong/donkeykong_blend.glg", "characters/donkeykong/donkeykong.glt", "art/animation/donkeykong.trg", AnimTriggerCallback_MARIO, "art/animation/donkeykong.shier", "donkeykong", GLOBALAnimProperties, 119, "art/animation/donkeykong.sanim", "donkeykong", gpDKSoundPropAccessor, "art/animation/DonkeyKong.cph", "dk.ini", "art/characters/donkeykong/animretarget/donkeykong.bin" },
    { "hammerbro", FIELDER, "characters/hammerbro/hammerbro.glg", "characters/hammerbro/hammerbro_blend.glg", "characters/hammerbro/hammerbro.glt", "art/animation/hammerbro.trg", AnimTriggerCallback_MARIO, "art/animation/hammerbro.shier", "hammerbro", GLOBALAnimProperties, 119, "art/animation/hammerbro.sanim", "hammerbro", gpHAMBROSSoundPropAccessor, "art/animation/hammerbro.cph", "hammerbros.ini", "art/characters/hammerbro/animretarget/hammerbro.bin" },
    { "koopa", FIELDER, "characters/koopa/koopa.glg", "characters/koopa/koopa_blend.glg", "characters/koopa/koopa.glt", "art/animation/koopa.trg", AnimTriggerCallback_MARIO, "art/animation/koopa.shier", "koopa", GLOBALAnimProperties, 119, "art/animation/koopa.sanim", "mario", gpKOOPASoundPropAccessor, "art/animation/koopa.cph", "koopa.ini", "art/characters/koopa/animretarget/koopa.bin" },
    { "luigi", FIELDER, "characters/luigi/luigi.glg", "characters/luigi/luigi_blend.glg", "characters/luigi/luigi.glt", "art/animation/luigi.trg", NULL, "art/animation/luigi.shier", "luigi", GLOBALAnimProperties, 119, "art/animation/luigi.sanim", "luigi", gpLUIGISoundPropAccessor, "art/animation/mario.cph", "luigi.ini", "art/characters/luigi/animretarget/luigi.bin" },
    { "mario", FIELDER, "characters/mario/mario.glg", "characters/mario/mario_blend.glg", "characters/mario/mario.glt", "art/animation/mario.trg", AnimTriggerCallback_MARIO, "art/animation/mario.shier", "mario", GLOBALAnimProperties, 119, "art/animation/mario.sanim", "mario", gpMARIOSoundPropAccessor, "art/animation/mario.cph", "mario.ini", "art/characters/mario/animretarget/mario.bin" },
    { "peach", FIELDER, "characters/peach/peach.glg", "characters/peach/peach_blend.glg", "characters/peach/peach.glt", "art/animation/peach.trg", AnimTriggerCallback_MARIO, "art/animation/peach.shier", "peach", GLOBALAnimProperties, 119, "art/animation/peach.sanim", "peach", gpPEACHSoundPropAccessor, "art/animation/peach.cph", "peach.ini", "art/characters/peach/animretarget/peach.bin" },
    { "toad", FIELDER, "characters/toad/toad.glg", "characters/toad/toad_blend.glg", "characters/toad/toad.glt", "art/animation/toad.trg", AnimTriggerCallback_MARIO, "art/animation/toad.shier", "toad", GLOBALAnimProperties, 119, "art/animation/toad.sanim", "toad", gpTOADSoundPropAccessor, "art/animation/toad.cph", "toad.ini", "art/characters/toad/animretarget/toad.bin" },
    { "waluigi", FIELDER, "characters/waluigi/waluigi.glg", "characters/waluigi/waluigi_blend.glg", "characters/waluigi/waluigi.glt", "art/animation/waluigi.trg", NULL, "art/animation/waluigi.shier", "waluigi", GLOBALAnimProperties, 119, "art/animation/waluigi.sanim", "waluigi", gpWALUIGISoundPropAccessor, "art/animation/waluigi.cph", "waluigi.ini", "art/characters/waluigi/animretarget/waluigi.bin" },
    { "wario", FIELDER, "characters/wario/wario.glg", "characters/wario/wario_blend.glg", "characters/wario/wario.glt", "art/animation/wario.trg", NULL, "art/animation/wario.shier", "wario", GLOBALAnimProperties, 119, "art/animation/wario.sanim", "wario", gpWARIOSoundPropAccessor, "art/animation/wario.cph", "wario.ini", "art/characters/wario/animretarget/wario.bin" },
    { "yoshi", FIELDER, "characters/yoshi/yoshi.glg", "characters/yoshi/yoshi_blend.glg", "characters/yoshi/yoshi.glt", "art/animation/yoshi.trg", AnimTriggerCallback_MARIO, "art/animation/yoshi.shier", "yoshi", GLOBALAnimProperties, 119, "art/animation/yoshi.sanim", "yoshi", gpYOSHISoundPropAccessor, "art/animation/yoshi.cph", "yoshi.ini", "art/characters/yoshi/animretarget/yoshi.bin" },
    { "superteam", FIELDER, "characters/superteam/superteam.glg", "characters/superteam/superteam_blend.glg", "characters/superteam/superteam.glt", "art/animation/superteam.trg", NULL, "art/animation/superteam.shier", "superteam", GLOBALAnimProperties, 119, "art/animation/superteam.sanim", "superteam", gpSUPERSoundPropAccessor, "art/animation/superteam.cph", "superteam.ini", "art/characters/superteam/animretarget/superteam.bin" },
};
static tCharacterTemplate* g_aCharacterTemplates[13];
static tCharacterTemplateInfo g_GoalieTemplateInfo = {
    "mariogoalie", GOALIE, "characters/mariogoalie/mariogoalie.glg", "characters/mariogoalie/mariogoalie_blend.glg", "characters/mariogoalie/mariogoalie.glt", "art/animation/mariogoalie.trg", NULL, "art/animation/mariogoalie.shier", "mariogoalie", GOALIEAnimProperties, 149, "art/animation/mariogoalie.sanim", "mario", gpCRITTERSoundPropAccessor, "art/animation/mariogoalie.cph", "goalie.ini", NULL
};
static tCharacterTemplate* g_GoalieTemplate;

// PORT: a texture hash; as s32, one with bit 31 set sign-extends when compared against the unsigned long handle.
static u32 skiptexture = 0xFFFFFFFF;

static tGoalieTemplateInfo g_GoalieTextureInfo[9] = {
    { "daisygoalie", "characters/daisygoalie/daisygoalie.glt", 0 },
    { "donkeykonggoalie", "characters/donkeykonggoalie/donkeykonggoalie.glt", 0 },
    { "luigigoalie", "characters/luigigoalie/luigigoalie.glt", 0 },
    { "mariogoalie", "characters/mariogoalie/mariogoalie.glt", 0 },
    { "peachgoalie", "characters/peachgoalie/peachgoalie.glt", 0 },
    { "waluigigoalie", "characters/waluigigoalie/waluigigoalie.glt", 0 },
    { "wariogoalie", "characters/wariogoalie/wariogoalie.glt", 0 },
    { "yoshigoalie", "characters/yoshigoalie/yoshigoalie.glt", 0 },
    { "superteamgoalie", "characters/superteamgoalie/superteamgoalie.glt", 0 },
};

static u32 GetHashFromTextureFile(const char* szTextureFileName)
{
    char name[200];
    char* pDest = name;
    const char* pSrc = NULL;
    int count = 0;

    for (count = 0; count < 100; count++)
    {
        if (szTextureFileName[count] == '\\' || szTextureFileName[count] == '/')
        {
            pSrc = &szTextureFileName[count + 1];
            break;
        }
    }

    for (int k = 0; k < 100; k++)
    {
        if (*pSrc != '\0' && *pSrc != '.')
        {
            __memcpy(pDest, pSrc, sizeof(*pDest));
            pSrc++;
            pDest++;
        }
        else
        {
            *pDest = '\0';
            return nlStringLowerHash(name);
        }
    }
    return 0;
}

static cAnimInventory* FindDuplicateAnimInventory(int nCurIndex, unsigned long uHashID);
static char* GetCharacterTriggerFileName(eCharacterClass cc);

/**
 * Offset/Address/Size: 0x2128 | 0x80014410 | size: 0x34
 */
char* GetCharacterName(eCharacterClass cc)
{
    if (cc < 13)
    {
        return (char*)g_aCharacterTemplateInfo[cc].szCharName;
    }
    return (char*)g_GoalieTextureInfo[cc - 13].szCharName;
}

static char* GetCharacterModelFileName(eCharacterClass cc)
{
    if (cc < NUM_FIELDER_CLASSES)
    {
        return (char*)g_aCharacterTemplateInfo[cc].szModelFilename;
    }
    return (char*)g_GoalieTemplateInfo.szModelFilename;
}

static char* GetCharacterModelTextureFileName(eCharacterClass cc)
{
    if (cc < NUM_FIELDER_CLASSES)
    {
        return (char*)g_aCharacterTemplateInfo[cc].szTextureFilename;
    }
    return (char*)g_GoalieTextureInfo[cc - NUM_FIELDER_CLASSES].szTextureFilename;
}

static AnimProperties* GetCharacterModelAnimProperties(eCharacterClass cc)
{
    if (cc < NUM_FIELDER_CLASSES)
    {
        return (AnimProperties*)g_aCharacterTemplateInfo[cc].pAnimProperties;
    }
    return (AnimProperties*)g_GoalieTemplateInfo.pAnimProperties;
}

static int GetCharacterModelNumAnumProperties(eCharacterClass cc)
{
    if (cc < NUM_FIELDER_CLASSES)
    {
        return g_aCharacterTemplateInfo[cc].nNumAnimProperties;
    }
    return g_GoalieTemplateInfo.nNumAnimProperties;
}

static char* GetCharacterModelHierarchyFileName(eCharacterClass cc)
{
    if (cc < NUM_FIELDER_CLASSES)
    {
        return (char*)g_aCharacterTemplateInfo[cc].szHierarchyFilename;
    }
    return (char*)g_GoalieTemplateInfo.szHierarchyFilename;
}

static char* GetCharacterModelHierarchyName(eCharacterClass cc)
{
    if (cc < NUM_FIELDER_CLASSES)
    {
        return (char*)g_aCharacterTemplateInfo[cc].szHierarchy;
    }
    return (char*)g_GoalieTemplateInfo.szHierarchy;
}

static char* GetCharacterModelAnimFileName(eCharacterClass cc)
{
    if (cc < NUM_FIELDER_CLASSES)
    {
        return (char*)g_aCharacterTemplateInfo[cc].szAnimFilename;
    }
    return (char*)g_GoalieTemplateInfo.szAnimFilename;
}

static unsigned char IsSidekick(eCharacterClass cc)
{
    if (cc == BIRDO || cc == HAMMERBROS || cc == KOOPA || cc == TOAD)
    {
        return true;
    }
    return false;
}

/**
 * Offset/Address/Size: 0x20EC | 0x800143D4 | size: 0x3C
 */
bool IsCaptain(eCharacterClass cc)
{
    if (((cc - 1) <= 1U) || ((cc - 5) <= 2U) || ((cc - 9) <= 2U) || (cc == 0xC))
    {
        return true;
    }
    return false;
}

/**
 * Offset/Address/Size: 0x1CFC | 0x80013FE4 | size: 0x3F0
 */
void CharacterLoadingGuts(tCharacterTemplate* pCharacterTemplate, const tCharacterTemplateInfo& charTemplateInfo, eCharacterClass cc, bool bForViewer)
{
    glModel* pRigidCharacterModel = glLoadModel(charTemplateInfo.szModelFilename, NULL);
    glModel* pBlendCharacterModel = glLoadModel(charTemplateInfo.szBlendedModelFilename, NULL);

    pCharacterTemplate->nCharacterModelID[0] = pRigidCharacterModel->id;
    pCharacterTemplate->nCharacterModelID[1] = pBlendCharacterModel->id;

    pCharacterTemplate->pHierarchyInventory = new (nlMalloc(sizeof(cInventory<cSHierarchy>), 8, false)) cInventory<cSHierarchy>();
    pCharacterTemplate->pHierarchyInventory->AddFile((char*)charTemplateInfo.szHierarchyFilename);

    if (!bForViewer)
    {
        CharacterPhysicsData* pPhys = new (nlMalloc(sizeof(CharacterPhysicsData), 8, false)) CharacterPhysicsData();
        pCharacterTemplate->pPhysicsData = pPhys;
        LoadCharacterPhysicsElements(charTemplateInfo.szPhysicsFilename, (CharacterPhysicsData*)pCharacterTemplate->pPhysicsData);
    }
    else
    {
        pCharacterTemplate->pPhysicsData = NULL;
    }

    pCharacterTemplate->uAnimInventoryHashID = nlStringLowerHash(charTemplateInfo.szAnimFilename);

    cAnimInventory* found = FindDuplicateAnimInventory(cc, pCharacterTemplate->uAnimInventoryHashID);

    if (found != NULL)
    {
        pCharacterTemplate->pAnimInventory = found;
        pCharacterTemplate->bAnimInventoryCopy = true;
    }
    else
    {
        cAnimInventory* pAnim = new (nlMalloc(sizeof(cAnimInventory), 8, false))
            cAnimInventory(charTemplateInfo.pAnimProperties, charTemplateInfo.nNumAnimProperties);
        pCharacterTemplate->pAnimInventory = pAnim;
        pCharacterTemplate->pAnimInventory->AddAnimBundle(charTemplateInfo.szAnimFilename);
        pCharacterTemplate->bAnimInventoryCopy = false;

        cInventory<cSAnim>* pAnimCont = pCharacterTemplate->pAnimInventory->m_pSAnimInventory;
        g_pAnimScriptInterp->SetupAnimationTriggers(GetCharacterTriggerFileName(cc), pAnimCont);
    }

    if (charTemplateInfo.szAnimRetargetFilename != NULL)
    {
        pCharacterTemplate->pAnimRetargetListInventory = new (nlMalloc(sizeof(cInventory<AnimRetargetList>), 8, false)) cInventory<AnimRetargetList>();
        pCharacterTemplate->pAnimRetargetListInventory->AddFile((char*)charTemplateInfo.szAnimRetargetFilename);
    }
    else
    {
        pCharacterTemplate->pAnimRetargetListInventory = NULL;
    }
}

static cAnimInventory* FindDuplicateAnimInventory(int nCurIndex, unsigned long uHashID)
{
    for (int index = 0; index < NUM_FIELDER_CLASSES; index++)
    {
        if (index == nCurIndex)
            continue;
        if (g_aCharacterTemplates[index] == NULL)
            continue;
        if (uHashID != g_aCharacterTemplates[index]->uAnimInventoryHashID)
            continue;
        return g_aCharacterTemplates[index]->pAnimInventory;
    }
    return NULL;
}

static char* GetCharacterTriggerFileName(eCharacterClass cc)
{
    if (cc < NUM_FIELDER_CLASSES)
    {
        return (char*)g_aCharacterTemplateInfo[cc].szTriggerFilename;
    }
    return (char*)g_GoalieTemplateInfo.szTriggerFilename;
}

/**
 * Offset/Address/Size: 0x1ABC | 0x80013DA4 | size: 0x240
 */
cPlayer* CreateCharacter(int nPlayerID, int nTeamID, eCharacterClass cc, bool bForViewer)
{
    if (cc >= NUM_FIELDER_CLASSES)
    {
        return CreateGoalie(cc, bForViewer);
    }

    if (g_aCharacterTemplates[cc] == NULL)
    {
        glLoadTextureBundle(g_aCharacterTemplateInfo[cc].szTextureFilename);
        g_aCharacterTemplates[cc] = (tCharacterTemplate*)nlMalloc(sizeof(tCharacterTemplate), 8, false);
        CharacterLoadingGuts(g_aCharacterTemplates[cc], g_aCharacterTemplateInfo[cc], cc, bForViewer);
    }

    cInventory<cSHierarchy>* pHierInv = g_aCharacterTemplates[cc]->pHierarchyInventory;
    u32 hash = nlStringHash(g_aCharacterTemplateInfo[cc].szHierarchy);

    AnimRetargetList* pAnimRetargetList;
    FielderTweaks* pTweaks;
    cSHierarchy* pHierarchy = pHierInv->Find((unsigned int)hash);

    pAnimRetargetList = NULL;
    if (g_aCharacterTemplates[cc]->pAnimRetargetListInventory != NULL)
    {
        pAnimRetargetList = g_aCharacterTemplates[cc]->pAnimRetargetListInventory->Find(0);
    }

    pTweaks = new (nlMalloc(sizeof(FielderTweaks), 8, false)) FielderTweaks(g_aCharacterTemplateInfo[cc].szTweaksFilename);

    cPlayer* pChar;
    if (!bForViewer)
    {
        cFielder* pFielder = new (nlMalloc(sizeof(cFielder), 8, false)) cFielder(
            nPlayerID, nTeamID, cc, (const int*)g_aCharacterTemplates[cc], pHierarchy, g_aCharacterTemplates[cc]->pAnimInventory, g_aCharacterTemplates[cc]->pPhysicsData, pTweaks, pAnimRetargetList);
        pChar = pFielder;
    }
    else
    {
        cPlayer* pPlayer = new (nlMalloc(sizeof(cPlayer), 8, false)) cPlayer(
            nPlayerID, cc, (const int*)g_aCharacterTemplates[cc], pHierarchy, g_aCharacterTemplates[cc]->pAnimInventory, g_aCharacterTemplates[cc]->pPhysicsData, (PlayerTweaks*)pTweaks, pAnimRetargetList, (eClassTypes)1);
        pChar = pPlayer;
    }

    pChar->m_szEffectsName = g_aCharacterTemplateInfo[cc].szEffectsName;
    if (!AudioLoader::gbDisableAudio)
    {
        pChar->SetSFX(g_aCharacterTemplateInfo[cc].pSFXPropAccessor);
    }

    return pChar;
}

/**
 * Offset/Address/Size: 0x1AA0 | 0x80013D88 | size: 0x1C
 */
// PORT: glxTextureLoadCallback_t returns unsigned long, and it was declared s32 and installed through a cast.
static unsigned long SidekickTexture_cb(unsigned long textureId)
{
    unsigned long result = (unsigned long)-1;
    if (textureId != skiptexture)
    {
        result = textureId;
    }
    return result;
}

/**
 * Offset/Address/Size: 0x14A4 | 0x8001378C | size: 0x5FC
 */
cPlayer* CreateSidekick(int nPlayerID, int nTeamID, eCharacterClass cc, eCharacterClass captaincc, bool bForViewer)
{
    char szTexPath[64];
    char szArtPath[64];
    char szBundlePath[64];
    char szPlayerPath[64];

    glxTextureLoadCallback_t oldCallback = glx_SetLoadCallback(SidekickTexture_cb);

    if (cc == HAMMERBROS)
    {
        nlSNPrintf(szTexPath, 64, "hammerbro/hammer_mario");
    }
    else
    {
        nlSNPrintf(szTexPath, 64, "%s/%s_mario", GetCharacterName(cc), GetCharacterName(cc));
    }

    skiptexture = glGetTexture(szTexPath);

    cPlayer* pChar = CreateCharacter(nPlayerID, nTeamID, cc, bForViewer);

    glx_SetLoadCallback(oldCallback);

    bool swaptextureloaded = false;

    if (cc == HAMMERBROS)
    {
        nlSNPrintf(szBundlePath, 64, "characters/%s/hammer_%s.glt", GetCharacterName(cc), GetCharacterName(captaincc));
        nlSNPrintf(szArtPath, 64, "art/characters/%s/hammer_%s.glt", GetCharacterName(cc), GetCharacterName(captaincc));
    }
    else
    {
        nlSNPrintf(szBundlePath, 64, "characters/%s/%s_%s.glt", GetCharacterName(cc), GetCharacterName(cc), GetCharacterName(captaincc));
        nlSNPrintf(szArtPath, 64, "art/characters/%s/%s_%s.glt", GetCharacterName(cc), GetCharacterName(cc), GetCharacterName(captaincc));
    }

    if (cc == HAMMERBROS)
    {
        nlSNPrintf(szPlayerPath, 64, "hammer_%s/hammer_%s", GetCharacterName(captaincc), GetCharacterName(captaincc));
    }
    else
    {
        nlSNPrintf(szPlayerPath, 64, "%s_%s/%s_%s", GetCharacterName(cc), GetCharacterName(captaincc), GetCharacterName(cc), GetCharacterName(captaincc));
    }

    if (glTextureLoad(glGetTexture(szPlayerPath)))
    {
        swaptextureloaded = true;
    }
    else
    {
        nlFile* texturefile = nlOpen(szArtPath);
        if (texturefile != NULL)
        {
            nlClose(texturefile);
            swaptextureloaded = glLoadTextureBundle(szBundlePath);
        }
    }

    if (swaptextureloaded)
    {
        if (cc == HAMMERBROS)
        {
            nlSNPrintf(szBundlePath, 64, "%s/hammer_mario", GetCharacterName(cc));
        }
        else
        {
            nlSNPrintf(szBundlePath, 64, "%s/%s_mario", GetCharacterName(cc), GetCharacterName(cc));
        }
        pChar->m_uNormalTextureID = glGetTexture(szBundlePath);
        pChar->m_uSwapTextureID = glGetTexture(szPlayerPath);
    }
    else
    {
        pChar->m_uNormalTextureID = (u32)-1;
        pChar->m_uSwapTextureID = (u32)-1;
    }

    return pChar;
}

/**
 * Offset/Address/Size: 0xE70 | 0x80013158 | size: 0x634
 */
cPlayer* CreateGoalie(eCharacterClass gcc, bool bForViewer)
{
    s32 goalieIdx = gcc - NUM_FIELDER_CLASSES;
    if (!g_GoalieTextureInfo[goalieIdx].bLoaded)
    {
        glLoadTextureBundle(g_GoalieTextureInfo[goalieIdx].szTextureFilename);
        g_GoalieTextureInfo[goalieIdx].bLoaded = 1;
    }

    if (g_GoalieTemplate == NULL)
    {
        g_GoalieTemplate = (tCharacterTemplate*)nlMalloc(sizeof(tCharacterTemplate), 8, false);
        CharacterLoadingGuts(g_GoalieTemplate, g_GoalieTemplateInfo, gcc, bForViewer);
    }

    cSHierarchy* pHierarchy = g_GoalieTemplate->pHierarchyInventory->Find((char*)g_GoalieTemplateInfo.szHierarchy);

    AnimRetargetList* pAnimRetargetList = NULL;
    if (g_GoalieTemplate->pAnimRetargetListInventory != NULL)
    {
        pAnimRetargetList = g_GoalieTemplate->pAnimRetargetListInventory->Find(0);
    }

    GoalieTweaks* pTweaks = new (nlMalloc(sizeof(GoalieTweaks), 8, false)) GoalieTweaks(g_GoalieTemplateInfo.szTweaksFilename);

    cPlayer* pChar;
    if (!bForViewer)
    {
        Goalie* pGoalie = new (nlMalloc(sizeof(Goalie), 8, false)) Goalie(
            gcc, (const int*)g_GoalieTemplate, pHierarchy, g_GoalieTemplate->pAnimInventory, g_GoalieTemplate->pPhysicsData, pTweaks, pAnimRetargetList);
        pChar = pGoalie;
    }
    else
    {
        cPlayer* pPlayer = new (nlMalloc(sizeof(cPlayer), 8, false)) cPlayer(
            4, gcc, (const int*)g_GoalieTemplate, pHierarchy, g_GoalieTemplate->pAnimInventory, g_GoalieTemplate->pPhysicsData, (PlayerTweaks*)pTweaks, pAnimRetargetList, (eClassTypes)3);
        pChar = pPlayer;
    }

    pChar->m_szEffectsName = g_GoalieTemplateInfo.szEffectsName;
    pChar->m_uNormalTextureID = GetHashFromTextureFile(g_GoalieTemplateInfo.szTextureFilename);
    pChar->m_uSwapTextureID = GetHashFromTextureFile(g_GoalieTextureInfo[goalieIdx].szTextureFilename);

    if (!AudioLoader::gbDisableAudio)
    {
        pChar->SetSFX(g_GoalieTemplateInfo.pSFXPropAccessor);
    }

    return pChar;
}

static eCharacterClass GetGoalieFromCaptain(eCharacterClass captain)
{
    switch (captain)
    {
    case DAISY:
        return DAISY_GOALIE;
    case DONKEYKONG:
        return DONKEYKONG_GOALIE;
    case LUIGI:
        return LUIGI_GOALIE;
    case MARIO:
        return MARIO_GOALIE;
    case PEACH:
        return PEACH_GOALIE;
    case WALUIGI:
        return WALUIGI_GOALIE;
    case WARIO:
        return WARIO_GOALIE;
    case YOSHI:
        return YOSHI_GOALIE;
    case MYSTERY:
        return SUPERTEAM_GOALIE;
    default:
        return MARIO_GOALIE;
    }
}

static inline bool CaptainClassGreater(eCharacterClass first, eCharacterClass second)
{
    return first > second;
}

static inline bool SameCharacterClass(const eCharacterClass* first, const eCharacterClass* second, int index)
{
    return first[index] == second[index];
}

/**
 * Offset/Address/Size: 0x954 | 0x80012C3C | size: 0x51C
 */
void CreateCharacters()
{
    eCharacterClass captain[2];
    eCharacterClass sidekick[2];
    eCharacterClass goalie[2];
    captain[0] = ConvertToCharacterClass(nlSingleton<GameInfoManager>::Instance()->GetTeam(0));
    captain[1] = ConvertToCharacterClass(nlSingleton<GameInfoManager>::Instance()->GetTeam(1));
    sidekick[0] = ConvertToCharacterClass(nlSingleton<GameInfoManager>::Instance()->GetSidekick(0));
    sidekick[1] = ConvertToCharacterClass(nlSingleton<GameInfoManager>::Instance()->GetSidekick(1));

    goalie[0] = GetGoalieFromCaptain(captain[0]);
    goalie[1] = GetGoalieFromCaptain(captain[1]);

    Config& cfg = Config::Global();
    bool allcaptains = cfg.Get<bool>("allcaptains", false);
    if (allcaptains)
    {
        sidekick[0] = captain[0];
        sidekick[1] = captain[1];
    }

    if (captain[0] == MYSTERY)
    {
        sidekick[0] = MYSTERY;
    }
    else if (captain[1] == MYSTERY)
    {
        sidekick[1] = MYSTERY;
    }

    nlVector3 pos[8] = {
        { 1.5f, 1.5f, 0.0f },
        { 1.5f, -1.5f, 0.0f },
        { 1.5f, 0.0f, 0.0f },
        { 1.5f, 2.5f, 0.0f },
        { -1.5f, 1.5f, 0.0f },
        { -1.5f, -1.5f, 0.0f },
        { -1.5f, 0.0f, 0.0f },
        { -1.5f, 2.5f, 0.0f },
    };

    nlVector3 goaliepos[2] = {
        { 18.0f, 0.0f, 0.0f },
        { -18.0f, 0.0f, 0.0f },
    };

    int plrindex;
    int charIdx;

    SebringAnimTagScriptInterpreter* pInterp = new (nlMalloc(sizeof(SebringAnimTagScriptInterpreter), 8, false)) SebringAnimTagScriptInterpreter();

    g_pAnimScriptInterp = pInterp;

    for (int teami = 0; teami < 2; teami++)
    {
        plrindex = CaptainClassGreater(captain[0], captain[1]) ? !teami : teami;

        int idx = plrindex * 4;
        g_pCharacters[idx] = CreateCharacter(0, plrindex, captain[plrindex], false);
        g_pCharacters[idx]->SetPosition(pos[idx]);
        ((Audio::cCharacterSFX*)g_pCharacters[idx]->m_pCharacterSFX)->mGroup = idx;

        g_pTeams[plrindex]->SetPlayer((cPlayer*)g_pCharacters[idx], 0);
        ((cPlayer*)g_pCharacters[idx])->m_pTeam = g_pTeams[plrindex];

        g_pCharacters[plrindex + 8] = CreateGoalie(goalie[plrindex], false);
        g_pCharacters[plrindex + 8]->SetPosition(goaliepos[plrindex]);

        g_pTeams[plrindex]->SetGoalie((Goalie*)g_pCharacters[plrindex + 8]);
        ((cPlayer*)g_pCharacters[plrindex + 8])->m_pTeam = g_pTeams[plrindex];
    }

    for (int teami = 0; teami < 2; teami++)
    {
        plrindex = (sidekick[0] > sidekick[1]) ? !teami : teami;

        charIdx = plrindex * 4 + 1;

        for (int index = 1; index < 4; index++)
        {
            if (SameCharacterClass(sidekick, captain, plrindex))
            {
                g_pCharacters[charIdx] = CreateCharacter(index, plrindex, captain[plrindex], false);
            }
            else
            {
                g_pCharacters[charIdx] = (cCharacter*)CreateSidekick(index, plrindex, sidekick[plrindex], captain[plrindex], false);
            }

            g_pCharacters[charIdx]->SetPosition(pos[charIdx]);
            ((Audio::cCharacterSFX*)g_pCharacters[charIdx]->m_pCharacterSFX)->mGroup = charIdx;

            g_pTeams[plrindex]->SetPlayer((cPlayer*)g_pCharacters[charIdx], index);
            ((cPlayer*)g_pCharacters[charIdx])->m_pTeam = g_pTeams[plrindex];

            charIdx++;
        }

        g_pTeams[plrindex]->UpdateControllers();
    }
}

/**
 * Offset/Address/Size: 0x294 | 0x8001257C | size: 0x6C0
 */
void DestroyCharacters()
{
    int i;

    delete g_pAnimScriptInterp;
    g_pAnimScriptInterp = NULL;

    for (i = 0; i < 10; i++)
    {
        delete g_pCharacters[i];
        g_pCharacters[i] = NULL;
    }

    for (i = 0; i < 13; i++)
    {
        if (g_aCharacterTemplates[i] != NULL)
        {
            delete g_aCharacterTemplates[i]->pHierarchyInventory;

            if (!g_aCharacterTemplates[i]->bAnimInventoryCopy)
            {
                delete g_aCharacterTemplates[i]->pAnimInventory;
            }

            delete g_aCharacterTemplates[i]->pPhysicsData;
            if (g_aCharacterTemplates[i]->pAnimRetargetListInventory != NULL)
            {
                delete g_aCharacterTemplates[i]->pAnimRetargetListInventory;
            }
            delete g_aCharacterTemplates[i];
            g_aCharacterTemplates[i] = NULL;
        }
    }

    if (g_GoalieTemplate != NULL)
    {
        delete g_GoalieTemplate->pHierarchyInventory;

        if (!g_GoalieTemplate->bAnimInventoryCopy)
        {
            delete g_GoalieTemplate->pAnimInventory;
        }

        delete g_GoalieTemplate->pPhysicsData;
        if (g_GoalieTemplate->pAnimRetargetListInventory != NULL)
        {
            delete g_GoalieTemplate->pAnimRetargetListInventory;
        }
        delete g_GoalieTemplate;
        g_GoalieTemplate = NULL;
    }

    g_GoalieTextureInfo[0].bLoaded = 0;
    g_GoalieTextureInfo[1].bLoaded = 0;
    g_GoalieTextureInfo[2].bLoaded = 0;
    g_GoalieTextureInfo[3].bLoaded = 0;
    g_GoalieTextureInfo[4].bLoaded = 0;
    g_GoalieTextureInfo[5].bLoaded = 0;
    g_GoalieTextureInfo[6].bLoaded = 0;
    g_GoalieTextureInfo[7].bLoaded = 0;
    g_GoalieTextureInfo[8].bLoaded = 0;

    SlotPoolBase::BaseFreeBlocks(&AnimTriggerCallbackInfo::m_AnimTriggerCallbackInfoSlotPool, sizeof(AnimTriggerCallbackInfo));
    SlotPoolBase::BaseFreeBlocks(&ScriptAction::m_ScriptActionSlotPool, sizeof(ScriptAction));
}

/**
 * Offset/Address/Size: 0x1C0 | 0x800124A8 | size: 0xD4
 */
s32 GetCharacterIndex(const cCharacter* character)
{
    cCharacter** ptr = g_pCharacters;
    for (s32 i = 0; i < 10; ++i, ptr++)
    {
        if (*ptr == character)
            return i;
    }
    return -1;
}

/**
 * Offset/Address/Size: 0x0 | 0x800122E8 | size: 0x1C0
 */
s32 GetGoalieIndex(int side)
{
    if (side == 0)
    {
        cCharacter** ptr = g_pCharacters;
        for (s32 i = 0; i < 10; ++i, ptr++)
        {
            if (*ptr == g_pCharacters[8])
                return i;
        }
        return -1;
    }

    cCharacter** ptr = g_pCharacters;
    for (s32 i = 0; i < 10; ++i, ptr++)
    {
        if (*ptr == g_pCharacters[9])
            return i;
    }
    return -1;
}
