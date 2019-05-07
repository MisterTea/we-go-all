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
  if (error.value()) {
    LOG(ERROR) << "Got error when trying to receive packet: " << error.value()
               << ": " << error.message();
    return;
  }
  VLOG(1) << "GOT PACKET FROM " << receiveEndpoint << " WITH SIZE "
          << bytesTransferred;
  if (bytesTransferred <= WGA_MAGIC.length()) {
    LOG(ERROR) << "Packet is too small to contain header: " << bytesTransferred;
  } else {
    string packetString(receiveBuffer.data(), bytesTransferred);

    string magicHeader = packetString.substr(0, WGA_MAGIC.length());
    if (magicHeader != WGA_MAGIC) {
      LOG(ERROR) << "Invalid packet header";
    } else {
      string packetContents = packetString.substr(WGA_MAGIC.length());
      // We need to find out where this needs to go
      shared_ptr<EncryptedMultiEndpointHandler> recipient;
      for (auto& it : recipients) {
        if (it->hasEndpointAndResurrectIfFound(receiveEndpoint)) {
          recipient = it;
        }
      }
      if (recipient.get() == NULL) {
        // We don't have any endpoint to receive this, so check the header
        LOG(INFO) << "Do not know who should get packet, will check header";
        for (auto& it : recipients) {
          if (it->canDecodeMagicHeader(magicHeader, packetContents)) {
            recipient = it;
            recipient->addEndpoint(receiveEndpoint);
            break;
          }
        }
      }
      if (recipient.get() == NULL) {
        LOG(ERROR) << "Could not find receipient";
      } else {
        recipient->receive(packetContents);
      }
    }
  }

  localSocket->async_receive_from(
      asio::buffer(receiveBuffer), receiveEndpoint,
      std::bind(&PortMultiplexer::handleRecieve, this, std::placeholders::_1,
                std::placeholders::_2));
}

}  // namespace wga