#pragma once
// Isolated, uncompiled owner support. Only included by the optional Vita probe.
#include <psp2/dmac5.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/cpu.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace avc_probe_owner
{
constexpr unsigned MiB = 1024u * 1024u;
constexpr uint64_t WindowUs = 30000000;
constexpr unsigned JsonBytes = 65536, MaxFiles = 24, MaxIOCallsPerTransfer = 128;
constexpr unsigned MaxFileBytes = MiB, MaxArtifactBytes = 1916928 + 4 * JsonBytes;
static_assert(ATOMIC_INT_LOCK_FREE == 2 && sizeof(unsigned) == 4,
              "Guardian requires lock-free 32-bit atomics");

struct Guardian
{
    std::atomic<unsigned> armed{0}, ready{0}, stop{0}, finished{0}, fired{0};
    std::atomic<int> exit_return{0};
    uint64_t begin_us = 0, deadline_us = 0;
    SceUID thread = -1;
    Guardian* argument = this; // Stable even if the SDK retains the argument storage.
    int priority = -1, create_rc = -1, start_rc = -1, wait_rc = -1, delete_rc = -1;
    int thread_status = -1, delay_error = 0;
    bool started = false, reaped = false, deleted = false;

    static uint64_t now() noexcept
    {
        const SceInt64 value = sceKernelGetSystemTimeWide();
        return value < 0 ? 0 : uint64_t(value);
    }
    bool within() const noexcept
    {
        const uint64_t value = now();
        return value >= begin_us && value < deadline_us && !fired.load(std::memory_order_acquire);
    }
    static int entry(SceSize bytes, void* opaque) noexcept
    {
        if (bytes != sizeof(Guardian*) || !opaque) return 91;
        Guardian* self = *static_cast<Guardian**>(opaque);
        if (!self || !self->armed.load(std::memory_order_acquire)) return 92;
        self->ready.store(1, std::memory_order_release);
        while (!self->stop.load(std::memory_order_acquire))
        {
            const uint64_t value = now();
            if (value < self->begin_us || value >= self->deadline_us)
            {
                self->fired.store(1, std::memory_order_release);
                // No I/O, mutex, allocation, hashing or codec cleanup before this request.
                const int rc = sceKernelExitProcess(93);
                // The SDK does not declare ExitProcess noreturn. Preserve the returned error;
                // root's independent 60-second launch watchdog is mandatory for this case.
                self->exit_return.store(rc, std::memory_order_release);
                self->finished.store(1, std::memory_order_release);
                return 93;
            }
            const int rc = sceKernelDelayThread(10000);
            if (rc < 0)
            {
                self->delay_error = rc;
                self->fired.store(1, std::memory_order_release);
                const int exitRc = sceKernelExitProcess(94);
                self->exit_return.store(exitRc, std::memory_order_release);
                self->finished.store(1, std::memory_order_release);
                return 94;
            }
        }
        self->finished.store(1, std::memory_order_release);
        return 0;
    }
    bool start() noexcept
    {
        begin_us = now();
        if (!begin_us || begin_us > std::numeric_limits<uint64_t>::max() - WindowUs) return false;
        deadline_us = begin_us + WindowUs;
        priority = sceKernelGetThreadCurrentPriority();
        if (priority < 0) return false;
        create_rc = sceKernelCreateThread("strikers_avc_guard", entry, priority, 64u * 1024u,
                                         0, SCE_KERNEL_CPU_MASK_USER_ALL, nullptr);
        if (create_rc < 0) return false;
        thread = create_rc;
        armed.store(1, std::memory_order_release);
        start_rc = sceKernelStartThread(thread, sizeof argument, &argument);
        if (start_rc < 0)
        {
            delete_rc = sceKernelDeleteThread(thread); // Created but not started.
            deleted = delete_rc >= 0;
            return false;
        }
        started = true;
        // Do not enter the codec until the independent thread has actually reported ready.
        for (unsigned i = 0; i < 100 && within(); ++i)
        {
            if (ready.load(std::memory_order_acquire)) return true;
            if (sceKernelDelayThread(1000) < 0) return false;
        }
        return false;
    }
    bool finish() noexcept
    {
        if (!started) return thread < 0 || deleted;
        stop.store(1, std::memory_order_release);
        SceUInt timeout = 1000000; // Same microsecond contract used by native_watchdog.cpp.
        wait_rc = sceKernelWaitThreadEnd(thread, &thread_status, &timeout);
        if (wait_rc < 0) return false; // Never delete a live/unconfirmed thread.
        reaped = true;
        delete_rc = sceKernelDeleteThread(thread);
        deleted = delete_rc >= 0;
        return deleted && thread_status == 0 && finished.load(std::memory_order_acquire) &&
               !fired.load(std::memory_order_acquire);
    }
};

// Fixed-capacity JSON builder; never truncate a supposedly complete record.
struct Json
{
    std::array<char, JsonBytes> bytes{};
    size_t size = 0;
    bool good = true;
    void reset() noexcept { size = 0; good = true; bytes[0] = 0; }
    void add(const char* fmt, ...) noexcept
    {
        if (!good) return;
        va_list args;
        va_start(args, fmt);
        const int n = std::vsnprintf(bytes.data() + size, bytes.size() - size, fmt, args);
        va_end(args);
        if (n < 0 || size_t(n) >= bytes.size() - size) { good = false; return; }
        size += size_t(n);
    }
    void string(const char* value) noexcept
    {
        if (!value) { add("null"); return; }
        add("\"");
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p)
        {
            if (*p == '"' || *p == '\\') add("\\%c", *p);
            else if (*p < 32) add("\\u%04x", unsigned(*p));
            else add("%c", *p);
        }
        add("\"");
    }
};

struct Digest
{
    alignas(64) std::array<uint8_t, 32> bytes{};
    char hex[65]{};
    bool valid = false;
};
struct Artifact
{
    char name[48]{};
    unsigned bytes = 0;
    char sha256[65]{};
    int open_rc = -1, sync_rc = -1, close_rc = -1, read_close_rc = -1;
    bool written = false, readback_equal = false;
};

struct IO
{
    Guardian* guardian = nullptr;
    char directory[192]{};
    SceUID journal = -1, scratch_block = -1;
    uint8_t* scratch = nullptr;
    unsigned journal_bytes = 0, artifact_bytes = 0, count = 0, sequence = 0;
    unsigned total_written = 0, total_read = 0;
    int error_rc = 0, scratch_alloc_rc = -1, scratch_base_rc = -1, scratch_free_rc = -1;
    int journal_sync_rc = -1, journal_close_rc = -1;
    bool directory_owned = false;
    const char* error = nullptr;
    std::array<Artifact, MaxFiles> files{};
    Json json;

    bool fail(const char* why, int rc = -1) noexcept
    {
        if (!error) { error = why; error_rc = rc; }
        return false;
    }
    bool live() noexcept { return guardian && guardian->within(); }
    bool path(const char* leaf, char (&out)[256]) noexcept
    {
        if (!directory_owned) return fail("output directory not owned");
        const int n = std::snprintf(out, sizeof out, "%s/%s", directory, leaf);
        return (n > 0 && size_t(n) < sizeof out) || fail("output path");
    }
    bool write_all(SceUID fd, const void* input, unsigned bytes) noexcept
    {
        const auto* source = static_cast<const uint8_t*>(input);
        unsigned at = 0;
        for (unsigned call = 0; at < bytes && call < MaxIOCallsPerTransfer; ++call)
        {
            if (!live()) return fail("write deadline");
            const unsigned remaining = bytes - at;
            const unsigned request = remaining > 65536 ? 65536 : remaining;
            const SceSSize rc = sceIoWrite(fd, source + at, request);
            if (rc <= 0 || unsigned(rc) > request) return fail("write extent", int(rc));
            at += unsigned(rc); total_written += unsigned(rc);
        }
        return at == bytes || fail("write call bound");
    }
    bool read_exact(SceUID fd, void* output, unsigned bytes) noexcept
    {
        auto* target = static_cast<uint8_t*>(output);
        unsigned at = 0;
        for (unsigned call = 0; at < bytes && call < MaxIOCallsPerTransfer; ++call)
        {
            if (!live()) return fail("read deadline");
            const unsigned remaining = bytes - at;
            const unsigned request = remaining > 65536 ? 65536 : remaining;
            const SceSSize rc = sceIoRead(fd, target + at, request);
            if (rc <= 0 || unsigned(rc) > request) return fail("read extent", int(rc));
            at += unsigned(rc); total_read += unsigned(rc);
        }
        if (at != bytes) return fail("read call bound");
        uint8_t extra = 0;
        const SceSSize rc = sceIoRead(fd, &extra, 1);
        if (rc > 0) ++total_read;
        return rc == 0 || fail("readback trailing byte", int(rc));
    }
    // Main thread only. The guardian never accesses this descriptor.
    bool note(const char* operation, const char* edge, int rc, uint64_t value = 0) noexcept
    {
        if (journal < 0) return false;
        char line[384];
        const int n = std::snprintf(line, sizeof line,
            "{\"seq\":%u,\"system_us\":%llu,\"op\":\"%s\",\"edge\":\"%s\",\"rc\":%d,\"value\":%llu}\n",
            sequence++, static_cast<unsigned long long>(Guardian::now()), operation, edge, rc,
            static_cast<unsigned long long>(value));
        if (n <= 0 || size_t(n) >= sizeof line ||
            unsigned(n) > JsonBytes - journal_bytes) return fail("journal bound");
        journal_bytes += unsigned(n); // Reserved even if a write remains partial.
        return write_all(journal, line, unsigned(n));
    }
    bool open(const char* label, Guardian& guard) noexcept
    {
        guardian = &guard;
        const int n = std::snprintf(directory, sizeof directory,
                                   "ux0:data/strikersVita/avc-continuity-%s", label);
        if (n <= 0 || size_t(n) >= sizeof directory) return fail("run path");
        const int rc = sceIoMkdir(directory, 0700);
        if (rc < 0) return fail("exclusive run directory", rc);
        directory_owned = true;
        char full[256];
        if (!path("journal.jsonl", full)) return false;
        journal = sceIoOpen(full, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_EXCL, 0600);
        if (journal < 0) return fail("exclusive journal", journal);
        return note("guardian", "ready", guard.start_rc, guard.deadline_us) &&
               note("process_id", "observed", sceKernelGetProcessId());
    }
    bool allocate_hash() noexcept
    {
        if (!note("sha_alloc", "intent", 0, MiB)) return false;
        scratch_alloc_rc = sceKernelAllocMemBlock("strikers_avc_sha",
            SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, MiB, nullptr);
        note("sha_alloc", "returned", scratch_alloc_rc, MiB);
        if (scratch_alloc_rc < 0) return fail("SHA allocation", scratch_alloc_rc);
        scratch_block = scratch_alloc_rc;
        void* base = nullptr;
        if (!note("sha_base", "intent", 0)) return false;
        scratch_base_rc = sceKernelGetMemBlockBase(scratch_block, &base);
        note("sha_base", "returned", scratch_base_rc);
        if (scratch_base_rc < 0 || !base) return fail("SHA base", scratch_base_rc);
        scratch = static_cast<uint8_t*>(base);
        return !error;
    }
    bool hash(const void* data, size_t bytes, Digest& out, bool journal_event = true) noexcept
    {
        if (!scratch || !data || !bytes || bytes > MiB || !live()) return fail("SHA extent/deadline");
        if (data != scratch) std::memcpy(scratch, data, bytes);
        if (journal_event && !note("sha256", "intent", 0, bytes)) return false;
        const int rc = sceSblDmac5Sha256Digest(scratch, out.bytes.data(), unsigned(bytes));
        if (journal_event) note("sha256", "returned", rc, bytes);
        if (rc < 0) return fail("SHA provider", rc);
        static constexpr char digits[] = "0123456789abcdef";
        for (unsigned i = 0; i < out.bytes.size(); ++i)
        { out.hex[2*i] = digits[out.bytes[i] >> 4]; out.hex[2*i+1] = digits[out.bytes[i] & 15]; }
        out.valid = true;
        return !error;
    }
    bool artifact(const char* name, const void* data, size_t bytes) noexcept
    {
        if (!data || !bytes || bytes > MaxFileBytes || count >= files.size() ||
            bytes > MaxArtifactBytes - artifact_bytes || std::strlen(name) >= sizeof files[0].name)
            return fail("artifact bound");
        auto& record = files[count++];
        std::strcpy(record.name, name); record.bytes = unsigned(bytes);
        artifact_bytes += unsigned(bytes);
        Digest original;
        if (!hash(data, bytes, original)) return false;
        std::strcpy(record.sha256, original.hex);
        char full[256];
        if (!path(name, full)) return false;
        record.open_rc = sceIoOpen(full, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_EXCL, 0600);
        if (record.open_rc < 0) return fail("exclusive artifact", record.open_rc);
        bool ok = write_all(record.open_rc, data, unsigned(bytes));
        if (ok)
        {
            record.sync_rc = sceIoSyncByFd(record.open_rc, 0);
            ok = record.sync_rc >= 0;
            if (!ok) fail("artifact sync", record.sync_rc);
        }
        record.close_rc = sceIoClose(record.open_rc); // Always exactly once, even after short write.
        if (record.close_rc < 0 || !ok) return fail("artifact write/sync/close", record.close_rc);
        record.written = true;
        const SceUID reader = sceIoOpen(full, SCE_O_RDONLY, 0);
        if (reader < 0) return fail("artifact readback open", reader);
        ok = read_exact(reader, scratch, unsigned(bytes));
        if (ok && std::memcmp(scratch, data, bytes)) ok = fail("artifact readback bytes differ", -7103);
        Digest observed;
        if (ok) ok = hash(scratch, bytes, observed) && !std::strcmp(original.hex, observed.hex);
        record.read_close_rc = sceIoClose(reader);
        record.readback_equal = ok && record.read_close_rc >= 0;
        return record.readback_equal || fail("artifact readback", record.read_close_rc);
    }
    bool json_artifact(const char* name) noexcept
    {
        return json.good ? artifact(name, json.bytes.data(), json.size) : fail("JSON capacity");
    }
    bool close_journal(Digest& digest) noexcept
    {
        if (journal < 0) return false;
        journal_sync_rc = sceIoSyncByFd(journal, 0);
        journal_close_rc = sceIoClose(journal);
        journal = -1; // Never reuse a possibly closed descriptor.
        if (journal_sync_rc < 0 || journal_close_rc < 0) return fail("journal sync/close");
        char full[256];
        if (!path("journal.jsonl", full)) return false;
        const SceUID reader = sceIoOpen(full, SCE_O_RDONLY, 0);
        if (reader < 0) return fail("journal readback open", reader);
        bool ok = read_exact(reader, scratch, journal_bytes);
        if (ok) ok = hash(scratch, journal_bytes, digest, false);
        const int rc = sceIoClose(reader);
        return (ok && rc >= 0) || fail("journal readback/close", rc);
    }
    bool free_hash() noexcept
    {
        if (scratch_block < 0) return true;
        scratch_free_rc = sceKernelFreeMemBlock(scratch_block);
        if (scratch_free_rc < 0) return fail("SHA free", scratch_free_rc);
        scratch_block = -1; scratch = nullptr;
        return true;
    }
    // Final small receipt after successful guardian join; the root's independent
    // launch deadline covers blocking storage calls in this last bounded suffix.
    bool terminal(const char* data, unsigned bytes) noexcept
    {
        if (!data || !bytes || bytes > 4096) return fail("terminal bound");
        char full[256];
        if (!path("terminal.json", full)) return false;
        const SceUID writer = sceIoOpen(full, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_EXCL, 0600);
        if (writer < 0) return fail("exclusive terminal", writer);
        bool ok = write_all(writer, data, bytes);
        const int sync = ok ? sceIoSyncByFd(writer, 0) : -1;
        const int close = sceIoClose(writer);
        if (!ok || sync < 0 || close < 0) return fail("terminal write/sync/close");
        std::array<uint8_t, 4096> copy{};
        const SceUID reader = sceIoOpen(full, SCE_O_RDONLY, 0);
        if (reader < 0) return fail("terminal readback open", reader);
        ok = read_exact(reader, copy.data(), bytes) && !std::memcmp(copy.data(), data, bytes);
        const int readClose = sceIoClose(reader);
        return (ok && readClose >= 0) || fail("terminal readback/close", readClose);
    }
};
} // namespace avc_probe_owner
