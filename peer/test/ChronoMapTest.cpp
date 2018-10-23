#include "Headers.hpp"

#include "gtest/gtest.h"

#include "ChronoMap.hpp"

namespace wga {
class ChronoMapTest : public testing::Test {
 public:
  void SetUp() override { srand(time(NULL)); }

  void TearDown() override {}

 protected:
  ChronoMap<string, string> testMap;
};

TEST_F(ChronoMapTest, Simple) {
  optional<string> vOptional;
  string v;

  vOptional = testMap.get(1, "k");
  EXPECT_EQ(vOptional, nullopt);

  testMap.put(0, 2, {{"k", "v"}, {"k2", "v2"}});

  v = testMap.getOrDie(0, "k");
  EXPECT_EQ(v, "v");

  v = testMap.getOrDie(0, "k2");
  EXPECT_EQ(v, "v2");

  v = testMap.getOrDie(1, "k");
  EXPECT_EQ(v, "v");

  v = testMap.getOrDie(1, "k2");
  EXPECT_EQ(v, "v2");

  vOptional = testMap.get(2, "k");
  EXPECT_EQ(vOptional, nullopt);

  testMap.put(2, 3, {{"k", "vv"}, {"k3", "v3"}});

  v = testMap.getOrDie(1, "k");
  EXPECT_EQ(v, "v");

  v = testMap.getOrDie(2, "k");
  EXPECT_EQ(v, "vv");

  vOptional = testMap.get(1, "k3");
  EXPECT_EQ(vOptional, nullopt);

  v = testMap.getOrDie(2, "k3");
  EXPECT_EQ(v, "v3");
}

}  // namespace wga