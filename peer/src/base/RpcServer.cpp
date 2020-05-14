#include "RpcServer.hpp"

namespace wga {
RpcServer::RpcServer(shared_ptr<NetEngine> _netEngine,
                     shared_ptr<udp::socket> _localSocket)
    : PortMultiplexer(_netEngine, _localSocket) {}

void RpcServer::addEndpoint(
    const string& id, shared_ptr<EncryptedMultiEndpointHandler> endpoint) {
  endpoints.insert(make_pair(id, endpoint));
  addRecipient(endpoint);
}

void RpcServer::broadcast(const string& payload) {
  for (auto it : endpoints) {
    it.second->requestOneWay(payload);
  }
}

RpcId RpcServer::request(const string& userId, const string& payload) {
  auto it = endpoints.find(userId);
  if (it == endpoints.end()) {
    LOGFATAL << "Tried to send to an invalid endpoint: " << userId;
  }
  return it->second->request(payload);
}

optional<UserIdIdPayload> RpcServer::getIncomingRequest() {
  for (auto it : endpoints) {
    if (it.second->hasIncomingRequest()) {
      return UserIdIdPayload(it.first, it.second->getFirstIncomingRequest());
    }
  }
  return nullopt;
}

void RpcServer::reply(const string& id, const RpcId& rpcId,
                      const string& payload) {
  auto it = endpoints.find(id);
  if (it == endpoints.end()) {
    LOGFATAL << "Could not find endpoint";
  }
  it->second->reply(rpcId, payload);
}

optional<UserIdIdPayload> RpcServer::getIncomingReply() {
  for (auto it : endpoints) {
    if (it.second->hasIncomingReply()) {
      return UserIdIdPayload(it.first, it.second->getFirstIncomingReply());
    }
  }
  return nullopt;
}

void RpcServer::heartbeat() {
  for (auto it : endpoints) {
    it.second->heartbeat();
  }
}

void RpcServer::resendRandomOutgoingMessage() {
  for (auto it : endpoints) {
    it.second->resendRandomOutgoingMessage();
  }
}

bool RpcServer::readyToSend() {
  for (auto it : endpoints) {
    if (!it.second->readyToSend()) {
      return false;
    }
  }
  return true;
}

void RpcServer::runUntilInitialized() {
  while (true) {
    {
      if (readyToSend()) {
        break;
      }
      heartbeat();
    }
    LOG(INFO) << "WAITING FOR INIT";
    microsleep(1000 * 1000);
  }
}

void RpcServer::sendShutdown() {
  for (auto it : endpoints) {
    it.second->sendShutdown();
  }
}

void RpcServer::finish() { endpoints.clear(); }

bool RpcServer::hasWork() {
  for (auto it : endpoints) {
    if (it.second->hasWork()) {
      return true;
    }
  }
  return false;
}

int RpcServer::getLivingPeerCount() {
  int count = 0;
  for (auto it : endpoints) {
    if (!it.second->isShuttingDown()) {
      count++;
    }
  }
  return count;
}

bool RpcServer::isPeerShutDown(const string& peerId) {
  auto it = endpoints.find(peerId);
  if (it == endpoints.end()) {
      // Peer hasn't started yet
      return false;
  }
  return it->second->isShuttingDown();
}

vector<string> RpcServer::getPeerIds() {
  vector<string> retval;
  for (auto it : endpoints) {
    retval.push_back(it.first);
  }
  return retval;
}

bool RpcServer::hasPeer(const string& id) {
  return endpoints.find(id) != endpoints.end();
}

void RpcServer::addEndpoints(const string& key,
                             const vector<udp::endpoint>& newEndpoints) {
  endpoints[key]->addEndpoints(newEndpoints);
}

shared_ptr<EncryptedMultiEndpointHandler> RpcServer::getEndpointHandler(
    const string& key) {
  auto it = endpoints.find(key);
  if (it == endpoints.end()) {
    LOGFATAL << "Invalid endpoint handler: " << key;
  }
  return it->second;
}

map<string, pair<double, double>> RpcServer::getPeerLatency() {
  map<string, pair<double, double>> retval;
  for (const auto& it : endpoints) {
    retval[it.first] = it.second->getLatency();
  }
  return retval;
}

double RpcServer::getHalfPingUpperBound() {
  double ping = 0;
  for (const auto& it : endpoints) {
    ping = max(ping, it.second->getHalfPingUpperBound());
  }
  ping = min(ping, 1000000.0);
  return ping;
}

}  // namespace wga