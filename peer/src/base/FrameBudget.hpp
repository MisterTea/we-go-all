#ifndef __WGA_FRAME_BUDGET_H__
#define __WGA_FRAME_BUDGET_H__

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <time.h>

namespace wga {

thread_local inline int64_t frameWaitUs = 0;
thread_local inline int64_t frameNetplayUs = 0;
thread_local inline int64_t frameNetplayWaitUs = 0;

struct FrameTimerBucket
{
	char name[80] = {};
	int64_t us = 0;
	int count = 0;
};
thread_local inline FrameTimerBucket frameTimers[12];
thread_local inline int frameTimerCount = 0;
thread_local inline int64_t frameTimerOtherUs = 0;

thread_local inline int64_t vblankFuWallUs = 0;
thread_local inline int64_t vblankFuCpuUs = 0;
thread_local inline int64_t vblankAfterWallUs = 0;
thread_local inline int64_t vblankAfterCpuUs = 0;
thread_local inline int64_t vblankWaitUs = 0;
thread_local inline int vblankSamples = 0;

inline int64_t threadCpuUs() {
	timespec ts{};
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0)
		return 0;
	return int64_t(ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
}

inline void addVblankSplit(int64_t fuWall, int64_t fuCpu, int64_t afterWall, int64_t afterCpu, int64_t waitUs) {
	vblankFuWallUs += fuWall;
	vblankFuCpuUs += fuCpu;
	vblankAfterWallUs += afterWall;
	vblankAfterCpuUs += afterCpu;
	if (waitUs > 0)
		vblankWaitUs += waitUs;
	vblankSamples++;
}

inline std::string takeVblankSplitDump() {
	std::string out;
	out += "n=";
	out += std::to_string(vblankSamples);
	out += " fu_wall_us=";
	out += std::to_string(vblankFuWallUs);
	out += " fu_cpu_us=";
	out += std::to_string(vblankFuCpuUs);
	out += " after_wall_us=";
	out += std::to_string(vblankAfterWallUs);
	out += " after_cpu_us=";
	out += std::to_string(vblankAfterCpuUs);
	out += " wait_us=";
	out += std::to_string(vblankWaitUs);
	vblankFuWallUs = vblankFuCpuUs = vblankAfterWallUs = vblankAfterCpuUs = vblankWaitUs = 0;
	vblankSamples = 0;
	return out;
}

// Wall time spent sleeping / lockstep-waiting on the emu thread this frame.
inline void addFrameWaitUs(int64_t us) {
  if (us > 0) {
    frameWaitUs += us;
  }
}

inline int64_t takeFrameWaitUs() {
  int64_t const us = frameWaitUs;
  frameWaitUs = 0;
  return us;
}

inline void addFrameNetplayUs(int64_t wallUs, int64_t waitUs) {
  if (wallUs > 0)
    frameNetplayUs += wallUs;
  if (waitUs > 0)
    frameNetplayWaitUs += waitUs;
}

inline int64_t takeFrameNetplayUs() {
  int64_t const us = frameNetplayUs;
  frameNetplayUs = 0;
  return us;
}

inline int64_t takeFrameNetplayWaitUs() {
  int64_t const us = frameNetplayWaitUs;
  frameNetplayWaitUs = 0;
  return us;
}

inline void addFrameTimerUs(char const *name, int64_t us) {
  if (us <= 0)
    return;
  if (!name || !name[0])
    name = "unnamed";
  for (int i = 0; i < frameTimerCount; i++) {
    if (std::strcmp(frameTimers[i].name, name) == 0) {
      frameTimers[i].us += us;
      frameTimers[i].count++;
      return;
    }
  }
  if (frameTimerCount < int(sizeof(frameTimers) / sizeof(frameTimers[0]))) {
    std::snprintf(frameTimers[frameTimerCount].name, sizeof(frameTimers[0].name), "%s", name);
    frameTimers[frameTimerCount].us = us;
    frameTimers[frameTimerCount].count = 1;
    frameTimerCount++;
    return;
  }
  frameTimerOtherUs += us;
}

inline std::string takeFrameTimerDump() {
  FrameTimerBucket tmp[12];
  int const n = frameTimerCount;
  for (int i = 0; i < n; i++)
    tmp[i] = frameTimers[i];
  int64_t const otherUs = frameTimerOtherUs;
  frameTimerCount = 0;
  frameTimerOtherUs = 0;
  for (int i = 0; i < n; i++)
    frameTimers[i] = FrameTimerBucket();
  std::sort(tmp, tmp + n, [](FrameTimerBucket const &a, FrameTimerBucket const &b) {
    return a.us > b.us;
  });
  std::string out;
  for (int i = 0; i < n; i++) {
    if (!out.empty())
      out += " | ";
    out += tmp[i].name;
    out += "=";
    out += std::to_string(tmp[i].us);
    out += "us x";
    out += std::to_string(tmp[i].count);
  }
  if (otherUs > 0) {
    if (!out.empty())
      out += " | ";
    out += "other=";
    out += std::to_string(otherUs);
    out += "us";
  }
  return out;
}

}  // namespace wga

#endif
