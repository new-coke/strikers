// The C API over one session: lock, network thread, and the game's clock, sockets and settings.

#include "port/online.h"

#include "port/config.h"
#include "port/host.h"
#include "port/lockstep.h"
#include "port/region.h"
#include "port/steamdeck.h"

#include "dolphin/pad.h"
#include "strikers_version.h"

#include "session.h"
#if defined(STRIKERS_UPNP)
#include "portmap.h"
#endif

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <direct.h>
#include <windows.h>
#include <winapifamily.h>
#if defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_APP
#define ONLINE_UWP 1
#endif
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#if defined(PORT_USE_AURORA)
#include <aurora/aurora.h>
namespace aurora
{
extern AuroraConfig g_config;
}
#endif

online::NetPipe* PortOnlineAutotestPipe(online::NetPipe* pipe);
extern "C" void PortOnlineAutotestPads(PADStatus* ports);

namespace
{
using online::NetAddr;
using online::NetPipe;

uint32_t fnv32(uint32_t h, const void* data, size_t n)
{
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < n; i++)
    {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

int make_dir(const char* path)
{
#if defined(_WIN32)
    return _mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

bool file_exists(const char* path)
{
    FILE* f = std::fopen(path, "rb");
    if (f == NULL)
        return false;
    std::fclose(f);
    return true;
}

// A name lookup on a thread of its own, so a slow resolver never holds the session.
struct ResolveJob
{
    std::mutex lock;
    bool done;
    bool ok;
    NetAddr addr;
};

class GameEnv : public online::SessionEnv
{
  public:
    GameEnv() : m_net(false)
    {
#if defined(STRIKERS_UPNP)
        m_mapPort = 0;
#endif
    }

    uint64_t NowUs() override { return port_monotonic_ns() / 1000u; }

    NetPipe* OpenPipe(int purpose, uint16_t port, int* error) override
    {
        if (!StartNet())
        {
            *error = online::PIPE_ERR_NETWORK;
            return NULL;
        }
        switch (purpose)
        {
        case online::PIPE_GAME:
            return PortOnlineAutotestPipe(online::UdpOpen(6, port, false, false, error));
        case online::PIPE_DISCOVERY: return online::UdpOpen(4, port, true, false, error);
        default:                     return online::UdpOpen(4, 0, false, true, error);
        }
    }

    void ClosePipe(NetPipe* pipe) override { online::UdpClose(pipe); }

    uint16_t PipePort(NetPipe* pipe) override { return online::UdpLocalPort(pipe); }

    void StartResolve(const char* host, uint16_t port) override
    {
        std::shared_ptr<ResolveJob> job(new ResolveJob);
        job->done = false;
        job->ok = false;
        std::memset(&job->addr, 0, sizeof job->addr);
        m_job = job;
        std::string name(host);
        std::thread([job, name, port]() {
            NetAddr a;
            const bool ok = online::NetResolve(name.c_str(), port, &a);
            std::lock_guard<std::mutex> g(job->lock);
            job->ok = ok;
            job->addr = a;
            job->done = true;
        }).detach();
    }

    int PollResolve(NetAddr* out) override
    {
        if (!m_job)
            return -1;
        std::lock_guard<std::mutex> g(m_job->lock);
        if (!m_job->done)
            return 0;
        if (!m_job->ok)
            return -1;
        *out = m_job->addr;
        return 1;
    }

    int BroadcastTargets(NetAddr* out, int max, uint16_t port) override
    {
        return online::NetBroadcastTargets(out, max, port);
    }

    bool LanAddress(char* out, size_t size) override { return online::NetLanAddress(out, size); }
    int OtherAddresses(char out[][16], int max) override
    {
        char all[ONLINE_MAX_OTHER_ADDRESSES + 1][16];
        const int n = online::NetLanAddresses(all, ONLINE_MAX_OTHER_ADDRESSES + 1);
        int k = 0;
        for (int i = 1; i < n && k < max; i++)
            std::memcpy(out[k++], all[i], 16);
        return k;
    }

#if defined(STRIKERS_UPNP)
    void StartPortMap(uint16_t port) override
    {
        char wan[16];
        if (port == m_mapPort && m_portMap.Poll(wan, sizeof wan) != online::PORTMAP_FAILED)
            return;
        // miniupnpc needs Winsock, which no pipe has started yet at launch.
        if (!StartNet())
            return;
        m_mapPort = port;
        m_portMap.Start(port);
    }
    int PollPortMap(char* wan, size_t size) override { return m_portMap.Poll(wan, size); }
    void StopPortMap() override
    {
        m_portMap.Stop();
        m_mapPort = 0;
    }
#endif

    bool Random(void* buf, size_t size) override { return online::NetRandom(buf, size); }

    bool ReportPath(char* out, size_t size) override
    {
        char dir[ONLINE_PATH_MAX];
        const char* user = std::getenv("STRIKERS_USER_DIR");
#if defined(PORT_USE_AURORA)
        if ((user == NULL || *user == '\0') && aurora::g_config.userPath != NULL)
            user = aurora::g_config.userPath;
#endif
        if (user != NULL && *user != '\0')
            std::snprintf(dir, sizeof dir, "%s", user);
        else if (port_executable_dir(dir, sizeof dir) != 0)
            return false;
        size_t n = std::strlen(dir);
        while (n > 0 && (dir[n - 1] == '/' || dir[n - 1] == '\\'))
            dir[--n] = '\0';
        char folder[ONLINE_PATH_MAX];
        std::snprintf(folder, sizeof folder, "%s/desync", dir);
        make_dir(folder);
        struct tm t;
        std::memset(&t, 0, sizeof t);
        port_localtime(std::time(NULL), &t);
        for (int i = 0; i < 100; i++)
        {
            char suffix[8] = "";
            if (i > 0)
                std::snprintf(suffix, sizeof suffix, "-%d", i + 1);
            std::snprintf(out, size, "%s/desync-%04d-%02d-%02d_%02d-%02d-%02d%s.txt", folder,
                          t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec,
                          suffix);
            if (!file_exists(out))
                return true;
        }
        return false;
    }

    void Log(const char* line) override { std::fprintf(stderr, "[online] %s\n", line); }

    bool NetStarted() const { return m_net; }

  private:
    bool StartNet()
    {
        if (!m_net)
            m_net = online::NetStartup();
        return m_net;
    }

    bool m_net;
    std::shared_ptr<ResolveJob> m_job;
#if defined(STRIKERS_UPNP)
    online::PortMap m_portMap;
    uint16_t m_mapPort;
#endif
};

int binary_family(void)
{
#if defined(__SWITCH__)
    return online::FAMILY_SWITCH;
#elif defined(ONLINE_UWP)
    return online::FAMILY_XBOX;
#elif defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
    return online::FAMILY_WINDOWS_X64;
#elif defined(__APPLE__) && defined(__aarch64__)
    return online::FAMILY_MACOS_ARM64;
#elif defined(__linux__) && defined(__x86_64__)
    return online::FAMILY_LINUX_X64;
#elif defined(__linux__) && defined(__aarch64__)
    return online::FAMILY_LINUX_ARM64;
#else
    return online::FAMILY_OTHER;
#endif
}

int this_platform(void)
{
#if defined(__SWITCH__)
    return ONLINE_PLATFORM_SWITCH;
#elif defined(ONLINE_UWP)
    return ONLINE_PLATFORM_XBOX;
#elif defined(_WIN32)
    return ONLINE_PLATFORM_WINDOWS;
#elif defined(__APPLE__)
    return ONLINE_PLATFORM_MACOS;
#elif defined(__linux__)
    return PortIsSteamDeck() ? ONLINE_PLATFORM_STEAM_DECK : ONLINE_PLATFORM_LINUX;
#else
    return ONLINE_PLATFORM_UNKNOWN;
#endif
}

online::Identity make_identity(void)
{
    online::Identity id;
    std::memset(&id, 0, sizeof id);
    id.platform = (uint8_t)this_platform();
    id.binaryFamily = (uint8_t)binary_family();
    id.dataRegion = port_region() == PORT_REGION_JAPAN ? 1 : 0;
    const char* version = STRIKERS_VERSION_STR;
    size_t n = 0;
    // The wire carries 16 bytes of printable ASCII.
    for (; version[n] != '\0' && n < 16; n++)
        id.version[n] = (version[n] >= 0x20 && version[n] < 0x7F) ? version[n] : '?';
    id.version[n] = '\0';
    const uint16_t protocol = ONLINE_PROTOCOL_VERSION;
    // Every platform plays every other: the binary family is reported, never compared, and a pair
    // that computes differently shows up as a desync.
    const uint32_t h = fnv32(2166136261u, version, std::strlen(version));
    id.build = fnv32(h, &protocol, sizeof protocol);
    // USA and Europe share a data family.
    const char* family = id.dataRegion == 1 ? "strikers data japan" : "strikers data usa-europe";
    id.dataFamily = fnv32(2166136261u, family, std::strlen(family));
    return id;
}

const int kSettingCount = 4;
const char* const kSettingKeys[kSettingCount] = { "online_nickname", "online_delay", "online_stats",
                                                  "online_port" };

bool key_is(const char* line, const char* key)
{
    while (*line == ' ' || *line == '\t')
        line++;
    size_t n = std::strlen(key);
    // The loader takes a key with or without the STRIKERS_ prefix, in any case.
    const char* prefix = "strikers_";
    size_t i = 0;
    for (; prefix[i] != '\0'; i++)
    {
        char c = line[i];
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        if (c != prefix[i])
            break;
    }
    if (prefix[i] == '\0')
        line += i;
    for (i = 0; i < n; i++)
    {
        char c = line[i];
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        if (c != key[i])
            return false;
    }
    line += n;
    while (*line == ' ' || *line == '\t')
        line++;
    return *line == '=';
}

void save_settings(const OnlineSettings& s)
{
    char path[1024];
    const char* loaded = PortConfigPath();
    if (loaded != NULL)
        std::snprintf(path, sizeof path, "%s", loaded);
    else
    {
        char dir[1024];
        if (port_executable_dir(dir, sizeof dir) != 0)
            return;
        std::snprintf(path, sizeof path, "%s/strikers.ini", dir);
    }

    std::vector<std::string> lines;
    FILE* f = std::fopen(path, "rb");
    if (f != NULL)
    {
        char buf[1024];
        while (std::fgets(buf, sizeof buf, f) != NULL)
        {
            size_t n = std::strlen(buf);
            while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
                buf[--n] = '\0';
            lines.push_back(buf);
        }
        std::fclose(f);
    }

    char values[kSettingCount][64];
    std::snprintf(values[0], sizeof values[0], "online_nickname = \"%s\"", s.nickname);
    std::snprintf(values[1], sizeof values[1], "online_delay = %u", (unsigned)s.delayTicks);
    std::snprintf(values[2], sizeof values[2], "online_stats = %u", (unsigned)s.showStats);
    std::snprintf(values[3], sizeof values[3], "online_port = %u", (unsigned)s.hostPort);
    bool placed[kSettingCount] = {};
    for (size_t i = 0; i < lines.size(); i++)
        for (int k = 0; k < kSettingCount; k++)
            if (!placed[k] && key_is(lines[i].c_str(), kSettingKeys[k]))
            {
                lines[i] = values[k];
                placed[k] = true;
            }
    size_t at = lines.size();
    for (size_t i = 0; i < lines.size(); i++)
        if (lines[i] == "[online]")
            at = i + 1;
    bool all = true;
    for (int k = 0; k < kSettingCount; k++)
        all = all && placed[k];
    if (at == lines.size() && !all)
    {
        bool section = false;
        for (size_t i = 0; i < lines.size(); i++)
            section = section || lines[i] == "[online]";
        if (!section)
        {
            if (!lines.empty() && !lines.back().empty())
                lines.push_back("");
            lines.push_back("[online]");
            at = lines.size();
        }
    }
    for (int k = kSettingCount - 1; k >= 0; k--)
        if (!placed[k])
            lines.insert(lines.begin() + (long)at, values[k]);

    char tmp[1100];
    std::snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE* out = std::fopen(tmp, "wb");
    if (out == NULL)
    {
        std::fprintf(stderr, "[online] can't save the online settings to %s\n", path);
        return;
    }
    for (size_t i = 0; i < lines.size(); i++)
        std::fprintf(out, "%s\n", lines[i].c_str());
    bool saved = std::fclose(out) == 0;
    bool removed = false;
    if (saved)
    {
#if defined(_WIN32)
        // One step; when a scanner holding the .tmp makes it fail, the old file is still whole.
        saved = MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
#if defined(__SWITCH__)
        // Horizon's rename never replaces an existing file.
        removed = std::remove(path) == 0;
#endif
        saved = std::rename(tmp, path) == 0;
#endif
    }
    if (!saved)
    {
        // Once the old file is gone, the .tmp holds the only copy.
        if (!removed)
            std::remove(tmp);
        std::fprintf(stderr, "[online] can't save the online settings to %s\n", path);
    }
}

OnlineSettings load_settings(void)
{
    OnlineSettings s;
    std::memset(&s, 0, sizeof s);
    const char* nick = std::getenv("STRIKERS_ONLINE_NICKNAME");
    if (nick == NULL || PortOnlineCleanName(s.nickname, nick) == 0)
        PortOnlineCleanName(s.nickname, "Player");
    const char* delay = std::getenv("STRIKERS_ONLINE_DELAY");
    if (delay != NULL)
    {
        const long v = std::strtol(delay, NULL, 10);
        s.delayTicks = (uint8_t)(v < 0                        ? 0
                                 : v > ONLINE_MAX_DELAY_TICKS ? ONLINE_MAX_DELAY_TICKS
                                                              : v);
    }
    const char* stats = std::getenv("STRIKERS_ONLINE_STATS");
    s.showStats = stats != NULL && std::strtol(stats, NULL, 10) != 0;
    const char* port = std::getenv("STRIKERS_ONLINE_PORT");
    const long p = port != NULL ? std::strtol(port, NULL, 10) : 0;
    s.hostPort = (uint16_t)(p >= 1025 && p <= 49151 ? p : ONLINE_DEFAULT_PORT);
    return s;
}

std::mutex s_lock;
std::condition_variable s_wake;
GameEnv* s_env;
online::Session* s_session;
// A pointer, so an exit() that skips PortOnlineShutdown runs no joinable thread's destructor.
std::thread* s_thread;
bool s_threadRunning;
bool s_stop;
OnlineState s_snapshot;
PADStatus s_frame[4];
char s_names[ONLINE_MAX_SLOTS][ONLINE_NAME_MAX + 1];

bool can_host(void)
{
#if defined(ONLINE_UWP)
    return false;
#else
    return true;
#endif
}

bool find_public(void)
{
    const char* findPublic = std::getenv("STRIKERS_ONLINE_PUBLIC");
    return can_host() && (findPublic == NULL || std::strtol(findPublic, NULL, 10) != 0);
}

void ensure_locked(void)
{
    if (s_session != NULL)
        return;
    if (s_env == NULL)
        s_env = new GameEnv;
    online::SessionConfig config;
    config.discoveryPort = ONLINE_DISCOVERY_PORT;
    config.canHost = can_host();
    config.findPublic = find_public();
    s_session = new online::Session(s_env, make_identity(), config);
    s_session->SetSettings(load_settings());
    s_session->Update(1.0f);
    s_snapshot = s_session->State();
}

void net_thread(void)
{
    std::unique_lock<std::mutex> lk(s_lock);
    while (!s_stop)
    {
        s_session->Poll();
        const uint64_t now = s_env->NowUs();
        const uint64_t wake = s_session->NextWakeUs();
        const uint64_t wait = wake > now ? wake - now : 0;
        NetPipe* pipes[4];
        const int n = s_session->Pipes(pipes, 4);
        if (n == 0)
        {
            s_wake.wait_for(lk, std::chrono::milliseconds(100));
            continue;
        }
        if (wait == 0)
        {
            // The mutex is not fair: without the yield this thread takes the lock straight back.
            lk.unlock();
            std::this_thread::yield();
            lk.lock();
            continue;
        }
        lk.unlock();
        online::UdpWaitAny(pipes, n, wait);
        lk.lock();
    }
}

// After a command: send what it queued, wake the thread, and show the result at once.
void after_command_locked(void)
{
    s_session->Flush();
    NetPipe* pipes[4];
    if (!s_threadRunning && s_session->Pipes(pipes, 4) > 0)
    {
        s_stop = false;
        s_threadRunning = true;
        s_thread = new std::thread(net_thread);
    }
    s_wake.notify_all();
    s_session->Update(0.0f);
    s_snapshot = s_session->State();
}

void to_local(const PADStatus* ports, online::LocalPad out[4])
{
    for (int i = 0; i < 4; i++)
    {
        out[i].pad.buttons = ports[i].button;
        out[i].pad.stickX = ports[i].stickX;
        out[i].pad.stickY = ports[i].stickY;
        out[i].pad.substickX = ports[i].substickX;
        out[i].pad.substickY = ports[i].substickY;
        out[i].pad.triggerL = ports[i].triggerLeft;
        out[i].pad.triggerR = ports[i].triggerRight;
        out[i].connected = ports[i].err != PAD_ERR_NO_CONTROLLER;
    }
}

void to_pad(const online::SlotInput& in, PADStatus* out)
{
    std::memset(out, 0, sizeof *out);
    switch (in.kind)
    {
    case ONLINE_SLOT_EMPTY: out->err = PAD_ERR_NO_CONTROLLER; return;
    case ONLINE_SLOT_CPU:   return;
    default:                break;
    }
    out->button = in.pad.buttons;
    out->stickX = in.pad.stickX;
    out->stickY = in.pad.stickY;
    out->substickX = in.pad.substickX;
    out->substickY = in.pad.substickY;
    out->triggerLeft = in.pad.triggerL;
    out->triggerRight = in.pad.triggerR;
    out->err = in.connected ? 0 : PAD_ERR_NO_CONTROLLER;
}
}   // namespace

extern "C" {

const OnlineState* PortOnlineGetState(void)
{
    std::lock_guard<std::mutex> g(s_lock);
    ensure_locked();
    return &s_snapshot;
}

int PortOnlineHost(uint16_t port, const char* nickname)
{
    std::lock_guard<std::mutex> g(s_lock);
    ensure_locked();
    const int r = s_session->Host(port, nickname);
    after_command_locked();
    return r;
}

int PortOnlineJoin(const char* address, const char* nickname)
{
    std::lock_guard<std::mutex> g(s_lock);
    ensure_locked();
    const int r = s_session->Join(address, nickname);
    after_command_locked();
    return r;
}

int PortOnlineRefreshNearby(void)
{
    std::lock_guard<std::mutex> g(s_lock);
    ensure_locked();
    const int r = s_session->RefreshNearby();
    after_command_locked();
    return r;
}

void PortOnlineLeave(void)
{
    std::lock_guard<std::mutex> g(s_lock);
    ensure_locked();
    s_session->Leave();
    after_command_locked();
}

int PortOnlineAddLocalPlayer(int localPort, const char* name)
{
    std::lock_guard<std::mutex> g(s_lock);
    ensure_locked();
    const int r = s_session->AddLocalPlayer(localPort, name);
    after_command_locked();
    return r;
}

int PortOnlineRemoveLocalPlayer(int localPort)
{
    std::lock_guard<std::mutex> g(s_lock);
    ensure_locked();
    const int r = s_session->RemoveLocalPlayer(localPort);
    after_command_locked();
    return r;
}

int PortOnlineSetDesc(const OnlineMatchDesc* desc)
{
    if (desc == NULL)
        return 0;
    std::lock_guard<std::mutex> g(s_lock);
    ensure_locked();
    const int r = s_session->SetDesc(*desc);
    after_command_locked();
    return r;
}

int PortOnlinePresentationSkip(unsigned int presentation, int pressed)
{
    std::lock_guard<std::mutex> g(s_lock);
    if (!s_session)
        return 0;
    const int result = s_session->PresentationSkip(presentation, pressed != 0);
    if (pressed)
        after_command_locked();
    return result;
}

int PortOnlineSelect(int op, int side, int team, int sidekick, int phase)
{
    std::lock_guard<std::mutex> g(s_lock);
    ensure_locked();
    const int result = s_session->Select(op, side, team, sidekick, phase);
    after_command_locked();
    return result;
}

int PortOnlineSetReady(int ready)
{
    std::lock_guard<std::mutex> g(s_lock);
    ensure_locked();
    const int r = s_session->SetReady(ready);
    after_command_locked();
    return r;
}

int PortOnlineStart(void)
{
    std::lock_guard<std::mutex> g(s_lock);
    ensure_locked();
    const int r = s_session->Start();
    after_command_locked();
    return r;
}

int PortOnlineVote(int slot, int vote)
{
    std::lock_guard<std::mutex> g(s_lock);
    ensure_locked();
    const int r = s_session->Vote(slot, vote);
    after_command_locked();
    return r;
}

int PortOnlineAfterOpponentsLeft(int continueAgainstCpu)
{
    std::lock_guard<std::mutex> g(s_lock);
    ensure_locked();
    const int r = s_session->AfterOpponentsLeft(continueAgainstCpu);
    after_command_locked();
    return r;
}

int PortOnlineSetSettings(const OnlineSettings* settings)
{
    if (settings == NULL)
        return 0;
    OnlineSettings saved;
    {
        std::lock_guard<std::mutex> g(s_lock);
        ensure_locked();
        s_session->SetSettings(*settings);
        saved = s_session->Settings();
        s_session->Update(0.0f);
        s_snapshot = s_session->State();
    }
    save_settings(saved);
    return 1;
}

void PortOnlineStartPortMap(void)
{
    std::lock_guard<std::mutex> g(s_lock);
    if (s_session != NULL || !find_public())
        return;
    if (s_env == NULL)
        s_env = new GameEnv;
    s_env->StartPortMap(load_settings().hostPort);
}

void PortOnlineInit(void)
{
    std::lock_guard<std::mutex> g(s_lock);
    ensure_locked();
}

void PortOnlineShutdown(void)
{
    {
        std::lock_guard<std::mutex> g(s_lock);
        if (s_session == NULL)
        {
            // Quitting before the session exists still removes the launch's mapping.
            const bool net = s_env != NULL && s_env->NetStarted();
            delete s_env;
            s_env = NULL;
            if (net)
                online::NetShutdown();
            return;
        }
        s_stop = true;
        s_wake.notify_all();
    }
    if (s_threadRunning)
    {
        s_thread->join();
        delete s_thread;
        s_thread = NULL;
        s_threadRunning = false;
    }
    std::lock_guard<std::mutex> g(s_lock);
    uint32_t serial;
    const std::string report = s_session->TakeReport(serial);
    if (!report.empty())
        s_session->SaveReport(report);
    s_session->Shutdown();
    delete s_session;
    s_session = NULL;
    const bool net = s_env->NetStarted();
    delete s_env;
    s_env = NULL;
    if (net)
        online::NetShutdown();
}

void PortOnlineUpdate(float timeScale)
{
    std::unique_lock<std::mutex> g(s_lock);
    ensure_locked();
    s_session->Update(timeScale);
    uint32_t serial;
    const std::string report = s_session->TakeReport(serial);
    if (!report.empty())
    {
        online::Session* session = s_session;
        g.unlock();
        const std::string path = session->SaveReport(report);
        g.lock();
        if (s_session == session)
            session->ReportSaved(path, serial);
    }
    s_snapshot = s_session->State();
}

int PortOnlineActive(void)
{
    std::lock_guard<std::mutex> g(s_lock);
    return s_session != NULL && s_session->Active();
}

int PortOnlineInMatch(void)
{
    std::lock_guard<std::mutex> g(s_lock);
    return s_session != NULL && s_session->InMatch();
}

void PortOnlineSampleLocalPads(const struct PADStatus* ports)
{
    if (ports == NULL)
        return;
    std::lock_guard<std::mutex> g(s_lock);
    ensure_locked();
    std::memcpy(s_frame, ports, sizeof s_frame);
    PortOnlineAutotestPads(s_frame);
    online::LocalPad local[4];
    to_local(s_frame, local);
    s_session->SampleLocalPads(local);
}

int PortOnlineRenderPads(struct PADStatus* pads)
{
    std::lock_guard<std::mutex> g(s_lock);
    if (s_session == NULL || !s_session->Active())
        return 0;
    online::SlotInput in[ONLINE_MAX_SLOTS];
    int port[ONLINE_MAX_SLOTS];
    if (!s_session->RenderSlots(in, port))
        return 0;
    for (int s = 0; s < ONLINE_MAX_SLOTS; s++)
    {
        if (in[s].kind == ONLINE_SLOT_LOCAL && port[s] >= 0)
            pads[s] = s_frame[port[s]];
        else
            to_pad(in[s], &pads[s]);
    }
    return 1;
}

int PortOnlineSlotKind(int slot)
{
    std::lock_guard<std::mutex> g(s_lock);
    if (s_session == NULL)
        return slot >= 0 && slot < ONLINE_MAX_SLOTS ? ONLINE_SLOT_LOCAL : ONLINE_SLOT_EMPTY;
    return s_session->SlotKind(slot);
}

int PortOnlineSlotLocalPort(int slot)
{
    std::lock_guard<std::mutex> g(s_lock);
    if (s_session == NULL)
        return slot >= 0 && slot < ONLINE_MAX_SLOTS ? slot : -1;
    return s_session->SlotLocalPort(slot);
}

const char* PortOnlineSlotName(int slot)
{
    if (slot < 0 || slot >= ONLINE_MAX_SLOTS)
        return "";
    std::lock_guard<std::mutex> g(s_lock);
    if (s_session == NULL)
        return "";
    std::snprintf(s_names[slot], sizeof s_names[slot], "%s", s_session->SlotName(slot));
    return s_names[slot];
}

void PortOnlineLoadProgress(int percent)
{
    std::lock_guard<std::mutex> g(s_lock);
    if (s_session == NULL)
        return;
    s_session->LoadProgress(percent);
    s_session->Flush();
}

void PortOnlineMatchLoaded(void)
{
    std::lock_guard<std::mutex> g(s_lock);
    if (s_session == NULL)
        return;
    s_session->MatchLoaded();
    s_session->Flush();
}

int PortOnlineMayBeginMatch(void)
{
    std::lock_guard<std::mutex> g(s_lock);
    return s_session != NULL && s_session->MayBeginMatch();
}

void PortOnlineMatchEnded(void)
{
    std::lock_guard<std::mutex> g(s_lock);
    if (s_session == NULL)
        return;
    s_session->MatchEnded();
    s_session->Flush();
}

int PortOnlineTickInput(unsigned int tick, struct PADStatus* pads)
{
    std::unique_lock<std::mutex> g(s_lock);
    if (s_session == NULL || !s_session->Engine().Active())
        return 0;
    online::SlotInput in[ONLINE_MAX_SLOTS];
    if (!s_session->TickInput(tick, in))
        return 0;
    for (int s = 0; s < ONLINE_MAX_SLOTS; s++)
        to_pad(in[s], &pads[s]);
    return 1;
}

unsigned int PortOnlineRollbackTo(void)
{
    std::lock_guard<std::mutex> g(s_lock);
    return s_session != NULL ? s_session->RollbackTo() : online::kNoTick;
}

void PortOnlineRewind(unsigned int tick)
{
    std::lock_guard<std::mutex> g(s_lock);
    if (s_session != NULL)
        s_session->Rewind(tick);
}

unsigned int PortOnlineConfirmedTick(void)
{
    std::lock_guard<std::mutex> g(s_lock);
    return s_session != NULL ? s_session->ConfirmedTick() : online::kNoTick;
}

unsigned int PortOnlineSettledTick(void)
{
    std::lock_guard<std::mutex> g(s_lock);
    return s_session != NULL ? s_session->SettledTick() : online::kNoTick;
}

void PortOnlineTickHash(unsigned int tick, uint64_t hash)
{
    std::lock_guard<std::mutex> g(s_lock);
    if (s_session != NULL)
        s_session->TickHash(tick, hash);
}

void PortOnlineTickComponents(unsigned int tick, const uint64_t* hashes, const char* const* names,
                              int count)
{
    std::lock_guard<std::mutex> g(s_lock);
    if (s_session != NULL && hashes != NULL)
        s_session->TickComponents(tick, hashes, names, count);
}

int PortOnlineTickActions(unsigned int tick, OnlineTickActions* out)
{
    std::lock_guard<std::mutex> g(s_lock);
    if (s_session == NULL)
    {
        std::memset(out, 0, sizeof *out);
        return 0;
    }
    return s_session->TickActions(tick, out);
}

void PortOnlineBarrierReached(int kind, unsigned int tick)
{
    std::lock_guard<std::mutex> g(s_lock);
    if (s_session == NULL)
        return;
    s_session->BarrierReached(kind, tick);
    s_session->Flush();
}

void PortOnlineBarrierDone(int kind)
{
    std::lock_guard<std::mutex> g(s_lock);
    if (s_session == NULL)
        return;
    s_session->BarrierDone(kind);
    s_session->Flush();
}

int PortOnlineMayRunTick(unsigned int tick)
{
    std::lock_guard<std::mutex> g(s_lock);
    return s_session == NULL || s_session->MayRunTick(tick);
}

float PortOnlineClockRate(void)
{
    std::lock_guard<std::mutex> g(s_lock);
    return s_session == NULL ? 1.0f : s_session->ClockRate();
}

}   // extern "C"
