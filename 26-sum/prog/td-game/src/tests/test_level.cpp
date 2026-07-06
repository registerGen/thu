#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string>

#include "game/config.h"
#include "game/level.h"

#ifndef TD_CONFIG_DIR
# error "TD_CONFIG_DIR must be defined by the build system"
#endif

namespace {
std::string config_dir() { return TD_CONFIG_DIR; }
}  // namespace

TEST_CASE("LevelRegistry navigates and clamps", "[level][registry]") {
  LevelRegistry reg;
  reg.load_from_config(config_dir());
  REQUIRE(reg.size() >= 3);
  REQUIRE(reg.current_index() == 0);
  REQUIRE(reg.has_next_official());

  reg.advance();
  REQUIRE(reg.current_index() == 1);

  auto infos = reg.infos();
  auto official_infos = reg.official_infos();
  REQUIRE(infos.size() == official_infos.size());
  REQUIRE(
    std::equal(
      infos.begin(),
      infos.end(),
      official_infos.begin(),
      official_infos.end(),
      [](auto lhs, auto rhs) { return lhs.name == lhs.name && lhs.index == rhs.index; }
    )
  );
  REQUIRE(infos.size() == reg.size());
  REQUIRE(infos[1].name == reg.current().name);

  reg.select(9999);  // clamp high -> last
  REQUIRE(reg.current_index() == static_cast<int>(reg.size()) - 1);
  reg.select(-3);  // clamp low -> first
  REQUIRE(reg.current_index() == 0);
}

TEST_CASE("LevelRegistry loads and sorts official levels by index", "[level][registry]") {
  LevelRegistry reg;
  reg.load_from_config(config_dir());
  REQUIRE(reg.size() >= 3);
  REQUIRE(reg.at(0).index == 1);
  REQUIRE(reg.at(1).index == 2);
  REQUIRE(reg.at(2).index == 3);
  REQUIRE(reg.at(0).name == "Meadow");
  REQUIRE(reg.at(1).name == "Switchback");
  REQUIRE(reg.at(2).name == "Glacier");
  // Any custom levels (index == -1) come after the official ones.
  for (std::size_t i = 3; i < reg.size(); ++i) {
    REQUIRE(reg.at(i).index == -1);
  }
}

TEST_CASE("LevelRegistry throws when no levels are found", "[level][registry]") {
  namespace fs = std::filesystem;
  // A config dir with stats files but no levels/ directory.
  auto tmp = fs::temp_directory_path() / "td_empty_levels";
  fs::create_directories(tmp);
  fs::copy_file(
    config_dir() + "/towers.json",
    tmp / "towers.json",
    fs::copy_options::overwrite_existing
  );
  fs::copy_file(
    config_dir() + "/enemies.json",
    tmp / "enemies.json",
    fs::copy_options::overwrite_existing
  );
  fs::remove_all(tmp / "levels");

  LevelRegistry reg;
  REQUIRE_THROWS_AS(reg.load_from_config(tmp.string()), std::runtime_error);
  fs::remove_all(tmp);
}

TEST_CASE("paired_portal_distance returns -1 for unknowns", "[level][portal]") {
  auto towers = config::load_towers(config_dir() + "/towers.json");
  auto enemies = config::load_enemies(config_dir() + "/enemies.json");
  // Level 2 has a portal, so its route carries portal_dest entries.
  Level lvl = config::load_level(config_dir() + "/levels/02-switchback.json", towers, enemies);
  Path const& p = lvl.routes[0].path;

  // Valid route path but a non-portal tile: no matching portal.
  REQUIRE(lvl.paired_portal_distance(&p, Vec2{0.5f, 0.5f}) < 0.0f);
  // A path pointer that matches no route on this level.
  REQUIRE(lvl.paired_portal_distance(nullptr, Vec2{0.5f, 0.5f}) < 0.0f);
}
