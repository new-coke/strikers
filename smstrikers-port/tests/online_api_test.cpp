// The C API and its network thread over UDP on 127.0.0.1, hosting and joining.

#include "port/online.h"

#include "dolphin/pad.h"
#include "strikers_version.h"

#include "online_fake_game.h"

#include "../src/platform/online/pipe.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <thread>

#include <sys/stat.h>
#include <unistd.h>

using namespace online;
using namespace online_test;

namespace
{
int g_failures;

#define CHECK(cond)                                                     \
    do                                                                  \
    {                                                                   \
        if (!(cond))                                                    \
        {                                                               \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                               \
        }                                                               \
    } while (0)

const std::chrono::steady_clock::time_point g_start = std::chrono::steady_clock::now();
std::string g_ini =
    std::string(ONLINE_TEST_OUT) + "/api-strikers-" + std::to_string(getpid()) + ".ini";

uint64_t NowUs()
{
    return 1000000 + (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::steady_clock::now() - g_start)
                         .count();
}
}   // namespace

// The game's side of what the API calls.
extern "C" {
unsigned long long port_monotonic_ns(void) { return (unsigned long long)NowUs() * 1000ull; }
int port_executable_dir(char* buf, size_t size)
{
    std::snprintf(buf, size, "%s", ONLINE_TEST_OUT);
    return 0;
}
int port_localtime(time_t when, struct tm* out) { return localtime_r(&when, out) != NULL ? 0 : -1; }
const char* PortConfigPath(void) { return g_ini.c_str(); }
int port_region(void) { return 0; }
int PortIsSteamDeck(void) { return 0; }
int PortSimLocalInput(unsigned int tick, struct PADStatus* pads)
{
    (void)tick;
    std::memset(pads, 0, 4 * sizeof(PADStatus));
    return 1;
}
void PortOnlineAutotestPads(PADStatus*) {}
}
online::NetPipe* PortOnlineAutotestPipe(online::NetPipe* pipe) { return pipe; }

namespace
{
class ApiSide : public Side
{
  public:
    void Update() override { PortOnlineUpdate(1.0f); }
    void SampleLocalPads(const LocalPad ports[4]) override
    {
        PADStatus p[4];
        std::memset(p, 0, sizeof p);
        for (int i = 0; i < 4; i++)
        {
            p[i].button = ports[i].pad.buttons;
            p[i].stickX = ports[i].pad.stickX;
            p[i].stickY = ports[i].pad.stickY;
            p[i].substickX = ports[i].pad.substickX;
            p[i].substickY = ports[i].pad.substickY;
            p[i].triggerLeft = ports[i].pad.triggerL;
            p[i].triggerRight = ports[i].pad.triggerR;
            p[i].err = ports[i].connected ? 0 : PAD_ERR_NO_CONTROLLER;
        }
        PortOnlineSampleLocalPads(p);
    }
    const OnlineState& State() override { return *PortOnlineGetState(); }
    void LoadProgress(int percent) override { PortOnlineLoadProgress(percent); }
    void MatchLoaded() override { PortOnlineMatchLoaded(); }
    int MayBeginMatch() override { return PortOnlineMayBeginMatch(); }
    float ClockRate() override { return PortOnlineClockRate(); }
    int TickInput(uint32_t tick, SlotInput out[ONLINE_MAX_SLOTS]) override
    {
        PADStatus pads[4];
        if (!PortOnlineTickInput(tick, pads))
            return 0;
        for (int s = 0; s < ONLINE_MAX_SLOTS; s++)
        {
            out[s].kind = (uint8_t)PortOnlineSlotKind(s);
            out[s].connected = out[s].kind != ONLINE_SLOT_EMPTY && pads[s].err == 0;
            out[s].pad.buttons = pads[s].button;
            out[s].pad.stickX = pads[s].stickX;
            out[s].pad.stickY = pads[s].stickY;
            out[s].pad.substickX = pads[s].substickX;
            out[s].pad.substickY = pads[s].substickY;
            out[s].pad.triggerL = pads[s].triggerLeft;
            out[s].pad.triggerR = pads[s].triggerRight;
        }
        return 1;
    }
    uint32_t RollbackTo() override { return PortOnlineRollbackTo(); }
    uint32_t SettledTick() override { return PortOnlineSettledTick(); }
    void Rewind(uint32_t tick) override { PortOnlineRewind(tick); }
    uint32_t ConfirmedTick() override { return PortOnlineConfirmedTick(); }
    void BarrierReached(int kind, uint32_t tick) override { PortOnlineBarrierReached(kind, tick); }
    void BarrierDone(int kind) override { PortOnlineBarrierDone(kind); }
    int MayRunTick(uint32_t tick) override { return PortOnlineMayRunTick(tick); }
    int TickActions(uint32_t tick, OnlineTickActions* out) override
    {
        return PortOnlineTickActions(tick, out);
    }
    void TickHash(uint32_t tick, uint64_t hash) override { PortOnlineTickHash(tick, hash); }
    void TickComponents(uint32_t tick, const uint64_t* hashes, const char* const* names,
                        int count) override
    {
        PortOnlineTickComponents(tick, hashes, names, count);
    }
    void MatchEnded() override { PortOnlineMatchEnded(); }
    void Flush() override {}
};

class UdpEnv : public SessionEnv
{
  public:
    uint64_t NowUs() override { return ::NowUs(); }
    NetPipe* OpenPipe(int purpose, uint16_t port, int* error) override
    {
        if (purpose == PIPE_GAME)
            return UdpOpen(6, port, false, false, error);
        if (purpose == PIPE_DISCOVERY)
            return UdpOpen(4, port, true, false, error);
        return UdpOpen(4, 0, false, true, error);
    }
    void ClosePipe(NetPipe* pipe) override { UdpClose(pipe); }
    uint16_t PipePort(NetPipe* pipe) override { return UdpLocalPort(pipe); }
    void StartResolve(const char* host, uint16_t port) override
    {
        m_ok = NetResolve(host, port, &m_addr);
    }
    int PollResolve(NetAddr* out) override
    {
        if (!m_ok)
            return -1;
        *out = m_addr;
        return 1;
    }
    int BroadcastTargets(NetAddr* out, int max, uint16_t port) override
    {
        if (max < 1)
            return 0;
        AddrParseNumeric("127.0.0.1", port, &out[0]);
        return 1;
    }
    bool LanAddress(char* out, size_t size) override { return NetLanAddress(out, size); }
    bool Random(void* buf, size_t size) override { return NetRandom(buf, size); }
    bool ReportPath(char* out, size_t size) override
    {
        std::snprintf(out, size, "%s/api-desync.txt", ONLINE_TEST_OUT);
        return true;
    }
    void Log(const char* line) override { std::fprintf(stderr, "[raw] %s\n", line); }

  private:
    bool m_ok = false;
    NetAddr m_addr;
};

std::string ReadFile(const std::string& path)
{
    std::string text;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == NULL)
        return text;
    char buf[512];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        text.append(buf, n);
    std::fclose(f);
    return text;
}

int Count(const std::string& text, const char* what)
{
    int n = 0;
    for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
        n++;
    return n;
}

void TestSettings()
{
    mkdir(ONLINE_TEST_OUT, 0755);
    FILE* f = std::fopen(g_ini.c_str(), "wb");
    std::fprintf(f, "# a comment\n[port]\nfullscreen = 1\n\n[online]\nONLINE_DELAY = 3\n");
    std::fclose(f);
    setenv("STRIKERS_ONLINE_NICKNAME", "  Apitest ", 1);
    setenv("STRIKERS_ONLINE_DELAY", "3", 1);
    // A test never asks the internet or the router for the room's address.
    setenv("STRIKERS_ONLINE_PUBLIC", "0", 1);
    PortOnlineInit();
    const OnlineState* s = PortOnlineGetState();
    CHECK(std::strcmp(s->settings.nickname, "Apitest") == 0);
    CHECK(s->settings.delayTicks == 3 && s->settings.hostPort == ONLINE_DEFAULT_PORT);
    CHECK(s->phase == ONLINE_PHASE_IDLE);

    OnlineSettings next = s->settings;
    std::snprintf(next.nickname, sizeof next.nickname, "  Zed ");
    next.delayTicks = 12;
    next.hostPort = 80;
    next.showStats = 5;
    CHECK(PortOnlineSetSettings(&next));
    s = PortOnlineGetState();
    CHECK(std::strcmp(s->settings.nickname, "Zed") == 0);
    CHECK(s->settings.delayTicks == 8 && s->settings.hostPort == ONLINE_DEFAULT_PORT &&
          s->settings.showStats == 1);
    next = s->settings;
    CHECK(PortOnlineSetSettings(&next));
    s = PortOnlineGetState();
    next = s->settings;
    next.delayTicks = 0;
    CHECK(PortOnlineSetSettings(&next));
    const std::string text = ReadFile(g_ini);
    CHECK(Count(text, "online_nickname = \"Zed\"") == 1);
    CHECK(Count(text, "online_delay = 0") == 1);
    CHECK(Count(text, "ONLINE_DELAY") == 0);
    CHECK(Count(text, "online_port = 41210") == 1);
    CHECK(Count(text, "fullscreen = 1") == 1 && Count(text, "# a comment") == 1);
    CHECK(Count(text, "[online]") == 1);
    // Each save replaced the file it found, and nothing was left beside it.
    struct stat st;
    CHECK(stat((g_ini + ".tmp").c_str(), &st) != 0);
}

void TestOffline()
{
    for (int s = 0; s < 4; s++)
    {
        CHECK(PortOnlineSlotKind(s) == ONLINE_SLOT_LOCAL);
        CHECK(PortOnlineSlotLocalPort(s) == s);
    }
    CHECK(PortOnlineSlotKind(4) == ONLINE_SLOT_EMPTY);
    CHECK(PortOnlineActive() == 0 && PortOnlineInMatch() == 0);
    CHECK(PortOnlineMayRunTick(10) == 1);
    CHECK(PortOnlineClockRate() == 1.0f);
    PADStatus pads[4];
    CHECK(PortOnlineTickInput(0, pads) == 0);
    CHECK(PortOnlineRenderPads(pads) == 0);
    OnlineTickActions a;
    CHECK(PortOnlineTickActions(0, &a) == 0);
    CHECK(PortOnlineStart() == 0 && PortOnlineSetReady(1) == 0);
    CHECK(!PortOnlineJoin("no such address!", NULL));
    CHECK(PortOnlineGetState()->error == ONLINE_ERR_BAD_ADDRESS);
    PortOnlineLeave();
    CHECK(PortOnlineGetState()->phase == ONLINE_PHASE_IDLE &&
          PortOnlineGetState()->error == ONLINE_ERR_NONE);
}

// This machine's addresses for the room's address screen: best first, none loopback or link-local.
void TestLanAddresses()
{
    char all[8][16];
    const int n = NetLanAddresses(all, 8);
    char best[64] = "";
    const bool have = NetLanAddress(best, sizeof best);
    std::printf("lan addresses: %d, best %s\n", n, have ? best : "-");
    CHECK(have == (n > 0));
    if (n > 0)
        CHECK(std::strcmp(all[0], best) == 0);
    for (int i = 0; i < n; i++)
    {
        NetAddr a;
        CHECK(AddrParseNumeric(all[i], 1, &a) && a.family == 4);
        CHECK(a.ip[0] != 127 && !(a.ip[0] == 169 && a.ip[1] == 254) && a.ip[0] != 0);
        for (int j = 0; j < i; j++)
            CHECK(std::strcmp(all[i], all[j]) != 0);
    }
    char one[1][16];
    CHECK(NetLanAddresses(one, 1) == (n > 0 ? 1 : 0));
}

bool Loop(FakeGame& api, FakeGame& raw, Session& r, double seconds,
          const std::function<bool()>& done, const std::function<void()>& script)
{
    const uint64_t frameUs = 16667;
    uint64_t last = NowUs();
    uint64_t nextFrame = last + frameUs;
    const uint64_t end = last + (uint64_t)(seconds * 1e6);
    while (NowUs() < end)
    {
        r.Poll();
        const uint64_t now = NowUs();
        if (now >= nextFrame)
        {
            api.Frame(now, now - last);
            raw.Frame(now, now - last);
            last = now;
            while (nextFrame <= now)
                nextFrame += frameUs;
            script();
            if (done())
                return true;
        }
        NetPipe* pipes[4];
        const int n = r.Pipes(pipes, 4);
        uint64_t wake = r.NextWakeUs();
        if (nextFrame < wake)
            wake = nextFrame;
        const uint64_t t = NowUs();
        if (wake > t)
            UdpWaitAny(pipes, n, wake - t);
    }
    return done();
}
}   // namespace

int main()
{
    NetStartup();
    TestLanAddresses();
    TestSettings();
    TestOffline();

    uint32_t seed = 0;
    NetRandom(&seed, sizeof seed);

    uint16_t port = 0;
    for (int attempt = 0; attempt < 8 && port == 0; attempt++)
    {
        const uint16_t p = (uint16_t)(20000 + (seed + 7919u * (uint32_t)attempt) % 20000);
        if (PortOnlineHost(p, "ApiHost"))
            port = p;
        else
            PortOnlineLeave();
    }
    CHECK(port != 0);
    CHECK(PortOnlineGetState()->phase == ONLINE_PHASE_HOSTING && PortOnlineActive());
    CHECK(PortOnlineGetState()->roomPort == port);
    // An API room's identity comes from its DESC; the raw session takes the same values.
    Identity id;
    std::memset(&id, 0, sizeof id);
    {
        const OnlineMatchDesc& d = PortOnlineGetState()->desc;
        id.build = d.build;
        id.dataFamily = d.dataFamily;
    }
#if defined(__APPLE__)
    id.platform = ONLINE_PLATFORM_MACOS;
    id.binaryFamily = FAMILY_MACOS_ARM64;
#elif defined(__aarch64__)
    id.platform = ONLINE_PLATFORM_LINUX;
    id.binaryFamily = FAMILY_LINUX_ARM64;
#else
    id.platform = ONLINE_PLATFORM_LINUX;
    id.binaryFamily = FAMILY_LINUX_X64;
#endif
    {
        char version[17] = {};
        std::snprintf(version, sizeof version, "%s", STRIKERS_VERSION_STR);
        std::memcpy(id.version, version, sizeof id.version);
    }

    UdpEnv env;
    SessionConfig config = { (uint16_t)(port + 7), true, false };
    {
        Session r(&env, id, config);
        SessionSide rs(&r);
        ApiSide as;
        FakeGame api(&as, seed, 1);
        FakeGame raw(&rs, seed + 1, 1);
        api.matchTicks = 300;
        raw.matchTicks = 300;
        char address[64];
        std::snprintf(address, sizeof address, "127.0.0.1:%u", (unsigned)port);
        CHECK(r.Join(address, "Raw"));
        bool readied = false;
        bool started = false;
        const bool played = Loop(
            api, raw, r, 40, [&] { return !api.results.empty() && !raw.results.empty(); },
            [&] {
                if (!readied && r.State().phase == ONLINE_PHASE_ROOM)
                    readied = r.SetReady(1) != 0;
                if (!started && PortOnlineGetState()->canStart)
                    started = PortOnlineStart() != 0;
            });
        CHECK(played);
        if (played)
        {
            std::printf("api host: final states %016llx and %016llx\n",
                        (unsigned long long)api.results[0].state,
                        (unsigned long long)raw.results[0].state);
            CHECK(api.results[0].state == raw.results[0].state);
        }
        CHECK(PortOnlineSlotKind(0) == ONLINE_SLOT_LOCAL &&
              PortOnlineSlotKind(1) == ONLINE_SLOT_REMOTE);
        CHECK(std::strcmp(PortOnlineSlotName(1), "Raw") == 0);
        PortOnlineLeave();
        CHECK(!PortOnlineActive());
        Loop(api, raw, r, 3, [&] { return r.State().phase == ONLINE_PHASE_ERROR; }, [] {});
        CHECK(r.State().error == ONLINE_ERR_HOST_LEFT);
    }

    {
        Session r(&env, id, config);
        SessionSide rs(&r);
        ApiSide as;
        FakeGame api(&as, seed + 2, 1);
        FakeGame raw(&rs, seed + 3, 1);
        api.matchTicks = 300;
        raw.matchTicks = 300;
        uint16_t hostPort = (uint16_t)(port + 2);
        CHECK(r.Host(hostPort, "RawHost"));
        char address[64];
        std::snprintf(address, sizeof address, "localhost:%u", (unsigned)hostPort);
        CHECK(PortOnlineJoin(address, "ApiGuest"));
        CHECK(PortOnlineGetState()->phase == ONLINE_PHASE_JOINING);
        bool readied = false;
        bool started = false;
        const bool played = Loop(
            api, raw, r, 40, [&] { return !api.results.empty() && !raw.results.empty(); },
            [&] {
                if (!readied && PortOnlineGetState()->phase == ONLINE_PHASE_ROOM)
                    readied = PortOnlineSetReady(1) != 0;
                if (!started && r.State().canStart)
                    started = r.Start() != 0;
            });
        CHECK(played);
        if (played)
        {
            std::printf("api guest: final states %016llx and %016llx\n",
                        (unsigned long long)api.results[0].state,
                        (unsigned long long)raw.results[0].state);
            CHECK(api.results[0].state == raw.results[0].state);
        }
        CHECK(PortOnlineGetState()->self == 1 &&
              PortOnlineGetState()->roomKind == ONLINE_ROOM_DIRECT);
        PortOnlineLeave();
        Loop(api, raw, r, 3, [&] { return r.State().phase == ONLINE_PHASE_HOSTING; }, [] {});
        CHECK(r.State().phase == ONLINE_PHASE_HOSTING);
    }

    PortOnlineShutdown();
    NetShutdown();
    if (g_failures != 0)
    {
        std::printf("online_api: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("online_api: ok\n");
    return 0;
}
