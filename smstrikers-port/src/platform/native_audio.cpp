#if defined(PORT_VITA)
#include "port/native_audio.hpp"
#include "port/native_audio_record.hpp"
#include "port/asset_archive.h"
#include "port/config.h"
#include "port/host.h"
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/cpu.h>
#endif
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <memory>
#include <pthread.h>
#include <string>
namespace
{
constexpr size_t Budget = 2 * 1024 * 1024;
using Digest = std::array<uint8_t, 32>;
struct Entry
{
    uint64_t key;
    uint32_t size;
    Digest digest;
};
struct Record
{
    std::vector<uint8_t> bytes;
    port::NativeAudioRecord view;
};
struct Binding
{
    uintptr_t base;
    std::shared_ptr<Record> record;
};
struct Cached
{
    Digest digest;
    std::shared_ptr<Record> record;
};
struct Request
{
    Entry entry;
    uintptr_t base;
};
PortAssetArchive* archive = nullptr;
std::vector<Entry> entries;
std::list<Cached> records;
std::vector<Binding> bindings;
std::array<Request, 64> requests{};
size_t queued = 0;
Request inFlight{};
bool loading = false, stopping = false, started = false;
pthread_t worker{};
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
// Use the same explicit pthread synchronization as the renderer command ring.
// Vita libstdc++ weak gthread activation can otherwise compile out mutex locks.
struct CacheLock
{
    bool owns;
    explicit CacheLock(bool tryOnly = false)
        : owns((tryOnly ? pthread_mutex_trylock(&mutex) : pthread_mutex_lock(&mutex)) == 0)
    {}
    CacheLock(const CacheLock&) = delete;
    CacheLock& operator=(const CacheLock&) = delete;
    ~CacheLock()
    {
        if (owns)
            pthread_mutex_unlock(&mutex);
    }
    void unlock()
    {
        pthread_mutex_unlock(&mutex);
        owns = false;
    }
    void lock() { owns = pthread_mutex_lock(&mutex) == 0; }
};
std::atomic<bool> enabled{ false };
NativeAudioStats stats{};

bool read(const char* path, std::vector<uint8_t>& bytes, size_t maximum)
{
    const int entry = archive ? port_asset_archive_find(archive, path) : -1;
    if (entry < 0)
        return false;
    const uint64_t size = port_asset_archive_size(archive, entry);
    if (!size || size > maximum)
        return false;
    bytes.resize(size);
    return port_asset_archive_read(archive, entry, bytes.data(), bytes.size(), 0) ==
           long(bytes.size());
}
std::shared_ptr<Record> load(const Entry& entry)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string path = "native/v1/audio/";
    path.reserve(84);
    for (auto c : entry.digest)
    {
        path += digits[c >> 4];
        path += digits[c & 15];
    }
    path += ".spcm";
    auto out = std::make_shared<Record>();
    if (!read(path.c_str(), out->bytes, 64 * 1024) || out->bytes.size() != entry.size ||
        !out->view.parse(out->bytes))
        return {};
    return out;
}
void bind_record(uintptr_t base, const std::shared_ptr<Record>& record)
{
    for (auto& binding : bindings)
        if (binding.base == base)
        {
            binding.record = record;
            return;
        }
    if (bindings.size() == 128)
        bindings.erase(bindings.begin());
    bindings.push_back({ base, record });   // Capacity reserved before the mixer starts.
}

// Cache and queue helpers require the cache lock. Only the worker publishes
// records; mixer-side lookup and request submission never perform archive I/O.
auto find_cached(const Digest& digest)
{
    return std::find_if(records.begin(), records.end(),
                        [&](const Cached& record) { return record.digest == digest; });
}

void queue_request(const Entry& entry, uintptr_t base)
{
    bool pending = loading && inFlight.base == base && inFlight.entry.digest == entry.digest;
    for (size_t i = 0; i < queued && !pending; ++i)
        pending = requests[i].base == base && requests[i].entry.digest == entry.digest;
    if (!pending && queued < requests.size())
    {
        requests[queued++] = { entry, base };
        pthread_cond_signal(&ready);
    }
}

bool publish_record(const Request& request, const std::shared_ptr<Record>& record)
{
    while (stats.cacheBytes + record->bytes.size() > Budget && !records.empty())
    {
        const auto old = records.back().record;
        bindings.erase(
            std::remove_if(bindings.begin(), bindings.end(),
                           [&](const Binding& binding) { return binding.record == old; }),
            bindings.end());
        stats.cacheBytes -= old->bytes.size();
        records.pop_back();
    }
    try
    {
        records.push_front({ request.entry.digest, record });
    }
    catch (...)
    {
        ++stats.rejected;
        return false;
    }
    stats.cacheBytes += record->bytes.size();
    ++stats.loads;
    bind_record(request.base, record);
    return true;
}

bool load_catalogue()
{
    std::vector<uint8_t> bytes;
    if (!read("native/v1/audio-index.bin", bytes, 4 * 1024 * 1024) || bytes.size() < 16 ||
        std::memcmp(bytes.data(), "STAIDX01", 8) || port::audio_word(bytes.data() + 8) != 1)
        return false;
    const size_t count = port::audio_word(bytes.data() + 12);
    if (count > 90000 || bytes.size() != 16 + count * 44)
        return false;
    entries.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        const auto* p = bytes.data() + 16 + i * 44;
        Entry entry{ uint64_t(port::audio_word(p)) | uint64_t(port::audio_word(p + 4)) << 32,
                     port::audio_word(p + 8),
                     {} };
        if (entry.size < 60 || entry.size > 64 * 1024 ||
            (!entries.empty() && entry.key < entries.back().key))
            return false;
        std::copy_n(p + 12, 32, entry.digest.begin());
        entries.push_back(entry);
    }
    return true;
}

void* run(void*)
{
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    const auto id = sceKernelGetThreadId();
    sceKernelChangeThreadCpuAffinityMask(id, SCE_KERNEL_CPU_MASK_USER_0);
    sceKernelChangeThreadPriority(id, 0x10000120);   // Below the audio output thread.
#endif
    CacheLock lock;
    if (!lock.owns)
        return nullptr;
    for (;;)
    {
        while (!queued && !stopping)
            pthread_cond_wait(&ready, &mutex);
        if (stopping)
            break;
        inFlight = requests[0];
        std::move(requests.begin() + 1, requests.begin() + queued, requests.begin());
        --queued;
        const Request request = inFlight;
        loading = true;
        const auto cached = find_cached(request.entry.digest);
        if (cached != records.end())
        {
            bind_record(request.base, cached->record);
            records.splice(records.begin(), records, cached);
            loading = false;
            continue;
        }
        // Archive I/O, decompression and record validation run outside the
        // cache lock. The mixer never waits for this worker.
        lock.unlock();
        const auto begin = port_monotonic_ns();
        std::shared_ptr<Record> record;
        try
        {
            record = load(request.entry);
        }
        catch (...)
        {}
        const uint64_t elapsed = (port_monotonic_ns() - begin) / 1000;
        lock.lock();
        loading = false;
        if (stopping)
            break;
        stats.maxLoadUs = std::max(stats.maxLoadUs, elapsed);
        if (!record)
        {
            ++stats.rejected;
            continue;
        }
        if (!publish_record(request, record))
            continue;
        if (PortDiagnosticsEnabled() &&
            (stats.loads <= 4 || (stats.loads & (stats.loads - 1)) == 0))
        {
            const auto snapshot = stats;
            lock.unlock();
            std::fprintf(stderr,
                         "[native-audio] async_loads=%llu hits=%llu misses=%llu rejected=%llu "
                         "cache_bytes=%llu max_load_us=%llu\n",
                         (unsigned long long)snapshot.loads, (unsigned long long)snapshot.hits,
                         (unsigned long long)snapshot.misses, (unsigned long long)snapshot.rejected,
                         (unsigned long long)snapshot.cacheBytes,
                         (unsigned long long)snapshot.maxLoadUs);
            lock.lock();
        }
    }
    return nullptr;
}
void clear()
{
    entries.clear();
    records.clear();
    bindings.clear();
    queued = 0;
    loading = false;
    if (archive)
        port_asset_archive_close(archive);
    archive = nullptr;
}
}   // namespace
void PortNativeAudioShutdown() noexcept
{
    enabled.store(false, std::memory_order_release);
    {
        CacheLock lock;
        if (!lock.owns)
            return;
        stopping = true;
        pthread_cond_signal(&ready);
    }
    if (started)
    {
        pthread_join(worker, nullptr);
        started = false;
    }
    CacheLock lock;
    clear();
}
void PortNativeAudioInitialize() noexcept
{
    PortNativeAudioShutdown();
    CacheLock lock;
    if (!lock.owns)
        return;
    stats = {};
    stopping = false;
    const char* enable = std::getenv("STRIKERS_VITA_NATIVE_AUDIO");
    const char* path = std::getenv("STRIKERS_ASSET_ARCHIVE");
    if (!enable || std::strcmp(enable, "1") || !path || !*path)
        return;
    try
    {
        char error[256];
        archive = port_asset_archive_open(path, error, sizeof error);
        if (!archive || !load_catalogue())
        {
            clear();
            return;
        }
        bindings.reserve(128);
        pthread_attr_t attr;
        int errorCode = pthread_attr_init(&attr);
        if (!errorCode)
        {
            errorCode = pthread_attr_setstacksize(&attr, 128 * 1024);
            if (!errorCode)
                errorCode = pthread_create(&worker, &attr, run, nullptr);
            pthread_attr_destroy(&attr);
        }
        if (errorCode)
        {
            clear();
            return;
        }
        started = true;
        enabled.store(true, std::memory_order_release);
        std::fprintf(stderr,
                     "[native-audio] indexed_blocks=%u budget=%u async_read=1 mixer_io=0 "
                     "exact_frame_validation=1\n",
                     unsigned(entries.size()), unsigned(Budget));
    }
    catch (...)
    {
        clear();
    }
}
bool PortNativeAudioBlock(const uint8_t* source, const int16_t* coef, int16_t y1, int16_t y2,
                          int16_t* pcm) noexcept
{
    if (!source || !coef || !pcm || !enabled.load(std::memory_order_acquire))
        return false;
    CacheLock lock(true);
    if (!lock.owns || !enabled.load(std::memory_order_relaxed))
        return false;
    const auto base = reinterpret_cast<uintptr_t>(source);
    for (const auto& binding : bindings)
    {
        const auto& view = binding.record->view;
        if (base >= binding.base && base - binding.base < view.sourceBytes &&
            !((base - binding.base) % 8) &&
            view.frame((base - binding.base) / 8, source, coef, y1, y2, pcm))
        {
            ++stats.hits;
            return true;
        }
    }
    const auto key = port::audio_frame_key(source, coef, y1, y2);
    auto it = std::lower_bound(entries.begin(), entries.end(), key,
                               [](const Entry& e, uint64_t k) { return e.key < k; });
    for (unsigned tried = 0; it != entries.end() && it->key == key && tried < 8; ++it, ++tried)
    {
        const auto cached = find_cached(it->digest);
        if (cached != records.end())
        {
            if (cached->record->view.frame(0, source, coef, y1, y2, pcm))
            {
                bind_record(base, cached->record);
                records.splice(records.begin(), records, cached);
                ++stats.hits;
                return true;
            }
            continue;
        }
        queue_request(*it, base);
    }
    ++stats.misses;
    // Preserve the exact existing decoder on a cold/mismatched record. Once
    // ready, the binding validates each raw frame/history before using PCM.
    return false;
}
NativeAudioStats PortNativeAudioGetStats() noexcept
{
    CacheLock lock;
    return lock.owns ? stats : NativeAudioStats{};
}
#endif
