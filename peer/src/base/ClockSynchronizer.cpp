#include "ClockSynchronizer.hpp"

#include "AdamOptimizer.hpp"
#include "TimeHandler.hpp"

namespace wga {
SlidingWindowEstimator offsetEstimator;
AdamOptimizer offsetOptimizer(0, 0.1);
void ClockSynchronizer::handleReply(const RpcId& id, int64_t requestReceiveTime,
                                    int64_t replySendTime) {
  lock_guard<mutex> guard(clockMutex);
  int64_t requestSendTime = requestSendTimeMap.at(id);
  requestSendTimeMap.erase(requestSendTimeMap.find(id));
  int64_t replyReceiveTime =
      timeHandler->currentTimeMicros() + timeHandler->getTimeShift();
  updateDrift(requestSendTime, requestReceiveTime, replySendTime,
              replyReceiveTime);
}

double ClockSynchronizer::getOffset() {
  lock_guard<mutex> guard(clockMutex);
  return offsetEstimator.getMean();
}

void ClockSynchronizer::updateDrift(int64_t requestSendTime,
                                    int64_t requestReceiptTime,
                                    int64_t replySendTime,
                                    int64_t replyReceiveTime) {
  int64_t ping_2 = int64_t(pingEstimator.getMean() / 2.0);
  int64_t timeOffsetRequest =
      -1 * ((requestReceiptTime - requestSendTime) - ping_2);
  int64_t timeOffsetReply = ((replyReceiveTime - replySendTime) - ping_2);
  int64_t timeOffset = (((timeOffsetRequest + (timeOffsetReply)) / 2));
  int64_t ping = (replyReceiveTime - requestSendTime) -
                 (replySendTime - requestReceiptTime);
  pingEstimator.addSample(double(ping));
  LOG_EVERY_N(100, INFO) << "Time offset: " << timeOffset << " "
                         << int64_t(offsetEstimator.getMean()) << " "
                         << requestReceiptTime << " " << requestSendTime << " "
                         << replyReceiveTime << " " << replySendTime << " "
                         << ping_2 << endl;
  LOG_EVERY_N(100, INFO) << "Ping: " << ping << " " << pingEstimator.getMean()
                         << " " << pingEstimator.getVariance() << endl;
  auto oldMean = offsetEstimator.getMean();
  count++;
  if (connectedToHost) {
    if (count < 10) {
      // offsetOptimizer.force(timeOffset);
      baselineOffset += timeOffset;
    } else if (count == 10) {
      // Average to get final baseline
      baselineOffset += timeOffset;
      baselineOffset /= count;
    } else {
      offsetOptimizer.updateWithLabel((timeOffset - baselineOffset) /
                                      1000000.0);
      offsetEstimator.addSample(double(timeOffset - baselineOffset));
    }
    auto oldTimeShift = timeHandler->getTimeShift();
    // auto newTimeShift = int64_t(offsetEstimator.getMean()) + baselineOffset;
    auto newTimeShift =
        int64_t((1000000 * offsetOptimizer.getCurrentValue())) + baselineOffset;
    timeHandler->setTimeShift(newTimeShift);
    auto timeShiftDifference = newTimeShift - oldTimeShift;
    LOG_EVERY_N(100, INFO) << "Time offset changed by " << timeShiftDifference;
    LOG_EVERY_N(100, INFO) << "Time offsets " << offsetEstimator.getMean()
                           << " vs "
                           << 1000000 * offsetOptimizer.getCurrentValue();
  }
#if 0
  for (auto& it : requestSendTimeMap) {
    it.second = it.second + timeShiftDifference;
  }
  for (auto& it : requestReceiveTimeMap) {
    it.second = it.second + timeShiftDifference;
  }
#endif
}

}  // namespace wga