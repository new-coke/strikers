#include "port/disc_reader.h"

#include <condition_variable>
#if defined(__vita__)
#include <psp2/kernel/threadmgr.h>
#endif
#include <mutex>
#include <thread>

namespace
{

struct Job
{
    void (*work)(void*);
    void* ctx;
};

// Larger than the DVD layer's pending pool, so callers block only if that pool grows.
constexpr size_t QueueSize = 64;

std::mutex g_mutex;
std::condition_variable g_work;
std::condition_variable g_space;
Job g_queue[QueueSize];
size_t g_head = 0;
size_t g_tail = 0;
// Never joined; destroying a joinable std::thread at exit would terminate the process.
std::thread* g_thread = nullptr;

void reader_main()
{
#if defined(__vita__)
    // CPU0, below both audio threads.
    sceKernelChangeThreadCpuAffinityMask(sceKernelGetThreadId(), SCE_KERNEL_CPU_MASK_USER_0);
    sceKernelChangeThreadPriority(sceKernelGetThreadId(), 0x10000140);
#endif
    for (;;)
    {
        Job job;
        {
            std::unique_lock<std::mutex> lock{g_mutex};
            g_work.wait(lock, [] { return g_head != g_tail; });
            job = g_queue[g_tail];
            g_tail = (g_tail + 1) % QueueSize;
        }
        g_space.notify_one();
        job.work(job.ctx);
    }
}

} // namespace

extern "C" void PortDiscQueue(void (*work)(void*), void* ctx)
{
    std::unique_lock<std::mutex> lock{g_mutex};
    if (g_thread == nullptr)
        g_thread = new std::thread(reader_main);
    g_space.wait(lock, [] { return (g_head + 1) % QueueSize != g_tail; });
    g_queue[g_head].work = work;
    g_queue[g_head].ctx = ctx;
    g_head = (g_head + 1) % QueueSize;
    g_work.notify_one();
}

extern "C" void* PortDiscSpawn(void (*work)(void*), void* ctx)
{
    return new std::thread(work, ctx);
}

extern "C" void PortDiscJoin(void* thread)
{
    std::thread* t = static_cast<std::thread*>(thread);
    if (t == nullptr)
        return;
    t->join();
    delete t;
}
