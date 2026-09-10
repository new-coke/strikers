#ifndef _GCSTREAMVIRTUALS_H_
#define _GCSTREAMVIRTUALS_H_

#include "Game/Sys/GCStream.h"

inline void GCAudioStreaming::StereoAudioStream::_InterleavedHdrReadCB(
    nlFile* pFile, void* pData, unsigned int Length, uintptr_t User)
{
    ((StereoAudioStream*)User)->InterleavedHdrReadCB(pFile, pData, Length);
}

inline void GCAudioStreaming::StereoAudioStream::_AsyncCancelCB(
    nlFile*, void* buffer, unsigned int, uintptr_t uParam,
    void (*Callback)(nlFile*, void*, unsigned int, uintptr_t))
{
    if (Callback == &_InterleavedHdrReadCB)
    {
        nlFree(buffer);
    }
    else
    {
        READ_CB_INFO* pCBInfo = (READ_CB_INFO*)uParam;
        READ_CB_INFO::s_AllocPool.DeleteEntry(pCBInfo);
    }
}

inline void GCAudioStreaming::StereoAudioStream::CancelPendingReads()
{
    nlCancelPendingAsyncReads(m_pFile, &_AsyncCancelCB);
}

inline unsigned long GCAudioStreaming::StereoAudioStream::GetUpdateReadLength()
{
    unsigned long len = m_Interleave;
    if (m_StreamPos + len > m_StreamLength)
    {
        len = (m_StreamLength - m_StreamPos + 0x1f) & ~0x1f;
    }
    return len;
}

inline u32 GCAudioStreaming::AudioStreamBuffer::_UpdateHandler(
    void*, u32 LengthA, void*, u32 LengthB,
    size_t user)
{
    AudioStreamBuffer* pBuffer = (AudioStreamBuffer*)user;
    return pBuffer->UpdateHandler(LengthA, LengthB);
}

inline void GCAudioStreaming::MonoAudioStream::_AsyncCancelCB(
    nlFile*, void*, unsigned int, uintptr_t uParam,
    void (*)(nlFile*, void*, unsigned int, uintptr_t))
{
    READ_CB_INFO* pCBInfo = (READ_CB_INFO*)uParam;
    READ_CB_INFO::s_AllocPool.DeleteEntry(pCBInfo);
}

inline unsigned long GCAudioStreaming::MonoAudioStream::GetUpdateReadLength()
{
    unsigned long streamPos = m_StreamPos;
    unsigned long length = m_UpdateLen;
    if (streamPos + length > m_StreamLength)
    {
        unsigned long aligned = (m_StreamLength - streamPos + 0x1f) & ~0x1f;
        if (aligned)
        {
            length = aligned;
        }
        return length;
    }
    return length;
}

inline void GCAudioStreaming::MonoAudioStream::CancelPendingReads()
{
    nlCancelPendingAsyncReads(m_pFile, &_AsyncCancelCB);
}

#endif // _GCSTREAMVIRTUALS_H_
