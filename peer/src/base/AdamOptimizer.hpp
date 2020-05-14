#pragma once

#include "Headers.hpp"

namespace wga {
class AdamOptimizer {
 public:
  AdamOptimizer(double _initialValue, double _learningRate = 0.01)
      : learningRate(_learningRate),
        averageGradient(0),
        averageSquaredGradient(0),
        normFactor(_initialValue),
        currentValue(1.0) {}

  void update(double gradient) {
    averageGradient = (BETA1 * averageGradient) + (ONE_MINUS_BETA1 * gradient);
    averageSquaredGradient = (BETA2 * averageSquaredGradient) +
                             (ONE_MINUS_BETA2 * (gradient * gradient));
    currentValue -= learningRate *
                    (averageGradient / sqrt(averageSquaredGradient) + EPSILON);
  }

  void updateWithLabel(double label) {
    //LOG(INFO) << "L1 LOSS: " << currentValue - (label/normFactor);
    double oldValue = currentValue;
    update(currentValue - (label/normFactor));
  }

  void force(double value) { 
    normFactor=value;
    currentValue = 1.0;
  }

  double getCurrentValue() { return currentValue*normFactor; }

 protected:
  double learningRate;
  double averageGradient;
  double averageSquaredGradient;
  double normFactor;
  double currentValue;
  constexpr static double BETA1 = 0.9;
  constexpr static double ONE_MINUS_BETA1 = 1.0 - BETA1;
  constexpr static double BETA2 = 0.99;
  constexpr static double ONE_MINUS_BETA2 = 1.0 - BETA2;
  constexpr static double EPSILON = 1e-10;
};
}  // namespace wga