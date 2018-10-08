#include "PortMultiplexer.hpp"

namespace wga {
PortMultiplexer::PortMultiplexer(shared_ptr<asio::io_service> _ioService,
                                 shared_ptr<mutex> _ioServiceMutex,
                                 shared_ptr<udp::socket> _localSocket)
    : ioService(_ioService),
      ioServiceMutex(_ioServiceMutex),
      localSocket(_localSocket) {
  localSocket->async_receive_from(
      asio::buffer(receiveBuffer), receiveEndpoint,
      std::bind(&PortMultiplexer::handleRecieve, this, std::placeholders::_1,
                std::placeholders::_2));
}

void PortMultiplexer::handleRecieve(const asio::error_code& error,
                                    std::size_t bytesTransferred) {
  lock_guard<std::mutex> guard(*ioServiceMutex);
  LOG(INFO) << "GOT PACKET FROM " << receiveEndpoint << " WITH SIZE "
            << bytesTransferred;
  if (bytesTransferred != 68 && bytesTransferred != 18 &&
      bytesTransferred != 1) {
    LOG(FATAL) << "INVALID SIZE: " << bytesTransferred << " "
               << receiveBuffer.data()[bytesTransferred - 1];
  }
  // We need to find out where this needs to go
  shared_ptr<MultiEndpointHandler> recipient;
  for (auto& it : endpointHandlers) {
    if (it->hasEndpointAndResurrectIfFound(receiveEndpoint)) {
      recipient = it;
    }
  }
  if (recipient.get() == NULL) {
    // We don't have any endpoint to receive this, so drop it.
    return;
  }

  string packetString(receiveBuffer.data(), bytesTransferred);
  recipient->receive(packetString);

  localSocket->async_receive_from(
      asio::buffer(receiveBuffer), receiveEndpoint,
      std::bind(&PortMultiplexer::handleRecieve, this, std::placeholders::_1,
                std::placeholders::_2));
}

}  // namespace wga