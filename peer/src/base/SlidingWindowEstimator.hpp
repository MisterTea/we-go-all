#pragma once

#include "Headers.hpp"

namespace wga {
class SlidingWindowEstimator {
 public:
  SlidingWindowEstimator() : mean(0), variance(0) {}

  void addSample(double newSample) {
    samples.push_back(newSample);
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
    return mean;
  }
  double getVariance() {
    return variance;
  }
  double getUpperBound() {
    if (samples.size() == 0) {
      return 0;
    }
    vector<double> sortedSamples(samples.begin(), samples.end());
    sort(sortedSamples.begin(), sortedSamples.end());
    // A percentile reflects recurring transport latency without allowing a
    // single scheduler pause to inflate the control value for minutes.
    size_t const upperBoundIndex = std::min(
        sortedSamples.size() - 1,
        size_t(std::ceil(sortedSamples.size() * 0.95)) - 1);
    return sortedSamples[upperBoundIndex];
  }

 protected:
  double mean;
  double variance;
  deque<double> samples;
  constexpr static int MAX_COUNT = 256;
};
}  // namespace wga
