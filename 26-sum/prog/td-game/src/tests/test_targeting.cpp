#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>
#include <random>
#include <vector>

#include "game/bullet.h"
#include "game/enemy.h"
#include "game/tile.h"
#include "game/tower.h"

using Catch::Approx;

namespace {
/// Exposes AttackTower's protected target selector for direct, deterministic
/// testing of each targeting policy.
class TargetProbe : public AttackTower {
public:
  using AttackTower::AttackTower;
  using AttackTower::select_target;
};
}  // namespace

TEST_CASE("aimed_velocity points at the target", "[targeting]") {
  BulletSpecBase cfg{5.0f, 5.0f, 0.0f};  // fixed speed, no spread
  std::mt19937 rng(42);
  Vec2 src{4.0f, 2.0f};

  SECTION("target to the left") {
    Vec2 v = cfg.aimed_velocity(rng, src, Vec2{2.0f, 2.0f});
    REQUIRE(v.x < 0.0f);
    REQUIRE(v.y == Approx(0.0f).margin(1e-5f));
    REQUIRE(v.length() == Approx(5.0f));
  }
  SECTION("target to the right") {
    Vec2 v = cfg.aimed_velocity(rng, src, Vec2{6.0f, 2.0f});
    REQUIRE(v.x > 0.0f);
    REQUIRE(v.y == Approx(0.0f).margin(1e-5f));
  }
  SECTION("target above") {
    Vec2 v = cfg.aimed_velocity(rng, src, Vec2{4.0f, 0.0f});
    REQUIRE(v.y < 0.0f);
    REQUIRE(v.x == Approx(0.0f).margin(1e-5f));
  }
  SECTION("diagonal target normalizes direction") {
    Vec2 v = cfg.aimed_velocity(rng, Vec2{0.0f, 0.0f}, Vec2{3.0f, 4.0f});
    REQUIRE(v.x == Approx(3.0f));
    REQUIRE(v.y == Approx(4.0f));  // direction (3,4) normalized * speed 5 = (3,4)
  }
}

TEST_CASE("aimed_velocity applies random speed within range", "[targeting]") {
  BulletSpecBase cfg{3.0f, 7.0f, 0.0f};
  std::mt19937 rng(7);
  for (int i = 0; i < 50; ++i) {
    Vec2 v = cfg.aimed_velocity(rng, Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f});
    float speed = v.length();
    REQUIRE(speed >= Approx(3.0f));
    REQUIRE(speed <= Approx(7.0f));
    REQUIRE(v.x > 0.0f);  // still aimed right
  }
}

TEST_CASE("aimed_velocity applies angular spread", "[targeting]") {
  BulletSpecBase cfg{5.0f, 5.0f, 0.5f};  // up to ~0.5 rad spread
  std::mt19937 rng(99);
  Vec2 src{0.0f, 0.0f};
  Vec2 target{10.0f, 0.0f};

  bool saw_nonzero_y = false;
  for (int i = 0; i < 100; ++i) {
    Vec2 v = cfg.aimed_velocity(rng, src, target);
    REQUIRE(v.x > 0.0f);  // still generally rightward
    if (std::abs(v.y) > 1e-4f) saw_nonzero_y = true;
  }
  REQUIRE(saw_nonzero_y);  // spread produced some off-axis shots
}

TEST_CASE("aimed_velocity handles zero target offset", "[targeting]") {
  BulletSpecBase cfg{5.0f, 5.0f, 0.0f};
  std::mt19937 rng(1);
  Vec2 v = cfg.aimed_velocity(rng, Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f});
  // Falls back to rightward rather than a zero/NaN velocity.
  REQUIRE(v.length() == Approx(5.0f));
  REQUIRE(std::isfinite(v.x));
  REQUIRE(std::isfinite(v.y));
}

TEST_CASE("Strongest targeting picks the highest-HP enemy in range", "[targeting][strongest]") {
  Tile tile(Rect(Vec2{0.5f, 0.5f}, 1.0f, 1.0f), 1.0f, true, 1.0f, 1.0f, false);
  // select_target ignores bullet release; pass a no-op factory.
  AttackTower::BulletFactory noop = [](Vec2, Vec2) { return std::unique_ptr<Bullet>{}; };
  TargetProbe
    tower("probe", tile, 100, 0, 1.0f, 5.0f, AttackTower::Targeting::Strongest, noop, 5.0f);

  auto strong =
    std::make_shared<Enemy>("normal", Rect(Vec2{1.5f, 0.5f}, 0.5f, 0.5f), 1.0f, 50, 0, 0.0f);
  auto weak =
    std::make_shared<Enemy>("normal", Rect(Vec2{2.5f, 0.5f}, 0.5f, 0.5f), 1.0f, 10, 0, 0.0f);
  std::vector<std::shared_ptr<Enemy>> enemies{weak, strong};

  REQUIRE(tower.select_target(enemies) == strong.get());
}

TEST_CASE("ExplosiveBullet exposes its splash radius", "[targeting][bullet]") {
  ExplosiveBullet eb(Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f}, 2.5f, 15);
  REQUIRE(eb.radius() == Approx(2.5f));
}
