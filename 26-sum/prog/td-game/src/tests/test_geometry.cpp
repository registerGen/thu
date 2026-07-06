#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <numbers>

#include "game/geometry.h"

using Catch::Approx;

TEST_CASE("Vec2 arithmetic", "[geometry][vec2]") {
  Vec2 a{1.0f, 2.0f};
  Vec2 b{3.0f, 4.0f};

  SECTION("addition and subtraction") {
    Vec2 s = a + b;
    REQUIRE(s.x == 4.0f);
    REQUIRE(s.y == 6.0f);
    Vec2 d = b - a;
    REQUIRE(d.x == 2.0f);
    REQUIRE(d.y == 2.0f);
  }

  SECTION("scalar multiply and divide") {
    Vec2 m = a * 3.0f;
    REQUIRE(m.x == 3.0f);
    REQUIRE(m.y == 6.0f);
    Vec2 q = b / 2.0f;
    REQUIRE(q.x == 1.5f);
    REQUIRE(q.y == 2.0f);
  }

  SECTION("in-place compound assignment") {
    a += b;
    REQUIRE(a.x == 4.0f);
    REQUIRE(a.y == 6.0f);
    a -= Vec2{1.0f, 1.0f};
    REQUIRE(a.x == 3.0f);
    REQUIRE(a.y == 5.0f);
    a *= 2.0f;
    REQUIRE(a.x == 6.0f);
    REQUIRE(a.y == 10.0f);
  }

  SECTION("default construct is zero") {
    Vec2 z;
    REQUIRE(z.x == 0.0f);
    REQUIRE(z.y == 0.0f);
  }
}

TEST_CASE("Vec2 length and normalization", "[geometry][vec2]") {
  Vec2 v{3.0f, 4.0f};
  REQUIRE(v.length() == Approx(5.0f));
  REQUIRE(v.length_sq() == Approx(25.0f));

  Vec2 n = v.normalized();
  REQUIRE(n.length() == Approx(1.0f));
  REQUIRE(n.x == Approx(0.6f));
  REQUIRE(n.y == Approx(0.8f));

  SECTION("zero vector normalizes to zero") {
    Vec2 z{0.0f, 0.0f};
    Vec2 zn = z.normalized();
    REQUIRE(zn.x == 0.0f);
    REQUIRE(zn.y == 0.0f);
  }
}

TEST_CASE("Vec2 distance, dot, cross", "[geometry][vec2]") {
  Vec2 a{0.0f, 0.0f};
  Vec2 b{3.0f, 4.0f};
  REQUIRE(a.distance(b) == Approx(5.0f));

  Vec2 u{1.0f, 0.0f};
  Vec2 v{0.0f, 1.0f};
  REQUIRE(u.dot(v) == Approx(0.0f));
  REQUIRE(u.cross(v) == Approx(1.0f));   // counter-clockwise
  REQUIRE(v.cross(u) == Approx(-1.0f));  // clockwise
  REQUIRE(u.dot(u) == Approx(1.0f));
}

TEST_CASE("Vec2 rotated", "[geometry][vec2]") {
  Vec2 x{1.0f, 0.0f};
  Vec2 up = x.rotated(std::numbers::pi_v<float> / 2.0f);
  REQUIRE(up.x == Approx(0.0f).margin(1e-5f));
  REQUIRE(up.y == Approx(1.0f));

  Vec2 back = x.rotated(std::numbers::pi_v<float>);
  REQUIRE(back.x == Approx(-1.0f).margin(1e-5f));
  REQUIRE(back.y == Approx(0.0f).margin(1e-5f));

  // 360-degree rotation is identity.
  Vec2 around = x.rotated(2.0f * std::numbers::pi_v<float>);
  REQUIRE(around.x == Approx(1.0f).margin(1e-4f));
  REQUIRE(around.y == Approx(0.0f).margin(1e-4f));
}

TEST_CASE("Rect contains", "[geometry][rect]") {
  Rect r{Vec2{5.0f, 5.0f}, 4.0f, 2.0f};  // spans x[3,7], y[4,6]

  REQUIRE(r.contains(Vec2{5.0f, 5.0f}));  // center
  REQUIRE(r.contains(Vec2{3.0f, 4.0f}));  // bottom-left corner
  REQUIRE(r.contains(Vec2{7.0f, 6.0f}));  // top-right corner
  REQUIRE_FALSE(r.contains(Vec2{2.9f, 5.0f}));
  REQUIRE_FALSE(r.contains(Vec2{7.1f, 5.0f}));
  REQUIRE_FALSE(r.contains(Vec2{5.0f, 3.9f}));
}

TEST_CASE("Rect offset", "[geometry][rect]") {
  Rect r{Vec2{0.0f, 0.0f}, 2.0f, 2.0f};
  Rect moved = r + Vec2{3.0f, 1.0f};
  REQUIRE(moved.center.x == 3.0f);
  REQUIRE(moved.center.y == 1.0f);
  REQUIRE(moved.width == 2.0f);
  REQUIRE(moved.height == 2.0f);

  r += Vec2{1.0f, 1.0f};
  REQUIRE(r.center.x == 1.0f);
  REQUIRE(r.center.y == 1.0f);
}
