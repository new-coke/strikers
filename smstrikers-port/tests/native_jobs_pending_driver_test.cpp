#include "port/native_jobs.hpp"
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/cpu.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <thread>

namespace
{
struct Control
{
    std::mutex mutex;
    std::condition_variable changed;
    bool holdWorker = false, workerAtGate = false, callerFinished = false;
    bool callerNeedsWorker = false, workerInCallback = false, releaseCallback = false;
    unsigned readyWaits = 0, doneWaits = 0, readySpurious = 0, doneSpurious = 0;
    unsigned finishedJobs = 0;
    pthread_cond_t* readyCondition = nullptr;
    pthread_cond_t* doneCondition = nullptr;
    pthread_mutex_t* jobMutex = nullptr;
} control;
thread_local bool isWorker = false;
std::atomic<unsigned> affinityCalls{0}, priorityCalls{0};
unsigned checkedJobs = 0;

template<class Predicate>
void await(const char* label, Predicate predicate)
{
    std::unique_lock lock(control.mutex);
    if (!control.changed.wait_for(lock, std::chrono::seconds(10), predicate))
    {
        std::fprintf(stderr, "Timed out: %s\n", label);
        std::abort();
    }
}

void reset(bool holdWorker)
{
    assert(port::jobs::worker_threads() == 0);
    std::lock_guard lock(control.mutex);
    control.holdWorker = holdWorker;
    control.workerAtGate = control.callerFinished = false;
    control.callerNeedsWorker = control.workerInCallback = control.releaseCallback = false;
    control.readyWaits = control.doneWaits = control.readySpurious = control.doneSpurious = 0;
    control.finishedJobs = 0;
    control.readyCondition = control.doneCondition = nullptr;
    control.jobMutex = nullptr;
}

void release_worker()
{
    std::lock_guard lock(control.mutex);
    control.holdWorker = false;
    control.changed.notify_all();
}

void mark_finished()
{
    std::lock_guard lock(control.mutex);
    control.callerFinished = true;
    control.changed.notify_all();
}

// Wake a real pthread wait without changing its protected predicate. Acquiring
// the job mutex ensures the announced waiter has actually released that mutex.
void spurious_wake(bool worker)
{
    pthread_cond_t* condition;
    pthread_mutex_t* mutex;
    unsigned before;
    {
        std::lock_guard lock(control.mutex);
        condition = worker ? control.readyCondition : control.doneCondition;
        mutex = control.jobMutex;
        before = worker ? control.readyWaits : control.doneWaits;
    }
    assert(condition && mutex);
    assert(pthread_mutex_lock(mutex) == 0);
    assert(pthread_cond_broadcast(condition) == 0);
    assert(pthread_mutex_unlock(mutex) == 0);
    await("predicate recheck after spurious wake", [&] {
        return (worker ? control.readyWaits : control.doneWaits) > before;
    });
}

struct Work
{
    explicit Work(size_t size) : count(size), hits(new std::atomic<unsigned>[size]{}) {}
    size_t count;
    std::unique_ptr<std::atomic<unsigned>[]> hits;
    std::atomic<unsigned> laneMask{0}, calls{0};
    std::atomic<bool> callerHandshake{false};
    bool rendezvous = false;
    size_t failure = std::numeric_limits<size_t>::max();
    void verify(unsigned requiredMask = 0) const
    {
        for (size_t i = 0; i < count; ++i) assert(hits[i].load() == 1);
        if (requiredMask) assert(laneMask.load() == requiredMask);
        ++checkedJobs;
    }
};

bool populate(void* context, size_t begin, size_t end, unsigned lane) noexcept
{
    auto& work = *static_cast<Work*>(context);
    assert(begin < end && end <= work.count && lane < 2);
    work.laneMask.fetch_or(1u << lane);
    work.calls.fetch_add(1);
    if (work.rendezvous && lane == 0 && !work.callerHandshake.exchange(true))
    {
        {
            std::lock_guard lock(control.mutex);
            control.callerNeedsWorker = true;
            control.changed.notify_all();
        }
        await("worker acquires pending job", [] { return control.workerInCallback; });
    }
    if (work.rendezvous && lane == 1)
    {
        {
            std::lock_guard lock(control.mutex);
            control.workerInCallback = true;
            control.changed.notify_all();
        }
        await("release worker callback", [] { return control.releaseCallback; });
    }
    for (size_t i = begin; i < end; ++i) assert(work.hits[i].fetch_add(1) == 0);
    return !(begin <= work.failure && work.failure < end);
}

void pending_request(bool oldImplementation)
{
    using namespace port::jobs;
    reset(true);
    control.readySpurious = 3;
    assert(initialize());
    await("worker stopped before first job mutex", [] { return control.workerAtGate; });
    auto work = std::make_unique<Work>(170);
    std::thread caller([&] {
        assert(parallel_for(work->count, 48, populate, work.get()));
        mark_finished();
    });
    if (oldImplementation)
    {
        await("old caller waits for unused worker", [] { return control.doneWaits != 0; });
        {
            std::lock_guard lock(control.mutex);
            assert(!control.callerFinished && control.holdWorker);
        }
        release_worker();
    }
    else
    {
        await("new caller returns while worker is held", [] { return control.callerFinished; });
        std::lock_guard lock(control.mutex);
        assert(control.doneWaits == 0 && control.holdWorker);
    }
    caller.join();
    work->verify(1);
    assert(work->calls == 4);
    work.reset(); // The delayed worker must not access this released context.
    if (!oldImplementation) release_worker();
    await("worker rejects stale request and spurious returns", [] { return control.readyWaits >= 4; });
    spurious_wake(true);
    shutdown();
    shutdown();
    assert(worker_threads() == 0);
}

void acquired_request()
{
    using namespace port::jobs;
    reset(true);
    control.doneSpurious = 2;
    assert(initialize());
    await("worker initial gate", [] { return control.workerAtGate; });
    Work work(31);
    work.rendezvous = true;
    work.failure = 1; // Worker takes range 1 while caller holds range 0.
    std::thread caller([&] {
        assert(!parallel_for(work.count, 1, populate, &work));
        mark_finished();
    });
    await("caller holds its first range", [] { return control.callerNeedsWorker; });
    release_worker();
    await("caller still joins active worker after two spurious returns", [] { return control.doneWaits >= 3; });
    {
        std::lock_guard lock(control.mutex);
        assert(control.workerInCallback && !control.callerFinished && !control.releaseCallback);
    }
    spurious_wake(false);
    {
        std::lock_guard lock(control.mutex);
        assert(!control.callerFinished);
        control.releaseCallback = true;
        control.changed.notify_all();
    }
    caller.join();
    work.verify(3);
    assert(work.calls == work.count);
    shutdown();
}

void repeated_cancellation()
{
    using namespace port::jobs;
    for (unsigned cycle = 0; cycle < 2; ++cycle)
    {
        reset(true);
        assert(initialize());
        await("worker held across repeated jobs", [] { return control.workerAtGate; });
        std::thread caller([&] {
            for (unsigned sequence = 0; sequence < 64; ++sequence)
            {
                auto work = std::make_unique<Work>(10 + sequence);
                const bool fail = sequence % 5 == 0;
                if (fail) work->failure = work->count - 1;
                assert(parallel_for(work->count, 3, populate, work.get()) == !fail);
                work->verify(1);
                work.reset();
                std::lock_guard lock(control.mutex);
                ++control.finishedJobs;
            }
            mark_finished();
        });
        await("all cancelled jobs return before worker release", [] { return control.callerFinished; });
        caller.join();
        {
            std::lock_guard lock(control.mutex);
            assert(control.doneWaits == 0 && control.finishedJobs == 64 && control.holdWorker);
        }
        release_worker();
        await("worker idle after all contexts were freed", [] { return control.readyWaits != 0; });
        spurious_wake(true);
        shutdown();
    }
}

void ordinary_lifecycle()
{
    using namespace port::jobs;
    assert(!parallel_for(3, 1, nullptr, nullptr));
    assert(!parallel_for(3, 0, populate, nullptr));
    assert(parallel_for(0, 1, populate, nullptr));
    Work serial(11);
    assert(parallel_for(serial.count, 3, populate, &serial));
    serial.verify(1);
    for (unsigned cycle = 0; cycle < 3; ++cycle)
    {
        reset(false);
        assert(initialize() && initialize());
        assert(worker_threads() == 1 && game_execution_lanes() == 2);
        for (size_t n : {1u, 10u, 170u, 1000u})
            for (size_t step : {1u, 3u, 48u})
            {
                Work work(n);
                const bool fail = step == 3;
                if (fail) work.failure = n - 1;
                assert(parallel_for(n, step, populate, &work) == !fail);
                work.verify();
            }
        shutdown();
        assert(worker_threads() == 0 && game_execution_lanes() == 1);
    }
}
} // namespace

extern "C" {
SceUID sceKernelGetThreadId()
{
    isWorker = true;
    return 17;
}
int sceKernelChangeThreadCpuAffinityMask(SceUID id, int affinity)
{
    assert(isWorker && id == 17 && affinity == SCE_KERNEL_CPU_MASK_USER_0);
    affinityCalls.fetch_add(1);
    return 0;
}
int sceKernelChangeThreadPriority(SceUID id, int priority)
{
    assert(isWorker && id == 17 && priority == 0x10000100);
    priorityCalls.fetch_add(1);
    std::unique_lock lock(control.mutex);
    control.workerAtGate = true;
    control.changed.notify_all();
    if (!control.changed.wait_for(lock, std::chrono::seconds(10), [] { return !control.holdWorker; }))
        std::abort();
    return 0;
}
int jobs_test_cond_wait(pthread_cond_t* condition, pthread_mutex_t* mutex)
{
    bool spurious;
    {
        std::lock_guard lock(control.mutex);
        assert(!control.jobMutex || control.jobMutex == mutex);
        control.jobMutex = mutex;
        if (isWorker)
        {
            control.readyCondition = condition;
            ++control.readyWaits;
            spurious = control.readySpurious != 0;
            if (spurious) --control.readySpurious;
        }
        else
        {
            control.doneCondition = condition;
            ++control.doneWaits;
            spurious = control.doneSpurious != 0;
            if (spurious) --control.doneSpurious;
        }
        control.changed.notify_all();
    }
    // A spurious return still owns the same mutex, as required by cond_wait.
    return spurious ? 0 : pthread_cond_wait(condition, mutex);
}
}

int main(int argc, char** argv)
{
    assert(argc == 1 || (argc == 2 && std::strcmp(argv[1], "--expect-old") == 0));
    const bool oldImplementation = argc == 2;
    pending_request(oldImplementation);
    acquired_request();
    if (!oldImplementation) repeated_cancellation();
    ordinary_lifecycle();
    assert(affinityCalls == priorityCalls);
    assert(affinityCalls == (oldImplementation ? 5u : 7u));
    std::printf("Native jobs pending: %s, %u exactly-once jobs, active-worker join, "
                "spurious waits/wakes, failure and repeated lifecycle passed\n",
                oldImplementation ? "old caller waits for unused worker" : "unused worker cancelled before context release",
                checkedJobs);
}
