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
  if (idPayload.payload.size() == 0) {
    MultiEndpointHandler::requestWithId(idPayload);
    return;
  }

  if (!readyToSend()) {
    LOGFATAL << "Tried to send data before we were ready";
  }
  IdPayload encryptedIdPayload =
      IdPayload(idPayload.id, cryptoHandler->encrypt(idPayload.payload));
  MultiEndpointHandler::requestWithId(encryptedIdPayload);
}

void EncryptedMultiEndpointHandler::reply(const RpcId& rpcId,
                                          const string& payload) {
  lock_guard<recursive_mutex> guard(mutex);

  if (payload.size() == 0) {
    MultiEndpointHandler::reply(rpcId, payload);
    return;
  }

  if (!readyToSend()) {
    LOGFATAL << "Got reply before we were ready, something went wrong";
  }
  string encryptedPayload = cryptoHandler->encrypt(payload);
  MultiEndpointHandler::reply(rpcId, encryptedPayload);
}

void EncryptedMultiEndpointHandler::addIncomingRequest(
    const IdPayload& idPayload) {
  lock_guard<recursive_mutex> guard(mutex);
  if (idPayload.payload.size() == 0) {
    // Heartbeat
    MultiEndpointHandler::addIncomingRequest(idPayload);
    return;
  }

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
      LOGFATAL << "Somehow got the wrong public key: " << CryptoHandler::keyToString(publicKey) << " != " << CryptoHandler::keyToString(cryptoHandler->getOtherPublicKey());
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
    MultiEndpointHandler::reply(idPayload.id, "OK");
    return;
  }

  if (!readyToReceive()) {
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
  if (payload.size() == 0) {
    // Heartbeat
    MultiEndpointHandler::addIncomingReply(uid, payload);
    return;
  }

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

void EncryptedMultiEndpointHandler::send(const string& message) {
  string messageWithHeader = WGA_MAGIC + message;
  MultiEndpointHandler::send(messageWithHeader);
}

bool EncryptedMultiEndpointHandler::hasPublicKeyMatchInPayload(
    const string& remainder) {
  LOG(INFO) << "Checking public key match";
  MessageReader reader;
  reader.load(remainder);
  RpcHeader header = (RpcHeader)reader.readPrimitive<unsigned char>();
  if (header != REQUEST) {
    LOG(INFO) << "Header isn't request";
    return false;
  }
  RpcId rpcId = reader.readClass<RpcId>();
  if (rpcId != SESSION_KEY_RPCID) {
    return false;
  }
  string payload = reader.readPrimitive<string>();
  MessageReader innerReader;
  innerReader.load(payload);
  PublicKey publicKey = CryptoHandler::stringToKey<PublicKey>(
      innerReader.readPrimitive<string>());
  return publicKey == cryptoHandler->getOtherPublicKey();
}
}  // namespace wga