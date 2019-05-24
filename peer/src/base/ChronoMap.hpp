#ifndef __CHRONO_MAP_H__
#define __CHRONO_MAP_H__

#include "Headers.hpp"

namespace wga {
template <typename K, typename V>
class ChronoMap {
 public:
  ChronoMap() : expirationTime(0) {}

  void put(int64_t lastExpirationTime, int64_t newExpirationTime,
           unordered_map<K, V> data) {
    if (lastExpirationTime < 0) {
      LOGFATAL << "Tried to put before start time";
    }
    if (lastExpirationTime < expirationTime) {
      VLOG(1) << "Tried to add a time interval that overlaps";
      return;
    }
    if (lastExpirationTime >= newExpirationTime) {
      LOGFATAL << "Invalid start/end time: " << lastExpirationTime << " "
               << newExpirationTime;
    }
    if (lastExpirationTime != expirationTime) {
      futureData.insert(
          make_pair(lastExpirationTime,
                    make_tuple(lastExpirationTime, newExpirationTime, data)));
    } else {
      addNextTimeBlock(lastExpirationTime, newExpirationTime, data);
    }
  }

  optional<V> get(int64_t timestamp, const K& key) const {
    if (timestamp < 0) {
      LOGFATAL << "Invalid time stamp";
    }
    if (timestamp >= expirationTime) {
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

  unordered_map<K, V> getAll(int64_t timestamp) const {
    unordered_map<K, V> retval;
    for (auto& it : data) {
      retval[it.first] = *(get(timestamp, it.first));
    }
    return retval;
  }

  V getOrDie(int64_t timestamp, const K& key) const {
    auto v = get(timestamp, key);
    if (v == nullopt) {
      LOGFATAL << "Tried to get a null value: " << timestamp << " " << key;
    }
    return *v;
  }

  int64_t getExpirationTime() const { return expirationTime; }

  bool empty() const { return expirationTime == 0; }

 protected:
  unordered_map<K, map<int64_t, V>> data;
  int64_t expirationTime;
  map<int64_t, tuple<int64_t, int64_t, unordered_map<K, V>>> futureData;

  void addNextTimeBlock(int64_t lastExpirationTime, int64_t newExpirationTime,
                        unordered_map<K, V> newData) {
    if (expirationTime != lastExpirationTime) {
      LOGFATAL << "Tried to add an invalid time block";
    }
    if (data.empty() && lastExpirationTime != 0) {
      LOGFATAL << "Inserting into an empty map should always use 0";
    }

    expirationTime = newExpirationTime;
    for (auto& it : newData) {
      if (data.find(it.first) == data.end()) {
        // New key.
        data[it.first] = {{lastExpirationTime, it.second}};
      } else if (!(data[it.first].rbegin()->second == it.second)) {
        // Updated data.  Add new information.
        data[it.first][lastExpirationTime] = it.second;
      }
    }

    if (futureData.empty()) {
      return;
    }

    if (std::get<0>(*(futureData.begin())) == expirationTime) {
      auto newData = futureData.begin()->second;
      futureData.erase(futureData.begin());
      addNextTimeBlock(std::get<0>(newData), std::get<1>(newData),
                       std::get<2>(newData));
    }
  }
};
}  // namespace wga

#endif
