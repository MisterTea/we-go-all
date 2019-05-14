#pragma once

#include "Headers.hpp"

namespace wga {
class WelfordEstimator {
 public:
  WelfordEstimator() : mean(0), m2(0), count(0) {}

  void addSample(double sample) {
    if (count < MAX_COUNT) count++;
    double meanDiff = sample - mean;
    mean += meanDiff / count;
    double meanDiff2 = sample - mean;
    m2 += meanDiff * meanDiff2;
  }

  double getMean() { return mean; }
  double getVariance() { return m2 / count; }

 protected:
  double mean;
  double m2;
  int count;
  constexpr static int MAX_COUNT = 1000;
};
}  // namespace wga