#include "Game/Sys/GCStream.h"
#include "Game/Audio/PriorityStream.h"
#include "Game/Audio/CrowdMood.h"
#include "Game/Game.h"
#include "NL/nlConfig.h"
#include "NL/nlPrint.h"
#include "NL/nlString.h"

unsigned char PriorityStream::PLAY_RECORD::s_BowserAttackNext = true;
unsigned char PriorityStream::PLAY_RECORD::s_SuddenDeathNext = true;

// Retail dead-stripped this body (MAP: UNUSED 0xB8). Keep the definition: a dead emission still anchors .text and pool order.
unsigned long PriorityStream::PLAY_RECORD::GetNextStreamId(unsigned long SimpleStreamId)
{
    char StreamName[64];
    char* Format;
    unsigned char* pCounter;

    switch (SimpleStreamId)
    {
    case 0x436E3953:
        pCounter = &PLAY_RECORD::s_BowserAttackNext;
        Format = "STAD_Bowser_Attack_%02d";
        break;
    case 0x57CB5A12:
        pCounter = &PLAY_RECORD::s_SuddenDeathNext;
        Format = "STAD_Sudden_Death_%02d";
        break;
    default:
        return SimpleStreamId;
    }

    nlSNPrintf(StreamName, 64, Format, *pCounter);
    *pCounter = *pCounter + 1;
    if (*pCounter == 4)
    {
        *pCounter = 1;
    }
    return nlStringLowerHash(StreamName);
}

// Retail dead-stripped this body (MAP: UNUSED 0x90). Keep the definition: a dead emission still anchors .text and pool order.
void PriorityStream::PLAY_RECORD::Set(
    unsigned long StreamId,
    float Volume,
    bool Looping,
    unsigned long FadeIn,
    unsigned long ExistingFadeOut,
    const char* StreamParam,
    Audio::MasterVolume::VOLUME_GROUP VolGroup,
    bool Queue,
    bool Active)
{
    m_StreamId = StreamId;
    m_OrigStreamId = StreamId;
    m_Volume = Volume;
    m_Looping = Looping;
    m_FadeIn = FadeIn;
    m_ExistingFadeOut = ExistingFadeOut;
    m_VolGroup = VolGroup;
    m_Queue = Queue;
    m_Active = Active;

    if (StreamParam)
    {
        nlStrNCpy<char>(m_StreamParam, StreamParam, 32);
    }
    else
    {
        m_StreamParam[0] = '\0';
    }
}

// Retail dead-stripped this body (MAP: UNUSED 0x180). Keep the definition: a dead emission still anchors .text and pool order.
void PriorityStream::PLAY_RECORD::Play(bool CheckActive, bool GetNextId)
{
    if (!m_StreamId)
    {
        return;
    }

    if (CheckActive && !m_Active)
    {
        return;
    }

    if (GetNextId)
    {
        m_StreamId = GetNextStreamId(m_OrigStreamId);
    }

    if (m_Queue)
    {
        m_Queue = 0;
        m_Track.QueueStream(
            m_StreamId,
            m_Volume,
            m_Looping,
            m_FadeIn,
            m_StreamParam[0] ? m_StreamParam : (const char*)0,
            (Audio::MasterVolume::VOLUME_GROUP)m_VolGroup);
    }
    else
    {
        m_Track.PlayStream(
            m_StreamId,
            m_Volume,
            m_Looping,
            m_FadeIn,
            m_ExistingFadeOut,
            m_StreamParam[0] ? m_StreamParam : (const char*)0,
            (Audio::MasterVolume::VOLUME_GROUP)m_VolGroup);
    }
}

/**
 * Offset/Address/Size: 0xEA8 | 0x8015895C | size: 0x10
 */
void PriorityStream::Reset()
{
    PLAY_RECORD::s_BowserAttackNext = true;
    PLAY_RECORD::s_SuddenDeathNext = true;
}

/**
 * Offset/Address/Size: 0xA34 | 0x801584E8 | size: 0x474
 */
void PriorityStream::PlayStream(unsigned long StreamId, float Volume, bool Looping, unsigned long FadeIn, unsigned long ExistingFadeOut, const char* StreamParam)
{
    if (GetConfigBool(Config::Global(), "no_stream", false) == true)
    {
        return;
    }

    if (g_pGame->mInSuddenDeath)
    {
        switch (StreamId)
        {
        case 0x09451A58:
        case 0xA207B1AE:
        case 0x436E3953:
            return;
        }
    }

    bool active = GrabCrowdStream(ExistingFadeOut);
    PLAY_RECORD* pRecord;
    Audio::MasterVolume::VOLUME_GROUP VolGroup = Audio::MasterVolume::VG_Special;
    bool queue = true;

    switch (StreamId)
    {
    case 0xE38B5407:
        VolGroup = Audio::MasterVolume::VG_Voice;
        break;
    case 0x436E3953:
        VolGroup = Audio::MasterVolume::VG_Music;
        break;
    case 0x09451A58:
        VolGroup = Audio::MasterVolume::VG_Music;
        queue = 0;
        break;
    case 0xA207B1AE:
        VolGroup = Audio::MasterVolume::VG_Music;
        queue = 0;
        break;
    case 0x57CB5A12:
        VolGroup = Audio::MasterVolume::VG_Music;
        break;
    }

    if (StreamId == 0xE38B5407)
    {
        pRecord = &m_CapChant;
    }
    else
    {
        pRecord = &m_PStream;
    }

    if ((StreamId == 0xA207B1AE) && m_CapChant.m_StreamId)
    {
        pRecord->m_StreamId = 0;
    }
    else
    {
        pRecord->Set(
            StreamId,
            Volume,
            Looping,
            FadeIn,
            ExistingFadeOut,
            StreamParam,
            VolGroup,
            active,
            queue);
    }

    FakeResume(false);
}

/**
 * Offset/Address/Size: 0x79C | 0x80158250 | size: 0x298
 */
void PriorityStream::Stop(unsigned long StreamId, unsigned long FadeOut)
{
    if ((StreamId == 0xE38B5407) && m_CapChant.m_StreamId)
    {
        m_Track.Stop(FadeOut);
        m_CapChant.m_StreamId = 0;

        FakeResume(true);
    }
    else if ((m_PStream.m_OrigStreamId == StreamId)
             || ((StreamId == 0x436E3953) && ((m_PStream.m_OrigStreamId == 0x09451A58) || (m_PStream.m_OrigStreamId == 0xA207B1AE))))
    {
        m_Track.Stop(FadeOut);
        m_PStream.m_StreamId = 0;
    }
}

/**
 * Offset/Address/Size: 0x770 | 0x80158224 | size: 0x2C
 */
void PriorityStream::FakePause(unsigned long Fadeout)
{
    m_InPause = true;
    m_Track.Stop(Fadeout);
}

/**
 * Offset/Address/Size: 0x530 | 0x80157FE4 | size: 0x240
 */
void PriorityStream::FakeResume(bool CheckActive)
{
    if (m_CapChant.m_StreamId)
    {
        m_CapChant.Play(CheckActive, false);
    }
    else
    {
        m_PStream.Play(CheckActive, true);
    }

    m_InPause = false;
}

/**
 * Offset/Address/Size: 0x380 | 0x80157E34 | size: 0x1B0
 */
void PriorityStream::TrackIdleCB()
{
    if (m_InPause)
    {
        return;
    }

    if (m_CapChant.m_StreamId)
    {
        m_CapChant.m_StreamId = 0;

        if (m_PStream.m_StreamId)
        {
            m_PStream.Play(true, true);
            return;
        }
    }

    if (m_PStream.m_StreamId)
    {
        m_PStream.m_StreamId = 0;
    }
    CrowdMood::UnlockStream();
}

/**
 * Offset/Address/Size: 0x0 | 0x80157AB4 | size: 0x380
 */
bool PriorityStream::GrabCrowdStream(unsigned long Fadeout)
{
    GCAudioStreaming::StereoAudioStream* pStream;
    bool result = false;

    if (!CrowdMood::IsStreamLocked())
    {
        pStream = CrowdMood::LockStream();
        if (pStream != NULL)
        {
            switch (pStream->m_State)
            {
            case GCAudioStreaming::SS_Playing:
            {
                if (Fadeout != 0)
                {
                    result = 1;
                    m_Track.AttachStream(
                        pStream, (Audio::MasterVolume::VOLUME_GROUP)4, (unsigned long)-1, 0, 0, 0);
                    m_Track.StopHead(Fadeout);
                }
                else
                {
                    pStream->Stop();
                }
                break;
            }
            case GCAudioStreaming::SS_Warming:
            case GCAudioStreaming::SS_Warm:
            {
                pStream->Stop();
                break;
            }
            }
        }
    }

    return result;
}
