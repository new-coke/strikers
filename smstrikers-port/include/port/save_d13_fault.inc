// Included by SaveLoad.cpp only. No new TU, allocator replacement or worker.
#include "port/save_d13_fault.hpp"
#include "port/config.h"
#include "port/native_watchdog.hpp"
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace port::save_d13
{
namespace
{
constexpr unsigned MaxRecords = 1024, MaxBytes = 1024 * 1024;
constexpr unsigned MaxEvents = 16384;
constexpr uint64_t HeartbeatUs = 500000;
constexpr char MarkerPath[] = "ux0:data/strikersVita/save_d13_fault.marker";
constexpr char OwnerPath[] = "ux0:data/strikersVita/save_fixture_lease/owner.json";

struct Request
{
    char test[3]{}, run[61]{}, nonce[33]{}, lease[25]{}, prepare[61]{};
    char self[65]{}, ini[65]{};
};
struct UI
{
    bool present{}, prompt{}, ready{}, locked{}, visible{}, busy{}, started{}, finished{};
    bool continued{}, success{};
    int type{-1}, next{-1};
    long result{};
    bool operator==(const UI&) const = default;
};
struct State
{
    Request request{};
    UI ui{};
    SceUID descriptor{-1}, pid{-1}, frontend{-1};
    uint64_t beginUs{}, lastWriteUs{};
    unsigned records{}, bytes{}, events{}, attempts{}, creates{}, callbacks{}, retries{}, continues{};
    unsigned faultSeq{}, callbackSeq{}, promptSeq{}, retrySeq{}, continueSeq{}, createSeq{};
    unsigned long faultLogical{}, faultCapacity{}, faultOldCapacity{}, faultSlot{};
    uintptr_t faultOwner{};
    long firstResult{}, lastResult{};
    int error{};
    bool initialized{}, active{}, armed{}, consumed{}, dirty{}, faultFileNull{};
} state;

bool frontend() noexcept
{
    // Current CARD backend calls completion inline on the initiating frontend.
    return state.active && sceKernelGetThreadId() == state.frontend;
}
unsigned event() noexcept
{
    state.dirty = true;
    if (state.events >= MaxEvents) {
        state.error = -75;
        state.armed = false;
        return state.events;
    }
    return ++state.events;
}
bool exact(const char* value, const char* wanted) noexcept
{
    return value && std::strcmp(value, wanted) == 0;
}
bool label(const char* text) noexcept
{
    unsigned n = 0;
    for (; text[n]; ++n) {
        const char c = text[n];
        if (n >= 60 || !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return n != 0;
}
bool hex(const char* text, unsigned count) noexcept
{
    for (unsigned i = 0; i < count; ++i)
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f')))
            return false;
    return text[count] == 0;
}
bool field(const char*& cursor, const char* prefix, char* out, unsigned capacity) noexcept
{
    while (*prefix) if (*cursor++ != *prefix++) return false;
    unsigned n = 0;
    while (*cursor && *cursor != '\n') {
        if (n + 1 >= capacity || *cursor < 0x20 || *cursor > 0x7e) return false;
        out[n++] = *cursor++;
    }
    if (*cursor != '\n' || n == 0) return false;
    ++cursor;
    out[n] = 0;
    return true;
}
bool parse(const char* data, Request& r) noexcept
{
    constexpr char prefix[] = "STRIKERS_SAVE_D13_V1\n";
    if (std::strncmp(data, prefix, sizeof(prefix) - 1) != 0) return false;
    const char* p = data + sizeof(prefix) - 1;
    char slot[4]{}, logical[8]{}, capacity[8]{};
    return field(p, "case=", r.test, sizeof r.test) &&
        field(p, "run=", r.run, sizeof r.run) &&
        field(p, "nonce=", r.nonce, sizeof r.nonce) &&
        field(p, "lease=", r.lease, sizeof r.lease) &&
        field(p, "prepare=", r.prepare, sizeof r.prepare) &&
        field(p, "self=", r.self, sizeof r.self) &&
        field(p, "ini=", r.ini, sizeof r.ini) &&
        field(p, "slot=", slot, sizeof slot) &&
        field(p, "logical=", logical, sizeof logical) &&
        field(p, "capacity=", capacity, sizeof capacity) && *p == 0 &&
        (exact(r.test, "D2") || exact(r.test, "D3")) && label(r.run) && label(r.prepare) &&
        hex(r.nonce, 32) && hex(r.lease, 24) && hex(r.self, 64) && hex(r.ini, 64) &&
        exact(slot, "0") && exact(logical, "8256") && exact(capacity, "16384");
}
template<unsigned N>
bool read_small(const char* path, char (&buffer)[N]) noexcept
{
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) return false;
    SceIoStat st{};
    bool ok = sceIoGetstatByFd(fd, &st) == 0 && SCE_S_ISREG(st.st_mode) &&
        st.st_size > 0 && static_cast<uint64_t>(st.st_size) < N;
    unsigned used = 0;
    if (ok) {
        const unsigned wanted = static_cast<unsigned>(st.st_size);
        for (unsigned calls = 0; calls < 8 && used < wanted; ++calls) {
            const SceSSize n = sceIoRead(fd, buffer + used, wanted - used);
            if (n <= 0 || static_cast<unsigned>(n) > wanted - used) { ok = false; break; }
            used += static_cast<unsigned>(n);
        }
        char extra{};
        ok = ok && used == wanted && sceIoRead(fd, &extra, 1) == 0;
        SceIoStat after{};
        ok = ok && sceIoGetstatByFd(fd, &after) == 0 && after.st_size == st.st_size &&
             SCE_S_ISREG(after.st_mode);
    }
    const int closed = sceIoClose(fd);
    if (!ok || closed != 0) return false;
    buffer[used] = 0;
    // Embedded NUL, CR and non-ASCII bytes cannot shorten a valid request.
    for (unsigned i = 0; i < used; ++i)
        if (buffer[i] == 0 || buffer[i] == '\r' || static_cast<unsigned char>(buffer[i]) > 0x7f)
            return false;
    return true;
}
bool write_record(const char* kind, unsigned long frame) noexcept
{
    if (state.descriptor < 0 || state.records >= MaxRecords || state.bytes >= MaxBytes)
        return false;
    const uint64_t now = sceKernelGetSystemTimeWide();
    char row[2048];
    const UI& u = state.ui;
    const int n = std::snprintf(row, sizeof row,
        "{\"v\":1,\"kind\":\"%s\",\"record\":%u,\"frame\":%lu,\"t_us\":%llu,"
        "\"pid\":%d,\"frontend\":%d,\"begin_us\":%llu,\"case\":\"%s\",\"run\":\"%s\","
        "\"nonce\":\"%s\",\"lease\":\"%s\",\"prepare\":\"%s\",\"self_expected\":\"%s\","
        "\"ini_expected\":\"%s\",\"error\":%d,\"armed\":%d,\"consumed\":%d,\"events\":%u,"
        "\"attempts\":%u,\"creates\":%u,\"callbacks\":%u,\"retries\":%u,\"continues\":%u,"
        "\"fault_seq\":%u,\"callback_seq\":%u,\"prompt_seq\":%u,\"retry_seq\":%u,"
        "\"continue_seq\":%u,\"create_seq\":%u,\"fault_logical\":%lu,\"fault_capacity\":%lu,"
        "\"fault_old_capacity\":%lu,\"fault_slot\":%lu,\"fault_file_null\":%d,"
        "\"fault_owner\":%llu,\"first_result\":%ld,\"last_result\":%ld,"
        "\"hdr_capacity\":%u,\"hdr_owner\":%llu,\"hdr_alias\":%llu,\"present\":%d,\"prompt\":%d,"
        "\"ready\":%d,\"locked\":%d,\"visible\":%d,\"busy\":%d,\"started\":%d,\"finished\":%d,"
        "\"continued\":%d,\"success\":%d,\"type\":%d,\"next\":%d,\"ui_result\":%ld}\n",
        kind, state.records, frame, static_cast<unsigned long long>(now),
        static_cast<int>(state.pid), static_cast<int>(state.frontend),
        static_cast<unsigned long long>(state.beginUs), state.request.test, state.request.run,
        state.request.nonce, state.request.lease, state.request.prepare, state.request.self,
        state.request.ini, state.error, state.armed, state.consumed, state.events,
        state.attempts, state.creates, state.callbacks, state.retries, state.continues,
        state.faultSeq, state.callbackSeq, state.promptSeq, state.retrySeq,
        state.continueSeq, state.createSeq, state.faultLogical, state.faultCapacity,
        state.faultOldCapacity, state.faultSlot, state.faultFileNull,
        static_cast<unsigned long long>(state.faultOwner),
        state.firstResult, state.lastResult, static_cast<unsigned>(gIconDataCache.mIconHdrCapacity),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(gIconDataCache.mIconHdrBuffer)),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(gIconDataCache.mIconDataInfo.pHeaderData)),
        u.present, u.prompt, u.ready, u.locked, u.visible, u.busy, u.started, u.finished,
        u.continued, u.success, u.type, u.next, u.result);
    if (n <= 0 || static_cast<unsigned>(n) >= sizeof row ||
        static_cast<unsigned>(n) > MaxBytes - state.bytes) return false;
    unsigned done = 0;
    for (unsigned calls = 0; calls < 8 && done < static_cast<unsigned>(n); ++calls) {
        const SceSSize wrote = sceIoWrite(state.descriptor, row + done, n - done);
        if (wrote <= 0 || static_cast<unsigned>(wrote) > static_cast<unsigned>(n) - done)
            return false;
        done += static_cast<unsigned>(wrote);
    }
    if (done != static_cast<unsigned>(n)) return false;
    ++state.records;
    state.bytes += done;
    state.lastWriteUs = now;
    state.dirty = false;
    return true;
}
void output_failure() noexcept
{
    state.error = -5;
    state.armed = false;
    // No retry and no callback I/O. The exclusive claim remains on disk.
    OSReport("[d13] diagnostic output failed; evidence invalid\n");
    if (state.descriptor >= 0) (void)sceIoClose(state.descriptor);
    state.descriptor = -1;
    state.active = false;
}
} // namespace

void initialize() noexcept
{
    if (state.initialized) return;
    state.initialized = true;
    if (!exact(std::getenv("STRIKERS_SAVE_D13_FAULT"), "allocheader")) return;
    if (!PortDiagnosticsEnabled() || !port::watchdog::enabled() ||
        port::watchdog::error_code() != 0 ||
        !exact(std::getenv("STRIKERS_VITA_TEST_MATCH"), "0")) {
        OSReport("[d13] inactive: diagnostics/watchdog/frontend configuration invalid\n");
        return;
    }
    char marker[768]{}, owner[512]{}, expectedOwner[512]{}, expectedLog[128]{};
    Request request{};
    if (!read_small(MarkerPath, marker) || !parse(marker, request) ||
        !exact(std::getenv("STRIKERS_SAVE_D13_NONCE"), request.nonce)) {
        OSReport("[d13] inactive: marker/nonce invalid\n");
        return;
    }
    const int logBytes = std::snprintf(expectedLog, sizeof expectedLog,
        "ux0:data/strikersVita/%s.log", request.run);
    const int ownerBytes = std::snprintf(expectedOwner, sizeof expectedOwner,
        "{\"label\":\"%s\",\"schema\":\"strikers-save-fixture-lease-v1\","
        "\"self_sha256\":\"%s\",\"token\":\"%s\"}\n", request.prepare, request.self, request.lease);
    if (logBytes <= 0 || static_cast<unsigned>(logBytes) >= sizeof expectedLog ||
        ownerBytes <= 0 || static_cast<unsigned>(ownerBytes) >= sizeof expectedOwner ||
        !exact(std::getenv("STRIKERS_LOG"), expectedLog) || !read_small(OwnerPath, owner) ||
        !exact(owner, expectedOwner)) {
        OSReport("[d13] inactive: run/lease identity mismatch\n");
        return;
    }
    char path[128]{};
    const int pathBytes = std::snprintf(path, sizeof path,
        "ux0:data/strikersVita/save_d13_%s.jsonl", request.nonce);
    if (pathBytes <= 0 || static_cast<unsigned>(pathBytes) >= sizeof path) return;
    state.descriptor = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_EXCL, 0600);
    if (state.descriptor < 0) {
        OSReport("[d13] inactive: exclusive nonce claim unavailable\n");
        return;
    }
    state.request = request;
    state.pid = sceKernelGetProcessId();
    state.frontend = sceKernelGetThreadId();
    state.beginUs = sceKernelGetSystemTimeWide();
    if (state.pid <= 0 || state.frontend <= 0 || !write_record("claim", 0)) {
        output_failure();
        return;
    }
    state.active = state.armed = state.dirty = true;
    OSReport("[d13] armed case=%s run=%s nonce=%s lease=%s pid=%d frontend=%d\n",
        request.test, request.run, request.nonce, request.lease,
        static_cast<int>(state.pid), static_cast<int>(state.frontend));
}

bool fail_replacement(unsigned long slot, bool fileNull, unsigned long logical,
                      unsigned long capacity, const void* owner, const void* alias,
                      unsigned long oldCapacity) noexcept
{
    if (!frontend()) return false;
    ++state.attempts;
    const unsigned seq = event();
    if (!state.armed || state.error) return false;
    if (state.attempts != 1 || slot != 0 || !fileNull || logical != 8256 ||
        capacity != 16384 || !owner || alias != owner || oldCapacity != logical) {
        state.error = -22;
        state.armed = false;
        return false; // A mismatch never injects a different allocation later.
    }
    state.armed = false;
    state.consumed = true;
    state.faultSeq = seq;
    state.faultLogical = logical;
    state.faultCapacity = capacity;
    state.faultOldCapacity = oldCapacity;
    state.faultSlot = slot;
    state.faultFileNull = fileNull;
    state.faultOwner = reinterpret_cast<uintptr_t>(owner);
    return true; // Only caller substitutes NULL; original error branch owns -1002.
}
void note_create() noexcept
{
    if (!frontend()) return;
    ++state.creates;
    state.createSeq = event();
}
void note_save_callback(long result) noexcept
{
    if (!frontend()) return;
    if (state.callbacks == 0) state.firstResult = result;
    ++state.callbacks;
    state.lastResult = result;
    state.callbackSeq = event();
}
void note_action(bool retry) noexcept
{
    if (!frontend()) return;
    if (retry) { ++state.retries; state.retrySeq = event(); }
    else { ++state.continues; state.continueSeq = event(); }
}
void observe_ui(bool present, bool prompt, bool ready, bool locked, bool visible,
                bool busy, bool started, bool finished, bool continued, bool success,
                int type, int next, long result) noexcept
{
    if (!frontend()) return;
    const UI nextUI{present,prompt,ready,locked,visible,busy,started,finished,continued,success,type,next,result};
    if (nextUI == state.ui) return;
    const unsigned seq = event();
    if (prompt && !state.ui.prompt) state.promptSeq = seq;
    state.ui = nextUI;
}
void drain(unsigned long frame) noexcept
{
    if (!frontend()) return;
    const uint64_t now = sceKernelGetSystemTimeWide();
    if (!state.dirty && (!state.ui.prompt || now - state.lastWriteUs < HeartbeatUs)) return;
    if (!write_record("sample", frame)) output_failure();
}
void shutdown(unsigned long frame) noexcept
{
    if (!frontend()) return;
    // This record is deliberately not a successful-close attestation.
    if (!write_record("exit_pending_close", frame)) { output_failure(); return; }
    const int sync = sceIoSyncByFd(state.descriptor, 0);
    const int closed = sceIoClose(state.descriptor);
    if (sync != 0) state.error = sync;
    else if (closed != 0) state.error = closed;
    state.descriptor = -1;
    state.active = state.armed = false;
    // The terminal reader must join this line to the same nonce and watchdog.
    OSReport("[d13] closed nonce=%s sync=%d close=%d error=%d\n",
        state.request.nonce, sync, closed, state.error);
}
} // namespace port::save_d13
