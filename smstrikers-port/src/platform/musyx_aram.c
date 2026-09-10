// ARAM is an offset space because aramUploadData takes an unsigned long, 4 bytes on Windows.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dolphin/types.h"

void OSReport(const char* msg, ...);

// The console's size, so a game that would run out of ARAM on hardware runs out here too.
#define PORT_ARAM_SIZE (16u * 1024u * 1024u)

// MusyX's silent source for voices past the end of a sample; the first user address is 0x500.
#define PORT_ARAM_ZERO_BYTES (640u * 2u)

#define PORT_ARAM_MAX_STREAM_BUFFERS 64

typedef struct StreamBuffer
{
    u32 aram;         // offset of the block, 0 when this slot is unused
    u32 length;       // what the caller asked for
    u32 allocLength;  // what was reserved, rounded up
    u8  inUse;
} StreamBuffer;

static u8* s_aram;
static u32 s_write;    // bump allocator for samples, growing up
static u32 s_stream;   // stream buffers are carved off the top, growing down
static StreamBuffer s_streamBuffers[PORT_ARAM_MAX_STREAM_BUFFERS];

static int s_logging = -1;
static int logging(void)
{
    if (s_logging < 0)
    {
        const char* e = getenv("STRIKERS_LOG_AUDIO");
        s_logging = (e != NULL && *e != '\0') ? 1 : 0;
    }
    return s_logging;
}

void PortAramInit(unsigned long length)
{
    if (s_aram == NULL)
        s_aram = (u8*)calloc(PORT_ARAM_SIZE, 1);
    if (s_aram == NULL)
        return;

    // Reserving the zero buffer keeps every offset non-zero, so 0 means "no address".
    s_write = PORT_ARAM_ZERO_BYTES;
    s_stream = (length != 0 && length < PORT_ARAM_SIZE) ? (u32)length : PORT_ARAM_SIZE;
    memset(s_streamBuffers, 0, sizeof s_streamBuffers);

    if (logging())
        OSReport("[port] aram: %u bytes, %u reserved for the zero buffer\n",
                 (unsigned)PORT_ARAM_SIZE, (unsigned)PORT_ARAM_ZERO_BYTES);
}

// NULL for an offset that was never allocated, so the mixer skips the voice.
void* PortAramResolve(size_t aram)
{
    if (s_aram == NULL || aram >= PORT_ARAM_SIZE)
        return NULL;
    return s_aram + aram;
}

void PortAramUpload(const void* mram, unsigned long aram, unsigned long len)
{
    if (s_aram == NULL || mram == NULL || len == 0)
        return;
    if ((size_t)aram + len > PORT_ARAM_SIZE)
        return;
    memcpy(s_aram + aram, mram, len);
    if (logging())
    {
        static unsigned long s_uploads;
        u8 i;
        for (i = 0; i < PORT_ARAM_MAX_STREAM_BUFFERS; ++i)
        {
            if (s_streamBuffers[i].aram != 0 && aram >= s_streamBuffers[i].aram
                && aram < s_streamBuffers[i].aram + s_streamBuffers[i].allocLength)
                break;
        }
        if (s_uploads++ < 60)
            OSReport("[port] aram: upload #%lu %lu bytes -> stream %u +%lu\n", s_uploads, len,
                     (unsigned)i, (unsigned long)(i < PORT_ARAM_MAX_STREAM_BUFFERS
                                                       ? aram - s_streamBuffers[i].aram
                                                       : aram));
    }
}

// The upload callback takes a file offset, so src is an offset whenever one is set.
static void* (*s_upload)(u32 offset, u32 bytes);
static unsigned long s_uploadChunk;

void PortAramSetUploadCallback(void* (*callback)(u32, u32), unsigned long chunkSize)
{
    s_upload = callback;
    s_uploadChunk = chunkSize != 0 ? chunkSize : 0x20000;
}

void* PortAramStoreSample(const void* src, unsigned long len)
{
    u8* p;
    unsigned long done;

    if (len == 0)
        return NULL;
    p = (u8*)malloc(len);
    if (p == NULL)
        return NULL;
    if (s_upload == NULL)
    {
        if (src == NULL)
        {
            free(p);
            return NULL;
        }
        memcpy(p, src, len);
        return p;
    }
    for (done = 0; done < len;)
    {
        unsigned long blk = len - done;
        const void* chunk;
        if (blk > s_uploadChunk)
            blk = s_uploadChunk;
        chunk = s_upload((u32)((size_t)src + done), (u32)blk);
        if (chunk == NULL)
        {
            OSReport("[port] aram: sample upload callback returned nothing for %lu bytes at %lu\n",
                     blk, (unsigned long)((size_t)src + done));
            free(p);
            return NULL;
        }
        memcpy(p + done, chunk, blk);
        done += blk;
    }
    return p;
}

void PortAramFreeSample(void* p)
{
    free(p);
}

// Off the top of ARAM, as on console; slots are reused as the stream count changes.
unsigned char PortAramAllocStreamBuffer(unsigned long len)
{
    u8 i;

    if (s_aram == NULL)
        return 0xFF;
    len = (len + 31u) & ~31u;

    for (i = 0; i < PORT_ARAM_MAX_STREAM_BUFFERS; ++i)
    {
        if (!s_streamBuffers[i].inUse && s_streamBuffers[i].allocLength >= len
            && s_streamBuffers[i].aram != 0)
        {
            s_streamBuffers[i].inUse = 1;
            s_streamBuffers[i].length = (u32)len;
            return i;
        }
    }

    for (i = 0; i < PORT_ARAM_MAX_STREAM_BUFFERS; ++i)
    {
        if (s_streamBuffers[i].aram == 0)
        {
            if (s_stream < len || s_stream - len < s_write)
            {
                OSReport("[port] aram: out of stream space for %lu bytes\n", len);
                return 0xFF;
            }
            s_stream -= (u32)len;
            s_streamBuffers[i].aram = s_stream;
            s_streamBuffers[i].allocLength = (u32)len;
            s_streamBuffers[i].length = (u32)len;
            s_streamBuffers[i].inUse = 1;
            if (logging())
                OSReport("[port] aram: stream buffer %u = %u bytes at 0x%x\n",
                         (unsigned)i, (unsigned)len, (unsigned)s_streamBuffers[i].aram);
            return i;
        }
    }

    OSReport("[port] aram: no stream buffer slots left\n");
    return 0xFF;
}

size_t PortAramStreamBufferAddress(unsigned char id, size_t* len)
{
    if (id >= PORT_ARAM_MAX_STREAM_BUFFERS)
        return 0;
    if (len != NULL)
        *len = s_streamBuffers[id].length;
    return s_streamBuffers[id].aram;
}

void PortAramFreeStreamBuffer(unsigned char id)
{
    // The block stays reserved so its address remains valid, and is reused by the next fit.
    if (id < PORT_ARAM_MAX_STREAM_BUFFERS)
        s_streamBuffers[id].inUse = 0;
}
