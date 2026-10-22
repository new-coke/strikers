#include "dolphin/card.h"
#include "port/host.h"

#if defined(PORT_VITA)

#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>

#if defined(STRIKERS_CARD_TEST)
FILE* port_card_test_fopen(const char*, const char*);
size_t port_card_test_fwrite(const void*, size_t, size_t, FILE*);
int port_card_test_fflush(FILE*);
int port_card_test_fclose(FILE*);
int port_card_test_rename(const char*, const char*);
int port_card_test_remove(const char*);
#endif

namespace {

namespace card_io {
#if defined(STRIKERS_CARD_TEST)
constexpr auto open = port_card_test_fopen;
constexpr auto write = port_card_test_fwrite;
constexpr auto flush = port_card_test_fflush;
constexpr auto close = port_card_test_fclose;
constexpr auto rename = port_card_test_rename;
constexpr auto remove = port_card_test_remove;
#else
constexpr auto open = std::fopen;
constexpr auto write = std::fwrite;
constexpr auto flush = std::fflush;
constexpr auto close = std::fclose;
constexpr auto rename = sceIoRename;
constexpr auto remove = std::remove;
#endif
}

bool finish_write(FILE* f)
{
    const bool flushed = card_io::flush(f) == 0;
    const bool closed = card_io::close(f) == 0;
    return flushed && closed;
}

constexpr int kChannels = 2;
constexpr int kMaxFiles = CARD_MAX_FILE;
constexpr s32 kCardSizeMbit = 16;
constexpr s32 kCardSizeBytes = 2 * 1024 * 1024;
constexpr s32 kSectorSize = CARD_SYSTEM_BLOCK_SIZE;
constexpr u32 kStatMagic = 0x53564d43u; // "CMVS" in little-endian storage.
constexpr u32 kStatVersion = 1;

struct FileSlot {
    bool used;
    s32 chan;
    char name[CARD_FILENAME_MAX + 1];
    CARDStat stat;
    bool statValid;
};

struct PersistedStat {
    u32 magic;
    u32 version;
    CARDStat stat;
};

FileSlot s_files[kMaxFiles];
bool s_initialized;
bool s_mounted[kChannels];
s32 s_xferred[kChannels];
char s_root[256];
char s_cardDir[kChannels][288];
char s_game[4] = {'G', '4', 'Q', 'E'};
char s_maker[2] = {'0', '1'};

bool valid_chan(s32 chan)
{
    return chan >= 0 && chan < kChannels;
}

void callback(CARDCallback cb, s32 chan, s32 result)
{
    if (cb != nullptr)
        cb(chan, result);
}

s32 complete_async(CARDCallback cb, s32 chan, s32 result)
{
    // CARDAsync returns the submission result. Once accepted, only the
    // callback reports completion, even when this backend completes inline.
    callback(cb, chan, result);
    return CARD_RESULT_READY;
}

bool has_suffix(const char* name, const char* suffix)
{
    const size_t n = std::strlen(name);
    const size_t s = std::strlen(suffix);
    return n >= s && std::strcmp(name + n - s, suffix) == 0;
}

void encode_name(const char* name, char* out, size_t outSize)
{
    static const char hex[] = "0123456789abcdef";
    size_t pos = 0;
    if (outSize == 0)
        return;

    for (size_t i = 0; name != nullptr && name[i] != '\0' && i < CARD_FILENAME_MAX; ++i) {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (pos + 2 >= outSize)
            break;
        out[pos++] = hex[c >> 4];
        out[pos++] = hex[c & 0xf];
    }
    out[pos] = '\0';
}

void make_path(s32 chan, const char* name, const char* suffix, char* out, size_t outSize)
{
    char encoded[CARD_FILENAME_MAX * 2 + 1];
    encode_name(name, encoded, sizeof encoded);
    std::snprintf(out, outSize, "%s/%s%s", s_cardDir[chan], encoded, suffix);
}

void ensure_dirs()
{
    if (s_initialized)
        return;

    if (port_executable_dir(s_root, sizeof s_root) != 0)
        std::snprintf(s_root, sizeof s_root, "ux0:data/strikersVita");

    for (int chan = 0; chan < kChannels; ++chan) {
        std::snprintf(s_cardDir[chan], sizeof s_cardDir[chan], "%s/card%c", s_root, chan == 0 ? 'A' : 'B');
        sceIoMkdir(s_cardDir[chan], 0777);
    }
    s_initialized = true;
}

long file_size(const char* path)
{
    FILE* f = std::fopen(path, "rb");
    if (f == nullptr)
        return -1;
    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        return -1;
    }
    const long size = std::ftell(f);
    std::fclose(f);
    return size;
}

FileSlot* slot_from_no(s32 fileNo)
{
    if (fileNo < 0 || fileNo >= kMaxFiles || !s_files[fileNo].used)
        return nullptr;
    return &s_files[fileNo];
}

FileSlot* find_slot(s32 chan, const char* name)
{
    for (int i = 0; i < kMaxFiles; ++i) {
        if (s_files[i].used && s_files[i].chan == chan && std::strncmp(s_files[i].name, name, CARD_FILENAME_MAX) == 0)
            return &s_files[i];
    }
    return nullptr;
}

FileSlot* alloc_slot(s32 chan, const char* name, s32* outNo)
{
    if (FileSlot* existing = find_slot(chan, name)) {
        if (outNo != nullptr)
            *outNo = static_cast<s32>(existing - s_files);
        return existing;
    }

    for (int i = 0; i < kMaxFiles; ++i) {
        if (!s_files[i].used) {
            std::memset(&s_files[i], 0, sizeof s_files[i]);
            s_files[i].used = true;
            s_files[i].chan = chan;
            std::snprintf(s_files[i].name, sizeof s_files[i].name, "%.*s", CARD_FILENAME_MAX, name);
            if (outNo != nullptr)
                *outNo = i;
            return &s_files[i];
        }
    }
    return nullptr;
}

void release_slot(FileSlot* slot)
{
    if (slot != nullptr)
        std::memset(slot, 0, sizeof *slot);
}

bool read_stat(const char* path, CARDStat& stat)
{
    FILE* f = std::fopen(path, "rb");
    if (f == nullptr)
        return false;
    PersistedStat persisted{};
    const bool valid = std::fread(&persisted, 1, sizeof persisted, f) == sizeof persisted
        && persisted.magic == kStatMagic && persisted.version == kStatVersion;
    const bool closed = std::fclose(f) == 0;
    if (valid && closed)
        stat = persisted.stat;
    return valid && closed;
}

int metadata_exists(const char* path)
{
    struct stat status{};
    if (::stat(path, &status) == 0)
        return S_ISREG(status.st_mode) ? 1 : -1;
    return errno == ENOENT ? 0 : -1;
}

bool load_stat(FileSlot& slot)
{
    if (slot.statValid)
        return true;

    std::memset(&slot.stat, 0, sizeof slot.stat);
    std::snprintf(slot.stat.fileName, sizeof slot.stat.fileName, "%.*s", CARD_FILENAME_MAX - 1, slot.name);

    char path[384];
    make_path(slot.chan, slot.name, ".stat", path, sizeof path);
    if (!read_stat(path, slot.stat)) {
        char backup[400];
        std::snprintf(backup, sizeof backup, "%s.bak", path);
        if (!read_stat(backup, slot.stat)
            && (metadata_exists(path) != 0 || metadata_exists(backup) != 0))
            return false;
    }
    slot.statValid = true;
    return true;
}

bool save_stat(const FileSlot& slot)
{
    char path[384];
    make_path(slot.chan, slot.name, ".stat", path, sizeof path);
    char temp[400];
    std::snprintf(temp, sizeof temp, "%s.tmp", path);
    char backup[400];
    std::snprintf(backup, sizeof backup, "%s.bak", path);
    FILE* f = card_io::open(temp, "wb");
    if (f == nullptr)
        return false;

    PersistedStat persisted{};
    persisted.magic = kStatMagic;
    persisted.version = kStatVersion;
    persisted.stat = slot.stat;
    const bool written = card_io::write(&persisted, 1, sizeof persisted, f) == sizeof persisted;
    const bool finished = finish_write(f);
    if (!written || !finished) {
        card_io::remove(temp);
        return false;
    }

    const auto replace = [&]() {
        const int backupExists = metadata_exists(backup);
        if (backupExists < 0)
            return false;
        if (backupExists) {
            CARDStat existing{};
            if (read_stat(path, existing)) {
                if (card_io::remove(backup) != 0)
                    return false;
            } else {
                if (!read_stat(backup, existing))
                    return false;
                if (card_io::remove(path) != 0 && errno != ENOENT)
                    return false;
                if (card_io::rename(backup, path) < 0)
                    return false;
            }
        }

        const int oldExists = metadata_exists(path);
        if (oldExists < 0 || (oldExists && card_io::rename(path, backup) < 0))
            return false;
        // Vita newlib rename() removes its destination before renaming. The
        // native operation never overwrites: keep the previous version in .bak.
        if (card_io::rename(temp, path) < 0) {
            if (oldExists && card_io::rename(backup, path) < 0) {
                // The last valid metadata stays in .bak for the next load.
                return false;
            }
            return false;
        }
        return true;
    };
    if (!replace()) {
        card_io::remove(temp);
        return false;
    }
    return true;
}

s32 open_file(s32 chan, const char* fileName, CARDFileInfo* fileInfo)
{
    ensure_dirs();
    if (!valid_chan(chan) || fileName == nullptr || fileInfo == nullptr)
        return CARD_RESULT_FATAL_ERROR;
    if (std::strlen(fileName) > CARD_FILENAME_MAX)
        return CARD_RESULT_NAMETOOLONG;

    char path[384];
    make_path(chan, fileName, ".sav", path, sizeof path);
    const long size = file_size(path);
    if (size < 0)
        return CARD_RESULT_NOFILE;

    s32 fileNo = -1;
    FileSlot* slot = alloc_slot(chan, fileName, &fileNo);
    if (slot == nullptr)
        return CARD_RESULT_LIMIT;
    if (!load_stat(*slot)) {
        release_slot(slot);
        return CARD_RESULT_IOERROR;
    }
    slot->stat.length = static_cast<u32>(size);

    fileInfo->chan = chan;
    fileInfo->fileNo = fileNo;
    fileInfo->offset = 0;
    fileInfo->length = static_cast<s32>(size);
    fileInfo->iBlock = 0;
    return CARD_RESULT_READY;
}

bool remove_all_in_channel(s32 chan)
{
    SceUID dir = sceIoDopen(s_cardDir[chan]);
    if (dir < 0)
        return false;
    bool removed = true;
    int readResult;
    SceIoDirent entry{};
    while ((readResult = sceIoDread(dir, &entry)) > 0) {
        if (entry.d_name[0] != '\0' && std::strcmp(entry.d_name, ".") != 0
            && std::strcmp(entry.d_name, "..") != 0
            && (has_suffix(entry.d_name, ".sav") || has_suffix(entry.d_name, ".stat")
                || has_suffix(entry.d_name, ".stat.bak") || has_suffix(entry.d_name, ".stat.tmp"))) {
            char path[384];
            std::snprintf(path, sizeof path, "%s/%s", s_cardDir[chan], entry.d_name);
            if (sceIoRemove(path) < 0)
                removed = false;
        }
        std::memset(&entry, 0, sizeof entry);
    }
    const bool closed = sceIoDclose(dir) >= 0;
    if (!removed || readResult < 0 || !closed)
        return false;

    for (auto& slot : s_files) {
        if (slot.used && slot.chan == chan)
            release_slot(&slot);
    }
    return true;
}

struct UsageContext {
    const char* dir;
    s32 bytes;
    s32 files;
};

void count_usage(void* user, const char* name)
{
    auto* ctx = static_cast<UsageContext*>(user);
    if (!has_suffix(name, ".sav"))
        return;
    char path[384];
    std::snprintf(path, sizeof path, "%s/%s", ctx->dir, name);
    const long size = file_size(path);
    if (size >= 0) {
        ctx->bytes += static_cast<s32>(size > 0x7fffffffL ? 0x7fffffffL : size);
        ++ctx->files;
    }
}

} // namespace

#if defined(STRIKERS_CARD_TEST)
void port_card_test_reset()
{
    std::memset(s_files, 0, sizeof s_files);
    std::memset(s_mounted, 0, sizeof s_mounted);
    std::memset(s_xferred, 0, sizeof s_xferred);
    s_initialized = false;
}
#endif

extern "C" {

void CARDInit(void)
{
    ensure_dirs();
}

s32 CARDProbeEx(s32 chan, s32* memSize, s32* sectorSize)
{
    ensure_dirs();
    if (!valid_chan(chan))
        return CARD_RESULT_NOCARD;
    if (memSize != nullptr)
        *memSize = kCardSizeMbit;
    if (sectorSize != nullptr)
        *sectorSize = kSectorSize;
    return CARD_RESULT_READY;
}

s32 CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback, CARDCallback attachCallback)
{
    (void)workArea;
    (void)detachCallback;
    ensure_dirs();
    if (!valid_chan(chan))
        return CARD_RESULT_NOCARD;
    s_mounted[chan] = true;
    callback(attachCallback, chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}

s32 CARDUnmount(s32 chan)
{
    if (!valid_chan(chan))
        return CARD_RESULT_NOCARD;
    s_mounted[chan] = false;
    return CARD_RESULT_READY;
}

s32 CARDCheckAsync(s32 chan, CARDCallback cb)
{
    if (!valid_chan(chan))
        return CARD_RESULT_NOCARD;
    return complete_async(cb, chan, CARD_RESULT_READY);
}

s32 CARDGetSerialNo(s32 chan, u64* serialNo)
{
    if (!valid_chan(chan) || serialNo == nullptr)
        return CARD_RESULT_FATAL_ERROR;
    *serialNo = 0x5354524b56544100ULL | static_cast<u64>(chan); // "STRKVTA"
    return CARD_RESULT_READY;
}

s32 CARDFreeBlocks(s32 chan, s32* byteNotUsed, s32* filesNotUsed)
{
    ensure_dirs();
    if (!valid_chan(chan))
        return CARD_RESULT_NOCARD;

    UsageContext usage{s_cardDir[chan], 0, 0};
    port_scan_dir(s_cardDir[chan], count_usage, &usage);
    if (byteNotUsed != nullptr)
        *byteNotUsed = usage.bytes >= kCardSizeBytes ? 0 : kCardSizeBytes - usage.bytes;
    if (filesNotUsed != nullptr)
        *filesNotUsed = usage.files >= kMaxFiles ? 0 : kMaxFiles - usage.files;
    return CARD_RESULT_READY;
}

s32 CARDGetXferredBytes(s32 chan)
{
    return valid_chan(chan) ? s_xferred[chan] : CARD_RESULT_NOCARD;
}

s32 CARDCreateAsync(s32 chan, const char* fileName, u32 size, CARDFileInfo* fileInfo, CARDCallback cb)
{
    ensure_dirs();
    s32 result = CARD_RESULT_READY;
    if (!valid_chan(chan) || fileName == nullptr || fileInfo == nullptr) {
        return CARD_RESULT_FATAL_ERROR;
    } else if (std::strlen(fileName) > CARD_FILENAME_MAX) {
        return CARD_RESULT_NAMETOOLONG;
    } else {
        char path[384];
        make_path(chan, fileName, ".sav", path, sizeof path);
        if (file_size(path) >= 0) {
            return CARD_RESULT_EXIST;
        } else {
            FILE* f = card_io::open(path, "wbx");
            if (f == nullptr) {
                result = errno == EEXIST ? CARD_RESULT_EXIST : CARD_RESULT_IOERROR;
            } else {
                if (size != 0 && (std::fseek(f, static_cast<long>(size - 1), SEEK_SET) != 0 || std::fputc(0, f) == EOF))
                    result = CARD_RESULT_IOERROR;
                if (!finish_write(f))
                    result = CARD_RESULT_IOERROR;
                if (result == CARD_RESULT_READY) {
                    s32 fileNo = -1;
                    FileSlot* slot = alloc_slot(chan, fileName, &fileNo);
                    if (slot == nullptr) {
                        result = CARD_RESULT_LIMIT;
                    } else {
                        std::memset(&slot->stat, 0, sizeof slot->stat);
                        std::snprintf(slot->stat.fileName, sizeof slot->stat.fileName, "%.*s", CARD_FILENAME_MAX - 1, fileName);
                        std::memcpy(slot->stat.gameName, s_game, sizeof s_game);
                        std::memcpy(slot->stat.company, s_maker, sizeof s_maker);
                        slot->stat.length = size;
                        slot->statValid = true;
                        if (!save_stat(*slot)) {
                            release_slot(slot);
                            result = CARD_RESULT_IOERROR;
                        } else {
                            fileInfo->chan = chan;
                            fileInfo->fileNo = fileNo;
                            fileInfo->offset = 0;
                            fileInfo->length = static_cast<s32>(size);
                            fileInfo->iBlock = 0;
                            s_xferred[chan] += static_cast<s32>(size);
                        }
                    }
                }
                if (result != CARD_RESULT_READY)
                    sceIoRemove(path);
            }
        }
    }
    return complete_async(cb, chan, result);
}

s32 CARDOpen(s32 chan, const char* fileName, CARDFileInfo* fileInfo)
{
    return open_file(chan, fileName, fileInfo);
}

s32 CARDClose(CARDFileInfo* fileInfo)
{
    if (fileInfo == nullptr || !valid_chan(fileInfo->chan))
        return CARD_RESULT_FATAL_ERROR;
    fileInfo->offset = 0;
    return CARD_RESULT_READY;
}

s32 CARDReadAsync(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset, CARDCallback cb)
{
    s32 result = CARD_RESULT_READY;
    if (fileInfo == nullptr || buf == nullptr || length < 0 || offset < 0 || !valid_chan(fileInfo->chan)) {
        return CARD_RESULT_FATAL_ERROR;
    } else if (FileSlot* slot = slot_from_no(fileInfo->fileNo); slot != nullptr && slot->chan == fileInfo->chan) {
        char path[384];
        make_path(slot->chan, slot->name, ".sav", path, sizeof path);
        FILE* f = card_io::open(path, "rb");
        if (f == nullptr || std::fseek(f, offset, SEEK_SET) != 0
            || std::fread(buf, 1, static_cast<size_t>(length), f) != static_cast<size_t>(length)
            || std::ferror(f)) {
            result = CARD_RESULT_IOERROR;
        }
        if (f != nullptr && card_io::close(f) != 0)
            result = CARD_RESULT_IOERROR;
        if (result == CARD_RESULT_READY) {
            s_xferred[fileInfo->chan] += length;
        }
    } else {
        return CARD_RESULT_NOFILE;
    }
    return complete_async(cb, fileInfo->chan, result);
}

s32 CARDWriteAsync(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset, CARDCallback cb)
{
    s32 result = CARD_RESULT_READY;
    if (fileInfo == nullptr || buf == nullptr || length < 0 || offset < 0 || !valid_chan(fileInfo->chan)) {
        return CARD_RESULT_FATAL_ERROR;
    } else if (FileSlot* slot = slot_from_no(fileInfo->fileNo); slot != nullptr && slot->chan == fileInfo->chan) {
        char path[384];
        make_path(slot->chan, slot->name, ".sav", path, sizeof path);
        FILE* f = card_io::open(path, "r+b");
        if (f == nullptr || std::fseek(f, offset, SEEK_SET) != 0
            || card_io::write(buf, 1, static_cast<size_t>(length), f) != static_cast<size_t>(length)) {
            result = CARD_RESULT_IOERROR;
        }
        if (f != nullptr && !finish_write(f))
            result = CARD_RESULT_IOERROR;
        if (result == CARD_RESULT_READY) {
            const long size = file_size(path);
            FileSlot updated = *slot;
            if (size < 0) {
                result = CARD_RESULT_IOERROR;
            } else {
                updated.stat.length = static_cast<u32>(size);
                if (!save_stat(updated))
                    result = CARD_RESULT_IOERROR;
                else
                    *slot = updated;
            }
            if (result == CARD_RESULT_READY)
                s_xferred[fileInfo->chan] += length;
        }
    } else {
        return CARD_RESULT_NOFILE;
    }
    return complete_async(cb, fileInfo->chan, result);
}

s32 CARDGetStatus(s32 chan, s32 fileNo, CARDStat* stat)
{
    if (!valid_chan(chan) || stat == nullptr)
        return CARD_RESULT_FATAL_ERROR;
    FileSlot* slot = slot_from_no(fileNo);
    if (slot == nullptr || slot->chan != chan)
        return CARD_RESULT_NOFILE;
    if (!load_stat(*slot))
        return CARD_RESULT_IOERROR;
    char path[384];
    make_path(chan, slot->name, ".sav", path, sizeof path);
    const long size = file_size(path);
    if (size < 0)
        return CARD_RESULT_NOFILE;
    slot->stat.length = static_cast<u32>(size);
    *stat = slot->stat;
    return CARD_RESULT_READY;
}

s32 CARDSetStatus(s32 chan, s32 fileNo, CARDStat* stat)
{
    if (!valid_chan(chan) || stat == nullptr)
        return CARD_RESULT_FATAL_ERROR;
    FileSlot* slot = slot_from_no(fileNo);
    if (slot == nullptr || slot->chan != chan)
        return CARD_RESULT_NOFILE;
    FileSlot updated = *slot;
    updated.stat = *stat;
    updated.statValid = true;
    if (!save_stat(updated))
        return CARD_RESULT_IOERROR;
    *slot = updated;
    return CARD_RESULT_READY;
}

s32 CARDSetStatusAsync(s32 chan, s32 fileNo, CARDStat* stat, CARDCallback cb)
{
    if (!valid_chan(chan) || stat == nullptr)
        return CARD_RESULT_FATAL_ERROR;
    const FileSlot* slot = slot_from_no(fileNo);
    if (slot == nullptr || slot->chan != chan)
        return CARD_RESULT_NOFILE;
    const s32 result = CARDSetStatus(chan, fileNo, stat);
    return complete_async(cb, chan, result);
}

s32 CARDDeleteAsync(s32 chan, const char* fileName, CARDCallback cb)
{
    ensure_dirs();
    s32 result = CARD_RESULT_READY;
    if (!valid_chan(chan) || fileName == nullptr) {
        return CARD_RESULT_FATAL_ERROR;
    } else {
        char dataPath[384];
        char statPath[384];
        make_path(chan, fileName, ".sav", dataPath, sizeof dataPath);
        make_path(chan, fileName, ".stat", statPath, sizeof statPath);
        if (file_size(dataPath) < 0) {
            return CARD_RESULT_NOFILE;
        } else if (sceIoRemove(dataPath) < 0) {
            result = CARD_RESULT_IOERROR;
        } else {
            if (card_io::remove(statPath) != 0 && errno != ENOENT)
                result = CARD_RESULT_IOERROR;
            const char* suffixes[] = {".bak", ".tmp"};
            for (const char* suffix : suffixes) {
                char sidecar[400];
                std::snprintf(sidecar, sizeof sidecar, "%s%s", statPath, suffix);
                if (card_io::remove(sidecar) != 0 && errno != ENOENT)
                    result = CARD_RESULT_IOERROR;
            }
            if (FileSlot* slot = find_slot(chan, fileName))
                release_slot(slot);
        }
    }
    return complete_async(cb, chan, result);
}

s32 CARDFormatAsync(s32 chan, CARDCallback cb)
{
    ensure_dirs();
    if (!valid_chan(chan))
        return CARD_RESULT_NOCARD;
    s32 result = CARD_RESULT_READY;
    if (!remove_all_in_channel(chan))
        result = CARD_RESULT_IOERROR;
    else
        s_xferred[chan] += CARD_WORKAREA_SIZE;
    return complete_async(cb, chan, result);
}

} // extern "C"

#endif // PORT_VITA
