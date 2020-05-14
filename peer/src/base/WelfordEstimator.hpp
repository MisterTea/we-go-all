#pragma once

#include "Headers.hpp"

namespace wga {
class WelfordEstimator {
 public:
  WelfordEstimator() : mean(0), m2(0), count(0) {}

  void addSample(double sample) {
    double meanDiff = sample - mean;
    count++;
    mean += meanDiff / count;
    double meanDiff2 = sample - mean;
    m2 += meanDiff * meanDiff2;
  }

  double getMean() { return mean; }
  double getVariance() { return m2 / count; }
  double getUpperBound() {
    return (getMean()) +
           ((sqrt(getVariance()) * 1));
  }

 protected:
  double mean;
  double m2;
  int count;
  constexpr static int MAX_COUNT = 10000;
};
}  // namespace wga