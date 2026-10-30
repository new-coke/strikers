// An in-memory network with delay, jitter, loss, reordering and duplication, and machines on it.

#ifndef ONLINE_NETSIM_H
#define ONLINE_NETSIM_H

#include "../src/platform/online/session.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <queue>
#include <random>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace online_test
{
using namespace online;

struct NetParams
{
    double oneWayMs;    // mean one-way delay
    double jitterMs;    // its standard deviation
    double loss;        // chance a datagram is lost
    double duplicate;   // chance a datagram arrives twice
};

class SimNet;
inline NetAddr SimAddr(uint32_t ip, uint16_t port);

class SimPipe : public NetPipe
{
  public:
    SimPipe(SimNet* net, const NetAddr& self) : m_net(net), m_self(self) {}
    ~SimPipe() override;
    int Send(const NetAddr& to, const uint8_t* data, size_t size) override;
    int Receive(NetAddr* from, uint8_t* data, size_t cap) override
    {
        if (m_inbox.empty())
            return -1;
        const Packet& p = m_inbox.front();
        const size_t n = p.data.size() < cap ? p.data.size() : cap;
        std::memcpy(data, p.data.data(), n);
        *from = p.from;
        m_inbox.pop_front();
        return (int)n;
    }
    struct Packet
    {
        NetAddr from;
        std::vector<uint8_t> data;
    };
    std::deque<Packet> m_inbox;
    SimNet* m_net;
    NetAddr m_self;
};

class SimNet
{
  public:
    explicit SimNet(uint64_t seed) : now(0), rng(seed), m_seq(0)
    {
        params.oneWayMs = 50;
        params.jitterMs = 20;
        params.loss = 0.02;
        params.duplicate = 0.01;
    }

    uint64_t now;
    NetParams params;
    std::mt19937_64 rng;
    // Datagrams from or to these addresses vanish, as a machine that went silent.
    std::vector<uint32_t> silenced;
    // A second address of a machine: datagrams to the key arrive at the value's sockets.
    std::map<uint32_t, uint32_t> alias;
    std::map<uint32_t, uint64_t> lastFrom;   // IPv4 address to the time its last datagram arrived
    uint64_t sent = 0;
    uint64_t lost = 0;

    static uint32_t Ip(const NetAddr& a)
    {
        return (uint32_t)a.ip[0] << 24 | a.ip[1] << 16 | a.ip[2] << 8 | a.ip[3];
    }

    void Bind(SimPipe* p) { m_bound[Key(p->m_self)] = p; }
    void Unbind(SimPipe* p)
    {
        auto it = m_bound.find(Key(p->m_self));
        if (it != m_bound.end() && it->second == p)
            m_bound.erase(it);
    }
    bool Bound(const NetAddr& a) const { return m_bound.count(Key(a)) != 0; }

    void Send(const NetAddr& from, const NetAddr& to, const uint8_t* data, size_t size)
    {
        sent++;
        for (uint32_t s : silenced)
            if (Ip(from) == s || Ip(to) == s)
                return;
        std::uniform_real_distribution<double> u(0, 1);
        if (u(rng) < params.loss)
        {
            lost++;
            return;
        }
        const int copies = u(rng) < params.duplicate ? 2 : 1;
        for (int c = 0; c < copies; c++)
        {
            std::normal_distribution<double> d(params.oneWayMs, params.jitterMs);
            double ms = d(rng);
            if (ms < 0.2)
                ms = 0.2;
            Flight f;
            f.at = now + (uint64_t)(ms * 1000);
            f.seq = m_seq++;
            f.from = from;
            f.to = to;
            f.data.assign(data, data + size);
            m_flights.push(f);
        }
    }

    uint64_t NextDelivery() const { return m_flights.empty() ? ~0ull : m_flights.top().at; }

    // Hands every datagram due by now to its socket; returns the addresses that got one.
    void Deliver(std::vector<uint32_t>* touched)
    {
        while (!m_flights.empty() && m_flights.top().at <= now)
        {
            Flight f = m_flights.top();
            m_flights.pop();
            bool dead = false;
            for (uint32_t s : silenced)
                if (Ip(f.from) == s || Ip(f.to) == s)
                    dead = true;
            if (dead)
                continue;
            if (Ip(f.to) == 0xFFFFFFFFu)
            {
                for (auto& b : m_bound)
                    if ((uint16_t)b.first == f.to.port)
                        Push(b.second, f, touched);
                continue;
            }
            NetAddr to = f.to;
            auto a = alias.find(Ip(to));
            if (a != alias.end())
                to = SimAddr(a->second, to.port);
            auto it = m_bound.find(Key(to));
            if (it == m_bound.end())
                continue;
            Push(it->second, f, touched);
        }
    }

  private:
    struct Flight
    {
        uint64_t at;
        uint64_t seq;
        NetAddr from;
        NetAddr to;
        std::vector<uint8_t> data;
        bool operator<(const Flight& o) const { return at != o.at ? at > o.at : seq > o.seq; }
    };

    static uint64_t Key(const NetAddr& a) { return (uint64_t)Ip(a) << 16 | a.port; }

    void Push(SimPipe* p, const Flight& f, std::vector<uint32_t>* touched)
    {
        SimPipe::Packet pk;
        pk.from = f.from;
        pk.data = f.data;
        p->m_inbox.push_back(pk);
        lastFrom[Ip(f.from)] = now;
        if (touched != NULL)
            touched->push_back(Ip(p->m_self));
    }

    std::priority_queue<Flight> m_flights;
    std::map<uint64_t, SimPipe*> m_bound;
    uint64_t m_seq;
};

inline SimPipe::~SimPipe() { m_net->Unbind(this); }

inline int SimPipe::Send(const NetAddr& to, const uint8_t* data, size_t size)
{
    // An IPv4 network, as a socket that fell back from dual stack.
    if (to.family != 4)
        return -1;
    m_net->Send(m_self, to, data, size);
    return 0;
}

inline NetAddr SimAddr(uint32_t ip, uint16_t port)
{
    NetAddr a;
    std::memset(&a, 0, sizeof a);
    a.family = 4;
    a.ip[0] = (uint8_t)(ip >> 24);
    a.ip[1] = (uint8_t)(ip >> 16);
    a.ip[2] = (uint8_t)(ip >> 8);
    a.ip[3] = (uint8_t)ip;
    a.port = port;
    return a;
}

inline void MakeDir(const std::string& path)
{
#if defined(_WIN32)
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0755);
#endif
}

class SimEnv : public SessionEnv
{
  public:
    SimEnv(SimNet* net, uint32_t ip, uint64_t seed, double drift, uint64_t offsetUs,
           const std::string& dir)
        : m_net(net), m_ip(ip), m_rng(seed), m_drift(drift), m_offset(offsetUs), m_dir(dir),
          m_nextPort(50000), verbose(false), reports(0)
    {}

    uint64_t NowUs() override
    {
        return m_offset + (uint64_t)((double)m_net->now * (1.0 + m_drift));
    }

    // Global time at which this machine's clock reads `local`.
    uint64_t GlobalAt(uint64_t local) const
    {
        if (local <= m_offset)
            return 0;
        return (uint64_t)std::ceil((double)(local - m_offset) / (1.0 + m_drift));
    }

    NetPipe* OpenPipe(int purpose, uint16_t port, int* error) override
    {
        (void)purpose;
        if (port == 0)
            port = m_nextPort++;
        NetAddr a = SimAddr(m_ip, port);
        if (m_net->Bound(a))
        {
            *error = PIPE_ERR_IN_USE;
            return NULL;
        }
        SimPipe* p = new SimPipe(m_net, a);
        m_net->Bind(p);
        pipes.push_back(p);
        *error = PIPE_OK;
        return p;
    }

    void ClosePipe(NetPipe* pipe) override
    {
        for (size_t i = 0; i < pipes.size(); i++)
            if (pipes[i] == pipe)
                pipes.erase(pipes.begin() + (long)i);
        delete pipe;
    }

    uint16_t PipePort(NetPipe* pipe) override { return static_cast<SimPipe*>(pipe)->m_self.port; }

    void StartResolve(const char* host, uint16_t port) override
    {
        auto it = names.find(host);
        m_resolved = std::string(host) == "host-a" || it != names.end() ? 1 : -1;
        m_resolvedAddr = SimAddr(it != names.end() ? it->second : 0x0A000001, port);
    }

    void StartPortMap(uint16_t port) override
    {
        portMapPort = port;
        portMapStops = 0;
    }
    int PollPortMap(char* wan, size_t size) override
    {
        std::snprintf(wan, size, "%s", portMapWan.c_str());
        return portMap;
    }
    void StopPortMap() override { portMapStops++; }

    int PollResolve(NetAddr* out) override
    {
        if (m_resolved > 0)
            *out = m_resolvedAddr;
        return m_resolved;
    }

    int BroadcastTargets(NetAddr* out, int max, uint16_t port) override
    {
        if (max < 1)
            return 0;
        out[0] = SimAddr(0xFFFFFFFFu, port);
        return 1;
    }

    bool LanAddress(char* out, size_t size) override
    {
        std::snprintf(out, size, "%u.%u.%u.%u", m_ip >> 24, m_ip >> 16 & 255, m_ip >> 8 & 255,
                      m_ip & 255);
        return true;
    }

    bool Random(void* buf, size_t size) override
    {
        if (randomFails)
            return false;
        uint8_t* p = (uint8_t*)buf;
        for (size_t i = 0; i < size; i++)
            p[i] = (uint8_t)m_rng();
        return true;
    }

    bool ReportPath(char* out, size_t size) override
    {
        MakeDir(m_dir);
        std::snprintf(out, size, "%s/desync-%u-%d.txt", m_dir.c_str(), m_ip & 255, reports++);
        return true;
    }

    void Log(const char* line) override
    {
        if (verbose)
            std::fprintf(stderr, "[%u] %s\n", m_ip & 255, line);
    }

    bool HasInput() const
    {
        for (size_t i = 0; i < pipes.size(); i++)
            if (!pipes[i]->m_inbox.empty())
                return true;
        return false;
    }

    std::vector<SimPipe*> pipes;

  private:
    SimNet* m_net;
    uint32_t m_ip;
    std::mt19937_64 m_rng;

  public:
    // The system's random source fails, as when the OS has none.
    bool randomFails = false;
    double m_drift;
    uint64_t m_offset;
    std::string m_dir;
    uint16_t m_nextPort;
    int m_resolved = -1;
    NetAddr m_resolvedAddr;
    // Names StartResolve knows besides host-a, and the router's answer to a port mapping.
    std::map<std::string, uint32_t> names;
    int portMap = PORTMAP_NONE;
    std::string portMapWan;
    uint16_t portMapPort = 0;
    int portMapStops = 0;

  public:
    bool verbose;
    int reports;
};
}   // namespace online_test

#endif   // ONLINE_NETSIM_H
