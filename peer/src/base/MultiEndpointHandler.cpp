#include "MultiEndpointHandler.hpp"

namespace wga {
MultiEndpointHandler::MultiEndpointHandler(
    shared_ptr<udp::socket> _localSocket, shared_ptr<NetEngine> _netEngine,
    shared_ptr<CryptoHandler> _cryptoHandler,
    const vector<udp::endpoint>& endpoints)
    : BiDirectionalRpc(),
      localSocket(_localSocket),
      netEngine(_netEngine),
      cryptoHandler(_cryptoHandler),
      lastUpdateTime(time(NULL)),
      lastUnrepliedSendTime(0) {
  if (cryptoHandler->canDecrypt() || cryptoHandler->canEncrypt()) {
    LOG(FATAL) << "Created endpoint handler with session key";
  }

  if (endpoints.empty()) {
    LOG(FATAL) << "Passed an empty endpoints array";
  }

  activeEndpoint = endpoints[0];
  for (int a = 1; a < endpoints.size(); a++) {
    alternativeEndpoints.insert(endpoints[a]);
  }

  // Send session key as a one-way rpc
  {
    IdPayload idPayload;
    EncryptedSessionKey encryptedSessionKey =
        cryptoHandler->generateOutgoingSessionKey();
    FATAL_IF_FALSE(
        Base64::Encode(string((const char*)encryptedSessionKey.data(),
                              encryptedSessionKey.size()),
                       &idPayload.payload));
    LOG(INFO) << idPayload.payload;
    idPayload.id = RpcId(0, 1);
    oneWayRequests.insert(idPayload.id);
    BiDirectionalRpc::requestWithId(idPayload);
  }
}

void MultiEndpointHandler::send(const string& message) {
  if (lastUnrepliedSendTime == 0) {
    lastUnrepliedSendTime = time(NULL);
  }

  if (time(NULL) != lastUpdateTime) {
    update();
  }

  netEngine->getIoService()->post([this, message]() {
    LOG(INFO) << "IN SEND LAMBDA: " << message.length();
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
  if (!ready()) {
    LOG(FATAL) << "Tried to send data before we were ready";
  }
  IdPayload encryptedIdPayload =
      IdPayload(idPayload.id, cryptoHandler->encrypt(idPayload.payload));
  BiDirectionalRpc::requestWithId(encryptedIdPayload);
}

void MultiEndpointHandler::reply(const RpcId& rpcId, const string& payload) {
  if (!ready()) {
    LOG(FATAL) << "Got reply before we were ready, something went wrong";
  }
  string encryptedPayload = cryptoHandler->encrypt(payload);
  BiDirectionalRpc::reply(rpcId, encryptedPayload);
}

void MultiEndpointHandler::addIncomingRequest(const IdPayload& idPayload) {
  if (idPayload.id.id == 1) {
    // Handshaking
    LOG(INFO) << "GOT HANDSHAKE";
    bool result = cryptoHandler->recieveIncomingSessionKey(
        CryptoHandler::stringToKey<EncryptedSessionKey>(idPayload.payload));
    if (!result) {
      LOG(FATAL) << "Invalid session key";
    }
    BiDirectionalRpc::addIncomingRequest(idPayload);
    BiDirectionalRpc::reply(idPayload.id, "OK");
    return;
  }
  if (!ready()) {
    LOG(INFO) << "Tried to receive data before we were ready";
    return;
  }
  IdPayload decryptedIdPayload =
      IdPayload(idPayload.id, cryptoHandler->decrypt(idPayload.payload));
  LOG(INFO) << "GOT REQUEST WITH PAYLOAD: " << decryptedIdPayload.payload;
  BiDirectionalRpc::addIncomingRequest(decryptedIdPayload);
}

void MultiEndpointHandler::addIncomingReply(const RpcId& uid,
                                            const string& payload) {
  if (!ready()) {
    LOG(FATAL) << "Got reply before we were ready, something went wrong";
  }
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