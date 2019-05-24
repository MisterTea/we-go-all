#ifndef __RPC_SERVER_H__
#define __RPC_SERVER_H__

#include "EncryptedMultiEndpointHandler.hpp"
#include "Headers.hpp"
#include "NetEngine.hpp"
#include "PortMultiplexer.hpp"

namespace wga {
class UserIdIdPayload {
 public:
  UserIdIdPayload() {}
  UserIdIdPayload(const string& _userId, const RpcId& _id,
                  const string& _payload)
      : userId(_userId), id(_id), payload(_payload) {}
  UserIdIdPayload(const string& _userId, const IdPayload& idPayload)
      : userId(_userId), id(idPayload.id), payload(idPayload.payload) {}

  string userId;
  RpcId id;
  string payload;
};

class RpcServer : public PortMultiplexer {
 public:
  RpcServer(shared_ptr<NetEngine> _netEngine,
            shared_ptr<udp::socket> _localSocket)
      : PortMultiplexer(_netEngine, _localSocket) {}

  void addEndpoint(const string& id,
                   shared_ptr<EncryptedMultiEndpointHandler> endpoint) {
    endpoints.insert(make_pair(id, endpoint));
    addRecipient(endpoint);
  }

  void broadcast(const string& payload) {
    for (auto it : endpoints) {
      it.second->requestNoReply(payload);
    }
  }

  RpcId request(const string& userId, const string& payload) {
    auto it = endpoints.find(userId);
    if (it == endpoints.end()) {
      LOGFATAL << "Tried to send to an invalid endpoint: " << userId;
    }
    return it->second->request(payload);
  }

  optional<UserIdIdPayload> getIncomingRequest() {
    for (auto it : endpoints) {
      if (it.second->hasIncomingRequest()) {
        return UserIdIdPayload(it.first, it.second->getFirstIncomingRequest());
      }
    }
    return nullopt;
  }

  void reply(const string& id, const RpcId& rpcId, const string& payload) {
    auto it = endpoints.find(id);
    if (it == endpoints.end()) {
      LOGFATAL << "Could not find endpoint";
    }
    it->second->reply(rpcId, payload);
  }

  optional<UserIdIdPayload> getIncomingReply() {
    for (auto it : endpoints) {
      if (it.second->hasIncomingReply()) {
        return UserIdIdPayload(it.first, it.second->getFirstIncomingReply());
      }
    }
    return nullopt;
  }

  void heartbeat() {
    for (auto it : endpoints) {
      it.second->heartbeat();
    }
  }

  bool readyToSend() {
    for (auto it : endpoints) {
      if (!it.second->readyToSend()) {
        return false;
      }
    }
    return true;
  }

  void runUntilInitialized() {
    while (true) {
      {
        if (readyToSend()) {
          break;
        }
        heartbeat();
      }
      LOG(INFO) << "WAITING FOR INIT";
      usleep(1000 * 1000);
    }
  }

  void finish() {
    for (auto it : endpoints) {
      it.second->shutdown();
    }
    for (int a = 0; a < 100; a++) {
      for (auto it : endpoints) {
        it.second->sendShutdown();
      }
      usleep(10 * 1000);
    }
    while (hasWork()) {
      {
        heartbeat();
      }
      LOG(INFO) << "WAITING FOR FINISH";
      usleep(1000 * 1000);
    }
  }

  bool hasWork() {
    for (auto it : endpoints) {
      if (it.second->hasWork()) {
        return true;
      }
    }
    return false;
  }

  int getLivingPeerCount() {
    int count = 0;
    for (auto it : endpoints) {
      if (!it.second->isShuttingDown()) {
        count++;
      }
    }
    return count;
  }

  vector<string> getPeerIds() {
    vector<string> retval;
    for (auto it : endpoints) {
      retval.push_back(it.first);
    }
    return retval;
  }

  bool hasPeer(const string& id) {
    return endpoints.find(id) != endpoints.end();
  }

  void addEndpoints(const string& key,
                    const vector<udp::endpoint>& newEndpoints) {
    endpoints[key]->addEndpoints(newEndpoints);
  }

  shared_ptr<EncryptedMultiEndpointHandler> getEndpointHandler(
      const string& key) {
    auto it = endpoints.find(key);
    if (it == endpoints.end()) {
      LOGFATAL << "Invalid endpoint handler: " << key;
    }
    return it->second;
  }

  map<string, pair<double, double>> getPeerLatency() {
    map<string, pair<double, double>> retval;
    for (const auto& it : endpoints) {
      retval[it.first] = it.second->getLatency();
    }
    return retval;
  }

  double getHalfPingUpperBound() {
    double ping = 0;
    for (const auto& it : endpoints) {
      ping = max(ping, it.second->getHalfPingUpperBound());
    }
    ping = min(ping, 1000000.0);
    return ping;
  }

 protected:
  map<string, shared_ptr<EncryptedMultiEndpointHandler>> endpoints;
};
}  // namespace wga

#endif
