#if defined(STRIKERS_VITA_VITAGL)
#include "port/deferred_renderer.hpp"
#include "port/host.h"
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
    pthread_cond_t ready = PTHREAD_COND_INITIALIZER, completed = PTHREAD_COND_INITIALIZER;
    bool created = false, stopping = false, bootDone = false, bootResult = false, timed = false,
         active = false;
    unsigned writeSlot = 0, readSlot = 0, readyCount = 0, pendingDraws = 0;
    uint64_t issued = 0, retired = 0;
    std::atomic<int> error{ 0 };
    std::atomic<const char*> operation{ "none" };
    Renderer::FrameTiming timing{};
    uint64_t frontendWaitUs = 0, commandBytes = 0;
    uint32_t slotWaits = 0;
    void snapshot()
    {
        error.store(core.error_code(), std::memory_order_release);
        operation.store(core.last_error(), std::memory_order_release);
    }
    static void* entry(void* opaque)
    {
        auto& d = *static_cast<Impl*>(opaque);
        const auto id = sceKernelGetThreadId();
        const int affinity = sceKernelChangeThreadCpuAffinityMask(id, SCE_KERNEL_CPU_MASK_USER_1);
        sceKernelChangeThreadPriority(id, 0x10000100);
        std::fprintf(stderr, "[vitagl-backend] GPU owner affinity=%08x rc=%08x\n",
                     SCE_KERNEL_CPU_MASK_USER_1, unsigned(affinity));
        const bool initialized = affinity >= 0 && d.core.initialize(d.timed);
        pthread_mutex_lock(&d.mutex);
        d.bootResult = initialized;
        d.bootDone = true;
        pthread_cond_broadcast(&d.completed);
        for (;;)
        {
            while (!d.readyCount && !d.stopping)
                pthread_cond_wait(&d.ready, &d.mutex);
            if (!d.readyCount && d.stopping)
                break;
            const unsigned slot = d.readSlot;
            pthread_mutex_unlock(&d.mutex);
            d.buffers[slot].execute();
            d.snapshot();
            pthread_mutex_lock(&d.mutex);
            --d.readyCount;
            d.busy[slot] = false;
            d.readSlot = (slot + 1) % Slots;
            ++d.retired;
            pthread_cond_broadcast(&d.completed);
        }
        pthread_mutex_unlock(&d.mutex);
        d.core.shutdown();
        return nullptr;
    }
    bool start(bool timings)
    {
        if (created)
            return bootResult;
        timed = timings;
        stopping = bootDone = bootResult = false;
        active = false;
        writeSlot = readSlot = readyCount = pendingDraws = 0;
        issued = retired = 0;
        busy.fill(false);
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
        std::fprintf(stderr, "[vitagl-backend] Command ring: 8 x 2 MiB, immutable commands, "
                             "publish every 16 draws\n");
        return true;
    }
    void publish()
    {
        if (!buffers[writeSlot].size())
            return;
        if (timed && active)
            commandBytes += buffers[writeSlot].size();
        pthread_mutex_lock(&mutex);
        busy[writeSlot] = true;
        ++readyCount;
        ++issued;
        pendingDraws = 0;
        pthread_cond_signal(&ready);
        writeSlot = (writeSlot + 1) % Slots;
        const bool waited = timed && active && busy[writeSlot];
        const auto waitBegin = waited ? port_monotonic_ns() : 0;
        while (busy[writeSlot])
            pthread_cond_wait(&completed, &mutex);
        if (waited)
        {
            frontendWaitUs += (port_monotonic_ns() - waitBegin) / 1000;
            ++slotWaits;
        }
        pthread_mutex_unlock(&mutex);
    }
    void drain()
    {
        if (!created)
            return;
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
    template <class T> T* append(size_t bytes, T value)
    {
        if (!created || !bootResult)
            return nullptr;
        auto* command = buffers[writeSlot].append<T>(bytes, std::move(value));
        if (!command)
        {
            publish();
            command = buffers[writeSlot].append<T>(bytes, std::move(value));
        }
        return command;
    }
    template <class Function> auto sync(Function function) -> decltype(function())
    {
        using Result = decltype(function());
        if constexpr (std::is_void_v<Result>)
        {
            struct Call
            {
                Function fn;
                void operator()() { fn(); }
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
                void operator()() { *result = fn(); }
            };
            if (!append(0, Call{ function, &result }))
                return result;
            drain();
            return result;
        }
    }
    template <class Function> bool enqueue(Function function)
    {
        struct Call
        {
            Function fn;
            void operator()() { fn(); }
        };
        return append(0, Call{ function }) != nullptr;
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
            created = false;
            bootResult = false;
        }
        for (unsigned i = 0; i < Slots; ++i)
        {
            port_aligned_free(storage[i]);
            storage[i] = nullptr;
            buffers[i].reset_storage(nullptr, 0);
        }
        active = false;
    }
    ~Impl()
    {
        stop();
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
    return impl_ && impl_->sync([&] { return impl_->core.initialize_scene_program(); });
}
uint32_t DeferredRenderer::create_texture(const TextureUpload& upload) noexcept
{
    return impl_ ? impl_->sync([&] { return impl_->core.create_texture(upload); }) : 0;
}
uint32_t DeferredRenderer::create_target(uint32_t w, uint32_t h) noexcept
{
    return impl_ ? impl_->sync([&] { return impl_->core.create_target(w, h); }) : 0;
}
uint32_t DeferredRenderer::create_model(std::span<const ModelVertex> v,
                                        std::span<const uint16_t> i) noexcept
{
    return impl_ ? impl_->sync([&] { return impl_->core.create_model(v, i); }) : 0;
}
void DeferredRenderer::release_texture(uint32_t id) noexcept
{
    if (impl_)
        impl_->enqueue([d = impl_.get(), id] { d->core.release_texture(id); });
}
void DeferredRenderer::release_model(uint32_t id) noexcept
{
    if (impl_)
        impl_->enqueue([d = impl_.get(), id] { d->core.release_model(id); });
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
        Renderer* core;
        uint32_t model, vertexCount, indexCount;
        std::array<float, 16> matrix;
        Raster raster;
        SceneMaterial material;
        void operator()()
        {
            const auto* v = reinterpret_cast<const ModelVertex*>(this + 1);
            const auto* i = reinterpret_cast<const uint16_t*>(v + vertexCount);
            core->draw_scene(model, { v, vertexCount }, { i, indexCount }, matrix, raster,
                             material);
        }
    };
    const size_t vertexBytes = model ? 0 : vertices.size_bytes(),
                 indexBytes = model ? 0 : indices.size_bytes();
    // Rare oversized CPU arrays use a synchronized borrowed-pointer call,
    // with result-bearing pointer calls. The normal path copies.
    if (vertexBytes > Impl::SlotBytes - sizeof(Draw) - 32 ||
        indexBytes > Impl::SlotBytes - sizeof(Draw) - 32 - vertexBytes)
        return d.sync(
            [&] { return d.core.draw_scene(model, vertices, indices, matrix, raster, material); });
    auto* command =
        d.append(vertexBytes + indexBytes,
                 Draw{ &d.core, model, model ? 0u : uint32_t(vertices.size()),
                       model ? 0u : uint32_t(indices.size()), matrix, raster, material });
    if (!command)
        return false;
    auto* payload = reinterpret_cast<uint8_t*>(command + 1);
    if (vertexBytes)
        std::memcpy(payload, vertices.data(), vertexBytes);
    if (indexBytes)
        std::memcpy(payload + vertexBytes, indices.data(), indexBytes);
    if (++d.pendingDraws >= 16)
        d.publish();
    return true;
}
bool DeferredRenderer::select_target(uint32_t id, bool color, bool depth) noexcept
{
    return impl_ && impl_->enqueue([d = impl_.get(), id, color, depth] {
        d->core.select_target(id, color, depth);
    });
}
bool DeferredRenderer::copy_texture(uint32_t id, std::array<float, 4> source,
                                    bool intensity) noexcept
{
    return impl_ && impl_->enqueue([d = impl_.get(), id, source, intensity] {
        d->core.copy_texture(id, source, intensity);
    });
}
bool DeferredRenderer::copy_textures(std::span<const TextureCopy> copies) noexcept
{
    if (!impl_)
        return false;
    struct Copy
    {
        Renderer* core;
        uint32_t count;
        void operator()()
        {
            core->copy_textures({ reinterpret_cast<const TextureCopy*>(this + 1), count });
        }
    };
    if (copies.size_bytes() > Impl::SlotBytes - sizeof(Copy) - 32)
        return impl_->sync([&] { return impl_->core.copy_textures(copies); });
    auto* command =
        impl_->append(copies.size_bytes(), Copy{ &impl_->core, uint32_t(copies.size()) });
    if (!command)
        return false;
    if (!copies.empty())
        std::memcpy(command + 1, copies.data(), copies.size_bytes());
    return true;
}
bool DeferredRenderer::clear_color(std::array<float, 4> color, bool depth) noexcept
{
    return impl_ &&
           impl_->enqueue([d = impl_.get(), color, depth] { d->core.clear_color(color, depth); });
}
bool DeferredRenderer::clear_depth() noexcept
{
    return impl_ && impl_->enqueue([d = impl_.get()] { d->core.clear_depth(); });
}
bool DeferredRenderer::frame_active() const noexcept { return impl_ && impl_->active; }
bool DeferredRenderer::begin_frame() noexcept
{
    if (!impl_ || impl_->active)
        return false;
    impl_->frontendWaitUs = impl_->commandBytes = impl_->slotWaits = 0;
    impl_->active = impl_->sync([&] { return impl_->core.begin_frame(); });
    return impl_->active;
}
bool DeferredRenderer::end_frame(bool present) noexcept
{
    if (!impl_ || !impl_->active)
        return false;
    const bool result = impl_->sync([&] {
        const bool ok = impl_->core.end_frame(present);
        impl_->timing = impl_->core.completed_timing();
        return ok;
    });
    impl_->timing.frontendWaitUs = impl_->frontendWaitUs;
    impl_->timing.commandBytes = impl_->commandBytes;
    impl_->timing.slotWaits += impl_->slotWaits;
    impl_->active = false;
    return result;
}
bool DeferredRenderer::readback_presented_rgba8(std::vector<uint8_t>& out) noexcept
{
    return impl_ && impl_->sync([&] { return impl_->core.readback_presented_rgba8(out); });
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
    return impl_ ? impl_->timing : FrameTiming{};
}
}   // namespace port::render
#endif
