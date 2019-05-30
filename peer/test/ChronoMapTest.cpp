#include "Headers.hpp"

#include "ChronoMap.hpp"

#undef CHECK
#include "Catch2/single_include/catch2/catch.hpp"

namespace wga {
TEST_CASE("Simple", "[ChronoMap]") {
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

}  // namespace wga