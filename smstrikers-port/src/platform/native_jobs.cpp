#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "port/native_jobs.hpp"
#include "port/native_watchdog.hpp"
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/cpu.h>
#include <cstdio>
#include <pthread.h>
#include <atomic>
#include <algorithm>
namespace port::jobs
{
namespace
{
pthread_t worker{};
bool started = false;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ready = PTHREAD_COND_INITIALIZER, done = PTHREAD_COND_INITIALIZER;
bool stopping = false, pending = false, completed = true;
Task function = nullptr;
void* context = nullptr;
size_t count = 0, grain = 0;
std::atomic<size_t> next{ 0 };
std::atomic<bool> success{ true };
void run(unsigned lane)
{
    for (;;)
    {
        size_t begin = next.fetch_add(grain, std::memory_order_relaxed);
        if (begin >= count)
            break;
        if (!function(context, begin, std::min(begin + grain, count), lane))
            success.store(false);
    }
}
void* worker_main(void*)
{
    // pthread_create registers the VitaSDK TLS record required by cond_wait and libc.
    const SceUID id = sceKernelGetThreadId();
    watchdog::enter_thread(watchdog::Role::Jobs, id);
    // Core 3 when CapUnlocker grants it, else core 0.
    int affinity = sceKernelChangeThreadCpuAffinityMask(id, SCE_KERNEL_CPU_MASK_SYSTEM);
    if (affinity < 0)
        affinity = sceKernelChangeThreadCpuAffinityMask(id, SCE_KERNEL_CPU_MASK_USER_0);
    if (affinity < 0)
        std::fprintf(stderr, "[jobs] affinity error=%08x\n", unsigned(affinity));
    sceKernelChangeThreadPriority(id, 0x10000100);
    watchdog::stage(watchdog::Role::Jobs, watchdog::Stage::JobsPublish);
    pthread_mutex_lock(&mutex);
    for (;;)
    {
        while (!pending && !stopping)
        {
            watchdog::stage(watchdog::Role::Jobs, watchdog::Stage::JobsWait);
            pthread_cond_wait(&ready, &mutex);
        }
        if (stopping)
            break;
        pending = false;
        pthread_mutex_unlock(&mutex);
        watchdog::stage(watchdog::Role::Jobs, watchdog::Stage::JobsRun);
        run(1);
        watchdog::advance(watchdog::Role::Jobs);
        watchdog::stage(watchdog::Role::Jobs, watchdog::Stage::JobsPublish);
        pthread_mutex_lock(&mutex);
        completed = true;
        pthread_cond_signal(&done);
    }
    pthread_mutex_unlock(&mutex);
    watchdog::leave_thread(watchdog::Role::Jobs);
    return nullptr;
}
}   // namespace
bool initialize() noexcept
{
    if (started)
        return true;
    pthread_attr_t attr;
    int error = pthread_attr_init(&attr);
    if (error)
        return false;
    error = pthread_attr_setstacksize(&attr, 0x80000);
    stopping = pending = false;
    completed = true;
    if (!error)
        error = pthread_create(&worker, &attr, worker_main, nullptr);
    pthread_attr_destroy(&attr);
    if (error)
    {
        std::fprintf(stderr, "[jobs] pthread create error=%d\n", error);
        return false;
    }
    started = true;
    return true;
}
void shutdown() noexcept
{
    if (!started)
        return;
    pthread_mutex_lock(&mutex);
    stopping = true;
    pthread_cond_signal(&ready);
    pthread_mutex_unlock(&mutex);
    pthread_join(worker, nullptr);
    started = false;
}
unsigned worker_threads() noexcept { return started ? 1 : 0; }
unsigned game_execution_lanes() noexcept { return started ? 2 : 1; }
bool parallel_for(size_t n, size_t step, Task task, void* opaque) noexcept
{
    if (!task || !step)
        return false;
    if (!n)
        return true;
    if (!started || n <= step)
        return task(opaque, 0, n, 0);
    pthread_mutex_lock(&mutex);
    function = task;
    context = opaque;
    count = n;
    grain = step;
    next.store(0);
    success.store(true);
    completed = false;
    pending = true;
    pthread_cond_signal(&ready);
    pthread_mutex_unlock(&mutex);
    run(0);
    watchdog::Scope watchdogJoin(watchdog::Role::Frontend, watchdog::Stage::JobsJoin);
    pthread_mutex_lock(&mutex);
    if (pending)
    {
        // The worker has not claimed this job under the mutex. The caller
        // drained every range, so cancel the unused wake before returning.
        pending = false;
        completed = true;
    }
    while (!completed)
        pthread_cond_wait(&done, &mutex);
    pthread_mutex_unlock(&mutex);
    return success.load();
}
}   // namespace port::jobs
#endif
