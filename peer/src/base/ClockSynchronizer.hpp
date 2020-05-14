#ifndef __CLOCK_SYNCHRONIZER_HPP__
#define __CLOCK_SYNCHRONIZER_HPP__

#include "Headers.hpp"

#include "RpcId.hpp"
#include "TimeHandler.hpp"
#include "WelfordEstimator.hpp"
#include "SlidingWindowEstimator.hpp"

namespace wga {
class ClockSynchronizer {
 public:
  ClockSynchronizer(shared_ptr<TimeHandler> _timeHandler, bool _connectedToHost)
      : timeHandler(_timeHandler), count(0), connectedToHost(_connectedToHost) {}

  int64_t createRequest(const RpcId& id) {
    auto now = timeHandler->currentTimeMicros() + timeHandler->getTimeShift();
    if (requestSendTimeMap.find(id) != requestSendTimeMap.end()) {
      LOG(FATAL) << "Duplicate request";
    }
    requestSendTimeMap[id] = now;
    return now;
  }

  int64_t receiveRequest(const RpcId& id) {
    if (requestReceiveTimeMap.find(id) != requestReceiveTimeMap.end()) {
      LOG(FATAL) << "Duplicate request";
    }
    auto now = timeHandler->currentTimeMicros() + timeHandler->getTimeShift();
    requestReceiveTimeMap[id] = now;
    return now;
  }

  pair<int64_t, int64_t> getReplyDuration(
      const RpcId& id) {
    int64_t sendTime = requestReceiveTimeMap.at(id);
    auto now = timeHandler->currentTimeMicros() + timeHandler->getTimeShift();
    return make_pair(sendTime, now);
  }

  void eraseRequestRecieveTime(const RpcId& id) {
    auto it = requestReceiveTimeMap.find(id);
    if (it == requestReceiveTimeMap.end()) {
      LOG(FATAL) << "Tried to remove request time that didn't exist";
    }
    requestReceiveTimeMap.erase(it);
  }

  void handleReply(const RpcId& id, int64_t requestReceiveTime, int64_t replySendTime
                   );

  double getPing() {
    return pingEstimator.getMean();
  }

  double getOffset();

  double getHalfPingUpperBound() {
    return pingEstimator.getUpperBound() / 2.0;
  }

 protected:
  void updateDrift(int64_t requestSendTime,
                   int64_t requestReceiptTime,
                   int64_t replySendTime,
                   int64_t replyReceiveTime);

  shared_ptr<TimeHandler> timeHandler;
  unordered_map<RpcId, int64_t> requestSendTimeMap;
  unordered_map<RpcId, int64_t> requestReceiveTimeMap;
  SlidingWindowEstimator pingEstimator;
  int64_t count;
  bool connectedToHost;
};
}  // namespace wga

#endif
