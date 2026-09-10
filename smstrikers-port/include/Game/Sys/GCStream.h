#ifndef _GCSTREAM_H_
#define _GCSTREAM_H_

#include "NL/nlArrayAllocator.h"
#include "NL/nlWare.h"
#include "NL/nlFile.h"
#include "NL/nlFileGC.h"
#include "musyx/musyx.h"
#include "dolphin/os.h"

// Declarations needed by the inline (weak) AudioStream methods defined below,
// so this header compiles standalone in every includer.
void nlServiceFileSystem();
extern "C" void sndStreamLPFParameter(unsigned long, bool, unsigned long);

struct sDSPADPCM
{
    /* 0x00 */ u32 num_samples;
    /* 0x04 */ u32 num_adpcm_nibbles;
    /* 0x08 */ u32 sample_rate;
    /* 0x0C */ unsigned short loop_flag;
    /* 0x0E */ unsigned short format;
    /* 0x10 */ u32 sa;
    /* 0x14 */ u32 ea;
    /* 0x18 */ u32 ca;
    /* 0x1C */ unsigned short coef[16];
    /* 0x3C */ unsigned short gain;
    /* 0x3E */ unsigned short ps;
    /* 0x40 */ unsigned short yn1;
    /* 0x42 */ unsigned short yn2;
    /* 0x44 */ unsigned short lps;
    /* 0x46 */ unsigned short lyn1;
    /* 0x48 */ unsigned short lyn2;
    /* 0x4A */ unsigned short pad[11];
}; // total size: 0x60

struct INTERLEAVED_ADPCM_HEADER
{
    /* 0x0 */ char Thumbprint[4];
    /* 0x4 */ u32 Interleave;
    /* 0x8 */ u32 StreamLength;
}; // total size: 0xC

namespace GCAudioStreaming
{

class AudioStream;

class AudioStreamBuffer
{
public:
    AudioStreamBuffer()
    {
        m_Volume = 0x7F;
        m_Pan = 0x40;
    }
    unsigned char* GetMRAMBuffer()
    {
        unsigned char* result = m_MRAMBuffer;
        return result;
    }
    void SetVolume(unsigned long volume);
    void SetLPF(bool on)
    {
        if (on != m_bLPFOn)
        {
            sndStreamLPFParameter(m_StreamId, on, m_LPFFreq);
            m_bLPFOn = on;
        }
    }
    void SetLPF(unsigned short frequency)
    {
        if (m_bLPFOn)
        {
            sndStreamLPFParameter(m_StreamId, m_bLPFOn, frequency);
        }
        m_LPFFreq = frequency;
    }
    void Reset(AudioStream* pStream = NULL)
    {
        m_pStream = pStream;
        m_UpdateOffset = 0;
        m_Volume = 0x7F;
        m_Pan = 0x40;
    }
    void Update(unsigned long Offset, unsigned long Length);
    unsigned long UpdateHandler(unsigned long LengthA, unsigned long LengthB);
    unsigned long DoUpdate(unsigned long Length);
    static inline u32 _UpdateHandler(
        void*, u32 LengthA, void*, u32 LengthB,
        size_t user);

    /* 0x00 */ unsigned char* m_MRAMBuffer;   // offset 0x0, size 0x4
    /* 0x04 */ unsigned long m_BufferSize;    // offset 0x4, size 0x4
    /* 0x08 */ unsigned long m_BufferSamples; // offset 0x8, size 0x4
    /* 0x0C */ unsigned long m_StreamId;      // offset 0xC, size 0x4
    /* 0x10 */ unsigned long m_UpdateOffset;  // offset 0x10, size 0x4
    /* 0x14 */ class AudioStream* m_pStream;  // offset 0x14, size 0x4
    /* 0x18 */ unsigned char m_Volume;        // offset 0x18, size 0x1
    /* 0x19 */ signed char m_Pan;             // offset 0x19, size 0x1
    /* 0x1A */ unsigned char m_SurroundPan;   // offset 0x1A, size 0x1
    /* 0x1B */ bool m_bLPFOn;                 // offset 0x1B, size 0x1
    /* 0x1C */ unsigned short m_LPFFreq;      // offset 0x1C, size 0x2
}; // total size: 0x20

enum BUFFER_ALLOC_STATE
{
    BAS_Busy = 0,
    BAS_Free = 1,
};

class AudioBufferMgr
{
public:
    AudioBufferMgr()
        : m_MRAMBuffer(NULL)
    {
        m_BuffersFree = 0;
        m_BufferCount = 0;
    }
    void* GetADPCMHdr();
    void SetBufferState(unsigned long index, BUFFER_ALLOC_STATE state)
    {
        m_BuffersFree = (m_BuffersFree & ~(1 << index)) | (state << index);
    }
    void Init(unsigned long BufferPoolSize);
    void CreateBuffers(unsigned long Count);
    void DeleteBuffers();
    void FreeBuffer(GCAudioStreaming::AudioStreamBuffer* pBuffer);
    AudioStreamBuffer* GetFreeBuffer(GCAudioStreaming::AudioStream* pStream);

    static const unsigned long MAX_BUFFERS = 8;

    /* 0x000 */ unsigned long m_PoolSize;                 // offset 0x0, size 0x4
    /* 0x004 */ unsigned char* m_MRAMBuffer;              // offset 0x4, size 0x4
    /* 0x008 */ AudioStreamBuffer m_Buffers[MAX_BUFFERS]; // offset 0x8, size 0x100
    /* 0x108 */ unsigned char m_ADPCMHdrMem[128];         // offset 0x108, size 0x80
    /* 0x188 */ unsigned long m_BuffersFree;              // offset 0x188, size 0x4
    /* 0x18C */ unsigned long m_BufferCount;              // offset 0x18C, size 0x4
    /* 0x190 */ unsigned long m_BufferSize;               // offset 0x190, size 0x4
}; // total size: 0x194

enum STREAM_STATE
{
    SS_New = 0,
    SS_Initd = 1,
    SS_Warming = 2,
    SS_Warm = 3,
    SS_Playing = 4,
};

enum STREAM_FLAG
{
    SF_Play = 0,
    SF_Loop = 1,
    SF_CoolOnStop = 2,
    SF_EndAtUpdate = 3,
    SF_SeriousStop = 4,
};

class AudioStream
{
public:
    class BufferCursor
    {
    public:
        BufferCursor(AudioStream& stream, unsigned long& index)
            : m_Stream(stream)
            , m_Index(index)
        {
        }

        AudioStreamBuffer* GetBuffer()
        {
            return m_Stream.GetBuffer(m_Index);
        }

        unsigned long GetIndex()
        {
            return m_Index;
        }

        unsigned long Advance()
        {
            return ++m_Index;
        }

    private:
        AudioStream& m_Stream;
        unsigned long& m_Index;
    };

    void SetFlag(STREAM_FLAG flag, bool value)
    {
        m_Flags = (m_Flags & ~(1 << flag))
                | ((unsigned long)value << flag);
    }
    void SetBuffer(unsigned long index, AudioStreamBuffer* buffer)
    {
        m_Buffers[index] = buffer;
    }
    void AllocBuffer()
    {
        SetBuffer(0, m_BuffMgr.GetFreeBuffer(this));
    }
    void* GetADPCMHdr()
    {
        return m_BuffMgr.GetADPCMHdr();
    }
    bool IsSeriousStop()
    {
        bool serious;
        if (m_Flags & (1 << SF_SeriousStop))
        {
            switch (m_State)
            {
            case SS_New:
            case SS_Initd:
                break;
            case SS_Warming:
                m_State = SS_Warm;
                break;
            case SS_Warm:
            case SS_Playing:
                break;
            }
            serious = true;
        }
        else
        {
            serious = false;
        }
        return serious;
    }
    AudioStreamBuffer* GetBuffer(unsigned long index)
    {
        AudioStreamBuffer* buffer;
        if (index < m_BufferCount)
        {
            buffer = m_Buffers[index];
        }
        else
        {
            buffer = NULL;
        }
        return buffer;
    }
    void SetVolume(unsigned long volume)
    {
        int clampedVolume = ((u32)(u8)volume <= 0x7Fu) ? volume : 0x7F;

        if (m_State >= SS_Warming)
        {
            AudioStreamBuffer* bufferIndex = NULL;
            AudioStreamBuffer* buffer = GetBuffer((uintptr_t)bufferIndex);

            while (buffer != NULL)
            {
                buffer->m_Volume = (u8)clampedVolume;
                sndStreamMixParameterEx(buffer->m_StreamId, buffer->m_Volume, buffer->m_Pan, buffer->m_SurroundPan, 0, 0);
                ((unsigned long&)bufferIndex)++;
                buffer = GetBuffer((uintptr_t)bufferIndex);
            }
        }

        m_Volume = (u8)clampedVolume;
    }
    void SetLPF(bool on)
    {
        if (m_State >= SS_Warming)
        {
            AudioStreamBuffer* bufferIndex = NULL;
            AudioStreamBuffer* buffer = GetBuffer((uintptr_t)bufferIndex);

            while (buffer != NULL)
            {
                buffer->SetLPF(on);
                ((unsigned long&)bufferIndex)++;
                buffer = GetBuffer((uintptr_t)bufferIndex);
            }
        }

        m_LPFOn = on;
    }
    void SetLPF(unsigned short frequency)
    {
        if (m_State >= SS_Warming)
        {
            AudioStreamBuffer* bufferIndex = NULL;
            AudioStreamBuffer* buffer = GetBuffer((uintptr_t)bufferIndex);

            while (buffer != NULL)
            {
                buffer->SetLPF(frequency);
                ((unsigned long&)bufferIndex)++;
                buffer = GetBuffer((uintptr_t)bufferIndex);
            }
        }

        m_LPFFreq = frequency;
    }
    void SetLoop(bool loop)
    {
        m_Flags = (m_Flags & ~(1 << SF_Loop)) | ((unsigned long)loop << SF_Loop);
    }
    void Play(bool coolOnStop)
    {
        m_Flags = (m_Flags & ~(1 << SF_CoolOnStop)) | ((unsigned long)coolOnStop << SF_CoolOnStop);

        switch (m_State)
        {
        case SS_Initd:
            m_Flags = (m_Flags & ~(1 << SF_Play)) | (1 << SF_Play);
            Warm(coolOnStop);
            break;
        case SS_Warming:
            m_Flags = (m_Flags & ~(1 << SF_Play)) | (1 << SF_Play);
            break;
        case SS_Warm:
        {
            AudioStreamBuffer* bufferIndex = NULL;
            AudioStreamBuffer* buffer = GetBuffer((uintptr_t)bufferIndex);

            while (buffer != NULL)
            {
                sndStreamActivate(buffer->m_StreamId);
                ((unsigned long&)bufferIndex)++;
                buffer = GetBuffer((uintptr_t)bufferIndex);
            }

            m_State = SS_Playing;
            break;
        }
        }
    }
    void ResetBuffers()
    {
        AudioStreamBuffer* buf;
        AudioStreamBuffer* BufferIndex = NULL;

        buf = GetBuffer(0);

        while (buf != NULL)
        {
            m_Buffers[(uintptr_t)BufferIndex] = NULL;
            ((unsigned long&)BufferIndex)++;
            buf = GetBuffer((uintptr_t)BufferIndex);
        }
    }
    void Stop()
    {
        SetFlag(SF_Play, false);
        if (m_State == SS_Playing)
        {
            unsigned long bufferIndex = 0;
            BufferCursor cursor(*this, bufferIndex);
            AudioStreamBuffer* pBuffer = GetBuffer(cursor.GetIndex());
            while (pBuffer != NULL)
            {
                pBuffer->m_Volume = 0;
                sndStreamMixParameterEx(
                    pBuffer->m_StreamId,
                    pBuffer->m_Volume,
                    pBuffer->m_Pan,
                    pBuffer->m_SurroundPan,
                    0,
                    0);
                sndStreamDeactivate(pBuffer->m_StreamId);
                m_State = SS_Warm;

                cursor.Advance();
                pBuffer = GetBuffer(cursor.GetIndex());
            }
            m_StreamPos = 0;
            m_State = SS_Warm;
        }

        CancelPendingReads();
        Cool();
    }
    AudioStream(AudioBufferMgr& mgr, unsigned long bufCount);
    inline virtual ~AudioStream();
    virtual void Warm(bool) = 0;
    virtual bool SafeToPurge() = 0;
    inline virtual void Purge();
    virtual unsigned long DoUpdateRead(unsigned long, unsigned long, unsigned long, unsigned long, AudioStreamBuffer*) = 0;
    virtual unsigned long GetUpdateReadLength() = 0;
    virtual void CancelPendingReads() = 0;
    void ActivateBuffers()
    {
        unsigned long bufferIndex = 0;
        BufferCursor cursor(*this, bufferIndex);
        AudioStreamBuffer* buffer = cursor.GetBuffer();

        while (buffer != NULL)
        {
            sndStreamActivate(buffer->m_StreamId);
            cursor.Advance();
            buffer = cursor.GetBuffer();
        }
    }
    inline virtual void WarmReadDone(AudioStreamBuffer* pBuffer);
    void Cool()
    {
        if (m_Flags & (1 << SF_CoolOnStop))
        {
            SetFlag(SF_CoolOnStop, false);
            if (m_State > SS_Initd)
            {
                AudioStreamBuffer* buffer = NULL;
                AudioStreamBuffer* bufferIndex = NULL;
                m_Flags = (m_Flags & ~(1 << SF_SeriousStop))
                        | (1 << SF_SeriousStop);

                if (m_BufferCount > (uintptr_t)buffer)
                {
                    buffer = m_Buffers[0];
                }
                else
                {
                    buffer = NULL;
                }
                while (buffer != NULL)
                {
                    m_BuffMgr.FreeBuffer(buffer);
                    m_Buffers[(uintptr_t)bufferIndex] = NULL;
                    ((unsigned long&)bufferIndex)++;
                    buffer = GetBuffer((uintptr_t)bufferIndex);
                }

                m_State = SS_Initd;
            }
        }
    }
    void StopPlaying()
    {
        m_Flags &= ~(1 << SF_Play);
        if (m_State == SS_Playing)
        {
            AudioStreamBuffer* pBuffer;
            AudioStreamBuffer* bufferIndex = NULL;
            pBuffer = GetBuffer((uintptr_t)bufferIndex);
            while (pBuffer != NULL)
            {
                pBuffer->m_Volume = 0;
                sndStreamMixParameterEx(
                    pBuffer->m_StreamId,
                    pBuffer->m_Volume,
                    pBuffer->m_Pan,
                    pBuffer->m_SurroundPan,
                    0,
                    0);
                sndStreamDeactivate(pBuffer->m_StreamId);
                m_State = SS_Warm;

                ((unsigned long&)bufferIndex)++;
                pBuffer = GetBuffer((uintptr_t)bufferIndex);
            }
            m_StreamPos = 0;
            m_State = SS_Warm;
        }
    }
    void ReleaseBuffers()
    {
        unsigned long bufferIndex;
        AudioStreamBuffer* pBuffer = NULL;
        bufferIndex = 0;
        BufferCursor cursor(*this, bufferIndex);

        SetFlag(SF_SeriousStop, true);
        unsigned long start = 0;

        if (start < m_BufferCount)
        {
            pBuffer = m_Buffers[0];
        }

        while (pBuffer != NULL)
        {
            m_BuffMgr.FreeBuffer(pBuffer);

            unsigned long idx = cursor.GetIndex();
            m_Buffers[idx] = NULL;
            idx = cursor.Advance();
            pBuffer = cursor.GetBuffer();
        }
    }

    void Destructor()
    {
        SetFlag(SF_SeriousStop, true);
        Stop();

        if (m_State > SS_Initd)
        {
            ReleaseBuffers();
            m_State = SS_Initd;
        }

        long long startTime = OSGetTime();

        while (!SafeToPurge())
        {
            nlServiceFileSystem();
            OSYieldThread();
            long long elapsed = OSGetTime() - startTime;
            if (OSTicksToMilliseconds(elapsed) > 250)
            {
                nlPrintf("WARNING! Breaking out of audio stream d'tor early!\n");
                break;
            }
        }

        Purge();
    }

    static void _HdrReadCB(nlFile* pFile, void* pData, unsigned int Length, uintptr_t User);
    static void _WarmReadCB(nlFile* pFile, void* pData, unsigned int Length, uintptr_t User);
    static void _UpdateReadCB(nlFile* pFile, void* pData, unsigned int Length, uintptr_t User);

    class READ_CB_INFO
    {
    public:
        /* 0x0 */ AudioStream* pStream;
        /* 0x4 */ class AudioStreamBuffer* pBuffer;

        void Set(AudioStream* stream, AudioStreamBuffer* buffer)
        {
            pStream = stream;
            pBuffer = buffer;
        }
        static nlArrayAllocator<READ_CB_INFO> s_AllocPool;
    };

    static const unsigned long MAX_BUFFERS = 2;

    /* 0x04 */ unsigned char m_FlagAtDelete;
    /* 0x08 */ STREAM_STATE m_State;
    /* 0x0C */ unsigned long m_StreamLength;
    /* 0x10 */ unsigned long m_StreamPos;
    /* 0x14 */ AudioStreamBuffer* m_Buffers[MAX_BUFFERS];
    /* 0x1C */ unsigned long m_LastPlayable;
    /* 0x20 */ unsigned long m_Volume;
    /* 0x24 */ bool m_LPFOn;
    /* 0x26 */ unsigned short m_LPFFreq;
    /* 0x28 */ unsigned long m_OldLength;
    /* 0x2C */ AudioBufferMgr& m_BuffMgr;
    /* 0x30 */ unsigned long m_Flags;
    /* 0x34 */ unsigned long m_BufferCount;
}; // total size: 0x38

class MonoAudioStream : public AudioStream
{
public:
    MonoAudioStream(AudioBufferMgr& mgr);
    void Open(const char* filename)
    {
        m_StreamLength = 0;
        m_OldLength = 0;
        m_StreamPos = 0;

        ResetBuffers();

        m_LastPlayable = 0;
        m_Flags = 0;
        m_Volume = 64;
        m_LPFOn = 0;
        m_LPFFreq = 0x3FFF;
        m_pFile = nlOpen(filename);
        m_State = SS_Initd;
    }
    // PORT: not inline, defined in src/platform/gcstream_purge.cpp.
    virtual ~MonoAudioStream();
    virtual unsigned long DoUpdateRead(unsigned long MRAMOffsetA, unsigned long LengthA, unsigned long MRAMOffsetB, unsigned long LengthB, GCAudioStreaming::AudioStreamBuffer* pRequestingBuffer);
    virtual void Warm(bool CoolOnStop);
    void ReadHeader();
    void ReadAsync(void* pData, unsigned int Length)
    {
        bool enabled = OSDisableInterrupts();
        READ_CB_INFO* pCBInfo = READ_CB_INFO::s_AllocPool.Allocate();
        OSRestoreInterrupts(enabled);
        uintptr_t user = (uintptr_t)pCBInfo;
        if (pCBInfo)
        {
            AudioStreamBuffer* pBuffer = m_Buffers[0];
            pCBInfo->Set(this, pBuffer);
        }
        nlReadAsync(m_pFile, pData, Length, _WarmReadCB, user);
    }
    // PORT: not inline, defined in src/platform/gcstream_purge.cpp.
    virtual unsigned long GetUpdateReadLength();
    virtual void CancelPendingReads();
    // PORT: not inline, defined in src/platform/gcstream_purge.cpp.
    static void _AsyncCancelCB(
        nlFile*, void*, unsigned int, uintptr_t uParam,
        void (*)(nlFile*, void*, unsigned int, uintptr_t));
    // PORT: not inline, defined in src/platform/gcstream_purge.cpp.
    virtual bool SafeToPurge();
    // PORT: not inline, defined in src/platform/gcstream_purge.cpp.
    virtual void Purge();

    /* 0x38 */ class nlFile* m_pFile;
    /* 0x3C */ unsigned long m_UpdateLen;
}; // total size: 0x40

class StereoAudioStream : public AudioStream
{
public:
    StereoAudioStream(AudioBufferMgr& mgr);
    void ReadHeader(unsigned long buffer);
    void Open(const char* filename)
    {
        m_StreamLength = 0;
        m_OldLength = 0;
        m_StreamPos = 0;

        ResetBuffers();

        m_LastPlayable = 0;
        m_Flags = 0;
        m_Volume = 64;
        m_LPFOn = 0;
        m_LPFFreq = 0x3FFF;
        m_pFile = nlOpen(filename);
        m_State = SS_Initd;
    }
    virtual ~StereoAudioStream()
    {
        Destructor();
    }
    virtual unsigned long DoUpdateRead(unsigned long MRAMOffsetA, unsigned long LengthA, unsigned long MRAMOffsetB, unsigned long LengthB, GCAudioStreaming::AudioStreamBuffer* pRequestingBuffer);
    void InterleavedHdrReadCB(nlFile* pFile, void* pData, unsigned int Length);
    AudioStreamBuffer* GetBuffer(unsigned long index)
    {
        return index < m_BufferCount ? m_Buffers[index] : NULL;
    }
    READ_CB_INFO* AllocReadCBInfo(AudioStreamBuffer* buffer)
    {
        bool enabled = OSDisableInterrupts();
        READ_CB_INFO* info = READ_CB_INFO::s_AllocPool.Allocate();
        OSRestoreInterrupts(enabled);
        if (info)
            info->Set(this, buffer);
        return info;
    }
    void UpdateReadDone(
        unsigned char* data, unsigned int length,
        AudioStreamBuffer* buffer)
    {
        _UpdateReadCB(
            m_pFile, data + length, length, (uintptr_t)AllocReadCBInfo(buffer));
    }
    void ReadFirst(
        unsigned char* data, unsigned int length,
        AudioStreamBuffer* buffer,
        unsigned long offsetA, unsigned long lengthA,
        unsigned long offsetB, unsigned long lengthB,
        ReadAsyncCallback callback);
    unsigned long GetFilePosition()
    {
        unsigned long position = nlGetFilePosition(m_pFile);
        return position;
    }
    void SetLastPlayable(
        unsigned long offsetA, unsigned long lengthA,
        unsigned long offsetB, unsigned long lengthB)
    {
        unsigned long result = offsetA + lengthA;
        if (lengthB != 0)
            result = offsetB + lengthB;
        m_LastPlayable = result;
    }
    void AdvanceStreamPosition(unsigned long length)
    {
        m_StreamPos += length;
    }
    virtual void Warm(bool CoolOnStop);
    inline virtual void CancelPendingReads();
    static inline void _AsyncCancelCB(
        nlFile*, void* buffer, unsigned int, uintptr_t uParam,
        void (*Callback)(nlFile*, void*, unsigned int, uintptr_t));
    static inline void _InterleavedHdrReadCB(
        nlFile* pFile, void* pData, unsigned int Length, uintptr_t User);
    inline virtual unsigned long GetUpdateReadLength();
    // PORT: not inline, defined in src/platform/gcstream_purge.cpp.
    virtual bool SafeToPurge();
    // PORT: not inline, defined in src/platform/gcstream_purge.cpp.
    virtual void Purge();

    /* 0x38 */ nlFile* m_pFile;
    /* 0x3C */ unsigned long m_Interleave;
}; // total size: 0x40

inline void AudioStream::Purge()
{
    m_State = SS_New;
}

inline AudioStream::~AudioStream()
{
}

inline void AudioStream::WarmReadDone(AudioStreamBuffer* pBuffer)
{
    if (m_Buffers[m_BufferCount - 1] != pBuffer)
    {
        return;
    }

    m_State = SS_Warm;

    if (!(m_Flags & (1 << SF_Play)))
    {
        return;
    }

    if (pBuffer != m_Buffers[m_BufferCount - 1])
    {
        return;
    }

    m_Flags &= ~(1 << SF_Play);
    ActivateBuffers();
    m_State = SS_Playing;
}

} // namespace GCAudioStreaming

// PORT: the inline virtual bodies, so that every TU emitting one of these vtables has them.
#include "Game/Sys/GCStreamVirtuals.h"

#endif // _GCSTREAM_H_
