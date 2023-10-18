#ifndef __TIME_HANDLER_H__
#define __TIME_HANDLER_H__

#include "AdamOptimizer.hpp"
#include "Headers.hpp"
#include "SlidingWindowEstimator.hpp"

namespace wga {
class TimeHandler {
 public:
  TimeHandler() : noiseShift(0), timeShift(0), offsetOptimizer(0, 0.1) {}

  virtual ~TimeHandler() {}

  void addNoise() {
    lock_guard<recursive_mutex> guard(timeHandlerMutex);
    noiseShift = -1 * chrono::duration_cast<chrono::microseconds>(
                          chrono::seconds(rand() % 30))
                          .count();
  }

  int64_t currentTimeMs() { return currentTimeMicros() / 1000; }

  int64_t currentTimeMicros() {
    lock_guard<recursive_mutex> guard(timeHandlerMutex);
    return now() - (timeShift + noiseShift);
  }

  int64_t getTimeShift() {
    lock_guard<recursive_mutex> guard(timeHandlerMutex);
    return timeShift;
  }

  void setTimeShift(int64_t _timeShift) {
    lock_guard<recursive_mutex> guard(timeHandlerMutex);
    timeShift = _timeShift;
  }

  AdamOptimizer* getOffsetOptimizer() { return &offsetOptimizer; }

  SlidingWindowEstimator* getOffsetEstimator() { return &offsetEstimator; }

 protected:
  virtual int64_t now() = 0;

  int64_t noiseShift;
  int64_t timeShift;
  recursive_mutex timeHandlerMutex;
  SlidingWindowEstimator offsetEstimator;
  AdamOptimizer offsetOptimizer;
};

class FakeTimeHandler : public TimeHandler {
 public:
  FakeTimeHandler() : currentTime(0) {}

  virtual ~FakeTimeHandler() {}

  void setCurrentTime(int64_t _currentTime) { currentTime = _currentTime; }

  template <class t>
  inline void addTime(t timeToAdd) {
    lock_guard<recursive_mutex> guard(timeHandlerMutex);
    currentTime +=
        chrono::duration_cast<chrono::microseconds>(timeToAdd).count();
  }

 protected:
  virtual int64_t now() {
    lock_guard<recursive_mutex> guard(timeHandlerMutex);
    return currentTime;
  }

  int64_t currentTime;
};

class SystemClockTimeHandler : public TimeHandler {
 public:
  SystemClockTimeHandler() {
    initialTime = chrono::duration_cast<chrono::microseconds>(
                      chrono::high_resolution_clock::now().time_since_epoch())
                      .count();
  }

  virtual ~SystemClockTimeHandler() {}

 protected:
  virtual int64_t now() {
    lock_guard<recursive_mutex> guard(timeHandlerMutex);
    return chrono::duration_cast<chrono::microseconds>(
               chrono::high_resolution_clock::now().time_since_epoch())
               .count() -
           initialTime;
  }
  int64_t initialTime;
};

class GlobalClock {
 public:
  static void addNoise() { timeHandler->addNoise(); }

  static int64_t currentTimeMs() { return timeHandler->currentTimeMs(); }

  static int64_t currentTimeMicros() {
    return timeHandler->currentTimeMicros();
  }

  static int64_t getTimeShift() { return timeHandler->getTimeShift(); }

  static void setTimeShift(int64_t timeShift) {
    timeHandler->setTimeShift(timeShift);
  }

  static shared_ptr<SystemClockTimeHandler> timeHandler;
};

}  // namespace wga

#endif
