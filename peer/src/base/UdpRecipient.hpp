#ifndef __UDP_RECIPIENT_H__
#define __UDP_RECIPIENT_H__

#include "Headers.hpp"

namespace wga {
class UdpRecipient {
 public:
  virtual bool hasEndpointAndResurrectIfFound(
      const udp::endpoint& endpoint) = 0;
  virtual void receive(const string& packetString) = 0;
};
}  // namespace wga

#endif
