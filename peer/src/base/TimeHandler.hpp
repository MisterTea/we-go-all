#ifndef __TIME_HANDLER_H__
#define __TIME_HANDLER_H__

#include "Headers.hpp"

namespace wga {
class TimeHandler {
 public:
  TimeHandler() {}

  virtual ~TimeHandler() {}

  void addNoise() {
    initialTime += chrono::duration_cast<chrono::microseconds>(
                       chrono::minutes(rand() % 100))
                       .count();
  }

  int64_t currentTimeMs() { return currentTimeMicros() / 1000; }

  int64_t currentTimeMicros() { return now() - initialTime; }

  int64_t getTimeShift() { return initialTime; }

  void setTimeShift(int64_t _timeShift) { initialTime = _timeShift; }

 protected:
  virtual int64_t now() = 0;

  int64_t initialTime;
};

class FakeTimeHandler : public TimeHandler {
 public:
  FakeTimeHandler() { initialTime = currentTime = int64_t(0); }

  virtual ~FakeTimeHandler() {}

  void setCurrentTime(int64_t _currentTime) { currentTime = _currentTime; }

  template <class t>
  inline void addTime(t timeToAdd) {
    currentTime +=
        chrono::duration_cast<chrono::microseconds>(timeToAdd).count();
  }

 protected:
  virtual int64_t now() { return currentTime; }

  int64_t currentTime;
};

class SystemClockTimeHandler : public TimeHandler {
 public:
  SystemClockTimeHandler() { initialTime = now(); }

  virtual ~SystemClockTimeHandler() {}

protected:
  virtual int64_t now() {
    return chrono::duration_cast<chrono::microseconds>(
               chrono::high_resolution_clock::now().time_since_epoch())
        .count();
  }
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
