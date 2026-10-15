#include "port/shaders.h"

#if defined(PORT_USE_AURORA)

#include "port/host.h"

#include <stdio.h>

#include <aurora/gfx.h>


namespace
{

// Past this the game moves on anyway and the rest finish in the background.
constexpr double kCapSeconds = 120.0;

bool s_waiting;
bool s_reported;
unsigned long long s_waitStart;
unsigned s_initialQueued;

unsigned queued()
{
    return aurora_get_queued_pipeline_count();
}

double seconds_since(unsigned long long t)
{
    return (double)(port_monotonic_ns() - t) / 1e9;
}

} // namespace

void PortShaderStageBegin(void)
{
    s_initialQueued = queued();
    if (s_initialQueued == 0)
        fprintf(stderr, "[shaders] pipeline compile queue is empty\n");
    else
        fprintf(stderr, "[shaders] %u pipelines queued; the memory card screen waits for them\n",
                s_initialQueued);
}

int PortShaderStagePending(void)
{
    if (s_reported || queued() == 0)
        return 0;
    if (!s_waiting)
    {
        s_waiting = true;
        s_waitStart = port_monotonic_ns();
        fprintf(stderr, "[shaders] memory card screen waiting for %u pipelines\n", queued());
    }
    return seconds_since(s_waitStart) < kCapSeconds ? 1 : 0;
}

int PortShaderStagePercent(void)
{
    const unsigned waiting = queued();
    if (waiting == 0 || s_initialQueued == 0)
        return 100;
    const unsigned remaining = waiting < s_initialQueued ? waiting : s_initialQueued;
    return (int)(100ull * (s_initialQueued - remaining) / s_initialQueued);
}

void PortShaderStageEnd(void)
{
    if (s_reported)
        return;
    s_reported = true;
    if (!s_waiting)
        fprintf(stderr, "[shaders] pipeline queue was ready before the memory card screen finished\n");
    else if (queued() == 0)
        fprintf(stderr, "[shaders] pipeline queue ready; the memory card screen waited %.1f s\n",
                seconds_since(s_waitStart));
    else
        fprintf(stderr,
                "[shaders] %u pipelines still compiling after %.0f s; moving on, the rest finish "
                "in the background\n",
                queued(), seconds_since(s_waitStart));
}


#endif // PORT_USE_AURORA
