#ifndef __FAN_OUT_RPC_H__
#define __FAN_OUT_RPC_H__

#include "Headers.hpp"

#include "BiDirectionalRpc.hpp"

namespace wga {
class FanOutRpc {
 public:
  FanOutRpc(const vector<shared_ptr<BiDirectionalRpc>>& _rpcs) : rpcs(_rpcs) {}

  RpcId request(const string& payload) {
    RpcId id = rpcs[0]->request(payload);
    for (int a = 1; a < rpcs.size(); a++) {
      rpcs[a]->requestWithId(IdPayload(id, payload));
    }
    return id;
  }
  void reply(const RpcId& rpcId, const string& payload) {
    for (auto it : rpcs) {
      it->reply(rpcId, payload);
    }
  }

  bool hasIncomingReplyWithId(const RpcId& rpcId) {
    for (auto it : rpcs) {
      if (!it->hasIncomingReplyWithId(rpcId)) {
        return false;
      }
    }
    return true;
  }
  vector<string> consumeIncomingReplyWithId(const RpcId& rpcId) {
    vector<string> retval;
    for (auto it : rpcs) {
      retval.push_back(it->consumeIncomingReplyWithId(rpcId));
    }
    return retval;
  }

  void heartbeat() {
    for (auto it : rpcs) {
      it->heartbeat();
    }
  }

  void shutdown() {
    for (auto it : rpcs) {
      it->shutdown();
    }
  }

 protected:
  vector<shared_ptr<BiDirectionalRpc>> rpcs;
};
}  // namespace wga

#endif