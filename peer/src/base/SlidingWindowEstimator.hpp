#pragma once

#include "Headers.hpp"

namespace wga {
class SlidingWindowEstimator {
 public:
  SlidingWindowEstimator() : mean(0), variance(0) {}

  void addSample(double sample) {
    lock_guard<mutex> guard(estimatorMutex);
    samples.push_back(sample);
    while (samples.size() > MAX_COUNT) {
      samples.pop_front();
    }
    mean = 0;
    for (double sample : samples) {
      mean += sample;
    }
    mean /= double(samples.size());
    variance = 0;
    for (double sample : samples) {
      variance += ((sample - mean) * (sample - mean));
    }
    variance /= double(samples.size());
  }

  double getMean() {
    lock_guard<mutex> guard(estimatorMutex);
    return mean;
  }
  double getVariance() {
    lock_guard<mutex> guard(estimatorMutex);
    return variance;
  }
  double getUpperBound() {
    if (samples.size() == 0) {
      return 0;
    }
    vector<double> sortedSamples(samples.begin(), samples.end());
    sort(sortedSamples.begin(), sortedSamples.end());
    int upperBoundIndex = int(samples.size()*0.99);
    double retval = sortedSamples[upperBoundIndex];
    if (retval < mean) {
      VLOG(1) << "UPPER BOUND IS WORSE THAN MEAN? " << mean << " " << retval << endl;
    }
    return retval;
  }

 protected:
  double mean;
  double variance;
  deque<double> samples;
  constexpr static int MAX_COUNT = 3600;
  mutex estimatorMutex;
};
}  // namespace wga