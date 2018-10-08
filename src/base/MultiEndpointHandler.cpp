#include "MultiEndpointHandler.hpp"

namespace wga {
MultiEndpointHandler::MultiEndpointHandler(
    shared_ptr<udp::socket> _localSocket,
    shared_ptr<asio::io_service> _ioService,
    shared_ptr<CryptoHandler> _cryptoHandler, const udp::endpoint& endpoint)
    : BiDirectionalRpc(),
      localSocket(_localSocket),
      ioService(_ioService),
      cryptoHandler(_cryptoHandler),
      activeEndpoint(endpoint),
      lastUpdateTime(time(NULL)),
      lastUnrepliedSendTime(0) {
  if (!cryptoHandler->canDecrypt() || !cryptoHandler->canEncrypt()) {
    LOG(FATAL) << "Created endpoint handler before we could encrypt/decrypt: "
               << cryptoHandler->canDecrypt() << " "
               << cryptoHandler->canEncrypt();
  }
}

void MultiEndpointHandler::send(const string& message) {
  if (lastUnrepliedSendTime == 0) {
    lastUnrepliedSendTime = time(NULL);
  }

  if (time(NULL) != lastUpdateTime) {
    update();
  }

  ioService->post([this, message]() {
    LOG(INFO) << "IN SEND LAMBDA";
    int bytesSent = localSocket->send_to(asio::buffer(message), activeEndpoint);
    LOG(INFO) << bytesSent << " bytes sent";
  });
  if (lastUnrepliedSendTime == 0) {
    lastUnrepliedSendTime = time(NULL);
  }
}

bool MultiEndpointHandler::hasEndpointAndResurrectIfFound(
    const udp::endpoint& endpoint) {
  if (endpoint == activeEndpoint) {
    return true;
  }
  for (auto it = alternativeEndpoints.begin(); it != alternativeEndpoints.end();
       it++) {
    if (*it == endpoint) {
      return true;
    }
  }
  for (auto it = deadEndpoints.begin(); it != deadEndpoints.end(); it++) {
    if (*it == endpoint) {
      deadEndpoints.erase(it);
      alternativeEndpoints.insert(*it);
      return true;
    }
  }
  return false;
}

void MultiEndpointHandler::requestWithId(const IdPayload& idPayload) {
  IdPayload encryptedIdPayload =
      IdPayload(idPayload.id, cryptoHandler->encrypt(idPayload.payload));
  BiDirectionalRpc::requestWithId(encryptedIdPayload);
}

void MultiEndpointHandler::reply(const RpcId& rpcId, const string& payload) {
  string encryptedPayload = cryptoHandler->encrypt(payload);
  BiDirectionalRpc::reply(rpcId, encryptedPayload);
}

void MultiEndpointHandler::addIncomingRequest(const IdPayload& idPayload) {
  IdPayload decryptedIdPayload =
      IdPayload(idPayload.id, cryptoHandler->decrypt(idPayload.payload));
  LOG(INFO) << "GOT REQUEST WITH PAYLOAD: " << decryptedIdPayload.payload;
  BiDirectionalRpc::addIncomingRequest(decryptedIdPayload);
}

void MultiEndpointHandler::addIncomingReply(const RpcId& uid,
                                            const string& payload) {
  string decryptedPayload = cryptoHandler->decrypt(payload);
  LOG(INFO) << "GOT REPLY WITH PAYLOAD: " << decryptedPayload;
  BiDirectionalRpc::addIncomingReply(uid, decryptedPayload);
}

void MultiEndpointHandler::update() {
  if (lastUnrepliedSendTime == 0) {
    // Nothing to do
    return;
  }

  if (lastUnrepliedSendTime + 5 < time(NULL)) {
    // We haven't got anything back for 5 seconds
    deadEndpoints.insert(activeEndpoint);
    if (!alternativeEndpoints.empty()) {
      DRAW_FROM_UNORDERED(it, alternativeEndpoints);
      activeEndpoint = *it;
      alternativeEndpoints.erase(it);
    } else {
      // We have no alternatives, try a dead endpoint
      DRAW_FROM_UNORDERED(it, deadEndpoints);
      activeEndpoint = *it;
      deadEndpoints.erase(it);
    }
  }

  lastUnrepliedSendTime = 0;
  lastUpdateTime = time(NULL);
}

}  // namespace wga