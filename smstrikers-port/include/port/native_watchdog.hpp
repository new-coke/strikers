#pragma once
#include <cstdint>

namespace port::watchdog
{
enum class Role : uint32_t { Frontend, Renderer, Jobs, AudioOutput, AudioLoader, Count };
enum class Stage : uint32_t
{
    Idle, Boot, VerifyArchive, NativeAudioInit, NativeAudioReady, RuntimeInit, GameInit,
    Limiter, HostInput, Debug, BeginFrame, Pad, Tasks, TaskTransition, TaskRun,
    AudioUpdate, EndFrame, Snapshot, Capture, FrameEnd, ExitRequested,
    ShutdownAudio, ShutdownAudioLoader, ShutdownRenderer, ShutdownRuntime,
    RendererInit, RendererWait, RendererBatch, RendererCommand, RendererLoading,
    RendererMutex, RendererStop, FrontendPublish, FrontendDrain,
    JobsWait, JobsRun, JobsPublish, JobsJoin, AudioMix, AudioOutput, AudioDump,
    AudioStatsLock, AudioStats, AudioLog, AudioLoop,
    LoaderLock, LoaderWait, LoaderRead, LoaderPublish, LoaderLog, Exited, Count
};
enum class Command : uint32_t
{
    None, InitializeScene, CreateTexture, CreateTarget, CreateModel, ReleaseTexture,
    ReleaseModel, Draw, SelectTarget, CopyTexture, CopyTextures, ClearColor, ClearDepth,
    SetBlack, PresentBitmap, SetLoading, BeginFrame, EndFrame, Readback, Count
};

#if (defined(PORT_VITA) && defined(STRIKERS_VITA_NATIVE_RUNTIME)) || defined(STRIKERS_WATCHDOG_TEST)
#define STRIKERS_WATCHDOG_ENABLED_BUILD 1
}
#include <atomic>
namespace port::watchdog
{
static_assert(std::atomic<uint32_t>::is_always_lock_free);
static_assert(std::atomic<int32_t>::is_always_lock_free);
struct alignas(64) Channel
{
    std::atomic<uint32_t> uid{0}, epoch{0}, live{0}, stage{0}, detail{0}, command{0};
    std::atomic<uint32_t> progress{0}, auxiliary{0}, frame{0}, task{0}, state{0}, pending{0};
};
static_assert(sizeof(Channel) == 64);
extern std::atomic<uint32_t> enabledFlag;
extern Channel channels[static_cast<unsigned>(Role::Count)];
inline bool enabled() noexcept { return enabledFlag.load(std::memory_order_relaxed) != 0; }
inline Channel& channel(Role role) noexcept { return channels[static_cast<unsigned>(role)]; }
inline void stage(Role role, Stage value, uint32_t detail = 0) noexcept
{
    if (!enabled()) return;
    auto& c = channel(role);
    c.detail.store(detail, std::memory_order_relaxed);
    c.stage.store(static_cast<uint32_t>(value), std::memory_order_relaxed);
}
inline void enter_thread(Role role, int32_t uid) noexcept
{
    if (!enabled()) return;
    auto& c = channel(role);
    c.epoch.fetch_add(1, std::memory_order_acq_rel);
    c.live.store(0, std::memory_order_relaxed);
    c.uid.store(static_cast<uint32_t>(uid), std::memory_order_relaxed);
    c.progress.store(0, std::memory_order_relaxed);
    c.auxiliary.store(0, std::memory_order_relaxed);
    c.detail.store(0, std::memory_order_relaxed);
    c.command.store(0, std::memory_order_relaxed);
    c.stage.store(static_cast<uint32_t>(Stage::Boot), std::memory_order_relaxed);
    c.live.store(1, std::memory_order_release);
    c.epoch.fetch_add(1, std::memory_order_release);
}
inline void leave_thread(Role role) noexcept
{
    if (!enabled()) return;
    auto& c = channel(role);
    c.stage.store(static_cast<uint32_t>(Stage::Exited), std::memory_order_relaxed);
    c.live.store(0, std::memory_order_release);
}
inline void progress(Role role, uint32_t value, uint32_t auxiliary = 0) noexcept
{
    if (!enabled()) return;
    auto& c = channel(role);
    c.progress.store(value, std::memory_order_relaxed);
    c.auxiliary.store(auxiliary, std::memory_order_relaxed);
}
inline void advance(Role role) noexcept
{
    if (!enabled()) return;
    auto& p = channel(role).progress; // Only this channel's owner increments it.
    p.store(p.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
}
inline void frame(uint32_t value) noexcept
{
    if (enabled()) channel(Role::Frontend).frame.store(value, std::memory_order_relaxed);
}
inline void task(uint32_t key, uint32_t current, uint32_t pending) noexcept
{
    if (!enabled()) return;
    auto& c = channel(Role::Frontend);
    c.task.store(key, std::memory_order_relaxed);
    c.state.store(current, std::memory_order_relaxed);
    c.pending.store(pending, std::memory_order_relaxed);
}
class Scope
{
  public:
    Scope(Role role, Stage value, uint32_t detail = 0) noexcept
    {
        if (!enabled()) return;
        owner = &channel(role);
        previousStage = owner->stage.load(std::memory_order_relaxed);
        previousDetail = owner->detail.load(std::memory_order_relaxed);
        owner->detail.store(detail, std::memory_order_relaxed);
        owner->stage.store(static_cast<uint32_t>(value), std::memory_order_relaxed);
    }
    ~Scope()
    {
        if (!owner || !enabled()) return;
        owner->detail.store(previousDetail, std::memory_order_relaxed);
        owner->stage.store(previousStage, std::memory_order_relaxed);
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
  private:
    Channel* owner = nullptr;
    uint32_t previousStage = 0, previousDetail = 0;
};
class CommandScope
{
  public:
    explicit CommandScope(Command command) noexcept
        : scope(Role::Renderer, Stage::RendererCommand, static_cast<uint32_t>(command))
    {
        if (enabled())
            channel(Role::Renderer).command.store(static_cast<uint32_t>(command),
                                                  std::memory_order_relaxed);
    }
    ~CommandScope() { advance(Role::Renderer); } // CPU command returned, not GPU completion.
  private:
    Scope scope;
};
bool start(const char* optIn, const char* logPath) noexcept;
int stop() noexcept;
int error_code() noexcept;
#else
inline bool enabled() noexcept { return false; }
inline void stage(Role, Stage, uint32_t = 0) noexcept {}
inline void enter_thread(Role, int32_t) noexcept {}
inline void leave_thread(Role) noexcept {}
inline void progress(Role, uint32_t, uint32_t = 0) noexcept {}
inline void advance(Role) noexcept {}
inline void frame(uint32_t) noexcept {}
inline void task(uint32_t, uint32_t, uint32_t) noexcept {}
class Scope
{
  public:
    Scope(Role, Stage, uint32_t = 0) noexcept {}
};
class CommandScope
{
  public:
    explicit CommandScope(Command) noexcept {}
};
inline bool start(const char*, const char*) noexcept { return false; }
inline int stop() noexcept { return 0; }
inline int error_code() noexcept { return 0; }
#endif
class Session
{
  public:
    Session(const char* optIn, const char* logPath) noexcept : owned(start(optIn, logPath)) {}
    ~Session() { finish(); }
    int finish() noexcept
    {
        if (!owned) return 0;
        owned = false;
        return stop();
    }
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
  private:
    bool owned;
};
} // namespace port::watchdog
