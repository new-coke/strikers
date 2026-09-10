
#if defined(PORT_USE_AURORA)

#include <math.h>
#include <stdlib.h>

#include <dolphin/card.h>
#include <dolphin/os.h>
#include <dolphin/gx.h>
#include <dolphin/types.h>

#include "port/aurora_compat.h"

void GXWaitDrawDone(void)
{
    GXDrawDone();
}

void GXClearGPMetric(void) {}
void GXSetGPMetric(u32 perf0, u32 perf1) { (void)perf0; (void)perf1; }

void GXReadGPMetric(u32* cnt0, u32* cnt1)
{
    if (cnt0) *cnt0 = 0;
    if (cnt1) *cnt1 = 0;
}

// Ten entries scaling fog depth by screen x. Left unwritten, glx_Send uploads stack garbage and the
// hardware paints flat fog over whole columns.
void GXInitFogAdjTable(void* table, u16 width, const f32 projmtx[4][4])
{
    GXFogAdjTable* adj = (GXFogAdjTable*)table;
    f32 nearZ;
    f32 sideX;
    u32 i;

    if (adj == NULL || width == 0)
        return;

    if (projmtx[3][3] == 0.0f)
    {
        nearZ = projmtx[2][3] / (projmtx[2][2] - 1.0f);
        sideX = nearZ / projmtx[0][0];
    }
    else
    {
        sideX = 1.0f / projmtx[0][0];
        nearZ = 1.73205f * sideX;
    }

    if (nearZ <= 0.0f)
    {
        // 256 is a factor of 1.0; the caller's table is a local, so it must be filled.
        for (i = 0; i < 10; i++)
            adj->r[i] = 256;
        return;
    }

    // The SDK's ten segments each cover a twentieth of the width, so it cancels: (i+1) * (width/20)
    // * (2/width) == (i+1)/10.
    for (i = 0; i < 10; i++)
    {
        f32 xi = (f32)(i + 1) * 0.1f * sideX;
        f32 rangeVal = sqrtf(1.0f + ((xi * xi) / (nearZ * nearZ)));
        adj->r[i] = (u16)((u32)(256.0f * rangeVal) & 0xFFF);
    }

    {
        static int logged = -1;
        if (logged < 0)
            logged = getenv("STRIKERS_LOG_FOG") != NULL ? 0 : 1;
        if (logged == 0)
        {
            logged = 1;
            OSReport("[fog] nearZ %f sideX %f table %u %u %u %u %u %u %u %u %u %u\n",
                     nearZ, sideX, adj->r[0], adj->r[1], adj->r[2], adj->r[3], adj->r[4],
                     adj->r[5], adj->r[6], adj->r[7], adj->r[8], adj->r[9]);
        }
    }
}

// Render modes Aurora does not define; they differ from NTSC only in scan and field configuration,
// which Aurora ignores.
GXRenderModeObj GXNtsc480Prog;
GXRenderModeObj GXEurgb60Hz480IntDf;

__attribute__((constructor)) static void port_init_render_modes(void)
{
    GXNtsc480Prog = GXNtsc480IntDf;
    GXEurgb60Hz480IntDf = GXNtsc480IntDf;
}


static u32 s_sampling_rate;

void SISetSamplingRate(u32 samplingRate)
{
    s_sampling_rate = samplingRate;
}


static PADSamplingCallback s_pad_sampling_cb;

PADSamplingCallback PADSetSamplingCallback(PADSamplingCallback callback)
{
    PADSamplingCallback prev = s_pad_sampling_cb;
    s_pad_sampling_cb = callback;
    return prev;
}

void PortInvokePadSamplingCallback(void)
{
    if (s_pad_sampling_cb)
        s_pad_sampling_cb();
}


#endif // PORT_USE_AURORA
