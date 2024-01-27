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
  netEngine->post([this] { localSocket->close(); });
}

void PortMultiplexer::addRecipient(shared_ptr<EncryptedMultiEndpointHandler> recipient) {
  lock_guard<recursive_mutex> guard(mut);

  // Ban any duplicate endpoints
  auto eps = recipient->aliveEndpoints();
  for (auto ep : eps) {
    if (endpointsSeen.find(ep) != endpointsSeen.end() || ep == receiveEndpoint) {
      // We've seen this endpoint more than once, ban it.
      if (ep == receiveEndpoint) {
        LOG(WARNING) << "Banning endpoint " << ep << " because it matches my recieve endpoint (" << receiveEndpoint << ").";
      } else {
        LOG(WARNING) << "Banning endpoint " << ep << " because two peers have it";
      }
      for (auto r : recipients) {
        r->banEndpoint(ep);
      }
    } else {
      LOG(INFO) << "ENDPOINT LOOKS GOOD: " << ep << " " << receiveEndpoint << endl;
      // Mark it so we ban next time if we see it again.
      endpointsSeen.insert(ep);
    }
  }

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
        }
      }
      if (recipient.get() == NULL && packetContents.size() > 0) {
        // We don't have any endpoint to receive this, so check the header
        LOG(INFO) << "Do not know who should get packet, will check header";
        for (auto& it : recipients) {
          if (it->isEndpointBanned(receiveEndpoint)) {
            continue;
          }
          // Possible match, try it out
          recipient = it;
          recipient->addEndpoint(receiveEndpoint);
          break;
        }
      }
      if (recipient.get() == NULL) {
        LOG(ERROR) << "Could not find receipient";
      } else {
        if (!recipient->receive(packetContents)) {
          recipient->banEndpoint(receiveEndpoint);
        }
      }
    }
  }

  localSocket->async_receive_from(
      asio::buffer(receiveBuffer), receiveEndpoint,
      std::bind(&PortMultiplexer::handleReceive, this, std::placeholders::_1,
                std::placeholders::_2));
}

}  // namespace wga
