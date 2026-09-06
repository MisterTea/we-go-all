#include "MultiEndpointHandler.hpp"

namespace wga {
MultiEndpointHandler::MultiEndpointHandler(
    shared_ptr<NetEngine> _netEngine, shared_ptr<udp::socket> _localSocket,
    const vector<udp::endpoint>& endpoints, bool connectedToHost)
    : UdpBiDirectionalRpc(_netEngine, _localSocket, connectedToHost),
      lastUpdateTime(chrono::steady_clock::now()),
      lastUnrepliedSendTime(chrono::steady_clock::now()),
      lastUnrepliedSendOrKillTime(chrono::steady_clock::now()),
      lastPacketReceiveTime(chrono::steady_clock::now()),
      lastPacketReceiveTimeInitialized(false),
      hasUnrepliedSend(false),
      endpointConfirmed(false) {
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
  endpointConfirmed = true;
  hasUnrepliedSend = false;
  lastPacketReceiveTime = chrono::steady_clock::now();
  lastPacketReceiveTimeInitialized = true;
  BiDirectionalRpc::handleReply(rpcId, payload, requestReceiveTime,
                                replySendTime);
}

void MultiEndpointHandler::send(const string& message) {
  // LOG(INFO) << "SENDING MESSAGE: " << message;
  lock_guard<recursive_mutex> lock(mutex);
  auto now = chrono::steady_clock::now();
  if (!hasUnrepliedSend) {
    hasUnrepliedSend = true;
    lastUnrepliedSendTime = lastUnrepliedSendOrKillTime = now;
  }

  auto elapsedSinceUpdate =
      chrono::duration_cast<chrono::milliseconds>(now - lastUpdateTime).count();
  if (elapsedSinceUpdate >= 50) {
    lastUpdateTime = now;
    VLOG(1) << "UPDATING ENDPOINT HANDLER";
    update();
  }

  UdpBiDirectionalRpc::send(message);
  // Before the first authenticated reply, punch every candidate path in
  // parallel.  Waiting five seconds per address is much too slow for a lobby
  // that deliberately has no relay fallback.
  if (!endpointConfirmed) {
    for (auto it : alternativeEndpoints) {
      auto tmp = activeEndpoint;
      activeEndpoint = it;
      UdpBiDirectionalRpc::send(message);
      activeEndpoint = tmp;
    }
  }

  if (!hasUnrepliedSend) {
    hasUnrepliedSend = true;
    lastUnrepliedSendTime = lastUnrepliedSendOrKillTime = chrono::steady_clock::now();
  }
}

bool MultiEndpointHandler::hasEndpointAndResurrectIfFound(
    const udp::endpoint& endpoint) {
  lock_guard<recursive_mutex> lock(mutex);
  if (bannedEndpoints.find(endpoint) != bannedEndpoints.end()) {
    return false;
  }
  lastPacketReceiveTime = chrono::steady_clock::now();
  lastPacketReceiveTimeInitialized = true;
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
  if (!hasUnrepliedSend) {
    // Nothing to do
    VLOG(1) << "Connection seems to be working";
    return;
  }

  auto now = chrono::steady_clock::now();
  int timeoutMs = endpointConfirmed ? 2000 : 400;
  auto elapsedMs =
      chrono::duration_cast<chrono::milliseconds>(now - lastUnrepliedSendOrKillTime).count();
  if (elapsedMs >= timeoutMs) {
    killEndpoint();
  } else {
    VLOG(1) << "Connection hasn't been dead long enough: "
            << elapsedMs << " < " << timeoutMs << " ms";
  }
}

void MultiEndpointHandler::killEndpoint() {
  auto previousEndpoint = activeEndpoint;
  deadEndpoints.insert(activeEndpoint);
  endpointConfirmed = false;
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
  lastUnrepliedSendOrKillTime = chrono::steady_clock::now();
}

void MultiEndpointHandler::onSendError(const udp::endpoint& destination) {
  lock_guard<recursive_mutex> lock(mutex);
  if (destination == activeEndpoint) {
    LOG(INFO) << "Send error to active endpoint " << destination
              << ", switching endpoint immediately";
    killEndpoint();
  } else {
    auto it = alternativeEndpoints.find(destination);
    if (it != alternativeEndpoints.end()) {
      LOG(INFO) << "Send error to alternative endpoint " << destination
                << ", marking dead";
      alternativeEndpoints.erase(it);
      deadEndpoints.insert(destination);
    }
  }
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
