#ifndef __RPC_SERVER_H__
#define __RPC_SERVER_H__

#include "Headers.hpp"
#include "MultiEndpointHandler.hpp"
#include "NetEngine.hpp"
#include "PortMultiplexer.hpp"

namespace wga {
class KeyIdPayload {
 public:
  KeyIdPayload() {}
  KeyIdPayload(const PublicKey& _key, const RpcId& _id, const string& _payload)
      : key(_key), id(_id), payload(_payload) {}
  KeyIdPayload(const PublicKey& _key, const IdPayload& idPayload)
      : key(_key), id(idPayload.id), payload(idPayload.payload) {}

  bool empty() { return id.empty(); }

  PublicKey key;
  RpcId id;
  string payload;
};

class RpcServer : public PortMultiplexer {
 public:
  RpcServer(shared_ptr<NetEngine> _netEngine,
            shared_ptr<udp::socket> _localSocket)
      : PortMultiplexer(_netEngine, _localSocket) {}

  void addEndpoint(shared_ptr<MultiEndpointHandler> endpoint) {
    endpoints.insert(
        make_pair(endpoint->getCryptoHandler()->getOtherPublicKey(), endpoint));
    addRecipient(endpoint);
  }

  void broadcast(const string& payload) {
    for (auto it : endpoints) {
      it.second->requestNoReply(payload);
    }
  }

  RpcId request(const PublicKey& publicKey, const string& payload) {
    auto it = endpoints.find(publicKey);
    if (it == endpoints.end()) {
      LOG(FATAL) << "Tried to send to an invalid endpoint";
    }
    return it->second->request(payload);
  }

  KeyIdPayload getIncomingRequest() {
    for (auto it : endpoints) {
      if (it.second->hasIncomingRequest()) {
        return KeyIdPayload(it.first, it.second->getFirstIncomingRequest());
      }
    }
    return KeyIdPayload();
  }

  void reply(const PublicKey& publicKey, const RpcId& rpcId,
             const string& payload) {
    auto it = endpoints.find(publicKey);
    if (it == endpoints.end()) {
      LOG(FATAL) << "Could not find endpoint";
    }
    it->second->reply(rpcId, payload);
  }

  KeyIdPayload getIncomingReply() {
    for (auto it : endpoints) {
      if (it.second->hasIncomingReply()) {
        return KeyIdPayload(it.first, it.second->getFirstIncomingReply());
      }
    }
    return KeyIdPayload();
  }

  void heartbeat() {
    for (auto it : endpoints) {
      it.second->heartbeat();
    }
  }

  bool ready() {
    for (auto it : endpoints) {
      if (!it.second->ready()) {
        return false;
      }
    }
    return true;
  }

  void runUntilInitialized() {
    while (!ready()) {
      {
        lock_guard<recursive_mutex> guard(*netEngine->getMutex());
        heartbeat();
      }
      LOG(INFO) << "WAITING FOR INIT";
      usleep(1000 * 1000);
    }
  }

  void finish() {
    while (!hasWork()) {
      {
        lock_guard<recursive_mutex> guard(*netEngine->getMutex());
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

  vector<PublicKey> getPeerKeys() {
    vector<PublicKey> retval;
    for (auto it : endpoints) {
      retval.push_back(it.first);
    }
    return retval;
  }

  PublicKey getMyPublicKey() {
    if (endpoints.empty()) {
      LOG(FATAL) << "Tried to get key without endpoints";
    }
    return endpoints.begin()->second->getCryptoHandler()->getMyPublicKey();
  }

 protected:
  map<PublicKey, shared_ptr<MultiEndpointHandler>> endpoints;
};
}  // namespace wga

#endif
