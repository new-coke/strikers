#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "port/native_runtime.hpp"
#include "port/native_jobs.hpp"
#include "port/native_pad.h"
#include "port/native_boot_screen.hpp"
#include "port/config.h"
#include "port/native_match_runtime.hpp"
#include <psp2/ctrl.h>
#include <psp2/power.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <cstdlib>
#include <cstdio>
namespace port::native_runtime
{
bool initialize() noexcept
{
    // Refresh idle timers before game initialization and resource preload.
    (void)sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    const int frontAffinity =
        sceKernelChangeThreadCpuAffinityMask(sceKernelGetThreadId(), SCE_KERNEL_CPU_MASK_USER_2);
    std::fprintf(stderr,
                 "[vitagl-runtime] Thread topology frontend=CPU2 renderer=CPU1 jobs/audio=CPU0 "
                 "affinity_rc=%08x\n",
                 unsigned(frontAffinity));
    if (frontAffinity < 0)
        return false;
    const char* cpu = std::getenv("STRIKERS_CPU_MHZ");
    const int mhz = cpu ? std::atoi(cpu) : 444;
    if (scePowerSetArmClockFrequency(mhz) < 0 || scePowerGetArmClockFrequency() < mhz)
        scePowerSetArmClockFrequency(444);
    scePowerSetGpuClockFrequency(222);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);
    port::jobs::initialize();
    std::fprintf(stderr,
                 "[vitagl-runtime] clocks requested_cpu=%d actual_cpu=%d gpu=%d bus=%d xbar=%d MHz "
                 "workers=%u\n",
                 mhz, scePowerGetArmClockFrequency(), scePowerGetGpuClockFrequency(),
                 scePowerGetBusClockFrequency(), scePowerGetGpuXbarClockFrequency(),
                 port::jobs::worker_threads());
    return true;
}
bool pump() noexcept
{
    // Match SDL Vita's default idle-timer handling while the game pumps events.
    (void)sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
    SceCtrlData c{};
    sceCtrlPeekBufferPositive(0, &c, 1);
    static SceUInt64 held = 0;
    const auto now = sceKernelGetSystemTimeWide();
    if ((c.buttons & SCE_CTRL_START) && (c.buttons & SCE_CTRL_SELECT))
    {
        if (!held)
            held = now;
        return now - held < 2000000;
    }
    held = 0;
    return true;
}
void shutdown() noexcept
{
    const int padStop = PortPadShutdown();
    if (padStop < 0)
        std::fprintf(stderr, "[vita-pad] actuator shutdown failed rc=%08x\n", unsigned(padStop));
    port::jobs::shutdown();
    port::startup::shutdown();
}
}   // namespace port::native_runtime
extern "C" void strikers_native_prepare_memory_write(const void*, size_t) noexcept {}
extern "C" void strikers_native_notify_memory_write(const void* p, size_t n) noexcept
{
    port::directmatch::notify_write(p, n);
}
#endif
