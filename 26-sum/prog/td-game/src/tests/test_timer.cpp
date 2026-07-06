#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "game/timer.h"

using Catch::Approx;

TEST_CASE("CountdownTimer started immediately", "[timer]") {
  CountdownTimer t(2.0f, true);  // start_now = true
  REQUIRE_FALSE(t.is_finished());
  REQUIRE(t.remaining_time() == Approx(2.0f));

  t.update(0.5f);
  REQUIRE(t.remaining_time() == Approx(1.5f));
  REQUIRE_FALSE(t.is_finished());

  t.update(1.5f);
  REQUIRE(t.is_finished());
  REQUIRE(t.remaining_time() == Approx(0.0f));
}

TEST_CASE("CountdownTimer started later is finished until started", "[timer]") {
  CountdownTimer t(1.0f, false);  // start_now = false
  REQUIRE(t.is_finished());       // not started yet
  REQUIRE(t.remaining_time() == Approx(0.0f));

  t.update(5.0f);  // no effect, not started
  REQUIRE(t.is_finished());

  t.reset();  // start it
  REQUIRE_FALSE(t.is_finished());
  REQUIRE(t.remaining_time() == Approx(1.0f));

  t.update(1.0f);
  REQUIRE(t.is_finished());
}

TEST_CASE("CountdownTimer reset restarts the full duration", "[timer]") {
  CountdownTimer t(3.0f, true);
  t.update(2.0f);
  REQUIRE(t.remaining_time() == Approx(1.0f));

  t.reset();
  REQUIRE(t.remaining_time() == Approx(3.0f));
  REQUIRE_FALSE(t.is_finished());
}

TEST_CASE("CountdownTimer does not go negative", "[timer]") {
  CountdownTimer t(1.0f, true);
  t.update(10.0f);  // over-advance
  REQUIRE(t.remaining_time() == Approx(0.0f));
  REQUIRE(t.is_finished());
}

TEST_CASE("CountdownTimer duration is preserved", "[timer]") {
  CountdownTimer t(4.5f, true);
  REQUIRE(t.duration() == Approx(4.5f));
}
