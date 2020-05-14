#ifndef __CHRONO_MAP_H__
#define __CHRONO_MAP_H__

#include "Headers.hpp"

namespace wga {
template <typename K, typename V>
class ChronoMap {
 public:
  ChronoMap() : expirationTime(0) {}

  bool waitForExpirationTime(long expirationTimeToWaitFor) {
    unique_lock<mutex> lk(dataReadyMutex);
    if (dataReady.wait_for(lk, std::chrono::seconds(1),
                           [this, expirationTimeToWaitFor] {
                             return expirationTime > expirationTimeToWaitFor;
                           })) {
      return true;
    }
    return false;
  }

  void put(int64_t startTime, int64_t endTime, unordered_map<K, V> data) {
    lock_guard<mutex> lk(dataReadyMutex);
    if (startTime < 0) {
      LOGFATAL << "Tried to put before start time";
    }
    if (startTime < expirationTime) {
      VLOG(1) << "Tried to add a time interval that overlaps";
      return;
    }
    if (startTime >= endTime) {
      LOGFATAL << "Invalid start/end time: " << startTime << " " << endTime;
    }
    if (startTime != expirationTime) {
      futureData.insert(
          make_pair(startTime, make_tuple(startTime, endTime, data)));
    } else {
      addNextTimeBlock(startTime, endTime, data);
    }
  }

  optional<V> get(int64_t timestamp, const K& key) const {
    lock_guard<mutex> lk(dataReadyMutex);
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
    unordered_set<K> keys;
    {
      lock_guard<mutex> lk(dataReadyMutex);
      for (auto& it : data) {
        keys.insert(it.first);
      }
    }
    unordered_map<K, V> retval;
    for (auto& it : keys) {
      retval[it] = getOrDie(timestamp, it);
    }
    return retval;
  }

  unordered_map<K, V> getChanges(unordered_map<K, V> futureData) const {
    unordered_map<K, V> changes;
    lock_guard<mutex> lk(dataReadyMutex);
    {
      for (auto& it : futureData) {
        auto itInData = data.find(it.first);
        if (itInData == data.end() || itInData->second.rbegin()->second != it.second) {
          VLOG(1) << "GOT NEW VALUE: " << it.first << " = " << it.second;
          changes[it.first] = it.second;
        }
      }
    }
    return changes;
  }

  V getOrDie(int64_t timestamp, const K& key) const {
    auto v = get(timestamp, key);
    if (v == nullopt) {
      LOGFATAL << "Tried to get a null value: " << timestamp << " " << key;
    }
    return *v;
  }

  int64_t getExpirationTime() const {
    lock_guard<mutex> lk(dataReadyMutex);
    return expirationTime;
  }

  bool empty() const {
    lock_guard<mutex> lk(dataReadyMutex);
    return expirationTime == 0;
  }

 protected:
  mutable mutex dataReadyMutex;
  mutable condition_variable dataReady;
  unordered_map<K, map<int64_t, V>> data;
  int64_t expirationTime;
  map<int64_t, tuple<int64_t, int64_t, unordered_map<K, V>>> futureData;

  void addNextTimeBlock(int64_t startTime, int64_t endTime,
                        unordered_map<K, V> newData) {
    if (expirationTime != startTime) {
      LOGFATAL << "Tried to add an invalid time block";
    }
    if (data.empty() && startTime != 0) {
      LOGFATAL << "Inserting into an empty map should always use 0";
    }

    for (auto& it : newData) {
      if (data.find(it.first) == data.end()) {
        // New key.
        data[it.first] = {{startTime, it.second}};
      } else if (!(data[it.first].rbegin()->second == it.second)) {
        // Updated data.  Add new information.
        data[it.first][startTime] = it.second;
      }
    }

    expirationTime = endTime;
    dataReady.notify_all();

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
