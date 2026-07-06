#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>
#include <string>
#include <vector>

#include "GameFixture.h"
#include "game/bullet.h"
#include "game/config.h"
#include "game/enemy.h"
#include "game/factory.h"
#include "game/game.h"
#include "game/tower.h"

using Catch::Approx;

#ifndef TD_CONFIG_DIR
# error "TD_CONFIG_DIR must be defined by the build system"
#endif

namespace {
std::string config_dir() { return TD_CONFIG_DIR; }

float path_distance_at(Level const& lvl, Vec2 pos) {
  auto const& wps = lvl.routes[0].path.waypoints();
  float cum = 0.0f;
  for (std::size_t i = 0; i < wps.size(); ++i) {
    if (i > 0) cum += (wps[i] - wps[i - 1]).length();
    if (std::abs(wps[i].x - pos.x) < 0.01f && std::abs(wps[i].y - pos.y) < 0.01f) return cum;
  }
  return -1.0f;
}

/// A 7x12 all-grass level with one straight horizontal path on row 3 and every
/// tower type available. Waves are pushed far into the future so they never
/// interfere with manual enemy spawns during a test.
std::string simpleLevelJson() {
  std::string row = "[";
  for (int c = 0; c < 12; ++c) {
    if (c > 0) row += ",";
    row += "\"grass\"";
  }
  row += "]";
  std::string terrain = "[";
  for (int r = 0; r < 7; ++r) {
    if (r > 0) terrain += ",";
    terrain += row;
  }
  terrain += "]";
  return std::string("{") +
         "\"name\":\"Testfield\","
         "\"map\":{\"rows\":7,\"cols\":12,\"terrain\":" +
         terrain +
         "},"
         "\"routes\":[{\"path\":[[0,3],[11,3]],\"portals\":[]}],"
         "\"starting_resources\":1000,"
         "\"auto_increase\":{\"amount\":0,\"interval\":1.0},"
         "\"available_towers\":[\"normal\",\"slow\",\"poison\",\"splash\",\"laser\",\"resource\","
         "\"wall\"],"
         "\"gaps\":[10.0],"
         "\"waves\":[{\"spawns\":[{\"type\":\"normal\",\"time\":100.0,\"route\":0}]}]"
         "}";
}

/// Switch `fx` to the controlled all-grass test level (all towers available).
void useTestLevel(GameFixture& fx) {
  auto towers = config::load_towers(config_dir() + "/towers.json");
  auto enemies = config::load_enemies(config_dir() + "/enemies.json");
  Level lvl = config::load_level_json(simpleLevelJson(), towers, enemies);
  fx.play(lvl);
}

/// Run up to `frames` updates until `pred` is true (or the game ends).
template <typename Pred>
void runUntil(Game& game, Pred pred, int frames, float dt = 1.0f / 60.0f) {
  for (int i = 0; i < frames && !game.over() && !pred(); ++i) game.update(dt);
}
}  // namespace

TEST_CASE("Game loads from config", "[game][config]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  REQUIRE(fx.registry.size() >= 3);
  REQUIRE(fx.registry.current_index() == 0);
  REQUIRE(game.state() == Game::State::Playing);
  REQUIRE_FALSE(game.over());
}

TEST_CASE("place_tower validation", "[game][placement]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;

  SECTION("valid off-path tile succeeds") {
    REQUIRE(game.place_tower("normal", Vec2{0.5f, 0.5f}));  // grass, row 0 col 0
    REQUIRE(game.towers().size() == 1);
  }
  SECTION("towers may be placed on the path (they block enemies)") {
    REQUIRE(game.place_tower("normal", Vec2{5.5f, 3.5f}));  // path row 3 col 5
    REQUIRE(game.towers().size() == 1);
  }
  SECTION("rock tile rejected") {
    REQUIRE_FALSE(game.place_tower("normal", Vec2{1.5f, 2.5f}));  // rock at col 1 row 2
    REQUIRE(game.towers().empty());
  }
  SECTION("wall can be placed on the path") {
    REQUIRE(game.place_tower("wall", Vec2{5.5f, 3.5f}));
    REQUIRE(game.towers().size() == 1);
  }
  SECTION("unavailable tower type rejected") {
    // Level 1 does not offer splash/laser/poison.
    REQUIRE_FALSE(game.place_tower("laser", Vec2{0.5f, 0.5f}));
    REQUIRE(game.towers().empty());
  }
  SECTION("insufficient resources rejected") {
    // Starting resources 150; normal costs 50 -> can place 3, 4th fails.
    REQUIRE(game.place_tower("normal", Vec2{0.5f, 0.5f}));
    REQUIRE(game.place_tower("normal", Vec2{2.5f, 0.5f}));
    REQUIRE(game.place_tower("normal", Vec2{4.5f, 0.5f}));
    REQUIRE_FALSE(game.place_tower("normal", Vec2{6.5f, 0.5f}));
  }
  SECTION("reject when game is paused") {
    game.pause();
    REQUIRE_FALSE(game.place_tower("normal", Vec2{0.5f, 0.5f}));
    game.resume();
    REQUIRE(game.place_tower("normal", Vec2{0.5f, 0.5f}));  // now allowed
  }
}

TEST_CASE("Attack tower fires a bullet aimed at an in-range enemy", "[game][targeting]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  REQUIRE(game.place_tower("normal", Vec2{4.5f, 2.5f}));  // grass, row 2 col 4, near path

  // Spawn an enemy partway along the path (in range of the tower) and make it
  // the "first" (furthest along) target so the tower shoots at it.
  auto enemy =
    make_enemy("normal", game.level().enemy_stats.at("normal"), &game.level().routes[0].path);
  enemy->set_path_distance(7.0f);  // position ~ (4, 3) — near the tower
  game.spawn_enemy(enemy);

  game.update(1.0f / 60.0f);

  REQUIRE(game.bullets().size() >= 1);
  // The bullet must travel generally toward the enemy (down from the tower at
  // (4.5, 2.5) to the enemy near (4, 3)).
  Vec2 v = game.bullets().front()->velocity();
  REQUIRE(v.y > 0.0f);  // enemy is below the tower
}

TEST_CASE("Wall blocks enemies and takes damage", "[game][wall]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  REQUIRE(game.place_tower("wall", Vec2{5.5f, 3.5f}));  // on the path (row 3 col 5)

  int initial_hp = 0;
  for (auto const& t : game.towers())
    if (dynamic_cast<WallTower const*>(t.get())) initial_hp = t->health();
  REQUIRE(initial_hp > 0);

  // Let waves spawn enemies; they reach the wall and attack it.
  float dt = 1.0f / 60.0f;
  for (int i = 0; i < 60 * 8 && !game.over(); ++i) game.update(dt);

  int hp_now = 0;
  for (auto const& t : game.towers())
    if (dynamic_cast<WallTower const*>(t.get())) hp_now = t->health();
  REQUIRE(hp_now < initial_hp);  // wall has been damaged
}

TEST_CASE("Any tower on the path blocks enemies and takes damage", "[game][block]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  // A normal attack tower placed ON the path blocks enemies just like a wall;
  // they stop and attack it. Spawn a tanky enemy just upstream so it reaches
  // the tower before the tower's shots kill it.
  REQUIRE(game.place_tower("normal", Vec2{5.5f, 3.5f}));  // on path, col 5 row 3 (distance ~6)
  auto enemy =
    make_enemy("armored", game.level().enemy_stats.at("armored"), &game.level().routes[0].path);
  enemy->set_path_distance(5.5f);  // just upstream of the tower
  game.spawn_enemy(enemy);

  int initial_hp = 0;
  for (auto const& t : game.towers())
    if (t->type() == "normal") initial_hp = t->health();
  REQUIRE(initial_hp > 0);

  float dt = 1.0f / 60.0f;
  for (int i = 0; i < 60 * 2 && !game.over(); ++i) game.update(dt);

  int hp_now = 0;
  for (auto const& t : game.towers())
    if (t->type() == "normal") hp_now = t->health();
  REQUIRE(hp_now < initial_hp);  // the on-path tower was damaged
}

TEST_CASE("Portal teleports enemies forward", "[game][portal]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  fx.advance();  // level 2 has a portal
  REQUIRE(fx.registry.current_index() == 1);
  REQUIRE_FALSE(game.level().routes[0].portal_dest.empty());

  auto [portal_pos, dest_dist] = game.level().routes[0].portal_dest.front();
  float source_dist = path_distance_at(game.level(), portal_pos);
  REQUIRE(source_dist >= 0.0f);
  REQUIRE(dest_dist > source_dist);  // forward-only

  auto enemy =
    make_enemy("normal", game.level().enemy_stats.at("normal"), &game.level().routes[0].path);
  enemy->set_path_distance(source_dist);
  game.spawn_enemy(enemy);

  game.update(1.0f / 60.0f);

  // The enemy should have jumped to (near) the destination distance.
  REQUIRE(enemy->path_distance() == Approx(dest_dist).epsilon(0.01f));
}

TEST_CASE("Defeat when an enemy reaches the exit", "[game][defeat]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  // No towers: enemies walk straight to the exit.
  float dt = 1.0f / 60.0f;
  for (int i = 0; i < 60 * 30 && !game.over(); ++i) game.update(dt);
  REQUIRE(game.state() == Game::State::Defeat);
  REQUIRE(game.last_result().cleared == false);
  REQUIRE(game.level().index == 1);  // the default-loaded official level (Meadow)
}

TEST_CASE("Victory when all waves are cleared", "[game][victory]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  float dt = 1.0f / 60.0f;
  // Keep enemies cleared so none leak while waiting for all waves to be issued.
  for (int i = 0; i < 60 * 120 && !game.over() && !game.all_waves_done(); ++i) {
    game.update(dt);
    game.apply_cheat("killall");
  }
  REQUIRE(game.all_waves_done());
  game.apply_cheat("killall");
  game.update(dt);  // triggers the victory check
  REQUIRE(game.state() == Game::State::Victory);
  REQUIRE(game.last_result().cleared == true);
}

TEST_CASE("Pause halts the simulation", "[game][pause]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  float t0 = game.elapsed_time();
  game.pause();
  for (int i = 0; i < 60; ++i) game.update(1.0f / 60.0f);
  REQUIRE(game.elapsed_time() == Approx(t0));  // no advance while paused
  REQUIRE(game.state() == Game::State::Playing);

  game.resume();
  game.update(1.0f / 60.0f);
  REQUIRE(game.elapsed_time() > t0);
}

TEST_CASE("Restart resets the level", "[game][restart]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  game.apply_cheat("gold");
  game.place_tower("normal", Vec2{0.5f, 0.5f});
  for (int i = 0; i < 60 && !game.over(); ++i) game.update(1.0f / 60.0f);
  REQUIRE(game.towers().size() == 1);
  REQUIRE(game.score() >= 0);

  game.restart();
  REQUIRE(game.state() == Game::State::Playing);
  REQUIRE(game.towers().empty());
  REQUIRE(game.enemies().empty());
  REQUIRE(game.score() == 0);
  REQUIRE(game.elapsed_time() == Approx(0.0f));
}

TEST_CASE("Cheats", "[game][cheat]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;

  SECTION("gold adds 1000 resources") {
    int before = game.resource().amount();
    game.apply_cheat("gold");
    REQUIRE(game.resource().amount() == before + 1000);
  }
  SECTION("killall clears enemies") {
    // Spawn an enemy directly.
    auto e =
      make_enemy("normal", game.level().enemy_stats.at("normal"), &game.level().routes[0].path);
    game.spawn_enemy(e);
    game.update(1.0f / 60.0f);  // flush into enemies
    REQUIRE_FALSE(game.enemies().empty());
    game.apply_cheat("killall");
    REQUIRE(game.enemies().empty());
  }
  SECTION("win instantly ends the level in victory") {
    game.apply_cheat("win");
    REQUIRE(game.state() == Game::State::Victory);
    REQUIRE(game.last_result().cleared == true);
  }
}

TEST_CASE("Level progression", "[game][progression]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  REQUIRE(fx.registry.current_index() == 0);
  std::string first = game.level().name;

  REQUIRE(fx.advance());
  REQUIRE(fx.registry.current_index() == 1);
  REQUIRE(game.level().name != first);
  REQUIRE(game.state() == Game::State::Playing);

  // Advance through all remaining levels.
  while (fx.advance()) { /* advance */
  }
  REQUIRE_FALSE(fx.advance());  // no more levels
}

TEST_CASE("Splitter spawns children on death", "[game][splitter]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  auto stats = game.level().enemy_stats.at("splitter");
  int child_count = stats.child_count;
  REQUIRE(child_count > 0);

  auto sp = make_enemy("splitter", stats, &game.level().routes[0].path);
  sp->on_death(game);  // queues `child_count` children into pending spawns
  game.update(0.0f);   // flush pending into enemies (waves haven't spawned yet)
  REQUIRE(game.enemies().size() == static_cast<std::size_t>(child_count));
}

TEST_CASE("Boss regenerates health over time", "[game][boss]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  auto stats = game.level().enemy_stats.at("boss");
  REQUIRE(stats.regen_interval > 0.0f);

  auto boss = make_enemy("boss", stats, &game.level().routes[0].path);
  boss->set_health(100);
  REQUIRE(boss->health() == 100);

  // Run past one regen interval; the boss applies a RegenerationEffect.
  float dt = 1.0f / 60.0f;
  for (int i = 0; i < static_cast<int>(stats.regen_interval / dt) + 60; ++i) {
    boss->update(game, dt);
  }
  REQUIRE(boss->health() > 100);  // regenerated
}

TEST_CASE("Killing an enemy awards its per-type score", "[game][score]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  auto stats = game.level().enemy_stats.at("normal");
  int expected = stats.score;
  REQUIRE(expected > 0);

  auto enemy = make_enemy("normal", stats, &game.level().routes[0].path);
  enemy->set_health(1);
  game.spawn_enemy(enemy);
  game.update(1.0f / 60.0f);  // enemy is now in enemies_
  REQUIRE(game.score() == 0);

  // Kill it directly; the next update reaps it and awards score.
  enemy->set_health(0);
  game.update(1.0f / 60.0f);
  REQUIRE(game.score() == expected);

  // A boss is worth more than a normal enemy.
  GameFixture fxg2(config_dir());
  Game& g2 = fxg2.game;
  auto boss = make_enemy("boss", g2.level().enemy_stats.at("boss"), &g2.level().routes[0].path);
  boss->set_health(1);
  g2.spawn_enemy(boss);
  g2.update(1.0f / 60.0f);
  boss->set_health(0);
  g2.update(1.0f / 60.0f);
  REQUIRE(g2.score() > expected);
}

// ===========================================================================
// Integration tests: multi-system scenarios through the public Game API.
// ===========================================================================

TEST_CASE("placement_cost reflects terrain and validity", "[game][integration]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;  // level 1 has fertile at (4.5, 2.5)
  REQUIRE(game.placement_cost("normal", Vec2{0.5f, 0.5f}) == 50);  // grass
  REQUIRE(game.placement_cost("normal", Vec2{4.5f, 2.5f}) == 35);  // fertile: 50 * 0.7
  REQUIRE(game.placement_cost("wall", Vec2{4.5f, 2.5f}) == 17);    // fertile: 25 * 0.7 = 17.5 -> 17
  REQUIRE(game.placement_cost("normal", Vec2{9.5f, 2.5f}) == 50);  // rock still reports its cost
  REQUIRE(game.placement_cost("normal", Vec2{-1.0f, -1.0f}) == -1);  // out of bounds
  REQUIRE(game.placement_cost("nonexistent", Vec2{0.5f, 0.5f}) == -1);
}

TEST_CASE("Resource tower generates gold over time", "[game][integration][resource]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  useTestLevel(fx);
  REQUIRE(game.place_tower("resource", Vec2{2.5f, 2.5f}));  // costs 50 -> 1000 - 50 = 950
  REQUIRE(game.resource().amount() == 950);  // nothing granted until the first update

  // The cooldown starts finished, so the first update grants immediately.
  game.update(1.0f / 60.0f);
  REQUIRE(game.resource().amount() == 958);  // +8

  // No further grant until the 2.5s cooldown elapses.
  for (int i = 0; i < 60 * 2 - 1; ++i) game.update(1.0f / 60.0f);  // ~2s total, still one grant
  REQUIRE(game.resource().amount() == 958);

  for (int i = 0; i < 60; ++i) game.update(1.0f / 60.0f);  // pass the 2.5s mark -> second grant
  REQUIRE(game.resource().amount() == 966);                // +8 again
}

TEST_CASE("Custom level plays end-to-end", "[game][integration][custom]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  std::size_t n = fx.registry.size();
  useTestLevel(fx);
  // play() does NOT add to the registry (mirrors production playCustomLevel).
  REQUIRE(fx.registry.size() == n);
  REQUIRE(fx.registry.current_index() == 0);  // registry untouched
  REQUIRE(game.level().name == "Testfield");
  REQUIRE(game.state() == Game::State::Playing);

  game.apply_cheat("win");
  REQUIRE(game.state() == Game::State::Victory);
  REQUIRE(game.last_result().cleared);
  REQUIRE(game.level().index == -1);  // custom level
}

TEST_CASE("Splash tower damages a clustered group", "[game][integration][splash]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  useTestLevel(fx);
  REQUIRE(game.place_tower("splash", Vec2{2.5f, 2.5f}));  // adjacent to row 3

  std::vector<std::shared_ptr<Enemy>> foes;
  for (int i = 0; i < 3; ++i) {
    auto e =
      make_enemy("normal", game.level().enemy_stats.at("normal"), &game.level().routes[0].path);
    e->set_path_distance(1.5f);  // overlapping at (2.0, 3.5), within the splash radius
    foes.push_back(e);
    game.spawn_enemy(e);
  }

  runUntil(
    game,
    [&] {
      return std::any_of(foes.begin(), foes.end(), [](auto const& e) { return e->health() < 25; });
    },
    90
  );

  // One splash shot deals 15 to every enemy in the radius (normal has 25 hp -> 10).
  for (auto const& e : foes) REQUIRE(e->health() == 10);
}

TEST_CASE("Laser pierces multiple enemies in its beam", "[game][integration][laser]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  useTestLevel(fx);
  REQUIRE(game.place_tower("laser", Vec2{2.5f, 2.5f}));

  std::vector<std::shared_ptr<Enemy>> foes;
  for (int i = 0; i < 2; ++i) {
    auto e =
      make_enemy("normal", game.level().enemy_stats.at("normal"), &game.level().routes[0].path);
    e->set_path_distance(1.5f);  // both on the beam
    foes.push_back(e);
    game.spawn_enemy(e);
  }

  runUntil(
    game,
    [&] {
      return std::all_of(foes.begin(), foes.end(), [](auto const& e) { return e->health() < 25; });
    },
    90
  );

  // The piercing laser hits both (25 - 20 = 5).
  for (auto const& e : foes) REQUIRE(e->health() == 5);
}

TEST_CASE("Poison deals one-shot damage and roots the enemy", "[game][integration][poison]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  useTestLevel(fx);
  REQUIRE(game.place_tower("poison", Vec2{2.5f, 2.5f}));

  auto e =
    make_enemy("armored", game.level().enemy_stats.at("armored"), &game.level().routes[0].path);
  e->set_path_distance(1.5f);
  game.spawn_enemy(e);

  // Armored has 100 hp; poison deals 20 once on application.
  runUntil(game, [&] { return e->health() < 100; }, 90);
  REQUIRE(e->health() == 80);
  REQUIRE(e->status_hint().poison);

  // Rooted: the enemy does not advance while the poison root is active (1.2s).
  float d = e->path_distance();
  for (int i = 0; i < 30; ++i) game.update(1.0f / 60.0f);  // 0.5s, still rooted
  REQUIRE(e->path_distance() == Approx(d).epsilon(0.01f));

  // After the root expires the enemy advances again.
  for (int i = 0; i < 120; ++i) game.update(1.0f / 60.0f);  // 2s more
  REQUIRE(e->path_distance() > d);
}

TEST_CASE("Slow tower applies a slow status and impedes progress", "[game][integration][slow]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  useTestLevel(fx);
  REQUIRE(game.place_tower("slow", Vec2{2.5f, 2.5f}));

  auto slowed =
    make_enemy("normal", game.level().enemy_stats.at("normal"), &game.level().routes[0].path);
  slowed->set_path_distance(1.5f);
  game.spawn_enemy(slowed);

  runUntil(game, [&] { return slowed->status_hint().slow; }, 90);
  REQUIRE(slowed->status_hint().slow);

  // Compare against an unslowed enemy in a fresh game over the same window.
  GameFixture fxg2(config_dir());
  Game& g2 = fxg2.game;
  useTestLevel(fxg2);
  auto free = make_enemy("normal", g2.level().enemy_stats.at("normal"), &g2.level().routes[0].path);
  free->set_path_distance(1.5f);
  g2.spawn_enemy(free);

  float d_slowed = slowed->path_distance();
  float d_free = free->path_distance();
  for (int i = 0; i < 60; ++i) {  // 1s
    game.update(1.0f / 60.0f);
    g2.update(1.0f / 60.0f);
  }
  // The slowed enemy advanced less than the free one.
  REQUIRE(slowed->path_distance() - d_slowed < free->path_distance() - d_free);
}

TEST_CASE("Resistant enemy reduces splash damage", "[game][integration][resistant]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  useTestLevel(fx);
  REQUIRE(game.place_tower("splash", Vec2{2.5f, 2.5f}));

  auto r =
    make_enemy("resistant", game.level().enemy_stats.at("resistant"), &game.level().routes[0].path);
  r->set_path_distance(1.5f);
  game.spawn_enemy(r);

  runUntil(game, [&] { return r->health() < 50; }, 90);
  // Splash factor 0.3: 15 * 0.3 = 4.5 -> 4 damage (vs 15 for a normal enemy).
  REQUIRE(r->health() == 46);
}

TEST_CASE("Boss shield flatly reduces every hit", "[game][integration][boss]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  useTestLevel(fx);
  auto& path = game.level().routes[0].path;
  auto boss = make_enemy("boss", game.level().enemy_stats.at("boss"), &path);

  REQUIRE(boss->health() == 500);
  boss->decrease_health(15);  // 15 - shield 10 = 5 through
  REQUIRE(boss->health() == 495);
  boss->decrease_health(5);  // max(0, 5 - 10) = 0, fully absorbed
  REQUIRE(boss->health() == 495);
  boss->decrease_health(20);  // 20 - 10 = 10 through
  REQUIRE(boss->health() == 485);
  boss->decrease_health(11);  // 11 - 10 = 1 through
  REQUIRE(boss->health() == 484);
}

TEST_CASE("First wave spawns its full complement of enemies", "[game][integration][wave]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;  // level 1: gap[0]=3.0, wave 0 = 4 normals at t=3,4,5,6
  float dt = 1.0f / 60.0f;
  for (int i = 0; i < 60 * 7 && !game.over(); ++i) game.update(dt);  // t = 7s
  REQUIRE(game.enemies().size() == 4);
  REQUIRE(game.state() == Game::State::Playing);
}

// ===========================================================================
// Coverage: query API, level selection, and entity lifecycle edge cases.
// ===========================================================================

TEST_CASE("Misc query API", "[game][query]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  REQUIRE_FALSE(game.map().tiles().empty());  // map() + Map::tiles()
  REQUIRE(game.map().width() > 0.0f);

  REQUIRE_FALSE(game.paused());  // paused()
  game.pause();
  REQUIRE(game.paused());
  game.resume();

  auto level_infos = fx.registry.infos();
  REQUIRE(level_infos.size() == fx.registry.size());
  REQUIRE(level_infos.front().name == game.level().name);

  // add_observer: it fires on the next discrete event.
  struct CountingObserver : GameObserver {
    int placed = 0;
    void on_tower_placed(Tower const&, int) override { ++placed; }
  } obs;
  game.add_observer(&obs);
  REQUIRE(game.place_tower("normal", Vec2{0.5f, 0.5f}));
  REQUIRE(obs.placed == 1);
}

TEST_CASE("Observer receives discrete game events", "[game][observer]") {
  struct TrackingObserver : GameObserver {
    int placed = 0;
    int killed = 0;
    int wave_started = 0;
    void on_tower_placed(Tower const&, int) override { ++placed; }
    void on_enemy_killed(Enemy const&) override { ++killed; }
    void on_wave_started(int, bool, bool) override { ++wave_started; }
  } obs;

  GameFixture fx(config_dir());
  Game& game = fx.game;
  game.add_observer(&obs);

  REQUIRE(game.place_tower("normal", Vec2{4.5f, 2.5f}));  // near the level-1 path
  REQUIRE(obs.placed == 1);

  // Run until a wave starts and the tower kills at least one spawned enemy.
  float dt = 1.0f / 60.0f;
  for (int i = 0; i < 60 * 20 && !game.over(); ++i) game.update(dt);
  REQUIRE(obs.wave_started >= 1);
  REQUIRE(obs.killed >= 1);
}

TEST_CASE("select_level jumps to and clamps the index", "[game][level-select]") {
  GameFixture fx(config_dir());
  int n = static_cast<int>(fx.registry.size());

  fx.select(n - 1);  // last level
  REQUIRE(fx.registry.current_index() == n - 1);
  fx.select(n + 100);  // clamp high -> last
  REQUIRE(fx.registry.current_index() == n - 1);
  fx.select(-5);  // clamp low -> first
  REQUIRE(fx.registry.current_index() == 0);
}

TEST_CASE("Out-of-bounds bullets are culled each update", "[game][bullets]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  useTestLevel(fx);
  // A bullet fired from the left edge heading further left exits within a couple
  // of steps (the bounds check runs before the move, so culling is one step late).
  game.spawn_bullet(std::make_unique<NormalBullet>(Vec2{0.5f, 3.5f}, Vec2{-50.0f, 0.0f}, 5));
  REQUIRE(game.bullets().size() == 1);
  for (int i = 0; i < 3; ++i) game.update(1.0f / 60.0f);
  REQUIRE(game.bullets().empty());  // moved to x < 0 and was erased
}

TEST_CASE("Tower query and health-mutator API", "[game][tower][query]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  useTestLevel(fx);
  REQUIRE(game.place_tower("normal", Vec2{2.5f, 2.5f}));
  REQUIRE(game.place_tower("resource", Vec2{4.5f, 2.5f}));

  for (auto const& t : game.towers()) {
    REQUIRE(t->max_health() == t->health());  // max_health()
    t->set_health(10);                        // set_health()
    REQUIRE(t->health() == 10);
    t->increase_health(5);  // increase_health()
    REQUIRE(t->health() == 15);
  }
  for (auto const& t : game.towers()) {
    if (auto const* at = dynamic_cast<AttackTower const*>(t.get())) {
      REQUIRE(at->aim().length_sq() == Approx(1.0f));  // aim() (unit vector)
    }
    if (auto const* rt = dynamic_cast<ResourceTower const*>(t.get())) {
      REQUIRE(rt->resource_amount() > 0);  // resource_amount()
    }
  }
}

TEST_CASE("A destroyed tower is removed from the board", "[game][tower][death]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  useTestLevel(fx);
  REQUIRE(game.place_tower("wall", Vec2{5.5f, 3.5f}));  // on the path

  // Make the wall die to a single hit so its destruction is deterministic.
  for (auto const& t : game.towers())
    if (dynamic_cast<WallTower const*>(t.get())) t->set_health(1);

  auto e =
    make_enemy("normal", game.level().enemy_stats.at("normal"), &game.level().routes[0].path);
  e->set_path_distance(4.0f);  // upstream (left) of the wall; it walks into and attacks it
  game.spawn_enemy(e);

  runUntil(game, [&] { return game.towers().empty() || game.over(); }, 60 * 5);
  REQUIRE(game.towers().empty());  // wall destroyed -> reaped in update_towers()
}

TEST_CASE("A tower stops tracking damage once its attacker dies", "[game][tower][cleanup]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  useTestLevel(fx);
  // A wall (which cannot shoot back) lets an attacker reach and damage it, then
  // we kill the attacker directly so the game erases it and the wall drops the
  // now-expired attacker from its damage tracker.
  REQUIRE(game.place_tower("wall", Vec2{5.5f, 3.5f}));
  {
    auto e =
      make_enemy("normal", game.level().enemy_stats.at("normal"), &game.level().routes[0].path);
    e->set_path_distance(4.0f);  // upstream (left) of the wall -> it blocks and attacks
    game.spawn_enemy(e);
  }

  // Wait until the attacker has reached the wall and landed a hit (so a
  // TowerDamage entry exists).
  runUntil(
    game,
    [&] {
      for (auto const& t : game.towers())
        if (dynamic_cast<WallTower const*>(t.get())) return t->health() < t->max_health();
      return false;
    },
    60 * 5
  );
  REQUIRE_FALSE(game.enemies().empty());

  // Kill the attacker; the next update erases it and the wall drops the entry.
  game.enemies().front()->set_health(0);
  game.update(1.0f / 60.0f);
  REQUIRE(game.enemies().empty());
  REQUIRE_FALSE(game.towers().empty());  // wall survived
}

TEST_CASE("Enemy query API", "[game][enemy][query]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  auto e =
    make_enemy("normal", game.level().enemy_stats.at("normal"), &game.level().routes[0].path);
  REQUIRE(e->max_health() == e->health());  // max_health()

  // predicted_position falls back to the current center when no path is set.
  Vec2 center = e->position();
  e->set_path(nullptr);
  Vec2 p = e->predicted_position(1.0f);
  REQUIRE(p.x == Approx(center.x));
  REQUIRE(p.y == Approx(center.y));
}

TEST_CASE("Boss regeneration advertises a regen status hint", "[game][boss][status]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  auto stats = game.level().enemy_stats.at("boss");
  auto boss = make_enemy("boss", stats, &game.level().routes[0].path);
  boss->set_health(100);
  REQUIRE_FALSE(boss->status_hint().regen);

  // Run past one regen interval so the boss applies a RegenerationEffect.
  float dt = 1.0f / 60.0f;
  for (int i = 0; i < static_cast<int>(stats.regen_interval / dt) + 60; ++i) boss->update(game, dt);
  REQUIRE(boss->status_hint().regen);
}

TEST_CASE("Splitter clamps children that would overshoot the exit", "[game][splitter][clamp]") {
  GameFixture fx(config_dir());
  Game& game = fx.game;
  auto& path = game.level().routes[0].path;
  float total = path.total_length();

  // Start the splitter past the exit by more than the perturbation, so the
  // random offset can never pull the child back inside the path -> the clamp
  // branch always fires and the child lands just before the exit.
  constexpr float kPerturbation = 1.0f;
  SplitterEnemy::ChildSpec spec{1.0f, 1, 0, 0.0f, 0.4f, 0.4f, 1, 1, kPerturbation};
  auto sp = std::make_shared<SplitterEnemy>(
    "splitter",
    Rect(Vec2{0.5f, 3.5f}, 0.5f, 0.5f),
    1.0f,
    1,
    0,
    0.0f,
    spec
  );
  sp->set_path(&path);
  sp->set_path_distance(total + kPerturbation);  // beyond the exit by >= perturbation
  sp->on_death(game);
  game.update(0.0f);  // flush the child into enemies_

  REQUIRE(game.enemies().size() == 1);
  REQUIRE(game.enemies().front()->path_distance() < total);  // clamped before the exit
}
