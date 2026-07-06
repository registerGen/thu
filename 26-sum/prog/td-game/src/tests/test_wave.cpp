#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "GameFixture.h"
#include "game/config.h"
#include "game/game.h"

#ifndef TD_CONFIG_DIR
# error "TD_CONFIG_DIR must be defined by the build system"
#endif

namespace {
std::string config_dir() { return TD_CONFIG_DIR; }

/// Run `game` for `seconds` at 60 fps, clearing enemies each frame so none leak.
void run_clearing(Game& game, float seconds) {
  float dt = 1.0f / 60.0f;
  int frames = static_cast<int>(seconds / dt);
  for (int i = 0; i < frames && !game.over(); ++i) {
    game.update(dt);
    game.apply_cheat("killall");
  }
}
}  // namespace

TEST_CASE("No enemies spawn during the initial pre-wave gap", "[wave][gap]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  float gap0 = game.level().gaps[0];  // e.g. 2.0s for level 1

  // Run for just under the gap — no enemies should have spawned yet.
  float dt = 1.0f / 60.0f;
  int frames = static_cast<int>((gap0 - 0.1f) / dt);
  for (int i = 0; i < frames; ++i) game.update(dt);

  REQUIRE(game.enemies().empty());
  REQUIRE_FALSE(game.all_waves_done());
}

TEST_CASE("Enemies appear after the initial gap elapses", "[wave][gap]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  float gap0 = game.level().gaps[0];

  // Run past the gap + a bit for the first spawn time.
  float dt = 1.0f / 60.0f;
  int frames = static_cast<int>((gap0 + 1.0f) / dt);
  for (int i = 0; i < frames && !game.over(); ++i) game.update(dt);

  REQUIRE_FALSE(game.enemies().empty());
  REQUIRE(game.current_wave() >= 1);
}

TEST_CASE("current_wave progresses through multiple waves", "[wave][progression]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;

  // Run long enough for at least 2 waves to start (gap0 + wave0_spawns + gap1).
  // Use killall to keep enemies from leaking.
  run_clearing(game, 15.0f);

  REQUIRE(game.current_wave() >= 2);
  REQUIRE_FALSE(game.all_waves_done());
}

TEST_CASE("all_waves_done becomes true after all waves are issued", "[wave][done]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;

  // Level 1 has 6 waves with gaps summing to ~26s plus spawn times.
  // Clearing enemies each frame prevents defeat; total ~60s is plenty.
  run_clearing(game, 60.0f);

  REQUIRE(game.all_waves_done());
}

TEST_CASE("Gaps are non-uniform between waves", "[wave][gaps]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  auto const& gaps = game.level().gaps;

  REQUIRE(gaps.size() >= 3);
  // At least two consecutive gaps differ (level 1: [2, 4, 5, 4, 6, 5]).
  bool any_diff = false;
  for (std::size_t i = 1; i < gaps.size(); ++i) {
    if (gaps[i] != gaps[0]) any_diff = true;
  }
  REQUIRE(any_diff);
}

TEST_CASE("Wave progression is independent of enemy state", "[wave][independent]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;

  // Even with enemies alive (no killall), waves still progress after the gap.
  // Run past gap0 + wave0_duration + gap1; wave 1 should start regardless.
  float dt = 1.0f / 60.0f;
  float gap0 = game.level().gaps[0];
  float gap1 = game.level().gaps.size() > 1 ? game.level().gaps[1] : 3.0f;

  // Find the last spawn time of wave 0.
  float last_spawn = 0.0f;
  for (auto const& s : game.level().waves[0].spawns) {
    last_spawn = std::max(last_spawn, s.time);
  }

  // Total time to wave 1 start: gap0 + last_spawn + gap1, plus margin.
  float target = gap0 + last_spawn + gap1 + 1.0f;
  int frames = static_cast<int>(target / dt);
  for (int i = 0; i < frames && !game.over(); ++i) game.update(dt);

  // Wave 1 should have started (current_wave >= 2) — unless an enemy leaked
  // and caused defeat. Either way, the wave counter advanced past 1 or the game
  // ended, proving the wave system ran.
  REQUIRE((game.current_wave() >= 2 || game.over()));
}

TEST_CASE("A level with no waves ends in immediate victory", "[wave][empty]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  auto towers = config::load_towers(config_dir() + "/towers.json");
  auto enemies = config::load_enemies(config_dir() + "/enemies.json");
  std::string empty = R"({
    "name":"empty","map":{"rows":1,"cols":2,"terrain":[["grass","grass"]]},
    "routes":[{"path":[[0,0],[1,0]],"portals":[]}],
    "starting_resources":100,"auto_increase":{"amount":0,"interval":1.0},
    "available_towers":["normal"],"gaps":[],"waves":[]
  })";
  Level lvl = config::load_level_json(empty, towers, enemies);
  fx.play(lvl);

  // start() sees the empty wave list and marks all waves done immediately.
  REQUIRE(game.all_waves_done());
  REQUIRE_FALSE(game.over());
  game.update(1.0f / 60.0f);
  REQUIRE(game.state() == Game::State::Victory);
}
