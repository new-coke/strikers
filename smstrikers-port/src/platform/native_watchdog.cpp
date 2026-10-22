#include "port/native_watchdog.hpp"
#if defined(STRIKERS_WATCHDOG_ENABLED_BUILD)
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/cpu.h>
#include <psp2/io/fcntl.h>
#include <cstddef>

namespace port::watchdog
{
std::atomic<uint32_t> enabledFlag{0};
Channel channels[static_cast<unsigned>(Role::Count)];
namespace
{
constexpr unsigned RoleCount = static_cast<unsigned>(Role::Count);
constexpr unsigned RecordBytes = 2048;
constexpr uint64_t PeriodUs = 1000000;
constexpr uint32_t StopTimeoutUs = 100000;
// Absolute native priority: SDL Vita's LOW and the installed VitaSDK pthread
// backend's lowest endpoint. Larger native numbers have lower priority. Do not
// extend the SDK's encoded 0x10000100 example by adding an arbitrary offset.
constexpr int SamplerPriority = 191;
constexpr int InvalidConfiguration = -22, Busy = -16, WriteFailure = -5, RecordOverflow = -75;
struct View
{
    uint32_t uid, epoch, live, stage, detail, command, progress, auxiliary, frame, task, state, pending;
    bool changing;
    bool operator==(const View&) const = default;
};
struct State
{
    // Never release this storage while a timed-out worker can still use it.
    SceUID descriptor = -1, semaphore = -1, thread = -1, process = -1, frontend = -1;
    uint64_t boot = 0, sequence = 0, lastSample = 0, lastWriteUs = 0;
    uint32_t instance = 0;
    char label[81]{}, path[160]{};
    std::atomic<uint32_t> stopping{0}, finished{0};
    std::atomic<int32_t> error{0};
    View previous[RoleCount]{};
    uint64_t unchangedSince[RoleCount]{};
} state;
void failure(int code) noexcept
{
    int32_t expected = 0;
    state.error.compare_exchange_strong(expected, code < 0 ? code : WriteFailure,
                                        std::memory_order_relaxed);
}
struct Text
{
    char data[RecordBytes];
    unsigned size = 0;
    bool valid = true;
    void character(char c) noexcept
    {
        if (size < sizeof(data)) data[size++] = c;
        else valid = false;
    }
    void string(const char* text) noexcept { while (*text) character(*text++); }
    void number(uint64_t value) noexcept
    {
        char digits[20];
        unsigned count = 0;
        do { digits[count++] = char('0' + value % 10); value /= 10; } while (value);
        while (count) character(digits[--count]);
    }
    void signed_number(int32_t value) noexcept
    {
        if (value < 0) { character('-'); number(uint64_t(-int64_t(value))); }
        else number(uint32_t(value));
    }
    void field(const char* name, uint64_t value) noexcept
    {
        string(",\""); string(name); string("\":"); number(value);
    }
    void signed_field(const char* name, int32_t value) noexcept
    {
        string(",\""); string(name); string("\":"); signed_number(value);
    }
    void word(const char* name, const char* value) noexcept
    {
        string(",\""); string(name); string("\":\""); string(value); character('"');
    }
};
constexpr const char* roleNames[]{
    "frontend", "renderer", "jobs", "audio_output", "audio_loader"
};
constexpr const char* stageNames[]{
    "idle", "boot", "verify_archive", "native_audio_init", "native_audio_ready", "runtime_init",
    "game_init", "limiter", "host_input", "debug", "begin_frame", "pad", "tasks",
    "task_transition", "task_run", "audio_update", "end_frame", "snapshot", "capture",
    "frame_end", "exit_requested", "shutdown_audio", "shutdown_audio_loader",
    "shutdown_renderer", "shutdown_runtime", "renderer_init", "renderer_wait",
    "renderer_batch", "renderer_command", "renderer_loading", "renderer_mutex", "renderer_stop",
    "frontend_publish", "frontend_drain", "jobs_wait", "jobs_run", "jobs_publish", "jobs_join",
    "audio_mix", "audio_output", "audio_dump", "audio_stats_lock", "audio_stats", "audio_log",
    "audio_loop", "loader_lock", "loader_wait", "loader_read", "loader_publish", "loader_log",
    "exited"
};
constexpr const char* commandNames[]{
    "none", "initialize_scene", "create_texture", "create_target", "create_model",
    "release_texture", "release_model", "draw", "select_target", "copy_texture", "copy_textures",
    "clear_color", "clear_depth", "set_black", "present_bitmap", "set_loading", "begin_frame",
    "end_frame", "readback"
};
static_assert(sizeof(stageNames) / sizeof(*stageNames) == static_cast<unsigned>(Stage::Count));
static_assert(sizeof(commandNames) / sizeof(*commandNames) == static_cast<unsigned>(Command::Count));
const char* stage_name(uint32_t value) noexcept
{
    return value < static_cast<unsigned>(Stage::Count) ? stageNames[value] : "unknown";
}
const char* command_name(uint32_t value) noexcept
{
    return value < static_cast<unsigned>(Command::Count) ? commandNames[value] : "unknown";
}
View observe(unsigned index) noexcept
{
    auto& c = channels[index];
    const auto first = c.epoch.load(std::memory_order_acquire);
    View v{c.uid.load(std::memory_order_relaxed), first,
           c.live.load(std::memory_order_acquire), c.stage.load(std::memory_order_relaxed),
           c.detail.load(std::memory_order_relaxed), c.command.load(std::memory_order_relaxed),
           c.progress.load(std::memory_order_relaxed), c.auxiliary.load(std::memory_order_relaxed),
           c.frame.load(std::memory_order_relaxed), c.task.load(std::memory_order_relaxed),
           c.state.load(std::memory_order_relaxed), c.pending.load(std::memory_order_relaxed), false};
    v.changing = (first & 1) || first != c.epoch.load(std::memory_order_acquire);
    return v; // Deliberately never spin waiting for an interrupted publisher.
}
bool write_record(const Text& text) noexcept
{
    if (!text.valid) { failure(RecordOverflow); return false; }
    unsigned offset = 0;
    // A regular-file write normally completes in one call. Bound pathological
    // short writes too: no diagnostic failure may enter an infinite retry.
    for (unsigned attempt = 0; attempt < 4 && offset < text.size; ++attempt)
    {
        const auto written = sceIoWrite(state.descriptor, text.data + offset, text.size - offset);
        if (written <= 0 || uint64_t(written) > text.size - offset)
        {
            failure(written < 0 ? int(written) : WriteFailure);
            return false;
        }
        offset += unsigned(written);
    }
    if (offset != text.size) { failure(WriteFailure); return false; }
    return true;
}
uint64_t clock_value() noexcept
{
    const auto now = sceKernelGetSystemTimeWide();
    return now > 0 ? uint64_t(now) : 0;
}
void prefix(Text& text, const char* kind, uint64_t now) noexcept
{
    // Leading newline also separates a previous process's truncated last line.
    text.string("\n{\"v\":1,\"kind\":\""); text.string(kind); text.character('"');
    text.word("label", state.label);
    text.signed_field("pid", state.process);
    text.field("boot_us", state.boot);
    text.field("instance", state.instance);
    text.field("seq", state.sequence++);
    text.field("t_us", now);
    text.signed_field("error", state.error.load(std::memory_order_relaxed));
}
bool header(const char* kind) noexcept
{
    Text text;
    prefix(text, kind, clock_value());
    text.signed_field("frontend_uid", state.frontend);
    text.signed_field("watchdog_uid", state.thread);
    text.string("}\n");
    return write_record(text);
}
bool sample(const char* kind) noexcept
{
    const uint64_t now = clock_value();
    Text text;
    prefix(text, kind, now);
    text.field("gap_us", now >= state.lastSample ? now - state.lastSample : 0);
    text.field("previous_write_us", state.lastWriteUs);
    text.string(",\"threads\":[");
    for (unsigned i = 0; i < RoleCount; ++i)
    {
        if (i) text.character(',');
        const auto v = observe(i);
        if (!state.lastSample || !(v == state.previous[i]))
            state.unchangedSince[i] = now;
        state.previous[i] = v;
        text.string("{\"role\":\""); text.string(roleNames[i]); text.character('"');
        text.field("uid", v.uid); text.field("gen", v.epoch); text.field("live", v.live);
        text.word("stage", stage_name(v.stage)); text.field("detail", v.detail);
        if (i == static_cast<unsigned>(Role::Renderer))
            text.word("cmd", command_name(v.command));
        text.field("progress", v.progress); text.field("aux", v.auxiliary);
        text.field("unchanged_us", now >= state.unchangedSince[i] ? now - state.unchangedSince[i] : 0);
        if (!i)
        {
            text.field("frame", v.frame); text.field("task", v.task);
            text.field("state", v.state); text.field("pending", v.pending);
        }
        if (v.uid && !v.changing)
        {
            SceKernelThreadInfo info{};
            info.size = sizeof(info);
            const int result = sceKernelGetThreadInfo(SceUID(v.uid), &info);
            // Fixed schema: rc, process, status, wait type/id, run clocks,
            // interrupt/thread preemptions and releases. Failed queries carry
            // only rc, never stale or zero-filled success fields.
            text.string(",\"kernel\":["); text.signed_number(result);
            if (result >= 0)
            {
                text.character(','); text.signed_number(info.processId);
                text.character(','); text.number(info.status);
                text.character(','); text.number(info.waitType);
                text.character(','); text.signed_number(info.waitId);
                text.character(','); text.number(info.runClocks);
                text.character(','); text.number(info.intrPreemptCount);
                text.character(','); text.number(info.threadPreemptCount);
                text.character(','); text.number(info.threadReleaseCount);
            }
            text.character(']');
        }
        else text.string(",\"kernel\":null");
        if (v.changing) text.string(",\"changing\":1");
        text.character('}');
    }
    text.character(']');
    const auto beforeWrite = clock_value();
    text.field("query_us", beforeWrite >= now ? beforeWrite - now : 0);
    text.string("}\n");
    if (!write_record(text)) return false;
    const auto afterWrite = clock_value();
    state.lastWriteUs = afterWrite >= beforeWrite ? afterWrite - beforeWrite : 0;
    state.lastSample = now;
    return true;
}
int worker(SceSize, void*) noexcept
{
    bool writable = true;
    if (!state.stopping.load(std::memory_order_acquire)) writable = sample("run");
    uint64_t next = clock_value() + PeriodUs;
    while (writable && !state.stopping.load(std::memory_order_acquire))
    {
        const auto now = clock_value();
        SceUInt timeout = SceUInt(now < next ? next - now : 0);
        if (timeout)
        {
            const int result = sceKernelWaitSema(state.semaphore, 1, &timeout);
            if (state.stopping.load(std::memory_order_acquire)) break;
            if (result < 0 && result != int(SCE_KERNEL_ERROR_WAIT_TIMEOUT))
            {
                failure(result);
                writable = header("wait_error");
                break;
            }
            if (result >= 0) continue;
        }
        writable = sample("run");
        // Skip missed deadlines in O(1); never emit a catch-up burst.
        next = clock_value() + PeriodUs;
    }
    if (writable && state.stopping.load(std::memory_order_acquire)) (void)sample("exit");
    const int closeResult = sceIoClose(state.descriptor);
    if (closeResult < 0) failure(closeResult);
    enabledFlag.store(0, std::memory_order_release);
    state.finished.store(1, std::memory_order_release);
    return 0;
}
bool starts_with(const char* text, const char* prefix) noexcept
{
    if (!text) return false;
    while (*prefix) if (*text++ != *prefix++) return false;
    return true;
}
bool output_path(const char* log) noexcept
{
    constexpr char plainPrefix[] = "ux0:data/strikersVita/";
    constexpr char slashPrefix[] = "ux0:/data/strikersVita/";
    const char* prefix = plainPrefix;
    unsigned prefixBytes = sizeof(plainPrefix) - 1;
    if (!starts_with(log, prefix))
    {
        prefix = slashPrefix;
        prefixBytes = sizeof(slashPrefix) - 1;
        if (!starts_with(log, prefix)) return false;
    }
    const char* name = log + prefixBytes;
    unsigned length = 0;
    for (; name[length] && length < 85; ++length)
    {
        const char c = name[length];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
            return false;
        if (c == '.' && length && name[length - 1] == '.') return false;
    }
    if (length < 5 || length > 84 || name[length] || name[length - 4] != '.' ||
        name[length - 3] != 'l' || name[length - 2] != 'o' || name[length - 1] != 'g')
        return false;
    const unsigned labelBytes = length - 4;
    for (unsigned i = 0; i < labelBytes; ++i) state.label[i] = name[i];
    state.label[labelBytes] = 0;
    unsigned at = 0;
    for (unsigned i = 0; i < prefixBytes; ++i) state.path[at++] = prefix[i];
    for (unsigned i = 0; i < labelBytes; ++i) state.path[at++] = name[i];
    const char* suffix = ".watchdog.jsonl";
    while (*suffix) state.path[at++] = *suffix++;
    state.path[at] = 0;
    return true;
}
void reset_channels() noexcept
{
    for (auto& c : channels)
    {
        c.uid.store(0); c.epoch.store(0); c.live.store(0); c.stage.store(0);
        c.detail.store(0); c.command.store(0); c.progress.store(0); c.auxiliary.store(0);
        c.frame.store(0); c.task.store(0); c.state.store(0); c.pending.store(0);
    }
}
void startup_cleanup() noexcept
{
    if (state.thread >= 0) sceKernelDeleteThread(state.thread);
    if (state.semaphore >= 0) sceKernelDeleteSema(state.semaphore);
    if (state.descriptor >= 0) sceIoClose(state.descriptor);
    state.thread = state.semaphore = state.descriptor = -1;
    enabledFlag.store(0, std::memory_order_release);
}
} // namespace
bool start(const char* optIn, const char* logPath) noexcept
{
    if (!optIn || optIn[0] != '1' || optIn[1]) return false;
    if (state.thread >= 0) { failure(Busy); return false; }
    state.error.store(0);
    if (!output_path(logPath)) { failure(InvalidConfiguration); return false; }
    state.sequence = state.lastSample = state.lastWriteUs = 0;
    state.stopping.store(0); state.finished.store(0);
    for (unsigned i = 0; i < RoleCount; ++i)
    {
        state.previous[i] = {};
        state.unchangedSince[i] = 0;
    }
    reset_channels();
    ++state.instance;
    state.boot = clock_value();
    state.process = sceKernelGetProcessId();
    state.frontend = sceKernelGetThreadId();
    state.descriptor = sceIoOpen(state.path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0666);
    if (state.descriptor < 0) { failure(state.descriptor); return false; }
    // This write completes on the frontend before audio or archive initialization.
    if (!header("boot")) { startup_cleanup(); return false; }
    state.semaphore = sceKernelCreateSema("strikers_watchdog_stop", 0, 0, 1, nullptr);
    if (state.semaphore >= 0)
        state.thread = sceKernelCreateThread("strikers_watchdog", worker, SamplerPriority, 64 * 1024,
                                             0, SCE_KERNEL_CPU_MASK_USER_ALL, nullptr);
    int result = state.semaphore < 0 ? state.semaphore : state.thread;
    if (result >= 0)
    {
        enabledFlag.store(1, std::memory_order_release);
        enter_thread(Role::Frontend, state.frontend);
        result = sceKernelStartThread(state.thread, 0, nullptr);
    }
    if (result < 0)
    {
        failure(result);
        (void)header("start_fail");
        startup_cleanup();
        return false;
    }
    return true;
}
int stop() noexcept
{
    if (state.thread < 0) return state.error.load(std::memory_order_relaxed);
    enabledFlag.store(0, std::memory_order_release);
    if (!state.stopping.exchange(1, std::memory_order_acq_rel))
    {
        const int result = sceKernelSignalSema(state.semaphore, 1);
        if (result < 0) failure(result);
    }
    SceUInt timeout = StopTimeoutUs;
    const int result = sceKernelWaitThreadEnd(state.thread, nullptr, &timeout);
    if (result < 0)
    {
        failure(result);
        // No cancellation or cleanup of storage still reachable by the worker.
        return result;
    }
    if (!state.finished.load(std::memory_order_acquire)) { failure(WriteFailure); return WriteFailure; }
    const int threadResult = sceKernelDeleteThread(state.thread);
    const int semaResult = sceKernelDeleteSema(state.semaphore);
    if (threadResult < 0) failure(threadResult);
    if (semaResult < 0) failure(semaResult);
    state.thread = state.semaphore = state.descriptor = -1;
    return state.error.load(std::memory_order_relaxed);
}
int error_code() noexcept { return state.error.load(std::memory_order_relaxed); }
} // namespace port::watchdog
#endif
