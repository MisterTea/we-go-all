#include "PortMappingHandler.hpp"

namespace wga {
PortMappingHandler::PortMappingHandler()
    : sourcePortDistribution(24000, 25000), upnpDevice(NULL) {
  int error = 0;
  // get a list of upnp devices (asks on the broadcast address and returns the
  // responses)
  upnpDevice = upnpDiscover(
      1000,     // timeout in milliseconds
      NULL,     // multicast address, default = "239.255.255.250"
      NULL,     // minissdpd socket, default = "/var/run/minissdpd.sock"
      0,        // source port, default = 1900
      0,        // 0 = IPv4, 1 = IPv6
      2,        // Timeout
      &error);  // error output

  if (error) {
    LOGFATAL << "Error discovering UPNP devices: " << error;
  }

  if (upnpDevice == NULL) {
    LOG(ERROR) << "Could not find any upnp devices";
    return;
  }

  // Get number of devices
  int deviceCount = 0;
  auto dev = upnpDevice;
  while (dev) {
    deviceCount++;
    dev = dev->pNext;
  }

  LOG(INFO) << "DEVICE COUNT: " << deviceCount;

  char lanAddressArray[4096];
  upnp_urls.reset(new UPNPUrls());
  int status = UPNP_GetValidIGD(upnpDevice, upnp_urls.get(), &upnp_data,
                                lanAddressArray, sizeof(lanAddressArray));
  lanAddress = string(lanAddressArray);

  if (status != 1) {  // there are more status codes in minupnpc.c but 1 is
                      // success all others are different failures
    LOG(ERROR) << "No valid Internet Gateway Device could be connected to: "
               << status;
    FreeUPNPUrls(upnp_urls.get());
    upnp_urls.reset();
    freeUPNPDevlist(upnpDevice);
    upnpDevice = NULL;
    return;
  }

  // get the external (WAN) IP address
  char wanAddressArray[4096];
  if (UPNP_GetExternalIPAddress(upnp_urls->controlURL,
                                upnp_data.first.servicetype,
                                wanAddressArray) != 0) {
    LOG(ERROR) << "Could not get external IP address";
    FreeUPNPUrls(upnp_urls.get());
    upnp_urls.reset();
    freeUPNPDevlist(upnpDevice);
    upnpDevice = NULL;
    return;
  } else {
    LOG(INFO) << "External IP: " << wanAddressArray;
  }
  wanAddress = string(wanAddressArray);
}

PortMappingHandler::~PortMappingHandler() {
  auto tmpSourcePorts = mappedExternalPorts;
  for (auto it : tmpSourcePorts) {
    unmapPort(it);
  }
  if (upnp_urls.get()) {
    FreeUPNPUrls(upnp_urls.get());
    upnp_urls.reset();
  }
  if (upnpDevice) {
    freeUPNPDevlist(upnpDevice);
  }
}

int PortMappingHandler::mapPort(int destinationPort,
                                const string& description) {
  if (upnpDevice == NULL) {
    LOG(INFO) << "Tried to map port but init already failed";
    return -1;
  }

  // Check if mapping already exists
  printf(
      "Lan Address\tWAN Port -> LAN Port\tProtocol\tDuration\tEnabled?\tRemote "
      "Host\tDescription\n");
  // list all port mappings
  size_t index;
  for (index = 0;; ++index) {
    char map_wan_port[6] = "";
    char map_lanAddress[16] = "";
    char map_lan_port[6] = "";
    char map_protocol[4] = "";
    char map_description[80] = "";
    char map_mapping_enabled[4] = "";
    char map_remote_host[64] = "";
    char map_lease_duration[16] = "";  // original time, not remaining time :(

    int error = UPNP_GetGenericPortMappingEntry(
        upnp_urls->controlURL, upnp_data.first.servicetype,
        to_string(index).c_str(), map_wan_port, map_lanAddress, map_lan_port,
        map_protocol, map_description, map_mapping_enabled, map_remote_host,
        map_lease_duration);

    if (error == 713) {
      break;  // no more port mappings available
    }

    if (error) {
      LOGFATAL << "Error fetching port mappings: " << error;
    }

    printf("%s\t%s -> %s\t%s\t%s\t%s\t%s\t%s\n", map_lanAddress, map_wan_port,
           map_lan_port, map_protocol, map_lease_duration, map_mapping_enabled,
           map_remote_host, map_description);

    if (stoi(string(map_lan_port)) == destinationPort &&
        string(map_lanAddress) == lanAddress) {
      LOG(INFO) << "Found existing mapping for port with description: "
                << map_description;

      // Found existing mapping, delete it
      int error = UPNP_DeletePortMapping(upnp_urls->controlURL,
                                         upnp_data.first.servicetype,
                                         map_wan_port, map_protocol, NULL);

      if (error) {
        LOGFATAL << "Failed to unmap ports: " << error;
      }
    }
  }

  // Monte carlo some source ports
  for (int a = 0; a < 1000; a++) {
    int sourcePort = sourcePortDistribution(generator);

    LOG(INFO) << "TRYING TO ADD PORT MAPPING";
    LOG(INFO) << upnp_urls->controlURL << " / " << upnp_data.first.servicetype
              << " / " << sourcePort << " / " << destinationPort << " / "
              << lanAddress << " / " << description;
    int error = UPNP_AddPortMapping(
        upnp_urls->controlURL, upnp_data.first.servicetype,
        to_string(sourcePort).c_str(), to_string(destinationPort).c_str(),
        lanAddress.c_str(), description.c_str(), "UDP", NULL, "86400");

    if (error == 718) {
      LOG(INFO) << "Mapping conflicts with another mapping.  Will retry...";
      continue;
    }

    if (error) {
      LOGFATAL << "Failed to map ports: " << error;
    }

    error = UPNP_AddPortMapping(
        upnp_urls->controlURL, upnp_data.first.servicetype,
        to_string(sourcePort).c_str(), to_string(destinationPort).c_str(),
        lanAddress.c_str(), description.c_str(), "TCP", NULL, "86400");

    if (error == 718) {
      LOG(INFO) << "Mapping conflicts with another mapping.  Will retry...";
      unmapPort(sourcePort);
      continue;
    }

    if (error) {
      LOGFATAL << "Failed to map ports: " << error;
    }

    LOG(INFO) << "Successfully mapped " << wanAddress << ":" << sourcePort
              << " to " << lanAddress << ":" << destinationPort;
    return sourcePort;
  }

  LOG(INFO) << "Ran out of ports to try";
  return -1;
}

void PortMappingHandler::unmapPort(int sourcePort) {
  if (upnpDevice == NULL) {
    LOG(INFO) << "Tried to unmap port but init already failed";
    return;
  }

  int error =
      UPNP_DeletePortMapping(upnp_urls->controlURL, upnp_data.first.servicetype,
                             to_string(sourcePort).c_str(), "UDP", NULL);

  if (error == 714) {
    LOG(INFO) << "Tried to delete a port mapping that didn't exist (maybe "
                 "timed out?)";
  } else if (error) {
    LOG(ERROR) << "Failed to unmap ports: " << error;
  }

  error =
      UPNP_DeletePortMapping(upnp_urls->controlURL, upnp_data.first.servicetype,
                             to_string(sourcePort).c_str(), "TCP", NULL);

  if (error == 714) {
    LOG(INFO) << "Tried to delete a port mapping that didn't exist (maybe "
                 "timed out?)";
  } else if (error) {
    LOG(ERROR) << "Failed to unmap ports: " << error;
  }
}
}  // namespace wga