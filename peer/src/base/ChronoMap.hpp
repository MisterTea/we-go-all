#ifndef __CHRONO_MAP_H__
#define __CHRONO_MAP_H__

#include "Headers.hpp"

namespace wga {
template <typename K, typename V>
class ChronoMap {
 public:
  ChronoMap() : currentTime(0) {}

  void put(int64_t startTime, int64_t endTime, map<K, V> data) {
    if (startTime < currentTime) {
      VLOG(1) << "Tried to add a time interval that overlaps";
      return;
    }
    if (startTime != currentTime) {
      futureData.insert(
          make_pair(startTime, make_tuple(startTime, endTime, data)));
    } else {
      addNextTimeBlock(startTime, endTime, data);
    }
  }

  void addNextTimeBlock(int64_t startTime, int64_t endTime, map<K, V> newData) {
    if (currentTime != startTime) {
      LOG(FATAL) << "Tried to add an invalid time block";
    }
    if (data.empty() && startTime != 0) {
      LOG(FATAL)
          << "Inserting into an empty map should always have a 0 startTime";
    }

    currentTime = endTime;
    for (auto& it : newData) {
      if (data.find(it.first) == data.end() ||
          data[it.first].rbegin()->second != it.second) {
        // New/updated data.  Add.
        data[it.first][startTime] = it.second;
        LOG(INFO) << "ADDING VALUE " << it.second;
      }
    }

    if (futureData.empty()) {
      return;
    }

    if (std::get<0>(*(futureData.begin())) == currentTime) {
      auto newData = futureData.begin()->second;
      futureData.erase(futureData.begin());
      addNextTimeBlock(std::get<0>(newData), std::get<1>(newData),
                       std::get<2>(newData));
    }
  }

  optional<string> get(int64_t timestamp, const K& key) {
    if (timestamp < 0) {
      LOG(FATAL) << "Invalid time stamp";
    }
    if (timestamp >= currentTime) {
      LOG(INFO) << "Tried to get a key from the future";
      return nullopt;
    }

    auto it = data.find(key);
    if (it == data.end()) {
      LOG(INFO) << "Tried to get a key that doesn't exist";
      return nullopt;
    }

    // Find the first element > timestamp and move back one.
    // innerIt is guaranteed to be <= timestamp
    auto innerItAhead = it->second.upper_bound(timestamp);
    V retval;
    if (innerItAhead == it->second.begin()) {
      return nullopt;
    } else if (innerItAhead == it->second.end()) {
      retval = it->second.rbegin()->second;
    } else {
      retval = (--(innerItAhead))->second;
    }

    // Return the value
    LOG(INFO) << "Got value " << retval;
    return retval;
  }

  V getOrDie(int64_t timestamp, const K& key) {
    auto v = get(timestamp, key);
    if (v == nullopt) {
      LOG(FATAL) << "Tried to get a null value: " << timestamp << " " << key;
    }
    return *v;
  }

  int64_t getCurrentTime() { return currentTime; }

 protected:
  unordered_map<K, map<int64_t, V>> data;
  int64_t currentTime;
  map<int64_t, tuple<int64_t, int64_t, map<K, V>>> futureData;
};
}  // namespace wga

#endif
