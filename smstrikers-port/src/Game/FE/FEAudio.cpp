#include "Game/FE/FEAudio.h"

#include "NL/nlString.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/BasicStadium.h"
#include "Game/Game.h"
#include "Game/Sys/audio.h"
#include "Game/Sys/eventman.h"
#include "Game/Sys/EventData.h"
#include "NL/nlList.h"
#include "NL/nlAlgorithm.h"

AnimAudioEventLookup* gp_AnimAudioEventTable;
unsigned long gNumAnimAudioEvents;

static bool mIsEnabled = true;
static void* gpLastSoundFromPlayer;

struct FrontEndAnimAudioData : EventData
{
    virtual u32 GetID() { return 0x16D; }
    /* 0x04 */ unsigned long audioIdentifier;
}; // total size: 0x8

#include <stdlib.h>   // PORT: was a local declaration with `unsigned long`, which is not size_t on Windows

typedef nlListSlotPoolHigh<AnimAudioEventLookup> FELookupPool;

/**
 * Offset/Address/Size: 0x7F4 | 0x8009F5A0 | size: 0x408
 */
void FEAudio::BuildAnimAudioEventLookup()
{
    unsigned long fileSize;
    ListEntry<AnimAudioEventLookup>* pEntry;
    char* pFileData = (char*)nlLoadEntireFile("audio/FEAnimAudio.txt", &fileSize, 0x20, (eAllocType)1);

    static AnimAudioEventLookup blankEntry;
    FELookupPool LookupList(0x10, 0x10);

    SimpleParser parser;
    parser.StartParsing(pFileData, fileSize, false);

    char* pToken;
    char* pSFXTypeStr;

    do
    {
        pToken = parser.NextToken(false);
        while (pToken != NULL)
        {
            char* equalPos = strchr(pToken, '=');
            if (equalPos != NULL)
            {
                equalPos[-1] = '\0';
                pSFXTypeStr = equalPos + 2;
                pEntry = LookupList.Allocate(AnimAudioEventLookup(blankEntry));

                nlListAddEnd(&LookupList.m_Head, &LookupList.m_Tail, pEntry);
                pEntry->entry.eventNameHash = nlStringHash(pToken);
                nlStrNCpy(pEntry->entry.szSFXType, pSFXTypeStr, 0x32);
                gNumAnimAudioEvents++;
            }

            pToken = parser.NextToken(false);
        }
    } while (parser.AdvanceLine());

    gp_AnimAudioEventTable = (AnimAudioEventLookup*)nlMalloc(gNumAnimAudioEvents * sizeof(AnimAudioEventLookup), 8, false);

    int i = 0;
    nlListIterator<AnimAudioEventLookup> iter = LookupList.Begin();
    while (iter.IsValid())
    {
        gp_AnimAudioEventTable[i] = iter.Current();
        i++;
        iter.Next();
    }

    nlQSort<AnimAudioEventLookup>(gp_AnimAudioEventTable, (int)gNumAnimAudioEvents, &nlDefaultQSortComparer<AnimAudioEventLookup>);

    nlFree(pFileData);

    LookupList.Clear();
    SlotPoolBase::BaseFreeBlocks(&LookupList.m_Allocator, sizeof(ListEntry<AnimAudioEventLookup>));
}

/**
 * Offset/Address/Size: 0x738 | 0x8009F4E4 | size: 0xBC
 */
long FEAudio::PlayAnimAudioEvent(const char* incomingstring, bool value)
{
    unsigned long hash = nlStringLowerHash(incomingstring);

    if (!AudioLoader::IsInited())
    {
        return -1;
    }

    if (!mIsEnabled)
    {
        return -1;
    }

    unsigned long stackHash = hash;
    AnimAudioEventLookup* result = nlBSearch<AnimAudioEventLookup, unsigned long>(stackHash, gp_AnimAudioEventTable, gNumAnimAudioEvents);
    AnimAudioEventLookup* event;
    if (result)
    {
        event = result;
    }
    else
    {
        event = NULL;
    }

    if (nlStrICmp<char>(event->szSFXType, "empty") == 0)
    {
        return -1;
    }

    return Audio::PlayWorldSFXbyStr(event->szSFXType, 100.0f, -1.0f, false, true, NULL, NULL, NULL);
}

/**
 * Offset/Address/Size: 0x6A8 | 0x8009F454 | size: 0x90
 */
void FEAudio::StopAnimAudioEvent(const char* incomingstring)
{
    if (!AudioLoader::IsInited())
    {
        return;
    }

    unsigned long hash = nlStringLowerHash(incomingstring);

    if (!AudioLoader::IsInited())
    {
        return;
    }

    unsigned long stackHash = hash;
    AnimAudioEventLookup* result = nlBSearch<AnimAudioEventLookup, unsigned long>(stackHash, gp_AnimAudioEventTable, gNumAnimAudioEvents);
    AnimAudioEventLookup* event;
    if (result)
    {
        event = result;
    }
    else
    {
        event = NULL;
    }

    if (nlStrCmp<char>(event->szSFXType, "empty") != 0)
    {
        Audio::StopWorldSFXbyStr(event->szSFXType);
    }
}

/**
 * Offset/Address/Size: 0x5F0 | 0x8009F39C | size: 0xB8
 */
long FEAudio::PlayAnimAudioEvent(unsigned long incomingHash, bool value)
{
    if (!AudioLoader::IsInited())
    {
        return -1;
    }

    if (!mIsEnabled)
    {
        return -1;
    }

    unsigned long stackHash = incomingHash;
    AnimAudioEventLookup* result = nlBSearch<AnimAudioEventLookup, unsigned long>(stackHash, gp_AnimAudioEventTable, gNumAnimAudioEvents);
    AnimAudioEventLookup* event;
    if (result)
    {
        event = result;
    }
    else
    {
        event = NULL;
    }

    if (nlStrICmp<char>(event->szSFXType, "empty") == 0)
    {
        return -1;
    }

    return Audio::PlayWorldSFXbyStr(event->szSFXType, 100.0f, -1.0f, false, true, NULL, NULL, NULL);
}

/**
 * Offset/Address/Size: 0x290 | 0x8009F03C | size: 0x360
 */
void FEAudio::PlayRandomVoiceToggleSFX()
{
    if (g_pGame != NULL)
    {
        Audio::eCharSFX charInGameDialogueTypes[] = {
            Audio::CHARSFX_EFFORTS_ATTACK_01,
            Audio::CHARSFX_EFFORTS_ATTACK_02,
            Audio::CHARSFX_EFFORTS_ATTACK_03,
            Audio::CHARSFX_EFFORTS_HIT_01,
            Audio::CHARSFX_EFFORTS_HIT_02,
            Audio::CHARSFX_EFFORTS_HIT_03,
            Audio::CHARSFX_EFFORTS_GET_HIT_01,
            Audio::CHARSFX_EFFORTS_GET_HIT_02,
            Audio::CHARSFX_EFFORTS_GET_HIT_03,
            Audio::CHARSFX_EFFORTS_PAIN_01,
            Audio::CHARSFX_EFFORTS_PAIN_02,
            Audio::CHARSFX_EFFORTS_PAIN_03,
            Audio::CHARSFX_EFFORTS_ELECTROCUTE_02,
            Audio::CHARSFX_EFFORTS_ELECTROCUTE_03,
            Audio::CHARSFX_EFFORTS_EXERT_01,
            Audio::CHARSFX_EFFORTS_EXERT_02,
            Audio::CHARSFX_EFFORTS_EXERT_03,
            Audio::CHARSFX_EFFORTS_KICK_01,
            Audio::CHARSFX_EFFORTS_KICK_02,
            Audio::CHARSFX_EFFORTS_KICK_03,
            Audio::CHARSFX_EFFORTS_PERFECT_PASS,
            Audio::CHARSFX_BREATH_WITH_BALL,
            Audio::CHARSFX_CALL_HEY_01,
            Audio::CHARSFX_CALL_HEY_02,
            Audio::CHARSFX_CALL_WO_01,
            Audio::CHARSFX_BOWSER_BREATH_FIRE,
        };

        static Audio::eCharSFX lastSoundPlayedType;
        static signed char init;
        if (!init)
        {
            lastSoundPlayedType = Audio::CHARSFX_NONE;
            init = 1;
        }

        GameInfoManager& gameInfo = *GameInfoManager::GetInstance();
        gameInfo.GetSidekick(0);
        gameInfo.GetSidekick(0);

        Audio::eCharSFX newSound;
        unsigned int randomElementIndex = nlRandom(26, &nlDefaultSeed);

        if (lastSoundPlayedType != Audio::CHARSFX_NONE)
        {
            if (lastSoundPlayedType == charInGameDialogueTypes[randomElementIndex])
            {
                randomElementIndex = (randomElementIndex + 1) % 26;
            }

            if (lastSoundPlayedType >= Audio::CHARSFX_BOWSER_ENTER && lastSoundPlayedType <= Audio::CHARSFX_BOWSER_HOWL_03)
            {
                BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->m_pCharacterSFX->Stop(lastSoundPlayedType, cGameSFX::SFX_STOP_FIRST);
            }
            else
            {
                if (gpLastSoundFromPlayer != NULL)
                {
                    ((cCharacter*)gpLastSoundFromPlayer)->StopSFX(lastSoundPlayedType);
                }
            }

            lastSoundPlayedType = Audio::CHARSFX_NONE;
            gpLastSoundFromPlayer = NULL;
        }

        newSound = charInGameDialogueTypes[randomElementIndex];
        lastSoundPlayedType = newSound;

        if (newSound < Audio::CHARSFX_BOWSER_ENTER && newSound != Audio::CHARSFX_EFFORTS_KICK_03)
        {
            if ((newSound >= Audio::CHARSFX_EFFORTS_DAZED && newSound <= Audio::CHARSFX_EFFORTS_STS_FLOAT_01) || (newSound >= Audio::CHARSFX_EFFORTS_PAIN_04 && newSound <= Audio::CHARSFX_EFFORTS_ELECTROCUTE_01))
            {
                gpLastSoundFromPlayer = g_pTeams[0]->GetFielder(1);
            }
            else
            {
                gpLastSoundFromPlayer = g_pTeams[0]->GetGoalie();
            }

            Audio::SoundAttributes sndAtr;
            sndAtr.Init();
            sndAtr.SetSoundType(charInGameDialogueTypes[randomElementIndex], false);
            ((cCharacter*)gpLastSoundFromPlayer)->m_pCharacterSFX->Play(sndAtr);
        }
        else
        {
            if (newSound >= Audio::CHARSFX_BOWSER_ENTER && newSound <= Audio::CHARSFX_BOWSER_HOWL_03)
            {
                BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->PlaySFX(newSound, NONE, -1.0f, false);
            }
            else
            {
                gpLastSoundFromPlayer = g_pTeams[0]->GetGoalie();
                Audio::SoundAttributes sndAtr;
                sndAtr.Init();
                sndAtr.SetSoundType(charInGameDialogueTypes[randomElementIndex], false);
                ((cCharacter*)gpLastSoundFromPlayer)->m_pCharacterSFX->Play(sndAtr);
            }
        }
    }
    else
    {
        const char* szEvent = "sfx_accept_mario";

        if (AudioLoader::IsInited())
        {
            unsigned long hash = nlStringLowerHash(szEvent);
            if (AudioLoader::IsInited())
            {
                unsigned long stackHash = hash;
                AnimAudioEventLookup* result = nlBSearch<AnimAudioEventLookup, unsigned long>(stackHash, gp_AnimAudioEventTable, gNumAnimAudioEvents);
                AnimAudioEventLookup* event = result ? result : NULL;

                if (nlStrCmp<char>(event->szSFXType, "empty") != 0)
                {
                    Audio::StopWorldSFXbyStr(event->szSFXType);
                }
            }
        }

        unsigned long hash = nlStringLowerHash(szEvent);
        if (AudioLoader::IsInited())
        {
            if (mIsEnabled)
            {
                unsigned long stackHash = hash;
                AnimAudioEventLookup* result = nlBSearch<AnimAudioEventLookup, unsigned long>(stackHash, gp_AnimAudioEventTable, gNumAnimAudioEvents);
                AnimAudioEventLookup* event = result ? result : NULL;

                if (nlStrICmp<char>(event->szSFXType, "empty") != 0)
                {
                    Audio::PlayWorldSFXbyStr(event->szSFXType, 100.0f, -1.0f, false, true, NULL, NULL, NULL);
                }
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x284 | 0x8009F030 | size: 0xC
 */
void FEAudio::ResetRandomVoiceToggleSFX()
{
    gpLastSoundFromPlayer = nullptr;
}

/**
 * Offset/Address/Size: 0x8 | 0x8009EDB4 | size: 0x27C
 */
void FEAudioEventHandler(Event* pEvent, void* userData)
{
    if (!AudioLoader::IsInited())
    {
        return;
    }

    switch (pEvent->m_uEventID)
    {
    case 0x48:
        Audio::gWorldSFX.Play(Audio::WORLDSFX_PLACEHOLDER, 100.0f, -1.0f, false, 100.0f);
        break;
    case 0x49:
        Audio::gWorldSFX.Play(Audio::WORLDSFX_PLACEHOLDER, 100.0f, -1.0f, false, 100.0f);
        break;
    case 0x4A:
        Audio::gWorldSFX.Play(Audio::WORLDSFX_PLACEHOLDER, 100.0f, -1.0f, false, 100.0f);
        break;
    case 0x4B:
        Audio::gWorldSFX.Play(Audio::WORLDSFX_PLACEHOLDER, 100.0f, -1.0f, false, 100.0f);
        break;
    case 0x4C:
        Audio::gWorldSFX.Play(Audio::WORLDSFX_FE_BUTTON_GEN_SELECT_ACCEPT, 100.0f, -1.0f, false, 100.0f);
        break;
    case 0x4D:
        Audio::gWorldSFX.Play(Audio::WORLDSFX_FE_BUTTON_GEN_SELECT_BACK, 100.0f, -1.0f, false, 100.0f);
        break;
    case 0x4F:
        AudioLoader::PlayPauseMenuMusic();
        break;
    case 0x50:
        Audio::gWorldSFX.Play(Audio::WORLDSFX_FE_SCREEN_GEN_BEGIN, 100.0f, -1.0f, false, 100.0f);
        break;
    case 0x51:
        Audio::gWorldSFX.Play(Audio::WORLDSFX_FE_SCREEN_GEN_END, 100.0f, -1.0f, false, 100.0f);
        break;
    case 0x52:
        Audio::gWorldSFX.Play(Audio::WORLDSFX_FE_DENY, 100.0f, -1.0f, false, 100.0f);
        break;
    case 0x53:
    case 0x54:
    {
        FrontEndAnimAudioData* data;
        pEvent->GetData(&data);

        unsigned long stackHash = data->audioIdentifier;
        AnimAudioEventLookup* result = nlBSearch<AnimAudioEventLookup, unsigned long>(stackHash, gp_AnimAudioEventTable, gNumAnimAudioEvents);
        AnimAudioEventLookup* event;
        if (result)
        {
            event = result;
        }
        else
        {
            event = NULL;
        }

        if (AudioLoader::IsInited() && mIsEnabled)
        {
            Audio::PlayWorldSFXbyStr(event->szSFXType, 100.0f, -1.0f, true, true, NULL, NULL, NULL);
        }

        break;
    }
    default:
        break;
    }
}
/**
 * Offset/Address/Size: 0x0 | 0x8009EDAC | size: 0x8
 */
void FEAudio::EnableSounds(bool enable)
{
    mIsEnabled = enable;
}
