#ifndef __MULTI_ENDPOINT_HANDLER_H__
#define __MULTI_ENDPOINT_HANDLER_H__

#include "Headers.hpp"
#include "NetEngine.hpp"
#include "RpcId.hpp"
#include "UdpBiDirectionalRpc.hpp"

namespace wga {
class MultiEndpointHandler : public UdpBiDirectionalRpc {
 public:
  MultiEndpointHandler(shared_ptr<NetEngine> _netEngine,
                       shared_ptr<udp::socket> _localSocket,
                       const vector<udp::endpoint>& endpoints);

  virtual ~MultiEndpointHandler() {}

  virtual void handleReply(const RpcId& rpcId, const string& payload) {
    lastUnrepliedSendTime = 0;
    BiDirectionalRpc::handleReply(rpcId, payload);
  }

  virtual bool hasEndpointAndResurrectIfFound(const udp::endpoint& endpoint);
  void addEndpoints(const vector<udp::endpoint>& newEndpoints);
  void addEndpoint(const udp::endpoint& newEndpoint) {
    if (activeEndpoint == newEndpoint) {
      return;
    }
    if (alternativeEndpoints.find(newEndpoint) != alternativeEndpoints.end()) {
      return;
    }
    if (deadEndpoints.find(newEndpoint) != deadEndpoints.end()) {
      return;
    }
    alternativeEndpoints.insert(newEndpoint);
  }

 protected:
  time_t lastUpdateTime;
  time_t lastUnrepliedSendTime;
  set<udp::endpoint> alternativeEndpoints;
  set<udp::endpoint> deadEndpoints;
  void update();
  virtual void send(const string& message);
};
}  // namespace wga

#endif
