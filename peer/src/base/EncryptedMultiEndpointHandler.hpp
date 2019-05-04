#ifndef __ENCRYPTED_MULTI_ENDPOINT_HANDLER_H__
#define __ENCRYPTED_MULTI_ENDPOINT_HANDLER_H__

#include "CryptoHandler.hpp"
#include "Headers.hpp"
#include "MultiEndpointHandler.hpp"
#include "NetEngine.hpp"
#include "RpcId.hpp"

namespace wga {
class EncryptedMultiEndpointHandler : public MultiEndpointHandler {
 public:
  EncryptedMultiEndpointHandler(shared_ptr<udp::socket> _localSocket,
                                shared_ptr<NetEngine> _netEngine,
                                shared_ptr<CryptoHandler> _cryptoHandler,
                                const vector<udp::endpoint>& endpoints);

  virtual ~EncryptedMultiEndpointHandler() {}

  virtual void requestWithId(const IdPayload& idPayload);
  virtual void reply(const RpcId& rpcId, const string& payload);
  shared_ptr<CryptoHandler> getCryptoHandler() { return cryptoHandler; }
  bool readyToSend() {
    // Make sure rpc(0,1) is finished
    for (auto& it : outgoingRequests) {
      if (it.id == SESSION_KEY_RPCID) {
        return false;
      }
    }
    return readyToRecieve();
  }

  bool readyToRecieve() {
    return cryptoHandler->canDecrypt() && cryptoHandler->canEncrypt();
  }

  bool canDecodeMagicHeader(const std::string& magicHeader,
                            const std::string& remainder) {
    if (!cryptoHandler->canDecrypt()) {
      // This must be the session key, try to dive into the packet and fetch the
      // public key
      return hasPublicKeyMatchInPayload(remainder);
    }

    auto result = cryptoHandler->decrypt(magicHeader);
    if (result) {
      if (*result == WGA_MAGIC) {
        return true;
      }
    }
    return false;
  }

 protected:
  shared_ptr<CryptoHandler> cryptoHandler;
  virtual void addIncomingRequest(const IdPayload& idPayload);
  virtual void addIncomingReply(const RpcId& uid, const string& payload);
  virtual void send(const string& message);
  bool hasPublicKeyMatchInPayload(const string& remainder);
};
}  // namespace wga

#endif
