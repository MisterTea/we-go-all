#include "MultiEndpointHandler.hpp"

namespace wga {
MultiEndpointHandler::MultiEndpointHandler(
    shared_ptr<NetEngine> _netEngine, shared_ptr<udp::socket> _localSocket,
    const vector<udp::endpoint>& endpoints)
    : UdpBiDirectionalRpc(_netEngine, _localSocket),
      lastUpdateTime(time(NULL)),
      lastUnrepliedSendTime(0) {
  if (endpoints.empty()) {
    LOGFATAL << "Passed an empty endpoints array";
  }

  activeEndpoint = endpoints[0];
  for (int a = 1; a < int(endpoints.size()); a++) {
    alternativeEndpoints.insert(endpoints[a]);
  }
}

void MultiEndpointHandler::send(const string& message) {
  // LOG(INFO) << "SENDING MESSAGE: " << message;
  lock_guard<recursive_mutex> lock(mutex);
  if (lastUnrepliedSendTime == 0) {
    lastUnrepliedSendTime = time(NULL);
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
    lastUnrepliedSendTime = time(NULL);
  }
}

bool MultiEndpointHandler::hasEndpointAndResurrectIfFound(
    const udp::endpoint& endpoint) {
  lock_guard<recursive_mutex> lock(mutex);
  if (endpoint == activeEndpoint) {
    return true;
  }
  for (auto it = alternativeEndpoints.begin(); it != alternativeEndpoints.end();
       it++) {
    if (*it == endpoint) {
      return true;
    }
  }
  for (auto it = deadEndpoints.begin(); it != deadEndpoints.end(); it++) {
    if (*it == endpoint) {
      deadEndpoints.erase(it);
      alternativeEndpoints.insert(*it);
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

  if (lastUnrepliedSendTime + 5 < time(NULL)) {
    auto previousEndpoint = activeEndpoint;
    // We haven't got anything back for 5 seconds
    deadEndpoints.insert(activeEndpoint);
    if (!alternativeEndpoints.empty()) {
      DRAW_FROM_UNORDERED(it, alternativeEndpoints);
      activeEndpoint = *it;
      alternativeEndpoints.erase(it);
    } else {
      // We have no alternatives, try a dead endpoint
      DRAW_FROM_UNORDERED(it, deadEndpoints);
      activeEndpoint = *it;
      deadEndpoints.erase(it);
    }
    LOG(INFO) << "Trying new endpoint: "
              << previousEndpoint.address().to_string() << ":"
              << previousEndpoint.port() << " -> "
              << activeEndpoint.address().to_string() << ":"
              << activeEndpoint.port();
    lastUnrepliedSendTime = time(NULL);
  } else {
    LOG(INFO) << "Connection hasn't been dead long enough: "
              << (lastUnrepliedSendTime + 5) << " < " << time(NULL);
  }
}

}  // namespace wga