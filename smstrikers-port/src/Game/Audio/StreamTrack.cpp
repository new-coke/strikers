#include "NL/nlBind.h"
#include "Game/Sys/GCStream.h"
#include "Game/Audio/StreamTrack.h"
#include <stddef.h>
#include "NL/nlAlgorithm.h"
#include "NL/nlConfig.h"
#include "NL/nlFileGC.h"
#include "NL/nlList.h"
#include "NL/nlString.h"

extern GCAudioStreaming::AudioBufferMgr g_BufferMgr;

/**
 * Offset/Address/Size: 0x24E4 | 0x8015723C | size: 0x100
 */
void AudioStreamTrack::TrackManagerBase::Update(float dT)
{
    typedef DLListEntry<GCAudioStreaming::StereoAudioStream*> Entry;

    GCAudioStreaming::StereoAudioStream* pStream;
    Entry* toFree;
    Entry* toRemove;
    Entry* head;
    Entry* entry;
    Entry** headAddr;

    Entry* start = nlDLRingGetStart(m_StreamDeleteList.m_Head);
    head = m_StreamDeleteList.m_Head;
    headAddr = &m_StreamDeleteList.m_Head;
    entry = start;

    while (entry != NULL)
    {
        if (entry->entry->SafeToPurge())
        {
            pStream = entry->entry;
            pStream->~StereoAudioStream();
            m_StreamPool.Free(pStream);

            toRemove = entry;
            toFree = entry;

            if (nlDLRingIsEnd(head, entry) || entry == NULL)
            {
                entry = NULL;
            }
            else
            {
                entry = entry->m_next;
            }

            nlDLRingRemove(headAddr, toRemove);
            m_StreamDeleteList.Deallocate(toFree, NULL);
        }
        else
        {
            if (nlDLRingIsEnd(head, entry) || entry == NULL)
            {
                entry = NULL;
            }
            else
            {
                entry = entry->m_next;
            }
        }
    }
}

namespace AudioStreamTrack
{
TrackManagerBase::StreamFileLookup::StreamFileLookup(
    const char* name,
    const Function<bool(const char*, char*, unsigned long)>& ParamCB)
    : m_ParamCB(ParamCB)
    , m_pLookup(NULL)
    , m_StreamCount(0)
{
    typedef STREAM_FILE_LIST_LOOKUP LookupT;
    typedef ListEntry<LookupT> ListEntryT;

    unsigned long TotalFileNameMem = 0;
    nlListSlotPoolHigh<LookupT> LookupList(16, 16);
    Config config(Config::ALLOCATE_HIGH);
    config.LoadFromFile("audio/data/streams/StreamNames.txt");

    for (Config::Iterator<const char*> iter(config); iter.IsValid(); iter.Next())
    {
        ListEntryT* entry = LookupList.Allocate(LookupT());
        nlListAddEnd(&LookupList.m_Head, &LookupList.m_Tail, entry);

        entry->entry.NameHash = nlStringLowerHash(iter.Tag());
        entry->entry.FileName = iter.Current();

        entry->entry.FileNameLen = nlStrLen(entry->entry.FileName) + 1;

        TotalFileNameMem += entry->entry.FileNameLen;
        ++m_StreamCount;
    }

    m_pLookup = (STREAM_FILE_LOOKUP*)nlMalloc(
        m_StreamCount * sizeof(STREAM_FILE_LOOKUP), 8, false);
    m_pStrings = (char*)nlMalloc(TotalFileNameMem, 8, false);

    int i = 0;
    char* pLastString = m_pStrings;
    nlListIterator<LookupT> listIter = LookupList.Begin();
    while (listIter.IsValid())
    {
        m_pLookup[i].NameHash = listIter.Current().NameHash;
        m_pLookup[i].FileName = pLastString;
        memcpy(pLastString, listIter.Current().FileName, listIter.Current().FileNameLen);

        i++;
        pLastString += listIter.Current().FileNameLen;
        listIter.Next();
    }

    nlQSort<STREAM_FILE_LOOKUP>(
        m_pLookup, (int)m_StreamCount, &nlDefaultQSortComparer<STREAM_FILE_LOOKUP>);

    LookupList.Clear();
    LookupList.m_Allocator.FreeBlocks();
}

void TrackManagerBase::StreamFileLookup::GetFileName(
    unsigned long StreamId, char* FileName, int MaxLength, const char* Param)
{
    const STREAM_FILE_LOOKUP* pLookup = nlBSearch<STREAM_FILE_LOOKUP, unsigned long>(StreamId, m_pLookup, m_StreamCount);

    const char* pPercent = strchr(pLookup->FileName, '%');
    if (pPercent != NULL)
    {
        char* pInsert = FileName;
        unsigned long InsertIndex = (unsigned long)(pPercent - pLookup->FileName);
        nlStrNCpy<char>(pInsert, pLookup->FileName, InsertIndex + 1);
        pInsert += InsertIndex;
        m_ParamCB(Param, pInsert, MaxLength - InsertIndex);
        unsigned long InsertLen = nlStrLen(pInsert);
        nlStrNCpy<char>(pInsert + InsertLen, pPercent + 3, MaxLength - InsertIndex - InsertLen);
    }
    else
    {
        nlStrNCpy<char>(FileName, pLookup->FileName, MaxLength);
    }
}

} // namespace AudioStreamTrack

/**
 * Offset/Address/Size: 0x2038 | 0x80156D90 | size: 0x104
 */
void AudioStreamTrack::TrackManagerBase::FadeManager::AddFade(
    GCAudioStreaming::StereoAudioStream* pStream, unsigned long startVol,
    unsigned long endVol, Audio::MasterVolume::VOLUME_GROUP volGroup,
    unsigned long fadeLength, const Function<FnVoidVoid>& callback)
{
    typedef STREAM_FADE_CTRL FadeCtrl;

    FadeCtrl* fadeCtrl = m_Fades.AllocateAtEnd(NULL);

    new (&fadeCtrl->Callback) Function<FnVoidVoid>();

    fadeCtrl->pStream = pStream;
    fadeCtrl->StartVol = startVol;
    fadeCtrl->EndVol = endVol;
    fadeCtrl->VolumeGroup = volGroup;
    fadeCtrl->FadeLength = fadeLength;
    fadeCtrl->Interp = 0.0f;

    fadeCtrl->Callback = callback;
}

extern "C" void sndStreamMixParameterEx(SND_STREAMID stid, unsigned char vol, unsigned char pan,
    unsigned char span, unsigned char auxa, unsigned char auxb);

void AudioStreamTrack::TrackManagerBase::FadeManager::CompleteFade(
    STREAM_FADE_CTRL* fadeCtrl)
{
    typedef DLListEntry<STREAM_FADE_CTRL> FadeEntry;

    Function<FnVoidVoid> callback(fadeCtrl->Callback);

    // PORT: was a literal 8, the payload offset, which is not 8 here.
    FadeEntry* entry =
        (FadeEntry*)((char*)fadeCtrl - offsetof(FadeEntry, entry));
    nlDLRingIsEnd(m_Fades.m_Head, entry);
    nlDLRingRemove(&m_Fades.m_Head, entry);

    m_Fades.DeleteEntry(entry);

    if (callback)
    {
        callback();
    }
}

/**
 * Offset/Address/Size: 0x1D60 | 0x80156AB8 | size: 0x2D8
 */
bool AudioStreamTrack::TrackManagerBase::FadeManager::ChangeFade(
    GCAudioStreaming::StereoAudioStream* pStream, unsigned long endVol,
    unsigned long fadeLength, const Function<FnVoidVoid>& callback)
{
    typedef STREAM_FADE_CTRL FadeCtrl;

    FadeCtrl* fadeCtrl = FindFade(pStream);

    if (fadeCtrl == NULL)
    {
        return false;
    }

    fadeCtrl->Callback = callback;

    unsigned long startVol = fadeCtrl->StartVol;
    unsigned long curEndVol = fadeCtrl->EndVol;
    int diff = (int)curEndVol - (int)startVol;
    float curVol = fadeCtrl->Interp * (float)diff + (float)startVol;

    if (endVol == (unsigned char)curVol)
    {
        CompleteFade(fadeCtrl);
    }
    else
    {
        fadeCtrl->StartVol = (int)curVol;
        fadeCtrl->Interp = 0.0f;
        fadeCtrl->EndVol = endVol;
        fadeCtrl->FadeLength = fadeLength;
    }

    return true;
}

static inline float ClampFadeInterp(float x)
{
    if (x <= 1.0f)
    {
        return x;
    }
    else
    {
        return 1.0f;
    }
}

/**
 * Offset/Address/Size: 0x1970 | 0x801566C8 | size: 0x3F0
 */
void AudioStreamTrack::TrackManagerBase::FadeManager::UpdateFade(STREAM_FADE_CTRL* pFade)
{
    unsigned long fadeLength = pFade->FadeLength;
    float totalTime;
    if (fadeLength != 0)
    {
        totalTime = (float)fadeLength;
    }
    else
    {
        totalTime = m_dT;
    }

    float& interp = pFade->Interp;
    interp = interp + (m_dT / totalTime);

    float newVal = ClampFadeInterp(pFade->Interp);
    pFade->Interp = newVal;

    float absDiff = (float)__fabs(pFade->Interp - 1.0f);
    if (absDiff <= 0.0001f)
    {
        float masterVol = Audio::MasterVolume::GetVolume(
            (Audio::MasterVolume::VOLUME_GROUP)pFade->VolumeGroup);

        unsigned long endVol = pFade->EndVol;

        pFade->pStream->SetVolume(
            (int)((float)endVol * masterVol));

        CompleteFade(pFade);
    }
    else
    {
        unsigned long startVol = pFade->StartVol;
        unsigned long endVol = pFade->EndVol;
        int diff = (int)endVol - (int)startVol;
        Audio::MasterVolume::VOLUME_GROUP vg = (Audio::MasterVolume::VOLUME_GROUP)pFade->VolumeGroup;

        float interpVol = pFade->Interp * (float)diff + (float)startVol;
        int interpVolInt = (int)interpVol;

        float masterVol = Audio::MasterVolume::GetVolume(vg);

        pFade->pStream->SetVolume(
            (int)((float)(u8)interpVolInt * masterVol));
    }
}

/**
 * Offset/Address/Size: 0x18FC | 0x80156654 | size: 0x74
 */
void AudioStreamTrack::StreamTrack::Update(float dT)
{
    DLListEntry<QUEUED_STREAM>* head = m_QueuedStreams.m_Head;
    if (!head)
    {
        return;
    }

    QUEUED_STREAM* qs = &m_QueuedStreams.Begin().m_Curr->entry;

    if (m_InFakePause)
    {
        return;
    }

    if (qs->pStream->m_State == 1)
    {
        StopQStream(qs);
        ProcessNewHeadStream();
    }
}

/**
 * Offset/Address/Size: 0x14D4 | 0x8015622C | size: 0x428
 */
void AudioStreamTrack::StreamTrack::PlayStream(
    unsigned long StreamId, float Volume, bool Looping,
    unsigned long FadeIn, unsigned long ExistingFadeOut,
    const char* StreamParam, Audio::MasterVolume::VOLUME_GROUP OverrideVolGroup)
{
    if (Config::Global().Get<bool>("no_stream", false) == true)
    {
        return;
    }

    TRACK_STATE StateAtStart = m_State;
    QueueStream(StreamId, Volume, Looping, FadeIn, StreamParam, OverrideVolGroup);

    if (StateAtStart != TS_Playing)
    {
        return;
    }

    QUEUED_STREAM Head;
    m_QueuedStreams.RemoveStart(&Head);

    QUEUED_STREAM Tail;
    m_QueuedStreams.Deallocate(nlDLRingRemoveEnd(&m_QueuedStreams.m_Head), &Tail);

    while (m_QueuedStreams.m_Head != NULL)
    {
        QUEUED_STREAM QStream;
        m_QueuedStreams.RemoveStart(&QStream);
        StopStream(QStream.pStream, QStream.TrackOwnsStream & 1);
    }

    m_QueuedStreams.AddStart(Head);
    m_QueuedStreams.AddEnd(Tail);

    QUEUED_STREAM* pHead = &m_QueuedStreams.Begin().m_Curr->entry;

    StartQStreamFadeout(pHead, ExistingFadeOut, Function<FnVoidVoid>(Bind<void>(MemFun<StreamTrack, void, QUEUED_STREAM*>(&StreamTrack::FadeOutDoneStartNext), this, pHead)));
}

inline GCAudioStreaming::AudioStream::AudioStream(GCAudioStreaming::AudioBufferMgr& mgr,
    unsigned long bufCount)
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

inline GCAudioStreaming::StereoAudioStream::StereoAudioStream(
    GCAudioStreaming::AudioBufferMgr& mgr)
    : AudioStream(mgr, 2)
{
    m_pFile = NULL;
    m_Interleave = 0;
}

/**
 * Offset/Address/Size: 0x10BC | 0x80155E14 | size: 0x418
 */
void AudioStreamTrack::StreamTrack::QueueStream(
    unsigned long StreamId, float Volume, bool Looping,
    unsigned long FadeIn, const char* StreamParam,
    Audio::MasterVolume::VOLUME_GROUP OverrideVolGroup)
{
    char FileName[256];

    if (Config::Global().Get<bool>("no_stream", false) == true)
    {
        return;
    }

    QUEUED_STREAM* queuedStream = m_QueuedStreams.AllocateAtEnd(NULL);
    queuedStream->StreamId = StreamId;

    TrackManagerBase& mgr = m_TrackMgr;
    GCAudioStreaming::StereoAudioStream* pStream = mgr.m_StreamPool.Allocate();
    new (pStream) GCAudioStreaming::StereoAudioStream(g_BufferMgr);

    queuedStream->pStream = pStream;
    queuedStream->FadeIn = FadeIn;
    queuedStream->StartVolume = (int)(127.0f * Volume);

    queuedStream->VolGroup = OverrideVolGroup == 0 ? m_VolumeGroup : OverrideVolGroup;
    queuedStream->Loop = Looping;
    queuedStream->TrackOwnsStream = m_TrackOwnsStreams;

    nlStrNCpy<char>(FileName, "audio/data/streams/", 0x100);
    unsigned long lookupKey = StreamId;
    unsigned long prefixLen;
    TrackManagerBase& lookupMgr = m_TrackMgr;
    TrackManagerBase::StreamFileLookup::STREAM_FILE_LOOKUP* lookup = nlBSearch<TrackManagerBase::StreamFileLookup::STREAM_FILE_LOOKUP, unsigned long>(
        lookupKey, lookupMgr.m_FileLookup.m_pLookup, lookupMgr.m_FileLookup.m_StreamCount);

    const char* percentPos = strchr(lookup->FileName, '%');
    if (percentPos != NULL)
    {
        prefixLen = (unsigned long)(percentPos - lookup->FileName);
        char* dest = &FileName[19];
        nlStrNCpy<char>(dest, lookup->FileName, prefixLen + 1);
        unsigned long remainLen = 0xed - prefixLen;
        dest += prefixLen;
        lookupMgr.m_FileLookup.m_ParamCB(StreamParam, dest, remainLen);
        unsigned long cbLen = nlStrLen(dest);
        nlStrNCpy<char>(dest + cbLen, percentPos + 3, remainLen - cbLen);
    }
    else
    {
        nlStrNCpy<char>(&FileName[19], lookup->FileName, 0xed);
    }

    queuedStream->pStream->Open(FileName);
    if (m_State == TS_Idle)
    {
        ProcessNewHeadStream();
    }
}

extern "C" bool sndStreamActivate(SND_STREAMID stid);
extern "C" void sndStreamDeactivate(SND_STREAMID stid);

/**
 * Offset/Address/Size: 0xE20 | 0x80155B78 | size: 0x29C
 */
void AudioStreamTrack::StreamTrack::ProcessNewHeadStream()
{
    if (m_QueuedStreams.m_Head == NULL)
    {
        SetIdleState();
        return;
    }

    DLListEntry<QUEUED_STREAM>* pEntry = m_QueuedStreams.Begin().m_Curr;

    if (pEntry->entry.StartVolume != 0)
    {
        m_TrackMgr.m_FadeMgr.AddFade(
            pEntry->entry.pStream,
            0,
            pEntry->entry.StartVolume,
            (Audio::MasterVolume::VOLUME_GROUP)pEntry->entry.VolGroup,
            pEntry->entry.FadeIn,
            Function<FnVoidVoid>());
    }

    pEntry->entry.pStream->SetVolume(0);
    GCAudioStreaming::StereoAudioStream* pStreamActive = pEntry->entry.pStream;

    pStreamActive->m_Flags = (pStreamActive->m_Flags & ~(1 << 1)) | (pEntry->entry.Loop << 1);
    pStreamActive->Play(true);

    m_State = TS_Playing;
}

void AudioStreamTrack::StreamTrack::SetIdleState()
{
    if (m_State != TS_Idle)
    {
        m_State = TS_Idle;
        if (m_IdleCallback)
        {
            m_IdleCallback();
        }
    }
}

/**
 * Offset/Address/Size: 0xC90 | 0x801559E8 | size: 0x190
 */
void AudioStreamTrack::StreamTrack::StopHead(unsigned long Fadeout)
{
    DLListEntry<QUEUED_STREAM>* entry = nlDLRingGetStart(m_QueuedStreams.m_Head);
    StreamTrack* pTrack = this;

    if (Fadeout == 0)
    {
        pTrack->StopQStream(&entry->entry);
    }
    else
    {
        QUEUED_STREAM* qs = &entry->entry;
        Function<FnVoidVoid> callback(Bind<void>(
            MemFun<StreamTrack, void, QUEUED_STREAM*>(&StreamTrack::FadeOutDoneStartNext), this, qs));
        StartQStreamFadeout(&entry->entry, Fadeout, callback);
    }
}

/**
 * Offset/Address/Size: 0xA28 | 0x80155780 | size: 0x268
 */
void AudioStreamTrack::StreamTrack::Stop(unsigned long Fadeout)
{
    DLListEntry<QUEUED_STREAM>* entry;

    if (m_InFakePause)
        return;

    if (Fadeout == 0)
    {
        while (m_QueuedStreams.m_Head != NULL)
        {
            StopQStream(&m_QueuedStreams.Begin().m_Curr->entry);
        }
        return;
    }

    if (m_QueuedStreams.m_Head == NULL)
        return;

    entry = nlDLRingGetStart(m_QueuedStreams.m_Head);
    QUEUED_STREAM* qs = &entry->entry;

    StartQStreamFadeout(&entry->entry, Fadeout, Function<FnVoidVoid>(Bind<void>(MemFun<StreamTrack, void, QUEUED_STREAM*>(&StreamTrack::FadeOutDone), this, &entry->entry)));

    QUEUED_STREAM* curQs;
    DLListEntry<QUEUED_STREAM>* iter = nlDLRingGetStart(m_QueuedStreams.m_Head);
    DLListEntry<QUEUED_STREAM>* head = m_QueuedStreams.m_Head;

    if (&iter->entry == qs)
    {
        if (nlDLRingIsEnd(head, iter) || iter == NULL)
        {
            iter = NULL;
        }
        else
        {
            iter = iter->m_next;
        }
    }

    while (iter != NULL)
    {
        curQs = &iter->entry;

        if (nlDLRingIsEnd(head, iter) || iter == NULL)
        {
            iter = NULL;
        }
        else
        {
            iter = iter->m_next;
        }

        StopQStream(curQs);
    }
}

/**
 * Offset/Address/Size: 0x94C | 0x801556A4 | size: 0xDC
 */
void AudioStreamTrack::StreamTrack::StartQStreamFadeout(
    QUEUED_STREAM* pQS, unsigned long Fadeout, const Function<FnVoidVoid>& callback)
{
    if (!m_TrackMgr.m_FadeMgr.ChangeFade(pQS->pStream, 0, Fadeout, callback))
    {
        GCAudioStreaming::StereoAudioStream* pStream = pQS->pStream;
        unsigned char vol = (unsigned char)pStream->m_Volume;
        if (vol != 0)
        {
            m_TrackMgr.m_FadeMgr.AddFade(pStream, vol, 0, (Audio::MasterVolume::VOLUME_GROUP)pQS->VolGroup, Fadeout, callback);
        }
        else
        {
            if (callback)
            {
                callback();
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x888 | 0x801555E0 | size: 0xC4
 */
void AudioStreamTrack::StreamTrack::StopQStream(QUEUED_STREAM* pQueuedStream)
{
    // PORT: was byte 0xB of the packed word, which is where these bits sit on console only.
    StopStream(pQueuedStream->pStream, pQueuedStream->TrackOwnsStream);

    // PORT: as above.
    DLListEntry<QUEUED_STREAM>* entry =
        (DLListEntry<QUEUED_STREAM>*)((char*)pQueuedStream
                                     - offsetof(DLListEntry<QUEUED_STREAM>, entry));

    nlDLRingIsEnd(m_QueuedStreams.m_Head, entry);
    nlDLRingRemove(&m_QueuedStreams.m_Head, entry);
    m_QueuedStreams.Deallocate(entry, NULL);

    if (m_QueuedStreams.m_Head == NULL)
    {
        SetIdleState();
    }
}

/**
 * Offset/Address/Size: 0x5B0 | 0x80155308 | size: 0x2D8
 */
void AudioStreamTrack::StreamTrack::StopStream(GCAudioStreaming::StereoAudioStream* pStream, bool TrackOwns)
{
    pStream->Stop();
    ReleaseStream(pStream, TrackOwns);
}

/**
 * Offset/Address/Size: 0x590 | 0x801552E8 | size: 0x20
 */
void AudioStreamTrack::StreamTrack::FadeOutDone(AudioStreamTrack::StreamTrack::QUEUED_STREAM* qs)
{
    StopQStream(qs);
}

/**
 * Offset/Address/Size: 0x55C | 0x801552B4 | size: 0x34
 */
void AudioStreamTrack::StreamTrack::FadeOutDoneStartNext(AudioStreamTrack::StreamTrack::QUEUED_STREAM* qs)
{
    FadeOutDone(qs);
    ProcessNewHeadStream();
}

void AudioStreamTrack::StreamTrack::UpdateLPF()
{
}

/**
 * Offset/Address/Size: 0x1E8 | 0x80154F40 | size: 0x374
 */
void AudioStreamTrack::StreamTrack::Pause(unsigned long Fadeout, bool bPause)
{
    m_InFakePause = 1;

    QUEUED_STREAM* qs = m_QueuedStreams.GetHead();

    if (qs == NULL)
    {
        return;
    }

    unsigned long endVol;
    if (m_TrackMgr.m_FadeMgr.IsFading(qs->pStream, &endVol) && endVol == 0)
    {
        StopHead(Fadeout);
        return;
    }

    m_TrackMgr.m_FadeMgr.RemoveFade(qs->pStream);

    if (bPause)
    {
        return;
    }

    GCAudioStreaming::StereoAudioStream* stream = qs->pStream;

    stream->StopPlaying();

    stream->CancelPendingReads();

    stream->Cool();
}

/**
 * Offset/Address/Size: 0x1B8 | 0x80154F10 | size: 0x30
 */
void AudioStreamTrack::StreamTrack::Resume()
{
    m_InFakePause = 0;
    ProcessNewHeadStream();
}

/**
 * Offset/Address/Size: 0x0 | 0x80154D58 | size: 0x1B8
 */
void AudioStreamTrack::StreamTrack::AttachStream(
    GCAudioStreaming::StereoAudioStream* pStream,
    Audio::MasterVolume::VOLUME_GROUP VolGroup,
    unsigned long StreamId,
    unsigned long FadeIn,
    bool Looping,
    bool TrackOwnsStream)
{
    if (m_State != TS_Idle)
    {
        return;
    }

    if (Config::Global().Get<bool>("no_stream", false) == true)
    {
        return;
    }

    QUEUED_STREAM* queuedStream = m_QueuedStreams.AllocateAtEnd(NULL);

    queuedStream->StreamId = StreamId;
    queuedStream->pStream = pStream;
    queuedStream->FadeIn = FadeIn;
    queuedStream->StartVolume = (u8)pStream->m_Volume;
    queuedStream->Loop = Looping;
    queuedStream->VolGroup = VolGroup;
    queuedStream->TrackOwnsStream = TrackOwnsStream;

    m_State = TS_Playing;
}
