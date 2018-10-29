#ifndef __CHRONO_MAP_H__
#define __CHRONO_MAP_H__

#include "Headers.hpp"

namespace wga {
template <typename K, typename V>
class ChronoMap {
 public:
  ChronoMap() : currentTime(0), startTime(0) {}

  ChronoMap(int64_t _startTime) : currentTime(_startTime), startTime(_startTime) {}

  void put(int64_t putStartTime, int64_t endTime, unordered_map<K, V> data) {
    if (putStartTime < startTime) {
      LOG(FATAL) << "Tried to put before start time";
    }
    if (putStartTime < currentTime) {
      VLOG(1) << "Tried to add a time interval that overlaps";
      return;
    }
    if (putStartTime >= endTime) {
      LOG(FATAL) << "Invalid start/end time: " << putStartTime << " " << endTime;
    }
    if (putStartTime != currentTime) {
      futureData.insert(
          make_pair(putStartTime, make_tuple(putStartTime, endTime, data)));
    } else {
      addNextTimeBlock(putStartTime, endTime, data);
    }
  }

  optional<V> get(int64_t timestamp, const K& key) {
    if (timestamp < startTime) {
      LOG(FATAL) << "Tried to put before start time: " << timestamp << " < " << startTime;
    }
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

  int64_t getStartTime() { return startTime; }

  bool empty() { return currentTime == startTime; }

 protected:
  unordered_map<K, map<int64_t, V>> data;
  int64_t currentTime;
  int64_t startTime;
  map<int64_t, tuple<int64_t, int64_t, unordered_map<K, V>>> futureData;

  void addNextTimeBlock(int64_t putStartTime, int64_t endTime, unordered_map<K, V> newData) {
    if (currentTime != putStartTime) {
      LOG(FATAL) << "Tried to add an invalid time block";
    }
    if (data.empty() && putStartTime != startTime) {
      LOG(FATAL)
          << "Inserting into an empty map should always use startTime";
    }

    currentTime = endTime;
    for (auto& it : newData) {
      if (data.find(it.first) == data.end()) {
        // New key.
        data[it.first] = {{putStartTime, it.second}};
      } else if(!(data[it.first].rbegin()->second == it.second)) {
        // Updated data.  Add new information.
        data[it.first][putStartTime] = it.second;
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

};
}  // namespace wga

#endif
