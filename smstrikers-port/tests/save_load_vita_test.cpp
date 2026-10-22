// test_save_load_vita.py inserts the shared filesystem adapter and unchanged
// SaveLoad bodies; MemCard and card_vita.cpp are compiled in full below.
#include "port/prelude.h"
#define PORT_VITA 1
// INSERT_CARD_HOST_HARNESS

#include "Game/DB/SaveLoad.h"
#include "NL/nlMemory.h"
#include <dolphin/dvd.h>
#include <set>

static std::set<void*> gameAllocations;
void* nlMalloc(size_t bytes, unsigned int alignment, bool)
{
    void* pointer = nullptr;
    if (posix_memalign(&pointer, std::max<size_t>(alignment, sizeof(void*)), bytes))
        std::abort();
    gameAllocations.insert(pointer);
    return pointer;
}
void nlFree(void* pointer)
{
    if (pointer) {
        assert(gameAllocations.erase(pointer) == 1);
        std::free(pointer);
    }
}
u32 nlStringHash(const char* value)
{
    u32 result = ~0u;
    while (*value) result = result * 33 + static_cast<unsigned char>(*value++);
    return result;
}
extern "C" void OSReport(const char*, ...) {}
extern "C" DVDDiskID* DVDGetCurrentDiskID()
{
    static DVDDiskID id{};
    return &id;
}
extern "C" void PortLogUserInfo(const char*) {}
MemCardFunctor::MCInternalFunctorBase::~MCInternalFunctorBase() = default;

static int failStatus, failReadSubmission, failReadCompletion, readCalls, failMountSubmission;
static bool failWriteSubmission;
s32 test_get_status(s32 chan, s32 fileNo, CARDStat* stat)
{
    if (failStatus && --failStatus == 0) return CARD_RESULT_IOERROR;
    return CARDGetStatus(chan, fileNo, stat);
}
s32 test_read_async(CARDFileInfo* file, void* output, s32 bytes, s32 offset, CARDCallback cb)
{
    ++readCalls;
    if (failReadSubmission == readCalls) return CARD_RESULT_IOERROR;
    if (failReadCompletion == readCalls) {
        cb(file->chan, CARD_RESULT_IOERROR);
        return CARD_RESULT_READY;
    }
    return CARDReadAsync(file, output, bytes, offset, cb);
}
s32 test_mount_async(s32 chan, void* work, CARDCallback detach, CARDCallback attach)
{
    if (failMountSubmission) return CARD_RESULT_NOCARD;
    return CARDMountAsync(chan, work, detach, attach);
}
s32 test_write_async(CARDFileInfo* file, void* input, s32 bytes, s32 offset, CARDCallback cb)
{
    if (failWriteSubmission) return CARD_RESULT_IOERROR;
    return CARDWriteAsync(file, input, bytes, offset, cb);
}
#include "src/platform/card_vita.cpp"
#define CARDGetStatus test_get_status
#define CARDReadAsync test_read_async
#define CARDMountAsync test_mount_async
#define CARDWriteAsync test_write_async
#include "src/Game/Sys/gcmemcard.cpp"
#undef CARDGetStatus
#undef CARDReadAsync
#undef CARDMountAsync
#undef CARDWriteAsync

static constexpr unsigned SaveSize = 18537;
static unsigned applications;
static std::vector<unsigned char> applied;
struct GameInfoManager
{
    unsigned long GetMemoryCardDataSize() const { return SaveSize; }
    void SetMemoryCardData(void* data)
    {
        ++applications;
        const auto* bytes = static_cast<unsigned char*>(data);
        applied.assign(bytes, bytes + SaveSize);
    }
    u8 CheckSaveIDChanged(void*) { return 0; }
};
template<class T> struct nlSingleton
{
    static T* Instance() { static T value; return &value; }
};
enum { DC_FE };
struct tDebugPrintManager { static void Print(int, const char*, ...) {} };
u32 nlChecksum32(const void* data, unsigned long count)
{
    // The original loop uses a 32-bit unsigned long on Vita.
    u32 crc = ~0u;
    const auto* bytes = static_cast<const unsigned char*>(data);
    while (count--) {
        crc ^= *bytes++;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
static void (*g_Callback)(long);
static bool InOperation;
static unsigned long gIconCRC;
static s64 mRequiredMemoryCardID;
static const char* MarioSoccerFileName = "MarioSoccer";
LoadCallbacks LoadSystem;
SaveCallbacks SaveSystem;
FileExistsCallbacks FileExistsSystem;

// INSERT_UNCHANGED_SAVE_LOAD_BODIES

static unsigned completions;
static long completedResult;
static bool nestedLoad;
static void loaded(long result)
{
    ++completions;
    completedResult = result;
    assert(!SaveLoad::CardBusy());
    assert(g_MemCards[0]->m_State == IS_IDLE);
    assert(g_MemCards[0]->m_OpenFiles.GetEntryCount() == 0);
    if (nestedLoad) {
        nestedLoad = false;
        assert(SaveLoad::StartLoad(0, loaded, true, false) == CARD_RESULT_READY);
    }
}

static void reset_operation()
{
    SaveLoad::FreeAllCallbackMemory();
    assert(g_MemCards[0]->m_OpenFiles.GetEntryCount() == 0);
    g_MemCards[0]->m_State = IS_IDLE;
    g_MemCards[0]->m_CardState = CS_IDLE;
    completions = applications = readCalls = 0;
    failStatus = failReadSubmission = failReadCompletion = failMountSubmission = 0;
    failWriteSubmission = false;
    InOperation = nestedLoad = false;
    applied.clear();
    failures.clear();
}

static void check_load(long result, bool apply = false)
{
    assert(SaveLoad::StartLoad(0, loaded, true, false) == CARD_RESULT_READY);
    assert(completions == 1 && completedResult == result);
    assert(applications == unsigned(apply));
    assert(streams.empty() && !SaveLoad::CardBusy());
    SaveLoad::FreeAllCallbackMemory();
    assert(LoadSystem.m_pReadBuffer == nullptr && LoadSystem.m_pIconReadBuffer == nullptr);
}

struct CompletionProbe
{
    unsigned count = 0;
    long result = 0;
    unsigned long callback(unsigned long slot, long value, void*)
    {
        assert(slot == 0);
        ++count;
        result = value;
        return 0;
    }
};

int main(int argc, char** argv)
{
    assert(argc == 2);
    root = argv[1];
    assert(fs::create_directory(root));
    CARDInit();
    const auto baselineAllocations = gameAllocations.size();
    CARDFileInfo file{};
    complete(0, CARD_RESULT_READY, [&] {
        return CARDCreateAsync(0, "MarioSoccer", 40960, &file, done);
    });
    auto stat = status(file);
    stat.bannerFormat = 2;
    stat.iconFormat = 2;
    stat.iconSpeed = 3;
    stat.iconAddr = 64;
    set_status(file, stat);
    const auto path = stem("MarioSoccer");
    std::string bytes(40960, '\0');
    for (unsigned i = 0; i < 8256; ++i) bytes[i] = static_cast<char>(i * 7 + 3);
    for (unsigned i = 0; i < SaveSize; ++i) bytes[16384 + 12 + i] = static_cast<char>(i * 31 + 1);
    MCFILE_HEADER header{SaveSize, nlChecksum32(bytes.data() + 16384 + 12, SaveSize),
                         nlChecksum32(bytes.data(), 8256)};
    std::memcpy(bytes.data() + 16384, &header, sizeof header);
    put_bytes(path + ".sav", bytes);
    const auto metadata = read_bytes(path + ".stat");

    // The actual bootstrap sequence probes then loads, without a write.
    assert(SaveLoad::StartFileExistsCheck(0, loaded) == CARD_RESULT_READY);
    assert(completions == 1 && completedResult == CARD_RESULT_READY && readCalls == 0);
    reset_operation();
    check_load(CARD_RESULT_READY, true);
    assert(std::memcmp(applied.data(), bytes.data() + 16384 + 12, SaveSize) == 0);
    assert(read_bytes(path + ".sav") == bytes && read_bytes(path + ".stat") == metadata);

    for (int read = 1; read <= 2; ++read) {
        reset_operation();
        failReadSubmission = read;
        check_load(CARD_RESULT_IOERROR);
        reset_operation();
        failReadCompletion = read;
        check_load(CARD_RESULT_IOERROR);
    }
    reset_operation();
    failStatus = 1;
    check_load(CARD_RESULT_IOERROR);
    assert(readCalls == 0);
    reset_operation();
    failMountSubmission = 1;
    assert(SaveLoad::StartLoad(0, loaded, true, false) == CARD_RESULT_NOCARD);
    assert(completions == 0 && applications == 0 && !SaveLoad::CardBusy());

    for (unsigned offset : {0u, 16384u, 16384u + 12}) {
        reset_operation();
        auto corrupt = bytes;
        corrupt[offset] ^= 1;
        put_bytes(path + ".sav", corrupt);
        check_load(-1000);
        assert(read_bytes(path + ".sav") == corrupt);
    }
    for (size_t length : {size_t(128), size_t(16384 + 128)}) {
        reset_operation();
        put_bytes(path + ".sav", bytes.substr(0, length));
        check_load(-1000);
        assert(readCalls == 0);
    }
    reset_operation();
    put_bytes(path + ".sav", bytes);
    std::remove((path + ".sav").c_str());
    check_load(CARD_RESULT_NOFILE);
    assert(readCalls == 0 && read_bytes(path + ".stat") == metadata);
    put_bytes(path + ".sav", bytes);

    // Metadata may advertise a larger icon region than the title allocates.
    // Reject it before the second read instead of overflowing that allocation.
    reset_operation();
    auto oversizedStat = stat;
    oversizedStat.iconSpeed = 0xffff;
    set_status(file, oversizedStat);
    std::string oversized(49152, '\0');
    std::memcpy(oversized.data() + 24576, bytes.data() + 16384, SaveSize + 12);
    put_bytes(path + ".sav", oversized);
    check_load(-1000);
    assert(readCalls == 1);
    set_status(file, stat);
    put_bytes(path + ".sav", bytes);

    // The icon-write continuation must report an immediate write rejection
    // once; a failed status query must not start that continuation at all.
    reset_operation();
    CompletionProbe probe;
    MemCardFunctor callback;
    new (callback.m_FunctorMem) MemCardFunctor::MCMemberFunctor<CompletionProbe>(
        &probe, &CompletionProbe::callback);
    MemCard* card = g_MemCards[0];
    assert(card->BeginCardAccess(callback) == CARD_RESULT_READY && probe.count == 1);
    MemCard::MC_FILE* opened = nullptr;
    assert(card->OpenFile("MarioSoccer", opened, nullptr) == CARD_RESULT_READY);
    failStatus = 1;
    assert(card->WriteFileIconData(opened, bytes.data(), callback) == CARD_RESULT_IOERROR);
    assert(probe.count == 1 && read_bytes(path + ".sav") == bytes);
    failWriteSubmission = true;
    assert(card->WriteFileIconData(opened, bytes.data(), callback) == CARD_RESULT_READY);
    assert(probe.count == 2 && probe.result == CARD_RESULT_IOERROR);
    assert(read_bytes(path + ".sav") == bytes);
    assert(card->CloseFile(opened) == CARD_RESULT_READY);
    card->m_State = IS_IDLE;
    card->m_CardState = CS_IDLE;
    CARDUnmount(0);

    // An outer completion can launch another operation without its cleanup
    // flags or return value overwriting the second operation's result.
    reset_operation();
    nestedLoad = true;
    assert(SaveLoad::StartLoad(0, loaded, true, false) == CARD_RESULT_READY);
    assert(completions == 2 && applications == 2 && readCalls == 4);
    reset_operation();
    failReadCompletion = 1;
    nestedLoad = true;
    assert(SaveLoad::StartLoad(0, loaded, true, false) == CARD_RESULT_READY);
    assert(completions == 2 && applications == 1 && readCalls == 3);
    reset_operation();
    assert(gameAllocations.size() == baselineAllocations);
    assert(read_bytes(path + ".sav") == bytes && read_bytes(path + ".stat") == metadata);
    std::puts("Vita production MemCard/SaveLoad: bootstrap, nested completion, submission errors, status, read failures, truncation and CRC passed");
}
