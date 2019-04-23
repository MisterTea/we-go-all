#pragma once

#include "Headers.hpp"

#include <miniupnpc.h>
#include <upnpcommands.h>

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
  UPNPUrls upnp_urls;
  IGDdatas upnp_data;
};
}  // namespace wga
