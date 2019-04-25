#include "PortMultiplexer.hpp"

namespace wga {
PortMultiplexer::PortMultiplexer(shared_ptr<NetEngine> _netEngine,
                                 shared_ptr<udp::socket> _localSocket)
    : netEngine(_netEngine), localSocket(_localSocket) {
  localSocket->async_receive_from(
      asio::buffer(receiveBuffer), receiveEndpoint,
      std::bind(&PortMultiplexer::handleRecieve, this, std::placeholders::_1,
                std::placeholders::_2));
}

void PortMultiplexer::handleRecieve(const asio::error_code& error,
                                    std::size_t bytesTransferred) {
  lock_guard<recursive_mutex> guard(*netEngine->getMutex());
  VLOG(1) << "GOT PACKET FROM " << receiveEndpoint << " WITH SIZE "
          << bytesTransferred;
  // We need to find out where this needs to go
  shared_ptr<MultiEndpointHandler> recipient;
  for (auto& it : recipients) {
    if (it->hasEndpointAndResurrectIfFound(receiveEndpoint)) {
      recipient = it;
    }
  }
  string packetString(receiveBuffer.data(), bytesTransferred);
  if (recipient.get() == NULL) {
    // We don't have any endpoint to receive this, so drop it.
    LOG(INFO) << "Do not know who should get packet, will check IP addresses";
    for (auto& it : recipients) {
      if (it->hasEndpointWithIp(receiveEndpoint.address())) {
        recipient = it;
        recipient->addEndpoint(receiveEndpoint);
        break;
      }
    }
    recipient = recipients[0];
    recipient->addEndpoint(receiveEndpoint);
  }
  if (recipient.get() == NULL) {
    LOG(ERROR) << "Could not find receipient";
  } else {
    recipient->receive(packetString);
  }

  localSocket->async_receive_from(
      asio::buffer(receiveBuffer), receiveEndpoint,
      std::bind(&PortMultiplexer::handleRecieve, this, std::placeholders::_1,
                std::placeholders::_2));
}

}  // namespace wga