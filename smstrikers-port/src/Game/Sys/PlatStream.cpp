#include "Game/Sys/PlatStream.h"
#include "Game/Sys/GCStream.h"
#include "NL/nlMemory.h"
#include "NL/nlSortedSlot.h"

extern "C"
{
    void sndStreamMixParameterEx(SND_STREAMID stid, unsigned char vol, unsigned char pan, unsigned char span, unsigned char auxa, unsigned char auxb);
    void sndStreamDeactivate(SND_STREAMID stid);
}

GCAudioStreaming::AudioBufferMgr g_BufferMgr;

nlStaticSortedSlot<GCAudioStreaming::AudioStream*, 7> g_Streams;
static bool g_StreamingInitd;

/**
 * Offset/Address/Size: 0x574 | 0x801C7638 | size: 0x44
 */
void PlatAudio::InitStreaming()
{
    if (!g_StreamingInitd)
    {
        g_BufferMgr.Init(0x5DD80);
        g_StreamingInitd = true;
    }
}

static inline void StopAllStreamObjs()
{
    using namespace GCAudioStreaming;
    unsigned long stream = 0;

    while (stream < g_Streams.m_EntryCount)
    {
        (*g_Streams.m_pEntryLookup[stream].pEntry)->Stop();
        stream++;
    }
}

static inline void PurgeStoppedStreams(unsigned char Block)
{
    using namespace GCAudioStreaming;
    unsigned long stream = 0;

    while (stream < g_Streams.m_EntryCount)
    {
        AudioStream** pStream = g_Streams.m_pEntryLookup[stream].pEntry;
        AudioStream* obj = *pStream;
        if (Block)
        {
            obj->SafeToPurge();
        }
        pStream = g_Streams.m_pEntryLookup[stream].pEntry;
        delete *pStream;
        if (pStream != NULL)
        {
            g_Streams.DeleteEntry(pStream);
        }
        stream++;
    }
}

/**
 * Offset/Address/Size: 0x208 | 0x801C72CC | size: 0x36C
 */
void PlatAudio::ShutdownStreaming()
{
    StopAllStreamObjs();
    PurgeStoppedStreams(true);

    g_Streams.Clear();

    nlFree(g_BufferMgr.m_MRAMBuffer);
    g_BufferMgr.m_MRAMBuffer = NULL;
    g_BufferMgr.m_PoolSize = 0;
    g_StreamingInitd = false;
}

/**
 * Offset/Address/Size: 0x1C4 | 0x801C7288 | size: 0x44
 */
void PlatAudio::ConfigureStreamBuffers(unsigned long count)
{
    g_BufferMgr.DeleteBuffers();
    g_BufferMgr.CreateBuffers(count);
}

/**
 * Offset/Address/Size: 0x1BC | 0x801C7280 | size: 0x8
 */
bool PlatAudio::IsStreamingInited()
{
    return g_StreamingInitd;
}

/**
 * Offset/Address/Size: 0x0 | 0x801C70C4 | size: 0x1BC
 */
void PlatAudio::StopAllStreams()
{
    using namespace GCAudioStreaming;
    typedef nlSortedSlot<AudioStream*, 7>::EntryLookup<AudioStream*> EL;

    AudioStream* stream;
    AudioStreamBuffer* buffer;
    unsigned long streamIndex;
    unsigned long zero = 0;

    // PORT: subscript, not byte arithmetic, EntryLookup is 16 bytes here.
    for (streamIndex = 0; streamIndex < g_Streams.m_EntryCount; streamIndex++)
    {
        stream = *((EL*)g_Streams.m_pEntryLookup)[streamIndex].pEntry;
        stream->m_Flags &= ~(1 << SF_Play);
        if (stream->m_State == SS_Playing)
        {
            volatile unsigned long j = (uintptr_t)(buffer = NULL);
            if (zero < stream->m_BufferCount)
                buffer = stream->m_Buffers[0];

            while (buffer != NULL)
            {
                buffer->m_Volume = 0;
                sndStreamMixParameterEx(buffer->m_StreamId, buffer->m_Volume, buffer->m_Pan, buffer->m_SurroundPan, 0, 0);
                sndStreamDeactivate(buffer->m_StreamId);

                stream->m_State = SS_Warm;
                {
                    unsigned long next = j + 1;
                    j = next;
                    if (next < stream->m_BufferCount)
                        buffer = stream->m_Buffers[next];
                    else
                        buffer = NULL;
                }
            }

            stream->m_StreamPos = 0;
            stream->m_State = SS_Warm;
        }

        stream->CancelPendingReads();
        if (stream->m_Flags & (1 << SF_CoolOnStop))
        {
            stream->m_Flags &= ~(1 << SF_CoolOnStop);
            if (stream->m_State > SS_Initd)
            {
                unsigned long flags = stream->m_Flags;
                flags &= ~(1 << SF_SeriousStop);
                flags |= (1 << SF_SeriousStop);
                stream->m_Flags = flags;

                volatile unsigned long k = 0;
                buffer = NULL;
                if (zero < stream->m_BufferCount)
                    buffer = stream->m_Buffers[0];

                while (buffer != NULL)
                {
                    stream->m_BuffMgr.FreeBuffer(buffer);

                    {
                        unsigned long idx = k;
                        stream->m_Buffers[idx] = NULL;
                        idx = idx + 1;
                        k = idx;
                        if (idx < stream->m_BufferCount)
                            buffer = stream->m_Buffers[idx];
                        else
                            buffer = NULL;
                    }
                }

                stream->m_State = SS_Initd;
            }
        }
    }
}
