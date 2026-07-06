#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>
#include <random>
#include <stdexcept>

#include "game/enemy.h"
#include "game/factory.h"
#include "game/map.h"
#include "game/path.h"
#include "game/stats.h"
#include "game/tile.h"
#include "game/tower.h"

using Catch::Approx;

namespace {

TowerStats full_tower_stats() {
  TowerStats s;
  s.health = 120;
  s.cost = 55;
  s.attack_cooldown = 0.3f;
  s.range = 3.0f;
  s.targeting = AttackTower::Targeting::First;
  s.min_speed = 5.0f;
  s.max_speed = 8.0f;
  s.max_angle_deviation = 0.1f;
  s.health_damage = 9;
  s.slow_factor = 0.5f;
  s.slow_duration = 2.0f;
  s.poison_damage = 12;
  s.poison_duration = 1.5f;
  s.radius = 1.2f;
  s.width = 0.4f;
  s.resource_amount = 8;
  s.resource_cooldown = 1.5f;
  return s;
}

Tile grass_tile(Vec2 c) { return Tile{Rect{c, 1.0f, 1.0f}, 1.0f, true, 1.0f, 1.0f, false}; }

Map one_tile_map() {
  std::vector<Tile> tiles{grass_tile(Vec2{0.5f, 0.5f})};
  return Map{1.0f, 1.0f, tiles};
}

}  // namespace

TEST_CASE("make_tower builds each tower type", "[factory][tower]") {
  auto stats = full_tower_stats();
  std::mt19937 rng(1);
  auto map = one_tile_map();
  Tile tile = grass_tile(Vec2{0.5f, 0.5f});

  SECTION("normal") {
    auto t = make_tower("normal", tile, stats, rng, map);
    REQUIRE(t->type() == "normal");
    REQUIRE(t->health() == 120);
  }
  SECTION("slow") {
    auto t = make_tower("slow", tile, stats, rng, map);
    REQUIRE(t->type() == "slow");
  }
  SECTION("poison") {
    auto t = make_tower("poison", tile, stats, rng, map);
    REQUIRE(t->type() == "poison");
  }
  SECTION("splash") {
    auto t = make_tower("splash", tile, stats, rng, map);
    REQUIRE(t->type() == "splash");
  }
  SECTION("laser") {
    auto t = make_tower("laser", tile, stats, rng, map);
    REQUIRE(t->type() == "laser");
  }
  SECTION("resource") {
    auto t = make_tower("resource", tile, stats, rng, map);
    REQUIRE(dynamic_cast<ResourceTower*>(t.get()) != nullptr);
    REQUIRE(t->health() == 120);
  }
  SECTION("wall") {
    auto t = make_tower("wall", tile, stats, rng, map);
    REQUIRE(dynamic_cast<WallTower*>(t.get()) != nullptr);
  }
  SECTION("unknown type throws") {
    REQUIRE_THROWS_AS(make_tower("bogus", tile, stats, rng, map), std::runtime_error);
  }
}

TEST_CASE("Tower placement rules", "[factory][tower][placement]") {
  Tile tile_a = grass_tile(Vec2{0.5f, 0.5f});
  Tile tile_b = grass_tile(Vec2{1.5f, 1.5f});

  auto stats = full_tower_stats();
  std::mt19937 rng(1);
  auto map = one_tile_map();

  SECTION("any tower accepts grass tiles") {
    auto t = make_tower("normal", tile_a, stats, rng, map);
    REQUIRE(t->can_place_on(tile_a));
    REQUIRE(t->can_place_on(tile_b));
    auto w = make_tower("wall", tile_b, stats, rng, map);
    REQUIRE(w->can_place_on(tile_a));
    REQUIRE(w->can_place_on(tile_b));
  }
  SECTION("occupied tiles are rejected") {
    tile_a.set_occupied_by_tower(true);
    tile_b.set_occupied_by_tower(true);
    auto t = make_tower("normal", tile_a, stats, rng, map);
    auto w = make_tower("wall", tile_b, stats, rng, map);
    REQUIRE_FALSE(t->can_place_on(tile_a));
    REQUIRE_FALSE(w->can_place_on(tile_b));
  }
}

TEST_CASE("make_enemy builds each enemy type with stats", "[factory][enemy]") {
  Path path{std::vector<Vec2>{{0.0f, 0.0f}, {10.0f, 0.0f}}};

  SECTION("normal") {
    EnemyStats s;
    s.health = 30;
    s.speed = 2.0f;
    s.size = 0.5f;
    auto e = make_enemy("normal", s, &path);
    REQUIRE(e->type() == "normal");
    REQUIRE(e->health() == 30);
    REQUIRE(e->path_distance() == Approx(0.0f));
    REQUIRE(e->position().x == Approx(0.0f));  // at path start
  }
  SECTION("fast") {
    EnemyStats s;
    s.speed = 4.0f;
    REQUIRE(make_enemy("fast", s, &path)->type() == "fast");
  }
  SECTION("armored") {
    EnemyStats s;
    s.speed = 1.0f;
    REQUIRE(make_enemy("armored", s, &path)->type() == "armored");
  }
  SECTION("resistant applies resist factors") {
    EnemyStats s;
    s.slow_resist = 2.0f;
    s.splash_resist = 0.3f;
    auto e = make_enemy("resistant", s, &path);
    auto r = dynamic_cast<ResistantEnemy*>(e.get());
    REQUIRE(r != nullptr);
    REQUIRE(r->slow_resist_factor() == Approx(2.0f));
    REQUIRE(r->splash_damage_factor() == Approx(0.3f));
  }
  SECTION("splitter") {
    EnemyStats s;
    s.child_count = 3;
    s.child_health = 10;
    s.child_speed = 2.5f;
    REQUIRE(dynamic_cast<SplitterEnemy*>(make_enemy("splitter", s, &path).get()) != nullptr);
  }
  SECTION("boss has a shield that mitigates damage") {
    EnemyStats s;
    s.shield = 40;
    s.health = 200;
    auto e = make_enemy("boss", s, &path);
    auto b = dynamic_cast<BossEnemy*>(e.get());
    REQUIRE(b != nullptr);
    REQUIRE(e->health() == 200);
    e->decrease_health(30);  // fully absorbed by shield
    REQUIRE(e->health() == 200);
    e->decrease_health(60);  // 60 - 40 shield = 20 damage
    REQUIRE(e->health() == 180);
  }
  SECTION("unknown enemy type throws") {
    EnemyStats s;
    REQUIRE_THROWS_AS(make_enemy("bogus", s, &path), std::runtime_error);
  }
  SECTION("null path spawns at origin") {
    EnemyStats s;
    s.size = 0.5f;
    auto e = make_enemy("normal", s, nullptr);
    REQUIRE(e->position().x == Approx(0.0f));
    REQUIRE(e->position().y == Approx(0.0f));
  }
}
