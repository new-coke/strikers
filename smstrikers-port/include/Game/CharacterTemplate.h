#ifndef _CHARACTERTEMPLATE_H_
#define _CHARACTERTEMPLATE_H_

#include "Game/CharacterLifecycle.h"
#include "Game/Character.h"
#include "Game/Inventory.h"
#include "Game/SHierarchy.h"

class cAnimInventory;
class CharacterPhysicsData;
class AnimRetargetList;

struct tCharacterTemplate
{
    /* 0x00 */ int nCharacterModelID[2];
    /* 0x08 */ unsigned long uAnimInventoryHashID;
    /* 0x0C */ unsigned char bAnimInventoryCopy;
    /* 0x0D */ char pad_0xD[3];
    /* 0x10 */ cInventory<cSHierarchy>* pHierarchyInventory;
    /* 0x14 */ cAnimInventory* pAnimInventory;
    /* 0x18 */ const CharacterPhysicsData* pPhysicsData;
    /* 0x1C */ cInventory<AnimRetargetList>* pAnimRetargetListInventory;
}; // total size: 0x20

struct tGoalieTemplateInfo
{
    const char* szCharName;        // offset 0x0, size 0x4
    const char* szTextureFilename; // offset 0x4, size 0x4
    unsigned char bLoaded;         // offset 0x8, size 0x1
}; // total size: 0xC

struct tCharacterTemplateInfo
{
    const char* szCharName;                       // offset 0x0, size 0x4
    eClassTypes classType;                        // offset 0x4, size 0x4
    const char* szModelFilename;                  // offset 0x8, size 0x4
    const char* szBlendedModelFilename;           // offset 0xC, size 0x4
    const char* szTextureFilename;                // offset 0x10, size 0x4
    const char* szTriggerFilename;                // offset 0x14, size 0x4
    void (*pTriggerCallback)(unsigned int);       // offset 0x18, size 0x4
    const char* szHierarchyFilename;              // offset 0x1C, size 0x4
    const char* szHierarchy;                      // offset 0x20, size 0x4
    const struct AnimProperties* pAnimProperties; // offset 0x24, size 0x4
    int nNumAnimProperties;                       // offset 0x28, size 0x4
    const char* szAnimFilename;                   // offset 0x2C, size 0x4
    const char* szEffectsName;                    // offset 0x30, size 0x4
    SoundPropAccessor* pSFXPropAccessor;          // offset 0x34, size 0x4
    const char* szPhysicsFilename;                // offset 0x38, size 0x4
    const char* szTweaksFilename;                 // offset 0x3C, size 0x4
    const char* szAnimRetargetFilename;           // offset 0x40, size 0x4
}; // total size: 0x44

struct tCharacterTemplateInfo;

s32 GetGoalieIndex(int side);
s32 GetCharacterIndex(const cCharacter* character);
cPlayer* CreateGoalie(eCharacterClass gcc, bool bForViewer);
cPlayer* CreateSidekick(int nPlayerID, int nTeamID, eCharacterClass cc, eCharacterClass captaincc, bool bForViewer);
cPlayer* CreateCharacter(int nPlayerID, int nTeamID, eCharacterClass cc, bool bForViewer);
void CharacterLoadingGuts(tCharacterTemplate* pCharacterTemplate, const tCharacterTemplateInfo& charTemplateInfo, eCharacterClass cc, bool bForViewer);
bool IsCaptain(eCharacterClass cc);
char* GetCharacterName(eCharacterClass cc);

extern cCharacter* g_pCharacters[10];

#endif // _CHARACTERTEMPLATE_H_
