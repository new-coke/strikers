// THP video: the low-level decoder the SDK provided and the decomp does not.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dolphin/types.h"
#include "port/benchmark.h"
#include "port/determinism.h"
#include "port/host.h"

#ifdef STRIKERS_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#endif

u32 OSGetTick(void);

// OSGetTick units: the console's 40.5 MHz timer, which src/platform/os.c and PortVirtualTicker both
// count in.
#define PORT_THP_TICKS_PER_SECOND 40500000.0

static u32 port_thp_now(void)
{
    // Under STRIKERS_FIXED_DT the virtual clock advances one frame period per retrace, which is
    // what keeps a capture at frame N the same frame N.
    return PortFixedTimestep() ? PortVirtualTicker() : OSGetTick();
}

static u32 s_pace_base;
static unsigned long long s_pace_wall_base;
static long s_pace_shown;
static int s_pace_started;

static void port_thp_pace_reset(void)
{
    s_pace_started = 0;
    s_pace_shown = 0;
    s_pace_base = 0;
}

static void port_thp_pace_begin(u32 now)
{
    if (!s_pace_started)
    {
        s_pace_started = 1;
        s_pace_base = now;
        s_pace_wall_base = port_monotonic_ns();
        s_pace_shown = 0;
    }
}

int port_thp_frame_due(float frameRate)
{
    u32 now;
    double elapsed;

    if (!(frameRate > 0.0f))
    {
        return 1; // no rate in the header: decode as fast as asked
    }

    now = port_thp_now();
    port_thp_pace_begin(now);

    // Movie frames elapsed since the movie started, on the game's clock.
    elapsed = (double)(u32)(now - s_pace_base) / PORT_THP_TICKS_PER_SECOND
            * (double)frameRate;
    return elapsed >= (double)s_pace_shown;
}

// One frame decoded. Counted here rather than in port_thp_frame_due because a movie has two
// possible clocks, this file's and the audio buffer's, and the progress line has to be right under
// both.
void port_thp_frame_shown(void)
{
    u32 now = port_thp_now();

    port_thp_pace_begin(now);
    s_pace_shown++;

    // Progress against the clock, so "the movie runs fast" is a number rather than an impression:
    // 500 frames at 29.97 fps is 16.7 s.
    if (getenv("STRIKERS_LOG_SCENES") != NULL && (s_pace_shown % 500) == 0)
    {
        // Both clocks, because the game clock is the wall clock in play and the virtual one under
        // STRIKERS_FIXED_DT: when they differ that is why, and when they agree the pacing is right.
        fprintf(stderr, "[port] movie: %ld frames shown in %.1f s on the game clock, "
                        "%.1f s on the wall\n",
                s_pace_shown,
                (double)(u32)(now - s_pace_base) / PORT_THP_TICKS_PER_SECOND,
                (double)(port_monotonic_ns() - s_pace_wall_base) / 1e9);
    }
}

static int port_thp_movies_disabled(void)
{
    const char* v;

    // STRIKERS_BENCHMARK's demo run is measuring a match and has to get to one.
    PortBenchInit();
    if (PortBenchWantsDemo())
    {
        return 1;
    }

    v = getenv("STRIKERS_NO_MOVIES");
    return v != NULL && v[0] != '\0' && v[0] != '0';
}

#ifdef STRIKERS_FFMPEG

static AVCodecContext* s_vctx;
static AVPacket* s_pkt;
static AVFrame* s_frame;
static unsigned char* s_padded;
static size_t s_paddedCap;
static int s_reported;

// GX I8: 8x4 tiles, 32 bytes each, tiles left to right then top to bottom, and within a tile four
// rows of eight bytes.
static void tile_i8(unsigned char* dst, const unsigned char* src, int srcStride,
                    int width, int height)
{
    int y, x, yy;
    for (y = 0; y < height; y += 4)
    {
        for (x = 0; x < width; x += 8)
        {
            for (yy = 0; yy < 4; yy++, dst += 8)
            {
                memcpy(dst, src + (size_t)(y + yy) * (size_t)srcStride + (size_t)x, 8);
            }
        }
    }
}

// The frame as it sits in the read buffer has no decoder padding after it, and libavcodec's
// bitstream readers are allowed to read AV_INPUT_BUFFER_PADDING_SIZE bytes past the end of a
// packet.
static int copy_padded(const void* frame, size_t size)
{
    size_t need = size + AV_INPUT_BUFFER_PADDING_SIZE;
    if (need > s_paddedCap)
    {
        unsigned char* p = (unsigned char*)realloc(s_padded, need);
        if (p == NULL)
        {
            return 0;
        }
        s_padded = p;
        s_paddedCap = need;
    }
    memcpy(s_padded, frame, size);
    memset(s_padded + size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    return 1;
}

static void report_once(const char* what)
{
    if (!s_reported)
    {
        s_reported = 1;
        fprintf(stderr, "[port] THP video: %s; the movie will be skipped.\n", what);
    }
}

static s32 decode_into(const void* frame, u32 size, void* tileY, void* tileU, void* tileV)
{
    const unsigned char* p = (const unsigned char*)frame;
    int cw, ch;
    int rc;

    if (s_vctx == NULL || p == NULL)
    {
        return -1;
    }

    // Every THP video record is a whole JPEG. Refusing anything that does not start SOI keeps a
    // mis-parsed container from being handed to the decoder as if it were a picture.
    if (p[0] != 0xFF || p[1] != 0xD8)
    {
        report_once("frame does not start with a JPEG SOI marker");
        return -1;
    }

    if (size == 0)
    {
        // No size from the caller (the SDK-shaped entry point): find the end of the JPEG.
        u32 scan;
        const u32 cap = 1u << 20;
        for (scan = 2; scan + 1 < cap; scan++)
        {
            if (p[scan] == 0xFF && p[scan + 1] == 0xD9)
            {
                size = scan + 2;
                break;
            }
        }
        if (size == 0)
        {
            report_once("no JPEG EOI within 1 MB of the frame");
            return -1;
        }
    }

    if (!copy_padded(frame, size))
    {
        return -1;
    }

    s_pkt->data = s_padded;
    s_pkt->size = (int)size;
    rc = avcodec_send_packet(s_vctx, s_pkt);
    s_pkt->data = NULL;
    s_pkt->size = 0;
    if (rc < 0)
    {
        report_once("libavcodec rejected the frame");
        return -1;
    }

    rc = avcodec_receive_frame(s_vctx, s_frame);
    if (rc < 0)
    {
        report_once("libavcodec produced no picture for the frame");
        return -1;
    }

    if (s_frame->format != AV_PIX_FMT_YUVJ420P && s_frame->format != AV_PIX_FMT_YUV420P)
    {
        report_once("decoded frame is not 4:2:0 planar");
        av_frame_unref(s_frame);
        return -1;
    }

    // GCTextureSize computes an I8 texture as width*height with no rounding, so the destinations
    // are exactly that big: a plane whose width is not a multiple of 8 or whose height is not a
    // multiple of 4 would be written past its end.
    cw = (s_frame->width + 1) / 2;
    ch = (s_frame->height + 1) / 2;
    if ((s_frame->width & 7) || (s_frame->height & 3) || (cw & 7) || (ch & 3))
    {
        report_once("frame size does not tile into GX I8 8x4 blocks");
        av_frame_unref(s_frame);
        return -1;
    }

    if (tileY != NULL)
    {
        tile_i8((unsigned char*)tileY, s_frame->data[0], s_frame->linesize[0],
                s_frame->width, s_frame->height);
    }
    if (tileU != NULL)
    {
        tile_i8((unsigned char*)tileU, s_frame->data[1], s_frame->linesize[1], cw, ch);
    }
    if (tileV != NULL)
    {
        tile_i8((unsigned char*)tileV, s_frame->data[2], s_frame->linesize[2], cw, ch);
    }

    av_frame_unref(s_frame);
    return 0;
}

BOOL THPInit(void)
{
    const AVCodec* codec;

    port_thp_pace_reset();

    if (port_thp_movies_disabled())
    {
        return FALSE;
    }

    if (s_vctx != NULL)
    {
        // One decoder for the process. THP frames are all intra, so nothing carries over between
        // movies, but the reference frame the decoder holds does; flush it so a new movie starts
        // from nothing.
        avcodec_flush_buffers(s_vctx);
        s_reported = 0;
        return TRUE;
    }

    codec = avcodec_find_decoder(AV_CODEC_ID_THP);
    if (codec == NULL)
    {
        fprintf(stderr, "[port] THP video: this libavcodec has no `thp` decoder; "
                        "movies are skipped.\n");
        return FALSE;
    }

    s_vctx = avcodec_alloc_context3(codec);
    s_pkt = av_packet_alloc();
    s_frame = av_frame_alloc();
    if (s_vctx == NULL || s_pkt == NULL || s_frame == NULL)
    {
        fprintf(stderr, "[port] THP video: out of memory opening the decoder.\n");
        return FALSE;
    }

    // One thread on purpose: THPSimple's contract is one picture per call, and a frame-threaded
    // decoder answers the first sends with "not yet", which this file cannot tell from a failed
    // frame.
    s_vctx->thread_count = 1;

    if (avcodec_open2(s_vctx, codec, NULL) < 0)
    {
        fprintf(stderr, "[port] THP video: avcodec_open2 failed; movies are skipped.\n");
        avcodec_free_context(&s_vctx);
        return FALSE;
    }

    s_reported = 0;
    return TRUE;
}

// The size-carrying entry point. THPSimple knows the component's length from the frame record it
// just parsed, and passing it is strictly better than having this file scan for the end of the JPEG
// in a buffer whose bounds it cannot see.
s32 port_thp_video_decode(const void* frame, u32 size, void* tileY, void* tileU, void* tileV)
{
    return decode_into(frame, size, tileY, tileU, tileV);
}

// The SDK's shape, kept because dolphin/thp/THPVideoDecode.h declares it.
s32 THPVideoDecode(void* file, void* tileY, void* tileU, void* tileV, void* work)
{
    (void)work;
    return decode_into(file, 0, tileY, tileU, tileV);
}

#else // !STRIKERS_FFMPEG

// Built without ffmpeg. Reporting failure is deliberate rather than lazy: src/Game/Sys/movie.cpp
// already handles a movie that will not open, so a truthful "no decoder" answer makes the game skip
// its movies and carry on.

static void port_thp_report_once(void)
{
    static int reported = 0;
    if (!reported)
    {
        reported = 1;
        fprintf(stderr,
                "[port] THP video decoding was not built in (no ffmpeg); movies "
                "are skipped. See src/platform/thp.c.\n");
    }
}

BOOL THPInit(void)
{
    port_thp_pace_reset();
    if (!port_thp_movies_disabled())
    {
        // Nothing to complain about when they were switched off on purpose.
        port_thp_report_once();
    }
    return FALSE;
}

s32 port_thp_video_decode(const void* frame, u32 size, void* tileY, void* tileU, void* tileV)
{
    (void)frame;
    (void)size;
    (void)tileY;
    (void)tileU;
    (void)tileV;
    port_thp_report_once();
    return -1;
}

s32 THPVideoDecode(void* file, void* tileY, void* tileU, void* tileV, void* work)
{
    (void)file;
    (void)tileY;
    (void)tileU;
    (void)tileV;
    (void)work;
    port_thp_report_once();
    return -1;
}

#endif // STRIKERS_FFMPEG

// Audio: Nintendo DSP-ADPCM, two channels, one record per video frame.

static u32 rd_be32(const u8* p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static s16 rd_be16(const u8* p)
{
    return (s16)(((u32)p[0] << 8) | (u32)p[1]);
}

static s16 clip16(long long v)
{
    if (v < -32768)
        return (s16)-32768;
    if (v > 32767)
        return (s16)32767;
    return (s16)v;
}

// Writes `count` samples into `dst`, stepping `step` shorts per sample so the two channels can be
// interleaved into one buffer without a second pass.
static void decode_adpcm_channel(s16* dst, int step, const u8* nibbles,
                                 const s16* coef, s16 yn1, s16 yn2, u32 count)
{
    s32 s1 = yn1;
    s32 s2 = yn2;
    u32 n = 0;

    while (n < count)
    {
        u32 header = *nibbles++;
        s32 scale = 1 << (header & 0x0F);
        const s16* c = coef + ((header >> 4) & 7) * 2;
        u32 k;
        u8 byte = 0;

        for (k = 0; k < 14 && n < count; k++, n++)
        {
            s32 nib;
            // 64-bit accumulator: the console's DSP had a wider one than the 32 bits this
            // arithmetic would otherwise get, and a coefficient pair near the extremes can carry
            // past 2^31 before the shift.
            long long v;

            if (k & 1)
            {
                nib = (s32)(byte & 0x0F);
            }
            else
            {
                byte = *nibbles++;
                nib = (s32)(byte >> 4);
            }
            if (nib >= 8)
            {
                nib -= 16;
            }

            v = ((long long)(nib * scale) << 11) + 1024
                + (long long)s1 * (long long)c[0] + (long long)s2 * (long long)c[1];
            v >>= 11;
            s2 = s1;
            s1 = clip16(v);
            *dst = (s16)s1;
            dst += step;
        }
    }
}

u32 THPAudioDecode(s16* audioBuffer, u8* audioFrame, s32 flag)
{
    u32 offsetNextChannel;
    u32 sampleSize;
    s16 coefL[16];
    s16 coefR[16];
    s16 lyn1, lyn2, ryn1, ryn2;
    const u8* nibbles;
    int i;

    (void)flag; // the SDK's "decode into the second half of the buffer" flag;
                // THPSimple only ever passes 0.

    if (audioBuffer == NULL || audioFrame == NULL)
    {
        return 0;
    }

    offsetNextChannel = rd_be32(audioFrame);
    sampleSize = rd_be32(audioFrame + 4);
    if (sampleSize == 0)
    {
        return 0;
    }

    for (i = 0; i < 16; i++)
    {
        coefL[i] = rd_be16(audioFrame + 8 + i * 2);
        coefR[i] = rd_be16(audioFrame + 40 + i * 2);
    }
    lyn1 = rd_be16(audioFrame + 72);
    lyn2 = rd_be16(audioFrame + 74);
    ryn1 = rd_be16(audioFrame + 76);
    ryn2 = rd_be16(audioFrame + 78);

    nibbles = audioFrame + 80;

    decode_adpcm_channel(audioBuffer, 2, nibbles, coefL, lyn1, lyn2, sampleSize);

    if (offsetNextChannel != 0)
    {
        decode_adpcm_channel(audioBuffer + 1, 2, nibbles + offsetNextChannel, coefR,
                             ryn1, ryn2, sampleSize);
    }
    else
    {
        // Mono record: the TEV, the mixer and the buffer are all stereo, so duplicate rather than
        // leave one side silent.
        u32 n;
        for (n = 0; n < sampleSize; n++)
        {
            audioBuffer[n * 2 + 1] = audioBuffer[n * 2];
        }
    }

    return sampleSize;
}
