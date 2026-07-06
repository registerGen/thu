#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "game/path.h"

using Catch::Approx;

TEST_CASE("Path total length", "[path]") {
  Path p{std::vector<Vec2>{{0.0f, 0.0f}, {3.0f, 0.0f}, {3.0f, 4.0f}}};
  REQUIRE(p.total_length() == Approx(7.0f));  // 3 + 4
}

TEST_CASE("Path position_at interpolates along segments", "[path]") {
  Path p{std::vector<Vec2>{{0.0f, 0.0f}, {3.0f, 0.0f}, {3.0f, 4.0f}}};

  REQUIRE(p.position_at(0.0f).x == Approx(0.0f));
  REQUIRE(p.position_at(0.0f).y == Approx(0.0f));

  // Midway through the first (horizontal) segment.
  Vec2 mid = p.position_at(1.5f);
  REQUIRE(mid.x == Approx(1.5f));
  REQUIRE(mid.y == Approx(0.0f));

  // At the first corner.
  Vec2 corner = p.position_at(3.0f);
  REQUIRE(corner.x == Approx(3.0f));
  REQUIRE(corner.y == Approx(0.0f));

  // Along the second (vertical) segment.
  Vec2 up = p.position_at(5.0f);  // 2 into the vertical segment
  REQUIRE(up.x == Approx(3.0f));
  REQUIRE(up.y == Approx(2.0f));

  // At the end.
  Vec2 end = p.position_at(7.0f);
  REQUIRE(end.x == Approx(3.0f));
  REQUIRE(end.y == Approx(4.0f));
}

TEST_CASE("Path position_at clamps", "[path]") {
  Path p{std::vector<Vec2>{{0.0f, 0.0f}, {2.0f, 0.0f}}};

  // Below zero -> start.
  REQUIRE(p.position_at(-5.0f).x == Approx(0.0f));
  // Above total -> end.
  REQUIRE(p.position_at(100.0f).x == Approx(2.0f));
}

TEST_CASE("Path with a single waypoint", "[path]") {
  Path p{std::vector<Vec2>{{1.0f, 2.0f}}};
  REQUIRE(p.total_length() == Approx(0.0f));
  REQUIRE(p.position_at(0.0f).x == Approx(1.0f));
  REQUIRE(p.position_at(0.0f).y == Approx(2.0f));
}

TEST_CASE("Path empty default", "[path]") {
  Path p;
  REQUIRE(p.total_length() == Approx(0.0f));
}

TEST_CASE("Path supports diagonal segments", "[path]") {
  // A purely diagonal route (45°): enemies can move along non-axis-aligned paths.
  Path p{std::vector<Vec2>{{0.0f, 0.0f}, {3.0f, 3.0f}}};
  REQUIRE(p.total_length() == Approx(std::sqrt(18.0f)));  // ~4.243

  Vec2 mid = p.position_at(p.total_length() * 0.5f);  // halfway along the diagonal
  REQUIRE(mid.x == Approx(1.5f));
  REQUIRE(mid.y == Approx(1.5f));

  // One unit of distance along the diagonal -> (1/√2, 1/√2).
  Vec2 one = p.position_at(1.0f);
  float s = std::sqrt(0.5f);
  REQUIRE(one.x == Approx(s));
  REQUIRE(one.y == Approx(s));
}
