#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "game/resource.h"

TEST_CASE("Resource increase/decrease", "[resource]") {
  Resource r(100, Resource::AutoIncrease{0, 1.0f});
  REQUIRE(r.amount() == 100);

  r.increase(50);
  REQUIRE(r.amount() == 150);

  REQUIRE(r.decrease(30));
  REQUIRE(r.amount() == 120);

  // Cannot go below zero: decrease fails.
  REQUIRE_FALSE(r.decrease(1000));
  REQUIRE(r.amount() == 120);  // unchanged on failure

  // Exact amount is allowed.
  REQUIRE(r.decrease(120));
  REQUIRE(r.amount() == 0);
}

TEST_CASE("Resource auto-increase over time", "[resource]") {
  Resource r(0, Resource::AutoIncrease{10, 2.0f});
  REQUIRE(r.amount() == 0);

  // Just under the interval: no increase yet.
  r.update(1.9f);
  REQUIRE(r.amount() == 0);

  // Cross the interval: +10 and the timer resets.
  r.update(0.2f);
  REQUIRE(r.amount() == 10);

  // Next interval.
  r.update(2.0f);
  REQUIRE(r.amount() == 20);

  // Partial progress toward the third interval does not add yet.
  r.update(1.0f);
  REQUIRE(r.amount() == 20);
}

TEST_CASE("Resource auto-increase accumulates multiple intervals", "[resource]") {
  Resource r(0, Resource::AutoIncrease{5, 1.0f});
  // 0.5s steps for 3.5s -> three 1-second intervals elapse -> +15.
  for (int i = 0; i < 7; ++i) r.update(0.5f);
  REQUIRE(r.amount() == 15);
}
