#include "Game/Audio/CrowdMood.h"
#include "Game/Audio/WorldAudio.h"
#include "Game/AI/AiUtil.h"
#include "Game/Game.h"
#include "Game/Sys/GCStream.h"

#include "NL/nlConfig.h"
#include "NL/nlFileGC.h"
#include "NL/nlMath.h"
#include "NL/nlString.h"

#include "Game/Audio/AudioStreamVirtuals.h"

extern GCAudioStreaming::AudioBufferMgr g_BufferMgr;
static void ___blank(const char*, ...);
static void UpdateTiming(float);
extern "C" void sndStreamMixParameterEx(SND_STREAMID stid, unsigned char vol, unsigned char pan, unsigned char span, unsigned char auxa, unsigned char auxb);

struct SoundStrToIDNode;
class AudioLoader
{
public:
    static unsigned long GetSFXIDFromStr(const char*, SoundStrToIDNode**);
};

struct CROWD_SETTINGS
{
    /* 0x000 */ float MoodDecayDelay;
    /* 0x004 */ float MoodDecayRate;
    /* 0x008 */ float BlendSpeedNormal;
    /* 0x00C */ float BlendSpeedFast;
    /* 0x010 */ float BlendStrictness;
    /* 0x014 */ float CrowdMasterVolume;
    /* 0x018 */ char NeutralSampleName[256];
    /* 0x118 */ char NegativeSampleName[256];
    /* 0x218 */ char PositiveSampleName[256];
    /* 0x318 */ char SaturationSampleNames[5][256];
    /* 0x818 */ unsigned char NoStreaming : 1;
}; // total size: 0x81C

bool g_Initd;

template <typename T>
/**
 * Offset/Address/Size: 0x3C78 | 0x8015138C | size: 0x10
 */
void Increment(T& Value)
{
    Value = (T)(Value + 1);
}

static MOOD_DEFINITION g_MoodDefs[5] = { };
static CROWD_SETTINGS g_Settings = { };

CROWD_AUDIO_INIT g_CrowdAudio;
CROWD_STATE g_CrowdState;

char* MoodNames[5] = {
    "Positive",
    "Negative",
    "Bored",
    "Frustrated",
    "Neutral",
};

bool g_DoDecay = true;
bool g_CrowdSFXStopped = true;

struct RANDOM_STREAMS
{
    unsigned long Count;
    char Files[32][256];
};

static RANDOM_STREAMS g_RandomChants = { };
static RANDOM_STREAMS g_RandomHeckles = { };

template <int N>
/**
 * Offset/Address/Size: 0x3C10 | 0x80151324 | size: 0x68
 */
float NDimDistance(float* A, float* B)
{
    float sum = 0.0f;
    for (int i = 0; i < N; i++)
    {
        float diff = B[i] - A[i];
        sum += diff * diff;
    }
    return nlSqrt(sum, false);
}

template <typename T>
/**
 * Offset/Address/Size: 0x3ADC | 0x801511F0 | size: 0x134
 */
static void WarmRandomStream(const RANDOM_STREAMS& RandomStreams, T* pStream)
{
    if (g_Settings.NoStreaming)
    {
        return;
    }

    if (pStream->m_State == GCAudioStreaming::SS_Initd)
    {
        pStream->Purge();
    }

    unsigned long randomIndex = nlRandom(RandomStreams.Count, &nlDefaultSeed);
    const char* filename = RandomStreams.Files[randomIndex];

    pStream->Open(filename);
    pStream->Warm(true);
}

static void MoodDefFromBlend(float*, MOOD_DEFINITION&);

/**
 * Offset/Address/Size: 0x3958 | 0x8015106C | size: 0x50
 */
static void ___blank(const char*, ...)
{
}

/**
 * Offset/Address/Size: 0x3948 | 0x8015105C | size: 0x10
 */
unsigned char CrowdMood::IsStreamLocked()
{
    return g_CrowdState.StreamLocked;
}

/**
 * Offset/Address/Size: 0x3780 | 0x80150E94 | size: 0x1C8
 */
void ChangeCrowdVolume(float NewVolume)
{
    MOOD_DEFINITION MoodDef;
    g_CrowdState.CrowdVolume = NewVolume * g_Settings.CrowdMasterVolume;
    MoodDefFromBlend(g_CrowdState.CurrentMoodBlend, MoodDef);
    PlayMoodDef(MoodDef);

    float zero = 0.0f;
    if (fabsf(NewVolume - zero) <= 0.0001f)
    {
        GCAudioStreaming::StereoAudioStream* pChant = g_CrowdAudio.pChantStream;
        if (pChant != NULL)
        {
            if (pChant->m_State >= GCAudioStreaming::SS_Warming)
            {
                GCAudioStreaming::AudioStreamBuffer* buf;
                volatile unsigned long i = (uintptr_t)(buf = NULL);
                unsigned long start = 0;

                if (start < pChant->m_BufferCount)
                {
                    buf = pChant->m_Buffers[0];
                }

                while (buf != NULL)
                {
                    buf->m_Volume = 0;
                    sndStreamMixParameterEx(buf->m_StreamId, buf->m_Volume, buf->m_Pan, buf->m_SurroundPan, 0, 0);

                    unsigned long ci = i + 1;
                    i = ci;
                    if (ci < pChant->m_BufferCount)
                    {
                        buf = pChant->m_Buffers[ci];
                    }
                    else
                    {
                        buf = NULL;
                    }
                }
            }
            pChant->m_Volume = 0;
        }

        GCAudioStreaming::MonoAudioStream* pHeckle = g_CrowdAudio.pHeckleStream;
        if (pHeckle != NULL)
        {
            if (pHeckle->m_State >= GCAudioStreaming::SS_Warming)
            {
                GCAudioStreaming::AudioStreamBuffer* buf;
                volatile unsigned long i = (uintptr_t)(buf = NULL);
                unsigned long start = 0;

                if (start < pHeckle->m_BufferCount)
                {
                    buf = pHeckle->m_Buffers[0];
                }

                while (buf != NULL)
                {
                    buf->m_Volume = 0;
                    sndStreamMixParameterEx(buf->m_StreamId, buf->m_Volume, buf->m_Pan, buf->m_SurroundPan, 0, 0);

                    unsigned long ci = i + 1;
                    i = ci;
                    if (ci < pHeckle->m_BufferCount)
                    {
                        buf = pHeckle->m_Buffers[ci];
                    }
                    else
                    {
                        buf = NULL;
                    }
                }
            }
            pHeckle->m_Volume = 0;
        }
    }
}

static inline void FixValueRange(float& Value, float& Range)
{
    float temp = Range;
    temp = Value - temp;
    Value = temp;
    Range *= 2.0f;
}

static inline void ScaleAndAddVocalDef(CROWD_VOCAL_DEFINITION& Dest, const CROWD_VOCAL_DEFINITION& Src, float Scale)
{
    Dest.Volume += Src.Volume * Scale;
    Dest.VolumeRange += Src.VolumeRange * Scale;
    if (Src.Volume)
    {
        float delayAdd;
        if (Src.Delay)
        {
            delayAdd = (1.0f / Src.Delay) * Scale;
        }
        else
        {
            delayAdd = 1000000.0f;
        }
        Dest.Delay += delayAdd;
    }
    else
    {
        Dest.Delay += 0.000001;
    }
    Dest.DelayRange += Src.DelayRange * Scale;
}

/**
 * Offset/Address/Size: 0x31F8 | 0x8015090C | size: 0x588
 */
static void MoodDefFromBlend(float* MoodBlend, MOOD_DEFINITION& MoodDef)
{
    float Zero;
    float* pMaxBlend;
    float AccountedFor;
    CrowdMood::CROWD_MOOD mood;
    float zero = 0.0f;

    memset(&MoodDef, 0, sizeof(MOOD_DEFINITION));

    Zero = zero;
    pMaxBlend = &Zero;
    AccountedFor = zero;

    for (mood = (CrowdMood::CROWD_MOOD)0; (int)mood < 4; Increment(mood))
    {
        if (fabsf(MoodBlend[mood] - zero) <= 0.0001f)
        {
            continue;
        }

        float* pBlend;
        float blendVal = MoodBlend[mood];
        pBlend = blendVal > *pMaxBlend ? &MoodBlend[mood] : pMaxBlend;
        pMaxBlend = pBlend;

        AccountedFor += g_CrowdState.CurrentMoodBlend[mood];
        MoodDef.NeutralVol += g_MoodDefs[mood].NeutralVol * MoodBlend[mood];

        MoodDef.PositiveVol += g_MoodDefs[mood].PositiveVol * MoodBlend[mood];
        MoodDef.NegativeVol += g_MoodDefs[mood].NegativeVol * MoodBlend[mood];

        ScaleAndAddVocalDef(MoodDef.Chant, g_MoodDefs[mood].Chant, MoodBlend[mood]);
        ScaleAndAddVocalDef(MoodDef.Heckle, g_MoodDefs[mood].Heckle, MoodBlend[mood]);
    }

    float remainWeight = (1.0f - AccountedFor >= 0.0f) ? 1.0f - AccountedFor : 0.0f;

    const MOOD_DEFINITION& SatMoodDef = g_MoodDefs[CrowdMood::CM_Neutral];
    MoodDef.NeutralVol += SatMoodDef.NeutralVol * remainWeight;
    MoodDef.PositiveVol += SatMoodDef.PositiveVol * remainWeight;
    MoodDef.NegativeVol += SatMoodDef.NegativeVol * remainWeight;
    ScaleAndAddVocalDef(MoodDef.Chant, g_MoodDefs[CrowdMood::CM_Neutral].Chant, remainWeight);
    ScaleAndAddVocalDef(MoodDef.Heckle, g_MoodDefs[CrowdMood::CM_Neutral].Heckle, remainWeight);

    MoodDef.Chant.Delay = MoodDef.Chant.Delay ? (1.0f / MoodDef.Chant.Delay) : 0.0f;
    MoodDef.Heckle.Delay = MoodDef.Heckle.Delay ? (1.0f / MoodDef.Heckle.Delay) : 0.0f;

    if (*pMaxBlend < remainWeight)
    {
        pMaxBlend = MoodBlend + CrowdMood::CM_Neutral;
    }

    FixValueRange(MoodDef.Chant.Volume, MoodDef.Chant.VolumeRange);
    FixValueRange(MoodDef.Chant.Delay, MoodDef.Chant.DelayRange);
    FixValueRange(MoodDef.Heckle.Volume, MoodDef.Heckle.VolumeRange);
    FixValueRange(MoodDef.Heckle.Delay, MoodDef.Heckle.DelayRange);

    if (*pMaxBlend > 0.0f)
    {
        int dominantMood = (pMaxBlend - MoodBlend);
        const MOOD_DEFINITION& SatDef = g_MoodDefs[dominantMood];
        if (*pMaxBlend > SatDef.SaturationStart && SatDef.SaturationVolume > 0.0f)
        {
            float satFactor = (*pMaxBlend - SatDef.SaturationStart) / (1.0f - SatDef.SaturationStart);
            MoodDef.SaturationVolume = satFactor * SatDef.SaturationVolume;
            CROWD_AUDIO_INIT& CrowdAudio = g_CrowdAudio;
            const char* sampleName = g_Settings.SaturationSampleNames[dominantMood];
            if (CrowdAudio.CurrentSaturationSampleName != sampleName)
            {
                CrowdAudio.CurrentSaturationSampleName = sampleName;
                MoodDef.SaturationSFXId = AudioLoader::GetSFXIDFromStr(sampleName, 0);
            }
            else
            {
                MoodDef.SaturationSFXId = CrowdAudio.CurrentSaturationSFXId;
            }
        }
        else
        {
            MoodDef.SaturationVolume = 0.0f;
            g_CrowdAudio.CurrentSaturationSampleName = 0;
        }
    }
}

/**
 * Offset/Address/Size: 0x2EA0 | 0x801505B4 | size: 0x358
 */
static bool PlayVocal(const CROWD_VOCAL_DEFINITION& VocalDef, CROWD_STATE::VOCALIZATION_STATE& VocalState, GCAudioStreaming::AudioStream* pStream)
{
    if (g_Settings.NoStreaming)
    {
        return false;
    }

    float zero = 0.0f;
    if (fabsf(VocalDef.Volume - zero) <= 0.0001f)
    {
        return false;
    }

    if (VocalState.SinceLast > VocalDef.Delay)
    {
        bool playNow;
        float delayRange = VocalDef.DelayRange;
        if (delayRange != zero)
        {
            playNow = ((VocalState.SinceLast - VocalDef.Delay) / delayRange) > VocalState.NextAt;
        }
        else
        {
            playNow = VocalState.SinceLast > VocalDef.Delay;
        }

        if (playNow)
        {
            ___blank("Playing vocal\n");

            float randVol = nlRandomf(VocalDef.VolumeRange, &nlDefaultSeed);
            float scalar = 127.0f;
            float vocalVol = VocalDef.Volume;
            vocalVol = vocalVol + randVol;
            float crowdVol = g_CrowdState.CrowdVolume;
            crowdVol = crowdVol * vocalVol;
            scalar = scalar * crowdVol;
            int tmpVol = (int)scalar;
            pStream->SetVolume(tmpVol);
            pStream->SetLPF((unsigned short)g_CrowdState.LPFFreq);
            pStream->SetLoop(false);
            pStream->Play(true);

            VocalState.Ready = false;
            VocalState.SinceLast = -1.0f;
            VocalState.NextAt = nlRandomf(1.0f, &nlDefaultSeed);
            return true;
        }
    }

    return false;
}

/**
 * Offset/Address/Size: 0x2C7C | 0x80150390 | size: 0x224
 */
void PlayMoodDef(MOOD_DEFINITION& MoodDef)
{
    Audio::SetSFXVolume(g_CrowdAudio.NeutralVoiceId, MoodDef.NeutralVol * g_CrowdState.CrowdVolume);
    Audio::SetSFXVolume(g_CrowdAudio.PositiveVoiceId, MoodDef.PositiveVol * g_CrowdState.CrowdVolume);
    Audio::SetSFXVolume(g_CrowdAudio.NegativeVoiceId, MoodDef.NegativeVol * g_CrowdState.CrowdVolume);

    if (MoodDef.SaturationVolume > 0.0f)
    {
        if (MoodDef.SaturationSFXId == g_CrowdAudio.CurrentSaturationSFXId)
        {
            Audio::SetSFXVolume(g_CrowdAudio.SaturationVoiceId, MoodDef.SaturationVolume * g_CrowdState.CrowdVolume);
        }
        else
        {
            Audio::SetSFXVolumeGroup(MoodDef.SaturationSFXId, 2);
            float fVolReverb = Audio::gCrowdSFX.GetSFXVolReverb(0x99);

            SFXStartInfo info;
            info.uSFXID = (unsigned long)-1;
            info.fVolume = 100.0f;
            info.fPan = 100.0f;
            info.fVolReverb = 100.0f;
            info.uSurroundPan = 0xFF;
            info.uPitchBend = 0x2000;
            info.uModulation = 0;
            info.uDoppler = 0x2000;
            info.bActivateFilter = 0;
            info.filterFreq = 0;

            info.uSFXID = MoodDef.SaturationSFXId;
            info.fVolume = 1.0f;
            info.fVolReverb = fVolReverb;

            g_CrowdAudio.SaturationVoiceId = Audio::PlaySFX(info);
            Audio::SetSFXVolume(g_CrowdAudio.SaturationVoiceId, MoodDef.SaturationVolume);
        }
    }
    else
    {
        Audio::StopSFX(g_CrowdAudio.CurrentSaturationSFXId);
        g_CrowdAudio.CurrentSaturationSFXId = (unsigned long)-1;
    }

    if (!g_CrowdState.StreamLocked)
    {
        PlayVocal(MoodDef.Chant, g_CrowdState.ChantState, g_CrowdAudio.pChantStream);

        if (PlayVocal(MoodDef.Heckle, g_CrowdState.HeckleState, g_CrowdAudio.pHeckleStream))
        {
            unsigned char pan;
            GCAudioStreaming::AudioStreamBuffer* pBuf;

            pan = (unsigned char)nlRandom(0x7F, &nlDefaultSeed);
            pBuf = g_CrowdAudio.pHeckleStream->m_Buffers[0];
            pBuf->m_Pan = pan;
            sndStreamMixParameterEx(pBuf->m_StreamId, pBuf->m_Volume, pBuf->m_Pan, pBuf->m_SurroundPan, 0, 0);

            pan = (unsigned char)nlRandom(0x7F, &nlDefaultSeed);
            pBuf = g_CrowdAudio.pHeckleStream->m_Buffers[0];
            pBuf->m_SurroundPan = pan;
            sndStreamMixParameterEx(pBuf->m_StreamId, pBuf->m_Volume, pBuf->m_Pan, pBuf->m_SurroundPan, 0, 0);
        }
    }
}

/**
 * Offset/Address/Size: 0x2A0C | 0x80150120 | size: 0x270
 */
static void UpdateTiming(float dtArg)
{
    float dt = g_pGame->GetGameTime() - g_CrowdState.LastGameTime;

    if (!g_DoDecay)
    {
        dt = dtArg;
    }

    g_CrowdState.LastGameTime = g_pGame->GetGameTime();

    do
    {
        if (g_DoDecay)
        {
            break;
        }

        if (!g_CrowdState.AtDestination)
        {
            float speed = g_CrowdState.BlendFast ? g_Settings.BlendSpeedFast : g_Settings.BlendSpeedNormal;
            g_CrowdState.Interpolant += dt / speed;
        }

        if (g_CrowdState.ChantState.Ready)
        {
            g_CrowdState.ChantState.SinceLast += dt;
        }

        if (g_CrowdState.HeckleState.Ready)
        {
            g_CrowdState.HeckleState.SinceLast += dt;
        }

        if (!g_DoDecay)
        {
            break;
        }

        g_CrowdState._unk78 += dt;

        if (g_CrowdState.AtDestination)
        {
            if (g_CrowdState._unk78 > g_Settings.MoodDecayDelay)
            {
                float f2 = g_CrowdState.SinceMoodDest;
                if (f2)
                {
                    if (g_DoDecay)
                    {
                        float f0 = g_Settings.MoodDecayRate;
                        if (f0 <= f2)
                        {
                            f2 = f0;
                        }

                        g_CrowdState.SinceMoodDest -= f2;
                        g_CrowdState.SinceMoodDest = (g_CrowdState.SinceMoodDest >= 0.0f) ? g_CrowdState.SinceMoodDest : 0.0f;

                        g_CrowdState.DestMoodLevel = (unsigned char)(int)(5.0f * (float)g_CrowdState.SinceMoodDest);
                        g_CrowdState.CurrentMoodBlend[(s8)g_CrowdState.CurrentMood] = (float)g_CrowdState.SinceMoodDest;
                        g_CrowdState.SkipBlend = true;
                    }
                }
            }
        }
    } while (0);

    if (g_CrowdState.VolumeFade.Time > 0.0f)
    {
        g_CrowdState.VolumeFade.Interp += dtArg / g_CrowdState.VolumeFade.Time;
        g_CrowdState.VolumeFade.Interp = (g_CrowdState.VolumeFade.Interp <= 1.0f) ? g_CrowdState.VolumeFade.Interp : 1.0f;

        {
            float f2 = (float)g_CrowdState.VolumeFade.Interp;

            if (fabsf(f2 - 1.0f) <= 0.0001f)
            {
                g_CrowdState.VolumeFade.Time = 0.0f;
            }
        }

        ChangeCrowdVolume(Interpolate(g_CrowdState.VolumeFade.StartVol,
            g_CrowdState.VolumeFade.EndVol,
            g_CrowdState.VolumeFade.Interp));
    }
}

/**
 * Offset/Address/Size: 0x16E4 | 0x8014EDF8 | size: 0x1328
 */
void CrowdMood::ReadConfig()
{
    Config config(Config::ALLOCATE_HIGH);
    config.LoadFromFile("audio/CrowdMood.ini");

    CROWD_SETTINGS& settings = (CROWD_SETTINGS&)g_Settings;
    char IniTag[256];
    char* TagEnd;
    unsigned long MaxTagLen;
    CROWD_MOOD mood = CM_Positive;

    for (; mood < CM_END; Increment(mood))
    {
        MOOD_DEFINITION& MoodDef = (MOOD_DEFINITION&)g_MoodDefs[mood];
        nlStrNCpy(IniTag, MoodNames[mood], 0x100);

        TagEnd = IniTag + nlStrLen(IniTag);
        MaxTagLen = 0xFF - (TagEnd - IniTag);
        *TagEnd = '_';
        nlStrNCpy(++TagEnd, "NeutralVol", MaxTagLen);
        MoodDef.NeutralVol = GetConfigFloat(config, IniTag, 0.0f) / 100.0f;

        nlStrNCpy(TagEnd, "NegativeVol", MaxTagLen);
        MoodDef.NegativeVol = GetConfigFloat(config, IniTag, 0.0f) / 100.0f;

        nlStrNCpy(TagEnd, "PositiveVol", MaxTagLen);
        MoodDef.PositiveVol = GetConfigFloat(config, IniTag, 0.0f) / 100.0f;

        nlStrNCpy(TagEnd, "SaturationStart", MaxTagLen);
        MoodDef.SaturationStart = GetConfigFloat(config, IniTag, 0.0f) / 100.0f;

        nlStrNCpy(TagEnd, "SaturationVolume", MaxTagLen);
        MoodDef.SaturationVolume = GetConfigFloat(config, IniTag, 0.0f) / 100.0f;

        nlStrNCpy(TagEnd, "SaturationSample", MaxTagLen);
        {
            const char* sampleName;
            TagValuePair& tvp = config.FindTvp(IniTag);
            if (tvp.tag == NULL)
            {
                config.Set(IniTag, "none");
                sampleName = "none";
            }
            else
            {
                sampleName = (tvp.type == _BOOL)   ? LexicalCast<const char*, bool>(tvp.value.b)
                           : (tvp.type == _INT)    ? LexicalCast<const char*, int>(tvp.value.i)
                           : (tvp.type == _FLOAT)  ? LexicalCast<const char*, float>(tvp.value.f)
                           : (tvp.type == _STRING) ? LexicalCast<const char*, const char*>(tvp.value.s)
                                                   : NULL;
            }
            nlStrNCpy(settings.SaturationSampleNames[mood], sampleName, 0x100);
        }

        nlStrNCpy(TagEnd, "ChantVolume", MaxTagLen);
        MoodDef.Chant.Volume = GetConfigFloat(config, IniTag, 0.0f) / 100.0f;

        nlStrNCpy(TagEnd, "ChantVolumeRange", MaxTagLen);
        MoodDef.Chant.VolumeRange = GetConfigFloat(config, IniTag, 0.0f) / 100.0f;

        nlStrNCpy(TagEnd, "ChantDelay", MaxTagLen);
        MoodDef.Chant.Delay = GetConfigFloat(config, IniTag, 0.0f) / 100.0f;

        nlStrNCpy(TagEnd, "ChantDelayRange", MaxTagLen);
        MoodDef.Chant.DelayRange = GetConfigFloat(config, IniTag, 0.0f) / 100.0f;

        nlStrNCpy(TagEnd, "HeckleVolume", MaxTagLen);
        MoodDef.Heckle.Volume = GetConfigFloat(config, IniTag, 0.0f) / 100.0f;

        nlStrNCpy(TagEnd, "HeckleVolumeRange", MaxTagLen);
        MoodDef.Heckle.VolumeRange = GetConfigFloat(config, IniTag, 0.0f) / 100.0f;

        nlStrNCpy(TagEnd, "HeckleDelay", MaxTagLen);
        MoodDef.Heckle.Delay = GetConfigFloat(config, IniTag, 0.0f) / 100.0f;

        nlStrNCpy(TagEnd, "HeckleDelayRange", MaxTagLen);
        MoodDef.Heckle.DelayRange = GetConfigFloat(config, IniTag, 0.0f) / 100.0f;

        MoodDef.Chant.Delay /= 10.0;
        MoodDef.Chant.DelayRange /= 10.0;
        MoodDef.Heckle.Delay /= 10.0;
        MoodDef.Heckle.DelayRange /= 10.0;
    }

    settings.MoodDecayDelay = GetConfigFloat(config, "MoodDecayDelay", 0.0f) / 100.0f;
    settings.MoodDecayRate = GetConfigFloat(config, "MoodDecayRate", 0.0f) / 100.0f;
    settings.BlendSpeedNormal = GetConfigFloat(config, "BlendSpeedNormal", 0.0f) / 100.0f;
    settings.BlendSpeedFast = GetConfigFloat(config, "BlendSpeedFast", 0.0f) / 100.0f;
    settings.BlendStrictness = GetConfigFloat(config, "BlendStrictness", 0.0f) / 100.0f;
    settings.CrowdMasterVolume = GetConfigFloat(config, "CrowdMasterVolume", 0.0f) / 100.0f;

    settings.BlendSpeedNormal /= 10.0f;
    settings.BlendSpeedFast /= 10.0f;
    settings.MoodDecayDelay /= 10.0f;
    settings.MoodDecayRate /= 100.0f;

    {
        const char* sampleName;
        TagValuePair& tvp = config.FindTvp("NeutralSample");
        if (tvp.tag == NULL)
        {
            config.Set("NeutralSample", "none");
            sampleName = "none";
        }
        else
        {
            sampleName = (tvp.type == _BOOL)   ? LexicalCast<const char*, bool>(tvp.value.b)
                       : (tvp.type == _INT)    ? LexicalCast<const char*, int>(tvp.value.i)
                       : (tvp.type == _FLOAT)  ? LexicalCast<const char*, float>(tvp.value.f)
                       : (tvp.type == _STRING) ? LexicalCast<const char*, const char*>(tvp.value.s)
                                               : NULL;
        }
        nlStrNCpy(settings.NeutralSampleName, sampleName, 0x100);
    }

    {
        const char* sampleName;
        TagValuePair& tvp = config.FindTvp("PositiveSample");
        if (tvp.tag == NULL)
        {
            config.Set("PositiveSample", "none");
            sampleName = "none";
        }
        else
        {
            sampleName = (tvp.type == _BOOL)   ? LexicalCast<const char*, bool>(tvp.value.b)
                       : (tvp.type == _INT)    ? LexicalCast<const char*, int>(tvp.value.i)
                       : (tvp.type == _FLOAT)  ? LexicalCast<const char*, float>(tvp.value.f)
                       : (tvp.type == _STRING) ? LexicalCast<const char*, const char*>(tvp.value.s)
                                               : NULL;
        }
        nlStrNCpy(settings.PositiveSampleName, sampleName, 0x100);
    }

    {
        const char* sampleName;
        TagValuePair& tvp = config.FindTvp("NegativeSample");
        if (tvp.tag == NULL)
        {
            config.Set("NegativeSample", "none");
            sampleName = "none";
        }
        else
        {
            sampleName = (tvp.type == _BOOL)   ? LexicalCast<const char*, bool>(tvp.value.b)
                       : (tvp.type == _INT)    ? LexicalCast<const char*, int>(tvp.value.i)
                       : (tvp.type == _FLOAT)  ? LexicalCast<const char*, float>(tvp.value.f)
                       : (tvp.type == _STRING) ? LexicalCast<const char*, const char*>(tvp.value.s)
                                               : NULL;
        }
        nlStrNCpy(settings.NegativeSampleName, sampleName, 0x100);
    }

    {
        nlStrNCpy(IniTag, "RandomChant", 0x100);
        const char* sampleName;
        char* tagEnd = IniTag + nlStrLen(IniTag);
        RANDOM_STREAMS* pStreams = (RANDOM_STREAMS*)&g_RandomChants;
        pStreams->Count = 0;

        while (pStreams->Count <= 0x20)
        {
            nlSNPrintf(tagEnd, 4, "%d", pStreams->Count + 1);

            TagValuePair& tvp = config.FindTvp(IniTag);
            if (tvp.tag == NULL)
            {
                config.Set(IniTag, "none");
                sampleName = "none";
            }
            else
            {
                sampleName = (tvp.type == _BOOL)   ? LexicalCast<const char*, bool>(tvp.value.b)
                           : (tvp.type == _INT)    ? LexicalCast<const char*, int>(tvp.value.i)
                           : (tvp.type == _FLOAT)  ? LexicalCast<const char*, float>(tvp.value.f)
                           : (tvp.type == _STRING) ? LexicalCast<const char*, const char*>(tvp.value.s)
                                                   : NULL;
            }

            if (nlStrNCmp<char>(sampleName, "none", 4) == 0)
            {
                break;
            }

            nlStrNCpy(pStreams->Files[pStreams->Count], sampleName, 0x100);
            pStreams->Count++;
        }
    }

    nlStrNCpy(IniTag, "RandomHeckle", 0x100);
    const char* sampleName;
    char* tagEnd = IniTag + nlStrLen(IniTag);
    RANDOM_STREAMS* pStreams = (RANDOM_STREAMS*)&g_RandomHeckles;
    pStreams->Count = 0;

    while (pStreams->Count <= 0x20)
    {
        nlSNPrintf(tagEnd, 4, "%d", pStreams->Count + 1);

        TagValuePair& tvp = config.FindTvp(IniTag);
        if (tvp.tag == NULL)
        {
            config.Set(IniTag, "none");
            sampleName = "none";
        }
        else
        {
            sampleName = (tvp.type == _BOOL)   ? LexicalCast<const char*, bool>(tvp.value.b)
                       : (tvp.type == _INT)    ? LexicalCast<const char*, int>(tvp.value.i)
                       : (tvp.type == _FLOAT)  ? LexicalCast<const char*, float>(tvp.value.f)
                       : (tvp.type == _STRING) ? LexicalCast<const char*, const char*>(tvp.value.s)
                                               : NULL;
        }

        if (nlStrNCmp<char>(sampleName, "none", 4) == 0)
        {
            break;
        }

        nlStrNCpy(pStreams->Files[pStreams->Count], sampleName, 0x100);
        pStreams->Count++;
    }

    settings.NoStreaming = GetConfigBool(Config::Global(), "no_stream", false);
}

inline GCAudioStreaming::AudioStream::AudioStream(GCAudioStreaming::AudioBufferMgr& mgr, unsigned long bufCount)
    : m_FlagAtDelete(0)
    , m_State(GCAudioStreaming::SS_New)
    , m_StreamLength((unsigned long)-1)
    , m_StreamPos(0)
    , m_OldLength(0)
    , m_BuffMgr(mgr)
    , m_Flags(0)
    , m_BufferCount(bufCount)
{
    memset(m_Buffers, sizeof(m_Buffers), 0);
}

inline GCAudioStreaming::StereoAudioStream::StereoAudioStream(GCAudioStreaming::AudioBufferMgr& mgr)
    : AudioStream(mgr, 2)
{
    m_pFile = NULL;
    m_Interleave = 0;
}

inline GCAudioStreaming::MonoAudioStream::MonoAudioStream(GCAudioStreaming::AudioBufferMgr& mgr)
    : AudioStream(mgr, 1)
{
    m_pFile = NULL;
}

/**
 * Offset/Address/Size: 0x1300 | 0x8014EA14 | size: 0x3E4
 */
void CrowdMood::Init()
{
    if (g_Initd)
        return;

    Config& config = Config::Global();
    bool crowdOff = GetConfigBool(config, "no_crowd", false);
    switch (crowdOff)
    {
    case false:
        break;
    default:
        return;
    }

    memset(&g_CrowdState, 0, sizeof(g_CrowdState));

    Audio::MasterVolume::SetVolume((Audio::MasterVolume::VOLUME_GROUP)4, g_CrowdState.CrowdVolume);

    g_CrowdState.DestMood = CM_Neutral;
    g_CrowdState.AtDestination = true;

    g_CrowdState.ChantState.NextAt = (float)nlRandom(1, &nlDefaultSeed);
    g_CrowdState.HeckleState.NextAt = (float)nlRandom(1, &nlDefaultSeed);
    g_CrowdState.ChantState.Ready = g_CrowdState.HeckleState.Ready = 0;

    RestartLoops();
    g_CrowdAudio.CurrentSaturationSFXId = (unsigned long)-1;

    if (!g_Settings.NoStreaming)
    {
        GCAudioStreaming::StereoAudioStream* pChant = new (nlMalloc(sizeof(GCAudioStreaming::StereoAudioStream), 8, false)) GCAudioStreaming::StereoAudioStream(g_BufferMgr);
        g_CrowdAudio.pChantStream = pChant;

        GCAudioStreaming::MonoAudioStream* pHeckle = new (nlMalloc(sizeof(GCAudioStreaming::MonoAudioStream), 8, false)) GCAudioStreaming::MonoAudioStream(g_BufferMgr);
        g_CrowdAudio.pHeckleStream = pHeckle;
    }

    g_CrowdAudio.CurrentSaturationSampleName = NULL;
    CrowdMood::SetCrowdVolume(0, 0);

    g_CrowdState.LPFOn = false;
    g_CrowdState.LPFOn = false;
    g_CrowdState.LPFFreq = 0;
    g_Initd = true;
}

/**
 * Offset/Address/Size: 0xF4C | 0x8014E660 | size: 0x3B4
 */
void CrowdMood::Purge(bool bJustStopSFX)
{
    g_CrowdSFXStopped = true;

    Audio::StopSFX(g_CrowdAudio.NeutralVoiceId);
    Audio::StopSFX(g_CrowdAudio.PositiveVoiceId);
    Audio::StopSFX(g_CrowdAudio.NegativeVoiceId);

    GCAudioStreaming::StereoAudioStream* pChant = g_CrowdAudio.pChantStream;
    if (pChant != NULL)
    {
        pChant->Stop();
    }

    GCAudioStreaming::MonoAudioStream* pHeckle = g_CrowdAudio.pHeckleStream;
    if (pHeckle != NULL)
    {
        pHeckle->Stop();
    }

    if (!bJustStopSFX)
    {
        delete g_CrowdAudio.pChantStream;
        delete g_CrowdAudio.pHeckleStream;
        g_CrowdAudio.pChantStream = NULL;
        g_CrowdAudio.pHeckleStream = NULL;
        memset(&g_CrowdState, 0, sizeof(CROWD_STATE));
        g_Initd = false;
    }
}

/**
 * Offset/Address/Size: 0x9F8 | 0x8014E10C | size: 0x554
 */
void CrowdMood::Update(float dt)
{
    MOOD_DEFINITION moodDef;
    if (!g_Initd)
        return;

    unsigned char noCrowd = GetConfigBool(Config::Global(), "no_crowd", false);
    if (noCrowd == 1)
        return;

    if (g_CrowdSFXStopped)
        return;

    if (!g_Settings.NoStreaming)
    {
        if (!g_CrowdState.StreamLocked)
        {
            GCAudioStreaming::StereoAudioStream* pChant = g_CrowdAudio.pChantStream;
            if (pChant->m_State <= GCAudioStreaming::SS_Initd && !g_CrowdState.ChantState.Ready)
            {
                if (pChant->SafeToPurge())
                {
                    g_CrowdState.ChantState.Ready = true;
                    g_CrowdState.ChantState.SinceLast = 0.0f;
                    WarmRandomStream<GCAudioStreaming::StereoAudioStream>(g_RandomChants, g_CrowdAudio.pChantStream);
                }
            }

            GCAudioStreaming::MonoAudioStream* pHeckle = g_CrowdAudio.pHeckleStream;
            if (pHeckle->m_State <= GCAudioStreaming::SS_Initd && !g_CrowdState.HeckleState.Ready)
            {
                if (pHeckle->SafeToPurge())
                {
                    g_CrowdState.HeckleState.Ready = true;
                    g_CrowdState.HeckleState.SinceLast = 0.0f;
                    WarmRandomStream<GCAudioStreaming::MonoAudioStream>(g_RandomHeckles, g_CrowdAudio.pHeckleStream);
                }
            }
        }
    }

    UpdateTiming(dt);

    if (g_CrowdState.HasChanged)
    {
        CROWD_MOOD mask = (CROWD_MOOD)-1;
        u8 level = g_CrowdState.DestMoodLevel;
        u32 clampedLevel = CM_END;
        if (((u32)level & mask) <= (u32)CM_END)
            clampedLevel = ((u32)level & mask);
        g_CrowdState.DestMoodLevel = clampedLevel;

        f32 halfFactor = 0.5f;
        CROWD_MOOD mood = CM_Positive;
        f32 zero = 0.0f;
        while (mood < (CROWD_MOOD)4)
        {
            g_CrowdState.StartingMood[mood] = g_CrowdState.CurrentMoodBlend[mood];

            f32 dest;
            if ((s8)g_CrowdState.DestMood == mood)
            {
                dest = (f32)g_CrowdState.DestMoodLevel / 5.0f;
            }
            else
            {
                dest = 0.0f;
            }

            g_CrowdState.DestinationMood[mood] = dest;

            bool bothNonZero = false;
            f32 midpoint = g_CrowdState.DestinationMood[mood] + g_CrowdState.StartingMood[mood];
            midpoint *= halfFactor;
            g_CrowdState.MidpointMood[mood] = midpoint;

            if (g_CrowdState.CurrentMoodBlend[mood] != zero)
            {
                if (g_CrowdState.DestinationMood[mood] != zero)
                {
                    bothNonZero = true;
                }
            }

            f32 factor;
            if (bothNonZero)
            {
                factor = 1.0f;
            }
            else
            {
                factor = g_Settings.BlendStrictness;
            }

            g_CrowdState.MidpointMood[mood] *= factor;

            Increment<CROWD_MOOD>(mood);
        }

        f32 distToMid = NDimDistance<4>(g_CrowdState.StartingMood, g_CrowdState.MidpointMood);
        f32 distToDest = NDimDistance<4>(g_CrowdState.MidpointMood, g_CrowdState.DestinationMood);

        f32 totalDist = distToMid + distToDest;
        f32 zeroF = 0.0f;
        f32 epsilon = 0.0001f;
        if (fabsf(totalDist - zeroF) <= epsilon)
        {
            g_CrowdState.AtDestination = false;
        }
        else
        {
            f32 interpMid = distToMid / totalDist;
            g_CrowdState.Interpolant = zeroF;
            g_CrowdState.AtDestination = false;
            g_CrowdState.InterpolantMidpoint = interpMid;
        }
        g_CrowdState.HasChanged = false;
    }

    if (!g_CrowdState.SkipBlend && !g_CrowdState.AtDestination)
    {
        f32 oneConst = 1.0f;
        f32 eps = 0.0001f;
        f32 interp = g_CrowdState.Interpolant;
        if (interp - oneConst > eps)
        {
            g_CrowdState.AtDestination = true;
            g_CrowdState.Interpolant = oneConst;
            g_CrowdState._unk78 = 0.0f;
            g_CrowdState.BlendFast = false;
        }

        f32 interpVal = g_CrowdState.Interpolant;
        f32 interpMidVal = g_CrowdState.InterpolantMidpoint;

        float* baseArray;
        if (interpVal < interpMidVal)
        {
            baseArray = g_CrowdState.StartingMood;
        }
        else
        {
            baseArray = g_CrowdState.MidpointMood;
        }

        float* targetArray;
        if (interpVal < interpMidVal)
        {
            targetArray = g_CrowdState.MidpointMood;
        }
        else
        {
            targetArray = g_CrowdState.DestinationMood;
        }

        f32 normalizedInterp = (fabsf(g_CrowdState.Interpolant - 1.0f) <= 0.0001f)
                                 ? 1.0f
                                 : ((g_CrowdState.Interpolant >= g_CrowdState.InterpolantMidpoint)
                                           ? (g_CrowdState.Interpolant - g_CrowdState.InterpolantMidpoint) / (1.0f - g_CrowdState.InterpolantMidpoint)
                                           : g_CrowdState.Interpolant / g_CrowdState.InterpolantMidpoint);

        g_CrowdState.SinceMoodDest = 0.0f;
        f32 complement = 1.0f - normalizedInterp;
        CROWD_MOOD mood2 = CM_Positive;
        while (mood2 < (CROWD_MOOD)4)
        {
            g_CrowdState.CurrentMoodBlend[mood2] = complement * baseArray[mood2] + normalizedInterp * targetArray[mood2];
            if (g_CrowdState.CurrentMoodBlend[mood2] > g_CrowdState.SinceMoodDest)
            {
                g_CrowdState.SinceMoodDest = g_CrowdState.CurrentMoodBlend[mood2];
                g_CrowdState.CurrentMood = (u8)mood2;
            }
            Increment<CROWD_MOOD>(mood2);
        }
    }
    else
    {
        g_CrowdState.SkipBlend = false;
    }

    MoodDefFromBlend(g_CrowdState.CurrentMoodBlend, moodDef);
    PlayMoodDef(moodDef);
}

/**
 * Offset/Address/Size: 0x92C | 0x8014E040 | size: 0xCC
 */
void CrowdMood::AdjustMood(CrowdMood::CROWD_MOOD Towards, unsigned long Amount)
{
    if (!g_Initd)
        return;

    if (Towards == (s8)g_CrowdState.DestMood)
    {
        g_CrowdState.DestMoodLevel += Amount;
    }
    else
    {
        u8 level = g_CrowdState.DestMoodLevel;
        if (level > Amount)
        {
            g_CrowdState.DestMoodLevel -= Amount;
        }
        else
        {
            g_CrowdState.DestMood = Towards;
            g_CrowdState.DestMoodLevel = (Towards == CM_Neutral) ? 0 : (Amount - level);
        }
    }

    g_CrowdState.HasChanged = true;
    g_CrowdState.AtDestination = false;

    ___blank("Crowd mood adjusted to %d %d\n", *(volatile s8*)&g_CrowdState.DestMood, g_CrowdState.DestMoodLevel);
}

/**
 * Offset/Address/Size: 0x804 | 0x8014DF18 | size: 0x128
 */
void CrowdMood::SetMood(CrowdMood::CROWD_MOOD Mood, unsigned long Amount)
{
    g_CrowdState.AtDestination = false;
    g_CrowdState.DestMoodLevel = Amount;
    g_CrowdState.DestMood = Mood;
    g_CrowdState.SinceMoodDest = (f32)Amount;
    g_CrowdState.CurrentMood = Mood;
    g_CrowdState.Interpolant = 1.0f;
    g_CrowdState.SkipBlend = true;

    for (CrowdMood::CROWD_MOOD mood = CM_Positive; mood < CM_Neutral; Increment<CrowdMood::CROWD_MOOD>(mood))
    {
        f32 blend;
        if (mood == Mood)
        {
            blend = (f32)Amount / (f32)CM_END;
        }
        else
        {
            blend = 0.0f;
        }
        g_CrowdState.CurrentMoodBlend[mood] = blend;
    }

    ___blank("Crowd mood set to %d %d\n", *(volatile s8*)&g_CrowdState.DestMood, g_CrowdState.DestMoodLevel);
}

/**
 * Offset/Address/Size: 0x7E8 | 0x8014DEFC | size: 0x1C
 */
void CrowdMood::InitiateFastCrowdTransition()
{
    g_CrowdState.BlendFast = true;
}

/**
 * Offset/Address/Size: 0x65C | 0x8014DD70 | size: 0x18C
 */
#pragma push
#pragma dont_inline on
void CrowdMood::SetCrowdVolume(unsigned long Volume, unsigned long FadeTime)
{
    MOOD_DEFINITION MoodDef;

    unsigned char crowdOff = GetConfigBool(Config::Global(), "no_crowd", false);

    if (crowdOff == 1)
    {
        g_CrowdState.CrowdVolume = 0.0f;
        MoodDefFromBlend(g_CrowdState.CurrentMoodBlend, MoodDef);
        PlayMoodDef(MoodDef);
        return;
    }

    if (FadeTime == 0)
    {
        ChangeCrowdVolume((float)Volume / 127.0f);
    }
    else
    {
        g_CrowdState.VolumeFade.StartVol = g_CrowdState.CrowdVolume;
        g_CrowdState.VolumeFade.EndVol = (float)Volume / 127.0f;
        g_CrowdState.VolumeFade.Time = (float)FadeTime / 1000.0f;
        g_CrowdState.VolumeFade.Interp = 0.0f;
    }
}
#pragma pop

namespace CrowdMood
{
/**
 * Unreferenced retail helper, dead-stripped at link. Its inverse relationship
 * with ChangeCrowdVolume, referenced globals, and 0x44 object size all agree
 * with the MAP and DWARF records.
 */
static unsigned long GetCrowdVolume()
{
    return (unsigned long)((g_CrowdState.CrowdVolume / g_Settings.CrowdMasterVolume) * 127.0f);
}
} // namespace CrowdMood

/**
 * Offset/Address/Size: 0x490 | 0x8014DBA4 | size: 0x1CC
 */
void CrowdMood::ActivateLPF(bool bOn)
{
    if (bOn == g_CrowdState.LPFOn)
    {
        return;
    }

    Audio::ActivateFilterOnSFX(g_CrowdAudio.NeutralVoiceId, bOn);
    Audio::ActivateFilterOnSFX(g_CrowdAudio.PositiveVoiceId, bOn);
    Audio::ActivateFilterOnSFX(g_CrowdAudio.NegativeVoiceId, bOn);

    GCAudioStreaming::StereoAudioStream* pChant = g_CrowdAudio.pChantStream;
    if (pChant != NULL
        && g_CrowdAudio.pHeckleStream != NULL
        && !g_CrowdState.StreamLocked)
    {
        pChant->SetLPF(bOn);
        GCAudioStreaming::MonoAudioStream* pHeckle = g_CrowdAudio.pHeckleStream;
        pHeckle->SetLPF(bOn);
    }

    g_CrowdState.LPFOn = bOn;
}

/**
 * Offset/Address/Size: 0x2D4 | 0x8014D9E8 | size: 0x1BC
 */
void CrowdMood::SetLPF(unsigned short Frequency)
{
    if (g_CrowdState.LPFFreq == Frequency)
        return;

    Audio::SetFilterFreqOnSFX(g_CrowdAudio.NeutralVoiceId, Frequency);
    Audio::SetFilterFreqOnSFX(g_CrowdAudio.PositiveVoiceId, Frequency);
    Audio::SetFilterFreqOnSFX(g_CrowdAudio.NegativeVoiceId, Frequency);

    GCAudioStreaming::StereoAudioStream* pChant = g_CrowdAudio.pChantStream;
    if (pChant != NULL && g_CrowdAudio.pHeckleStream != NULL && !g_CrowdState.StreamLocked)
    {
        pChant->SetLPF(Frequency);
        g_CrowdAudio.pHeckleStream->SetLPF(Frequency);
    }

    g_CrowdState.LPFFreq = Frequency;
}

/**
 * Offset/Address/Size: 0x2B4 | 0x8014D9C8 | size: 0x20
 */
GCAudioStreaming::StereoAudioStream* CrowdMood::LockStream()
{
    g_CrowdState.StreamLocked = true;
    return g_CrowdAudio.pChantStream;
}

/**
 * Offset/Address/Size: 0x104 | 0x8014D818 | size: 0x1B0
 */
void CrowdMood::UnlockStream()
{
    GCAudioStreaming::StereoAudioStream* pChant = g_CrowdAudio.pChantStream;

    if (pChant != NULL)
    {
        pChant->Stop();
    }

    g_CrowdState.StreamLocked = false;
}

/**
 * Offset/Address/Size: 0xFC | 0x8014D810 | size: 0x8
 */
void CrowdMood::EnableCrowdDecay(bool enable)
{
    g_DoDecay = enable;
}

/**
 * Offset/Address/Size: 0x0 | 0x8014D714 | size: 0xFC
 */
void CrowdMood::RestartLoops()
{
    struct LOOP_LOAD
    {
        const char* SampleName;
        unsigned long AudioId;
        unsigned long& VoiceId;
    };

    LOOP_LOAD LoadData[3] = {
        { g_Settings.NeutralSampleName, Audio::CROWDSFX_EVENT_YEAH_SMALL1, g_CrowdAudio.NeutralVoiceId },
        { g_Settings.PositiveSampleName, Audio::CROWDSFX_EVENT_YEAH_BIG, g_CrowdAudio.PositiveVoiceId },
        { g_Settings.NegativeSampleName, Audio::CROWDSFX_EVENT_YEAH_SMALL2, g_CrowdAudio.NegativeVoiceId },
    };

    u32 i;
    for (i = 0; i < 3; i++)
    {
        Audio::SoundAttributes sndAtr;
        sndAtr.Init();
        sndAtr.SetSoundType(LoadData[i].AudioId, false);
        sndAtr.mf_Volume = 1.0f;
        LoadData[i].VoiceId = Audio::gCrowdSFX.Play(sndAtr);
        PlatAudio::SetSFXVolume(LoadData[i].VoiceId, 0.0f);
    }
    g_CrowdSFXStopped = false;
}

/**
 * Unreferenced retail test, dead-stripped at link. The MAP records a 0x208
 * body here and DWARF records the StopAt and Frame locals plus references to
 * g_Initd and g_CrowdState. Its real SetMood and AdjustMood calls also restore
 * the retail literal chronology without synthetic pool declarations.
 */
static void CrowdMoodTest()
{
    long long Frame = OSGetTime();
    long long StopAt = Frame + OSSecondsToTicks(5);

    do
    {
        UpdateTiming(5.0f);
        CrowdMood::SetMood((CrowdMood::CROWD_MOOD)g_CrowdState.DestMood, g_CrowdState.DestMoodLevel);
        CrowdMood::AdjustMood((CrowdMood::CROWD_MOOD)g_CrowdState.DestMood, g_CrowdState.DestMoodLevel);
        Frame = OSGetTime();
    } while (Frame < StopAt);
}
