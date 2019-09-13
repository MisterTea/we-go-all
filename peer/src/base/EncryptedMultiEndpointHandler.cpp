#include "EncryptedMultiEndpointHandler.hpp"

namespace wga {
EncryptedMultiEndpointHandler::EncryptedMultiEndpointHandler(
    shared_ptr<udp::socket> _localSocket, shared_ptr<NetEngine> _netEngine,
    shared_ptr<CryptoHandler> _cryptoHandler,
    const vector<udp::endpoint>& endpoints)
    : MultiEndpointHandler(_netEngine, _localSocket, endpoints),
      cryptoHandler(_cryptoHandler) {
  if (cryptoHandler->canDecrypt() || cryptoHandler->canEncrypt()) {
    LOGFATAL << "Created endpoint handler with session key";
  }

  // Send session key as a one-way rpc
  {
    IdPayload idPayload;
    MessageWriter writer;
    writer.start();
    writer.writePrimitive(
        CryptoHandler::keyToString(cryptoHandler->getMyPublicKey()));
    writer.writePrimitive(CryptoHandler::keyToString(
        cryptoHandler->generateOutgoingSessionKey()));
    idPayload.payload = writer.finish();
    VLOG(1) << idPayload.payload;
    idPayload.id = SESSION_KEY_RPCID;
    oneWayRequests.insert(idPayload.id);
    MultiEndpointHandler::requestWithId(idPayload);
  }
}

void EncryptedMultiEndpointHandler::requestWithId(const IdPayload& idPayload) {
  if (!readyToSend()) {
    // These are heartbeats that can't go out yet
    LOGFATAL << "Tried to send data before we were ready: " << readyToReceive();
  }
  IdPayload encryptedIdPayload =
      IdPayload(idPayload.id, cryptoHandler->encrypt(idPayload.payload));
  MultiEndpointHandler::requestWithId(encryptedIdPayload);
}

void EncryptedMultiEndpointHandler::reply(const RpcId& rpcId,
                                          const string& payload) {
  lock_guard<recursive_mutex> guard(mutex);

  if (!readyToReceive()) {
    LOGFATAL << "Got reply before we were ready, something went wrong " << readyToReceive();
  }
  string encryptedPayload = cryptoHandler->encrypt(payload);
  MultiEndpointHandler::reply(rpcId, encryptedPayload);
}

void EncryptedMultiEndpointHandler::addIncomingRequest(
    const IdPayload& idPayload) {
  lock_guard<recursive_mutex> guard(mutex);
  if (idPayload.id == SESSION_KEY_RPCID) {
    // Handshaking
    LOG(INFO) << "GOT HANDSHAKE";
    if (cryptoHandler->canDecrypt()) {
      LOG(WARNING) << "We already got the key, skipping request";
      return;
    }
    MessageReader reader;
    reader.load(idPayload.payload);
    PublicKey publicKey =
        CryptoHandler::stringToKey<PublicKey>(reader.readPrimitive<string>());
    if (publicKey != cryptoHandler->getOtherPublicKey()) {
      LOG(ERROR) << "Somehow got the wrong public key: " << CryptoHandler::keyToString(publicKey) << " != " << CryptoHandler::keyToString(cryptoHandler->getOtherPublicKey());
      return;
    }
    EncryptedSessionKey encryptedSessionKey =
        CryptoHandler::stringToKey<EncryptedSessionKey>(
            reader.readPrimitive<string>());
    bool result = cryptoHandler->receiveIncomingSessionKey(encryptedSessionKey);
    if (!result) {
      LOG(ERROR) << "Invalid session key";
      return;
    }
    MultiEndpointHandler::addIncomingRequest(idPayload);
    reply(idPayload.id, "OK");
    return;
  }

  if (!readyToReceive()) {
    LOG(INFO) << "Tried to receive data before we were ready";
    return;
  }
  auto decryptedString = cryptoHandler->decrypt(idPayload.payload);
  if (!decryptedString) {
    // Corrupt message, ignore
    LOG(ERROR) << "Got a corrupt packet: " << idPayload.payload;
    return;
  }
  IdPayload decryptedIdPayload = IdPayload(idPayload.id, *decryptedString);
  VLOG(1) << "GOT REQUEST WITH PAYLOAD SIZE: "
          << decryptedIdPayload.payload.length();
  MultiEndpointHandler::addIncomingRequest(decryptedIdPayload);
}

void EncryptedMultiEndpointHandler::addIncomingReply(const RpcId& uid,
                                                     const string& payload) {
  lock_guard<recursive_mutex> guard(mutex);
  if (!readyToSend()) {
    LOG(ERROR) << "Got reply before we were ready, something went wrong";
    return;
  }
  auto decryptedPayload = cryptoHandler->decrypt(payload);
  if (!decryptedPayload) {
    LOG(ERROR) << "Got corrupt packet";
    return;
  }
  VLOG(1) << "GOT REPLY WITH PAYLOAD SIZE: " << decryptedPayload->length();
  MultiEndpointHandler::addIncomingReply(uid, *decryptedPayload);
}

void EncryptedMultiEndpointHandler::sendAcknowledge(const RpcId& uid) {
  MessageWriter writer;
  writer.start();
  writer.writePrimitive<unsigned char>(ACKNOWLEDGE);
  writer.writeClass<RpcId>(uid);
  writer.writePrimitive<string>(cryptoHandler->encrypt("ACK_OK"));
  send(writer.finish());
}

void EncryptedMultiEndpointHandler::send(const string& message) {
  string header = WGA_MAGIC;
  string messageWithHeader = header + message;
  MultiEndpointHandler::send(messageWithHeader);
}

bool EncryptedMultiEndpointHandler::validatePacket(const RpcId& rpcId, const string& payload) {
  if (rpcId == SESSION_KEY_RPCID) {
    return true;
  }
  if (!cryptoHandler->canDecrypt()) {
    LOG(WARNING) << "Tried to validate packet but we can't decrypt yet";
    return false;
  }
  bool result = bool(cryptoHandler->decrypt(payload));
  if (!result) {
    LOG(WARNING) << "Got a packet intended for someone else (or malformed)";
  }
  return result;
}
}  // namespace wga