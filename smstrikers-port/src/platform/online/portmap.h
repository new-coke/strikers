// A router port mapping over UPnP IGD, on a thread of its own because every step blocks.

#ifndef PORT_ONLINE_PORTMAP_H
#define PORT_ONLINE_PORTMAP_H

#include <cstddef>
#include <cstdint>
#include <memory>

namespace online
{
struct PortMapJob;

class PortMap
{
  public:
    ~PortMap();
    // Maps UDP `port` to this machine once the previous mapping is gone, renewing it until Stop.
    void Start(uint16_t port);
    // PORTMAP_*, with the router's outside IPv4 once open or shared.
    int Poll(char* wan, size_t size);
    // Removes the mapping on the worker without waiting.
    void Stop();

  private:
    std::shared_ptr<PortMapJob> m_job;
};
}   // namespace online

#endif   // PORT_ONLINE_PORTMAP_H
