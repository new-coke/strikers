#ifndef _STREAMTRACK_H_
#define _STREAMTRACK_H_

#include "NL/nlString.h"
#include "Game/Sys/audio.h"
#include "NL/nlFunction.h"
#include "NL/nlDLListContainer.h"
#include "NL/nlSlotPool.h"
#include "NL/nlSortedSlot.h"
#include "NL/WalkHelper.h"

// Defined in Game/Audio/audio.cpp.
extern f32 gfVolumeGroups[23];

namespace GCAudioStreaming
{
class StereoAudioStream;
}

namespace Audio
{
bool TrackMgrFileNameParamLookup(const char* param, char* out, unsigned long size);
} // namespace Audio

namespace AudioStreamTrack
{

enum TRACK_STATE
{
    TS_Idle = 0,
    TS_Playing = 1,
};

class StreamTrack;

class TrackManagerBase
{
public:
    TrackManagerBase();
    TrackManagerBase(const Function<bool(const char*, char*, unsigned long)>&);

    class StreamFileLookup
    {
    public:
        struct STREAM_FILE_LOOKUP
        {
            /* 0x00 */ unsigned long NameHash;
            /* 0x04 */ const char* FileName;

            operator unsigned long() const { return NameHash; }
        }; // total size: 0x8

        struct STREAM_FILE_LIST_LOOKUP : public STREAM_FILE_LOOKUP
        {
            /* 0x08 */ unsigned long FileNameLen;
        }; // total size: 0xC

        StreamFileLookup(const char* name,
            const Function<bool(const char*, char*, unsigned long)>& ParamCB);
        void GetFileName(unsigned long StreamId, char* FileName, int MaxLength,
            const char* Param);

        /* 0x00 */ Function<bool(const char*, char*, unsigned long)> m_ParamCB;
        /* 0x08 */ STREAM_FILE_LOOKUP* m_pLookup;
        /* 0x0C */ unsigned long m_StreamCount;
        /* 0x10 */ char* m_pStrings;
    }; // total size: 0x14

    typedef DLListEntry<GCAudioStreaming::StereoAudioStream*> StreamDeleteEntry;
    typedef BasicSlotPool<StreamDeleteEntry> StreamDeleteAllocator;
    typedef DLListContainerBase<GCAudioStreaming::StereoAudioStream*, StreamDeleteAllocator> StreamDeleteList;

    void AddDeleteStream(
        GCAudioStreaming::StereoAudioStream* stream,
        StreamDeleteEntry*& entry)
    {
        m_StreamDeleteList.m_Allocator.Allocate(entry);
        if (entry != NULL)
        {
            entry->m_next = NULL;
            entry->m_prev = NULL;
            entry->entry = stream;
        }
        nlDLRingAddEnd(&m_StreamDeleteList.m_Head, entry);
    }

    class FadeManager
    {
    public:
        struct STREAM_FADE_CTRL
        {
            STREAM_FADE_CTRL()
                : Callback()
            {
            }

            /* 0x00 */ Function<FnVoidVoid> Callback;
            /* 0x08 */ GCAudioStreaming::StereoAudioStream* pStream;
            /* 0x0C */ float Interp;
            /* 0x10 */ unsigned long FadeLength : 14;
            /* 0x10 */ unsigned long StartVol : 7;
            /* 0x10 */ unsigned long EndVol : 7;
            /* 0x10 */ unsigned long VolumeGroup : 3;
        }; // total size: 0x14

        typedef nlDLListSlotPool<STREAM_FADE_CTRL> FadeList;

        void Update(float dT);
        void Clear()
        {
            m_Fades.Clear();
            m_Fades.m_Allocator.FreeBlocks();
        }
        ~FadeManager()
        {
            Clear();
        }
        void CompleteFade(STREAM_FADE_CTRL* fadeCtrl);
        void UpdateFade(STREAM_FADE_CTRL* pFade);
        bool ChangeFade(GCAudioStreaming::StereoAudioStream* pStream, unsigned long endVol, unsigned long fadeLength,
            const Function<FnVoidVoid>& callback);
        void AddFade(GCAudioStreaming::StereoAudioStream* pStream, unsigned long startVol, unsigned long endVol,
            Audio::MasterVolume::VOLUME_GROUP volGroup, unsigned long fadeLength, const Function<FnVoidVoid>& callback);

        STREAM_FADE_CTRL* FindFade(GCAudioStreaming::StereoAudioStream*);
        bool IsFading(
            GCAudioStreaming::StereoAudioStream*,
            unsigned long* = NULL);
        void RemoveFade(GCAudioStreaming::StereoAudioStream*);

        /* 0x00 */ FadeList m_Fades;
        /* 0x1C */ float m_dT;
    }; // total size: 0x20

    virtual ~TrackManagerBase()
    {
        m_StreamPool.FreeBlocks();
    }

    void PurgeStreams()
    {
        StreamDeleteEntry** headAddr;
        StreamDeleteEntry* entry;
        StreamDeleteEntry* head;
        StreamDeleteEntry* toRemove;
        StreamDeleteEntry* toFree;
        GCAudioStreaming::StereoAudioStream* pStream;

        StreamDeleteEntry* start = nlDLRingGetStart(m_StreamDeleteList.m_Head);
        head = m_StreamDeleteList.m_Head;
        headAddr = &m_StreamDeleteList.m_Head;
        entry = start;

        while (entry != NULL)
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
    }

    /* 0x0C */ virtual void Update(float dT);
    /* 0x10 */ virtual StreamTrack* CreateTrack(const char*, Audio::MasterVolume::VOLUME_GROUP) = 0;
    /* 0x14 */ virtual void DestroyAllTracks() = 0;
    static TrackManagerBase* Get();

    /* 0x18 */ virtual StreamTrack* GetTrack(unsigned long) = 0;
    /* 0x1C */ virtual void StopAllTracks(unsigned long) = 0;
    /* 0x20 */ virtual void OnMasterVolumeChange(Audio::MasterVolume::VOLUME_GROUP) = 0;

    /* 0x04 */ StreamFileLookup m_FileLookup;
    /* 0x18 */ FadeManager m_FadeMgr;
    /* 0x38 */ SlotPool<GCAudioStreaming::StereoAudioStream> m_StreamPool;
    /* 0x50 */ nlDLListSlotPool<GCAudioStreaming::StereoAudioStream*> m_StreamDeleteList;
}; // total size: 0x6C

inline TrackManagerBase::FadeManager::STREAM_FADE_CTRL*
TrackManagerBase::FadeManager::FindFade(
    GCAudioStreaming::StereoAudioStream* stream)
{
    typedef DLListEntry<STREAM_FADE_CTRL> FadeEntry;

    FadeEntry* current = nlDLRingGetStart(m_Fades.m_Head);
    nlDLListIterator<STREAM_FADE_CTRL> iter;
    iter.m_Head = m_Fades.m_Head;
    iter.m_Curr = current;
    while (iter.hasNext())
    {
        FadeEntry* entry = iter.m_Curr;
        if (entry->entry.pStream == stream)
        {
            return &entry->entry;
        }
        iter.next();
    }
    return NULL;
}

inline bool TrackManagerBase::FadeManager::IsFading(
    GCAudioStreaming::StereoAudioStream* stream,
    unsigned long* endVolume)
{
    STREAM_FADE_CTRL* fadeCtrl = FindFade(stream);
    bool result;
    if (fadeCtrl != NULL)
    {
        if (endVolume != NULL)
        {
            *endVolume = fadeCtrl->EndVol;
        }
        result = true;
    }
    else
    {
        result = false;
    }
    return result;
}

inline void TrackManagerBase::FadeManager::RemoveFade(
    GCAudioStreaming::StereoAudioStream* stream)
{
    typedef DLListEntry<STREAM_FADE_CTRL> FadeEntry;

    STREAM_FADE_CTRL* fadeCtrl = FindFade(stream);
    if (fadeCtrl != NULL)
    {
        // PORT: was a literal 8, the payload offset, which is not 8 here.
    FadeEntry* entry =
        (FadeEntry*)((char*)fadeCtrl - offsetof(FadeEntry, entry));
        nlDLRingIsEnd(m_Fades.m_Head, entry);
        nlDLRingRemove(&m_Fades.m_Head, entry);
        m_Fades.DeleteEntry(entry);
    }
}

inline void TrackManagerBase::FadeManager::Update(float dT)
{
    typedef WalkHelper<STREAM_FADE_CTRL, DLListEntry<STREAM_FADE_CTRL>, FadeManager> FadeWalkHelper;
    typedef void (FadeWalkHelper::*WalkCBType)(DLListEntry<STREAM_FADE_CTRL>*);

    m_dT = dT * 1000.0f;

    FadeWalkHelper helper;
    helper.m_CBClass = this;
    helper.m_CB = &FadeManager::UpdateFade;
    WalkCBType callback = &FadeWalkHelper::Callback;
    nlWalkDLRing(m_Fades.m_Head, &helper, callback);
}

class StreamTrack
{
public:
    struct QUEUED_STREAM
    {
        /* 0x0 */ unsigned long StreamId;
        /* 0x4 */ GCAudioStreaming::StereoAudioStream* pStream;
        /* 0x8 */ unsigned long FadeIn : 16;
        /* 0x8 */ unsigned long StartVolume : 10;
        /* 0x8 */ unsigned long VolGroup : 3;
        /* 0xB */ bool Loop : 1;
        /* 0xB */ bool TrackOwnsStream : 1;
    }; // total size: 0xC

    StreamTrack(TrackManagerBase& mgr, Audio::MasterVolume::VOLUME_GROUP volumeGroup);
    ~StreamTrack() { }

    void Update(float dT);
    void PlayStream(unsigned long StreamId, float Volume, bool Looping, unsigned long FadeIn, unsigned long ExistingFadeOut, const char* StreamParam, Audio::MasterVolume::VOLUME_GROUP OverrideVolGroup);
    void QueueStream(unsigned long StreamId, float Volume, bool Looping, unsigned long FadeIn, const char* StreamParam, Audio::MasterVolume::VOLUME_GROUP OverrideVolGroup);
    void ProcessNewHeadStream();
    void SetIdleState();
    void StopHead(unsigned long Fadeout);
    void Stop(unsigned long Fadeout);
    void StopQStream(QUEUED_STREAM* pQueuedStream);
    void StopStream(GCAudioStreaming::StereoAudioStream* pStream, bool TrackOwns);
    void ReleaseStream(GCAudioStreaming::StereoAudioStream*, bool);
    void FadeOutDone(QUEUED_STREAM* qs);
    void FadeOutDoneStartNext(QUEUED_STREAM* qs);
    void StartQStreamFadeout(QUEUED_STREAM* pQS, unsigned long Fadeout, const Function<FnVoidVoid>& callback);
    void Pause(unsigned long Fadeout, bool bPause);
    void UpdateLPF();
    void Resume();
    void AttachStream(GCAudioStreaming::StereoAudioStream* pStream, Audio::MasterVolume::VOLUME_GROUP VolGroup, unsigned long StreamId, unsigned long FadeIn, bool Looping, bool TrackOwnsStream);

    static const unsigned long MAX_QUEUED_STREAMS = 4;

    /* 0x00 */ TrackManagerBase& m_TrackMgr;
    /* 0x04 */ DLListContainerBase<QUEUED_STREAM, nlStaticArrayAllocator<DLListEntry<QUEUED_STREAM>, MAX_QUEUED_STREAMS> > m_QueuedStreams;
    /* 0x5C */ unsigned long m_LPFFreq;
    /* 0x60 */ bool m_LPFOn : 1;
    /* 0x60 */ bool m_InFakePause : 1;
    /* 0x60 */ bool m_TrackOwnsStreams : 1;
    /* 0x64 */ TRACK_STATE m_State;
    /* 0x68 */ Audio::MasterVolume::VOLUME_GROUP m_VolumeGroup;
    /* 0x6C */ Function<FnVoidVoid> m_IdleCallback;
}; // total size: 0x74

inline void StreamTrack::ReleaseStream(
    GCAudioStreaming::StereoAudioStream* stream, bool ownsStream)
{
    m_TrackMgr.m_FadeMgr.RemoveFade(stream);
    if (ownsStream)
    {
        TrackManagerBase::StreamDeleteEntry* entry = NULL;
        TrackManagerBase* delMgr = &m_TrackMgr;
        delMgr->AddDeleteStream(stream, entry);
    }
}

inline StreamTrack::StreamTrack(TrackManagerBase& mgr, Audio::MasterVolume::VOLUME_GROUP volumeGroup)
    : m_TrackMgr(mgr)
    , m_QueuedStreams(0)
    , m_LPFFreq(32000)
    , m_LPFOn(false)
    , m_InFakePause(false)
    , m_TrackOwnsStreams(true)
    , m_State(TS_Idle)
    , m_VolumeGroup(volumeGroup)
{
}

inline TrackManagerBase::TrackManagerBase()
    : m_FileLookup("audio/data/streams/StreamNames.txt", Audio::TrackMgrFileNameParamLookup)
    , m_StreamPool(16, 16)
{
}

inline TrackManagerBase::TrackManagerBase(const Function<bool(const char*, char*, unsigned long)>& fn)
    : m_FileLookup("audio/data/streams/StreamNames.txt", fn)
    , m_StreamPool(16, 16)
{
}

template <int N>
class TrackManager : public TrackManagerBase
{
public:
    TrackManager(const Function<bool(const char*, char*, unsigned long)>&);
    virtual ~TrackManager() { }
    virtual StreamTrack* CreateTrack(
        const char* Name, Audio::MasterVolume::VOLUME_GROUP VolumeGroup)
    {
        unsigned long hash = nlStringLowerHash(Name);
        return new (m_Tracks.AddEntry(hash)) StreamTrack(*this, VolumeGroup);
    }
    virtual void DestroyAllTracks()
    {
        StreamTrack* track;

        StopAllTracks(0);

        while (m_Tracks.GetEntryCount() != 0)
        {
            track = m_Tracks.m_pEntryLookup->pEntry;
            if (track == NULL)
                continue;

            if (track)
            {
                track->m_InFakePause = 0;
                track->Stop(0);
                track->m_IdleCallback.~Function();
                track->m_QueuedStreams.~DLListContainerBase();
            }

            if (track == NULL)
                continue;

            m_Tracks.DeleteEntry(track);
        }
    }
    virtual void Update(float dT)
    {
        unsigned long track;

        m_FadeMgr.Update(dT);

        // PORT: subscript, not byte arithmetic, EntryLookup is 16 bytes here.
        for (track = 0; track < m_Tracks.GetEntryCount(); track++)
        {
            ((typename nlSortedSlot<StreamTrack, N>::template EntryLookup<StreamTrack>*)
                 m_Tracks.m_pEntryLookup)[track]
                .pEntry->Update(dT);
        }

        TrackManagerBase::Update(dT);
    }
    virtual void StopAllTracks(unsigned long FadeOut)
    {
        for (unsigned long track = 0; track < m_Tracks.GetEntryCount(); track++)
        {
            m_Tracks.m_pEntryLookup[track].pEntry->Stop(FadeOut);
        }
    }
    virtual StreamTrack* GetTrack(unsigned long Name)
    {
        typedef typename nlSortedSlot<StreamTrack, N>::template EntryLookup<StreamTrack> EntryLookup;

        EntryLookup* result;
        if (m_Tracks.GetEntryCount() != 0)
        {
            result = nlBSearch(Name, m_Tracks.m_pEntryLookup, m_Tracks.GetEntryCount());
        }
        else
        {
            result = NULL;
        }
        if (result != NULL)
        {
            return result->pEntry;
        }
        return NULL;
    }
    virtual void OnMasterVolumeChange(Audio::MasterVolume::VOLUME_GROUP VolumeGroup)
    {
        unsigned long track;
        float vol = ::gfVolumeGroups[VolumeGroup];

        // PORT: subscript, not byte arithmetic, see Update above.
        for (track = 0; track < m_Tracks.GetEntryCount(); track++)
        {
            StreamTrack::QUEUED_STREAM* qs
                = ((typename nlSortedSlot<StreamTrack, N>::template EntryLookup<StreamTrack>*)
                       m_Tracks.m_pEntryLookup)[track]
                      .pEntry->m_QueuedStreams.GetHead();

            if (qs != NULL)
            {
                if (m_FadeMgr.FindFade(qs->pStream) == NULL
                    && (Audio::MasterVolume::VOLUME_GROUP)qs->VolGroup == VolumeGroup)
                {
                    qs->pStream->SetVolume((unsigned long)(s32)(vol * (float)qs->StartVolume));
                }
            }
        }
    }

    /* 0x6C */ nlStaticSortedSlot<StreamTrack, N> m_Tracks;
};

template <int N>
inline TrackManager<N>::TrackManager(const Function<bool(const char*, char*, unsigned long)>& fn)
    : TrackManagerBase(fn)
    , m_Tracks()
{
}

inline TrackManagerBase* TrackManagerBase::Get()
{
    return g_pTrackManager;
}

} // namespace AudioStreamTrack

#endif // _STREAMTRACK_H_
