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
                                const vector<udp::endpoint>& endpoints,
                                bool connectedToHost);

  virtual ~EncryptedMultiEndpointHandler() {}

  void sendSessionKey();
  virtual void requestWithId(const IdPayload& idPayload);
  virtual void reply(const RpcId& rpcId, const string& payload);
  shared_ptr<CryptoHandler> getCryptoHandler() { return cryptoHandler; }
  virtual bool readyToSend() {
    lock_guard<recursive_mutex> guard(mutex);
    // Make sure rpc(0,1) is finished
    for (const auto& it : outgoingRequests) {
      if (it.first == SESSION_KEY_RPCID) {
        return false;
      }
    }
    for (const auto& it : outgoingReplies) {
      if (it.first == SESSION_KEY_RPCID) {
        return false;
      }
    }
    return readyToReceive();
  }

  bool readyToReceive() {
    lock_guard<recursive_mutex> guard(mutex);
    return cryptoHandler->canDecrypt() && cryptoHandler->canEncrypt();
  }

 protected:
  shared_ptr<CryptoHandler> cryptoHandler;
  virtual void addIncomingRequest(const IdPayload& idPayload);
  virtual void addIncomingReply(const RpcId& uid, const string& payload);
  virtual void send(const string& message);
  virtual void sendAcknowledge(const RpcId& uid);
  bool validatePacket(const RpcId& rpcId, const string& payload);
};
}  // namespace wga

#endif
