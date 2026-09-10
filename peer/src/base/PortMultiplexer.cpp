#include "PortMultiplexer.hpp"

namespace wga {
PortMultiplexer::PortMultiplexer(shared_ptr<NetEngine> _netEngine,
                                 shared_ptr<udp::socket> _localSocket)
    : netEngine(_netEngine), localSocket(_localSocket) {
  localSocket->async_receive_from(
      asio::buffer(receiveBuffer), receiveEndpoint,
      std::bind(&PortMultiplexer::handleReceive, this, std::placeholders::_1,
                std::placeholders::_2));
}

void PortMultiplexer::closeSocket() {
  auto sock = localSocket;
  netEngine->post([sock] {
    if (sock) {
      asio::error_code ec;
      sock->close(ec);
    }
  });
}

void PortMultiplexer::addRecipient(shared_ptr<EncryptedMultiEndpointHandler> recipient) {
  lock_guard<recursive_mutex> guard(mut);
  recipients.push_back(recipient);
}

void PortMultiplexer::handleReceive(const asio::error_code& error,
                                    std::size_t bytesTransferred) {
  if (error == asio::error::operation_aborted) {
    return;
  }
  if (error.value()) {
    LOG(ERROR) << "Got error when trying to receive packet on "
               << receiveEndpoint << ": " << error.value() << ": "
               << error.message();
    localSocket->async_receive_from(
        asio::buffer(receiveBuffer), receiveEndpoint,
        std::bind(&PortMultiplexer::handleReceive, this, std::placeholders::_1,
                  std::placeholders::_2));
    return;
  }
  lock_guard<recursive_mutex> guard(mut);
  VLOG(2) << "GOT PACKET FROM " << receiveEndpoint << " WITH SIZE "
          << bytesTransferred;
  if (bytesTransferred < WGA_MAGIC.length()) {
    VLOG(2) << "Packet is too small to contain header: " << bytesTransferred;
  } else {
    string packetString(receiveBuffer.data(), bytesTransferred);

    string magicHeader = packetString.substr(0, WGA_MAGIC.length());
    if (magicHeader != WGA_MAGIC) {
      LOG(ERROR) << "Invalid packet header (total size):" << bytesTransferred
                 << " data: " << packetString;
    } else {
      string packetContents = packetString.substr(WGA_MAGIC.length());
      // We need to find out where this needs to go
      shared_ptr<EncryptedMultiEndpointHandler> recipient;
      for (auto& it : recipients) {
        if (it->hasEndpointAndResurrectIfFound(receiveEndpoint)) {
          recipient = it;
          break;
        }
      }
      if (recipient != nullptr) {
        if (!recipient->receive(packetContents)) {
          // Could not decrypt/validate with this recipient; might belong to another peer
          recipient = nullptr;
        }
      }
      if (recipient == nullptr && packetContents.size() > 0) {
        // Try other eligible recipients
        for (auto& it : recipients) {
          if (it->isEndpointBanned(receiveEndpoint)) {
            continue;
          }
          if (it->receive(packetContents)) {
            recipient = it;
            recipient->addEndpoint(receiveEndpoint);
            break;
          }
        }
      }
      if (recipient == nullptr) {
        VLOG(2) << "No recipient could handle packet from " << receiveEndpoint;
      }
    }
  }

  localSocket->async_receive_from(
      asio::buffer(receiveBuffer), receiveEndpoint,
      std::bind(&PortMultiplexer::handleReceive, this, std::placeholders::_1,
                std::placeholders::_2));
}

}  // namespace wga
