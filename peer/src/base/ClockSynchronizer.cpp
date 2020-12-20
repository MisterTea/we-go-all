#include "ClockSynchronizer.hpp"

#include "TimeHandler.hpp"

namespace wga {
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
  return timeHandler->getOffsetEstimator()->getMean();
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
  if (log) {
    LOG_EVERY_N(100, INFO) << "Time offset: " << timeOffset << " "
                           << int64_t(
                                  timeHandler->getOffsetEstimator()->getMean())
                           << " " << requestReceiptTime << " "
                           << requestSendTime << " " << replyReceiveTime << " "
                           << replySendTime << " " << ping_2 << endl;
    LOG_EVERY_N(100, INFO) << "Ping: " << ping << " " << pingEstimator.getMean()
                           << " " << pingEstimator.getVariance() << endl;
  }
  auto oldMean = timeHandler->getOffsetEstimator()->getMean();
  count++;
  if (connectedToHost) {
    if (count < 10) {
      // timeHandler->getOffsetOptimizer()->force(timeOffset);
      baselineOffset += timeOffset;
    } else if (count == 10) {
      // Average to get final baseline
      baselineOffset += timeOffset;
      baselineOffset /= count;
    } else {
      timeHandler->getOffsetOptimizer()->updateWithLabel(
          (timeOffset - baselineOffset) / 1000000.0);
      timeHandler->getOffsetEstimator()->addSample(
          double(timeOffset - baselineOffset));
    }
    auto oldTimeShift = timeHandler->getTimeShift();
    // auto newTimeShift = int64_t(timeHandler->getOffsetEstimator()->getMean())
    // + baselineOffset;
    auto newTimeShift =
        int64_t(
            (1000000 * timeHandler->getOffsetOptimizer()->getCurrentValue())) +
        baselineOffset;
    timeHandler->setTimeShift(newTimeShift);
    if (log) {
      auto timeShiftDifference = newTimeShift - oldTimeShift;
      LOG_EVERY_N(100, INFO)
          << "Time offset changed by " << timeShiftDifference;
      LOG_EVERY_N(100, INFO)
          << "Time offsets " << timeHandler->getOffsetEstimator()->getMean()
          << " vs "
          << 1000000 * timeHandler->getOffsetOptimizer()->getCurrentValue();
    }
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
