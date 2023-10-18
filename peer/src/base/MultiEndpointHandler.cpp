#include "MultiEndpointHandler.hpp"

namespace wga {
MultiEndpointHandler::MultiEndpointHandler(
    shared_ptr<NetEngine> _netEngine, shared_ptr<udp::socket> _localSocket,
    const vector<udp::endpoint>& endpoints, bool connectedToHost)
    : UdpBiDirectionalRpc(_netEngine, _localSocket, connectedToHost),
      lastUpdateTime(time(NULL)),
      lastUnrepliedSendTime(0),
      lastUnrepliedSendOrKillTime(0) {
  if (endpoints.empty()) {
    LOGFATAL << "Passed an empty endpoints array";
  }

  activeEndpoint = endpoints[0];
  for (int a = 1; a < int(endpoints.size()); a++) {
    alternativeEndpoints.insert(endpoints[a]);
  }
}

void MultiEndpointHandler::handleReply(const RpcId& rpcId,
                                       const string& payload,
                                       int64_t requestReceiveTime,
                                       int64_t replySendTime) {
  lock_guard<recursive_mutex> lock(mutex);
  lastUnrepliedSendTime = lastUnrepliedSendOrKillTime = 0;
  BiDirectionalRpc::handleReply(rpcId, payload, requestReceiveTime,
                                replySendTime);
}

void MultiEndpointHandler::send(const string& message) {
  // LOG(INFO) << "SENDING MESSAGE: " << message;
  lock_guard<recursive_mutex> lock(mutex);
  if (lastUnrepliedSendTime == 0) {
    lastUnrepliedSendTime = lastUnrepliedSendOrKillTime = time(NULL);
  }

  if (time(NULL) != lastUpdateTime) {
    lastUpdateTime = time(NULL);
    LOG(INFO) << "UPDATING ENDPOINT HANDLER";
    update();
  }

  UdpBiDirectionalRpc::send(message);
  /*
  if (lastUnrepliedSendTime + 5 < time(NULL)) {
    // Send on all channels
    LOG(INFO) << "SENDING ON ALL CHANNELS";
    for (auto it : alternativeEndpoints) {
      auto tmp = activeEndpoint;
      activeEndpoint = it;
      UdpBiDirectionalRpc::send(message);
      activeEndpoint = tmp;
    }
  }
  */

  if (lastUnrepliedSendTime == 0) {
    lastUnrepliedSendTime = lastUnrepliedSendOrKillTime = time(NULL);
  }
}

bool MultiEndpointHandler::hasEndpointAndResurrectIfFound(
    const udp::endpoint& endpoint) {
  lock_guard<recursive_mutex> lock(mutex);
  if (bannedEndpoints.find(endpoint) != bannedEndpoints.end()) {
    return false;
  }
  if (endpoint == activeEndpoint) {
    return true;
  }
  VLOG(1) << "RESCURRECTING ENDPOINT: " << endpoint;
  for (auto it = alternativeEndpoints.begin(); it != alternativeEndpoints.end();
       it++) {
    if (*it == endpoint) {
      // Promote alternative to active
      alternativeEndpoints.insert(activeEndpoint);
      alternativeEndpoints.erase(endpoint);
      activeEndpoint = endpoint;
      return true;
    }
  }
  for (auto it = deadEndpoints.begin(); it != deadEndpoints.end(); it++) {
    if (*it == endpoint) {
      // Promote dead to active
      deadEndpoints.erase(it);
      alternativeEndpoints.insert(activeEndpoint);
      activeEndpoint = endpoint;
      return true;
    }
  }
  return false;
}

void MultiEndpointHandler::addEndpoints(
    const vector<udp::endpoint>& newEndpoints) {
  for (auto& it : newEndpoints) {
    addEndpoint(it);
  }
}

void MultiEndpointHandler::update() {
  lock_guard<recursive_mutex> lock(mutex);
  if (lastUnrepliedSendTime == 0) {
    // Nothing to do
    LOG(INFO) << "Connection seems to be working";
    return;
  }

  if ((lastUnrepliedSendOrKillTime + 5) < time(NULL)) {
    killEndpoint();
  } else {
    VLOG(1) << "Connection hasn't been dead long enough: "
              << (lastUnrepliedSendOrKillTime + 5) << " < " << time(NULL);
  }
}

void MultiEndpointHandler::killEndpoint() {
  auto previousEndpoint = activeEndpoint;
  // We haven't got anything back for 5 seconds
  deadEndpoints.insert(activeEndpoint);
  if (!alternativeEndpoints.empty()) {
    DRAW_FROM_UNORDERED(it, alternativeEndpoints);
    activeEndpoint = *it;
    alternativeEndpoints.erase(it);
  } else {
    if (deadEndpoints.empty()) {
      LOG(FATAL) << "No endpoints to try!";
    }
    // We have no alternatives, try a dead endpoint
    DRAW_FROM_UNORDERED(it, deadEndpoints);
    activeEndpoint = *it;
    deadEndpoints.erase(it);
  }
  LOG(INFO) << "Trying new endpoint: " << previousEndpoint.address().to_string()
            << ":" << previousEndpoint.port() << " -> "
            << activeEndpoint.address().to_string() << ":"
            << activeEndpoint.port();
  lastUnrepliedSendOrKillTime = time(NULL);
}

void MultiEndpointHandler::banEndpoint(const udp::endpoint& newEndpoint) {
  LOG(INFO) << "Banning endpoint: " << newEndpoint;
  bannedEndpoints.insert(newEndpoint);
  if (activeEndpoint == newEndpoint) {
    killEndpoint();
  }
  {
    auto it = alternativeEndpoints.find(newEndpoint);
    if (it != alternativeEndpoints.end()) {
      alternativeEndpoints.erase(it);
    }
  }
  {
    auto it = deadEndpoints.find(newEndpoint);
    if (it != deadEndpoints.end()) {
      deadEndpoints.erase(it);
    }
  }
}

}  // namespace wga
