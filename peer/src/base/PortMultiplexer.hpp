#ifndef __PORT_MULTIPLEXER_HPP__
#define __PORT_MULTIPLEXER_HPP__

#include "Headers.hpp"
#include "MultiEndpointHandler.hpp"
#include "NetEngine.hpp"
#include "UdpRecipient.hpp"

namespace wga {
class PortMultiplexer {
 public:
  PortMultiplexer(shared_ptr<NetEngine> _netEngine,
                  shared_ptr<udp::socket> _localSocket);

  shared_ptr<udp::socket> getLocalSocket() { return localSocket; }

  void addRecipient(shared_ptr<UdpRecipient> recipient) {
    recipients.push_back(recipient);
  }

 protected:
  void handleRecieve(const asio::error_code& error,
                     std::size_t bytesTransferred);

  shared_ptr<NetEngine> netEngine;
  shared_ptr<udp::socket> localSocket;
  vector<shared_ptr<UdpRecipient>> recipients;

  udp::endpoint receiveEndpoint;
  std::array<char, 1024 * 1024> receiveBuffer;
};
}  // namespace wga

#endif