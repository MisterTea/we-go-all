#include "EncryptedMultiEndpointHandler.hpp"

namespace wga {
EncryptedMultiEndpointHandler::EncryptedMultiEndpointHandler(
    shared_ptr<udp::socket> _localSocket, shared_ptr<NetEngine> _netEngine,
    shared_ptr<CryptoHandler> _cryptoHandler,
    const vector<udp::endpoint>& endpoints)
    : MultiEndpointHandler(_netEngine, _localSocket, endpoints),
      cryptoHandler(_cryptoHandler) {
  if (cryptoHandler->canDecrypt() || cryptoHandler->canEncrypt()) {
    LOG(FATAL) << "Created endpoint handler with session key";
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
    VLOG(1) << idPayload.payload;
    idPayload.id = RpcId(0, 1);
    oneWayRequests.insert(idPayload.id);
    MultiEndpointHandler::requestWithId(idPayload);
  }
}

void EncryptedMultiEndpointHandler::requestWithId(const IdPayload& idPayload) {
  if (!readyToSend()) {
    LOG(FATAL) << "Tried to send data before we were ready";
  }
  IdPayload encryptedIdPayload =
      IdPayload(idPayload.id, cryptoHandler->encrypt(idPayload.payload));
  MultiEndpointHandler::requestWithId(encryptedIdPayload);
}

void EncryptedMultiEndpointHandler::reply(const RpcId& rpcId,
                                          const string& payload) {
  if (!readyToSend()) {
    LOG(FATAL) << "Got reply before we were ready, something went wrong";
  }
  string encryptedPayload = cryptoHandler->encrypt(payload);
  MultiEndpointHandler::reply(rpcId, encryptedPayload);
}

void EncryptedMultiEndpointHandler::addIncomingRequest(
    const IdPayload& idPayload) {
  if (idPayload.id == RpcId(0, 1)) {
    // Handshaking
    LOG(INFO) << "GOT HANDSHAKE";
    bool result = cryptoHandler->recieveIncomingSessionKey(
        CryptoHandler::stringToKey<EncryptedSessionKey>(idPayload.payload));
    if (!result) {
      LOG(ERROR) << "Invalid session key";
      return;
    }
    MultiEndpointHandler::addIncomingRequest(idPayload);
    MultiEndpointHandler::reply(idPayload.id, "OK");
    return;
  }
  if (!readyToRecieve()) {
    LOG(INFO) << "Tried to receive data before we were ready";
    return;
  }
  auto decryptedString = cryptoHandler->decrypt(idPayload.payload);
  if (!decryptedString) {
    // Corrupt message, ignore
    LOG(ERROR) << "Got a corrupt packet";
    return;
  }
  IdPayload decryptedIdPayload = IdPayload(idPayload.id, *decryptedString);
  VLOG(1) << "GOT REQUEST WITH PAYLOAD SIZE: "
          << decryptedIdPayload.payload.length();
  MultiEndpointHandler::addIncomingRequest(decryptedIdPayload);
}

void EncryptedMultiEndpointHandler::addIncomingReply(const RpcId& uid,
                                                     const string& payload) {
  if (!readyToSend()) {
    LOG(FATAL) << "Got reply before we were ready, something went wrong";
  }
  auto decryptedPayload = cryptoHandler->decrypt(payload);
  if (!decryptedPayload) {
    LOG(ERROR) << "Got corrupt packet";
    return;
  }
  VLOG(1) << "GOT REPLY WITH PAYLOAD SIZE: " << decryptedPayload->length();
  MultiEndpointHandler::addIncomingReply(uid, *decryptedPayload);
}

}  // namespace wga