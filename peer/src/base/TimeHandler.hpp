#ifndef __TIME_HANDLER_H__
#define __TIME_HANDLER_H__

#include "Headers.hpp"

namespace wga {
class TimeHandler {
 public:
  static void init() {
    timeval tp;
    gettimeofday(&tp, 0);
    initialTime = (tp.tv_sec) * (int64_t(1000) + (tp.tv_usec / 1000));
  }

  static int64_t currentTimeMs() {
    timeval tp;
    gettimeofday(&tp, 0);
    int64_t curTime = (tp.tv_sec) * (int64_t(1000) + (tp.tv_usec / 1000));
    return curTime - initialTime;
  }

  static int64_t initialTime;
};
}  // namespace wga

#endif
