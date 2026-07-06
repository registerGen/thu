#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <filesystem>
#include <fstream>
#include <string>

#include "game/config.h"

using Catch::Approx;

#ifndef TD_CONFIG_DIR
# error "TD_CONFIG_DIR must be defined by the build system"
#endif

namespace {
std::string config_dir() { return TD_CONFIG_DIR; }
}  // namespace

TEST_CASE("Tower stats load from JSON", "[config][towers]") {
  auto table = config::load_towers(config_dir() + "/towers.json");

  SECTION("all seven tower types are present") {
    for (auto const& type : {"normal", "slow", "poison", "splash", "laser", "resource", "wall"}) {
      REQUIRE(table.count(type) == 1);
    }
  }
  SECTION("normal tower values") {
    auto const& s = table.at("normal");
    REQUIRE(s.max_health == 100);
    REQUIRE(s.resource_cost == 50);
    REQUIRE(s.attack_interval == Approx(0.5f));
    REQUIRE(s.attack_range == Approx(2.5f));
    REQUIRE(s.targeting == AttackTower::Targeting::First);
    REQUIRE(s.health_damage == 15);
    REQUIRE(s.min_speed == Approx(7.0f));
    REQUIRE(s.max_speed == Approx(9.0f));
  }
  SECTION("slow tower carries a slow effect") {
    auto const& s = table.at("slow");
    REQUIRE(s.slow_factor == Approx(0.4f));
    REQUIRE(s.slow_duration == Approx(2.5f));
  }
  SECTION("splash tower has a radius") {
    auto const& s = table.at("splash");
    REQUIRE(s.radius == Approx(2.0f));
    REQUIRE(s.targeting == AttackTower::Targeting::Closest);
  }
  SECTION("wall has no attack fields") {
    auto const& s = table.at("wall");
    REQUIRE(s.max_health == 400);
    REQUIRE(s.resource_cost == 25);
    REQUIRE(s.attack_range == Approx(0.0f));  // no range
  }
  SECTION("resource tower generates resources") {
    auto const& s = table.at("resource");
    REQUIRE(s.resource_inc_amount == 8);
    REQUIRE(s.resource_inc_interval == Approx(2.5f));
  }
}

TEST_CASE("Enemy stats load from JSON", "[config][enemies]") {
  auto table = config::load_enemies(config_dir() + "/enemies.json");

  SECTION("all six enemy types are present") {
    for (auto const& type : {"normal", "fast", "armored", "resistant", "splitter", "boss"}) {
      REQUIRE(table.count(type) == 1);
    }
  }
  SECTION("fast enemy is fast and frail") {
    auto const& f = table.at("fast");
    auto const& n = table.at("normal");
    REQUIRE(f.speed > n.speed);
    REQUIRE(f.max_health < n.max_health);
  }
  SECTION("armored enemy is slow and tanky") {
    auto const& a = table.at("armored");
    auto const& n = table.at("normal");
    REQUIRE(a.speed < n.speed);
    REQUIRE(a.max_health > n.max_health);
  }
  SECTION("resistant enemy has resist factors") {
    auto const& r = table.at("resistant");
    REQUIRE(r.slow_resist > 1.0f);
    REQUIRE(r.splash_resist < 1.0f);
  }
  SECTION("splitter has children spec") {
    auto const& s = table.at("splitter");
    REQUIRE(s.child_count > 0);
    REQUIRE(s.child_max_health > 0);
  }
  SECTION("boss has shield and regeneration") {
    auto const& b = table.at("boss");
    REQUIRE(b.shield > 0);
    REQUIRE(b.regen_amount > 0.0f);
    REQUIRE(b.regen_interval > 0.0f);
  }
  SECTION("enemies can damage walls") {
    auto const& n = table.at("normal");
    REQUIRE(n.tower_damage > 0);  // so walls eventually fall
  }
  SECTION("score differs by type") {
    auto const& n = table.at("normal");
    auto const& b = table.at("boss");
    REQUIRE(n.score > 0);
    REQUIRE(b.score > n.score);  // boss is worth more than a normal enemy
  }
}

TEST_CASE("Level 1 has a path", "[config][level][paths]") {
  auto towers = config::load_towers(config_dir() + "/towers.json");
  auto enemies = config::load_enemies(config_dir() + "/enemies.json");
  Level lvl = config::load_level(config_dir() + "/levels/01-meadow.json", towers, enemies);

  REQUIRE(lvl.paths.size() == 1);
  REQUIRE(lvl.paths[0].total_length() > 0.0f);
}

TEST_CASE("Levels load from JSON", "[config][level]") {
  auto towers = config::load_towers(config_dir() + "/towers.json");
  auto enemies = config::load_enemies(config_dir() + "/enemies.json");

  Level lvl = config::load_level(config_dir() + "/levels/01-meadow.json", towers, enemies);

  REQUIRE(lvl.name == "Meadow");
  REQUIRE(lvl.index == 1);
  REQUIRE(lvl.starting_resources == 150);
  REQUIRE(lvl.waves.size() >= 5);  // spec requires >= 5 waves
  REQUIRE(lvl.available_towers.size() >= 4);

  SECTION("map dimensions and path tiles") {
    REQUIRE(lvl.map.width() == 12.0f);
    REQUIRE(lvl.map.height() == 7.0f);
    Tile const* path_tile = lvl.map.tile_at(Vec2{5.5f, 3.5f});
    REQUIRE(path_tile != nullptr);
    REQUIRE(path_tile->can_place_tower());
  }
  SECTION("path runs entrance to exit") {
    REQUIRE(lvl.paths[0].total_length() > 0.0f);
    // Entrance on the right, exit on the left for level 1.
    REQUIRE(
      lvl.paths[0].position_at(0.0f).x > lvl.paths[0].position_at(lvl.paths[0].total_length()).x
    );
  }
  SECTION("waves reference known enemy types") {
    for (auto const& wave : lvl.waves) {
      for (auto const& spawn : wave.spawns) {
        REQUIRE(enemies.count(spawn.type) == 1);
      }
    }
  }
}

TEST_CASE("Level 2 has portals", "[config][level][portal]") {
  auto towers = config::load_towers(config_dir() + "/towers.json");
  auto enemies = config::load_enemies(config_dir() + "/enemies.json");
  Level lvl = config::load_level(config_dir() + "/levels/02-switchback.json", towers, enemies);

  REQUIRE_FALSE(lvl.paths[0].portal_pairs().empty());
  // One portal pair, stored as a directional (src, tgt) waypoint-index pair.
  REQUIRE(lvl.paths[0].portal_pairs().size() == 1);
}

TEST_CASE("Level 3 has ice terrain", "[config][level][terrain]") {
  auto towers = config::load_towers(config_dir() + "/towers.json");
  auto enemies = config::load_enemies(config_dir() + "/enemies.json");
  Level lvl = config::load_level(config_dir() + "/levels/03-glacier.json", towers, enemies);

  // Find an ice tile on the path (row 3, cols 4-9 are ice in 03-glacier.json).
  Tile const* ice = lvl.map.tile_at(Vec2{5.5f, 3.5f});
  REQUIRE(ice != nullptr);
  REQUIRE(ice->enemy_speed_factor() > 1.0f);  // ice speeds enemies up
  REQUIRE(ice->slow_bullet_factor() < 1.0f);  // ice strengthens slow
  REQUIRE(ice->can_place_tower());            // ice is placeable (path or not)
}

TEST_CASE("Per-wave gaps are loaded and match the wave count", "[config][level][gaps]") {
  auto towers = config::load_towers(config_dir() + "/towers.json");
  auto enemies = config::load_enemies(config_dir() + "/enemies.json");
  Level lvl = config::load_level(config_dir() + "/levels/01-meadow.json", towers, enemies);

  REQUIRE(lvl.waves.size() >= 5);
  for (auto const& w : lvl.waves) REQUIRE(w.gap > 0.0f);
  // Gaps are authored non-uniformly.
  bool any_diff = false;
  for (std::size_t i = 1; i < lvl.waves.size(); ++i)
    if (lvl.waves[i].gap != lvl.waves[0].gap) any_diff = true;
  REQUIRE(any_diff);
}

TEST_CASE("Spawn path is mandatory and validated", "[config][level][path]") {
  auto towers = config::load_towers(config_dir() + "/towers.json");
  auto enemies = config::load_enemies(config_dir() + "/enemies.json");

  // A spawn without a "path" key must be rejected.
  std::string missing = R"({
    "name":"t","map":{"rows":1,"cols":2,"terrain":[["grass","grass"]]},
    "paths":[{"waypoints":[[1,0],[0,0]],"portals":[]}],
    "waves":[{"gap":0.0,"spawns":[{"kind":"normal","time":0.0}]}]
  })";
  REQUIRE_THROWS_AS(config::load_level_json(missing, towers, enemies), std::runtime_error);

  // A spawn referencing an out-of-range path must be rejected.
  std::string oob = R"({
    "name":"t","map":{"rows":1,"cols":2,"terrain":[["grass","grass"]]},
    "paths":[{"waypoints":[[1,0],[0,0]],"portals":[]}],
    "waves":[{"gap":0.0,"spawns":[{"kind":"normal","time":0.0,"path":5}]}]
  })";
  REQUIRE_THROWS_AS(config::load_level_json(oob, towers, enemies), std::runtime_error);
}

// ===========================================================================
// Loader fallbacks and validation (cover the catch/throw branches).
// ===========================================================================

namespace {
std::pair<TowerStatsTable, EnemyStatsTable> load_stats() {
  return {
    config::load_towers(config_dir() + "/towers.json"),
    config::load_enemies(config_dir() + "/enemies.json")
  };
}
}  // namespace

TEST_CASE("Per-wave gaps are loaded", "[config][level][gaps]") {
  auto [towers, enemies] = load_stats();
  Level lvl = config::load_level(config_dir() + "/levels/01-meadow.json", towers, enemies);
  REQUIRE(lvl.waves.size() >= 5);
  for (float g : {lvl.waves[0].gap, lvl.waves[1].gap}) REQUIRE(g > 0.0f);
  // Gaps are authored non-uniformly.
  bool any_diff = false;
  for (std::size_t i = 1; i < lvl.waves.size(); ++i)
    if (lvl.waves[i].gap != lvl.waves[0].gap) any_diff = true;
  REQUIRE(any_diff);
}

TEST_CASE("Missing tower/enemy types are skipped, not fatal", "[config][stats]") {
  namespace fs = std::filesystem;
  auto tmp = fs::temp_directory_path() / "td_cfg_stats";
  fs::create_directories(tmp);
  // towers.json without "wall"; enemies.json without "boss".
  std::ofstream(tmp / "towers.json")
    << R"([{"kind":"normal","max_health":100,"resource_cost":50}])";
  std::ofstream(tmp / "enemies.json") << R"([{"kind":"normal","max_health":25,"speed":1.0}])";

  auto towers = config::load_towers((tmp / "towers.json").string());
  REQUIRE(towers.count("normal") == 1);
  REQUIRE(towers.count("wall") == 0);  // absent type skipped via the catch/continue

  auto enemies = config::load_enemies((tmp / "enemies.json").string());
  REQUIRE(enemies.count("normal") == 1);
  REQUIRE(enemies.count("boss") == 0);

  fs::remove_all(tmp);
}

TEST_CASE("Portal parsing edge cases", "[config][level][portal]") {
  auto [towers, enemies] = load_stats();

  SECTION("a directional portal pair is stored as-is") {
    // [src, tgt] = [1, 2]: an enemy at waypoint 1 teleports to waypoint 2's
    // path distance; waypoint 2 (the target) does not teleport.
    std::string dir = R"({
      "name":"dir","map":{"rows":1,"cols":3,"terrain":[["grass","grass","grass"]]},
      "paths":[{"waypoints":[[0,0],[1,0],[2,0]],"portals":[[1,2]]}],
      "resource_auto_inc_amount":0,"resource_auto_inc_interval":1.0,
      "waves":[{"gap":0.0,"spawns":[{"kind":"normal","time":0.0,"path":0}]}]
    })";
    Level lvl = config::load_level_json(dir, towers, enemies);
    REQUIRE(lvl.paths[0].portal_pairs().size() == 1);
    REQUIRE(lvl.paths[0].paired_portal_distance(Vec2{1.5f, 0.5f}) == Approx(2.0f));
    REQUIRE(lvl.paths[0].paired_portal_distance(Vec2{2.5f, 0.5f}) == -1.0f);  // target
  }

  SECTION("a portal tile reused by two pairs is rejected (silently)") {
    // Index 1 belongs to both pairs -> the second pair's tile is already a
    // portal, throwing inside the try, so only the first pair survives.
    std::string reuse = R"({
      "name":"reuse","map":{"rows":1,"cols":3,"terrain":[["grass","grass","grass"]]},
      "paths":[{"waypoints":[[0,0],[1,0],[2,0]],"portals":[[0,1],[1,2]]}],
      "resource_auto_inc_amount":0,"resource_auto_inc_interval":1.0,
      "waves":[{"gap":0.0,"spawns":[{"kind":"normal","time":0.0,"path":0}]}]
    })";
    Level lvl = config::load_level_json(reuse, towers, enemies);
    REQUIRE(lvl.paths[0].portal_pairs().size() == 1);
  }

  SECTION("an out-of-range portal index is ignored") {
    std::string oob = R"({
      "name":"oob","map":{"rows":1,"cols":3,"terrain":[["grass","grass","grass"]]},
      "paths":[{"waypoints":[[0,0],[1,0],[2,0]],"portals":[[0,9]]}],
      "resource_auto_inc_amount":0,"resource_auto_inc_interval":1.0,
      "waves":[{"gap":0.0,"spawns":[{"kind":"normal","time":0.0,"path":0}]}]
    })";
    Level lvl = config::load_level_json(oob, towers, enemies);
    REQUIRE(lvl.paths[0].portal_pairs().empty());
  }

  SECTION("a path without a 'portals' key yields no portals") {
    std::string no_portals = R"({
      "name":"none","map":{"rows":1,"cols":3,"terrain":[["grass","grass","grass"]]},
      "paths":[{"waypoints":[[0,0],[1,0],[2,0]]}],
      "resource_auto_inc_amount":0,"resource_auto_inc_interval":1.0,
      "waves":[{"gap":0.0,"spawns":[{"kind":"normal","time":0.0,"path":0}]}]
    })";
    Level lvl = config::load_level_json(no_portals, towers, enemies);
    REQUIRE(lvl.paths[0].portal_pairs().empty());
  }
}

// ===========================================================================
// Level serialization (save_level_json / save_level) — the level editor path.
// ===========================================================================

TEST_CASE("save_level_json round-trips a level", "[config][level][save]") {
  auto [towers, enemies] = load_stats();

  for (auto const& name : {"01-meadow", "02-switchback", "03-glacier"}) {
    Level original =
      config::load_level(config_dir() + "/levels/" + name + ".json", towers, enemies);
    std::string json = config::save_level_json(original);
    REQUIRE_FALSE(json.empty());
    REQUIRE(json.front() == '{');
    REQUIRE(json.back() == '\n');

    Level restored = config::load_level_json(json, towers, enemies);
    // Structural fields must survive a save -> load round-trip.
    REQUIRE(restored.name == original.name);
    REQUIRE(restored.map.width() == original.map.width());
    REQUIRE(restored.map.height() == original.map.height());
    REQUIRE(restored.starting_resources == original.starting_resources);
    REQUIRE(restored.resource_auto_inc_amount == original.resource_auto_inc_amount);
    REQUIRE(restored.resource_auto_inc_interval == Approx(original.resource_auto_inc_interval));
    REQUIRE(restored.available_towers == original.available_towers);

    REQUIRE(restored.waves.size() == original.waves.size());
    REQUIRE(restored.paths.size() == original.paths.size());
    REQUIRE(restored.paths[0].total_length() == Approx(original.paths[0].total_length()));
    // Portals (level 2) must round-trip too.
    REQUIRE(restored.paths[0].portal_pairs().size() == original.paths[0].portal_pairs().size());
  }
}

TEST_CASE("save_level_json escapes special characters in the name", "[config][level][save]") {
  auto [towers, enemies] = load_stats();
  Level lvl = config::load_level(config_dir() + "/levels/01-meadow.json", towers, enemies);
  lvl.name = "a\"b\\c";  // embedded quote and backslash

  std::string json = config::save_level_json(lvl);
  // esc() turned the embedded quote and backslash into the JSON escapes \" and \\.
  REQUIRE(json.find("a\\\"b\\\\c") != std::string::npos);

  // And the escaped name still round-trips to the original.
  Level restored = config::load_level_json(json, towers, enemies);
  REQUIRE(restored.name == "a\"b\\c");
}

TEST_CASE("save_level writes a file and rejects unwritable paths", "[config][level][save]") {
  namespace fs = std::filesystem;
  auto [towers, enemies] = load_stats();
  Level lvl = config::load_level(config_dir() + "/levels/01-meadow.json", towers, enemies);

  auto tmp = fs::temp_directory_path() / "td_saved_level.json";
  REQUIRE_NOTHROW(config::save_level(tmp.string(), lvl));
  REQUIRE(fs::exists(tmp));

  // The written file must be readable back as an equivalent level.
  Level restored = config::load_level(tmp.string(), towers, enemies);
  REQUIRE(restored.name == lvl.name);
  REQUIRE(restored.waves.size() == lvl.waves.size());

  // A path whose parent directory doesn't exist can't be opened for writing.
  REQUIRE_THROWS_AS(config::save_level("/no/such/dir/level.json", lvl), std::runtime_error);
  fs::remove(tmp);
}

TEST_CASE("Portal tiles are marked is_portal", "[config][level][portal]") {
  auto towers = config::load_towers(config_dir() + "/towers.json");
  auto enemies = config::load_enemies(config_dir() + "/enemies.json");
  Level lvl = config::load_level(config_dir() + "/levels/02-switchback.json", towers, enemies);

  // Portal pair [[1,4]]: waypoint 1 = (11,3) -> world (11.5, 3.5),
  // waypoint 4 = (6,3) -> world (6.5, 3.5). Both tiles must be flagged.
  Tile const* src = lvl.map.tile_at(Vec2{11.5f, 3.5f});
  REQUIRE(src != nullptr);
  REQUIRE(src->is_portal());

  Tile const* tgt = lvl.map.tile_at(Vec2{6.5f, 3.5f});
  REQUIRE(tgt != nullptr);
  REQUIRE(tgt->is_portal());

  // A non-portal tile should not be flagged.
  Tile const* grass = lvl.map.tile_at(Vec2{0.5f, 0.5f});
  REQUIRE(grass != nullptr);
  REQUIRE_FALSE(grass->is_portal());
}
