#ifndef __MULTI_ENDPOINT_HANDLER_H__
#define __MULTI_ENDPOINT_HANDLER_H__

#include "BiDirectionalRpc.hpp"
#include "CryptoHandler.hpp"
#include "Headers.hpp"
#include "NetEngine.hpp"
#include "RpcId.hpp"

namespace wga {
class MultiEndpointHandler : public BiDirectionalRpc {
 public:
  MultiEndpointHandler(shared_ptr<udp::socket> _localSocket,
                       shared_ptr<NetEngine> _netEngine,
                       shared_ptr<CryptoHandler> _cryptoHandler,
                       const vector<udp::endpoint>& endpoints);

  void gotMessage() { lastUnrepliedSendTime = 0; }
  virtual bool hasEndpointAndResurrectIfFound(const udp::endpoint& endpoint);
  virtual void requestWithId(const IdPayload& idPayload);
  virtual void reply(const RpcId& rpcId, const string& payload);
  shared_ptr<CryptoHandler> getCryptoHandler() { return cryptoHandler; }
  bool ready() {
    return cryptoHandler->canDecrypt() && cryptoHandler->canEncrypt();
  }

 protected:
  shared_ptr<udp::socket> localSocket;
  shared_ptr<NetEngine> netEngine;
  shared_ptr<CryptoHandler> cryptoHandler;
  udp::endpoint activeEndpoint;
  time_t lastUpdateTime;
  time_t lastUnrepliedSendTime;
  set<udp::endpoint> alternativeEndpoints;
  set<udp::endpoint> deadEndpoints;
  virtual void addIncomingRequest(const IdPayload& idPayload);
  virtual void addIncomingReply(const RpcId& uid, const string& payload);
  void update();
  virtual void send(const string& message);
};
}  // namespace wga

#endif
