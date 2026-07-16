#include "AdamOptimizer.hpp"
#include "Base64.hpp"
#include "Headers.hpp"
#include "MessageReader.hpp"
#include "MessageWriter.hpp"
#include "PidController.hpp"
#include "RpcId.hpp"
#include "SlidingWindowEstimator.hpp"
#include "WelfordEstimator.hpp"

#undef CHECK
#include "Catch2/single_include/catch2/catch.hpp"

namespace wga {

TEST_CASE("Base64RoundTrip") {
  REQUIRE(b64::Base64::Encode("") == "");
  REQUIRE(b64::Base64::Encode("f") == "Zg==");
  REQUIRE(b64::Base64::Encode("fo") == "Zm8=");
  REQUIRE(b64::Base64::Encode("foo") == "Zm9v");
  REQUIRE(b64::Base64::Decode("") == "");
  REQUIRE(b64::Base64::Decode("Zg==") == "f");
  REQUIRE(b64::Base64::Decode("Zm8=") == "fo");
  REQUIRE(b64::Base64::Decode("Zm9v") == "foo");
  REQUIRE(b64::Base64::Decode("abc") ==
          "Input data size is not a multiple of 4");
}

TEST_CASE("MessageWriterReader") {
  MessageWriter writer;
  writer.writePrimitive<int32_t>(42);
  writer.writePrimitive<string>("hello");
  writer.writeMap(unordered_map<string, int>{{"one", 1}, {"two", 2}});
  auto encoded = writer.finish();

  MessageReader reader;
  reader.load(encoded);
  REQUIRE(reader.readPrimitive<int32_t>() == 42);
  REQUIRE(reader.readPrimitive<string>() == "hello");
  auto values = reader.readMap<unordered_map<string, int>>();
  REQUIRE(values.at("one") == 1);
  REQUIRE(values.at("two") == 2);
  REQUIRE(reader.sizeRemaining() == 0);

  array<char, 2> bytes = {{'a', 'b'}};
  REQUIRE_THROWS_AS(reader.load(bytes, 3), runtime_error);
}

TEST_CASE("PidControllerAndEstimators") {
  PidController pid(1.0, 5.0, -5.0, 1.0, 0.5, 0.25);
  REQUIRE(pid.calculate(10, 0) == 5.0);
  REQUIRE(pid.calculate(0, 10) == -5.0);

  WelfordEstimator welford;
  REQUIRE(welford.getVariance() == 0);
  welford.addSample(1);
  welford.addSample(2);
  welford.addSample(3);
  REQUIRE(welford.getMean() == Approx(2));
  REQUIRE(welford.getVariance() == Approx(2.0 / 3.0));
  REQUIRE(welford.getUpperBound() == Approx(2 + sqrt(2.0 / 3.0)));

  SlidingWindowEstimator window;
  REQUIRE(window.getUpperBound() == 0);
  window.addSample(1);
  window.addSample(2);
  window.addSample(3);
  REQUIRE(window.getMean() == Approx(2));
  REQUIRE(window.getVariance() == Approx(2.0 / 3.0));
  REQUIRE(window.getUpperBound() >= 3);

  AdamOptimizer optimizer(0, 0.1);
  optimizer.update(1);
  REQUIRE(optimizer.getCurrentValue() < 0);
  optimizer.force(2);
  optimizer.updateWithLabel(3);
  REQUIRE(optimizer.getCurrentValue() > 2);
}

TEST_CASE("RpcIdOrderingAndSerialization") {
  RpcId first(1, 2);
  RpcId second(1, 3);
  REQUIRE(first != second);
  REQUIRE(first < second);
  REQUIRE(!first.empty());
  REQUIRE(RpcId().empty());
  REQUIRE(first.str() == "1/2");
}

}  // namespace wga
