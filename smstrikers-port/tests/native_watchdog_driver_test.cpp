#include "port/native_watchdog.hpp"
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/cpu.h>
#include <psp2/io/fcntl.h>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Exercise the actual formatter, sampler, SDK calls and lifetime code. Only
// the kernel and file service are replaced; no production function is copied.
#include "../src/platform/native_watchdog.cpp"

namespace mock
{
using namespace std::chrono_literals;
constexpr SceUID Frontend = 10, Sampler = 20, Semaphore = 30, File = 40;
thread_local SceUID uid = Frontend;
std::mutex kernelMutex, ioMutex;
std::condition_variable kernelChanged, ioChanged;
std::thread thread;
SceKernelThreadEntry entry;
std::atomic<uint64_t> clockUs{1000000};
std::atomic<unsigned> calls{0}, queries{0}, waits{0}, writes{0}, closes{0}, deletes{0}, semaDeletes{0};
std::atomic<int> queryFailure{0};
bool started = false, ended = false, sema = false, blockedWrite = false, writerInside = false;
unsigned tokens = 0, ticks = 0;
int requestedPriority = 0;
unsigned priorityRejections = 0;
int waitFailure = 0, startFailure = 0, createFailure = 0, openFailure = 0;
int nextWrite = -1; // -1 full, -2 error, -3 zero, otherwise maximum bytes.
unsigned writeLimitCalls = 0;
std::string output, path;
std::vector<std::string> successfulLogs;
std::atomic<bool> maximumInfo{false};

template<class Predicate> void await(Predicate predicate)
{
    std::unique_lock lock(kernelMutex);
    assert(kernelChanged.wait_for(lock, 3s, predicate));
}
void await_write()
{
    std::unique_lock lock(ioMutex);
    assert(ioChanged.wait_for(lock, 3s, [] { return writerInside; }));
}
void reset(bool preserveOutput = false)
{
    assert(!thread.joinable());
    started = ended = sema = blockedWrite = writerInside = false;
    tokens = ticks = 0;
    requestedPriority = 0; priorityRejections = 0;
    waitFailure = startFailure = createFailure = openFailure = 0;
    nextWrite = -1; writeLimitCalls = 0;
    queryFailure.store(0); maximumInfo.store(false);
    if (!preserveOutput) output.clear();
    path.clear();
    calls = queries = waits = writes = closes = deletes = semaDeletes = 0;
    clockUs.fetch_add(10000000);
}
std::string text()
{
    std::lock_guard lock(ioMutex);
    return output;
}
void tick()
{
    std::lock_guard lock(kernelMutex);
    clockUs.fetch_add(1000000);
    ++ticks;
    kernelChanged.notify_all();
}
void await_wait(unsigned after = 0)
{
    await([after] { return waits.load() > after || ended; });
}
void finish()
{
    assert(port::watchdog::stop() == 0);
    assert(!port::watchdog::enabled());
    assert(!thread.joinable());
    assert(closes == 1 && deletes == 1 && semaDeletes == 1);
}
void keep()
{
    const auto log = text();
    assert(!log.empty());
    successfulLogs.push_back(log);
}
}

extern "C"
{
SceInt64 sceKernelGetSystemTimeWide(void) { ++mock::calls; return SceInt64(mock::clockUs.load()); }
int sceKernelGetThreadId(void) { ++mock::calls; return mock::uid; }
SceUID sceKernelGetProcessId(void) { ++mock::calls; return 1234; }
SceUID sceKernelCreateSema(const char*, SceUInt attr, int initial, int maximum, SceKernelSemaOptParam* options)
{
    ++mock::calls;
    assert(!attr && !initial && maximum == 1 && !options);
    mock::sema = true;
    return mock::Semaphore;
}
int sceKernelDeleteSema(SceUID id)
{
    ++mock::calls; ++mock::semaDeletes;
    assert(id == mock::Semaphore && mock::sema && (!mock::started || mock::ended));
    mock::sema = false;
    return 0;
}
int sceKernelSignalSema(SceUID id, int count)
{
    ++mock::calls;
    std::lock_guard lock(mock::kernelMutex);
    assert(id == mock::Semaphore && count == 1 && mock::sema);
    ++mock::tokens;
    mock::kernelChanged.notify_all();
    return 0;
}
int sceKernelWaitSema(SceUID id, int count, SceUInt* timeout)
{
    ++mock::calls;
    std::unique_lock lock(mock::kernelMutex);
    assert(mock::uid == mock::Sampler && id == mock::Semaphore && count == 1);
    assert(timeout && *timeout > 0 && *timeout <= 1000000);
    ++mock::waits;
    mock::kernelChanged.notify_all();
    assert(mock::kernelChanged.wait_for(lock, std::chrono::seconds(3), [] {
        return mock::tokens || mock::ticks || mock::waitFailure;
    }));
    if (mock::waitFailure) return mock::waitFailure;
    if (mock::tokens) { --mock::tokens; return 0; }
    --mock::ticks;
    return int(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
}
SceUID sceKernelCreateThread(const char*, SceKernelThreadEntry entry, int priority,
                            SceSize stack, SceUInt attr, int affinity,
                            const SceKernelThreadOptParam* options)
{
    ++mock::calls;
    assert(mock::uid == mock::Frontend && mock::text().find("\"kind\":\"boot\"") != std::string::npos);
    mock::requestedPriority = priority;
    // Candidate7 hardware rejected this value. The old mock accepted it by
    // equality alone, hiding the real startup failure. Other encoded priority
    // forms are outside this driver's contract; require an absolute priority.
    if (priority == 0x10000140)
    {
        ++mock::priorityRejections;
        return int(SCE_KERNEL_ERROR_ILLEGAL_PRIORITY);
    }
    assert(priority >= 64 && priority <= 191);
    assert(priority == 191 && stack == 64 * 1024 && !attr && !options);
    assert(affinity == SCE_KERNEL_CPU_MASK_USER_ALL);
    mock::entry = entry;
    return mock::createFailure ? mock::createFailure : mock::Sampler;
}
int sceKernelStartThread(SceUID id, SceSize bytes, void* args)
{
    ++mock::calls;
    assert(id == mock::Sampler && !bytes && !args && !mock::thread.joinable());
    assert(mock::text().find("\"kind\":\"boot\"") != std::string::npos);
    if (mock::startFailure) return mock::startFailure;
    mock::started = true;
    mock::thread = std::thread([] {
        mock::uid = mock::Sampler;
        assert(mock::entry(0, nullptr) == 0);
        std::lock_guard lock(mock::kernelMutex);
        mock::ended = true;
        mock::kernelChanged.notify_all();
    });
    return 0;
}
int sceKernelWaitThreadEnd(SceUID id, int*, SceUInt* timeout)
{
    ++mock::calls;
    assert(id == mock::Sampler && timeout && *timeout == 100000);
    std::unique_lock lock(mock::kernelMutex);
    if (!mock::kernelChanged.wait_for(lock, std::chrono::microseconds(*timeout), [] { return mock::ended; }))
        return int(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
    lock.unlock();
    if (mock::thread.joinable()) mock::thread.join();
    return 0;
}
int sceKernelDeleteThread(SceUID id)
{
    ++mock::calls; ++mock::deletes;
    assert(id == mock::Sampler && (!mock::started || mock::ended));
    return 0;
}
int sceKernelGetThreadInfo(SceUID id, SceKernelThreadInfo* info)
{
    ++mock::calls; ++mock::queries;
    assert(mock::uid == mock::Sampler && info && info->size == sizeof(*info));
    if (mock::queryFailure) { std::memset(info, 0xab, sizeof(*info)); return mock::queryFailure; }
    info->processId = 1234;
    info->status = id == mock::Frontend ? SCE_THREAD_WAITING : SCE_THREAD_SUSPENDED;
    info->waitType = 6;
    info->waitId = 0x1234;
    info->runClocks = 0x123456789abcdef0ull;
    info->intrPreemptCount = 11;
    info->threadPreemptCount = 22;
    info->threadReleaseCount = 33;
    if (mock::maximumInfo)
    {
        info->processId = info->waitId = std::numeric_limits<int32_t>::min();
        info->status = info->waitType = info->intrPreemptCount = info->threadPreemptCount =
            info->threadReleaseCount = std::numeric_limits<uint32_t>::max();
        info->runClocks = std::numeric_limits<uint64_t>::max();
    }
    return 0;
}
SceUID sceIoOpen(const char* path, int flags, SceMode mode)
{
    ++mock::calls;
    assert(mock::uid == mock::Frontend);
    assert(flags == (SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND) && mode == 0666);
    mock::path = path;
    return mock::openFailure ? mock::openFailure : mock::File;
}
SceSSize sceIoWrite(SceUID fd, const void* bytes, SceSize size)
{
    ++mock::calls; ++mock::writes;
    assert(fd == mock::File && bytes && size <= 2048);
    std::unique_lock lock(mock::ioMutex);
    if (mock::blockedWrite && mock::uid == mock::Sampler)
    {
        mock::writerInside = true;
        mock::ioChanged.notify_all();
        assert(mock::ioChanged.wait_for(lock, std::chrono::seconds(3), [] { return !mock::blockedWrite; }));
    }
    int limit = -1;
    if (mock::writeLimitCalls) { --mock::writeLimitCalls; limit = mock::nextWrite; }
    if (limit == -2) return -123;
    if (limit == -3) return 0;
    const auto written = limit < 0 ? size : std::min(size, SceSize(limit));
    mock::output.append(static_cast<const char*>(bytes), written);
    return SceSSize(written);
}
int sceIoClose(SceUID fd)
{
    ++mock::calls; ++mock::closes;
    assert(fd == mock::File);
    return 0;
}
}

int main(int argc, char** argv)
{
    using namespace port::watchdog;
    if (argc == 2 && std::strcmp(argv[1], "--expect-invalid-priority") == 0)
    {
        // Run this mode against the retained pre-fix production source. It
        // proves the actual start() emits BOOT/start_fail and cleans up without
        // a sampler, rather than merely testing the mock's return value.
        mock::reset();
        assert(!start("1", "ux0:data/strikersVita/priority-rejected.log"));
        assert(error_code() == int(SCE_KERNEL_ERROR_ILLEGAL_PRIORITY));
        assert(mock::requestedPriority == 0x10000140 && mock::priorityRejections == 1);
        assert(!enabled() && !mock::started && !mock::queries);
        assert(mock::closes == 1 && mock::deletes == 0 && mock::semaDeletes == 1);
        const auto rejected = mock::text();
        assert(rejected.find("\"kind\":\"boot\"") != std::string::npos);
        assert(rejected.find("\"kind\":\"start_fail\"") != std::string::npos);
        assert(rejected.find("\"kind\":\"run\"") == std::string::npos);
        std::fwrite(rejected.data(), 1, rejected.size(), stdout);
        std::puts("{\"watchdog_priority_rejection\":\"PASS\",\"error\":-2147319773}");
        return 0;
    }
    assert(argc == 1);
    mock::reset();
    // OFF never touches the kernel/files, clocks, producer mirrors or creates a worker.
    for (const char* flag : {static_cast<const char*>(nullptr), "", "0", "true", "01", "1x"})
        assert(!start(flag, "ux0:data/strikersVita/off.log"));
    enter_thread(Role::Frontend, 777); stage(Role::Frontend, Stage::NativeAudioInit);
    progress(Role::Frontend, 42); frame(3); task(123, 4, 5); advance(Role::Frontend);
    { Scope s(Role::Frontend, Stage::Tasks); CommandScope c(Command::Draw); }
    assert(!enabled() && mock::calls == 0 && channel(Role::Frontend).uid == 0);
    for (const char* bad : {static_cast<const char*>(nullptr), "", "other/run.log",
                           "ux0:data/strikersVita/../run.log", "ux0:data/strikersVita/a/b.log",
                           "ux0:data/strikersVita/a\".log", "ux0:data/strikersVita/run.txt"})
        assert(!start("1", bad));
    assert(mock::calls == 0);

    mock::reset();
    assert(start("1", "ux0:data/strikersVita/boot-a.log"));
    assert(mock::requestedPriority == 191 && !mock::priorityRejections);
    assert(mock::path == "ux0:data/strikersVita/boot-a.watchdog.jsonl");
    mock::await_wait();
    // BOOT exists even if native audio initialization has not returned; the
    // sampler progresses while an unrelated engine/audio lock stays held.
    std::mutex engineLock;
    std::unique_lock blockedEngine(engineLock);
    stage(Role::Frontend, Stage::NativeAudioInit);
    enter_thread(Role::AudioOutput, 11);
    enter_thread(Role::Renderer, 12);
    stage(Role::AudioOutput, Stage::AudioStatsLock);
    progress(Role::AudioOutput, 123, 456);
    frame(900); task(0x81001234, 4, 2);
    const unsigned before = mock::waits;
    mock::tick(); mock::await_wait(before);
    const auto first = mock::text();
    assert(first.find("\"stage\":\"native_audio_init\"") != std::string::npos);
    assert(first.find("\"stage\":\"audio_stats_lock\"") != std::string::npos);
    assert(first.find("\"kernel\":[0,1234,8,6,4660,1311768467463790320,11,22,33]") != std::string::npos);
    assert(first.find("\"kernel\":[0,1234,256,6,4660,1311768467463790320,11,22,33]") != std::string::npos);
    assert(first.find("\"frame\":900,\"task\":2164265524,\"state\":4,\"pending\":2") != std::string::npos);
    {
        Scope outer(Role::Frontend, Stage::Tasks, 7);
        { Scope inner(Role::Frontend, Stage::FrontendDrain); }
        assert(channel(Role::Frontend).stage == uint32_t(Stage::Tasks));
        assert(channel(Role::Frontend).detail == 7);
    }
    assert(channel(Role::Frontend).stage == uint32_t(Stage::NativeAudioInit));
    { CommandScope command(Command::CreateTexture); }
    assert(channel(Role::Renderer).command == uint32_t(Command::CreateTexture));
    assert(channel(Role::Renderer).progress == 1);
    blockedEngine.unlock();
    mock::finish(); mock::keep();

    // The same label appends a distinct BOOT/session and never truncates prior evidence.
    const auto prior = mock::text();
    mock::reset(true);
    assert(start("1", "ux0:data/strikersVita/boot-a.log"));
    mock::await_wait(); mock::finish();
    assert(mock::text().starts_with(prior));
    assert(mock::text().find("\"instance\":2") != std::string::npos);
    mock::keep();

    mock::reset();
    // Two legal short writes are completed, preserving one complete BOOT.
    mock::nextWrite = 17; mock::writeLimitCalls = 2;
    assert(start("1", "ux0:/data/strikersVita/short.log"));
    assert(mock::path == "ux0:/data/strikersVita/short.watchdog.jsonl");
    mock::await_wait(); mock::finish(); mock::keep();
    for (int failure : {-2, -3, 1})
    {
        mock::reset(); mock::nextWrite = failure; mock::writeLimitCalls = 5;
        assert(!start("1", "ux0:data/strikersVita/write-fail.log"));
        assert(!enabled() && mock::closes == 1 && !mock::started);
        assert(mock::writes == (failure == 1 ? 4 : 1));
        if (failure == 1)
        {
            const auto partial = mock::text();
            mock::reset(true);
            assert(start("1", "ux0:data/strikersVita/write-fail.log"));
            mock::await_wait(); mock::finish();
            assert(mock::text().starts_with(partial + "\n{\"v\":1"));
            // The previous partial line is explicitly outside the new session;
            // validate the complete records after its resynchronizing newline.
            mock::successfulLogs.push_back(mock::text().substr(partial.size()));
        }
    }
    mock::reset(); mock::startFailure = -99;
    assert(!start("1", "ux0:data/strikersVita/start-fail.log"));
    assert(mock::closes == 1 && mock::deletes == 1 && mock::semaDeletes == 1);
    assert(error_code() == -99); mock::keep();
    mock::reset(); mock::createFailure = -98;
    assert(!start("1", "ux0:data/strikersVita/create-fail.log"));
    assert(mock::closes == 1 && mock::deletes == 0 && mock::semaDeletes == 1);
    assert(error_code() == -98); mock::keep();
    mock::reset(); mock::openFailure = -97;
    assert(!start("1", "ux0:data/strikersVita/open-fail.log"));
    assert(!mock::writes && !mock::closes && error_code() == -97);

    mock::reset(); mock::queryFailure = -88;
    assert(start("1", "ux0:data/strikersVita/query-fail.log"));
    mock::await_wait(); mock::finish();
    assert(mock::text().find("\"kernel\":[-88]") != std::string::npos);
    assert(mock::text().find("2880154539") == std::string::npos); // driver poison is never serialized.
    mock::keep();
    mock::reset();
    assert(start("1", "ux0:data/strikersVita/wait-fail.log"));
    mock::await_wait();
    { std::lock_guard lock(mock::kernelMutex); mock::waitFailure = -77; mock::kernelChanged.notify_all(); }
    mock::await([] { return mock::ended; });
    assert(stop() == -77 && !mock::thread.joinable() && mock::closes == 1);
    assert(mock::text().find("\"kind\":\"wait_error\"") != std::string::npos);
    mock::keep();

    mock::reset();
    assert(start("1", "ux0:data/strikersVita/runtime-write-fail.log"));
    mock::await_wait();
    { std::lock_guard lock(mock::ioMutex); mock::nextWrite = -2; mock::writeLimitCalls = 1; }
    mock::tick();
    mock::await([] { return mock::ended; });
    assert(!enabled() && stop() == -123 && mock::closes == 1 && mock::deletes == 1);
    mock::keep();

    // A stalled file service cannot turn shutdown into an unbounded join or
    // free the semaphore/storage still reachable by the sampler.
    mock::reset(); mock::blockedWrite = true;
    assert(start("1", "ux0:data/strikersVita/blocked.log"));
    mock::await_write();
    assert(stop() == int(SCE_KERNEL_ERROR_WAIT_TIMEOUT));
    assert(!enabled() && !mock::closes && !mock::deletes && !mock::semaDeletes);
    assert(!start("1", "ux0:data/strikersVita/rejected-restart.log"));
    { std::lock_guard lock(mock::ioMutex); mock::blockedWrite = false; mock::ioChanged.notify_all(); }
    mock::await([] { return mock::ended; });
    assert(stop() == int(SCE_KERNEL_ERROR_WAIT_TIMEOUT));
    assert(!mock::thread.joinable() && mock::closes == 1 && mock::deletes == 1 && mock::semaDeletes == 1);
    mock::keep();

    // Maximum producer/kernel values, a maximum path label and an interrupted
    // UID publication remain bounded and parseable, with no seqlock spinning.
    mock::reset();
    const std::string longLabel(80, 'x');
    assert(start("1", ("ux0:data/strikersVita/" + longLabel + ".log").c_str()));
    mock::await_wait();
    mock::maximumInfo = true;
    for (unsigned i = 0; i < unsigned(Role::Count); ++i)
    {
        auto& c = channel(Role(i));
        enter_thread(Role(i), std::numeric_limits<int32_t>::max());
        c.epoch = 0xfffffffeu; c.live = 1; c.detail = c.progress = c.auxiliary = 0xffffffffu;
        c.frame = c.task = c.state = c.pending = 0xffffffffu;
        stage(Role(i), Stage::ShutdownAudioLoader, 0xffffffffu);
    }
    channel(Role::Renderer).command = uint32_t(Command::InitializeScene);
    unsigned waited = mock::waits;
    mock::tick(); mock::await_wait(waited);
    assert(enabled() && error_code() == 0);
    assert(mock::text().find("18446744073709551615") != std::string::npos);
    channel(Role::Jobs).epoch = 3; // Publisher was interrupted before committing its UID.
    waited = mock::waits;
    mock::tick(); mock::await_wait(waited);
    assert(enabled() && mock::text().find("\"kernel\":null,\"changing\":1") != std::string::npos);
    mock::finish(); mock::keep();

    for (const auto& log : mock::successfulLogs)
        std::fwrite(log.data(), 1, log.size(), stdout);
    std::puts("{\"watchdog_test\":\"PASS\",\"cases\":17}");
    return 0;
}
