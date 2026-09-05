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
    lock_guard<recursive_mutex> guard(mutex);
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
    lock_guard<recursive_mutex> guard(mutex);
    return bannedEndpoints.find(newEndpoint) != bannedEndpoints.end();
  }
  bool isConnectionDead() {
    lock_guard<recursive_mutex> guard(mutex);
    if (!hasUnrepliedSend) {
      return false;
    }
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now() - lastUnrepliedSendTime)
               .count() >= 5;
  }
  virtual bool hasWork() {
    lock_guard<recursive_mutex> guard(mutex);
    if (isConnectionDead()) {
      return false;
    }
    return UdpBiDirectionalRpc::hasWork();
  }
  bool hasEndpoint(const udp::endpoint& newEndpoint) {
    lock_guard<recursive_mutex> guard(mutex);
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
    lock_guard<recursive_mutex> guard(mutex);
    auto result = alternativeEndpoints;
    result.insert(activeEndpoint);
    return result;
  }
  virtual void onSendError(const udp::endpoint& destination) override;

 protected:
  chrono::steady_clock::time_point lastUpdateTime;
  chrono::steady_clock::time_point lastUnrepliedSendTime;
  chrono::steady_clock::time_point lastUnrepliedSendOrKillTime;
  bool hasUnrepliedSend;
  bool endpointConfirmed;
  set<udp::endpoint> alternativeEndpoints;
  set<udp::endpoint> deadEndpoints;
  set<udp::endpoint> bannedEndpoints;
  void update();
  virtual void send(const string& message);
  void killEndpoint();
};
}  // namespace wga

#endif
