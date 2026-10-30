// UPnP IGD port mapping with miniupnpc: discovery, the mapping, its renewal and its removal.

#include "portmap.h"

#include "session.h"

#include <miniupnpc.h>
#include <upnpcommands.h>
#include <upnperrors.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

namespace online
{
// Every member but `prev` is under `lock`.
struct PortMapJob
{
    std::mutex lock;
    std::condition_variable wake;
    bool stop;
    bool finished;
    int state;
    char wan[16];
    std::shared_ptr<PortMapJob> prev;
};

namespace
{
const char* const kDescription = "Strikers";
// Seconds, so a mapping left by a crash expires.
const char* const kLease = "7200";
const std::chrono::minutes kRenewEvery(45);
const int kUpnpOnlyPermanentLeases = 725;
const int kUpnpConflictInMappingEntry = 718;

void log_line(const char* line) { std::fprintf(stderr, "[online] %s\n", line); }

void set_state(PortMapJob& job, int state, const char* wan)
{
    std::lock_guard<std::mutex> g(job.lock);
    job.state = state;
    std::snprintf(job.wan, sizeof job.wan, "%s", wan != NULL ? wan : "");
}

bool stopping(PortMapJob& job)
{
    std::lock_guard<std::mutex> g(job.lock);
    return job.stop;
}

enum
{
    MAPPING_FAILED,
    MAPPING_OURS,      // added by this game, so renewed and removed by it
    MAPPING_BORROWED   // someone else's enabled rule to this machine, never removed
};

// `*error` is the UPnP or miniupnpc error behind a failure.
int add_mapping(const UPNPUrls& urls, const IGDdatas& data, const char* port, const char* lan,
                bool* permanent, int* error)
{
    int r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, port, port, lan,
                                kDescription, "UDP", NULL, *permanent ? "0" : kLease);
    if (r == kUpnpOnlyPermanentLeases && !*permanent)
    {
        *permanent = true;
        r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, port, port, lan,
                                kDescription, "UDP", NULL, "0");
    }
    *error = r;
    if (r == UPNPCOMMAND_SUCCESS)
        return MAPPING_OURS;
    if (r != kUpnpConflictInMappingEntry)
        return MAPPING_FAILED;
    char client[40] = "";
    char inPort[8] = "";
    char desc[80] = "";
    char enabled[8] = "";
    char lease[16] = "";
    if (UPNP_GetSpecificPortMappingEntry(urls.controlURL, data.first.servicetype, port, "UDP", NULL,
                                         client, inPort, desc, enabled,
                                         lease) != UPNPCOMMAND_SUCCESS ||
        std::strcmp(client, lan) != 0 || std::strcmp(inPort, port) != 0 ||
        std::strcmp(enabled, "1") != 0)
        return MAPPING_FAILED;
    // An earlier run's rule carries this game's description.
    return std::strcmp(desc, kDescription) == 0 ? MAPPING_OURS : MAPPING_BORROWED;
}

void run(std::shared_ptr<PortMapJob> job, uint16_t portNumber)
{
    if (job->prev)
    {
        std::unique_lock<std::mutex> g(job->prev->lock);
        job->prev->wake.wait(g, [&] { return job->prev->finished; });
    }
    job->prev.reset();

    char port[8];
    std::snprintf(port, sizeof port, "%u", (unsigned)portNumber);
    char line[160];
    int err = 0;
    UPNPDev* devices =
        stopping(*job) ? NULL : upnpDiscover(2000, NULL, NULL, UPNP_LOCAL_PORT_ANY, 0, 2, &err);
    UPNPUrls urls;
    IGDdatas data;
    std::memset(&urls, 0, sizeof urls);
    std::memset(&data, 0, sizeof data);
    char lan[64] = "";
    char wan[64] = "";
    const int igd = devices != NULL
                        ? UPNP_GetValidIGD(devices, &urls, &data, lan, sizeof lan, wan, sizeof wan)
                        : UPNP_NO_IGD;
    freeUPNPDevlist(devices);

    int mapping = MAPPING_FAILED;
    bool permanent = false;
    // Some routers report themselves disconnected, with outside address 0.0.0.0, and still forward.
    if ((igd == UPNP_CONNECTED_IGD || igd == UPNP_DISCONNECTED_IGD) && !stopping(*job))
    {
        int r = 0;
        mapping = add_mapping(urls, data, port, lan, &permanent, &r);
        if (mapping != MAPPING_FAILED)
        {
            char ext[40] = "";
            if (UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, ext) ==
                UPNPCOMMAND_SUCCESS)
                std::snprintf(wan, sizeof wan, "%s", ext);
            if (std::strcmp(wan, "0.0.0.0") == 0)
                wan[0] = '\0';
            set_state(*job, PORTMAP_OPEN, wan);
        }
        else
        {
            std::snprintf(line, sizeof line, "the router refused to forward port %s: %d %s", port,
                          r, strupnperror(r) != NULL ? strupnperror(r) : "");
            log_line(line);
            set_state(*job, PORTMAP_FAILED, NULL);
        }
    }
    else if (igd == UPNP_PRIVATEIP_IGD)
        set_state(*job, PORTMAP_SHARED, wan);
    else
    {
        if (!stopping(*job))
            log_line(igd == UPNP_NO_IGD ? "no UPnP router answered"
                                        : "the UPnP device isn't an internet gateway");
        set_state(*job, PORTMAP_FAILED, NULL);
    }

    if (mapping == MAPPING_OURS)
    {
        std::unique_lock<std::mutex> g(job->lock);
        while (!job->stop)
        {
            if (job->wake.wait_for(g, kRenewEvery, [&] { return job->stop; }))
                break;
            if (permanent)
                continue;
            g.unlock();
            int r = 0;
            const int renewed = add_mapping(urls, data, port, lan, &permanent, &r);
            g.lock();
            if (renewed != MAPPING_OURS)
            {
                std::snprintf(line, sizeof line, "the router refused to renew port %s: %d", port,
                              r);
                log_line(line);
                job->state = PORTMAP_FAILED;
                job->wan[0] = '\0';
                break;
            }
        }
        g.unlock();
        UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port, "UDP", NULL);
    }
    FreeUPNPUrls(&urls);
    std::lock_guard<std::mutex> g(job->lock);
    job->finished = true;
    job->wake.notify_all();
}
}   // namespace

PortMap::~PortMap()
{
    if (!m_job)
        return;
    Stop();
    // A second for the router to remove the mapping; the lease covers the rest.
    std::unique_lock<std::mutex> g(m_job->lock);
    m_job->wake.wait_for(g, std::chrono::seconds(1), [&] { return m_job->finished; });
}

void PortMap::Start(uint16_t port)
{
    Stop();
    std::shared_ptr<PortMapJob> job(new PortMapJob);
    job->stop = false;
    job->finished = false;
    job->state = PORTMAP_PENDING;
    job->wan[0] = '\0';
    job->prev = m_job;
    m_job = job;
    std::thread(run, job, port).detach();
}

int PortMap::Poll(char* wan, size_t size)
{
    if (!m_job)
        return PORTMAP_NONE;
    std::lock_guard<std::mutex> g(m_job->lock);
    std::snprintf(wan, size, "%s", m_job->wan);
    return m_job->state;
}

void PortMap::Stop()
{
    if (!m_job)
        return;
    std::lock_guard<std::mutex> g(m_job->lock);
    m_job->stop = true;
    m_job->wake.notify_all();
}
}   // namespace online
