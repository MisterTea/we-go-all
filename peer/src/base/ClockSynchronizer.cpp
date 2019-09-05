#include "ClockSynchronizer.hpp"

#include "TimeHandler.hpp"

namespace wga {
void ClockSynchronizer::handleReply(const RpcId& id, int64_t requestReceiveTime,
                                    int64_t replySendTime) {
  int64_t requestSendTime = requestSendTimeMap.at(id);
  requestSendTimeMap.erase(requestSendTimeMap.find(id));
  int64_t replyReceiveTime = timeHandler->currentTimeMicros();
  updateDrift(requestSendTime, requestReceiveTime, replySendTime,
              replyReceiveTime);
}

void ClockSynchronizer::updateDrift(int64_t requestSendTime,
                                    int64_t requestReceiptTime,
                                    int64_t replySendTime,
                                    int64_t replyReceiveTime) {
  int64_t ping_2 = int64_t(pingEstimator.getMean() / 2.0);
  int64_t timeOffsetRequest =
      -1 * ((requestReceiptTime - requestSendTime) - ping_2);
  int64_t timeOffsetReply = ((replyReceiveTime - replySendTime) - ping_2);
  int64_t timeOffset = (((timeOffsetRequest + (timeOffsetReply)) / 2)) +
                       int64_t(offsetEstimator.getMean());
  int64_t ping = (replyReceiveTime - requestSendTime) -
                 (replySendTime - requestReceiptTime);
  cout << "Time offset: " << timeOffset << " "
       << int64_t(offsetEstimator.getMean()) << " " << requestReceiptTime << " "
       << requestSendTime << " " << replyReceiveTime << " " << replySendTime
       << " " << ping_2 << endl;
  cout << "Ping: " << ping << endl;
  pingEstimator.addSample(double(ping));
  offsetEstimator.addSample(double(timeOffset));
  auto oldTimeShift = timeHandler->getTimeShift();
  timeHandler->setTimeShift(int64_t(offsetEstimator.getMean()));
  auto newTimeShift = timeHandler->getTimeShift();
  auto timeShiftDifference = newTimeShift - oldTimeShift;
  cout << "Shifting times by: " << timeShiftDifference << endl;
  for (auto& it : requestSendTimeMap) {
    it.second = it.second + timeShiftDifference;
    cout << it.second << endl;
  }
  cout << "***" << endl;
  for (auto& it : requestSendTimeMap) {
    cout << it.second << endl;
  }
  cout << "***" << endl;
  for (auto& it : requestReceiveTimeMap) {
    it.second = it.second + timeShiftDifference;
    cout << it.second << endl;
  }
  cout << "***" << endl;
  for (auto& it : requestReceiveTimeMap) {
    cout << it.second << endl;
  }
  cout << "***" << endl;
#if 0
  if (networkStatsQueue.size() >= 100) {
    VLOG(2) << "Time Sync Info: " << timeOffset << " " << ping << " "
            << (replyReceiveTime - requestSendTime) << " "
            << (replySendTime - requestReceiptTime);
    int64_t sumShift = 0;
    int64_t shiftCount = 0;
    for (int i = 0; i < networkStatsQueue.size(); i++) {
      sumShift += networkStatsQueue.at(i).offset;
      shiftCount++;
    }
    VLOG(2) << "New shift: " << (sumShift / shiftCount);
    auto shift = std::chrono::microseconds{sumShift / shiftCount / int64_t(-5)};
    VLOG(2) << "TIME CHANGE: " << timeHandler->currentTimeMicros();
    timeHandler->timeShift += shift;
    VLOG(2) << "TIME CHANGE: " << timeHandler->currentTimeMicros();
    pingEstimator.addSample(ping);
    VLOG(2) << "Ping Estimate: " << pingEstimator.getMean() << " +/- "
            << sqrt(pingEstimator.getVariance());
    networkStatsQueue.clear();
  }
  networkStatsQueue.push_back({timeOffset, ping});
#endif
}

}  // namespace wga