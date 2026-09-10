#include "Game/CharacterAudio.h"
#include "Game/Character.h"
#include "Game/Team.h"
#include "Game/Game.h"
#include "Game/GameInfo.h"
#include "Game/Render/Nis.h"
#include "Game/AI/Fielder.h"
#include "Game/FE/feHelpFuncs.h"

#include "NL/nlMath.h"
#include "NL/nlSingleton.h"

extern cTeam* g_pTeams[2];

namespace Audio
{

const char* gCharSoundTable[] = {
    "CHARSFX_NONE",
    "CHARSFX_RUN_01",
    "CHARSFX_RUN_02",
    "CHARSFX_RUN_03",
    "CHARSFX_RUN_04",
    "CHARSFX_RUN_05",
    "CHARSFX_WALK_01",
    "CHARSFX_WALK_02",
    "CHARSFX_WALK_03",
    "CHARSFX_WALK_04",
    "CHARSFX_WALK_05",
    "CHARSFX_LAND",
    "CHARSFX_SLIDE",
    "CHARSFX_JUMP",
    "CHARSFX_TURN",
    "CHARSFX_DEKE_LEFT",
    "CHARSFX_DEKE_RIGHT",
    "CHARSFX_BODYFALL",
    "CHARSFX_KICK_ATTEMPT",
    "CHARSFX_KICK_SUPER",
    "CHARSFX_SHOT_WINDUP",
    "CHARSFX_SHOT_LOCK",
    "CHARSFX_SHOT_RELEASE",
    "CHARSFX_S2S_METER_GLOW",
    "CHARSFX_HIT_BODY",
    "CHARSFX_HIT_BODY_BONE",
    "CHARSFX_HIT_SLIDE",
    "CHARSFX_BODYFALL_BONECRUNCH",
    "CHARSFX_SUPER_KICK_WINDUP",
    "CHARSFX_PWRUP_MUSH_IN_EFFECT",
    "CHARSFX_PWRUP_STAR_IN_EFFECT",
    "CHARSFX_SHELL_FREEZE_HIT",
    "CHARSFX_SHELL_FREEZE_END",
    "CHARSFX_EFFORTS_ATTACK_01",
    "CHARSFX_EFFORTS_ATTACK_02",
    "CHARSFX_EFFORTS_ATTACK_03",
    "CHARSFX_EFFORTS_HIT_01",
    "CHARSFX_EFFORTS_HIT_02",
    "CHARSFX_EFFORTS_HIT_03",
    "CHARSFX_EFFORTS_GET_HIT_01",
    "CHARSFX_EFFORTS_GET_HIT_02",
    "CHARSFX_EFFORTS_GET_HIT_03",
    "CHARSFX_EFFORTS_PAIN_01",
    "CHARSFX_EFFORTS_PAIN_02",
    "CHARSFX_EFFORTS_PAIN_03",
    "CHARSFX_EFFORTS_ELECTROCUTE_01",
    "CHARSFX_EFFORTS_ELECTROCUTE_02",
    "CHARSFX_EFFORTS_ELECTROCUTE_03",
    "CHARSFX_EFFORTS_EXERT_01",
    "CHARSFX_EFFORTS_EXERT_02",
    "CHARSFX_EFFORTS_EXERT_03",
    "CHARSFX_EFFORTS_KICK_01",
    "CHARSFX_EFFORTS_KICK_02",
    "CHARSFX_EFFORTS_KICK_03",
    "CHARSFX_EFFORTS_DAZED",
    "CHARSFX_EFFORTS_HEAD_SHAKE",
    "CHARSFX_EFFORTS_KICK_SUPER",
    "CHARSFX_EFFORTS_SHOT_WINDUP",
    "CHARSFX_EFFORTS_STS_JUMP_01",
    "CHARSFX_EFFORTS_STS_FLOAT_01",
    "CHARSFX_EFFORTS_STS_KICK_01",
    "CHARSFX_EFFORTS_PERFECT_PASS",
    "CHARSFX_BREATH_WITH_BALL",
    "CHARSFX_CALL_HEY_01",
    "CHARSFX_CALL_HEY_02",
    "CHARSFX_CALL_WO_01",
    "CHARSFX_CALL_WO_02",
    "CHARSFX_CALL_PERFECT_PASS",
    "CHARSFX_DAZED",
    "CHARSFX_HEAD_POP",
    "CHARSFX_ELECTROCUTED",
    "CHARSFX_THROW",
    "CHARSFX_SWISH_UP_01",
    "CHARSFX_SWISH_UP_02",
    "CHARSFX_SWISH_UP_03",
    "CHARSFX_SWISH_DOWN_01",
    "CHARSFX_SWISH_DOWN_02",
    "CHARSFX_SWISH_DOWN_03",
    "CHARSFX_GEN_STOS_JUMP",
    "CHARSFX_GEN_STOS_FLOAT",
    "CHARSFX_GEN_STOS_FLOAT_HYPER",
    "CHARSFX_NIS_CLAP_01",
    "CHARSFX_NIS_CLAP_02",
    "CHARSFX_NIS_CLAP_03",
    "CHARSFX_NIS_SPIN_LOOP",
    "CHARSFX_BOWSER_ENTER",
    "CHARSFX_BOWSER_ACTIVATE",
    "CHARSFX_BOWSER_HOWL_01",
    "CHARSFX_BOWSER_HOWL_02",
    "CHARSFX_BOWSER_HOWL_03",
    "CHARSFX_BOWSER_CHARGE_01",
    "CHARSFX_BOWSER_CHARGE_02",
    "CHARSFX_BOWSER_BREATH_FIRE",
    "CHARSFX_CRITTER_HIT_NET",
    "CHARSFX_CRITTER_EFFORTS_CATCH_MOUTH",
    "CHARSFX_CRITTER_EFFORTS_FEAR",
    "CHARSFX_CRITTER_EFFORTS_PUZZLED",
    "CHARSFX_CRITTER_EFFORTS_ANGER",
    "CHARSFX_CRITTER_HIT_HYPER",
    "CHARSFX_SUPER_LEFT_LEG_SERVO",
    "CHARSFX_SUPER_RIGHT_LEG_SERVO",
    "CHARSFX_TOAD_NIS_GOAL_HIGH_01",
    "CHARSFX_TOAD_NIS_GOAL_LOW_01",
    "CHARSFX_HAM_NIS_GOAL_HIGH_01",
    "CHARSFX_HAM_NIS_GOAL_LOW_01",
    "CHARSFX_KOOPA_NIS_GOAL_HIGH_01",
    "CHARSFX_KOOPA_NIS_GOAL_LOW_01",
    "CHARSFX_BIRDO_NIS_GOAL_HIGH_01",
    "CHARSFX_BIRDO_NIS_GOAL_LOW_01",
    "CHARSFX_DAISY_NIS_ENTER_HOME_01",
    "CHARSFX_DAISY_NIS_ENTER_AWAY_01",
    "CHARSFX_DAISY_NIS_ATTITUDE_01",
    "CHARSFX_DAISY_NIS_GOAL_HIGH_01",
    "CHARSFX_DAISY_NIS_GOAL_LOW_01",
    "CHARSFX_DAISY_NIS_GAME_WIN_01",
    "CHARSFX_DK_WIN_HOME_LOW",
    "CHARSFX_DK_END_GAME_HOME",
    "CHARSFX_DK_ATTITUDE_HOME_CHEST",
    "CHARSFX_DK_NIS_ENTER_HOME_01",
    "CHARSFX_DK_NIS_ENTER_AWAY_01",
    "CHARSFX_DK_NIS_ATTITUDE_01",
    "CHARSFX_DK_NIS_GOAL_HIGH_00",
    "CHARSFX_DK_NIS_GOAL_HIGH_01",
    "CHARSFX_DK_NIS_GOAL_LOW_01",
    "CHARSFX_DK_NIS_GAME_WIN_01",
    "CHARSFX_LUIGI_NIS_ENTER_HOME_01",
    "CHARSFX_LUIGI_NIS_ENTER_AWAY_01",
    "CHARSFX_LUIGI_NIS_ATTITUDE_01",
    "CHARSFX_LUIGI_NIS_GOAL_HIGH_01",
    "CHARSFX_LUIGI_NIS_GOAL_LOW_01",
    "CHARSFX_LUIGI_NIS_GAME_WIN_01",
    "CHARSFX_MARIO_NIS_ENTER_HOME_01",
    "CHARSFX_MARIO_NIS_ENTER_AWAY_01",
    "CHARSFX_MARIO_NIS_ATTITUDE_01",
    "CHARSFX_MARIO_NIS_ATTITUDE_02",
    "CHARSFX_MARIO_NIS_GOAL_HIGH_01",
    "CHARSFX_MARIO_NIS_GOAL_HIGH_02",
    "CHARSFX_MARIO_NIS_GOAL_LOW_01",
    "CHARSFX_MARIO_NIS_GAME_WIN_01",
    "CHARSFX_MARIO_NIS_GAME_WIN_02",
    "CHARSFX_PEACH_NIS_ENTER_HOME_01",
    "CHARSFX_PEACH_NIS_ENTER_AWAY_01",
    "CHARSFX_PEACH_NIS_ATTITUDE_01",
    "CHARSFX_PEACH_NIS_GOAL_HIGH_01",
    "CHARSFX_PEACH_NIS_GOAL_LOW_01",
    "CHARSFX_PEACH_NIS_GAME_WIN_01",
    "CHARSFX_WALUIGI_NIS_ENTER_HOME_01",
    "CHARSFX_WALUIGI_NIS_ENTER_AWAY_01",
    "CHARSFX_WALUIGI_NIS_ATTITUDE_01",
    "CHARSFX_WALUIGI_NIS_GOAL_HIGH_00",
    "CHARSFX_WALUIGI_NIS_GOAL_HIGH_01",
    "CHARSFX_WALUIGI_NIS_GOAL_LOW_01",
    "CHARSFX_WALUIGI_NIS_GAME_WIN_01",
    "CHARSFX_WARIO_NIS_ENTER_HOME_01",
    "CHARSFX_WARIO_NIS_ENTER_AWAY_01",
    "CHARSFX_WARIO_NIS_ATTITUDE_01",
    "CHARSFX_WARIO_NIS_GOAL_HIGH_01",
    "CHARSFX_WARIO_NIS_GOAL_LOW_01",
    "CHARSFX_WARIO_NIS_GAME_WIN_01",
    "CHARSFX_YOSHI_NIS_ENTER_HOME_01",
    "CHARSFX_YOSHI_NIS_ENTER_AWAY_01",
    "CHARSFX_YOSHI_NIS_ATTITUDE_01",
    "CHARSFX_YOSHI_NIS_GOAL_HIGH_01",
    "CHARSFX_YOSHI_NIS_GOAL_LOW_01",
    "CHARSFX_YOSHI_NIS_GAME_WIN_01",
    "CHARSFX_SUPER_NIS_ENTER_HOME_01",
    "CHARSFX_SUPER_NIS_ENTER_AWAY_01",
    "CHARSFX_SUPER_NIS_ATTITUDE_01",
    "CHARSFX_SUPER_NIS_GOAL_HIGH_01",
    "CHARSFX_SUPER_NIS_GOAL_LOW_01",
    "CHARSFX_SUPER_NIS_GAME_WIN_01",
    "CHARSFX_MYST_NIS_ENERGY_BLAST_01",
    "CHARSFX_MYST_NIS_ROCKET_BOOTS_01",
    "NUM_CHARSFX"
};

} // namespace Audio

static Audio::eCharSFX charDialogueSFX[29] = {
    Audio::CHARSFX_EFFORTS_ATTACK_01,      // 0x21 = 33
    Audio::CHARSFX_EFFORTS_ATTACK_02,      // 0x22 = 34
    Audio::CHARSFX_EFFORTS_ATTACK_03,      // 0x23 = 35
    Audio::CHARSFX_EFFORTS_GET_HIT_01,     // 0x27 = 39
    Audio::CHARSFX_EFFORTS_GET_HIT_02,     // 0x28 = 40
    Audio::CHARSFX_EFFORTS_GET_HIT_03,     // 0x29 = 41
    Audio::CHARSFX_EFFORTS_HIT_01,         // 0x24 = 36
    Audio::CHARSFX_EFFORTS_HIT_02,         // 0x25 = 37
    Audio::CHARSFX_EFFORTS_HIT_03,         // 0x26 = 38
    Audio::CHARSFX_EFFORTS_PAIN_01,        // 0x2A = 42
    Audio::CHARSFX_EFFORTS_PAIN_02,        // 0x2B = 43
    Audio::CHARSFX_EFFORTS_PAIN_03,        // 0x2C = 44
    Audio::CHARSFX_EFFORTS_PAIN_04,        // 0x2D = 45
    Audio::CHARSFX_EFFORTS_PAIN_05,        // 0x2E = 46
    Audio::CHARSFX_EFFORTS_ELECTROCUTE_01, // 0x2F = 47
    Audio::CHARSFX_EFFORTS_PERFECT_PASS,   // 0x3F = 63
    Audio::CHARSFX_BREATH_WITH_BALL,       // 0x40 = 64
    Audio::CHARSFX_CALL_HEY_01,            // 0x41 = 65
    Audio::CHARSFX_CALL_HEY_02,            // 0x42 = 66
    Audio::CHARSFX_CALL_WO_01,             // 0x43 = 67
    Audio::CHARSFX_EFFORTS_ELECTROCUTE_02, // 0x30 = 48
    Audio::CHARSFX_EFFORTS_ELECTROCUTE_03, // 0x31 = 49
    Audio::CHARSFX_EFFORTS_EXERT_01,       // 0x32 = 50
    Audio::CHARSFX_EFFORTS_EXERT_02,       // 0x33 = 51
    Audio::CHARSFX_EFFORTS_EXERT_03,       // 0x34 = 52
    Audio::CHARSFX_EFFORTS_KICK_01,        // 0x35 = 53
    Audio::CHARSFX_GEN_STOS_FLOAT,         // 0x51 = 81
    Audio::CHARSFX_GEN_STOS_FLOAT_HYPER,   // 0x52 = 82
    Audio::CHARSFX_NIS_CLAP_01,            // 0x53 = 83
};

static CharDialogueSFXInfo charDialogueSFXInfo[11] = {
    { 0x00, 0x03 }, // 0, 3
    { 0x03, 0x03 }, // 3, 3
    { 0x06, 0x03 }, // 6, 3
    { 0x09, 0x03 }, // 9, 3
    { 0x0C, 0x03 }, // 12, 3
    { 0x0F, 0x05 }, // 15, 5
    { 0x14, 0x03 }, // 20, 3
    { 0x17, 0x03 }, // 23, 3
    { 0x1A, 0x03 }, // 26, 3
    { 0x1A, 0x00 }, // 26, 0
    { 0x1A, 0x00 }, // 26, 0
};

static Audio::eCharSFX charFootstepSFX[2][5] = {
    {
        Audio::CHARSFX_RUN_01,
        Audio::CHARSFX_RUN_02,
        Audio::CHARSFX_RUN_03,
        Audio::CHARSFX_RUN_04,
        Audio::CHARSFX_RUN_05,
    },
    {
        Audio::CHARSFX_WALK_01,
        Audio::CHARSFX_WALK_02,
        Audio::CHARSFX_WALK_03,
        Audio::CHARSFX_WALK_04,
        Audio::CHARSFX_WALK_05,
    },
};

namespace Audio
{

static inline bool isElectrocuteSFXPlaying(Audio::cCharacterSFX* self)
{
    for (s32 i = charDialogueSFXInfo[CHAR_DIALOGUE_ELECTROCUTE].charDialogueSFXIndex;
        i < charDialogueSFXInfo[CHAR_DIALOGUE_ELECTROCUTE].charDialogueSFXIndex + charDialogueSFXInfo[CHAR_DIALOGUE_ELECTROCUTE].numRandomSFX;
        i++)
    {
        if (self->IsKeepingTrackOf(charDialogueSFX[i], NULL))
            return true;
    }
    return false;
}

static inline void ClearFootstepFlags(cCharacterSFX* self, eCharSFX* pSFX, eCharSFX* pLimitSFX)
{
    for (int i = pSFX[0]; i < (int)pLimitSFX[2]; i++)
    {
        self->mCharSFX[i].m_unk_0x40 = false;
    }
}

inline void cCharacterSFX::StopPlayingAllCharDialogue()
{
    s32 limit = charDialogueSFXInfo[6].numRandomSFX + 0x14;
    s32 i = 0;
    for (; i < limit; i++)
    {
        if (IsKeepingTrackOf(charDialogueSFX[i], NULL))
        {
            Stop(charDialogueSFX[i], cGameSFX::SFX_STOP_FIRST);
        }
    }
}

/**
 * Offset/Address/Size: 0x1310 | 0x8014D6B4 | size: 0x60
 */
cCharacterSFX::cCharacterSFX()
{
    mGroup = 0;
    mpPhysObj = NULL;
    mpMovementLoopSound = NULL;
    m_unk_0x339C = 100.0f;
    m_unk_0x33A0 = false;
    meClassType = CHAR;
}

/**
 * Offset/Address/Size: 0x1294 | 0x8014D638 | size: 0x7C
 */
cCharacterSFX::~cCharacterSFX()
{
    mpPhysObj = NULL;
    mGroup = 0;
    mpMovementLoopSound = NULL;
    m_unk_0x339C = 100.0f;
    m_unk_0x33A0 = false;
}

/**
 * Offset/Address/Size: 0x11C8 | 0x8014D56C | size: 0xCC
 */
void Audio::cCharacterSFX::Init()
{
    if (mbInited != 0)
    {
        return;
    }

    for (int i = 0; i < 173; i++)
    {
        mCharSFX[i].typeID = -1;
        mCharSFX[i].typeStr = nullptr;
        mCharSFX[i].musyxStr = nullptr;
        mCharSFX[i].musyxID = -1;
        mCharSFX[i].fVolume = 100.0f;
        mCharSFX[i].fDelay = -1.0f;
        mCharSFX[i].fVolReverb = 100.0f;
        mCharSFX[i].volGrp = -1;
        mCharSFX[i].sfxPriority = 0;
        mCharSFX[i].uHashVal = 0;
        mCharSFX[i].pSoundPropAccessor = nullptr;
        mCharSFX[i].bSoundPropTableReloaded = 0;
        mCharSFX[i].pSoundProp = nullptr;
        mCharSFX[i].pOwner = nullptr;
        mCharSFX[i].lastVoiceID = -1;
        mCharSFX[i].pLastEmitter = nullptr;
        mCharSFX[i].m_unk_0x40 = 0;

        mSoundTypeEnumMap[i] = -1;

        mDebugTimer[i] = 0.0f;
    }

    mpSFX = mCharSFX;
    mpSoundStrTable = Audio::gCharSoundTable;
    mNumSFXTypes = 173;
    meClassType = CHAR;

    cGameSFX::Init();
}

unsigned long cCharacterSFX::Play(eCharSFX sfxType, float fVolume, float fPan, float fAttenuate)
{
    SoundAttributes attrs;
    attrs.Init();
    attrs.mu_Type = sfxType;
    attrs.mf_Volume = fVolume;
    attrs.mf_Panning = fPan;
    attrs.mf_Attenuate = fAttenuate;
    return Play(attrs);
}

unsigned long cCharacterSFX::Play3D(eCharSFX sfxType, PosUpdateMethod posUpdateMethod, float fMaxVolume, float fAttenuate)
{
    SoundAttributes attrs;
    attrs.Init();
    attrs.mu_Type = sfxType;
    attrs.mb_Is3D = true;
    attrs.mf_Volume = fMaxVolume;
    attrs.mf_Attenuate = fAttenuate;
    attrs.mf_Panning = 0.0f;
    attrs.posUpdateMethod = posUpdateMethod;
    return Play(attrs);
}

/**
 * Offset/Address/Size: 0x114C | 0x8014D4F0 | size: 0x7C
 */
uintptr_t cCharacterSFX::Play(SoundAttributes& attrs)
{
    attrs.me_ClassType = 1;
    attrs.mi_EmitterGroup = mGroup;

    if (attrs.posUpdateMethod == PHYSOBJ)
    {
        attrs.UsePhysObj(mpPhysObj);
    }

    if (attrs.mu_Type == 0U)
    {
        return Audio::GetSndIDError();
    }

    return cGameSFX::Play(attrs);
}

/**
 * Offset/Address/Size: 0x112C | 0x8014D4D0 | size: 0x20
 */
void cCharacterSFX::Stop(eCharSFX sfxType, cGameSFX::StopFlag flag)
{
    cGameSFX::Stop(sfxType, flag);
}

/**
 * Offset/Address/Size: 0xC5C | 0x8014D000 | size: 0x4D0
 */
unsigned long cCharacterSFX::PlayRandomCharDialogue(CharDialogueType dType, PosUpdateMethod posUpdateMethod, float fVol, float fDelay, bool bAvoidCurrent)
{
    Audio::SoundAttributes sfxAtr;

    if (mbInited == 0)
    {
        return -1;
    }

    if (dType == CHAR_FOOTSTEP_WALK)
    {
        if (mbInited == 0)
            return -1;
        if (mbInited == 0)
            return -1;

        Audio::SoundAttributes walkAtr;
        walkAtr.Init();
        walkAtr.me_ClassType = 1;

        s32 randomIndex = nlRandom(5, &nlDefaultSeed);
        Audio::eCharSFX* walkSFX = charFootstepSFX[1];
        Audio::eCharSFX sfxType = walkSFX[randomIndex];

        if (mCharSFX[sfxType].m_unk_0x40 != 0)
        {
            sfxType = walkSFX[(randomIndex + 1) % 5];
        }

        ClearFootstepFlags(this, walkSFX, charFootstepSFX[1]);

        mCharSFX[sfxType].m_unk_0x40 = true;
        walkAtr.SetSoundType(sfxType, true);
        walkAtr.UseStationaryPosVector(mpPhysObj->GetPosition());
        return Play(walkAtr);
    }
    else if (dType == CHAR_FOOTSTEP_RUN)
    {
        if (mbInited == 0)
            return -1;
        if (mbInited == 0)
            return -1;

        Audio::SoundAttributes runAtr;
        runAtr.Init();
        runAtr.me_ClassType = 1;

        s32 randomIndex = nlRandom(5, &nlDefaultSeed);
        Audio::eCharSFX* runSFX = charFootstepSFX[0];
        Audio::eCharSFX sfxType = runSFX[randomIndex];

        if (mCharSFX[sfxType].m_unk_0x40 != 0)
        {
            sfxType = runSFX[(randomIndex + 1) % 5];
        }

        ClearFootstepFlags(this, runSFX, charFootstepSFX[0]);

        mCharSFX[sfxType].m_unk_0x40 = true;
        runAtr.SetSoundType(sfxType, true);
        runAtr.UseStationaryPosVector(mpPhysObj->GetPosition());
        return Play(runAtr);
    }

    if (dType != CHAR_DIALOGUE_ELECTROCUTE)
    {
        if (isElectrocuteSFXPlaying(this))
        {
            return -1;
        }
    }

    s32 i = 0;
    s32 limit = charDialogueSFXInfo[6].numRandomSFX + 0x14;
    for (; i < limit; i++)
    {
        if (IsKeepingTrackOf(charDialogueSFX[i], NULL))
        {
            cGameSFX::Stop(charDialogueSFX[i], cGameSFX::SFX_STOP_FIRST);
        }
    }

    if (mpMovementLoopSound != NULL)
    {
        StopEmitter(mpMovementLoopSound, 0);
        mpMovementLoopSound = NULL;
        m_unk_0x33A0 = false;
    }

    sfxAtr.Init();
    sfxAtr.me_ClassType = 1;

    s32 baseIndex = charDialogueSFXInfo[dType].charDialogueSFXIndex;
    s32 numRandom = charDialogueSFXInfo[dType].numRandomSFX;
    s32 randomOffset = nlRandom(numRandom, &nlDefaultSeed);

    Audio::eCharSFX sfxType = charDialogueSFX[baseIndex + randomOffset];

    if (bAvoidCurrent)
    {
        if (mCharSFX[sfxType].m_unk_0x40 != 0)
        {
            sfxType = charDialogueSFX[baseIndex + (randomOffset + 1) % numRandom];
        }
    }

    for (int i = charDialogueSFX[baseIndex]; i < (int)charDialogueSFX[baseIndex + numRandom]; i++)
    {
        mCharSFX[i].m_unk_0x40 = false;
    }

    mCharSFX[sfxType].m_unk_0x40 = true;
    sfxAtr.SetSoundType(sfxType, true);

    sfxAtr.mf_Volume = fVol;
    if (fVol != 100.0f)
    {
        SoundStrToIDNode* pSfxNode = &mpSFX[sfxAtr.mu_Type];
        sfxAtr.mf_Volume = sfxAtr.mf_Volume * pSfxNode->fVolume;
    }

    sfxAtr.mf_DelayTime = fDelay;
    sfxAtr.posUpdateMethod = posUpdateMethod;

    if (posUpdateMethod == VECTORS)
    {
        sfxAtr.UseStationaryPosVector(mpPhysObj->GetPosition());
    }

    return Play(sfxAtr);
}

/**
 * Offset/Address/Size: 0x7C4 | 0x8014CB68 | size: 0x498
 */
uintptr_t cCharacterSFX::PlayRandomCharDialogue(CharDialogueType dType, Audio::SoundAttributes& sndAttributes, bool bAvoidCurrent, unsigned long* pOutSfx)
{
    if (mbInited == 0)
    {
        return -1;
    }

    if (dType == CHAR_FOOTSTEP_WALK)
    {
        if (mbInited == 0)
            return -1;
        if (mbInited == 0)
            return -1;

        sndAttributes.me_ClassType = 1;
        s32 randomIndex = nlRandom(5, &nlDefaultSeed);
        eCharSFX* pSFX = charFootstepSFX[1];
        eCharSFX sfxType = pSFX[randomIndex];

        if (mCharSFX[sfxType].m_unk_0x40 != 0)
        {
            sfxType = pSFX[(randomIndex + 1) % 5];
        }

        ClearFootstepFlags(this, pSFX, charFootstepSFX[1]);

        mCharSFX[sfxType].m_unk_0x40 = true;
        sndAttributes.SetSoundType(sfxType, true);

        if (sndAttributes.posUpdateMethod == NONE)
        {
            sndAttributes.UseStationaryPosVector(mpPhysObj->GetPosition());
        }

        return Play(sndAttributes);
    }
    else if (dType == CHAR_FOOTSTEP_RUN)
    {
        if (mbInited == 0)
            return -1;
        if (mbInited == 0)
            return -1;

        sndAttributes.me_ClassType = 1;
        s32 randomIndex = nlRandom(5, &nlDefaultSeed);
        eCharSFX* pSFX = charFootstepSFX[0];
        eCharSFX sfxType = pSFX[randomIndex];

        if (mCharSFX[sfxType].m_unk_0x40 != 0)
        {
            sfxType = pSFX[(randomIndex + 1) % 5];
        }

        ClearFootstepFlags(this, pSFX, charFootstepSFX[0]);

        mCharSFX[sfxType].m_unk_0x40 = true;
        sndAttributes.SetSoundType(sfxType, true);

        if (sndAttributes.posUpdateMethod == NONE)
        {
            sndAttributes.UseStationaryPosVector(mpPhysObj->GetPosition());
        }

        return Play(sndAttributes);
    }

    if (dType != CHAR_DIALOGUE_ELECTROCUTE)
    {
        if (isElectrocuteSFXPlaying(this))
        {
            return -1;
        }
    }

    StopPlayingAllCharDialogue();

    if (mpMovementLoopSound != NULL)
    {
        StopEmitter(mpMovementLoopSound, 0);
        mpMovementLoopSound = NULL;
        m_unk_0x33A0 = false;
    }

    s32 baseIndex = charDialogueSFXInfo[dType].charDialogueSFXIndex;
    s32 numRandom = charDialogueSFXInfo[dType].numRandomSFX;
    s32 randomIndex = nlRandom(numRandom, &nlDefaultSeed);
    eCharSFX sfxType = charDialogueSFX[baseIndex + randomIndex];

    if (bAvoidCurrent)
    {
        if (mCharSFX[sfxType].m_unk_0x40 != 0)
        {
            randomIndex = (randomIndex + 1) % numRandom;
            sfxType = charDialogueSFX[baseIndex + randomIndex];
        }
    }

    for (int i = charDialogueSFX[baseIndex]; i < (int)charDialogueSFX[baseIndex + numRandom]; i++)
    {
        mCharSFX[i].m_unk_0x40 = false;
    }

    mCharSFX[sfxType].m_unk_0x40 = true;
    sndAttributes.SetSoundType(sfxType, sndAttributes.mb_Is3D);

    if (pOutSfx != NULL)
    {
        *pOutSfx = charDialogueSFX[charDialogueSFXInfo[dType].charDialogueSFXIndex + randomIndex];
    }

    if (sndAttributes.posUpdateMethod == VECTORS)
    {
        sndAttributes.UseStationaryPosVector(mpPhysObj->GetPosition());
    }

    return Play(sndAttributes);
}

/**
 * Offset/Address/Size: 0x730 | 0x8014CAD4 | size: 0x94
 */
bool cCharacterSFX::IsPlayingRandomCharDialogue(CharDialogueType dType)
{
    for (s32 i = charDialogueSFXInfo[dType].charDialogueSFXIndex; i < charDialogueSFXInfo[dType].charDialogueSFXIndex + charDialogueSFXInfo[dType].numRandomSFX; i++)
    {
        if (IsKeepingTrackOf(charDialogueSFX[i], nullptr))
        {
            return true;
        }
    }

    return false;
}

/**
 * Offset/Address/Size: 0x698 | 0x8014CA3C | size: 0x98
 */
void cCharacterSFX::StopPlayingRandomCharDialogue(CharDialogueType dType)
{
    for (s32 i = charDialogueSFXInfo[dType].charDialogueSFXIndex; i < charDialogueSFXInfo[dType].charDialogueSFXIndex + charDialogueSFXInfo[dType].numRandomSFX; i++)
    {
        if (IsKeepingTrackOf(charDialogueSFX[i], nullptr))
        {
            cGameSFX::Stop(charDialogueSFX[i], cGameSFX::SFX_STOP_FIRST);
        }
    }
}

/**
 * Offset/Address/Size: 0x600 | 0x8014C9A4 | size: 0x98
 */
void cCharacterSFX::StopPlayingAllRandomCharDialogue()
{
    s32 limit = charDialogueSFXInfo[6].numRandomSFX + 0x14;
    for (s32 i = 0; i < limit; i++)
    {
        if (IsKeepingTrackOf(charDialogueSFX[i], nullptr))
        {
            cGameSFX::Stop(charDialogueSFX[i], cGameSFX::SFX_STOP_FIRST);
        }
    }
}

/**
 * Offset/Address/Size: 0x4B8 | 0x8014C85C | size: 0x148
 */
int cCharacterSFX::PlayRandomWalkFootstep(float fVol, bool bNoRepeat)
{
    if (mbInited == 0)
    {
        return -1;
    }

    // Duplicate check (dead code)
    if (mbInited == 0)
    {
        return -1;
    }

    Audio::SoundAttributes attrs;
    attrs.Init();
    attrs.me_ClassType = 1;

    s32 randomIndex = nlRandom(5, &nlDefaultSeed);
    Audio::eCharSFX* walkSFX = charFootstepSFX[1];
    Audio::eCharSFX sfxType = walkSFX[randomIndex];

    if (bNoRepeat != 0)
    {
        if (mCharSFX[sfxType].m_unk_0x40 != 0)
        {
            sfxType = walkSFX[(randomIndex + 1) % 5];
        }
    }

    ClearFootstepFlags(this, walkSFX, charFootstepSFX[1]);

    mCharSFX[sfxType].m_unk_0x40 = true;

    attrs.SetSoundType(sfxType, true);
    attrs.UseStationaryPosVector(mpPhysObj->GetPosition());

    return Play(attrs);
}

/**
 * Offset/Address/Size: 0x2F0 | 0x8014C694 | size: 0x1C8
 */
uintptr_t cCharacterSFX::PlayNISRandomCharDialogue(CharDialogueType dialogueType, NisCharacterClass charIdentifier, float fVol, float fDelay, bool bIs3D, const nlVector3* pInitialPosVector, const nlVector3* pInitialDirVector, unsigned long* unkPtr)
{
    cCharacter* pChar;
    CharDialogueType dType = dialogueType;
    NisCharacterClass charId = charIdentifier;
    bool is3D = bIs3D;
    const nlVector3* initialPos = pInitialPosVector;
    const nlVector3* initialDir = pInitialDirVector;
    unsigned long* pType = unkPtr;

    if (!Audio::IsInited())
    {
        return -1;
    }

    if (g_pGame == NULL)
    {
        return -1;
    }

    pChar = GetCharacterFromNisCharClass(charId);

    if (pChar == NULL)
    {
        return -1;
    }

    Audio::SoundAttributes sndAtr;
    sndAtr.Init();
    sndAtr.mf_Volume = fVol;
    sndAtr.mf_DelayTime = fDelay;
    sndAtr.mb_KeepTrack = false;

    if (is3D)
    {
        sndAtr.mb_Is3D = is3D;
        sndAtr.UseVectorPtrs(initialPos, initialDir);
        sndAtr.mf_ReturnEmitterOnPlay = true;
    }

    return pChar->m_pCharacterSFX->PlayRandomCharDialogue(dType, sndAtr, !!pType, NULL);
}

/**
 * Offset/Address/Size: 0x1E4 | 0x8014C588 | size: 0x10C
 */
cCharacter* cCharacterSFX::GetCharacterFromNisCharClass(NisCharacterClass charIdentifier)
{
    cCharacter* pChar = NULL;

    switch (charIdentifier)
    {
    case NIS_CHAR_CLASS_DAISY:
    case NIS_CHAR_CLASS_DONKEYKONG:
    case NIS_CHAR_CLASS_LUIGI:
    case NIS_CHAR_CLASS_MARIO:
    case NIS_CHAR_CLASS_PEACH:
    case NIS_CHAR_CLASS_WALUIGI:
    case NIS_CHAR_CLASS_WARIO:
    case NIS_CHAR_CLASS_YOSHI:
    case NIS_CHAR_CLASS_MYSTERY:
        for (int team = 0; team < 2; team++)
        {
            cTeam* pTeam = g_pTeams[team];
            if ((u32)pTeam->GetCaptain()->m_eCharacterClass == (u32)charIdentifier)
            {
                pChar = pTeam->GetCaptain();
                break;
            }
        }
        break;
    case NIS_CHAR_CLASS_BIRDO:
    case NIS_CHAR_CLASS_HAMMERBROS:
    case NIS_CHAR_CLASS_KOOPA:
    case NIS_CHAR_CLASS_TOAD:
        for (int team = 0; team < 2; team++)
        {
            if ((u32)ConvertToCharacterClass(GameInfoManager::Instance()->GetSidekick((short)team)) == (u32)charIdentifier)
            {
                pChar = g_pTeams[team]->GetFielder(1);
                break;
            }
        }
        break;
    case NIS_CHAR_CLASS_HOME_GOALIE:
    case NIS_CHAR_CLASS_AWAY_GOALIE:
    {
        int team = 0;
        if (charIdentifier == NIS_CHAR_CLASS_AWAY_GOALIE)
        {
            team = 1;
        }
        pChar = g_pTeams[team]->GetGoalie();
        break;
    }
    }

    return pChar;
}

/**
 * Offset/Address/Size: 0x80 | 0x8014C424 | size: 0x164
 */
void cCharacterSFX::StartMovementLoop()
{
    if (!mbInited)
        return;

    if (IsPlayingRandomCharDialogue(CHAR_DIALOGUE_ELECTROCUTE))
        return;

    StopPlayingAllRandomCharDialogue();

    // Setup sound attributes for movement loop
    Audio::SoundAttributes sndAtr;
    sndAtr.Init();
    sndAtr.me_ClassType = 1;
    sndAtr.SetSoundType(0x3E, true);
    sndAtr.UsePhysObj(mpPhysObj);
    sndAtr.mf_ReturnEmitterOnPlay = true;
    sndAtr.mb_NoPhasingFilter = false;

    mpMovementLoopSound = (SFXEmitter*)Play(sndAtr);
}

/**
 * Offset/Address/Size: 0x38 | 0x8014C3DC | size: 0x48
 */
void cCharacterSFX::StopMovementLoop()
{
    if (mpMovementLoopSound)
    {
        cGameSFX::StopEmitter(mpMovementLoopSound, 0);
        mpMovementLoopSound = nullptr;
        m_unk_0x33A0 = false;
    }
}

/**
 * Offset/Address/Size: 0x14 | 0x8014C3B8 | size: 0x24
 */
bool cCharacterSFX::IsMovementLoopPlaying()
{
    return Audio::IsEmitterActive(mpMovementLoopSound);
}

/**
 * Offset/Address/Size: 0x0 | 0x8014C3A4 | size: 0x14
 */
bool cCharacterSFX::IsMovementLoopStarted()
{
    return mpMovementLoopSound != NULL;
}

} // namespace Audio
