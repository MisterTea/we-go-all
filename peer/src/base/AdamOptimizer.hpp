#pragma once

#include "Headers.hpp"

namespace wga {
class AdamOptimizer {
 public:
  AdamOptimizer(double _initialValue, double _learningRate = 0.01)
      : learningRate(_learningRate),
        averageGradient(0),
        averageSquaredGradient(0),
        currentValue(_initialValue) {}

  double update(double gradient) {
    averageGradient = (BETA1 * averageGradient) + (ONE_MINUS_BETA1 * gradient);
    averageSquaredGradient = (BETA2 * averageSquaredGradient) +
                             (ONE_MINUS_BETA2 * (gradient * gradient));
    currentValue -= learningRate *
                    (averageGradient / sqrt(averageSquaredGradient) + EPSILON);
    return currentValue;
  }

  void force(double value) {
    currentValue = value;
  }

  double getCurrentValue() { return currentValue; }

 protected:
  double learningRate;
  double averageGradient;
  double averageSquaredGradient;
  double currentValue;
  constexpr static double BETA1 = 0.9;
  constexpr static double ONE_MINUS_BETA1 = 1.0 - BETA1;
  constexpr static double BETA2 = 0.99;
  constexpr static double ONE_MINUS_BETA2 = 1.0 - BETA2;
  constexpr static double EPSILON = 1e-10;
};
}  // namespace wga