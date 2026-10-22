#include "dolphin/card.h"
#include "port/host.h"
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;
void port_card_test_reset();

namespace {
enum class Op { Open, Write, Flush, Close, OldRename, CommitRename, RestoreRename,
                Remove, DirectoryOpen, DirectoryRead, DirectoryClose };
struct Failure { Op op; std::string path; };
std::vector<Failure> failures;
std::map<FILE*, std::string> streams;
struct Directory { std::vector<std::string> entries; size_t next = 0; };
std::map<SceUID, Directory> directories;
SceUID nextDirectory = 1;
std::string root;
bool inCall;
int callbackCount, callbackChannel, callbackResult, checks;

bool fail(Op op, const std::string& path = {})
{
    for (auto it = failures.begin(); it != failures.end(); ++it) {
        if (it->op == op && path.find(it->path) != std::string::npos) {
            failures.erase(it);
            errno = EIO;
            return true;
        }
    }
    return false;
}

void done(s32 chan, s32 result)
{
    assert(inCall);
    ++callbackCount;
    callbackChannel = chan;
    callbackResult = result;
}

template<class F> void complete(int chan, int expected, F call)
{
    callbackCount = 0;
    inCall = true;
    const auto result = call();
    inCall = false;
    assert(result == CARD_RESULT_READY);
    assert(callbackCount == 1 && callbackChannel == chan && callbackResult == expected);
    assert(streams.empty());
    ++checks;
}

template<class F> void rejected(int expected, F call)
{
    callbackCount = 0;
    inCall = true;
    const auto result = call();
    inCall = false;
    assert(result == expected && callbackCount == 0);
    assert(streams.empty());
    ++checks;
}

CARDFileInfo* nestedFile;
unsigned char nestedRead[8];
unsigned outerCompletions, innerCompletions;
int outerResult;
void nested_done(s32 chan, s32 result)
{
    assert(chan == 0 && result == CARD_RESULT_READY);
    ++innerCompletions;
}
void outer_done(s32 chan, s32 result)
{
    assert(chan == 0 && result == outerResult);
    ++outerCompletions;
    assert(CARDReadAsync(nullptr, nestedRead, 8, 0, nested_done) == CARD_RESULT_FATAL_ERROR);
    const auto before = innerCompletions;
    assert(CARDReadAsync(nestedFile, nestedRead, 8, 8, nested_done) == CARD_RESULT_READY);
    assert(innerCompletions == before + 1);
}

std::string stem(const std::string& name, int chan = 0)
{
    constexpr char hex[] = "0123456789abcdef";
    std::string out = root + (chan ? "/cardB/" : "/cardA/");
    for (unsigned char c : name) {
        out += hex[c >> 4];
        out += hex[c & 15];
    }
    return out;
}

std::string read_bytes(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void put_bytes(const std::string& path, const std::string& bytes)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.close();
    assert(file);
}

CARDFileInfo create(const std::string& name, int chan = 0)
{
    CARDFileInfo info{};
    complete(chan, CARD_RESULT_READY, [&] {
        return CARDCreateAsync(chan, name.c_str(), 64, &info, done);
    });
    assert(info.fileNo >= 0 && info.length == 64);
    return info;
}

CARDStat status(CARDFileInfo info)
{
    CARDStat result{};
    assert(CARDGetStatus(info.chan, info.fileNo, &result) == CARD_RESULT_READY);
    return result;
}

void set_status(CARDFileInfo info, CARDStat& stat, int expected = CARD_RESULT_READY)
{
    complete(info.chan, expected, [&] {
        return CARDSetStatusAsync(info.chan, info.fileNo, &stat, done);
    });
}

CARDFileInfo reopen(const std::string& name, int chan = 0)
{
    port_card_test_reset();
    CARDFileInfo info{};
    assert(CARDOpen(chan, name.c_str(), &info) == CARD_RESULT_READY);
    return info;
}
} // namespace

FILE* port_card_test_fopen(const char* path, const char* mode)
{
    if (fail(Op::Open, path))
        return nullptr;
    FILE* f = std::fopen(path, mode);
    if (f)
        streams.emplace(f, path);
    return f;
}

size_t port_card_test_fwrite(const void* data, size_t size, size_t count, FILE* f)
{
    const bool shortWrite = fail(Op::Write, streams.at(f));
    return std::fwrite(data, size, count - (shortWrite && count ? 1 : 0), f);
}

int port_card_test_fflush(FILE* f)
{
    const bool failed = fail(Op::Flush, streams.at(f));
    const int result = std::fflush(f);
    return failed ? EOF : result;
}

int port_card_test_fclose(FILE* f)
{
    const bool failed = fail(Op::Close, streams.at(f));
    assert(streams.erase(f) == 1);
    const int result = std::fclose(f);
    return failed ? EOF : result;
}

int port_card_test_rename(const char* from, const char* to)
{
    const std::string source = from;
    const auto op = source.ends_with(".tmp") ? Op::CommitRename :
                    source.ends_with(".bak") ? Op::RestoreRename : Op::OldRename;
    if (fail(op, source))
        return -1;
    // sceIoRename refuses to overwrite; host rename would mask this constraint.
    if (fs::exists(to)) {
        errno = EEXIST;
        return -1;
    }
    return std::rename(from, to);
}

int port_card_test_remove(const char* path)
{
    return fail(Op::Remove, path) ? -1 : std::remove(path);
}

extern "C" {
int port_executable_dir(char* out, size_t size)
{
    if (root.size() + 1 > size)
        return -1;
    std::memcpy(out, root.c_str(), root.size() + 1);
    return 0;
}

int port_scan_dir(const char* path, void (*visit)(void*, const char*), void* user)
{
    for (const auto& entry : fs::directory_iterator(path))
        visit(user, entry.path().filename().c_str());
    return 0;
}

int sceIoMkdir(const char* path, SceMode)
{
    std::error_code ec;
    fs::create_directory(path, ec);
    return ec ? -1 : 0;
}

int sceIoRemove(const char* path)
{
    return port_card_test_remove(path);
}

SceUID sceIoDopen(const char* path)
{
    if (fail(Op::DirectoryOpen, path))
        return -1;
    std::error_code ec;
    Directory dir;
    for (const auto& entry : fs::directory_iterator(path, ec))
        dir.entries.push_back(entry.path().filename().string());
    if (ec)
        return -1;
    std::sort(dir.entries.begin(), dir.entries.end());
    const auto id = nextDirectory++;
    directories.emplace(id, std::move(dir));
    return id;
}

int sceIoDread(SceUID id, SceIoDirent* entry)
{
    if (fail(Op::DirectoryRead))
        return -1;
    auto& dir = directories.at(id);
    if (dir.next == dir.entries.size())
        return 0;
    std::memset(entry, 0, sizeof *entry);
    std::snprintf(entry->d_name, sizeof entry->d_name, "%s", dir.entries[dir.next++].c_str());
    return 1;
}

int sceIoDclose(SceUID id)
{
    assert(directories.erase(id) == 1);
    return fail(Op::DirectoryClose) ? -1 : 0;
}
} // extern "C"

int main()
{
    std::random_device random;
    for (unsigned attempt = 0; attempt < 32 && root.empty(); ++attempt) {
        const auto candidate = fs::temp_directory_path() /
            ("strikers-card-test-" + std::to_string(random()) + "-" + std::to_string(random()));
        if (fs::create_directory(candidate))
            root = candidate.string();
    }
    assert(!root.empty());
    struct Cleanup {
        ~Cleanup() { std::error_code ec; fs::remove_all(root, ec); }
    } cleanup;
    CARDInit();

    auto info = create("progress");
    auto initial = status(info);
    initial.time = 1234;
    set_status(info, initial);
    const auto path = stem("progress");
    assert(fs::exists(path + ".stat.bak"));
    info = reopen("progress");
    assert(status(info).time == 1234);

    unsigned char data[] = {1,2,3,4,5,6,7,8};
    const int beforeWrite = CARDGetXferredBytes(0);
    complete(0, CARD_RESULT_READY, [&] {
        return CARDWriteAsync(&info, data, sizeof data, 8, done);
    });
    assert(CARDGetXferredBytes(0) == beforeWrite + sizeof data);
    unsigned char readBack[sizeof data]{};
    complete(0, CARD_RESULT_READY, [&] {
        return CARDReadAsync(&info, readBack, sizeof readBack, 8, done);
    });
    assert(std::memcmp(readBack, data, sizeof data) == 0);

    // Completion errors are never also reported as submission errors. Nested
    // reads and rejected inner submissions keep each callback/result distinct.
    for (const auto fault : {Op::Open, Op::Close}) {
        failures = {{fault, ".sav"}};
        const int before = CARDGetXferredBytes(0);
        complete(0, CARD_RESULT_IOERROR, [&] {
            return CARDReadAsync(&info, readBack, sizeof readBack, 8, done);
        });
        assert(failures.empty() && CARDGetXferredBytes(0) == before);
    }
    complete(0, CARD_RESULT_IOERROR, [&] {
        return CARDReadAsync(&info, readBack, sizeof readBack, 60, done);
    });
    rejected(CARD_RESULT_FATAL_ERROR, [&] {
        return CARDReadAsync(&info, readBack, sizeof readBack, -1, done);
    });
    nestedFile = &info;
    failures = {{Op::Close, ".sav"}};
    outerResult = CARD_RESULT_IOERROR;
    assert(CARDReadAsync(&info, readBack, 8, 8, outer_done) == CARD_RESULT_READY);
    assert(outerCompletions == 1 && innerCompletions == 1 && failures.empty());
    outerResult = CARD_RESULT_READY;
    assert(CARDReadAsync(&info, readBack, 8, 8, outer_done) == CARD_RESULT_READY);
    assert(outerCompletions == 2 && innerCompletions == 2 && streams.empty());
    assert(std::memcmp(nestedRead, data, 8) == 0);

    // Existing save creation is refused without changing its bytes or metadata.
    const auto savedData = read_bytes(path + ".sav");
    const auto savedMeta = read_bytes(path + ".stat");
    CARDFileInfo rejectedFile{};
    rejectedFile.fileNo = -77;
    const int beforeExisting = CARDGetXferredBytes(0);
    rejected(CARD_RESULT_EXIST, [&] {
        return CARDCreateAsync(0, "progress", 512, &rejectedFile, done);
    });
    assert(rejectedFile.fileNo == -77 && CARDGetXferredBytes(0) == beforeExisting);
    assert(read_bytes(path + ".sav") == savedData && read_bytes(path + ".stat") == savedMeta);

    int sequence = 0;
    for (const auto fault : {Op::Open, Op::Flush, Op::Close}) {
        const auto label = "create-data-" + std::to_string(sequence++);
        failures = {{fault, ".sav"}};
        CARDFileInfo failed{};
        failed.fileNo = -22;
        const int before = CARDGetXferredBytes(0);
        complete(0, CARD_RESULT_IOERROR, [&] {
            return CARDCreateAsync(0, label.c_str(), 64, &failed, done);
        });
        assert(failures.empty() && failed.fileNo == -22);
        assert(CARDGetXferredBytes(0) == before && !fs::exists(stem(label) + ".sav"));
    }
    for (const auto fault : {Op::Open, Op::Write, Op::Flush, Op::Close, Op::CommitRename}) {
        const auto label = "create-stat-" + std::to_string(sequence++);
        const auto orphan = stem(label) + ".stat";
        put_bytes(orphan, savedMeta);
        failures = {{fault, ".stat.tmp"}};
        CARDFileInfo failed{};
        failed.fileNo = -22;
        const int before = CARDGetXferredBytes(0);
        complete(0, CARD_RESULT_IOERROR, [&] {
            return CARDCreateAsync(0, label.c_str(), 64, &failed, done);
        });
        assert(failures.empty() && failed.fileNo == -22);
        assert(CARDGetXferredBytes(0) == before && !fs::exists(stem(label) + ".sav"));
        assert(read_bytes(orphan) == savedMeta && !fs::exists(orphan + ".tmp"));
    }

    for (const auto fault : {Op::Open, Op::Write, Op::Flush, Op::Close}) {
        failures = {{fault, ".sav"}};
        const int before = CARDGetXferredBytes(0);
        const auto meta = read_bytes(path + ".stat");
        complete(0, CARD_RESULT_IOERROR, [&] {
            return CARDWriteAsync(&info, data, sizeof data, 0, done);
        });
        assert(failures.empty() && CARDGetXferredBytes(0) == before);
        assert(read_bytes(path + ".stat") == meta);
    }

    auto replacement = status(info);
    replacement.time = 4321;
    for (const auto fault : {Op::Open, Op::Write, Op::Flush, Op::Close,
                            Op::OldRename, Op::CommitRename}) {
        failures = {{fault, ".stat"}};
        const auto meta = read_bytes(path + ".stat");
        const int before = CARDGetXferredBytes(0);
        set_status(info, replacement, CARD_RESULT_IOERROR);
        assert(failures.empty() && read_bytes(path + ".stat") == meta);
        assert(status(info).time == 1234 && CARDGetXferredBytes(0) == before);
        assert(!fs::exists(path + ".stat.tmp"));
    }

    // A metadata error after writing save data must reach the caller too.
    failures = {{Op::Flush, ".stat.tmp"}};
    const int beforeMetadataWrite = CARDGetXferredBytes(0);
    complete(0, CARD_RESULT_IOERROR, [&] {
        return CARDWriteAsync(&info, data, sizeof data, 0, done);
    });
    assert(failures.empty() && CARDGetXferredBytes(0) == beforeMetadataWrite);

    // Failed publication and failed rollback keep the old metadata recoverable.
    const auto oldMeta = read_bytes(path + ".stat");
    failures = {{Op::CommitRename, ".stat.tmp"}, {Op::RestoreRename, ".stat.bak"}};
    set_status(info, replacement, CARD_RESULT_IOERROR);
    assert(failures.empty() && !fs::exists(path + ".stat"));
    assert(read_bytes(path + ".stat.bak") == oldMeta);
    info = reopen("progress");
    assert(status(info).time == 1234);
    set_status(info, replacement);
    assert(status(info).time == 4321);
    info = reopen("progress");
    assert(status(info).time == 4321); // A valid primary wins over an older backup.

    failures = {{Op::Remove, ".stat.bak"}};
    auto latest = status(info);
    latest.time = 5678;
    const auto primary = read_bytes(path + ".stat");
    set_status(info, latest, CARD_RESULT_IOERROR);
    assert(failures.empty() && read_bytes(path + ".stat") == primary);
    assert(status(info).time == 4321);
    set_status(info, latest);
    assert(status(info).time == 5678);

    // Interrupted replacement with corrupt primary recovers only a valid backup.
    put_bytes(path + ".stat", "broken");
    info = reopen("progress");
    assert(status(info).time == 4321);
    latest.time = 6789;
    set_status(info, latest);
    assert(status(info).time == 6789);
    const auto goodPrimary = read_bytes(path + ".stat");
    put_bytes(path + ".stat.bak", "invalid backup");
    info = reopen("progress");
    assert(status(info).time == 6789);
    latest.time = 7890;
    set_status(info, latest); // A valid primary permits removal of a stale bad backup.
    put_bytes(path + ".stat", "bad primary");
    put_bytes(path + ".stat.bak", "bad backup");
    port_card_test_reset();
    CARDFileInfo corrupt{};
    assert(CARDOpen(0, "progress", &corrupt) == CARD_RESULT_IOERROR);
    assert(read_bytes(path + ".stat") == "bad primary");
    assert(read_bytes(path + ".stat.bak") == "bad backup");
    put_bytes(path + ".stat", goodPrimary);
    info = reopen("progress");

    auto deleteInfo = create("delete-check");
    const auto deletePath = stem("delete-check");
    failures = {{Op::Remove, ".sav"}};
    complete(0, CARD_RESULT_IOERROR, [&] { return CARDDeleteAsync(0, "delete-check", done); });
    assert(failures.empty() && fs::exists(deletePath + ".sav"));
    assert(status(deleteInfo).length == 64);
    failures = {{Op::Remove, ".stat"}};
    complete(0, CARD_RESULT_IOERROR, [&] { return CARDDeleteAsync(0, "delete-check", done); });
    assert(failures.empty() && !fs::exists(deletePath + ".sav"));
    auto missingSidecar = create("delete-no-stat");
    (void)missingSidecar;
    std::remove((stem("delete-no-stat") + ".stat").c_str());
    complete(0, CARD_RESULT_READY, [&] { return CARDDeleteAsync(0, "delete-no-stat", done); });

    create("format-check", 1);
    put_bytes(root + "/cardB/keep.txt", "unrelated");
    for (const auto fault : {Op::DirectoryOpen, Op::DirectoryRead, Op::DirectoryClose, Op::Remove}) {
        create("format-" + std::to_string(sequence++), 1);
        failures = {{fault, ""}};
        const int before = CARDGetXferredBytes(1);
        complete(1, CARD_RESULT_IOERROR, [&] { return CARDFormatAsync(1, done); });
        assert(failures.empty() && CARDGetXferredBytes(1) == before && directories.empty());
        assert(read_bytes(root + "/cardB/keep.txt") == "unrelated");
    }
    const int beforeFormat = CARDGetXferredBytes(1);
    complete(1, CARD_RESULT_READY, [&] { return CARDFormatAsync(1, done); });
    assert(CARDGetXferredBytes(1) == beforeFormat + CARD_WORKAREA_SIZE);
    assert(read_bytes(root + "/cardB/keep.txt") == "unrelated");
    for (const auto& entry : fs::directory_iterator(root + "/cardB"))
        assert(entry.path().filename() == "keep.txt");

    rejected(CARD_RESULT_NOCARD, [&] { return CARDFormatAsync(8, done); });
    rejected(CARD_RESULT_NOCARD, [&] { return CARDCheckAsync(8, done); });
    assert(streams.empty() && directories.empty() && failures.empty());
    std::printf("card_vita_driver: %d completion paths plus persistence/recovery assertions passed\n", checks);
}

// Dolphin's legacy OS header defines globals; compile the real backend once.
#include "../src/platform/card_vita.cpp"
