#include "port/native_jobs.hpp"
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/cpu.h>
#include <atomic>
#include <cassert>
#include <pthread.h>
#include <vector>
#include <cstdio>
namespace
{
std::atomic<unsigned> affinityCalls{ 0 };
pthread_t caller;
}   // namespace
extern "C" {
SceUID sceKernelGetThreadId()
{
    assert(!pthread_equal(pthread_self(), caller));
    return 17;
}
// Core 3 (the SYSTEM mask) is refused unless CapUnlocker is simulated.
std::atomic<bool> core3Granted{ false };
int sceKernelChangeThreadCpuAffinityMask(SceUID id, int affinity)
{
    assert(id == 17);
    if (affinity == SCE_KERNEL_CPU_MASK_SYSTEM)
        return core3Granted ? 0 : -1;
    assert(affinity == SCE_KERNEL_CPU_MASK_USER_0);
    affinityCalls.fetch_add(1);
    return 0;
}
int sceKernelChangeThreadPriority(SceUID id, int priority)
{
    assert(id == 17 && priority == 0x10000100);
    return 0;
}
}
struct Work
{
    std::vector<unsigned> values;
    std::atomic<unsigned> lanes{ 0 }, calls{ 0 };
};
bool populate(void* ptr, size_t begin, size_t end, unsigned lane) noexcept
{
    auto& w = *static_cast<Work*>(ptr);
    w.lanes.fetch_or(1u << lane);
    for (size_t i = begin; i < end; ++i)
    {
        assert(w.values[i] == 0);
        w.values[i] = unsigned(i + 1);
    }
    w.calls.fetch_add(1);
    return true;
}
bool fail(void*, size_t, size_t, unsigned) noexcept { return false; }
int main()
{
    using namespace port::jobs;
    caller = pthread_self();
    assert(worker_threads() == 0);
    Work serial;
    serial.values.resize(100);
    assert(parallel_for(100, 3, populate, &serial));
    for (unsigned i = 0; i < 100; ++i)
        assert(serial.values[i] == i + 1);
    for (unsigned cycle = 0; cycle < 3; ++cycle)
    {
        core3Granted = cycle == 2;
        assert(initialize() && worker_threads() == 1 && game_execution_lanes() == 2);
        assert(initialize());
        for (unsigned n : { 1u, 10u, 1000u, 10000u })
        {
            Work w;
            w.values.resize(n);
            assert(parallel_for(n, 3, populate, &w));
            for (unsigned i = 0; i < n; ++i)
                assert(w.values[i] == i + 1);
        }
        assert(!parallel_for(1000, 1, fail, nullptr));
        assert(!parallel_for(10, 0, fail, nullptr));
        assert(parallel_for(0, 1, fail, nullptr));
        shutdown();
        assert(!worker_threads());
        // Core 0 is requested only when core 3 is refused.
        assert(affinityCalls == (cycle < 2 ? cycle + 1 : 2u));
    }
    std::puts("Native jobs: real pthread create/join, affinity, complete-before-return, repeated "
              "lifecycle, failure propagation and serial fallback passed");
}
