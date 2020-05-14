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
            shared_ptr<udp::socket> _localSocket);

  void addEndpoint(const string& id,
                   shared_ptr<EncryptedMultiEndpointHandler> endpoint);

  void broadcast(const string& payload);

  RpcId request(const string& userId, const string& payload);

  optional<UserIdIdPayload> getIncomingRequest();

  void reply(const string& id, const RpcId& rpcId, const string& payload);

  optional<UserIdIdPayload> getIncomingReply();

  void heartbeat();
  void resendRandomOutgoingMessage();
  bool readyToSend();
  void runUntilInitialized();
  void sendShutdown();
  void finish();
  bool hasWork();
  int getLivingPeerCount();
  bool isPeerShutDown(const string& peerId);

  vector<string> getPeerIds();

  bool hasPeer(const string& id);

  void addEndpoints(const string& key,
                    const vector<udp::endpoint>& newEndpoints);

  shared_ptr<EncryptedMultiEndpointHandler> getEndpointHandler(
      const string& key);

  map<string, pair<double, double>> getPeerLatency();

  double getHalfPingUpperBound();

 protected:
  map<string, shared_ptr<EncryptedMultiEndpointHandler>> endpoints;
};
}  // namespace wga

#endif
