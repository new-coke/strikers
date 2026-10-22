#include "port/benchmark.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned long long now;
int PortDiagnosticsEnabled(void) { return 0; }
int PortFpsOverlayEnabled(void) { return 0; }
unsigned long long port_monotonic_ns(void) { return now; }
int main(int argc, char** argv)
{
    assert(argc == 2);
    const char* path = argv[1];
    setenv("STRIKERS_FRAME_CAPTURE", path, 1);
    setenv("STRIKERS_FRAME_CAPTURE_FRAMES", "8", 1);
    setenv("STRIKERS_FRAME_CAPTURE_SCENE", "cutscene", 1);
    setenv("STRIKERS_FRAME_CAPTURE_PLAY_ONLY", "0", 1);
    setenv("STRIKERS_FRAME_CAPTURE_MATCH_ONLY", "0", 1);
    PortBenchInit();
    const char* names[] = {"GameBegin", "GoalCaptainCelebration", "GoalCaptainCelebration", "PlayCupThrophy"};
    const unsigned states[] = {0x100, 0x100, 0x10, 0x100};
    for (unsigned i = 0; i < 4; ++i)
    {
        for (unsigned frame = 0; frame < 2; ++frame)
        {
            PortBenchSetScene(states[i], 2, names[i]);
            PortBenchFrameBegin(); now += 20000000; PortBenchAfterTasks();
            now += (frame ? 20000000 : 10000000); PortBenchFrameEnd();
        }
        PortBenchSetScene(2, 2, "Idle");
        PortBenchFrameBegin(); now += 5000000; PortBenchAfterTasks();
        now += 5000000; PortBenchFrameEnd();
    }
    PortBenchCaptureStatus status;
    PortBenchGetCaptureStatus(&status);
    assert(status.state == PORT_CAPTURE_COMPLETE && status.samples == 8);
    puts("cutscene capture excludes live play and retains intro/goal/replay/trophy identities");
}
