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

  for (int a = 0; a < 100000; a++) {
    RpcId first(0, 0);
    sync.createRequest(first);

    VLOG(1) << "TIME SHIFTS: " << requesterTimeHandler->getTimeShift() << " "
            << responderTimeHandler->getTimeShift() << endl;

    VLOG(1) << "REQUEST TIME: " << requesterTimeHandler->currentTimeMicros()
            << " " << responderTimeHandler->currentTimeMicros() << endl;
    // Request takes ms to arrive
    if (PING_2) {
      auto delay = chrono::microseconds(
          int64_t(stats::rnorm(double(PING_2 * 1000.0), 1.0 * 1000.0)));
      requesterTimeHandler->addTime(delay);
      responderTimeHandler->addTime(delay);
    }

    auto timeBeforeProcess = responderTimeHandler->currentTimeMicros();
    VLOG(1) << "TIME BEFORE PROCESS: "
            << requesterTimeHandler->currentTimeMicros() << " "
            << responderTimeHandler->currentTimeMicros() << endl;

    // Reply takes ms to process
    if (PROCESS_TIME) {
      auto delay = chrono::microseconds(
          int64_t(stats::rnorm(double(PROCESS_TIME * 1000.0), 1.0 * 1000.0)));
      requesterTimeHandler->addTime(delay);
      responderTimeHandler->addTime(delay);
    }
    auto replyTime = responderTimeHandler->currentTimeMicros();
    VLOG(1) << "REPLY TIME: " << requesterTimeHandler->currentTimeMicros()
            << " " << responderTimeHandler->currentTimeMicros() << endl;

    // Reply takes ms to arrive
    if (PING_2) {
      auto delay = chrono::microseconds(
          int64_t(stats::rnorm(double(PING_2 * 1000.0), 1.0 * 1000.0)));
      requesterTimeHandler->addTime(delay);
      responderTimeHandler->addTime(delay);
    }
    VLOG(1) << "REPLY ARRIVE TIME: "
            << requesterTimeHandler->currentTimeMicros() << " "
            << responderTimeHandler->currentTimeMicros() << endl;

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

  SECTION("NoDriftRandomLag") {
    PING_2 = 10;
    simulate(requesterTimeHandler, responderTimeHandler, sync, PING_2,
             PROCESS_TIME, DRIFT);
  }

  SECTION("RandomDriftNoLag") {
    DRIFT = 10;
    simulate(requesterTimeHandler, responderTimeHandler, sync, PING_2,
             PROCESS_TIME, DRIFT);
  }

  SECTION("RandomDriftRandomLag") {
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

    VLOG(1) << "TIME SHIFTS: " << firstTimeHandler->getTimeShift() << " "
            << secondTimeHandler->getTimeShift() << endl;

    VLOG(1) << "REQUEST TIME: " << firstTimeHandler->currentTimeMicros() << " "
            << secondTimeHandler->currentTimeMicros() << endl;
    // Request takes ms to arrive
    if (PING_2) {
      auto delay = chrono::microseconds(
          int64_t(stats::rnorm(double(PING_2 * 1000.0), 1.0 * 1000.0)));
      firstTimeHandler->addTime(delay);
      secondTimeHandler->addTime(delay);
    }

    auto firstTimeBeforeProcess = firstTimeHandler->currentTimeMicros();
    auto secondTimeBeforeProcess = secondTimeHandler->currentTimeMicros();
    VLOG(1) << "TIME BEFORE PROCESS: " << firstTimeHandler->currentTimeMicros()
            << " " << secondTimeHandler->currentTimeMicros() << endl;

    // Reply takes ms to process
    if (PROCESS_TIME) {
      auto delay = chrono::microseconds(
          int64_t(stats::rnorm(double(PROCESS_TIME * 1000.0), 1.0 * 1000.0)));
      firstTimeHandler->addTime(delay);
      secondTimeHandler->addTime(delay);
    }

    auto firstReplyTime = firstTimeHandler->currentTimeMicros();
    auto secondReplyTime = secondTimeHandler->currentTimeMicros();
    VLOG(1) << "REPLY TIME: " << firstTimeHandler->currentTimeMicros() << " "
            << secondTimeHandler->currentTimeMicros() << endl;

    // Reply takes ms to arrive
    if (PING_2) {
      auto delay = chrono::microseconds(
          int64_t(stats::rnorm(double(PING_2 * 1000.0), 1.0 * 1000.0)));
      firstTimeHandler->addTime(delay);
      secondTimeHandler->addTime(delay);
    }
    VLOG(1) << "REPLY ARRIVE TIME: " << firstTimeHandler->currentTimeMicros()
            << " " << secondTimeHandler->currentTimeMicros() << endl;

    firstSync.handleReply(id, secondTimeBeforeProcess, secondReplyTime);
    secondSync.handleReply(id, firstTimeBeforeProcess, firstReplyTime);
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

  SECTION("NoDriftRandomLag") {
    PING_2 = 10;
    simulateTwo(firstTimeHandler, secondTimeHandler, firstSync, secondSync,
                PING_2, PROCESS_TIME, DRIFT);
  }

  SECTION("RandomDriftNoLag") {
    DRIFT = 10;
    simulateTwo(firstTimeHandler, secondTimeHandler, firstSync, secondSync,
                PING_2, PROCESS_TIME, DRIFT);
  }

  SECTION("RandomDriftRandomLag") {
    PING_2 = 10;
    DRIFT = 10;
    simulateTwo(firstTimeHandler, secondTimeHandler, firstSync, secondSync,
                PING_2, PROCESS_TIME, DRIFT);
  }
}

inline void simulateN(vector<shared_ptr<FakeTimeHandler>> timeHandlers,
                      vector<shared_ptr<ClockSynchronizer>> clockSyncs,
                      int PING_2, int PROCESS_TIME, int DRIFT) {
  const int NUM_CLOCKS = int(timeHandlers.size());
  for (int a = 0; a < NUM_CLOCKS; a++) {
    if (a % 2 == 0) {
      timeHandlers[a]->setTimeShift(DRIFT * 1000 / 2 * -1);
    } else {
      timeHandlers[a]->setTimeShift(DRIFT * 1000 / 2);
    }
  }

  RpcId id(0, 0);
  for (int a = 0; a < 100000; a++) {
    for (int a = 0; a < NUM_CLOCKS; a++) {
      clockSyncs[a]->createRequest(id);
    }

    // Request takes ms to arrive
    if (PING_2) {
      auto delay = chrono::microseconds(
          int64_t(stats::rnorm(double(PING_2 * 1000.0), 1.0 * 1000.0)));
      for (auto handler : timeHandlers) {
        handler->addTime(delay);
      }
    }

    vector<int64_t> timesBeforeProcess;
    for (auto timeHandler : timeHandlers) {
      timesBeforeProcess.push_back(timeHandler->currentTimeMicros());
    }

    // Reply takes ms to process
    if (PROCESS_TIME) {
      auto delay = chrono::microseconds(
          int64_t(stats::rnorm(double(PROCESS_TIME * 1000.0), 1.0 * 1000.0)));
      for (auto handler : timeHandlers) {
        handler->addTime(delay);
      }
    }

    vector<int64_t> replyTimes;
    for (auto timeHandler : timeHandlers) {
      replyTimes.push_back(timeHandler->currentTimeMicros());
    }

    // Reply takes ms to arrive
    if (PING_2) {
      auto delay = chrono::microseconds(
          int64_t(stats::rnorm(double(PING_2 * 1000.0), 1.0 * 1000.0)));
      for (auto handler : timeHandlers) {
        handler->addTime(delay);
      }
    }

    for (int a = 0; a < NUM_CLOCKS; a++) {
      auto clockSync = clockSyncs[a];
      int replyFrom = rand() % NUM_CLOCKS;
      while (replyFrom == a) {
        replyFrom = rand() % NUM_CLOCKS;
      }
      clockSync->handleReply(id, timesBeforeProcess[replyFrom],
                             replyTimes[replyFrom]);
    }
  }

  for (auto clockSync : clockSyncs) {
    REQUIRE(clockSync->getHalfPingUpperBound() >=
            Approx(PING_2 * 1000).margin(100.0));
    REQUIRE(clockSync->getPing() == Approx(PING_2 * 2000).margin(100.0));
    for (auto clockSync2 : clockSyncs) {
      REQUIRE(clockSync->getOffset() ==
              Approx(clockSync2->getOffset()).margin(100.0));
    }
  }

  for (auto timeHandler : timeHandlers) {
    for (auto timeHandler2 : timeHandlers) {
      REQUIRE(timeHandler->getTimeShift() ==
              Approx(timeHandler2->getTimeShift()).margin(100.0));
    }
  }
}

TEST_CASE("EightWay", "[ClockSynchronizer]") {
  const int NUM_CLOCKS = 8;

  vector<shared_ptr<FakeTimeHandler>> timeHandlers;
  vector<shared_ptr<ClockSynchronizer>> clockSyncs;

  for (int a = 0; a < NUM_CLOCKS; a++) {
    timeHandlers.push_back(make_shared<FakeTimeHandler>());
    clockSyncs.push_back(make_shared<ClockSynchronizer>(timeHandlers[a]));
  }

  int PING_2 = 0;
  int PROCESS_TIME = 10;
  int DRIFT = 0;

  SECTION("NoDriftNoLag") {
    simulateN(timeHandlers, clockSyncs, PING_2, PROCESS_TIME, DRIFT);
  }

  SECTION("NoDriftRandomLag") {
    PING_2 = 10;
    simulateN(timeHandlers, clockSyncs, PING_2, PROCESS_TIME, DRIFT);
  }

  SECTION("RandomDriftNoLag") {
    DRIFT = 10;
    simulateN(timeHandlers, clockSyncs, PING_2, PROCESS_TIME, DRIFT);
  }

  SECTION("RandomDriftRandomLag") {
    PING_2 = 10;
    DRIFT = 10;
    simulateN(timeHandlers, clockSyncs, PING_2, PROCESS_TIME, DRIFT);
  }
}

}  // namespace wga