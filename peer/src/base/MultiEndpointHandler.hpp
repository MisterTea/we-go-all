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
                       const vector<udp::endpoint>& endpoints,
                       bool connectedToHost);

  virtual ~MultiEndpointHandler() {}

  virtual void handleReply(const RpcId& rpcId, const string& payload, int64_t requestReceiveTime, int64_t replySendTime);
  virtual bool hasEndpointAndResurrectIfFound(const udp::endpoint& endpoint);
  void addEndpoints(const vector<udp::endpoint>& newEndpoints);
  void addEndpoint(const udp::endpoint& newEndpoint) {
    if (bannedEndpoints.find(newEndpoint) != bannedEndpoints.end()) {
      return;
    }
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
  void banEndpoint(const udp::endpoint& newEndpoint);
  bool isEndpointBanned(const udp::endpoint& newEndpoint) {
    return bannedEndpoints.find(newEndpoint) != bannedEndpoints.end();
  }
  bool isConnectionDead() {
    return (lastUnrepliedSendTime + 5) < time(NULL);
  }
  virtual bool hasWork() {
    lock_guard<recursive_mutex> guard(mutex);
    if (isConnectionDead()) {
      return false;
    }
    return UdpBiDirectionalRpc::hasWork();
  bool hasEndpoint(const udp::endpoint& newEndpoint) {
    if (bannedEndpoints.find(newEndpoint) != bannedEndpoints.end()) {
      return true;
    }
    if (activeEndpoint == newEndpoint) {
      return true;
    }
    if (alternativeEndpoints.find(newEndpoint) != alternativeEndpoints.end()) {
      return true;
    }
    if (deadEndpoints.find(newEndpoint) != deadEndpoints.end()) {
      return true;
    }
    return false;
  }
  set<udp::endpoint> aliveEndpoints() {
      auto result = alternativeEndpoints;
      result.insert(activeEndpoint);
      return result;
  }

 protected:
  time_t lastUpdateTime;
  time_t lastUnrepliedSendTime;
  time_t lastUnrepliedSendOrKillTime;
  set<udp::endpoint> alternativeEndpoints;
  set<udp::endpoint> deadEndpoints;
  set<udp::endpoint> bannedEndpoints;
  void update();
  virtual void send(const string& message);
  void killEndpoint();
};
}  // namespace wga

#endif
