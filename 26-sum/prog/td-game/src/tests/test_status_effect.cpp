#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "GameFixture.h"
#include "game/enemy.h"
#include "game/factory.h"
#include "game/game.h"
#include "game/path.h"
#include "game/stats.h"
#include "game/status_effect.h"

using Catch::Approx;

#ifndef TD_CONFIG_DIR
# error "TD_CONFIG_DIR must be defined by the build system"
#endif

namespace {
std::string config_dir() { return TD_CONFIG_DIR; }

Path straight_path() { return Path{std::vector<Vec2>{{0.0f, 0.0f}, {10.0f, 0.0f}}}; }

// Build a normal enemy on the given path. The path must outlive the enemy.
std::shared_ptr<Enemy> make_path_enemy(Path const& path, float speed, int hp) {
  EnemyStats s;
  s.speed = speed;
  s.health = hp;
  s.size = 0.5f;
  return make_enemy("normal", s, &path);
}

/// Enemy::update() now resolves terrain from the level map rather than taking it
/// as a parameter. These are status-effect unit tests, so they share one
/// read-only game purely for its map: the straight path runs along level 1's
/// row 0, which is all grass (terrain factor 1.0), keeping the movement math
/// identical to the old explicit 1.0 calls.
Game& default_game() {
  static GameFixture fx{config_dir()};
  return fx.game;
}
}  // namespace

TEST_CASE("SlowEffect reduces movement speed", "[status][slow]") {
  Path path = straight_path();
  auto slowed = make_path_enemy(path, 2.0f, 100);
  slowed->apply_status_effect(std::make_unique<SlowEffect>(0.5f, 10.0f));
  slowed->update(default_game(), 1.0f);
  REQUIRE(slowed->position().x == Approx(1.0f));  // 2 * 1 * 0.5

  auto unslowed = make_path_enemy(path, 2.0f, 100);
  unslowed->update(default_game(), 1.0f);
  REQUIRE(unslowed->position().x == Approx(2.0f));
}

TEST_CASE("SlowEffect is not a root and expires after its duration", "[status][slow]") {
  SlowEffect slow{0.5f, 1.0f};
  REQUIRE_FALSE(slow.roots());
  REQUIRE_FALSE(slow.expired());

  Path path = straight_path();
  auto enemy = make_path_enemy(path, 1.0f, 100);
  enemy->apply_status_effect(std::make_unique<SlowEffect>(0.5f, 1.0f));
  enemy->update(default_game(), 1.1f);  // expire the slow
  float before = enemy->position().x;
  enemy->update(default_game(), 1.0f);
  REQUIRE(enemy->position().x - before == Approx(1.0f));  // full speed again
}

TEST_CASE("PoisonEffect deals one-shot damage on application", "[status][poison]") {
  Path path = straight_path();
  auto enemy = make_path_enemy(path, 1.0f, 100);
  REQUIRE(enemy->health() == 100);

  enemy->apply_status_effect(std::make_unique<PoisonEffect>(25, 2.0f));
  REQUIRE(enemy->health() == 75);  // one-shot, immediate

  enemy->update(default_game(), 0.5f);
  REQUIRE(enemy->health() == 75);  // no further tick damage
}

TEST_CASE("PoisonEffect roots the enemy for its duration", "[status][poison]") {
  Path path = straight_path();
  auto enemy = make_path_enemy(path, 2.0f, 100);
  float start_x = enemy->position().x;

  enemy->apply_status_effect(std::make_unique<PoisonEffect>(0, 1.0f));  // no damage, just root
  enemy->update(default_game(), 0.5f);
  REQUIRE(enemy->position().x == Approx(start_x));  // rooted

  // The effect expires *during* the next update (rooted check is at the start),
  // so the enemy is still held this frame...
  enemy->update(default_game(), 1.0f);
  REQUIRE(enemy->position().x == Approx(start_x));

  // ...and only moves on the following update once the effect is gone.
  enemy->update(default_game(), 1.0f);
  REQUIRE(enemy->position().x > start_x);
}

TEST_CASE("RegenerationEffect heals over time", "[status][regen]") {
  Path path = straight_path();
  auto enemy = make_path_enemy(path, 1.0f, 50);
  enemy->set_health(40);
  REQUIRE(enemy->health() == 40);

  enemy->apply_status_effect(std::make_unique<RegenerationEffect>(20, 1.0f));  // 20 hp/s for 1s
  enemy->update(default_game(), 0.5f);
  REQUIRE(enemy->health() == 50);  // +10
  enemy->update(default_game(), 0.5f);
  REQUIRE(enemy->health() == 60);  // +10 more

  enemy->update(default_game(), 1.0f);  // expired
  REQUIRE(enemy->health() == 60);
}

TEST_CASE("Multiple slow effects stack multiplicatively", "[status]") {
  Path path = straight_path();
  auto enemy = make_path_enemy(path, 2.0f, 100);
  enemy->apply_status_effect(std::make_unique<SlowEffect>(0.5f, 10.0f));
  enemy->apply_status_effect(std::make_unique<SlowEffect>(0.5f, 10.0f));
  enemy->update(default_game(), 1.0f);
  REQUIRE(enemy->position().x == Approx(0.5f));  // 2 * 1 * 0.5 * 0.5
}
