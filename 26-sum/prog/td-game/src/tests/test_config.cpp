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
    REQUIRE(s.health == 100);
    REQUIRE(s.cost == 50);
    REQUIRE(s.attack_cooldown == Approx(0.5f));
    REQUIRE(s.range == Approx(2.5f));
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
    REQUIRE(s.health == 400);
    REQUIRE(s.cost == 25);
    REQUIRE(s.range == Approx(0.0f));  // no range
  }
  SECTION("resource tower generates resources") {
    auto const& s = table.at("resource");
    REQUIRE(s.resource_amount == 8);
    REQUIRE(s.resource_cooldown == Approx(2.5f));
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
    REQUIRE(f.health < n.health);
  }
  SECTION("armored enemy is slow and tanky") {
    auto const& a = table.at("armored");
    auto const& n = table.at("normal");
    REQUIRE(a.speed < n.speed);
    REQUIRE(a.health > n.health);
  }
  SECTION("resistant enemy has resist factors") {
    auto const& r = table.at("resistant");
    REQUIRE(r.slow_resist > 1.0f);
    REQUIRE(r.splash_resist < 1.0f);
  }
  SECTION("splitter has children spec") {
    auto const& s = table.at("splitter");
    REQUIRE(s.child_count > 0);
    REQUIRE(s.child_health > 0);
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

TEST_CASE("Level 1 has a route", "[config][level][routes]") {
  auto towers = config::load_towers(config_dir() + "/towers.json");
  auto enemies = config::load_enemies(config_dir() + "/enemies.json");
  Level lvl = config::load_level(config_dir() + "/levels/01-meadow.json", towers, enemies);

  REQUIRE(lvl.routes.size() == 1);
  REQUIRE(lvl.routes[0].path.total_length() > 0.0f);
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
    REQUIRE(lvl.routes[0].path.total_length() > 0.0f);
    // Entrance on the right, exit on the left for level 1.
    REQUIRE(
      lvl.routes[0].path.position_at(0.0f).x >
      lvl.routes[0].path.position_at(lvl.routes[0].path.total_length()).x
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

  REQUIRE_FALSE(lvl.routes[0].portal_dest.empty());
  // Portals are forward-only: each jump destination is further along the path
  // than some other portal (no backward jumps that would soft-lock).
  REQUIRE(lvl.routes[0].portal_dest.size() == 1);  // one pair -> one forward entry
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

  REQUIRE(lvl.gaps.size() == lvl.waves.size());
  for (float g : lvl.gaps) REQUIRE(g > 0.0f);
  // Gaps are authored non-uniformly.
  bool any_diff = false;
  for (std::size_t i = 1; i < lvl.gaps.size(); ++i)
    if (lvl.gaps[i] != lvl.gaps[0]) any_diff = true;
  REQUIRE(any_diff);
}

TEST_CASE("Spawn route is mandatory and validated", "[config][level][route]") {
  auto towers = config::load_towers(config_dir() + "/towers.json");
  auto enemies = config::load_enemies(config_dir() + "/enemies.json");

  // A spawn without a "route" key must be rejected.
  std::string missing = R"({
    "name":"t","map":{"rows":1,"cols":2,"terrain":[["grass","grass"]]},
    "routes":[{"path":[[1,0],[0,0]],"portals":[]}],
    "gaps":[0.0],
    "waves":[{"spawns":[{"type":"normal","time":0.0}]}]
  })";
  REQUIRE_THROWS_AS(config::load_level_json(missing, towers, enemies), std::runtime_error);

  // A spawn referencing an out-of-range route must be rejected.
  std::string oob = R"({
    "name":"t","map":{"rows":1,"cols":2,"terrain":[["grass","grass"]]},
    "routes":[{"path":[[1,0],[0,0]],"portals":[]}],
    "gaps":[0.0],
    "waves":[{"spawns":[{"type":"normal","time":0.0,"route":5}]}]
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

TEST_CASE("Gap count must match the wave count", "[config][level][gaps]") {
  auto [towers, enemies] = load_stats();
  std::string two_gaps_one_wave = R"({
    "name":"t","map":{"rows":1,"cols":2,"terrain":[["grass","grass"]]},
    "routes":[{"path":[[1,0],[0,0]],"portals":[]}],
    "auto_increase":{"amount":0,"interval":1.0},
    "gaps":[0.0,1.0],
    "waves":[{"spawns":[{"type":"normal","time":0.0,"route":0}]}]
  })";
  REQUIRE_THROWS_AS(
    config::load_level_json(two_gaps_one_wave, towers, enemies),
    std::runtime_error
  );
}

TEST_CASE(
  "Legacy level without a 'routes' key falls back to the root path",
  "[config][level][legacy]"
) {
  auto [towers, enemies] = load_stats();
  // No "routes" and no "available_towers": exercises both fallback catches.
  std::string legacy = R"({
    "name":"legacy","map":{"rows":1,"cols":3,"terrain":[["grass","grass","grass"]]},
    "path":[[0,0],[2,0]],
    "auto_increase":{"amount":0,"interval":1.0},
    "gaps":[0.0],
    "waves":[{"spawns":[{"type":"normal","time":0.0,"route":0}]}]
  })";
  Level lvl = config::load_level_json(legacy, towers, enemies);
  REQUIRE(lvl.routes.size() == 1);
  REQUIRE(lvl.routes[0].path.total_length() > 0.0f);
  REQUIRE(lvl.available_towers.empty());  // absent -> empty, not a throw
}

TEST_CASE("Missing tower/enemy types are skipped, not fatal", "[config][stats]") {
  namespace fs = std::filesystem;
  auto tmp = fs::temp_directory_path() / "td_cfg_stats";
  fs::create_directories(tmp);
  // towers.json without "wall"; enemies.json without "boss".
  std::ofstream(tmp / "towers.json") << R"({"normal":{"health":100,"cost":50}})";
  std::ofstream(tmp / "enemies.json") << R"({"normal":{"health":25,"speed":1.0}})";

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

  SECTION("portal pair listed in reverse order still resolves forward") {
    // The later waypoint is declared first, so dA > dB -> B->A forward entry.
    std::string reverse = R"({
      "name":"rev","map":{"rows":1,"cols":3,"terrain":[["grass","grass","grass"]]},
      "routes":[{"path":[[0,0],[1,0],[2,0]],"portals":[[[2,0],[0,0]]]}],
      "auto_increase":{"amount":0,"interval":1.0},
      "gaps":[0.0],"waves":[{"spawns":[{"type":"normal","time":0.0,"route":0}]}]
    })";
    Level lvl = config::load_level_json(reverse, towers, enemies);
    REQUIRE(lvl.routes[0].portal_dest.size() == 1);
  }

  SECTION("a portal tile reused by two pairs is rejected (silently)") {
    // [1,0] belongs to both pairs -> second pair throws, caught inside build_route.
    std::string reuse = R"({
      "name":"reuse","map":{"rows":1,"cols":3,"terrain":[["grass","grass","grass"]]},
      "routes":[{"path":[[0,0],[1,0],[2,0]],"portals":[[[0,0],[1,0]],[[1,0],[2,0]]]}],
      "auto_increase":{"amount":0,"interval":1.0},
      "gaps":[0.0],"waves":[{"spawns":[{"type":"normal","time":0.0,"route":0}]}]
    })";
    Level lvl = config::load_level_json(reuse, towers, enemies);
    // The reused-tile error aborts portal resolution; at most one pair survives.
    REQUIRE(lvl.routes[0].portal_dest.size() <= 1);
  }

  SECTION("a portal pointing off the path is ignored") {
    std::string off_path = R"({
      "name":"off","map":{"rows":1,"cols":3,"terrain":[["grass","grass","grass"]]},
      "routes":[{"path":[[0,0],[1,0],[2,0]],"portals":[[[9,9],[0,0]]]}],
      "auto_increase":{"amount":0,"interval":1.0},
      "gaps":[0.0],"waves":[{"spawns":[{"type":"normal","time":0.0,"route":0}]}]
    })";
    Level lvl = config::load_level_json(off_path, towers, enemies);
    REQUIRE(lvl.routes[0].portal_dest.empty());
  }

  SECTION("a route without a 'portals' key yields no portals") {
    std::string no_portals = R"({
      "name":"none","map":{"rows":1,"cols":3,"terrain":[["grass","grass","grass"]]},
      "routes":[{"path":[[0,0],[1,0],[2,0]]}],
      "auto_increase":{"amount":0,"interval":1.0},
      "gaps":[0.0],"waves":[{"spawns":[{"type":"normal","time":0.0,"route":0}]}]
    })";
    Level lvl = config::load_level_json(no_portals, towers, enemies);
    REQUIRE(lvl.routes[0].portal_dest.empty());
    REQUIRE(lvl.routes[0].portal_pairs.empty());
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
    REQUIRE(restored.available_towers == original.available_towers);
    REQUIRE(restored.gaps == original.gaps);
    REQUIRE(restored.waves.size() == original.waves.size());
    REQUIRE(restored.routes.size() == original.routes.size());
    REQUIRE(
      restored.routes[0].path.total_length() == Approx(original.routes[0].path.total_length())
    );
    // Portals (level 2) must round-trip too.
    REQUIRE(restored.routes[0].portal_dest.size() == original.routes[0].portal_dest.size());
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
