#pragma once

#include "Headers.hpp"

#ifdef __cplusplus
extern "C" {
#endif

struct UPNPDev;
struct UPNPUrls;
struct IGDdatas;

#ifdef __cplusplus
}
#endif

namespace wga {
class PortMappingHandler {
 public:
  PortMappingHandler();

  virtual ~PortMappingHandler();

  int mapPort(int destinationPort, const string& description);

  void unmapPort(int sourcePort);

 protected:
  vector<int> mappedExternalPorts;
  random_device generator;
  uniform_int_distribution<int> sourcePortDistribution;
  UPNPDev* upnpDevice;
  string lanAddress;
  string wanAddress;
  shared_ptr<UPNPUrls> upnp_urls;
  shared_ptr<IGDdatas> upnp_data;
};
}  // namespace wga
