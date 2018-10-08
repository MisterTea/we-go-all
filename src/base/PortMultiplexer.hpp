#ifndef __PORT_MULTIPLEXER_HPP__
#define __PORT_MULTIPLEXER_HPP__

#include "Headers.hpp"
#include "MultiEndpointHandler.hpp"

namespace wga {
class PortMultiplexer {
 public:
  PortMultiplexer(shared_ptr<asio::io_service> _ioService,
                  shared_ptr<udp::socket> _localSocket);

  void handleRecieve(const asio::error_code& error,
                     std::size_t bytesTransferred);

  void addEndpointHandler(shared_ptr<MultiEndpointHandler> endpointHandler) {
    endpointHandlers.push_back(endpointHandler);
  }

 protected:
  shared_ptr<asio::io_service> ioService;
  shared_ptr<udp::socket> localSocket;
  vector<shared_ptr<MultiEndpointHandler>> endpointHandlers;

  udp::endpoint receiveEndpoint;
  std::array<char, 1024 * 1024> receiveBuffer;
};
}  // namespace wga

#endif
