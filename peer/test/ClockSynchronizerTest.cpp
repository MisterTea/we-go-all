#include "Headers.hpp"

#include "ClockSynchronizer.hpp"

#undef CHECK
#include "Catch2/single_include/catch2/catch.hpp"
using namespace Catch::literals;

namespace wga {
inline void simulate(shared_ptr<FakeTimeHandler> requesterTimeHandler,
                     shared_ptr<FakeTimeHandler> responderTimeHandler,
                     ClockSynchronizer& sync, int PING_2, int PROCESS_TIME,
                     int DRIFT) {
  responderTimeHandler->setTimeShift(DRIFT * 1000);

  for (int a = 0; a < 1000; a++) {
    RpcId first(0, 0);
    sync.createRequest(first);

    cout << "TIME SHIFTS: " << requesterTimeHandler->getTimeShift() << " "
         << responderTimeHandler->getTimeShift() << endl;

    cout << "REQUEST TIME: " << requesterTimeHandler->currentTimeMicros() << " "
         << responderTimeHandler->currentTimeMicros() << endl;
    // Request takes ms to arrive
    if (PING_2) {
      auto delay =
          chrono::milliseconds(int64_t(stats::rnorm(double(PING_2), 0.5)));
      requesterTimeHandler->addTime(delay);
      responderTimeHandler->addTime(delay);
    }

    auto timeBeforeProcess = responderTimeHandler->currentTimeMicros();
    cout << "TIME BEFORE PROCESS: " << requesterTimeHandler->currentTimeMicros()
         << " " << responderTimeHandler->currentTimeMicros() << endl;

    // Reply takes ms to process
    if (PROCESS_TIME) {
      auto delay = chrono::milliseconds(
          int64_t(stats::rnorm(double(PROCESS_TIME), 0.5)));
      requesterTimeHandler->addTime(delay);
      responderTimeHandler->addTime(delay);
    }
    auto replyTime = responderTimeHandler->currentTimeMicros();
    cout << "REPLY TIME: " << requesterTimeHandler->currentTimeMicros() << " "
         << responderTimeHandler->currentTimeMicros() << endl;

    // Reply takes ms to arrive
    if (PING_2) {
      auto delay =
          chrono::milliseconds(int64_t(stats::rnorm(double(PING_2), 0.5)));
      requesterTimeHandler->addTime(delay);
      responderTimeHandler->addTime(delay);
    }
    cout << "REPLY ARRIVE TIME: " << requesterTimeHandler->currentTimeMicros()
         << " " << responderTimeHandler->currentTimeMicros() << endl;

    sync.handleReply(first, timeBeforeProcess, replyTime);
  }

  REQUIRE(sync.getHalfPingUpperBound() >= Approx(PING_2 * 1000).margin(100.0));
  REQUIRE(sync.getPing() == Approx(PING_2 * 2000).margin(100.0));
  REQUIRE(requesterTimeHandler->getTimeShift() ==
          Approx(DRIFT * 1000).margin(100.0));
  REQUIRE(sync.getOffset() == Approx(DRIFT * 1000).margin(100.0));
}

TEST_CASE("OneWay", "[ClockSynchronizer]") {
  shared_ptr<FakeTimeHandler> requesterTimeHandler(new FakeTimeHandler());
  shared_ptr<FakeTimeHandler> responderTimeHandler(new FakeTimeHandler());
  ClockSynchronizer sync(requesterTimeHandler);

  int PING_2 = 0;
  int PROCESS_TIME = 10;
  int DRIFT = 0;

  SECTION("NoDriftNoLag") {
    simulate(requesterTimeHandler, responderTimeHandler, sync, PING_2,
             PROCESS_TIME, DRIFT);
  }

  SECTION("NoDriftConstantLag") {
    PING_2 = 10;
    simulate(requesterTimeHandler, responderTimeHandler, sync, PING_2,
             PROCESS_TIME, DRIFT);
  }

  SECTION("ConstantDriftNoLag") {
    DRIFT = 10;
    simulate(requesterTimeHandler, responderTimeHandler, sync, PING_2,
             PROCESS_TIME, DRIFT);
  }

  SECTION("ConstantDriftConstantLag") {
    PING_2 = 10;
    DRIFT = 10;
    simulate(requesterTimeHandler, responderTimeHandler, sync, PING_2,
             PROCESS_TIME, DRIFT);
  }
}

inline void simulateTwo(shared_ptr<FakeTimeHandler> firstTimeHandler,
                        shared_ptr<FakeTimeHandler> secondTimeHandler,
                        ClockSynchronizer& firstSync,
                        ClockSynchronizer& secondSync, int PING_2,
                        int PROCESS_TIME, int DRIFT) {
  firstTimeHandler->setTimeShift(DRIFT * 1000 / 2 * -1);
  secondTimeHandler->setTimeShift(DRIFT * 1000 / 2);

  RpcId id(0, 0);
  for (int a = 0; a < 100000; a++) {
    firstSync.createRequest(id);
    secondSync.createRequest(id);

    cout << "TIME SHIFTS: " << firstTimeHandler->getTimeShift() << " "
         << secondTimeHandler->getTimeShift() << endl;

    cout << "REQUEST TIME: " << firstTimeHandler->currentTimeMicros() << " "
         << secondTimeHandler->currentTimeMicros() << endl;
    // Request takes ms to arrive
    if (PING_2) {
      auto delay = chrono::microseconds(
          int64_t(stats::rnorm(double(PING_2 * 1000.0), 1.0 * 1000.0)));
      firstTimeHandler->addTime(delay);
      secondTimeHandler->addTime(delay);
    }

    auto timeBeforeProcess = secondTimeHandler->currentTimeMicros();
    cout << "TIME BEFORE PROCESS: " << firstTimeHandler->currentTimeMicros()
         << " " << secondTimeHandler->currentTimeMicros() << endl;

    // Reply takes ms to process
    if (PROCESS_TIME) {
      auto delay = chrono::microseconds(
          int64_t(stats::rnorm(double(PROCESS_TIME * 1000.0), 1.0 * 1000.0)));
      firstTimeHandler->addTime(delay);
      secondTimeHandler->addTime(delay);
    }
    auto replyTime = secondTimeHandler->currentTimeMicros();
    cout << "REPLY TIME: " << firstTimeHandler->currentTimeMicros() << " "
         << secondTimeHandler->currentTimeMicros() << endl;

    // Reply takes ms to arrive
    if (PING_2) {
      auto delay = chrono::microseconds(
          int64_t(stats::rnorm(double(PING_2 * 1000.0), 1.0 * 1000.0)));
      firstTimeHandler->addTime(delay);
      secondTimeHandler->addTime(delay);
    }
    cout << "REPLY ARRIVE TIME: " << firstTimeHandler->currentTimeMicros()
         << " " << secondTimeHandler->currentTimeMicros() << endl;

    firstSync.handleReply(id, timeBeforeProcess, replyTime);
    secondSync.handleReply(id, timeBeforeProcess, replyTime);
  }

  REQUIRE(firstSync.getHalfPingUpperBound() >=
          Approx(PING_2 * 1000).margin(100.0));
  REQUIRE(firstSync.getPing() == Approx(PING_2 * 2000).margin(100.0));
  REQUIRE(secondSync.getHalfPingUpperBound() >=
          Approx(PING_2 * 1000).margin(100.0));
  REQUIRE(secondSync.getPing() == Approx(PING_2 * 2000).margin(100.0));

  REQUIRE(firstTimeHandler->getTimeShift() ==
          Approx(secondTimeHandler->getTimeShift()).margin(100.0));
  REQUIRE(firstSync.getOffset() ==
          Approx(secondSync.getOffset()).margin(100.0));
}

TEST_CASE("TwoWay", "[ClockSynchronizer]") {
  shared_ptr<FakeTimeHandler> firstTimeHandler(new FakeTimeHandler());
  ClockSynchronizer firstSync(firstTimeHandler);

  shared_ptr<FakeTimeHandler> secondTimeHandler(new FakeTimeHandler());
  ClockSynchronizer secondSync(secondTimeHandler);

  int PING_2 = 0;
  int PROCESS_TIME = 10;
  int DRIFT = 0;

  SECTION("NoDriftNoLag") {
    simulateTwo(firstTimeHandler, secondTimeHandler, firstSync, secondSync,
                PING_2, PROCESS_TIME, DRIFT);
  }

  SECTION("NoDriftConstantLag") {
    PING_2 = 10;
    simulateTwo(firstTimeHandler, secondTimeHandler, firstSync, secondSync,
                PING_2, PROCESS_TIME, DRIFT);
  }

  SECTION("ConstantDriftNoLag") {
    DRIFT = 10;
    simulateTwo(firstTimeHandler, secondTimeHandler, firstSync, secondSync,
                PING_2, PROCESS_TIME, DRIFT);
  }

  SECTION("ConstantDriftConstantLag") {
    PING_2 = 10;
    DRIFT = 10;
    simulateTwo(firstTimeHandler, secondTimeHandler, firstSync, secondSync,
                PING_2, PROCESS_TIME, DRIFT);
  }
}
}  // namespace wga