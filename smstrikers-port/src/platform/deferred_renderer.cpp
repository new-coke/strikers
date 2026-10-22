#if defined(STRIKERS_VITA_VITAGL)
#include "port/deferred_renderer.hpp"
#include "port/host.h"
#include "port/native_watchdog.hpp"
#include "../../extern/command-buffer/command_buffer.hpp"
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/cpu.h>
#include <pthread.h>
#include <atomic>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <type_traits>
#include <utility>
#include <cerrno>
#include <time.h>
namespace port::render
{
struct DeferredRenderer::Impl
{
    // Bounded command ring: storage is CPU memory, never the GL scratch pool.
    static constexpr unsigned Slots = 8;
    static constexpr size_t SlotBytes = 2 * 1024 * 1024;
    Renderer core;
    std::array<RenderCommandBuffer, Slots> buffers{};
    std::array<void*, Slots> storage{};
    std::array<bool, Slots> busy{};
    pthread_t worker{};
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t ready{}, completed = PTHREAD_COND_INITIALIZER;
    bool readyInitialized = false;
    int readyError = 0;
    bool created = false, stopping = false, bootDone = false, bootResult = false, timed = false,
         active = false;
    unsigned writeSlot = 0, readSlot = 0, readyCount = 0, pendingDraws = 0;
    // Producer-only: an unacquired write slot may still be executing on the owner.
    bool writeSlotAcquired = false;
    uint64_t issued = 0, retired = 0;
    // Producer-owned sequence fences for complete frame packs. Each complete
    // frame may span several immutable command slots, but its final slot is a
    // single retirement boundary. Allow no more than two frames in flight.
    std::array<uint64_t, 2> frameEndSerials{};
    unsigned frameEndCount = 0;
    std::atomic<int> error{ 0 };
    std::atomic<bool> fatalFrameFailure{false};
    std::atomic<const char*> operation{ "none" };
    Renderer::FrameTiming timing{};
    uint64_t frontendWaitUs = 0, commandBytes = 0;
    uint64_t commandCopyNs = 0, commandVertexBytes = 0, commandIndexBytes = 0;
    uint32_t slotWaits = 0;
    FrameTimeline* producerCapture = nullptr;
    FrameTimeline::Owner* ownerCapture = nullptr;
    std::array<bool, Slots> slotDraws{};
    static uint64_t timeline_now() noexcept
    {
        return uint64_t(sceKernelGetSystemTimeWide()) * 1000;
    }
    void draw_ready(bool borrowed) noexcept
    {
        if (!producerCapture) return;
        auto& p = producerCapture->producer;
        if (!p.drawCommands) p.firstDrawReadyNs = timeline_now();
        ++p.drawCommands;
        p.borrowedDrawCommands += borrowed;
        slotDraws[writeSlot] = true;
    }
    void draw_begin() noexcept
    {
        if (!ownerCapture) return;
        if (!ownerCapture->drawCommands) ownerCapture->firstDrawCommandNs = timeline_now();
        ++ownerCapture->drawCommands;
    }
    void draw_end(bool result) noexcept
    {
        if (!ownerCapture) return;
        ownerCapture->lastDrawCommandEndNs = timeline_now();
        if (!result) ++ownerCapture->failedDrawCommands;
    }
    // Only failed frame commands latch here. A handled resource-admission
    // failure may fall back to dynamic geometry without invalidating a frame.
    // Failed commands prevent presentation until an explicit renderer restart.
    bool commandFailed = false;
    int commandError = 0;
    const char* commandOperation = "none";
    bool loading = false;
    uint64_t nextLoadingWake = 0;
    static constexpr uint64_t LoadingPeriodNs = 1000000000ull / 60;
    Impl()
    {
        pthread_condattr_t attributes;
        readyError = pthread_condattr_init(&attributes);
        if (readyError)
            return;
#if !defined(__APPLE__)
        readyError = pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
#endif
        if (!readyError)
            readyError = pthread_cond_init(&ready, &attributes);
        pthread_condattr_destroy(&attributes);
        readyInitialized = !readyError;
    }
    bool loading_failure(const char* operation, int code)
    {
        if (!commandFailed)
        {
            commandFailed = true;
            commandError = code ? -code : -1;
            commandOperation = operation;
        }
        loading = false;
        fatalFrameFailure.store(true, std::memory_order_release);
        // Timer failures are owner-thread failures too: release the private
        // backdrop and operation textures without publishing another image.
        core.set_loading(false, {});
        return false;
    }
    bool loading_now(uint64_t& result)
    {
        timespec now{};
        if (clock_gettime(CLOCK_MONOTONIC, &now))
            return loading_failure("loading_clock", errno);
        result = uint64_t(now.tv_sec) * 1000000000ull + uint64_t(now.tv_nsec);
        return true;
    }
    int loading_wait()
    {
        timespec deadline{time_t(nextLoadingWake / 1000000000ull),
                          long(nextLoadingWake % 1000000000ull)};
#if defined(__APPLE__)
        // Darwin has no condattr_setclock. Its relative wait is the host
        // adapter for the same monotonic deadline used by the Vita path.
        uint64_t now = 0;
        if (!loading_now(now))
            return EINVAL;
        if (now >= nextLoadingWake)
            return ETIMEDOUT;
        const auto remaining = nextLoadingWake - now;
        timespec relative{time_t(remaining / 1000000000ull), long(remaining % 1000000000ull)};
        return pthread_cond_timedwait_relative_np(&ready, &mutex, &relative);
#else
        return pthread_cond_timedwait(&ready, &mutex, &deadline);
#endif
    }
    void service_loading()
    {
        if (!loading || commandFailed)
            return;
        if (!core.loading_active())
        {
            loading = false;
            core.set_loading(false, {});
            return;
        }
        uint64_t now = 0;
        if (!loading_now(now) || now < nextLoadingWake)
            return;
        if (!record(core.loading_tick(), "loading_tick"))
            return;
        if (!core.loading_active())
        {
            loading = false;
            return;
        }
        if (loading_now(now))
            nextLoadingWake += ((now - nextLoadingWake) / LoadingPeriodNs + 1) * LoadingPeriodNs;
    }
    bool record(bool result, const char* op)
    {
        if (!result && !commandFailed)
        {
            commandFailed = true;
            commandError = core.error_code();
            if (!commandError)
                commandError = -1;
            commandOperation = op;
        }
        if (!result)
        {
            fatalFrameFailure.store(true, std::memory_order_release);
            loading = false;
            core.set_loading(false, {});
        }
        return result;
    }
    void snapshot()
    {
        operation.store(commandFailed ? commandOperation : core.last_error(), std::memory_order_release);
        error.store(commandFailed ? commandError : core.error_code(), std::memory_order_release);
    }
    static void* entry(void* opaque)
    {
        auto& d = *static_cast<Impl*>(opaque);
        const auto id = sceKernelGetThreadId();
        watchdog::enter_thread(watchdog::Role::Renderer, id);
        watchdog::stage(watchdog::Role::Renderer, watchdog::Stage::RendererInit);
        const int affinity = sceKernelChangeThreadCpuAffinityMask(id, SCE_KERNEL_CPU_MASK_USER_1);
        sceKernelChangeThreadPriority(id, 0x10000100);
        std::fprintf(stderr, "[vitagl-backend] GPU owner affinity=%08x rc=%08x\n",
                     SCE_KERNEL_CPU_MASK_USER_1, unsigned(affinity));
        const bool initialized = affinity >= 0 && d.core.initialize(d.timed);
        watchdog::stage(watchdog::Role::Renderer, watchdog::Stage::RendererMutex);
        pthread_mutex_lock(&d.mutex);
        d.bootResult = initialized;
        d.bootDone = true;
        pthread_cond_broadcast(&d.completed);
        for (;;)
        {
            while (!d.readyCount && !d.stopping)
            {
                watchdog::stage(watchdog::Role::Renderer, watchdog::Stage::RendererWait);
                auto* capture = d.ownerCapture;
                const bool captureWait = capture && capture->drawCommands;
                const auto waitStart = captureWait ? timeline_now() : 0;
                // No clock, retrace query or timeout exists on the OFF path.
                const int status = d.loading && !d.commandFailed
                                       ? d.loading_wait()
                                       : pthread_cond_wait(&d.ready, &d.mutex);
                if (captureWait) capture->add_wait(waitStart, timeline_now());
                if (status == ETIMEDOUT)
                    break;
                if (status)
                {
                    d.loading_failure("loading_wait", status);
                    d.snapshot();
                }
            }
            if (!d.readyCount && d.stopping)
                break;
            if (d.readyCount)
            {
                const unsigned slot = d.readSlot;
                pthread_mutex_unlock(&d.mutex);
                watchdog::stage(watchdog::Role::Renderer, watchdog::Stage::RendererBatch);
                if (d.ownerCapture) ++d.ownerCapture->batches;
                d.buffers[slot].execute();
                d.snapshot();
                watchdog::stage(watchdog::Role::Renderer, watchdog::Stage::RendererMutex);
                pthread_mutex_lock(&d.mutex);
                --d.readyCount;
                d.busy[slot] = false;
                d.readSlot = (slot + 1) % Slots;
                ++d.retired;
                pthread_cond_broadcast(&d.completed);
            }
            if (d.loading && !d.commandFailed && !d.stopping)
            {
                pthread_mutex_unlock(&d.mutex);
                watchdog::stage(watchdog::Role::Renderer, watchdog::Stage::RendererLoading);
                d.service_loading();
                d.snapshot();
                watchdog::stage(watchdog::Role::Renderer, watchdog::Stage::RendererMutex);
                pthread_mutex_lock(&d.mutex);
            }
        }
        pthread_mutex_unlock(&d.mutex);
        watchdog::stage(watchdog::Role::Renderer, watchdog::Stage::RendererStop);
        if (d.ownerCapture)
        {
            d.ownerCapture->cancelled = true;
            d.ownerCapture = nullptr;
        }
        d.core.shutdown();
        watchdog::leave_thread(watchdog::Role::Renderer);
        return nullptr;
    }
    bool start(bool timings)
    {
        if (created)
            return bootResult;
        if (!readyInitialized)
        {
            error.store(-readyError, std::memory_order_release);
            operation.store("loading_wait_initialize", std::memory_order_release);
            return false;
        }
        timed = timings;
        stopping = bootDone = bootResult = false;
        active = false;
        commandFailed = false;
        commandError = 0;
        commandOperation = "none";
        loading = false;
        nextLoadingWake = 0;
        error.store(0, std::memory_order_release);
        fatalFrameFailure.store(false, std::memory_order_release);
        operation.store("none", std::memory_order_release);
        writeSlot = readSlot = readyCount = pendingDraws = 0;
        issued = retired = 0;
        frameEndSerials = {};
        frameEndCount = 0;
        busy.fill(false);
        writeSlotAcquired = true;
        pthread_attr_t attr;
        int status = pthread_attr_init(&attr);
        if (status)
            return false;
        status = pthread_attr_setstacksize(&attr, 2 * 1024 * 1024);
        if (!status)
            status = pthread_create(&worker, &attr, entry, this);
        pthread_attr_destroy(&attr);
        if (status)
            return false;
        created = true;
        pthread_mutex_lock(&mutex);
        while (!bootDone)
            pthread_cond_wait(&completed, &mutex);
        pthread_mutex_unlock(&mutex);
        if (!bootResult)
        {
            stop();
            return false;
        }
        // Reserve the command ring after VitaGL has budgeted its mapped pools.
        for (unsigned i = 0; i < Slots; ++i)
        {
            void* block = port_aligned_alloc(16, SlotBytes);
            if (!block)
            {
                stop();
                return false;
            }
            storage[i] = block;
            buffers[i].reset_storage(block, SlotBytes);
        }
        std::fprintf(stderr, "[vitagl-backend] Command ring: 8 x 2 MiB, immutable frame segments, "
                             "publish every 16 draws; max 2 frames in flight\n");
        return true;
    }
    void publish()
    {
        // A full ring leaves the next slot owner-owned. Even size() is a read
        // of that owner's mutable command buffer until acquire_write_slot().
        if (!writeSlotAcquired || !buffers[writeSlot].size())
            return;
        watchdog::Scope watchdogWait(watchdog::Role::Frontend, watchdog::Stage::FrontendPublish);
        if (timed && active)
            commandBytes += buffers[writeSlot].size();
        pthread_mutex_lock(&mutex);
        if (producerCapture)
        {
            auto& p = producerCapture->producer;
            ++p.batches;
            if (slotDraws[writeSlot])
            {
                if (!p.drawBatches) p.firstDrawPublishNs = timeline_now();
                ++p.drawBatches;
                slotDraws[writeSlot] = false;
            }
        }
        busy[writeSlot] = true;
        ++readyCount;
        ++issued;
        pendingDraws = 0;
        pthread_cond_signal(&ready);
        writeSlot = (writeSlot + 1) % Slots;
        // Preserve the free-slot fast path under the existing mutex. When the
        // ring is full, let the caller prepare its next packet before waiting.
        writeSlotAcquired = !busy[writeSlot];
        pthread_mutex_unlock(&mutex);
    }
    void acquire_write_slot()
    {
        if (writeSlotAcquired)
            return;
        watchdog::Scope watchdogWait(watchdog::Role::Frontend, watchdog::Stage::FrontendPublish);
        pthread_mutex_lock(&mutex);
        const bool waited = timed && active && busy[writeSlot];
        const auto waitBegin = waited ? port_monotonic_ns() : 0;
        while (busy[writeSlot])
            pthread_cond_wait(&completed, &mutex);
        if (waited)
        {
            frontendWaitUs += (port_monotonic_ns() - waitBegin) / 1000;
            ++slotWaits;
        }
        // execute() has destroyed the prior payloads and reset the buffer
        // before retiring this slot under the same mutex.
        writeSlotAcquired = true;
        pthread_mutex_unlock(&mutex);
    }
    void drain()
    {
        if (!created)
            return;
        watchdog::Scope watchdogWait(watchdog::Role::Frontend, watchdog::Stage::FrontendDrain);
        publish();
        pthread_mutex_lock(&mutex);
        const auto target = issued;
        const bool waited = timed && active && retired < target;
        const auto waitBegin = waited ? port_monotonic_ns() : 0;
        while (retired < target)
            pthread_cond_wait(&completed, &mutex);
        if (waited)
            frontendWaitUs += (port_monotonic_ns() - waitBegin) / 1000;
        pthread_mutex_unlock(&mutex);
    }
    bool settle_frames(unsigned allowed)
    {
        pthread_mutex_lock(&mutex);
        while (frameEndCount)
        {
            const uint64_t oldest = frameEndSerials[0];
            if (retired < oldest && frameEndCount <= allowed)
                break;
            while (retired < oldest)
                pthread_cond_wait(&completed, &mutex);
            --frameEndCount;
            if (frameEndCount)
                frameEndSerials[0] = frameEndSerials[1];
        }
        pthread_mutex_unlock(&mutex);
        return !fatalFrameFailure.load(std::memory_order_acquire);
    }
    void completed_frame_queued()
    {
        // end_frame publishes the final immutable slot before recording this
        // sequence. The owner can retire it immediately without a data race.
        publish();
        if (frameEndCount < frameEndSerials.size())
            frameEndSerials[frameEndCount++] = issued;
    }
    template <class T> T* append(size_t bytes, T value)
    {
        if (!created || !bootResult)
            return nullptr;
        acquire_write_slot();
        auto* command = buffers[writeSlot].append<T>(bytes, std::move(value));
        if (!command)
        {
            publish();
            acquire_write_slot();
            command = buffers[writeSlot].append<T>(bytes, std::move(value));
        }
        return command;
    }
    template <class T, class... Args> T* emplace(size_t bytes, Args&&... args) noexcept
    {
        static_assert(std::is_nothrow_constructible_v<T, Args...>);
        if (!created || !bootResult)
            return nullptr;
        acquire_write_slot();
        auto* command = buffers[writeSlot].append<T>(bytes, std::forward<Args>(args)...);
        if (!command)
        {
            // Capacity is checked before construction, so all arguments remain
            // available until the next slot is acquired.
            publish();
            acquire_write_slot();
            command = buffers[writeSlot].append<T>(bytes, std::forward<Args>(args)...);
        }
        return command;
    }
    template <watchdog::Command Operation, class Function> auto sync(Function function) -> decltype(function())
    {
        using Result = decltype(function());
        if constexpr (std::is_void_v<Result>)
        {
            struct Call
            {
                Function fn;
                void operator()() { watchdog::CommandScope observed(Operation); fn(); }
            };
            if (!append(0, Call{ function }))
                return;
            drain();
        }
        else
        {
            Result result{};
            struct Call
            {
                Function fn;
                Result* result;
                void operator()() { watchdog::CommandScope observed(Operation); *result = fn(); }
            };
            if (!append(0, Call{ function, &result }))
                return result;
            if constexpr (Operation == watchdog::Command::Draw)
                draw_ready(true);
            drain();
            return result;
        }
    }
    template <watchdog::Command Operation, class Function> bool enqueue(Function function)
    {
        struct Call
        {
            Function fn;
            void operator()() { watchdog::CommandScope observed(Operation); fn(); }
        };
        return append(0, Call{ function }) != nullptr;
    }
    template <watchdog::Command Operation, class Function> bool enqueue_checked(Function function, const char* op)
    {
        struct Call
        {
            Impl* owner;
            Function fn;
            const char* operation;
            void operator()() { watchdog::CommandScope observed(Operation); owner->record(fn(), operation); }
        };
        return append(0, Call{ this, function, op }) != nullptr;
    }
    void stop()
    {
        if (created)
        {
            if (bootResult)
                drain();
            pthread_mutex_lock(&mutex);
            stopping = true;
            pthread_cond_signal(&ready);
            pthread_mutex_unlock(&mutex);
            pthread_join(worker, nullptr);
            if (producerCapture)
            {
                producerCapture->producer.cancelled = true;
                producerCapture = nullptr;
            }
            created = false;
            bootResult = false;
        }
        for (unsigned i = 0; i < Slots; ++i)
        {
            port_aligned_free(storage[i]);
            storage[i] = nullptr;
            buffers[i].reset_storage(nullptr, 0);
        }
        writeSlotAcquired = false;
        active = false;
    }
    ~Impl()
    {
        stop();
        if (readyInitialized)
            pthread_cond_destroy(&ready);
        pthread_cond_destroy(&completed);
        pthread_mutex_destroy(&mutex);
    }
};
DeferredRenderer::DeferredRenderer() noexcept : impl_(new (std::nothrow) Impl) {}
DeferredRenderer::~DeferredRenderer() { shutdown(); }
bool DeferredRenderer::initialize(bool timing) noexcept { return impl_ && impl_->start(timing); }
bool DeferredRenderer::initialize_scene_program() noexcept
{
    return impl_ && impl_->sync<watchdog::Command::InitializeScene>([&] { return impl_->core.initialize_scene_program(); });
}
uint32_t DeferredRenderer::create_texture(const TextureUpload& upload) noexcept
{
    return impl_ ? impl_->sync<watchdog::Command::CreateTexture>([&] { return impl_->core.create_texture(upload); }) : 0;
}
uint32_t DeferredRenderer::create_target(uint32_t w, uint32_t h, TargetFormat format) noexcept
{
    return impl_ ? impl_->sync<watchdog::Command::CreateTarget>([this, w, h, format] { return impl_->core.create_target(w, h, format); }) : 0;
}
uint32_t DeferredRenderer::create_model(std::span<const ModelVertex> v,
                                        std::span<const uint16_t> i) noexcept
{
    return impl_ ? impl_->sync<watchdog::Command::CreateModel>([&] { return impl_->core.create_model(v, i); }) : 0;
}
void DeferredRenderer::release_texture(uint32_t id) noexcept
{
    if (impl_)
        impl_->enqueue<watchdog::Command::ReleaseTexture>([d = impl_.get(), id] { d->core.release_texture(id); });
}
void DeferredRenderer::release_model(uint32_t id) noexcept
{
    if (impl_)
        impl_->enqueue<watchdog::Command::ReleaseModel>([d = impl_.get(), id] { d->core.release_model(id); });
}
bool DeferredRenderer::draw_scene(uint32_t model, std::span<const ModelVertex> vertices,
                                  std::span<const uint16_t> indices,
                                  const std::array<float, 16>& matrix, const Raster& raster,
                                  const SceneMaterial& material) noexcept
{
    if (!impl_ || !impl_->active)
        return false;
    auto& d = *impl_;
    struct Draw
    {
        Impl* owner;
        uint32_t model, vertexCount, indexCount;
        std::array<float, 16> matrix;
        Raster raster;
        SceneMaterial material;
        Draw(Impl* owner, uint32_t model, uint32_t vertexCount, uint32_t indexCount,
             const std::array<float, 16>& matrix, const Raster& raster,
             const SceneMaterial& material) noexcept
            : owner(owner), model(model), vertexCount(vertexCount), indexCount(indexCount),
              matrix(matrix), raster(raster), material(material)
        {}
        void operator()()
        {
            watchdog::CommandScope observed(watchdog::Command::Draw);
            const auto* v = reinterpret_cast<const ModelVertex*>(this + 1);
            const auto* i = reinterpret_cast<const uint16_t*>(v + vertexCount);
            owner->draw_begin();
            const bool result = owner->record(owner->core.draw_scene(model, { v, vertexCount }, { i, indexCount },
                                                                      matrix, raster, material), "draw_scene");
            owner->draw_end(result);
        }
    };
    const size_t vertexBytes = model ? 0 : vertices.size_bytes(),
                 indexBytes = model ? 0 : indices.size_bytes();
    // Rare oversized CPU arrays use a synchronized borrowed-pointer call,
    // with result-bearing pointer calls. The normal path copies.
    if (vertexBytes > Impl::SlotBytes - sizeof(Draw) - 32 ||
        indexBytes > Impl::SlotBytes - sizeof(Draw) - 32 - vertexBytes)
        return d.sync<watchdog::Command::Draw>([&] {
            d.draw_begin();
            const bool result = d.record(d.core.draw_scene(model, vertices, indices, matrix, raster, material),
                                         "draw_scene");
            d.draw_end(result);
            return result;
        });
    auto* command =
        d.emplace<Draw>(vertexBytes + indexBytes, &d, model,
                        model ? 0u : uint32_t(vertices.size()),
                        model ? 0u : uint32_t(indices.size()), matrix, raster, material);
    if (!command)
        return false;
    auto* payload = reinterpret_cast<uint8_t*>(command + 1);
    const auto copyBegin = d.timed && (vertexBytes || indexBytes) ? port_monotonic_ns() : 0;
    if (vertexBytes)
        std::memcpy(payload, vertices.data(), vertexBytes);
    if (indexBytes)
        std::memcpy(payload + vertexBytes, indices.data(), indexBytes);
    if (copyBegin)
    {
        d.commandCopyNs += port_monotonic_ns() - copyBegin;
        d.commandVertexBytes += vertexBytes;
        d.commandIndexBytes += indexBytes;
    }
    d.draw_ready(false);
    if (++d.pendingDraws >= 16)
        d.publish();
    return true;
}
bool DeferredRenderer::select_target(uint32_t id, bool color, bool depth) noexcept
{
    return impl_ && impl_->enqueue_checked<watchdog::Command::SelectTarget>([d = impl_.get(), id, color, depth] {
        return d->core.select_target(id, color, depth);
    }, "select_target");
}
bool DeferredRenderer::copy_texture(uint32_t id, std::array<float, 4> source,
                                    bool intensity) noexcept
{
    return impl_ && impl_->enqueue_checked<watchdog::Command::CopyTexture>([d = impl_.get(), id, source, intensity] {
        return d->core.copy_texture(id, source, intensity);
    }, "copy_texture");
}
bool DeferredRenderer::copy_textures(std::span<const TextureCopy> copies) noexcept
{
    if (!impl_)
        return false;
    struct Copy
    {
        Impl* owner;
        uint32_t count;
        void operator()()
        {
            watchdog::CommandScope observed(watchdog::Command::CopyTextures);
            owner->record(owner->core.copy_textures(
                              { reinterpret_cast<const TextureCopy*>(this + 1), count }),
                          "copy_textures");
        }
    };
    if (copies.size_bytes() > Impl::SlotBytes - sizeof(Copy) - 32)
        return impl_->sync<watchdog::Command::CopyTextures>([&] {
            return impl_->record(impl_->core.copy_textures(copies), "copy_textures");
        });
    auto* command =
        impl_->append(copies.size_bytes(), Copy{ impl_.get(), uint32_t(copies.size()) });
    if (!command)
        return false;
    if (!copies.empty())
        std::memcpy(command + 1, copies.data(), copies.size_bytes());
    return true;
}
bool DeferredRenderer::clear_color(std::array<float, 4> color, bool depth) noexcept
{
    return impl_ && impl_->enqueue_checked<watchdog::Command::ClearColor>([d = impl_.get(), color, depth] {
        return d->core.clear_color(color, depth);
    }, "clear_color");
}
bool DeferredRenderer::clear_depth() noexcept
{
    return impl_ && impl_->enqueue_checked<watchdog::Command::ClearDepth>([d = impl_.get()] { return d->core.clear_depth(); },
                                          "clear_depth");
}
bool DeferredRenderer::set_black(bool black) noexcept
{
    return impl_ && impl_->enqueue_checked<watchdog::Command::SetBlack>([d = impl_.get(), black] {
        return d->core.set_black(black);
    }, "set_black");
}
bool DeferredRenderer::present_bitmap(const TextureUpload& upload,
                                      std::array<float, 4> destination) noexcept
{
    return impl_ && impl_->sync<watchdog::Command::PresentBitmap>([&] {
        // Drain prior commands in order, but never publish a frame whose
        // earlier command failed. This does not close the producer's frame.
        return !impl_->commandFailed && impl_->record(
            impl_->core.present_bitmap(upload, destination), "present_bitmap");
    });
}
bool DeferredRenderer::set_loading(bool begin, const LoadingIndicator& request) noexcept
{
    return impl_ && impl_->sync<watchdog::Command::SetLoading>([&] {
        auto& d = *impl_;
        // OFF is cleanup and remains executable after a previous failure.
        if (begin && d.commandFailed)
            return false;
        if (!d.record(d.core.set_loading(begin, request), "set_loading"))
            return false;
        d.loading = begin && d.core.loading_active();
        if (d.loading)
        {
            uint64_t now = 0;
            if (!d.loading_now(now))
                return false;
            d.nextLoadingWake = now + Impl::LoadingPeriodNs;
        }
        return true;
    });
}
bool DeferredRenderer::frame_active() const noexcept { return impl_ && impl_->active; }
bool DeferredRenderer::begin_frame(FrameTimeline* capture, bool asynchronous) noexcept
{
    if (!impl_ || impl_->active)
        return false;
    // Never reuse capture storage or queue a third frame past an unretired
    // backend. Synchronous inspection drains the prior asynchronous passes.
    const bool framesHealthy = impl_->settle_frames(asynchronous && !capture ? 1 : 0);
    if (!framesHealthy && asynchronous && !capture)
        return false;
    impl_->frontendWaitUs = impl_->commandBytes = impl_->slotWaits = 0;
    impl_->commandCopyNs = impl_->commandVertexBytes = impl_->commandIndexBytes = 0;
    if (capture)
    {
        impl_->producerCapture = capture;
        *capture = {};
        impl_->slotDraws.fill(false);
        capture->producer.observed = true;
        capture->producer.beginRequestNs = Impl::timeline_now();
    }
    if (asynchronous && !capture)
    {
        impl_->active = impl_->enqueue_checked<watchdog::Command::BeginFrame>(
            [d = impl_.get()] { return !d->commandFailed && d->core.begin_frame(nullptr); },
            "begin_frame");
        return impl_->active;
    }
    impl_->active = impl_->sync<watchdog::Command::BeginFrame>([&] {
        if (capture)
        {
            impl_->ownerCapture = &capture->owner;
            capture->owner.drawTimingEnabled = impl_->timed;
            capture->owner.beginStartNs = Impl::timeline_now();
            capture->owner.batches = 1; // The batch containing this begin command.
        }
        const bool result = !impl_->commandFailed && impl_->record(
            impl_->core.begin_frame(impl_->ownerCapture), "begin_frame");
        if (capture) capture->owner.beginEndNs = Impl::timeline_now();
        if (!result && impl_->ownerCapture) impl_->ownerCapture = nullptr;
        return result;
    });
    if (capture)
    {
        capture->producer.beginReturnNs = Impl::timeline_now();
        capture->producer.beginSucceeded = impl_->active;
    }
    if (!impl_->active && impl_->producerCapture) impl_->producerCapture = nullptr;
    return impl_->active;
}
bool DeferredRenderer::end_frame(bool present, bool asynchronous) noexcept
{
    if (!impl_ || !impl_->active)
        return false;
    auto* capture = impl_->producerCapture;
    if (asynchronous && !capture)
    {
        const bool queued = impl_->enqueue_checked<watchdog::Command::EndFrame>(
            [d = impl_.get(), present] {
                const bool commandsOk = !d->commandFailed;
                const bool ended = d->core.end_frame(present && commandsOk);
                d->timing = d->core.completed_timing();
                return ended && commandsOk;
            }, "end_frame");
        if (queued)
            impl_->completed_frame_queued();
        impl_->active = false;
        return queued && !impl_->fatalFrameFailure.load(std::memory_order_acquire);
    }
    if (capture) capture->producer.endRequestNs = Impl::timeline_now();
    const bool result = impl_->sync<watchdog::Command::EndFrame>([&] {
        if (impl_->ownerCapture) impl_->ownerCapture->endStartNs = Impl::timeline_now();
        const bool commandsOk = !impl_->commandFailed;
        const bool ok = impl_->record(impl_->core.end_frame(present && commandsOk), "end_frame");
        impl_->timing = impl_->core.completed_timing();
        if (impl_->ownerCapture) impl_->ownerCapture->endEndNs = Impl::timeline_now();
        if (impl_->ownerCapture) impl_->ownerCapture = nullptr;
        return ok && commandsOk;
    });
    if (capture)
    {
        capture->producer.endReturnNs = Impl::timeline_now();
        capture->producer.endSucceeded = result;
    }
    if (impl_->producerCapture) impl_->producerCapture = nullptr;
    impl_->timing.frontendWaitUs = impl_->frontendWaitUs;
    impl_->timing.commandBytes = impl_->commandBytes;
    impl_->timing.commandCopyNs = impl_->commandCopyNs;
    impl_->timing.commandVertexBytes = impl_->commandVertexBytes;
    impl_->timing.commandIndexBytes = impl_->commandIndexBytes;
    impl_->timing.slotWaits += impl_->slotWaits;
    impl_->active = false;
    return result;
}
bool DeferredRenderer::readback_presented_rgba8(std::vector<uint8_t>& out) noexcept
{
    return impl_ && impl_->sync<watchdog::Command::Readback>([&] { return impl_->core.readback_presented_rgba8(out); });
}
void DeferredRenderer::shutdown() noexcept
{
    if (impl_)
        impl_->stop();
}
const char* DeferredRenderer::last_error() const noexcept
{
    return impl_ ? impl_->operation.load(std::memory_order_acquire) : "allocation";
}
int DeferredRenderer::error_code() const noexcept
{
    return impl_ ? impl_->error.load(std::memory_order_acquire) : -1;
}
DeferredRenderer::FrameTiming DeferredRenderer::completed_timing() const noexcept
{
    // The owner writes this only after a frame retires. An uncollected async
    // frame may still be updating it; never read across that boundary.
    return impl_ && !impl_->frameEndCount ? impl_->timing : FrameTiming{};
}
uint64_t DeferredRenderer::pending_frontend_wait_us() const noexcept
{
    return impl_ ? impl_->frontendWaitUs : 0;
}
}   // namespace port::render
#endif
