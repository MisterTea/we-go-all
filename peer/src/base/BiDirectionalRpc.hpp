#ifndef __BIDIRECTIONAL_RPC_H__
#define __BIDIRECTIONAL_RPC_H__

#include "ClockSynchronizer.hpp"
#include "Headers.hpp"
#include "MessageReader.hpp"
#include "MessageWriter.hpp"
#include "PidController.hpp"
#include "RpcId.hpp"

namespace wga {
class IdPayload {
 public:
  IdPayload() {}
  IdPayload(const RpcId& _id, const string& _payload)
      : id(_id), payload(_payload) {}

  RpcId id;
  string payload;
};

extern bool ALL_RPC_FLAKY;

enum RpcHeader {
  REQUEST = 1,
  REPLY = 2,
  ACKNOWLEDGE = 3,
  SHUTDOWN = 4,
  SHUTDOWN_REPLY = 5,
};

class BiDirectionalRpc {
 public:
  BiDirectionalRpc();
  virtual ~BiDirectionalRpc();

  void sendShutdown();
  void shutdown();
  void initTimeShift();
  void heartbeat();
  void barrier() {
    lock_guard<recursive_mutex> guard(mutex);
    onBarrier++;
  }

  RpcId request(const string& payload);
  void requestOneWay(const string& payload);
  virtual void requestWithId(const IdPayload& idPayload);
  virtual void reply(const RpcId& rpcId, const string& payload);
  inline void replyOneWay(const RpcId& rpcId) { reply(rpcId, "OK"); }

  bool hasIncomingRequest() {
    lock_guard<recursive_mutex> guard(mutex);
    return !incomingRequests.empty();
  }
  bool hasIncomingRequestWithId(const RpcId& rpcId) {
    lock_guard<recursive_mutex> guard(mutex);
    return incomingRequests.find(rpcId) != incomingRequests.end();
  }
  IdPayload getFirstIncomingRequest() {
    lock_guard<recursive_mutex> guard(mutex);
    if (!hasIncomingRequest()) {
      LOGFATAL << "Tried to get a request when one doesn't exist";
    }
    return IdPayload(incomingRequests.begin()->first,
                     incomingRequests.begin()->second);
  }

  bool hasIncomingReply() {
    lock_guard<recursive_mutex> guard(mutex);
    return !incomingReplies.empty();
  }

  IdPayload getFirstIncomingReply() {
    lock_guard<recursive_mutex> guard(mutex);
    if (incomingReplies.empty()) {
      LOGFATAL << "Tried to get reply when there was none";
    }
    IdPayload idPayload = IdPayload(incomingReplies.begin()->first,
                                    incomingReplies.begin()->second);
    processedReplies.put(idPayload.id, true);
    incomingReplies.erase(incomingReplies.begin());
    return idPayload;
  }

  bool hasIncomingReplyWithId(const RpcId& rpcId) {
    lock_guard<recursive_mutex> guard(mutex);
    return incomingReplies.find(rpcId) != incomingReplies.end();
  }

  bool hasProcessedReplyWithId(const RpcId& rpcId) {
    lock_guard<recursive_mutex> guard(mutex);
    return processedReplies.exists(rpcId);
  }

  string consumeIncomingReplyWithId(const RpcId& rpcId) {
    lock_guard<recursive_mutex> guard(mutex);
    auto it = incomingReplies.find(rpcId);
    if (it == incomingReplies.end()) {
      LOGFATAL << "Tried to get a reply that didn't exist!";
    }
    string payload = it->second;
    processedReplies.put(it->first, true);
    incomingReplies.erase(it);
    return payload;
  }

  void setFlaky(bool _flaky) { flaky = _flaky; }

  virtual bool receive(const string& message);

  bool hasWork() {
    lock_guard<recursive_mutex> guard(mutex);
    if (shuttingDown) {
      return false;
    }
    for (const auto& it : delayedRequests) {
      if (!it.second.empty()) {
        return true;
      }
    }
    for (const auto& it : outgoingRequests) {
      if (!it.second.empty()) {
        return true;
      }
    }
    for (const auto& it : incomingRequests) {
      if (!it.second.empty()) {
        return true;
      }
    }
#if 0
    // The person we are sending a reply to may have already left
    for (const auto& it : outgoingReplies) {
      if (!it.second.empty()) {
        return true;
      }
    }
#endif
    for (const auto& it : incomingReplies) {
      if (!it.second.empty()) {
        return true;
      }
    }
    return false;
  }

  pair<double, double> getLatency() {
    return make_pair(clockSynchronizer.getPing(),
                     clockSynchronizer.getHalfPingUpperBound(1));
  }

  double getHalfPingUpperBound() {
    return clockSynchronizer.getHalfPingUpperBound(1);
  }

  inline bool isShuttingDown() {
    lock_guard<recursive_mutex> guard(mutex);
    return shuttingDown;
  }

  virtual bool readyToSend() { return true; }

  void resendRandomOutgoingMessage();

 protected:
  unordered_map<RpcId, string> delayedRequests;
  unordered_map<RpcId, string> outgoingRequests;
  unordered_map<RpcId, string> incomingRequests;
  unordered_set<RpcId> oneWayRequests;

  unordered_map<RpcId, string> outgoingReplies;
  unordered_map<RpcId, string> incomingReplies;

  lru_cache<RpcId, bool> processedReplies;

  int64_t onBarrier;
  uint64_t onId;
  bool flaky;
  recursive_mutex mutex;
  bool shuttingDown;

  ClockSynchronizer clockSynchronizer;

  void handleRequest(const RpcId& rpcId, const string& payload);
  virtual void handleReply(const RpcId& rpcId, const string& payload,
                           int64_t requestReceiveTime, int64_t replySendTime);
  void tryToSendBarrier();
  void sendRequest(const RpcId& id, const string& payload, bool batch);
  void sendReply(const RpcId& id, const string& payload, bool batch);
  virtual void sendAcknowledge(const RpcId& uid);
  virtual void addIncomingRequest(const IdPayload& idPayload);
  virtual void addIncomingReply(const RpcId& uid, const string& payload) {
    incomingReplies.emplace(uid, payload);
  }
  virtual void send(const string& message) = 0;
  bool validatePacket(const RpcId& rpcId, const string& payload) {
    return true;
  }
};
}  // namespace wga

#endif  // __BIDIRECTIONAL_RPC_H__