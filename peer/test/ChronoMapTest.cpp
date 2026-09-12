#include "Headers.hpp"

#include "ChronoMap.hpp"

#undef CHECK
#include "Catch2/single_include/catch2/catch.hpp"

namespace wga {
TEST_CASE("ChronoMapSimple") {
  ChronoMap<string, string> testMap;
  optional<string> vOptional;
  string v;

  vOptional = testMap.get(1, "k");
  REQUIRE(vOptional == nullopt);

  testMap.put(0, 2, {{"k", "v"}, {"k2", "v2"}});

  v = testMap.getOrDie(0, "k");
  REQUIRE(v == "v");

  v = testMap.getOrDie(0, "k2");
  REQUIRE(v == "v2");

  v = testMap.getOrDie(1, "k");
  REQUIRE(v == "v");

  v = testMap.getOrDie(1, "k2");
  REQUIRE(v == "v2");

  vOptional = testMap.get(2, "k");
  REQUIRE(vOptional == nullopt);

  testMap.put(2, 3, {{"k", "vv"}, {"k3", "v3"}});

  v = testMap.getOrDie(1, "k");
  REQUIRE(v == "v");

  v = testMap.getOrDie(2, "k");
  REQUIRE(v == "vv");

  vOptional = testMap.get(1, "k3");
  REQUIRE(vOptional == nullopt);

  v = testMap.getOrDie(2, "k3");
  REQUIRE(v == "v3");

  testMap.put(4, 5, {{"k", "v4"}});

  vOptional = testMap.get(3, "k");
  REQUIRE(vOptional == nullopt);

  vOptional = testMap.get(4, "k");
  REQUIRE(vOptional == nullopt);

  testMap.put(5, 6, {{"k", "v5"}});

  vOptional = testMap.get(3, "k");
  REQUIRE(vOptional == nullopt);

  vOptional = testMap.get(4, "k");
  REQUIRE(vOptional == nullopt);

  testMap.put(3, 4, {{"k", "v3"}});

  v = testMap.getOrDie(3, "k");
  REQUIRE(v == "v3");

  v = testMap.getOrDie(4, "k");
  REQUIRE(v == "v4");

  v = testMap.getOrDie(5, "k");
  REQUIRE(v == "v5");

  testMap.put(3, 4, {{"k", "v333"}});

  v = testMap.getOrDie(3, "k");
  REQUIRE(v == "v3");
}

TEST_CASE("ChronoMapOverlapExtendsFrontier") {
  ChronoMap<string, string> testMap;
  testMap.put(0, 100, {{"k", "a"}});
  REQUIRE(testMap.getExpirationTime() == 100);

  // Rebroadcast that starts in the past but extends past the frontier must
  // advance coverage (not be dropped).
  testMap.put(80, 200, {{"k", "b"}});
  REQUIRE(testMap.getExpirationTime() == 200);
  REQUIRE(testMap.getOrDie(99, "k") == "a");
  REQUIRE(testMap.getOrDie(100, "k") == "b");
  REQUIRE(testMap.getOrDie(199, "k") == "b");

  // Fully obsolete overlap remains a no-op.
  testMap.put(50, 150, {{"k", "c"}});
  REQUIRE(testMap.getExpirationTime() == 200);
  REQUIRE(testMap.getOrDie(100, "k") == "b");
}

TEST_CASE("ChronoMapNetworkGapParksUntilContiguous") {
  ChronoMap<string, string> testMap;
  testMap.put(0, 100, {{"k", "a"}});
  // Lost packet: later interval must not invent coverage for the hole.
  testMap.putFromNetwork(200, 300, {{"k", "b"}});
  REQUIRE(testMap.getExpirationTime() == 100);

  // Contiguous rebroadcast of the missing piece unlocks the parked interval.
  testMap.putFromNetwork(100, 200, {{"k", "a"}});
  REQUIRE(testMap.getExpirationTime() == 300);
  REQUIRE(testMap.getOrDie(150, "k") == "a");
  REQUIRE(testMap.getOrDie(250, "k") == "b");
}

TEST_CASE("ChronoMapPruneHistory") {
  ChronoMap<string, string> testMap;

  // Build a changing history longer than HISTORY_RETENTION_MS.
  for (int64_t t = 0; t < 20000; t += 10) {
    testMap.put(t, t + 10, {{"k", std::to_string(t)}});
  }

  // Recent values inside the retention/cap window must still resolve.
  REQUIRE(testMap.getOrDie(19990, "k") == "19990");
  REQUIRE(testMap.getOrDie(18000, "k") == "18000");

  // Values far older than the retention window are pruned away.
  REQUIRE(testMap.get(0, "k") == nullopt);
  REQUIRE(testMap.keyCount() == 1);
}

}  // namespace wga
