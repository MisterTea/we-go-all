#ifndef __CHRONO_MAP_H__
#define __CHRONO_MAP_H__

#include "Headers.hpp"
#include "TimeHandler.hpp"

namespace wga {
template <typename K, typename V>
class ChronoMap {
 public:
  static constexpr int64_t HISTORY_RETENTION_MS = 10000;

  ChronoMap() : stopWaitingFlag(false), expirationTime(0) {}

  bool waitForExpirationTime(long expirationTimeToWaitFor,
                             int timeoutMs = 1000) {
    if (stopWaitingFlag.load()) {
      return false;
    }
    unique_lock<mutex> lk(dataReadyMutex);
    auto const waitStart = std::chrono::steady_clock::now();
    bool const ready = dataReady.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                           [this, expirationTimeToWaitFor] {
                             return (expirationTime > expirationTimeToWaitFor) ||
                                    stopWaitingFlag.load();
                           });
    addFrameWaitUs(std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::steady_clock::now() - waitStart)
                       .count());
    if (ready) {
      return !stopWaitingFlag.load();
    }
    return false;
  }

  void stopWaiting() {
    stopWaitingFlag.store(true);
    lock_guard<mutex> lk(dataReadyMutex);
    dataReady.notify_all();
  }

  void resumeWaiting() {
    stopWaitingFlag.store(false);
  }

  void put(int64_t startTime, int64_t endTime, unordered_map<K, V> data) {
    lock_guard<mutex> lk(dataReadyMutex);
    putUnlocked(startTime, endTime, std::move(data));
  }

  // Same as put(); named for call sites that receive remote intervals.
  // Out-of-order maps park in futureData until contiguous; never invent gaps.
  void putFromNetwork(int64_t startTime, int64_t endTime,
                      unordered_map<K, V> data) {
    put(startTime, endTime, std::move(data));
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
    unordered_map<K, V> retval;
    lock_guard<mutex> lk(dataReadyMutex);
    if (timestamp < 0) {
      LOGFATAL << "Invalid time stamp";
    }
    if (timestamp >= expirationTime) {
      LOG(INFO) << "Tried to get a key from the future";
      return retval;
    }
    for (const auto& it : data) {
      auto innerItAhead = it.second.upper_bound(timestamp);
      if (innerItAhead == it.second.begin()) {
        continue;
      } else if (innerItAhead == it.second.end()) {
        retval[it.first] = it.second.rbegin()->second;
      } else {
        retval[it.first] = (--innerItAhead)->second;
      }
    }
    return retval;
  }

  unordered_map<K, V> getChanges(unordered_map<K, V> futureData) const {
    unordered_map<K, V> changes;
    lock_guard<mutex> lk(dataReadyMutex);
    {
      for (auto& it : futureData) {
        auto itInData = data.find(it.first);
        if (itInData == data.end() ||
            itInData->second.rbegin()->second != it.second) {
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

  size_t keyCount() const {
    lock_guard<mutex> lk(dataReadyMutex);
    return data.size();
  }

 protected:
  mutable mutex dataReadyMutex;
  mutable condition_variable dataReady;
  std::atomic<bool> stopWaitingFlag;
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
    pruneHistory();
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


  void pruneHistory() {
    int64_t const cutoff = expirationTime - HISTORY_RETENTION_MS;
    if (cutoff <= 0) {
      return;
    }
    for (auto& entry : data) {
      auto& history = entry.second;
      auto firstKept = history.lower_bound(cutoff);
      if (firstKept == history.begin()) {
        continue;
      }
      V baseline = std::prev(firstKept)->second;
      history.erase(history.begin(), firstKept);
      if (history.empty() || history.begin()->first > cutoff) {
        history.emplace(cutoff, std::move(baseline));
      }
    }
  }

  void putUnlocked(int64_t startTime, int64_t endTime, unordered_map<K, V> data) {
    if (startTime < 0) {
      LOGFATAL << "Tried to put before start time";
    }
    if (endTime <= expirationTime) {
      return;
    }
    if (startTime < expirationTime) {
      VLOG(1) << "Clamping overlapping put " << startTime << "->" << endTime
              << " to " << expirationTime << "->" << endTime;
      startTime = expirationTime;
    }
    if (startTime >= endTime) {
      return;
    }
    if (startTime != expirationTime) {
      futureData.insert(
          make_pair(startTime, make_tuple(startTime, endTime, data)));
      return;
    }
    addNextTimeBlock(startTime, endTime, data);
  }
};
}  // namespace wga

#endif
