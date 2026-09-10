// Audio stream virtuals the decompilation declares but never defines.

#include "Game/Sys/GCStream.h"
#include "NL/nlFileGC.h"

// SafeToPurge answers whether this stream's async reads have finished, so its buffers can be freed.
static bool port_stream_reads_drained(const nlFile* pFile)
{
    if (pFile == nullptr)
        return true;

    // nlOpen returns a GCFile for anything on the disc; the async count lives there.
    return ((const GCFile*)pFile)->PendingAsync.m_Count == 0;
}

bool GCAudioStreaming::StereoAudioStream::SafeToPurge()
{
    return port_stream_reads_drained(m_pFile);
}

bool GCAudioStreaming::MonoAudioStream::SafeToPurge() { return port_stream_reads_drained(m_pFile); }

// The base class resets the stream state; the overrides owe it the file they opened.
static void port_stream_purge(nlFile*& pFile)
{
    if (pFile != nullptr)
    {
        nlClose(pFile);
        pFile = nullptr;
    }
}

void GCAudioStreaming::StereoAudioStream::Purge()
{
    port_stream_purge(m_pFile);
    AudioStream::Purge();
}

void GCAudioStreaming::MonoAudioStream::Purge()
{
    port_stream_purge(m_pFile);
    AudioStream::Purge();
}

// Same body as StereoAudioStream's inline destructor.
GCAudioStreaming::MonoAudioStream::~MonoAudioStream() { Destructor(); }
