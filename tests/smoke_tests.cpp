#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("glyph smoke test binary runs") {
  const std::string name = "glyph";
  REQUIRE(name.size() == 5);
}
