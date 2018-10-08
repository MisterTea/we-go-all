#ifndef __MULTI_ENDPOINT_HANDLER_H__
#define __MULTI_ENDPOINT_HANDLER_H__

#include "BiDirectionalRpc.hpp"
#include "CryptoHandler.hpp"
#include "Headers.hpp"
#include "RpcId.hpp"

namespace wga {
class MultiEndpointHandler : public BiDirectionalRpc {
 public:
  MultiEndpointHandler(shared_ptr<udp::socket> _localSocket,
                       shared_ptr<asio::io_service> _ioService,
                       shared_ptr<CryptoHandler> _cryptoHandler,
                       const udp::endpoint& endpoint);

  void gotMessage() { lastUnrepliedSendTime = 0; }
  bool hasEndpointAndResurrectIfFound(const udp::endpoint& endpoint);
  virtual void requestWithId(const IdPayload& idPayload);
  virtual void reply(const RpcId& rpcId, const string& payload);

 protected:
  shared_ptr<udp::socket> localSocket;
  shared_ptr<asio::io_service> ioService;
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