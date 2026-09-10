#include "Game/Sys/GCStream.h"
#include "port/endian.h"
#include "Game/Sys/GCStreamVirtuals.h"
#include "NL/nlAlgorithm.h"
#include "NL/nlFileGC.h"
#include "NL/nlMemory.h"

static void ___blank(const char*, ...);

inline void GCAudioStreaming::StereoAudioStream::ReadFirst(
    unsigned char* data,
    unsigned int length,
    AudioStreamBuffer* buffer,
    unsigned long offsetA,
    unsigned long lengthA,
    unsigned long offsetB,
    unsigned long lengthB,
    ReadAsyncCallback callback)
{
    unsigned long position = GetFilePosition();
    const char* format = "Reading into %d %d and %d %d from %d\n";
    ___blank(format, offsetA, lengthA, offsetB, lengthB, position);
    bool enabled = OSDisableInterrupts();
    READ_CB_INFO* info = READ_CB_INFO::s_AllocPool.Allocate();
    OSRestoreInterrupts(enabled);
    uintptr_t user = (uintptr_t)info;
    if (info)
    {
        info->Set(this, buffer);
    }
    nlReadAsync(m_pFile, data, length, callback, user);
}

namespace GCAudioStreaming
{

inline void AudioStreamBuffer::SetVolume(
    unsigned long volume)
{
    m_Volume = (unsigned char)volume;
    sndStreamMixParameterEx(
        m_StreamId, m_Volume, m_Pan, m_SurroundPan, 0, 0);
}

inline unsigned long AudioStreamBuffer::DoUpdate(unsigned long Length)
{
    unsigned long UpdateLen = Length >> 3;
    UpdateLen *= 14;
    return UpdateHandler(UpdateLen, 0);
}

} // namespace GCAudioStreaming

unsigned char ARRAY_ALLOCATOR_MEMORY_class_name_s_AllocPool[sizeof(GCAudioStreaming::AudioStream::READ_CB_INFO) * 32];

nlArrayAllocator<GCAudioStreaming::AudioStream::READ_CB_INFO> GCAudioStreaming::AudioStream::READ_CB_INFO::s_AllocPool(
    (GCAudioStreaming::AudioStream::READ_CB_INFO*)ARRAY_ALLOCATOR_MEMORY_class_name_s_AllocPool, 32);

/**
 * Offset/Address/Size: 0x1984 | 0x801C9134 | size: 0x50
 */
static void ___blank(const char*, ...)
{
}

/**
 * Offset/Address/Size: 0x187C | 0x801C902C | size: 0x108
 */
void GCAudioStreaming::AudioStream::_HdrReadCB(nlFile* pFile, void* pData, unsigned int Length, uintptr_t User)
{
    // PORT: big-endian on disc. Three u32s, two u16s, three u32s.
    {
        sDSPADPCM* h = (sDSPADPCM*)((uintptr_t)pData - Length);
        u16* w;
        unsigned i;
        port_be32_array(&h->num_samples, 3);
        h->loop_flag = port_be16(&h->loop_flag);
        h->format = port_be16(&h->format);
        port_be32_array(&h->sa, 3);
        w = (u16*)&h->coef[0];
        for (i = 0; i < (0x60 - 0x1C) / 2; i++)
            w[i] = port_be16(&w[i]);
    }

    READ_CB_INFO* pCBInfo = (READ_CB_INFO*)User;

    bool serious;
    if (pCBInfo->pStream->m_Flags & (1 << SF_SeriousStop))
    {
        switch (pCBInfo->pStream->m_State)
        {
        case SS_New:
        case SS_Initd:
            break;
        case SS_Warming:
            pCBInfo->pStream->m_State = SS_Warm;
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

    if (serious)
    {
        READ_CB_INFO::s_AllocPool.DeleteEntry(pCBInfo);
    }
    else
    {
        AudioStreamBuffer* pBuffer;
        unsigned long streamLength;
        AudioStream* pStream;
        sDSPADPCM* pHdr = (sDSPADPCM*)((uintptr_t)pData - Length);

        pCBInfo->pStream->m_StreamLength = (pHdr->num_samples / 14) * 8;

        pStream = pCBInfo->pStream;
        streamLength = pStream->m_StreamLength;
        if (pStream->m_StreamPos >= streamLength)
        {
            streamLength = pStream->m_StreamPos;
        }
        pStream->m_StreamLength = streamLength;

        pCBInfo->pStream->m_OldLength = pCBInfo->pStream->m_StreamLength;

        pBuffer = pCBInfo->pBuffer;
        sndStreamFrq(pBuffer->m_StreamId, pHdr->sample_rate);
        sndStreamADPCMParameter(pBuffer->m_StreamId, (SND_ADPCMSTREAM_INFO*)pHdr->coef);

        READ_CB_INFO::s_AllocPool.DeleteEntry(pCBInfo);
    }
}

/**
 * Offset/Address/Size: 0x17BC | 0x801C8F6C | size: 0xC0
 */
void GCAudioStreaming::AudioStream::_WarmReadCB(nlFile* pFile, void* pData, unsigned int Length, uintptr_t User)
{
    READ_CB_INFO* pCBInfo = (READ_CB_INFO*)User;
    AudioStream* pStream = pCBInfo->pStream;

    bool serious;
    if (pStream->m_Flags & (1 << SF_SeriousStop))
    {
        switch (pStream->m_State)
        {
        case SS_New:
        case SS_Initd:
            break;
        case SS_Warming:
            pStream->m_State = SS_Warm;
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

    if (serious)
    {
        READ_CB_INFO::s_AllocPool.DeleteEntry(pCBInfo);
    }
    else
    {
        unsigned long samples = (Length >> 3) * 0xE;
        sndStreamARAMUpdate(pCBInfo->pBuffer->m_StreamId, 0, samples, 0, 0);
        pCBInfo->pStream->WarmReadDone(pCBInfo->pBuffer);
        READ_CB_INFO::s_AllocPool.DeleteEntry(pCBInfo);
    }
}

inline void GCAudioStreaming::AudioStreamBuffer::Update(
    unsigned long Offset, unsigned long Length)
{
    sndStreamARAMUpdate(
        m_StreamId,
        (Offset >> 3) * 0xe,
        (Length >> 3) * 0xe,
        0,
        0);
}

/**
 * Offset/Address/Size: 0x1704 | 0x801C8EB4 | size: 0xB8
 */
void GCAudioStreaming::AudioStream::_UpdateReadCB(nlFile* pFile, void* pData, unsigned int Length, uintptr_t User)
{
    READ_CB_INFO* pCBInfo = (READ_CB_INFO*)User;
    AudioStream* pStream = pCBInfo->pStream;

    bool serious;
    if (pStream->m_Flags & (1 << SF_SeriousStop))
    {
        switch (pStream->m_State)
        {
        case SS_New:
        case SS_Initd:
            break;
        case SS_Warming:
            pStream->m_State = SS_Warm;
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

    if (serious)
    {
        READ_CB_INFO::s_AllocPool.DeleteEntry(pCBInfo);
    }
    else
    {
        AudioStreamBuffer* pBuffer = pCBInfo->pBuffer;
        pBuffer->Update(
            (uintptr_t)pData - Length - (uintptr_t)pBuffer->m_MRAMBuffer,
            Length);
        READ_CB_INFO::s_AllocPool.DeleteEntry(pCBInfo);
    }
}

inline void* GCAudioStreaming::AudioBufferMgr::GetADPCMHdr()
{
    return (void*)(((uintptr_t)m_ADPCMHdrMem + 0x1F) & ~0x1F);
}

inline void GCAudioStreaming::MonoAudioStream::ReadHeader()
{
    void* pADPCMHdr = GetADPCMHdr();
    bool enabled = OSDisableInterrupts();
    READ_CB_INFO* pCBInfo = READ_CB_INFO::s_AllocPool.Allocate();
    OSRestoreInterrupts(enabled);
    uintptr_t user = (uintptr_t)pCBInfo;

    if (pCBInfo)
    {
        AudioStreamBuffer* pBuffer = m_Buffers[0];
        pCBInfo->Set(this, pBuffer);
    }
    nlReadAsync(m_pFile, pADPCMHdr, sizeof(sDSPADPCM), _HdrReadCB, user);
}

/**
 * Offset/Address/Size: 0x1364 | 0x801C8B14 | size: 0x3A0
 */
void GCAudioStreaming::MonoAudioStream::Warm(bool CoolOnStop)
{
    m_State = SS_Warming;
    SetFlag(SF_SeriousStop, false);
    SetFlag(SF_CoolOnStop, CoolOnStop);

    AllocBuffer();

    m_UpdateLen = m_Buffers[0]->m_BufferSize >> 1;
    m_StreamLength = (unsigned long)-1;

    unsigned int AllocSize;
    nlFileSize(m_pFile, &AllocSize);

    long ReadLen = m_UpdateLen;
    ReadLen = AllocSize - 0x60 <= (unsigned long)ReadLen
                ? AllocSize - 0x60
                : ReadLen;
    m_UpdateLen = ReadLen;

    nlSeek(m_pFile, 0, 0);

    ReadHeader();

    AudioStreamBuffer* pBuffer = m_Buffers[0];
    unsigned char* pDataBuf = pBuffer->m_MRAMBuffer;

    ReadAsync(pDataBuf, ReadLen);

    m_StreamPos = ReadLen;
    m_Buffers[0]->m_UpdateOffset += ReadLen;

    if ((unsigned long)ReadLen < m_Buffers[0]->m_BufferSize >> 1)
    {
        memset(pDataBuf + ReadLen, 0, (m_Buffers[0]->m_BufferSize >> 1) - ReadLen);
    }

    long secondReadLen = m_UpdateLen;
    secondReadLen = AllocSize - 0x60 - ReadLen
                         <= (unsigned long)secondReadLen
                      ? AllocSize - 0x60 - ReadLen
                      : secondReadLen;
    if (secondReadLen > 0)
    {
        pBuffer = m_Buffers[0];
        pBuffer->DoUpdate(secondReadLen);
    }
    else
    {
        SetFlag(SF_EndAtUpdate, true);
        m_LastPlayable = m_StreamPos;
    }
}

/**
 * Offset/Address/Size: 0xDCC | 0x801C857C | size: 0x598
 */
unsigned long GCAudioStreaming::MonoAudioStream::DoUpdateRead(unsigned long MRAMOffsetA, unsigned long LengthA, unsigned long MRAMOffsetB, unsigned long LengthB, GCAudioStreaming::AudioStreamBuffer* pRequestingBuffer)
{
    unsigned long OffsetChunk;
    unsigned long ReadASize;
    unsigned long ReadBSize;
    unsigned long AlignOff;
    unsigned char* pMRAMBuffer = m_Buffers[0]->m_MRAMBuffer;
    if (m_Flags & (1 << SF_EndAtUpdate))
    {
        if (MRAMOffsetA <= m_LastPlayable && MRAMOffsetA + LengthA >= m_LastPlayable)
        {
            Stop();
            return 0;
        }
        ___blank("Read of zereos %d %d and %d %d\n", MRAMOffsetA, LengthA, MRAMOffsetB, LengthB);
        memset(pMRAMBuffer + MRAMOffsetA, 0, LengthA);
        bool enabled = OSDisableInterrupts();
        READ_CB_INFO* pCBInfo = READ_CB_INFO::s_AllocPool.Allocate();
        OSRestoreInterrupts(enabled);
        if (pCBInfo)
        {
            AudioStreamBuffer* pB = m_Buffers[0];
            pCBInfo->pStream = this;
            pCBInfo->pBuffer = pB;
        }
        _UpdateReadCB(m_pFile, pMRAMBuffer + MRAMOffsetA + LengthA, LengthA, (uintptr_t)pCBInfo);
        if (LengthB != 0)
        {
            memset(pMRAMBuffer + MRAMOffsetB, 0, LengthB);
            bool e3 = OSDisableInterrupts();
            READ_CB_INFO* pCBInfo2 = READ_CB_INFO::s_AllocPool.Allocate();
            OSRestoreInterrupts(e3);
            if (pCBInfo2)
            {
                AudioStreamBuffer* pB = m_Buffers[0];
                pCBInfo2->pStream = this;
                pCBInfo2->pBuffer = pB;
            }
            _UpdateReadCB(m_pFile, pMRAMBuffer + MRAMOffsetB + LengthB, LengthB, (uintptr_t)pCBInfo2);
        }
        return LengthA + LengthB;
    }
    for (OffsetChunk = 0; OffsetChunk < 2; OffsetChunk++)
    {
        unsigned long ReadLen = LengthA;
        if (OffsetChunk != 0)
            ReadLen = LengthB;
        unsigned long MRAMOffset;
        if (ReadLen == 0)
            continue;
        ReadASize = ReadLen;
        MRAMOffset = MRAMOffsetA;
        ReadBSize = 0;
        if (OffsetChunk != 0)
            MRAMOffset = MRAMOffsetB;
        ___blank("Len: %d Pos: %d ReadLen: %d\n", m_StreamLength, m_StreamPos, ReadLen);
        m_StreamPos += ReadLen;
        if (m_StreamPos >= m_StreamLength)
        {
            ___blank("Stream wrap\n");
            ReadASize = ReadLen - (m_StreamPos = m_StreamPos - m_StreamLength);
            AlignOff = 32 - (ReadASize & 31);
            AlignOff = (AlignOff == 32) ? 0 : AlignOff;
            ReadASize += AlignOff;
            ReadBSize = m_StreamPos - AlignOff;
            if (!(m_Flags & (1 << SF_Loop)))
            {
                unsigned long eosOff = MRAMOffset + ReadASize;
                ___blank("EOS zeros %d %d\n", eosOff, ReadBSize);
                memset(pMRAMBuffer + ReadASize, 0, ReadBSize);
                bool e4 = OSDisableInterrupts();
                READ_CB_INFO* pCBInfo3 = READ_CB_INFO::s_AllocPool.Allocate();
                OSRestoreInterrupts(e4);
                if (pCBInfo3)
                {
                    AudioStreamBuffer* pB = m_Buffers[0];
                    pCBInfo3->pStream = this;
                    pCBInfo3->pBuffer = pB;
                }
                _UpdateReadCB(m_pFile, pMRAMBuffer + MRAMOffset + ReadASize + ReadBSize, ReadBSize, (uintptr_t)pCBInfo3);
                m_LastPlayable = eosOff;
                ReadBSize = 0;
                m_Flags = (m_Flags & ~(1 << SF_EndAtUpdate)) | (1 << SF_EndAtUpdate);
            }
        }
        ___blank("Reading into %d %d from %d\n", MRAMOffset, ReadASize, nlGetFilePosition(m_pFile));
        bool e5 = OSDisableInterrupts();
        READ_CB_INFO* pCBInfo4 = READ_CB_INFO::s_AllocPool.Allocate();
        OSRestoreInterrupts(e5);
        if (pCBInfo4)
        {
            AudioStreamBuffer* pB = m_Buffers[0];
            pCBInfo4->pStream = this;
            pCBInfo4->pBuffer = pB;
        }
        nlReadAsync(m_pFile, pMRAMBuffer + MRAMOffset, ReadASize, _UpdateReadCB, (uintptr_t)pCBInfo4);
        if (ReadBSize != 0)
        {
            nlSeek(m_pFile, 0x60, 0);
            unsigned long filePos2 = nlGetFilePosition(m_pFile);
            ___blank("Also reading into %d %d from %d\n", MRAMOffset + ReadASize, ReadBSize, filePos2);
            OSGetConsoleType();
            bool e6 = OSDisableInterrupts();
            READ_CB_INFO* pCBInfo5 = READ_CB_INFO::s_AllocPool.Allocate();
            OSRestoreInterrupts(e6);
            if (pCBInfo5)
            {
                AudioStreamBuffer* pB = m_Buffers[0];
                pCBInfo5->pStream = this;
                pCBInfo5->pBuffer = pB;
            }
            nlReadAsync(m_pFile, pMRAMBuffer + MRAMOffset + ReadASize, ReadBSize, _UpdateReadCB, (uintptr_t)pCBInfo5);
        }
    }
    return 0;
}

inline void GCAudioStreaming::StereoAudioStream::ReadHeader(
    unsigned long buffer)
{
    // PORT: seek, the file position cannot be trusted here.
    nlSeek(m_pFile,
           sizeof(INTERLEAVED_ADPCM_HEADER) + buffer * sizeof(sDSPADPCM), 0);
    void* pADPCMHdr = m_BuffMgr.GetADPCMHdr();
    bool enabled = OSDisableInterrupts();
    READ_CB_INFO* pCBInfo = READ_CB_INFO::s_AllocPool.Allocate();
    OSRestoreInterrupts(enabled);
    if (pCBInfo)
    {
        AudioStreamBuffer* pBuffer = m_Buffers[buffer];
        pCBInfo->pStream = this;
        pCBInfo->pBuffer = pBuffer;
    }
    nlReadAsync(
        m_pFile,
        pADPCMHdr,
        sizeof(sDSPADPCM),
        _HdrReadCB,
        (uintptr_t)pCBInfo);
}

/**
 * Offset/Address/Size: 0xA48 | 0x801C81F8 | size: 0x384
 */
void GCAudioStreaming::StereoAudioStream::Warm(bool CoolOnStop)
{
    m_State = SS_Warming;
    m_Flags &= ~(1 << SF_SeriousStop);
    m_Flags = (m_Flags & ~(1 << SF_CoolOnStop)) | ((unsigned long)CoolOnStop << SF_CoolOnStop);

    AudioStreamBuffer* pBuf = m_BuffMgr.GetFreeBuffer(this);
    m_Buffers[0] = pBuf;

    pBuf = m_BuffMgr.GetFreeBuffer(this);
    m_Buffers[1] = pBuf;

    {
        AudioStreamBuffer* pBuf0 = m_Buffers[0];
        pBuf0->m_Pan = 0;
        sndStreamMixParameterEx(pBuf0->m_StreamId, pBuf0->m_Volume, pBuf0->m_Pan, pBuf0->m_SurroundPan, 0, 0);
    }
    {
        AudioStreamBuffer* pBuf1 = m_Buffers[1];
        pBuf1->m_Pan = 0x7F;
        sndStreamMixParameterEx(pBuf1->m_StreamId, pBuf1->m_Volume, pBuf1->m_Pan, pBuf1->m_SurroundPan, 0, 0);
    }

    AudioStreamBuffer* BufferIndex = NULL;
    AudioStreamBuffer* pBuffer = GetBuffer((uintptr_t)BufferIndex);
    while (pBuffer != NULL)
    {
        pBuffer->SetVolume(m_Volume);
        pBuffer->SetLPF(m_LPFOn);
        pBuffer->SetLPF(m_LPFFreq);

        ((unsigned long&)BufferIndex)++;
        pBuffer = GetBuffer((uintptr_t)BufferIndex);
    }

    m_StreamLength = (unsigned long)-1;
    nlSeek(m_pFile, 0, 0);

    void* pInterlvHdr = nlMalloc(sizeof(INTERLEAVED_ADPCM_HEADER), 0x20, true);
    nlReadAsync(m_pFile, pInterlvHdr, sizeof(INTERLEAVED_ADPCM_HEADER), _InterleavedHdrReadCB, (uintptr_t)this);

    for (unsigned long buffer = 0; buffer < 2; buffer++)
    {
        ReadHeader(buffer);
    }
}

/**
 * Offset/Address/Size: 0x7E8 | 0x801C7F98 | size: 0x260
 */
void GCAudioStreaming::StereoAudioStream::InterleavedHdrReadCB(nlFile* pFile, void* pData, unsigned int Length)
{
    INTERLEAVED_ADPCM_HEADER* pHdr = (INTERLEAVED_ADPCM_HEADER*)((uintptr_t)pData - Length);
    // PORT: big-endian on disc, a four-character tag then two u32s, the interleave size and the stream length.
    port_be32_array((char*)pHdr + 4, 2);

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

    if (serious)
    {
        nlFree(pHdr);
        return;
    }

    m_Interleave = pHdr->Interleave;
    m_StreamLength = pHdr->StreamLength;
    nlFree(pHdr);

    unsigned char* pMRAMBuffer;
    unsigned long aramLen;
    unsigned long offset;
    unsigned long firstLen;
    unsigned long secondLen;
    AudioStreamBuffer* BufferIndex = NULL;
    AudioStreamBuffer* pBuffer = GetBuffer((uintptr_t)BufferIndex);

    while (pBuffer != NULL)
    {
        pBuffer->m_UpdateOffset += m_Interleave;
        pMRAMBuffer = pBuffer->m_MRAMBuffer;

        bool enabled = OSDisableInterrupts();
        READ_CB_INFO* pCBInfo = READ_CB_INFO::s_AllocPool.Allocate();
        OSRestoreInterrupts(enabled);

        if (pCBInfo)
        {
            pCBInfo->pStream = this;
            pCBInfo->pBuffer = pBuffer;
        }

        nlReadAsync(m_pFile, pMRAMBuffer, m_Interleave, _WarmReadCB, (uintptr_t)pCBInfo);

        ((unsigned long&)BufferIndex)++;
        pBuffer = GetBuffer((uintptr_t)BufferIndex);
    }

    m_StreamPos = m_Interleave;
    unsigned long readLen = GetUpdateReadLength();
    AudioStreamBuffer* pBuf = m_Buffers[0];
    aramLen = (readLen / 8) * 14;

    if (aramLen == 0)
        return;

    unsigned long bufReadLen = pBuf->m_pStream->GetUpdateReadLength();
    if ((aramLen / 14) * 8 < bufReadLen)
        return;

    offset = pBuf->m_UpdateOffset;
    pBuf->m_UpdateOffset = offset + bufReadLen;

    if (pBuf->m_UpdateOffset >= pBuf->m_BufferSize)
    {
        unsigned long wrapped = pBuf->m_UpdateOffset - pBuf->m_BufferSize;
        pBuf->m_UpdateOffset = wrapped;
        firstLen = bufReadLen - wrapped;
        secondLen = pBuf->m_UpdateOffset & ~31;
    }
    else
    {
        firstLen = bufReadLen;
        secondLen = 0;
    }

    ___blank(pBuf->m_pStream->m_Buffers[0] == pBuf ? "Left " : "Right ");
    ___blank("Asking for %d %d and %d %d\n", offset, firstLen, 0, secondLen);

    pBuf->m_pStream->DoUpdateRead(offset, firstLen, 0, secondLen, pBuf);
}

/**
 * Offset/Address/Size: 0x2A8 | 0x801C7A58 | size: 0x540
 */
unsigned long GCAudioStreaming::StereoAudioStream::DoUpdateRead(
    unsigned long MRAMOffsetA, unsigned long LengthA,
    unsigned long MRAMOffsetB, unsigned long LengthB,
    GCAudioStreaming::AudioStreamBuffer* pRequestingBuffer)
{
    class BufferCursor
    {
    public:
        BufferCursor(StereoAudioStream& stream, unsigned long& index)
            : m_Stream(stream)
            , m_Index(index)
        {
        }

        AudioStreamBuffer* GetBuffer()
        {
            return m_Stream.GetBuffer(m_Index);
        }

        void Advance()
        {
            m_Index++;
        }

    private:
        StereoAudioStream& m_Stream;
        unsigned long& m_Index;
    };

    if (IsSeriousStop())
        return 0;
    if (pRequestingBuffer != m_Buffers[0])
    {
        ___blank("Skiping right channel\n");
        return LengthA + LengthB;
    }
    if (m_OldLength == 0)
        m_OldLength = m_StreamLength;
    if (m_Flags & (1 << SF_EndAtUpdate))
    {
        ___blank(
            "Lookat at stopping, last playable @ %d, currently @ %d for %d (%d) \n",
            m_LastPlayable,
            MRAMOffsetA,
            LengthA,
            MRAMOffsetA + LengthA);
        if (MRAMOffsetA < m_LastPlayable
            && MRAMOffsetA + LengthA >= m_LastPlayable)
        {
            Stop();
            return 0;
        }
        unsigned long BufferIndex = 0;
        BufferCursor cursor(*this, BufferIndex);
        AudioStreamBuffer* pBuffer;
        for (pBuffer = cursor.GetBuffer(); pBuffer;
            cursor.Advance(), pBuffer = cursor.GetBuffer())
        {
            unsigned char* pMRAMBuffer = pBuffer->GetMRAMBuffer();
            ___blank(
                "Read of zereos %d %d and %d %d\n",
                MRAMOffsetA,
                LengthA,
                MRAMOffsetB,
                LengthB);
            memset(pMRAMBuffer + MRAMOffsetA, 0, LengthA);
            UpdateReadDone(pMRAMBuffer + MRAMOffsetA, LengthA, pBuffer);
            if (LengthB != 0)
            {
                memset(pMRAMBuffer + MRAMOffsetB, 0, LengthB);
                UpdateReadDone(pMRAMBuffer + MRAMOffsetB, LengthB, pBuffer);
            }
        }
        return LengthA + LengthB;
    }
    unsigned long TotalReadLen = LengthA + LengthB;
    unsigned long BufferIndex = 0;
    BufferCursor cursor(*this, BufferIndex);
    AudioStreamBuffer* pBuffer;
    for (pBuffer = cursor.GetBuffer(); pBuffer;
        cursor.Advance(), pBuffer = cursor.GetBuffer())
    {
        unsigned char* pMRAMBuffer = pBuffer->GetMRAMBuffer();
        ReadFirst(
            pMRAMBuffer + MRAMOffsetA, LengthA, pBuffer, MRAMOffsetA, LengthA, MRAMOffsetB, LengthB, _UpdateReadCB);
        if (LengthB != 0)
        {
            bool enabled = OSDisableInterrupts();
            READ_CB_INFO* info = READ_CB_INFO::s_AllocPool.Allocate();
            OSRestoreInterrupts(enabled);
            if (info)
            {
                info->Set(this, pBuffer);
            }
            nlReadAsync(
                m_pFile, pMRAMBuffer + MRAMOffsetB, LengthB, _UpdateReadCB, (uintptr_t)info);
        }
    }
    AdvanceStreamPosition(TotalReadLen);
    if (m_StreamPos >= m_StreamLength)
    {
        ___blank("Stream wrap\n");
        m_StreamPos = 0;
        nlSeek(m_pFile, 0xCC, 0);
        if (!(m_Flags & (1 << SF_Loop)))
        {
            SetLastPlayable(MRAMOffsetA, LengthA, MRAMOffsetB, LengthB);
            m_Flags = (m_Flags & ~(1 << SF_EndAtUpdate))
                    | (1 << SF_EndAtUpdate);
        }
    }
    return TotalReadLen;
}

/**
 * Offset/Address/Size: 0x268 | 0x801C7A18 | size: 0x40
 */
void GCAudioStreaming::AudioBufferMgr::Init(unsigned long BufferPoolSize)
{
    m_PoolSize = BufferPoolSize;
    m_MRAMBuffer = (unsigned char*)nlMalloc(BufferPoolSize, 0x20, false);
}

/**
 * Offset/Address/Size: 0x134 | 0x801C78E4 | size: 0x134
 */
void GCAudioStreaming::AudioBufferMgr::CreateBuffers(unsigned long Count)
{
    unsigned long buffer;

    m_BufferCount = Count;
    m_BufferSize = (m_PoolSize / Count) & ~31u;

    for (buffer = 0; buffer < m_BufferCount; buffer++)
    {
        unsigned long freeMask = m_BuffersFree & ~(1u << buffer);
        m_BuffersFree = freeMask | (1u << buffer);

        unsigned char* bufAddr = m_MRAMBuffer + m_BufferSize * buffer;
        m_Buffers[buffer].m_BufferSize = m_BufferSize;
        m_Buffers[buffer].m_BufferSamples = (m_Buffers[buffer].m_BufferSize / 8) * 14;
        m_Buffers[buffer].m_MRAMBuffer = bufAddr;

        m_Buffers[buffer].m_Volume = 0x7F;
        m_Buffers[buffer].m_Pan = 0x40;
        m_Buffers[buffer].m_bLPFOn = 0;
        m_Buffers[buffer].m_LPFFreq = 0x3FFF;
        m_Buffers[buffer].m_UpdateOffset = 0;
        m_Buffers[buffer].m_pStream = 0;

        m_Buffers[buffer].m_StreamId = sndStreamAllocEx(
            0xFF,
            m_Buffers[buffer].m_MRAMBuffer,
            m_Buffers[buffer].m_BufferSamples,
            0x7D00,
            0,
            0x40,
            0x40,
            0,
            0,
            0,
            0x30001,
            AudioStreamBuffer::_UpdateHandler,
            (uintptr_t)&m_Buffers[buffer],
            NULL);

        sndStreamMixParameterEx(
            m_Buffers[buffer].m_StreamId,
            m_Buffers[buffer].m_Volume,
            m_Buffers[buffer].m_Pan,
            m_Buffers[buffer].m_SurroundPan,
            0,
            0);
    }
}

/**
 * Offset/Address/Size: 0x98 | 0x801C7848 | size: 0x9C
 */
void GCAudioStreaming::AudioBufferMgr::DeleteBuffers()
{
    unsigned long buffer;
    for (buffer = 0; buffer < m_BufferCount; buffer++)
    {
        m_Buffers[buffer].m_Volume = 0;
        sndStreamMixParameterEx(m_Buffers[buffer].m_StreamId, m_Buffers[buffer].m_Volume, m_Buffers[buffer].m_Pan, m_Buffers[buffer].m_SurroundPan, 0, 0);
        sndStreamDeactivate(m_Buffers[buffer].m_StreamId);
        sndStreamFree(m_Buffers[buffer].m_StreamId);
    }
    m_BufferCount = 0;
}

GCAudioStreaming::AudioStreamBuffer*
GCAudioStreaming::AudioBufferMgr::GetFreeBuffer(AudioStream* pStream)
{
    for (unsigned long buffer = 0; buffer < m_BufferCount; buffer++)
    {
        if ((int)(bool)(m_BuffersFree & (1 << buffer)) == 1)
        {
            SetBufferState(buffer, BAS_Busy);
            m_Buffers[buffer].Reset(pStream);
            ___blank(
                "After buffer alloc there are %d availible\n",
                nlCountBits(m_BuffersFree));
            return &m_Buffers[buffer];
        }
    }
    return NULL;
}

/**
 * Offset/Address/Size: 0x0 | 0x801C77B0 | size: 0x98
 */
void GCAudioStreaming::AudioBufferMgr::FreeBuffer(GCAudioStreaming::AudioStreamBuffer* pBuffer)
{
    unsigned long buff = pBuffer - m_Buffers;

    m_Buffers[buff].Reset();

    SetBufferState(buff, BAS_Free);

    ___blank("After buffer free there are %d availible\n", nlCountBits(m_BuffersFree));
}

inline unsigned long GCAudioStreaming::AudioStreamBuffer::UpdateHandler(
    unsigned long LengthA, unsigned long LengthB)
{
    if (!LengthA && !LengthB)
    {
        return 0;
    }

    unsigned long StreamChunkSize = m_pStream->GetUpdateReadLength();

    if ((LengthA + LengthB) / 14 * 8 < StreamChunkSize)
    {
        return 0;
    }

    unsigned long ChunkAOffset = m_UpdateOffset;
    m_UpdateOffset = ChunkAOffset + StreamChunkSize;
    unsigned long ChunkASize;
    unsigned long ChunkBSize;

    if (m_UpdateOffset >= m_BufferSize)
    {
        unsigned long wrapped = m_UpdateOffset - m_BufferSize;
        m_UpdateOffset = wrapped;
        ChunkASize = StreamChunkSize - wrapped;
        ChunkBSize = m_UpdateOffset & ~31;
    }
    else
    {
        ChunkASize = StreamChunkSize;
        ChunkBSize = 0;
    }

    ___blank(m_pStream->m_Buffers[0] == this ? "Left " : "Right ");
    ___blank(
        "Asking for %d %d and %d %d\n",
        ChunkAOffset,
        ChunkASize,
        0,
        ChunkBSize);

    m_pStream->DoUpdateRead(
        ChunkAOffset, ChunkASize, 0, ChunkBSize, this);

    return (ChunkASize + ChunkBSize) / 8 * 14;
}
