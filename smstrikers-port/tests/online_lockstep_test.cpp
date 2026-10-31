// Sessions on a simulated network: long runs, a desync, silence, refusals, a hostile guest.

#include "online_fake_game.h"
#include "online_netsim.h"

#include "../src/platform/online/stun.h"

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

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

Identity TestIdentity()
{
    Identity id;
    std::memset(&id, 0, sizeof id);
    id.build = 0x1234ABCD;
    id.dataFamily = 0x0D474F55;
    id.platform = ONLINE_PLATFORM_MACOS;
    id.binaryFamily = FAMILY_MACOS_ARM64;
    id.dataRegion = 0;
    std::snprintf(id.version, sizeof id.version, "1.6.0");
    return id;
}

std::string OutDir(const char* name)
{
    std::string base = ONLINE_TEST_OUT;
    MakeDir(base);
    return base + "/" + name;
}

struct Machine
{
    Machine(SimNet* net, uint32_t ip, uint64_t seed, double drift, double hz, int humans,
            const Identity& id, const std::string& dir)
        : env(net, ip, seed, drift, 1000000000ull * (ip & 255), dir), session(&env, id, Config()),
          side(&session), game(&side, (uint32_t)seed, humans), ip(ip),
          frameUs((uint64_t)(1e6 / hz)), paused(false), framesPaused(false)
    {
        lastFrame = env.NowUs();
        nextFrame = lastFrame + frameUs;
        nextWake = session.NextWakeUs();
        OnlineSettings s = session.Settings();
        session.SetSettings(s);
    }

    static SessionConfig Config()
    {
        SessionConfig c;
        c.discoveryPort = ONLINE_DISCOVERY_PORT;
        c.canHost = true;
        c.findPublic = false;
        return c;
    }

    const OnlineState& State() const { return session.State(); }

    SimEnv env;
    Session session;
    SessionSide side;
    FakeGame game;
    uint32_t ip;
    uint64_t frameUs;
    uint64_t lastFrame;
    uint64_t nextFrame;
    uint64_t nextWake;
    bool paused;
    bool framesPaused;   // the game loop stops while the network side runs on
};

class World
{
  public:
    explicit World(uint64_t seed) : net(seed) {}
    ~World()
    {
        for (size_t i = 0; i < machines.size(); i++)
            delete machines[i];
    }

    Machine* Add(uint32_t ip, double drift, double hz, int humans, const Identity& id,
                 const char* dir)
    {
        Machine* m = new Machine(&net, ip, net.rng(), drift, hz, humans, id, OutDir(dir));
        m->env.verbose = std::getenv("ONLINE_TEST_VERBOSE") != NULL;
        machines.push_back(m);
        return m;
    }

    // Moves time to the next event and handles it.
    void Step()
    {
        uint64_t next = net.NextDelivery();
        for (size_t i = 0; i < machines.size(); i++)
        {
            Machine& m = *machines[i];
            if (m.paused)
                continue;
            const uint64_t f = m.env.GlobalAt(m.nextFrame);
            const uint64_t w = m.env.GlobalAt(m.nextWake);
            if (f < next && !m.framesPaused)
                next = f;
            if (w < next)
                next = w;
        }
        if (next <= net.now)
            next = net.now + 1;
        if (trace && next / 1000000 != net.now / 1000000)
            for (size_t i = 0; i < machines.size(); i++)
            {
                Machine& m = *machines[i];
                const OnlineState& s = m.State();
                std::fprintf(stderr,
                             "t=%.0f m%zu phase %d tick %u conf %u have %u/%u delay %d stalls %d "
                             "advance %.2f "
                             "problem %d\n",
                             net.now / 1e6, i, s.phase, m.game.tick, s.confirmedTick,
                             m.session.Engine().Have(0), m.session.Engine().Have(1), s.delayTicks,
                             m.game.stallCount, s.advanceTicks, s.problem);
            }
        net.now = next;
        net.Deliver(NULL);
        for (size_t i = 0; i < machines.size(); i++)
        {
            Machine& m = *machines[i];
            if (m.paused)
            {
                for (size_t p = 0; p < m.env.pipes.size(); p++)
                    m.env.pipes[p]->m_inbox.clear();
                continue;
            }
            const uint64_t local = m.env.NowUs();
            if (m.env.HasInput() || local + 2 >= m.nextWake)
            {
                m.session.Poll();
                m.nextWake = m.session.NextWakeUs();
            }
            if (local + 2 >= m.nextFrame && !m.framesPaused)
            {
                m.game.Frame(local, local - m.lastFrame);
                m.lastFrame = local;
                while (m.nextFrame <= local + 2)
                    m.nextFrame += m.frameUs;
                // A frame can bring the network thread's next wake forward, never push it back.
                const uint64_t wake = m.session.NextWakeUs();
                if (wake < m.nextWake)
                    m.nextWake = wake;
                if (onFrame)
                    onFrame(m);
            }
        }
    }

    template <class Pred> bool RunUntil(Pred pred, double seconds)
    {
        const uint64_t end = net.now + (uint64_t)(seconds * 1e6);
        while (net.now < end)
        {
            if (pred())
                return true;
            Step();
        }
        return pred();
    }

    void RunFor(double seconds)
    {
        RunUntil([] { return false; }, seconds);
    }

    SimNet net;
    std::vector<Machine*> machines;
    std::function<void(Machine&)> onFrame;
    bool trace = std::getenv("ONLINE_TEST_TRACE") != NULL;
};

// Host on a, join from b, and wait until both sit in the room.
bool Connect(World& w, Machine* a, Machine* b, const char* address = "10.0.0.1:41210")
{
    if (!a->session.Host(ONLINE_DEFAULT_PORT, "Alice"))
        return false;
    if (!b->session.Join(address, "Bob"))
        return false;
    return w.RunUntil(
        [&] {
            return a->State().phase == ONLINE_PHASE_ROOM && b->State().phase == ONLINE_PHASE_ROOM &&
                   b->State().machineCount == 2 && a->State().machineCount == 2;
        },
        10);
}

bool StartMatch(World& w, Machine* host, const std::vector<Machine*>& guests)
{
    for (Machine* g : guests)
        g->session.SetReady(1);
    if (!w.RunUntil([&] { return host->State().canStart != 0; }, 10))
        return false;
    return host->session.Start() != 0;
}

// The session's auto delay: only the part of the worst path past the guessing window.
int ExpectedDelay(const NetParams& p)
{
    const double b = p.oneWayMs + 2 * p.jitterMs + 3 - 14 * 20.0;
    int d = b > 0 ? (int)std::ceil(b / 20.0) : 0;
    return d > 8 ? 8 : d;
}

// ONLINE_SIM_MATCHES and ONLINE_SIM_TICKS lengthen the run.
void TestManyMatches()
{
    const char* envMatches = std::getenv("ONLINE_SIM_MATCHES");
    const char* envTicks = std::getenv("ONLINE_SIM_TICKS");
    const int matches = envMatches != NULL ? std::atoi(envMatches) : 100;
    const uint32_t ticks = envTicks != NULL ? (uint32_t)std::atoi(envTicks) : 6000;

    World w(1);
    w.net.params.oneWayMs = 50;
    w.net.params.jitterMs = 20;
    w.net.params.loss = 0.02;
    w.net.params.duplicate = 0.01;
    Identity id = TestIdentity();
    // Clocks 0.4% apart and different frame rates, so time sync has something to do.
    Machine* a = w.Add(0x0A000001, 0.002, 60, 1, id, "many");
    Machine* b = w.Add(0x0A000002, -0.002, 144, 1, id, "many");
    // The wandering pads press Start, which often pauses a machine already past the pause tick.
    for (Machine* m : { a, b })
    {
        m->game.matchTicks = ticks;
        m->game.pausesPerMatch = 3;
    }
    CHECK(Connect(w, a, b));
    w.RunFor(5);
    CHECK(a->State().delayTicks == ExpectedDelay(w.net.params));
    CHECK(StartMatch(w, a, { b }));

    int desyncs = 0;
    int goals = 0;
    for (int k = 0; k < matches; k++)
    {
        const bool done = w.RunUntil(
            [&] {
                return a->game.results.size() > (size_t)k && b->game.results.size() > (size_t)k &&
                       a->State().phase == ONLINE_PHASE_POST_MATCH &&
                       b->State().phase == ONLINE_PHASE_POST_MATCH;
            },
            ticks * 0.02 * 3 + 60);
        CHECK(done);
        if (!done)
        {
            std::printf("match %d stuck: phases %d %d, ticks %u %u, desync %d %d\n", k,
                        a->State().phase, b->State().phase, a->game.tick, b->game.tick,
                        a->State().desync, b->State().desync);
            break;
        }
        const MatchResult& ra = a->game.results[k];
        const MatchResult& rb = b->game.results[k];
        CHECK(ra.state == rb.state);
        CHECK(ra.ticks == ticks && rb.ticks == ticks);
        CHECK(ra.serial == rb.serial);
        goals += ra.goals;
        if (a->State().desync != ONLINE_DESYNC_NONE || b->State().desync != ONLINE_DESYNC_NONE)
            desyncs++;
        CHECK(a->session.Engine().Conflicts() == 0 && b->session.Engine().Conflicts() == 0);

        if (k + 1 == matches)
            break;
        if (k % 4 != 3)
        {
            // Rematch: straight back to loading.
            CHECK(a->session.Vote(-1, ONLINE_VOTE_REMATCH));
            CHECK(b->session.Vote(-1, ONLINE_VOTE_REMATCH));
            a->session.Flush();
            b->session.Flush();
            CHECK(w.RunUntil(
                [&] {
                    return a->State().phase == ONLINE_PHASE_LOADING &&
                           b->State().phase == ONLINE_PHASE_LOADING;
                },
                10));
        }
        else
        {
            CHECK(a->session.Vote(-1, ONLINE_VOTE_BACK_TO_ROOM));
            CHECK(b->session.Vote(-1, ONLINE_VOTE_CHANGE_TEAMS));
            a->session.Flush();
            b->session.Flush();
            CHECK(w.RunUntil(
                [&] {
                    return a->State().phase == ONLINE_PHASE_ROOM &&
                           b->State().phase == ONLINE_PHASE_ROOM;
                },
                10));
            CHECK(StartMatch(w, a, { b }));
        }
    }

    const int expected = ExpectedDelay(w.net.params);
    int onBudget = 0;
    int offByOne = 0;
    int total = 0;
    for (Machine* m : { a, b })
        for (int d : m->game.kickoffDelays)
        {
            total++;
            if (d == expected)
                onBudget++;
            else if (d == expected + 1 || d == expected - 1)
                offByOne++;
        }
    std::printf("many matches: %zu played, %d goals, %d desyncs\n", a->game.results.size(), goals,
                desyncs);
    std::printf("  stalls: a %d (longest %u ms), b %d (longest %u ms); over 250 ms: %d, %d\n",
                a->game.stallCount, a->game.maxStallMs, b->game.stallCount, b->game.maxStallMs,
                a->game.longStalls, b->game.longStalls);
    std::printf("  kickoff delay: %d of %d at the budget's %d ticks, %d one off\n", onBudget, total,
                expected, offByOne);
    std::printf("  datagrams: %llu sent, %llu lost\n", (unsigned long long)w.net.sent,
                (unsigned long long)w.net.lost);
    std::printf("  pauses: %d\n", a->game.pausesTaken);
    std::printf("  rollbacks: a %d (%llu ticks again of %llu, longest %u), b %d (%llu of %llu, "
                "longest %u)\n",
                a->game.rollbacks, (unsigned long long)a->game.ticksRerun,
                (unsigned long long)a->game.ticksRun, a->game.longestRollback, b->game.rollbacks,
                (unsigned long long)b->game.ticksRerun, (unsigned long long)b->game.ticksRun,
                b->game.longestRollback);
    CHECK((int)a->game.results.size() == matches);
    CHECK(desyncs == 0);
    CHECK(a->game.pausesTaken >= matches);
    CHECK(a->game.maxStallMs <= 250 && b->game.maxStallMs <= 250);
    CHECK(onBudget + offByOne == total);
    CHECK(onBudget * 10 >= total * 9);
}

void TestBitFlip()
{
    World w(2);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0.0, 60, 1, id, "flip");
    Machine* b = w.Add(0x0A000002, 0.0, 60, 1, id, "flip");
    a->game.matchTicks = 3000;
    b->game.matchTicks = 3000;
    b->game.flipAtTick = 777;
    CHECK(Connect(w, a, b));
    w.RunFor(2);
    CHECK(StartMatch(w, a, { b }));
    const bool ended = w.RunUntil(
        [&] {
            return a->State().desync == ONLINE_DESYNC_ENDED &&
                   b->State().desync == ONLINE_DESYNC_ENDED &&
                   a->State().phase == ONLINE_PHASE_ROOM && b->State().phase == ONLINE_PHASE_ROOM;
        },
        120);
    CHECK(ended);
    std::printf("bit flip at 777: a stopped at %u, b at %u; desync ticks %u and %u\n", a->game.tick,
                b->game.tick, a->State().desyncTick, b->State().desyncTick);
    CHECK(a->State().desyncTick == 777);
    CHECK(b->State().desyncTick == 777);
    CHECK(a->State().desyncCount == 1);
    CHECK(a->game.tick < 777 + 64 && b->game.tick < 777 + 64);
    for (Machine* m : { a, b })
    {
        const char* path = m->State().desyncReport;
        CHECK(path[0] != '\0');
        FILE* f = std::fopen(path, "r");
        CHECK(f != NULL);
        if (f == NULL)
            continue;
        char line[256];
        bool first = false;
        bool desc = false;
        int inputs = 0;
        int hashes = 0;
        while (std::fgets(line, sizeof line, f) != NULL)
        {
            if (std::strncmp(line, "first-tick 777", 14) == 0)
                first = true;
            if (std::strncmp(line, "desc ", 5) == 0 && std::strlen(line) >= 5 + 128)
                desc = true;
            if (std::strncmp(line, "input ", 6) == 0)
                inputs++;
            if (std::strncmp(line, "hash ", 5) == 0)
                hashes++;
        }
        std::fclose(f);
        CHECK(first && desc && inputs > 0 && hashes > 0);
    }
}

void TestSilent(bool hostGoes)
{
    World w(hostGoes ? 4 : 3);
    w.net.params.jitterMs = 5;
    w.net.params.loss = 0.0;
    w.net.params.duplicate = 0.0;
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0.0, 60, 1, id, "silent");
    Machine* b = w.Add(0x0A000002, 0.0, 60, 1, id, "silent");
    a->game.matchTicks = 6000;
    b->game.matchTicks = 6000;
    a->game.goalOdds = 0;
    b->game.goalOdds = 0;
    CHECK(Connect(w, a, b));
    w.RunFor(2);
    CHECK(StartMatch(w, a, { b }));
    CHECK(w.RunUntil([&] { return a->game.tick >= 600 && b->game.tick >= 600; }, 60));

    Machine* gone = hostGoes ? a : b;
    Machine* stays = hostGoes ? b : a;
    w.net.silenced.push_back(gone->ip);
    gone->paused = true;
    const uint64_t last = w.net.lastFrom[gone->ip];
    uint64_t waiting = 0;
    float firstSeconds = -1;
    uint64_t dropped = 0;
    const uint32_t notices = stays->State().noticeSerial;
    w.onFrame = [&](Machine& m) {
        if (&m != stays)
            return;
        const OnlineState& s = m.State();
        if (waiting == 0 && s.problem == ONLINE_PROBLEM_WAITING)
        {
            waiting = w.net.now;
            firstSeconds = s.problemSeconds;
        }
        if (dropped == 0 && s.noticeSerial != notices && s.notice == ONLINE_NOTICE_DROPPED_TO_CPU)
            dropped = w.net.now;
    };
    const uint32_t before = stays->game.tick;
    CHECK(w.RunUntil([&] { return dropped != 0; }, 10));
    w.RunFor(2);
    w.onFrame = nullptr;
    const double waitAt = (double)(waiting - last) / 1e6;
    const double dropAt = (double)(dropped - last) / 1e6;
    std::printf(
        "%s silent: waiting after %.3f s (%.2f s to go), dropped after %.3f s, ticks %u -> %u\n",
        hostGoes ? "host" : "guest", waitAt, firstSeconds, dropAt, before, stays->game.tick);
    CHECK(waiting != 0 && waitAt >= 1.0 && waitAt <= 1.05);
    CHECK(firstSeconds > 4.9f && firstSeconds <= 5.0f);
    CHECK(dropped != 0 && dropAt >= 6.0 && dropAt <= 6.06);
    CHECK(stays->game.tick > before + 50);
    CHECK(stays->State().opponentsLeft == 1);
    CHECK(stays->session.SlotKind(hostGoes ? 0 : 1) == ONLINE_SLOT_CPU);
    CHECK(stays->State().problem != ONLINE_PROBLEM_WAITING);
    // Playing on alone to the final whistle, through barriers.
    CHECK(stays->session.AfterOpponentsLeft(1));
    CHECK(w.RunUntil([&] { return !stays->game.results.empty(); }, 200));
    w.RunFor(0.1);
    std::printf("  after the final whistle: phase %d, error %d\n", stays->State().phase,
                stays->State().error);
    if (hostGoes)
        CHECK(w.RunUntil([&] { return stays->State().phase == ONLINE_PHASE_ERROR; }, 2));
    else
    {
        CHECK(stays->State().phase == ONLINE_PHASE_POST_MATCH);
        // Nobody is left to play a rematch with, so the vote opens the room again.
        CHECK(stays->session.Vote(-1, ONLINE_VOTE_REMATCH));
        CHECK(w.RunUntil([&] { return stays->State().phase == ONLINE_PHASE_HOSTING; }, 2));
    }
}

void TestCouches()
{
    World w(5);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0.001, 60, 2, id, "couch");
    Machine* b = w.Add(0x0A000002, -0.001, 120, 2, id, "couch");
    a->game.matchTicks = 2000;
    b->game.matchTicks = 2000;
    CHECK(Connect(w, a, b));
    CHECK(a->session.AddLocalPlayer(1, "Ann"));
    CHECK(b->session.AddLocalPlayer(1, NULL));
    CHECK(w.RunUntil(
        [&] { return a->State().desc.slotMachine[3] == 1 && b->State().desc.slotMachine[3] == 1; },
        5));
    const OnlineMatchDesc& d = a->State().desc;
    CHECK(d.slotMachine[0] == 0 && d.slotMachine[1] == 1 && d.slotMachine[2] == 0 &&
          d.slotMachine[3] == 1);
    CHECK(d.padSide[0] == 0 && d.padSide[1] == 1 && d.padSide[2] == 0 && d.padSide[3] == 1);
    CHECK(b->session.SlotKind(1) == ONLINE_SLOT_LOCAL &&
          b->session.SlotKind(0) == ONLINE_SLOT_REMOTE);
    CHECK(b->session.SlotLocalPort(3) == 1 && a->session.SlotLocalPort(2) == 1);
    CHECK(std::strcmp(b->session.SlotName(2), "Ann") == 0);
    CHECK(StartMatch(w, a, { b }));
    CHECK(w.RunUntil([&] { return !a->game.results.empty() && !b->game.results.empty(); }, 200));
    if (!a->game.results.empty() && !b->game.results.empty())
        CHECK(a->game.results[0].state == b->game.results[0].state);
    CHECK(a->State().desync == ONLINE_DESYNC_NONE);
}

void TestRefusals()
{
    struct Case
    {
        const char* what;
        int error;
        const char* words;
    };
    const Case cases[] = {
        { "version", ONLINE_ERR_VERSION, "1.6.0" },
        { "data", ONLINE_ERR_DATA_FAMILY, "Japanese" },
    };
    for (const Case& c : cases)
    {
        World w(6);
        Identity host = TestIdentity();
        Identity guest = TestIdentity();
        if (std::strcmp(c.what, "version") == 0)
            std::snprintf(guest.version, sizeof guest.version, "1.5.9");
        if (std::strcmp(c.what, "data") == 0)
        {
            host.dataFamily = 0xA11CE;
            host.dataRegion = 1;
        }
        Machine* a = w.Add(0x0A000001, 0, 60, 1, host, "refuse");
        Machine* b = w.Add(0x0A000002, 0, 60, 1, guest, "refuse");
        CHECK(a->session.Host(ONLINE_DEFAULT_PORT, "Alice"));
        CHECK(b->session.Join("10.0.0.1", "Bob"));
        CHECK(w.RunUntil([&] { return b->State().phase == ONLINE_PHASE_ERROR; }, 10));
        std::printf("refused for %s: %s\n", c.what, b->State().lastError);
        CHECK(b->State().error == c.error);
        CHECK(std::strstr(b->State().lastError, c.words) != NULL);
        w.RunFor(3);
        CHECK(a->State().phase == ONLINE_PHASE_HOSTING);
        CHECK(a->State().machineCount == 1);
    }

    // Four humans already: the fifth is refused before any keys.
    {
        World w(7);
        Identity id = TestIdentity();
        Machine* a = w.Add(0x0A000001, 0, 60, 2, id, "full");
        Machine* b = w.Add(0x0A000002, 0, 60, 2, id, "full");
        Machine* c = w.Add(0x0A000003, 0, 60, 1, id, "full");
        CHECK(Connect(w, a, b));
        CHECK(a->session.AddLocalPlayer(1, "Ann"));
        CHECK(b->session.AddLocalPlayer(1, "Bea"));
        CHECK(w.RunUntil([&] { return a->State().desc.slotMachine[3] == 1; }, 5));
        CHECK(c->session.Join("10.0.0.1:41210", "Cy"));
        CHECK(w.RunUntil([&] { return c->State().phase == ONLINE_PHASE_ERROR; }, 10));
        CHECK(c->State().error == ONLINE_ERR_ROOM_FULL);
    }

    // Nobody there.
    {
        World w(8);
        Identity id = TestIdentity();
        Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "none");
        CHECK(b->session.Join("10.0.0.9:41210", "Bob"));
        CHECK(w.RunUntil([&] { return b->State().phase == ONLINE_PHASE_ERROR; }, 10));
        CHECK(b->State().error == ONLINE_ERR_UNREACHABLE);
        CHECK(std::strstr(b->State().lastError, "10.0.0.9:41210") != NULL);
        CHECK(!b->session.Join("not an address", "Bob"));
        CHECK(b->State().error == ONLINE_ERR_BAD_ADDRESS);
        CHECK(b->session.Join("nowhere:41210", "Bob"));
        w.RunFor(0.1);
        CHECK(b->State().error == ONLINE_ERR_BAD_ADDRESS);
        // A link-local address, and an IPv6 address on a socket that has only IPv4.
        b->session.Join("[fe80::1]:41210", "Bob");
        CHECK(b->State().error == ONLINE_ERR_BAD_ADDRESS &&
              std::strstr(b->State().lastError, "link-local") != NULL);
        b->session.Join("[2001:db8::5]:41210", "Bob");
        CHECK(b->State().error == ONLINE_ERR_UNREACHABLE &&
              std::strstr(b->State().lastError, "IPv6") != NULL);
    }

    // A name that resolves, and the port already taken.
    {
        World w(9);
        Identity id = TestIdentity();
        Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "names");
        Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "names");
        CHECK(Connect(w, a, b, "host-a:41210"));
        Machine* a2 = w.Add(0x0A000001, 0, 60, 1, id, "names");
        CHECK(!a2->session.Host(ONLINE_DEFAULT_PORT, "Again"));
        CHECK(a2->State().error == ONLINE_ERR_PORT_IN_USE);
    }
}

void TestNearby()
{
    World w(11);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "nearby");
    Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "nearby");
    CHECK(a->session.Host(ONLINE_DEFAULT_PORT, "Alice"));
    CHECK(b->session.RefreshNearby());
    CHECK(w.RunUntil([&] { return b->State().nearbySearching == 0; }, 3));
    CHECK(b->State().nearbyCount == 1);
    if (b->State().nearbyCount == 1)
    {
        const OnlineNearbyRoom& r = b->State().nearby[0];
        CHECK(std::strcmp(r.address, "10.0.0.1:41210") == 0);
        CHECK(std::strcmp(r.hostName, "Alice") == 0);
        CHECK(r.compatible == 1 && r.machines == 1 && r.humans == 1 && r.inMatch == 0);
        CHECK(b->session.Join(r.address, "Bob"));
        CHECK(w.RunUntil([&] { return b->State().phase == ONLINE_PHASE_ROOM; }, 5));
        CHECK(b->State().roomKind == ONLINE_ROOM_LAN);
    }
    w.RunFor(6);
    CHECK(b->State().nearbyCount == 0);
}

void TestHostileGuest()
{
    World w(12);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "hostile");
    CHECK(a->session.Host(ONLINE_DEFAULT_PORT, "Alice"));
    w.net.params.loss = 0;
    w.net.params.jitterMs = 1;

    SimPipe raw(&w.net, SimAddr(0x0A000066, 5555));
    w.net.Bind(&raw);
    const NetAddr host = SimAddr(0x0A000001, ONLINE_DEFAULT_PORT);
    std::mt19937_64 rng(99);
    KeyPair key;
    for (int i = 0; i < 32; i++)
        key.secret[i] = (uint8_t)rng();
    KeyPairFromSecret(&key);
    Hello h;
    std::memcpy(h.publicKey, key.publicKey, 32);
    for (int i = 0; i < 16; i++)
        h.nonce[i] = (uint8_t)rng();
    uint8_t hello[kHelloSize];
    EncodeHello(h, hello);
    raw.Send(host, hello, sizeof hello);
    Welcome welcome;
    bool welcomed = false;
    w.RunUntil(
        [&] {
            NetAddr from;
            uint8_t buf[2048];
            const int n = raw.Receive(&from, buf, sizeof buf);
            if (n > 0 && DecodeWelcome(buf, (size_t)n, &welcome))
                welcomed = true;
            return welcomed;
        },
        2);
    CHECK(welcomed);
    uint8_t g2h[32];
    uint8_t h2g[32];
    CHECK(DeriveLinkKeys(key.secret, welcome.publicKey, key.publicKey, welcome.publicKey, h.nonce,
                         welcome.linkId, g2h, h2g));
    uint64_t seq = 0;
    auto sendSealed = [&](const uint8_t* plain, size_t n) {
        uint8_t out[kMaxDatagram];
        const size_t size = SealDatagram(g2h, welcome.linkId, ++seq, plain, n, out);
        raw.Send(host, out, size);
    };

    // A proper JOIN first, so the link is admitted.
    Message join;
    std::memset(&join, 0, sizeof join);
    join.kind = MSG_JOIN;
    join.u.join.protocol = ONLINE_PROTOCOL_VERSION;
    join.u.join.platform = id.platform;
    join.u.join.binaryFamily = id.binaryFamily;
    join.u.join.build = id.build;
    join.u.join.dataFamily = id.dataFamily;
    std::memcpy(join.u.join.version, id.version, sizeof id.version);
    std::snprintf(join.u.join.nickname, sizeof join.u.join.nickname, "Mallory");
    join.u.join.humanCount = 1;
    std::snprintf(join.u.join.human[0], sizeof join.u.join.human[0], "Mallory");
    uint8_t body[kMaxMessageBody];
    const size_t bn = EncodeMessageBody(join, body);
    uint8_t plain[kMaxPayload];
    ChunkWriter cw(plain, sizeof plain);
    cw.Reliable(MSG_JOIN, 1, body, bn);
    sendSealed(plain, cw.Size());
    CHECK(w.RunUntil([&] { return a->State().machineCount == 2; }, 2));

    // Well-formed chunks with hostile values, then random bytes, all authenticated.
    int sentBad = 0;
    for (int round = 0; round < 400; round++)
    {
        uint8_t p[kMaxPayload];
        size_t n;
        if (round < 200)
        {
            ChunkWriter wr(p, sizeof p);
            Message m;
            std::memset(&m, 0, sizeof m);
            const uint8_t kinds[] = { MSG_READY,        MSG_LOCAL_PLAYER,  MSG_VOTE,   MSG_DESC_ACK,
                                      MSG_BARRIER_DONE, MSG_LOAD_PROGRESS, MSG_DESYNC, MSG_JOIN };
            const uint8_t kind = kinds[rng() % 8];
            m.kind = kind;
            uint8_t b2[kMaxMessageBody];
            size_t len = EncodeMessageBody(m, b2);
            for (size_t i = 0; i < len; i++)
                if (rng() % 3 == 0)
                    b2[i] = (uint8_t)rng();
            wr.Reliable(kind, (uint16_t)(2 + round), b2, len);
            uint8_t rec[kRecordSize] = {};
            for (size_t i = 0; i < kRecordSize; i++)
                rec[i] = (uint8_t)(rng() % 4 == 0 ? rng() : 0);
            wr.Input((uint32_t)rng() % 3, (uint8_t)(rng() % 4), (uint32_t)(rng() % 100), 1, rec);
            n = wr.Size();
        }
        else
        {
            n = 1 + rng() % 300;
            for (size_t i = 0; i < n; i++)
                p[i] = (uint8_t)rng();
            sentBad++;
        }
        sendSealed(p, n);
        w.RunFor(0.005);
    }
    // A replayed datagram and a forged tag are dropped without a reply.
    w.RunFor(1);
    std::printf("hostile guest: host phase %d with %d machines after %d random payloads\n",
                a->State().phase, a->State().machineCount, sentBad);
    CHECK(a->State().phase == ONLINE_PHASE_HOSTING);
    CHECK(a->State().machineCount == 1);
    CHECK(a->State().noticeSerial != 0);
    w.net.Unbind(&raw);
}

// A client that sends what no Session would, from its own address or a forged one.
struct RawPeer
{
    RawPeer(World& world, uint32_t ip, uint16_t port, uint64_t seed)
        : w(world), pipe(&world.net, SimAddr(ip, port)),
          host(SimAddr(0x0A000001, ONLINE_DEFAULT_PORT)), rng(seed), payload(new Payload)
    {
        w.net.Bind(&pipe);
        for (int i = 0; i < 32; i++)
            key.secret[i] = (uint8_t)rng();
        KeyPairFromSecret(&key);
        std::memcpy(hello.publicKey, key.publicKey, 32);
        for (int i = 0; i < 16; i++)
            hello.nonce[i] = (uint8_t)rng();
    }

    // One HELLO; true once its WELCOME has come and the keys are derived.
    bool Handshake(double seconds)
    {
        uint8_t out[kHelloSize];
        EncodeHello(hello, out);
        pipe.Send(host, out, sizeof out);
        bool welcomed = false;
        w.RunUntil(
            [&] {
                NetAddr from;
                uint8_t buf[2048];
                int n;
                while ((n = pipe.Receive(&from, buf, sizeof buf)) > 0)
                    if (DecodeWelcome(buf, (size_t)n, &welcome) &&
                        std::memcmp(welcome.nonce, hello.nonce, 16) == 0)
                        welcomed = true;
                return welcomed;
            },
            seconds);
        return welcomed && DeriveLinkKeys(key.secret, welcome.publicKey, key.publicKey,
                                          welcome.publicKey, hello.nonce, welcome.linkId, g2h, h2g);
    }

    // Seals a payload and sends it from this peer, or from `from`; the datagram's size.
    size_t Seal(const uint8_t* plain, size_t n, SimPipe* from = NULL)
    {
        uint8_t out[kMaxDatagram];
        const size_t size = SealDatagram(g2h, welcome.linkId, ++seq, plain, n, out);
        (from != NULL ? from : &pipe)->Send(host, out, size);
        return size;
    }

    // Reliable messages in one datagram, acknowledging everything received.
    void Send(const Message* ms, int count)
    {
        uint8_t plain[kMaxPayload];
        ChunkWriter cw(plain, sizeof plain);
        cw.RelAck(recvNext, 0);
        for (int i = 0; i < count; i++)
        {
            uint8_t body[kMaxMessageBody];
            const size_t n = EncodeMessageBody(ms[i], body);
            CHECK(cw.Reliable(ms[i].kind, nextRel++, body, n));
        }
        Seal(plain, cw.Size());
    }

    // The host's reliable messages since the last call, each once and in order.
    std::vector<Message> Receive()
    {
        std::vector<Message> got;
        NetAddr from;
        uint8_t buf[2048];
        int n;
        while ((n = pipe.Receive(&from, buf, sizeof buf)) > 0)
        {
            SealedHeader h;
            uint8_t plain[kMaxDatagram];
            size_t size = 0;
            if (!DecodeSealedHeader(buf, (size_t)n, &h) || h.linkId != welcome.linkId ||
                !OpenDatagram(h2g, buf, (size_t)n, plain, &size) ||
                !ParsePayload(plain, size, payload.get()))
                continue;
            for (int i = 0; i < payload->messageCount; i++)
                if (payload->messages[i].relSeq == recvNext)
                {
                    recvNext++;
                    got.push_back(payload->messages[i]);
                }
        }
        return got;
    }

    World& w;
    SimPipe pipe;
    NetAddr host;
    std::mt19937_64 rng;
    std::unique_ptr<Payload> payload;
    KeyPair key;
    Hello hello;
    Welcome welcome;
    uint8_t g2h[32];
    uint8_t h2g[32];
    uint64_t seq = 0;
    uint16_t nextRel = 1;
    uint16_t recvNext = 1;
};

Message JoinMessage(const Identity& id, const char* name)
{
    Message m;
    std::memset(&m, 0, sizeof m);
    m.kind = MSG_JOIN;
    m.u.join.protocol = ONLINE_PROTOCOL_VERSION;
    m.u.join.platform = id.platform;
    m.u.join.binaryFamily = id.binaryFamily;
    m.u.join.build = id.build;
    m.u.join.dataFamily = id.dataFamily;
    std::memcpy(m.u.join.version, id.version, sizeof id.version);
    std::snprintf(m.u.join.nickname, sizeof m.u.join.nickname, "%s", name);
    m.u.join.humanCount = 1;
    std::snprintf(m.u.join.human[0], sizeof m.u.join.human[0], "%s", name);
    return m;
}

void QuietNet(World& w)
{
    w.net.params.loss = 0;
    w.net.params.jitterMs = 1;
    w.net.params.duplicate = 0;
}

void TestSetupMismatch()
{
    World w(10);
    QuietNet(w);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "mismatch");
    CHECK(a->session.Host(ONLINE_DEFAULT_PORT, "Alice"));
    RawPeer m(w, 0x0A000066, 5555, 10);
    CHECK(m.Handshake(1));
    Message join = JoinMessage(id, "Mallory");
    m.Send(&join, 1);
    // A guest that acknowledges the room but refuses the final descriptor, as one whose
    // settings differ would.
    uint32_t revision = 0;
    bool refused = false;
    auto answer = [&] {
        for (const Message& msg : m.Receive())
        {
            Message r[2];
            std::memset(r, 0, sizeof r);
            if (msg.kind != MSG_DESC)
                continue;
            revision = msg.u.desc.revision;
            OnlineMatchDesc d;
            CHECK(PortOnlineDescDecode(msg.u.desc.desc, &d) == NULL);
            r[0].kind = MSG_DESC_ACK;
            r[0].u.descAck.revision = revision;
            r[0].u.descAck.hash = PortOnlineDescHash(&d);
            r[0].u.descAck.accepted = !refused;
            r[0].u.descAck.reason = refused ? 2 : 0;
            r[1].kind = MSG_READY;
            r[1].u.ready = 1;
            m.Send(r, 2);
        }
        m.Send(NULL, 0);
    };
    CHECK(w.RunUntil(
        [&] {
            answer();
            return a->State().canStart != 0;
        },
        5));
    const uint32_t na = a->State().noticeSerial;
    refused = true;
    CHECK(a->session.Start());
    CHECK(w.RunUntil(
        [&] {
            answer();
            return a->State().noticeSerial != na &&
                   a->State().notice == ONLINE_NOTICE_SETUP_MISMATCH &&
                   a->State().phase == ONLINE_PHASE_ROOM;
        },
        5));
    CHECK(a->State().matchSerial == 0);
}

// A link that never joined reports a desync for every match serial it can guess.
void TestOutsiderDesync()
{
    World w(20);
    QuietNet(w);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "outsider");
    Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "outsider");
    CHECK(Connect(w, a, b));
    RawPeer m(w, 0x0A000066, 5555, 20);
    CHECK(m.Handshake(1));
    CHECK(StartMatch(w, a, { b }));
    CHECK(w.RunUntil([&] { return a->game.tick >= 10 && b->game.tick >= 10; }, 6));
    for (int batch = 0; batch < 2; batch++)
    {
        Message d[16];
        for (int i = 0; i < 16; i++)
        {
            std::memset(&d[i], 0, sizeof d[i]);
            d[i].kind = MSG_DESYNC;
            d[i].u.desync.matchSerial = (uint32_t)(batch * 16 + i + 1);
            d[i].u.desync.tick = 5;
            d[i].u.desync.reporter = 1;
        }
        m.Send(d, 16);
    }
    const uint32_t before = a->game.tick;
    w.RunFor(2);
    std::printf("outsider desync: host desync %d phase %d, ticks %u -> %u\n", a->State().desync,
                a->State().phase, before, a->game.tick);
    CHECK(a->State().desync == ONLINE_DESYNC_NONE && b->State().desync == ONLINE_DESYNC_NONE);
    CHECK(a->State().phase == ONLINE_PHASE_MATCH && a->game.tick > before + 50);
}

// A guest in the match pushes its input log ahead of the host's tick as fast as it can send.
void TestInputFlood()
{
    World w(21);
    QuietNet(w);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "flood");
    a->game.goalOdds = 0;
    CHECK(a->session.Host(ONLINE_DEFAULT_PORT, "Alice"));
    RawPeer m(w, 0x0A000066, 5555, 21);
    CHECK(m.Handshake(1));
    Message join = JoinMessage(id, "Mallory");
    m.Send(&join, 1);

    int machine = -1;
    uint32_t serial = 0;
    bool go = false;
    // Answers as a guest would, up to GO.
    auto answer = [&] {
        const std::vector<Message> got = m.Receive();
        // An acknowledgement, or the host's window of unacknowledged messages fills.
        if (!got.empty())
            m.Send(NULL, 0);
        for (const Message& msg : got)
        {
            Message r[2];
            std::memset(r, 0, sizeof r);
            if (msg.kind == MSG_JOIN_ACCEPT)
                machine = msg.u.joinAccept.machine;
            if (msg.kind == MSG_DESC)
            {
                OnlineMatchDesc d;
                CHECK(PortOnlineDescDecode(msg.u.desc.desc, &d) == NULL);
                r[0].kind = MSG_DESC_ACK;
                r[0].u.descAck.revision = msg.u.desc.revision;
                r[0].u.descAck.hash = PortOnlineDescHash(&d);
                r[0].u.descAck.accepted = 1;
                r[1].kind = MSG_READY;
                r[1].u.ready = 1;
                m.Send(r, 2);
            }
            if (msg.kind == MSG_START)
            {
                serial = msg.u.start.matchSerial;
                r[0].kind = MSG_LOAD_PROGRESS;
                r[0].u.loadProgress.matchSerial = serial;
                r[0].u.loadProgress.loadState = ONLINE_LOAD_READY;
                r[0].u.loadProgress.percent = 100;
                m.Send(r, 1);
            }
            if (msg.kind == MSG_GO)
                go = true;
        }
        return go;
    };
    CHECK(w.RunUntil(
        [&] {
            answer();
            return a->State().canStart != 0;
        },
        5));
    CHECK(a->session.Start());
    CHECK(w.RunUntil(answer, 5));
    CHECK(machine == 1 && serial == 1);

    uint8_t records[kMaxRecordsPerChunk][kRecordSize];
    uint32_t next = 0;
    for (int i = 0; i < 1000; i++)
    {
        uint8_t plain[kMaxPayload];
        ChunkWriter cw(plain, sizeof plain);
        cw.RelAck(m.recvNext, 0);
        for (int c = 0; c < 2; c++)
        {
            std::memset(records, 0, sizeof records);
            for (int r = 0; r < kMaxRecordsPerChunk; r++)
            {
                records[r][16] = 1;
                records[r][0] = (uint8_t)((next + r) & 0x7F);
            }
            CHECK(cw.Input(serial, (uint8_t)machine, next, kMaxRecordsPerChunk, &records[0][0]));
            next += kMaxRecordsPerChunk;
        }
        m.Seal(plain, cw.Size());
        w.RunFor(0.001);
        m.Receive();
    }
    w.RunFor(0.2);
    const uint32_t have = a->session.Engine().Have((int)machine);
    std::printf("input flood: %u records sent, the host holds %u at tick %u\n", next, have,
                a->State().tick);
    CHECK(have <= a->State().tick + 64 + 8);
}

// Links that authenticate but never join, one after another, then a forged source address.
void TestUnjoinedLinks()
{
    World w(22);
    QuietNet(w);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "unjoined");
    CHECK(a->session.Host(ONLINE_DEFAULT_PORT, "Alice"));
    std::vector<std::unique_ptr<RawPeer>> peers;
    int welcomed = 0;
    for (int i = 0; i < 12; i++)
    {
        peers.emplace_back(new RawPeer(w, 0x0A000066, (uint16_t)(6000 + i), 100 + i));
        if (!peers.back()->Handshake(0.3))
            continue;
        welcomed++;
        uint8_t plain[16];
        ChunkWriter cw(plain, sizeof plain);
        cw.RelAck(1, 0);
        peers.back()->Seal(plain, cw.Size());
        w.RunFor(0.05);
    }
    std::printf("unjoined links: %d of 12 handshakes answered\n", welcomed);
    CHECK(welcomed == 8);

    // One of them points its link at a victim and keeps it alive from there.
    RawPeer& m = *peers[0];
    SimPipe victim(&w.net, SimAddr(0x0A000077, 7777));
    w.net.Bind(&victim);
    size_t forged = 0;
    for (int i = 0; i < 4; i++)
    {
        uint8_t plain[16];
        ChunkWriter cw(plain, sizeof plain);
        cw.RelAck(1, 0);
        forged += m.Seal(plain, cw.Size(), &victim);
        w.RunFor(1.5);
    }
    size_t bytes = 0;
    int count = 0;
    NetAddr from;
    uint8_t buf[2048];
    int n;
    while ((n = victim.Receive(&from, buf, sizeof buf)) > 0)
    {
        bytes += (size_t)n;
        count++;
    }
    std::printf("forged source: %zu bytes from it, %d datagrams and %zu bytes back to it\n", forged,
                count, bytes);
    CHECK(bytes <= 3 * forged);
}

// A guest's NAT moves it mid-match: the host follows, and the match plays on in sync.
void TestRebind()
{
    World w(23);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0.001, 60, 1, id, "rebind");
    Machine* b = w.Add(0x0A000002, -0.001, 60, 1, id, "rebind");
    a->game.matchTicks = 3000;
    b->game.matchTicks = 3000;
    CHECK(Connect(w, a, b));
    CHECK(StartMatch(w, a, { b }));
    CHECK(w.RunUntil([&] { return a->game.tick >= 600 && b->game.tick >= 600; }, 60));
    const uint32_t notices = a->State().noticeSerial;
    CHECK(b->env.pipes.size() == 1);
    SimPipe* p = b->env.pipes[0];
    w.net.Unbind(p);
    p->m_self.port = (uint16_t)(p->m_self.port + 7);
    w.net.Bind(p);
    CHECK(w.RunUntil([&] { return !a->game.results.empty() && !b->game.results.empty(); }, 120));
    if (!a->game.results.empty() && !b->game.results.empty())
        CHECK(a->game.results[0].state == b->game.results[0].state);
    std::printf("rebind: longest stall %u ms and %u ms\n", a->game.maxStallMs, b->game.maxStallMs);
    CHECK(a->State().noticeSerial == notices);
    CHECK(a->game.maxStallMs <= 250 && b->game.maxStallMs <= 250);
}

bool ConnectThree(World& w, Machine* a, Machine* b, Machine* c)
{
    if (!Connect(w, a, b) || !c->session.Join("10.0.0.1:41210", "Cy"))
        return false;
    return w.RunUntil(
        [&] {
            return a->State().machineCount == 3 && b->State().machineCount == 3 &&
                   c->State().machineCount == 3;
        },
        10);
}

// The host goes silent: each guest reached the other only through it.
void TestHostLostOfFour()
{
    World w(24);
    w.net.params.jitterMs = 5;
    w.net.params.loss = 0.0;
    w.net.params.duplicate = 0.0;
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "four");
    Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "four");
    Machine* c = w.Add(0x0A000003, 0, 60, 1, id, "four");
    Machine* d = w.Add(0x0A000004, 0, 60, 1, id, "four");
    for (Machine* m : { a, b, c, d })
    {
        m->game.matchTicks = 3000;
        m->game.goalOdds = 0;
    }
    CHECK(ConnectThree(w, a, b, c));
    CHECK(d->session.Join("10.0.0.1:41210", "Dee"));
    CHECK(w.RunUntil(
        [&] {
            return a->State().machineCount == 4 && b->State().machineCount == 4 &&
                   c->State().machineCount == 4 && d->State().machineCount == 4;
        },
        10));
    CHECK(StartMatch(w, a, { b, c, d }));
    CHECK(w.RunUntil(
        [&] {
            return a->game.tick >= 600 && b->game.tick >= 600 && c->game.tick >= 600 &&
                   d->game.tick >= 600;
        },
        60));
    w.net.silenced.push_back(a->ip);
    a->paused = true;
    const uint32_t tb = b->game.tick;
    const uint32_t tc = c->game.tick;
    const uint32_t td = d->game.tick;
    w.RunFor(8);
    std::printf("host of four silent: guests at %u and %u, 8 s later %u and %u\n", tb, tc,
                b->game.tick, c->game.tick);
    CHECK(b->game.tick > tb + 50 && c->game.tick > tc + 50 && d->game.tick > td + 50);
    CHECK(b->session.SlotKind(0) == ONLINE_SLOT_CPU && b->session.SlotKind(2) == ONLINE_SLOT_CPU &&
          b->session.SlotKind(3) == ONLINE_SLOT_CPU);
    CHECK(c->session.SlotKind(0) == ONLINE_SLOT_CPU && c->session.SlotKind(1) == ONLINE_SLOT_CPU &&
          c->session.SlotKind(3) == ONLINE_SLOT_CPU);
    CHECK(d->session.SlotKind(0) == ONLINE_SLOT_CPU && d->session.SlotKind(1) == ONLINE_SLOT_CPU &&
          d->session.SlotKind(2) == ONLINE_SLOT_CPU);
    CHECK(w.RunUntil(
        [&] {
            return !b->game.results.empty() && !c->game.results.empty() && !d->game.results.empty();
        },
        120));
}

// With no system randomness nobody hosts or joins, and a host refuses a guest's HELLO.
void TestNoRandomness()
{
    World w(49);
    QuietNet(w);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "no-random");
    Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "no-random");
    a->env.randomFails = true;
    CHECK(!a->session.Host(41210, "Ann"));
    CHECK(a->State().phase == ONLINE_PHASE_ERROR && a->State().error == ONLINE_ERR_NETWORK);
    a->session.Leave();
    b->env.randomFails = true;
    CHECK(!b->session.Join("10.0.0.1:41210", "Bo"));
    CHECK(b->State().phase == ONLINE_PHASE_ERROR);
    b->session.Leave();
    b->env.randomFails = false;
    a->env.randomFails = false;
    // A failed Host's socket closes at the next poll.
    w.RunFor(0.1);
    CHECK(a->session.Host(41210, "Ann"));
    a->env.randomFails = true;
    CHECK(b->session.Join("10.0.0.1:41210", "Bo"));
    w.RunFor(2);
    CHECK(a->State().machineCount == 1 && b->State().phase == ONLINE_PHASE_JOINING);
}

// A guest that keeps sending but never acknowledges is dropped once its reliable queue fills.
void TestUnacknowledged()
{
    World w(50);
    QuietNet(w);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "unacked");
    CHECK(a->session.Host(ONLINE_DEFAULT_PORT, "Alice"));
    RawPeer m(w, 0x0A000066, 5555, 50);
    CHECK(m.Handshake(1));
    Message join = JoinMessage(id, "Mallory");
    m.Send(&join, 1);
    CHECK(w.RunUntil([&] { return a->State().machineCount == 2; }, 5));
    OnlineMatchDesc d;
    PortOnlineDescDefault(&d);
    for (int i = 0; i < 300 && a->State().machineCount == 2; i++)
    {
        d.gameTime = (uint16_t)(i % 2 == 0 ? 180 : 120);
        a->session.SetDesc(d);
        m.Send(NULL, 0);
        w.RunFor(0.02);
    }
    std::printf("unacknowledged guest: %d machines after the queue filled\n",
                a->State().machineCount);
    CHECK(a->State().machineCount == 1);
}

// A guest dropped during play cannot block the remaining players' rematch.
void TestRematchAfterDrop()
{
    World w(47);
    QuietNet(w);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "drop-rematch");
    Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "drop-rematch");
    Machine* c = w.Add(0x0A000003, 0, 60, 1, id, "drop-rematch");
    for (Machine* m : { a, b, c })
    {
        m->game.matchTicks = 1200;
        m->game.goalOdds = 0;
    }
    CHECK(ConnectThree(w, a, b, c));
    CHECK(StartMatch(w, a, { b, c }));
    CHECK(w.RunUntil([&] { return c->game.tick >= 100; }, 60));
    w.net.silenced.push_back(c->ip);
    c->paused = true;
    CHECK(w.RunUntil(
        [&] {
            return a->session.SlotKind(2) == ONLINE_SLOT_CPU &&
                   b->session.SlotKind(2) == ONLINE_SLOT_CPU;
        },
        20));
    CHECK(w.RunUntil(
        [&] {
            return a->State().phase == ONLINE_PHASE_POST_MATCH &&
                   b->State().phase == ONLINE_PHASE_POST_MATCH;
        },
        120));
    CHECK(a->session.Vote(-1, ONLINE_VOTE_REMATCH));
    CHECK(b->session.Vote(-1, ONLINE_VOTE_REMATCH));
    CHECK(w.RunUntil([&] { return a->game.results.size() == 2 && b->game.results.size() == 2; },
                     120));
    CHECK(a->State().machineCount == 2 && b->State().machineCount == 2);
}

// Everyone votes for a rematch and one machine leaves before it starts.
void TestRematchLeaver()
{
    World w(25);
    QuietNet(w);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "leaver");
    Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "leaver");
    Machine* c = w.Add(0x0A000003, 0, 60, 1, id, "leaver");
    for (Machine* m : { a, b, c })
    {
        m->game.matchTicks = 300;
        m->game.goalOdds = 0;
    }
    CHECK(ConnectThree(w, a, b, c));
    CHECK(StartMatch(w, a, { b, c }));
    CHECK(w.RunUntil(
        [&] {
            return a->State().phase == ONLINE_PHASE_POST_MATCH &&
                   b->State().phase == ONLINE_PHASE_POST_MATCH &&
                   c->State().phase == ONLINE_PHASE_POST_MATCH;
        },
        60));
    CHECK(a->session.Vote(-1, ONLINE_VOTE_REMATCH));
    CHECK(b->session.Vote(-1, ONLINE_VOTE_REMATCH));
    b->session.Flush();
    w.RunFor(0.3);
    // The host decides on this vote, then hears the goodbye before the rematch starts.
    CHECK(c->session.Vote(-1, ONLINE_VOTE_REMATCH));
    c->session.Flush();
    w.RunFor(0.06);
    c->session.Leave();
    c->session.Flush();
    c->paused = true;
    const bool loading = w.RunUntil(
        [&] {
            return a->State().phase == ONLINE_PHASE_LOADING &&
                   b->State().phase == ONLINE_PHASE_LOADING;
        },
        5);
    std::printf("rematch with a leaver: phases %d and %d, %d machines\n", a->State().phase,
                b->State().phase, a->State().machineCount);
    CHECK(loading);
    CHECK(a->State().machineCount == 2);
}

// The guest's match ends first and it votes before the host's has ended.
void TestEarlyVote()
{
    World w(26);
    QuietNet(w);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "early-vote");
    Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "early-vote");
    for (Machine* m : { a, b })
    {
        m->game.matchTicks = 300;
        m->game.goalOdds = 0;
    }
    CHECK(Connect(w, a, b));
    CHECK(StartMatch(w, a, { b }));
    // The host's game loop stops once its input covers the last tick, before it runs it.
    CHECK(w.RunUntil([&] { return a->game.tick + (uint32_t)a->State().delayTicks >= 300; }, 60));
    a->framesPaused = true;
    CHECK(w.RunUntil([&] { return b->State().phase == ONLINE_PHASE_POST_MATCH; }, 10));
    CHECK(a->State().phase == ONLINE_PHASE_MATCH);
    CHECK(b->session.Vote(-1, ONLINE_VOTE_REMATCH));
    b->session.Flush();
    w.RunFor(0.5);
    a->framesPaused = false;
    CHECK(w.RunUntil([&] { return a->State().phase == ONLINE_PHASE_POST_MATCH; }, 10));
    CHECK(a->session.Vote(-1, ONLINE_VOTE_REMATCH));
    a->session.Flush();
    const bool loading = w.RunUntil(
        [&] {
            return a->State().phase == ONLINE_PHASE_LOADING &&
                   b->State().phase == ONLINE_PHASE_LOADING;
        },
        5);
    std::printf("early vote: phases %d and %d\n", a->State().phase, b->State().phase);
    CHECK(loading);
}

// A player presses A all through the intro; no tick after it may carry that press.
void TestBarrierPresses()
{
    World w(26);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "presses");
    Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "presses");
    for (Machine* m : { a, b })
    {
        m->game.goalOdds = 0;
        m->game.wander = false;
    }
    int pressedFrames = 0;
    w.onFrame = [&](Machine& m) {
        if (&m != a)
            return;
        // A for a frame, released for a frame, only while the intro presents.
        const bool press = m.game.presenting && m.game.armedKind == 1 && (pressedFrames++ & 1) == 0;
        m.game.pads[0].pad.buttons = press ? 0x0100 : 0;
    };
    CHECK(Connect(w, a, b));
    CHECK(StartMatch(w, a, { b }));
    // A machine may run ahead of the other's records on guesses, so wait for b to hold a's.
    CHECK(w.RunUntil(
        [&] {
            return a->game.tick >= 40 && b->game.tick >= 40 && b->session.Engine().Have(0) >= 40;
        },
        60));
    w.onFrame = nullptr;
    int carried = 0;
    for (uint32_t t = 0; t < 40; t++)
    {
        const uint8_t* rec = b->session.Engine().Record(0, t);
        CHECK(rec != NULL);
        if (rec != NULL && (RecordGet(rec, 0).buttons & 0x0100) != 0)
            carried++;
    }
    std::printf("presses at the intro: %d frames pressed, %d ticks carry A\n", pressedFrames / 2,
                carried);
    CHECK(pressedFrames > 10);
    CHECK(carried == 0);
}

// The host answers from an address other than the one the guest typed.
// A STUN server on the simulated network that answers each Binding request with `mapped`.
struct StunResponder
{
    StunResponder(SimNet* net, uint32_t ip, uint16_t port) : pipe(net, SimAddr(ip, port))
    {
        net->Bind(&pipe);
    }

    void Answer()
    {
        uint8_t buf[256];
        NetAddr from;
        int n;
        while ((n = pipe.Receive(&from, buf, sizeof buf)) >= 0)
        {
            requests++;
            if (n != (int)kStunRequestSize || !answering)
                continue;
            pipe.Send(from, Response(buf + 8, mapped), 32);
        }
    }

    // A Binding Success response to `txid` carrying `a` as XOR-MAPPED-ADDRESS.
    static const uint8_t* Response(const uint8_t* txid, const NetAddr& a)
    {
        static uint8_t r[32];
        const uint8_t head[] = { 0x01, 0x01, 0x00, 0x0c, 0x21, 0x12, 0xa4, 0x42 };
        std::memcpy(r, head, 8);
        std::memcpy(r + 8, txid, 12);
        const uint8_t attr[] = { 0x00, 0x20, 0x00, 0x08, 0x00, 0x01 };
        std::memcpy(r + 20, attr, 6);
        r[26] = (uint8_t)((a.port >> 8) ^ 0x21);
        r[27] = (uint8_t)((a.port & 255) ^ 0x12);
        const uint8_t cookie[4] = { 0x21, 0x12, 0xa4, 0x42 };
        for (int i = 0; i < 4; i++)
            r[28 + i] = (uint8_t)(a.ip[i] ^ cookie[i]);
        return r;
    }

    SimPipe pipe;
    NetAddr mapped;
    bool answering = true;
    int requests = 0;
};

struct PublicHost
{
    PublicHost(SimNet* net, bool findPublic)
        : env(net, 0x0A000001, 7, 0, 0, OutDir("public")),
          session(&env, TestIdentity(), Config(findPublic))
    {
        env.names["stun.cloudflare.com"] = 0x08080801;
        env.names["stun.l.google.com"] = 0x08080802;
        env.verbose = std::getenv("ONLINE_TEST_VERBOSE") != NULL;
    }

    static SessionConfig Config(bool findPublic)
    {
        SessionConfig c;
        c.discoveryPort = ONLINE_DISCOVERY_PORT;
        c.canHost = true;
        c.findPublic = findPublic;
        return c;
    }

    // Runs the host and the responders for `seconds` of simulated time, a millisecond at a time.
    void Run(SimNet& net, std::vector<StunResponder*> servers, double seconds)
    {
        const uint64_t end = net.now + (uint64_t)(seconds * 1e6);
        while (net.now < end)
        {
            net.now += 1000;
            net.Deliver(NULL);
            for (StunResponder* r : servers)
                r->Answer();
            session.Poll();
            session.Update(1.0f);
        }
    }

    SimEnv env;
    Session session;
};

void TestPublicAddress()
{
    auto quiet = [](SimNet& net) {
        net.params.oneWayMs = 10;
        net.params.jitterMs = 0;
        net.params.loss = 0;
        net.params.duplicate = 0;
    };

    // STUN alone: the outside IP with the room's port, though the NAT sent it out on another.
    {
        SimNet net(11);
        quiet(net);
        StunResponder cloudflare(&net, 0x08080801, 3478);
        cloudflare.mapped = SimAddr(0x56010203, 61000);
        PublicHost h(&net, true);
        h.env.portMap = PORTMAP_FAILED;
        CHECK(h.session.Host(ONLINE_DEFAULT_PORT, "Alice"));
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_LOOKING);
        CHECK(h.session.State().roomId != 0);
        CHECK(h.env.portMapPort == ONLINE_DEFAULT_PORT);
        h.Run(net, { &cloudflare }, 1);
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_FOUND);
        CHECK(std::strcmp(h.session.State().publicAddress, "86.1.2.3:41210") == 0);
        CHECK(cloudflare.requests == 1);
        h.session.Leave();
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_NONE);
        CHECK(h.session.State().publicAddress[0] == '\0' && h.session.State().roomId == 0);
        // The mapping outlives the room; only the game's exit removes it.
        CHECK(h.env.portMapStops == 0);
        h.session.Shutdown();
        CHECK(h.env.portMapStops == 1);
    }

    // Settings ask the router at launch, and a room on that port doesn't ask again.
    {
        SimNet net(21);
        quiet(net);
        StunResponder cloudflare(&net, 0x08080801, 3478);
        cloudflare.mapped = SimAddr(0x56010203, ONLINE_DEFAULT_PORT);
        PublicHost h(&net, true);
        h.env.portMap = PORTMAP_OPEN;
        h.env.portMapWan = "86.1.2.3";
        h.session.SetSettings(h.session.Settings());
        CHECK(h.env.portMapPort == ONLINE_DEFAULT_PORT);
        h.env.portMapPort = 0;
        CHECK(h.session.Host(ONLINE_DEFAULT_PORT, "Alice"));
        CHECK(h.env.portMapPort == 0);
        h.Run(net, { &cloudflare }, 1);
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_OPEN);
        h.session.Leave();
        OnlineSettings s = h.session.Settings();
        s.hostPort = 41300;
        h.session.SetSettings(s);
        CHECK(h.env.portMapPort == 41300);
        // A refused mapping is asked for again by the next room.
        h.env.portMap = PORTMAP_FAILED;
        h.env.portMapPort = 0;
        CHECK(h.session.Host(41300, "Alice"));
        CHECK(h.env.portMapPort == 41300);
    }

    // Forged answers: the right transaction from elsewhere, and the wrong one from the server.
    {
        SimNet net(12);
        quiet(net);
        StunResponder cloudflare(&net, 0x08080801, 3478);
        StunResponder forger(&net, 0x09090909, 3478);
        PublicHost h(&net, true);
        h.env.portMap = PORTMAP_FAILED;
        CHECK(h.session.Host(ONLINE_DEFAULT_PORT, "Alice"));
        // An on-path forger knows the transaction id.
        uint8_t req[64];
        NetAddr host;
        bool got = false;
        for (int i = 0; i < 100 && !got; i++)
        {
            h.Run(net, {}, 0.001);
            got = cloudflare.pipe.Receive(&host, req, sizeof req) == (int)kStunRequestSize;
        }
        CHECK(got);
        const NetAddr evil = SimAddr(0x01010101, 1);
        forger.pipe.Send(host, StunResponder::Response(req + 8, evil), 32);
        uint8_t wrong[12];
        std::memcpy(wrong, req + 8, 12);
        wrong[0] ^= 1;
        cloudflare.pipe.Send(host, StunResponder::Response(wrong, evil), 32);
        h.Run(net, {}, 0.05);
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_LOOKING);
        cloudflare.pipe.Send(host, StunResponder::Response(req + 8, SimAddr(0x56010203, 41210)),
                             32);
        h.Run(net, {}, 0.05);
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_FOUND);
        CHECK(std::strcmp(h.session.State().publicAddress, "86.1.2.3:41210") == 0);
    }

    // The router forwards the port and STUN agrees on the address.
    {
        SimNet net(13);
        quiet(net);
        StunResponder cloudflare(&net, 0x08080801, 3478);
        cloudflare.mapped = SimAddr(0x56010203, ONLINE_DEFAULT_PORT);
        PublicHost h(&net, true);
        h.env.portMap = PORTMAP_PENDING;
        CHECK(h.session.Host(ONLINE_DEFAULT_PORT, "Alice"));
        h.Run(net, { &cloudflare }, 1);
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_LOOKING);
        CHECK(std::strcmp(h.session.State().publicAddress, "86.1.2.3:41210") == 0);
        h.env.portMap = PORTMAP_OPEN;
        h.env.portMapWan = "86.1.2.3";
        h.Run(net, { &cloudflare }, 0.05);
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_OPEN);
        CHECK(std::strcmp(h.session.State().publicAddress, "86.1.2.3:41210") == 0);
        // A refused renewal drops the room back to found.
        h.env.portMap = PORTMAP_FAILED;
        h.Run(net, { &cloudflare }, 0.05);
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_FOUND);
        CHECK(std::strcmp(h.session.State().publicAddress, "86.1.2.3:41210") == 0);
    }

    // A second router: the inner one forwarded, but the internet sees the outer one.
    {
        SimNet net(14);
        quiet(net);
        StunResponder cloudflare(&net, 0x08080801, 3478);
        cloudflare.mapped = SimAddr(0x56010203, ONLINE_DEFAULT_PORT);
        PublicHost h(&net, true);
        h.env.portMap = PORTMAP_OPEN;
        h.env.portMapWan = "192.168.1.5";
        CHECK(h.session.Host(ONLINE_DEFAULT_PORT, "Alice"));
        h.Run(net, { &cloudflare }, 1);
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_FOUND);
        CHECK(std::strcmp(h.session.State().publicAddress, "86.1.2.3:41210") == 0);
    }

    // The router forwarded but didn't say its outside address: STUN's stands in.
    {
        SimNet net(19);
        quiet(net);
        StunResponder cloudflare(&net, 0x08080801, 3478);
        cloudflare.mapped = SimAddr(0x25E4ED83, 26420);
        PublicHost h(&net, true);
        h.env.portMap = PORTMAP_OPEN;
        CHECK(h.session.Host(ONLINE_DEFAULT_PORT, "Alice"));
        h.Run(net, { &cloudflare }, 1);
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_OPEN);
        CHECK(std::strcmp(h.session.State().publicAddress, "37.228.237.131:41210") == 0);
    }

    // STUN's address shows at once, and goes when the router turns out to be a carrier's.
    {
        SimNet net(20);
        quiet(net);
        StunResponder cloudflare(&net, 0x08080801, 3478);
        cloudflare.mapped = SimAddr(0x56010203, ONLINE_DEFAULT_PORT);
        PublicHost h(&net, true);
        h.env.portMap = PORTMAP_PENDING;
        CHECK(h.session.Host(ONLINE_DEFAULT_PORT, "Alice"));
        h.Run(net, { &cloudflare }, 1);
        CHECK(std::strcmp(h.session.State().publicAddress, "86.1.2.3:41210") == 0);
        h.env.portMap = PORTMAP_SHARED;
        h.env.portMapWan = "100.64.0.1";
        h.Run(net, { &cloudflare }, 0.05);
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_SHARED);
        CHECK(h.session.State().publicAddress[0] == '\0');
    }

    // A carrier's NAT, seen by STUN and by the router.
    {
        SimNet net(15);
        quiet(net);
        StunResponder cloudflare(&net, 0x08080801, 3478);
        cloudflare.mapped = SimAddr(0x64400001, 1234);
        PublicHost h(&net, true);
        h.env.portMap = PORTMAP_SHARED;
        h.env.portMapWan = "100.64.0.1";
        CHECK(h.session.Host(ONLINE_DEFAULT_PORT, "Alice"));
        h.Run(net, { &cloudflare }, 1);
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_SHARED);
        CHECK(h.session.State().publicAddress[0] == '\0');
    }

    // The first server is silent, the second answers.
    {
        SimNet net(16);
        quiet(net);
        StunResponder cloudflare(&net, 0x08080801, 3478);
        cloudflare.answering = false;
        StunResponder google(&net, 0x08080802, 19302);
        google.mapped = SimAddr(0x56010204, ONLINE_DEFAULT_PORT);
        PublicHost h(&net, true);
        h.env.portMap = PORTMAP_FAILED;
        CHECK(h.session.Host(ONLINE_DEFAULT_PORT, "Alice"));
        h.Run(net, { &cloudflare, &google }, 2.5);
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_LOOKING);
        h.Run(net, { &cloudflare, &google }, 2);
        CHECK(cloudflare.requests == 6 && google.requests == 1);
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_FOUND);
        CHECK(std::strcmp(h.session.State().publicAddress, "86.1.2.4:41210") == 0);
    }

    // Nothing answers.
    {
        SimNet net(17);
        quiet(net);
        PublicHost h(&net, true);
        h.env.portMap = PORTMAP_FAILED;
        CHECK(h.session.Host(ONLINE_DEFAULT_PORT, "Alice"));
        h.Run(net, {}, 8);
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_FAILED);
    }

    // online_public = 0, and a guest: neither looks.
    {
        SimNet net(18);
        quiet(net);
        StunResponder cloudflare(&net, 0x08080801, 3478);
        PublicHost h(&net, false);
        CHECK(h.session.Host(ONLINE_DEFAULT_PORT, "Alice"));
        h.Run(net, { &cloudflare }, 1);
        CHECK(h.session.State().publicState == ONLINE_PUBLIC_NONE && cloudflare.requests == 0);
        CHECK(h.env.portMapPort == 0);
    }
}

void TestWelcomeFromElsewhere()
{
    World w(27);
    Identity id = TestIdentity();
    w.net.alias[0x0A000015] = 0x0A000001;
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "elsewhere");
    Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "elsewhere");
    CHECK(Connect(w, a, b, "10.0.0.21:41210"));
    std::printf("join through a second address: phase %d, error %d\n", b->State().phase,
                b->State().error);
    CHECK(b->State().phase == ONLINE_PHASE_ROOM && b->State().error == ONLINE_ERR_NONE);
}

bool Dropped(Machine* m, uint32_t notices)
{
    return m->State().noticeSerial != notices && m->State().notice == ONLINE_NOTICE_DROPPED_TO_CPU;
}

// A guest freezes for 8 s mid-match, then again at the intro barrier of the next: the others
// count down, nobody drops, and all three finish together each time.
void TestFrozen()
{
    World w(32);
    QuietNet(w);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "frozen");
    Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "frozen");
    Machine* c = w.Add(0x0A000003, 0, 60, 1, id, "frozen");
    for (Machine* m : { a, b, c })
    {
        m->game.matchTicks = 1500;
        m->game.goalOdds = 0;
    }
    CHECK(ConnectThree(w, a, b, c));
    CHECK(StartMatch(w, a, { b, c }));
    CHECK(w.RunUntil(
        [&] { return a->game.tick >= 600 && b->game.tick >= 600 && c->game.tick >= 600; }, 60));
    const uint32_t na = a->State().noticeSerial;
    const uint32_t nb = b->State().noticeSerial;
    std::vector<float> countdown;
    w.onFrame = [&](Machine& m) {
        if (&m == a && m.State().problem == ONLINE_PROBLEM_WAITING)
            countdown.push_back(m.State().problemSeconds);
    };
    c->framesPaused = true;
    w.RunFor(3);
    CHECK(a->State().problem == ONLINE_PROBLEM_WAITING && a->State().problemMachine == 2);
    CHECK(b->State().problem == ONLINE_PROBLEM_WAITING && b->State().problemMachine == 2);
    w.RunFor(5);
    c->framesPaused = false;
    w.onFrame = nullptr;
    CHECK(w.RunUntil(
        [&] {
            return !a->game.results.empty() && !b->game.results.empty() && !c->game.results.empty();
        },
        60));
    bool falling = !countdown.empty();
    for (size_t i = 1; i < countdown.size(); i++)
        falling = falling && countdown[i] <= countdown[i - 1];
    std::printf("frozen: waiting from %.2f s down to %.2f s\n",
                countdown.empty() ? 0.0f : countdown.front(),
                countdown.empty() ? 0.0f : countdown.back());
    // 15 s of freeze plus 6 s of silence, less the second before the warning shows.
    CHECK(!countdown.empty() && countdown.front() > 19.0f && countdown.front() <= 20.1f);
    CHECK(falling && countdown.back() > 12.0f);
    CHECK(!Dropped(a, na) && !Dropped(b, nb));
    if (!a->game.results.empty() && !b->game.results.empty() && !c->game.results.empty())
        CHECK(a->game.results[0].state == b->game.results[0].state &&
              b->game.results[0].state == c->game.results[0].state);

    // The rematch's intro barrier holds while a machine is frozen, and ends once it is back.
    CHECK(w.RunUntil(
        [&] {
            return a->State().phase == ONLINE_PHASE_POST_MATCH &&
                   b->State().phase == ONLINE_PHASE_POST_MATCH &&
                   c->State().phase == ONLINE_PHASE_POST_MATCH;
        },
        10));
    for (Machine* m : { a, b, c })
        CHECK(m->session.Vote(-1, ONLINE_VOTE_REMATCH));
    CHECK(w.RunUntil([&] { return c->game.held && c->game.armedKind == 1; }, 30));
    c->framesPaused = true;
    w.RunFor(8);
    CHECK(a->game.tick <= 3);
    c->framesPaused = false;
    CHECK(
        w.RunUntil([&] { return a->game.results.size() == 2 && c->game.results.size() == 2; }, 60));
    CHECK(!Dropped(a, na));
    if (a->game.results.size() == 2 && c->game.results.size() == 2)
        CHECK(a->game.results[1].state == c->game.results[1].state);
}

// A frozen machine keeps talking for kLoopGiveUpUs, then drops; one that goes silent drops 6 s
// after its last datagram.
void TestStopped()
{
    World w(28);
    QuietNet(w);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "stopped");
    Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "stopped");
    Machine* c = w.Add(0x0A000003, 0, 60, 1, id, "stopped");
    for (Machine* m : { a, b, c })
        m->game.goalOdds = 0;
    CHECK(ConnectThree(w, a, b, c));
    CHECK(StartMatch(w, a, { b, c }));
    CHECK(w.RunUntil([&] { return a->game.tick >= 600; }, 60));
    uint32_t notices = a->State().noticeSerial;
    b->framesPaused = true;
    const uint64_t stopped = w.net.now;
    CHECK(w.RunUntil([&] { return Dropped(a, notices); }, 30));
    const double dropAt = (double)(w.net.now - stopped) / 1e6;
    const uint32_t before = a->game.tick;
    w.RunFor(1);
    std::printf("stopped: frozen loop dropped after %.2f s\n", dropAt);
    CHECK(dropAt > 15.0 && dropAt < 25.0);
    CHECK(a->game.tick > before + 30);

    notices = a->State().noticeSerial;
    c->framesPaused = true;
    w.RunFor(3);
    CHECK(!Dropped(a, notices));
    w.net.silenced.push_back(c->ip);
    c->paused = true;
    const uint64_t last = w.net.lastFrom[c->ip];
    CHECK(w.RunUntil([&] { return Dropped(a, notices); }, 15));
    const double silentAt = (double)(w.net.now - last) / 1e6;
    std::printf("stopped: silent machine dropped %.2f s after its last datagram\n", silentAt);
    CHECK(silentAt >= 6.0 && silentAt <= 6.2);
}

// A skip from either side reaches the other, and an old skip cannot affect a later cutscene.
void TestSharedSkip()
{
    World w(47);
    QuietNet(w);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "shared-skip");
    Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "shared-skip");
    for (Machine* m : { a, b })
    {
        m->game.matchTicks = 1800;
        m->game.goalOdds = 0;
    }
    CHECK(Connect(w, a, b));
    CHECK(StartMatch(w, a, { b }));
    CHECK(w.RunUntil([&] { return a->game.tick >= 300 && b->game.tick >= 300; }, 60));
    w.net.params.loss = 0.15;
    w.net.params.duplicate = 0.1;
    CHECK(b->session.PresentationSkip(250, true));
    CHECK(w.RunUntil([&] { return a->session.PresentationSkip(250, false); }, 5));
    CHECK(!a->session.PresentationSkip(251, false));
    CHECK(a->session.PresentationSkip(251, true));
    CHECK(w.RunUntil([&] { return b->session.PresentationSkip(251, false); }, 5));
    CHECK(!b->session.PresentationSkip(250, true));
    CHECK(!a->session.PresentationSkip(0, true));
    CHECK(!a->session.PresentationSkip(999999, true));
    CHECK(w.RunUntil([&] { return !a->game.results.empty() && !b->game.results.empty(); }, 60));
    if (!a->game.results.empty() && !b->game.results.empty())
        CHECK(a->game.results[0].state == b->game.results[0].state);
}

// Team ownership, ready gating, back/cancel and stadium confirmation use the same room state.
void TestRetailSelection()
{
    World w(48);
    QuietNet(w);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "retail-selection");
    Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "retail-selection");
    CHECK(Connect(w, a, b));
    CHECK(a->session.Select(ONLINE_SELECT_BEGIN, 0, 0, 0, 0));
    CHECK(w.RunUntil([&] { return b->State().selection.stage == 1; }, 5));
    const uint32_t epoch = b->State().selection.epoch;
    CHECK(!b->session.Select(ONLINE_SELECT_TEAM, 0, 3, 0, 3));
    CHECK(!b->session.Select(ONLINE_SELECT_FINISH, 0, 0, 0, 0));
    CHECK(!a->session.Select(ONLINE_SELECT_FINISH, 0, 0, 0, 0));
    CHECK(a->session.Select(ONLINE_SELECT_OPTIONS, 0, 0, 0, 1));
    CHECK(a->session.Select(ONLINE_SELECT_TEAM, 0, 4, 2, 3));
    CHECK(b->session.Select(ONLINE_SELECT_TEAM, 1, 2, 1, 3));
    CHECK(w.RunUntil(
        [&] { return a->State().selection.phase[1] == 3 && b->State().selection.optionsOpen; }, 5));
    CHECK(a->State().selection.stage == 1);
    CHECK(!b->session.Select(ONLINE_SELECT_OPTIONS, 0, 0, 0, 0));
    CHECK(a->session.Select(ONLINE_SELECT_OPTIONS, 0, 0, 0, 0));
    CHECK(w.RunUntil([&] { return b->State().selection.stage == 2; }, 5));
    CHECK(b->State().desc.team[0] == 4 && b->State().desc.sidekick[0] == 2);
    CHECK(b->session.Select(ONLINE_SELECT_BACK, 0, 0, 0, 0));
    CHECK(w.RunUntil(
        [&] { return b->State().selection.stage == 1 && b->State().selection.epoch != epoch; }, 5));
    CHECK(b->session.Select(ONLINE_SELECT_CANCEL, 0, 0, 0, 0));
    CHECK(w.RunUntil(
        [&] { return a->State().selection.stage == 0 && b->State().selection.stage == 0; }, 5));
    CHECK(a->session.Select(ONLINE_SELECT_BEGIN, 0, 0, 0, 0));
    CHECK(w.RunUntil([&] { return b->State().selection.stage == 1; }, 5));
    CHECK(a->session.Select(ONLINE_SELECT_TEAM, 0, 4, 2, 3));
    CHECK(b->session.Select(ONLINE_SELECT_TEAM, 1, 2, 1, 3));
    CHECK(w.RunUntil([&] { return b->State().selection.stage == 2; }, 5));
    OnlineMatchDesc d = a->State().desc;
    d.stadium = 4;
    CHECK(a->session.SetDesc(d));
    CHECK(a->session.Select(ONLINE_SELECT_FINISH, 0, 0, 0, 0));
    CHECK(w.RunUntil(
        [&] {
            return a->State().phase == ONLINE_PHASE_MATCH && b->State().phase == ONLINE_PHASE_MATCH;
        },
        30));
    CHECK(b->State().desc.stadium == 4 && b->State().desc.team[0] == 4);
}

// A seed the host's setup carries is used for the next match only.
void TestSeedReplay()
{
    World w(35);
    QuietNet(w);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "seed");
    Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "seed");
    for (Machine* m : { a, b })
    {
        m->game.matchTicks = 200;
        m->game.goalOdds = 0;
    }
    CHECK(Connect(w, a, b));
    OnlineMatchDesc d = a->State().desc;
    d.seed = 0x5487A49Cu;
    CHECK(a->session.SetDesc(d));
    CHECK(StartMatch(w, a, { b }));
    CHECK(w.RunUntil([&] { return b->State().phase == ONLINE_PHASE_MATCH; }, 30));
    const uint32_t first = b->State().desc.seed;
    CHECK(first == 0x5487A49Cu && a->State().desc.seed == first);
    CHECK(w.RunUntil([&] { return !a->game.results.empty() && !b->game.results.empty(); }, 60));
    CHECK(w.RunUntil(
        [&] {
            return a->State().phase == ONLINE_PHASE_POST_MATCH &&
                   b->State().phase == ONLINE_PHASE_POST_MATCH;
        },
        10));
    CHECK(a->session.Vote(-1, ONLINE_VOTE_BACK_TO_ROOM));
    CHECK(b->session.Vote(-1, ONLINE_VOTE_BACK_TO_ROOM));
    CHECK(w.RunUntil(
        [&] {
            return a->State().phase == ONLINE_PHASE_ROOM && b->State().phase == ONLINE_PHASE_ROOM;
        },
        10));
    CHECK(StartMatch(w, a, { b }));
    CHECK(w.RunUntil([&] { return b->State().phase == ONLINE_PHASE_MATCH; }, 30));
    std::printf("seed replay: first %08x, next %08x\n", (unsigned)first,
                (unsigned)b->State().desc.seed);
    CHECK(b->State().desc.seed != first && b->State().desc.seed != 0);
}

// Arms a pause barrier at `at` on each machine, and reads each one's resume actions there.
struct PauseWatch
{
    uint32_t at = 0;
    OnlineTickActions act[3] = {};
};

void ArmPause(World& w, const std::vector<Machine*>& ms, PauseWatch& pw, uint32_t at)
{
    pw = PauseWatch();
    pw.at = at;
    for (Machine* m : ms)
        m->game.ArmFromOutside(4, at);
    w.onFrame = [ms, &pw](Machine& m) {
        for (size_t i = 0; i < ms.size(); i++)
        {
            OnlineTickActions now;
            if (ms[i] == &m && pw.act[i].actions == 0 && m.session.TickActions(pw.at, &now))
                pw.act[i] = now;
        }
    };
}

bool ResumedWithoutReseed(const OnlineTickActions& a)
{
    return (a.actions & ONLINE_ACTION_RESUME) && !(a.actions & ONLINE_ACTION_RESEED) &&
           a.barrier == 4;
}

// Both machines pause at one tick and resume together without a reseed; at the next pause the
// guest holds its menu and leaves, the host drops it, and its own pause alone still keeps the seed.
void TestPause()
{
    World w(34);
    QuietNet(w);
    Identity id = TestIdentity();
    Machine* a = w.Add(0x0A000001, 0, 60, 1, id, "pause");
    Machine* b = w.Add(0x0A000002, 0, 60, 1, id, "pause");
    for (Machine* m : { a, b })
    {
        m->game.matchTicks = 3000;
        m->game.goalOdds = 0;
    }
    CHECK(Connect(w, a, b));
    CHECK(StartMatch(w, a, { b }));
    CHECK(w.RunUntil([&] { return a->game.tick >= 300 && b->game.tick >= 300; }, 60));
    PauseWatch pw;
    ArmPause(w, { a, b }, pw, std::max(a->game.tick, b->game.tick) + 100);
    CHECK(w.RunUntil([&] { return a->game.tick > pw.at + 10 && b->game.tick > pw.at + 10; }, 30));
    w.onFrame = nullptr;
    std::printf("pause at %u: actions %x and %x\n", pw.at, (unsigned)pw.act[0].actions,
                (unsigned)pw.act[1].actions);
    CHECK(ResumedWithoutReseed(pw.act[0]) && pw.act[1].actions == pw.act[0].actions);

    ArmPause(w, { a, b }, pw, std::max(a->game.tick, b->game.tick) + 50);
    // The guest holds its pause menu: its presentation never ends.
    CHECK(w.RunUntil([&] { return a->game.held && b->game.held; }, 10));
    b->game.presentEndUs = ~0ull;
    w.RunFor(2);
    CHECK(a->game.tick <= pw.at);
    const uint32_t notices = a->State().noticeSerial;
    b->session.Leave();
    b->framesPaused = true;
    CHECK(w.RunUntil([&] { return a->game.tick > pw.at + 50; }, 5));
    w.onFrame = nullptr;
    CHECK(Dropped(a, notices) && a->session.SlotKind(1) == ONLINE_SLOT_CPU);
    CHECK(std::strcmp(a->State().noticeName, "Bob") == 0);

    ArmPause(w, { a }, pw, a->game.tick + 50);
    CHECK(w.RunUntil([&] { return a->game.tick > pw.at + 10; }, 30));
    w.onFrame = nullptr;
    std::printf("solo pause at %u: actions %x\n", pw.at, (unsigned)pw.act[0].actions);
    CHECK(ResumedWithoutReseed(pw.act[0]));
}

// Ticks run on a guess at a leaver's input run again with the CPU once the drop arrives.
void TestDropBehindGuess()
{
    online::Lockstep lock;
    const int8_t slots[ONLINE_MAX_SLOTS] = { 0, 1, -1, -1 };
    lock.Begin(1, 0, 0x3, slots);
    lock.StartGeneration();
    SlotInput in[ONLINE_MAX_SLOTS];
    uint8_t rec[1][online::kRecordSize] = {};
    rec[0][16] = 1;
    for (uint32_t t = lock.Have(1); t < 5; t++)
        CHECK(lock.Add(1, t, 1, rec) == 1);
    for (uint32_t t = 0; t < 10; t++)
    {
        lock.Generate(t);
        lock.ReadPredicted(t, in);
    }
    CHECK(lock.TakeMispredicted() == online::kNoTick);
    lock.Drop(1, 7);
    CHECK(lock.TakeMispredicted() == 7);
    lock.ReadPredicted(7, in);
    CHECK(in[1].kind == ONLINE_SLOT_CPU);
}

// A Windows machine joins a Mac's room: every platform plays every other, with no check.
void TestCrossPlatform()
{
    World w(38);
    QuietNet(w);
    Identity mac = TestIdentity();
    Identity win = TestIdentity();
    win.platform = ONLINE_PLATFORM_WINDOWS;
    win.binaryFamily = FAMILY_WINDOWS_X64;
    Machine* a = w.Add(0x0A000001, 0, 60, 1, mac, "cross");
    Machine* b = w.Add(0x0A000002, 0, 60, 1, win, "cross");
    for (Machine* m : { a, b })
    {
        m->game.matchTicks = 600;
        m->game.goalOdds = 0;
    }
    CHECK(Connect(w, a, b));
    CHECK(StartMatch(w, a, { b }));
    CHECK(w.RunUntil([&] { return !a->game.results.empty() && !b->game.results.empty(); }, 60));
    std::printf("cross-platform: %s guest in a %s room, phases %d and %d\n",
                PortOnlinePlatformName(b->State().machines[1].platform),
                PortOnlinePlatformName(a->State().machines[0].platform), a->State().phase,
                b->State().phase);
    CHECK(b->State().machines[1].platform == ONLINE_PLATFORM_WINDOWS);
    if (!a->game.results.empty() && !b->game.results.empty())
        CHECK(a->game.results[0].state == b->game.results[0].state);
}
}   // namespace

int main()
{
    // ONLINE_TEST_ONLY=<name> runs one scenario.
    const char* only = std::getenv("ONLINE_TEST_ONLY");
    auto run = [&](const char* name, void (*fn)()) {
        if (only == NULL || std::strcmp(only, name) == 0)
            fn();
    };
    run("refusals", TestRefusals);
    run("mismatch", TestSetupMismatch);
    run("nearby", TestNearby);
    run("hostile", TestHostileGuest);
    run("flip", TestBitFlip);
    run("silent-guest", [] { TestSilent(false); });
    run("silent-host", [] { TestSilent(true); });
    run("couches", TestCouches);
    run("outsider-desync", TestOutsiderDesync);
    run("input-flood", TestInputFlood);
    run("unjoined", TestUnjoinedLinks);
    run("rebind", TestRebind);
    run("host-of-four", TestHostLostOfFour);
    run("drop-rematch", TestRematchAfterDrop);
    run("no-random", TestNoRandomness);
    run("unacked", TestUnacknowledged);
    run("rematch-leaver", TestRematchLeaver);
    run("early-vote", TestEarlyVote);
    run("barrier-presses", TestBarrierPresses);
    run("welcome-elsewhere", TestWelcomeFromElsewhere);
    run("public-address", TestPublicAddress);
    run("frozen", TestFrozen);
    run("stopped", TestStopped);
    run("shared-skip", TestSharedSkip);
    run("retail-selection", TestRetailSelection);
    run("pause", TestPause);
    run("drop-behind-guess", TestDropBehindGuess);
    run("seed-replay", TestSeedReplay);
    run("cross-platform", TestCrossPlatform);
    run("many", TestManyMatches);
    if (g_failures != 0)
    {
        std::printf("online_lockstep: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("online_lockstep: ok\n");
    return 0;
}
