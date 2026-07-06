#pragma once

#include <string>

#include "level.h"
#include "stats.h"

namespace config {

/// Load the global tower stats table from towers.json.
TowerStatsTable load_towers(std::string const& path);

/// Load the global enemy stats table from enemies.json.
EnemyStatsTable load_enemies(std::string const& path);

/// Load a single level from its JSON file, using the shared stats tables.
Level load_level(
  std::string const& path,
  TowerStatsTable const& towers,
  EnemyStatsTable const& enemies
);

/// Load a single level from an inline JSON string (for tests / programmatic use).
Level load_level_json(
  std::string_view json,
  TowerStatsTable const& towers,
  EnemyStatsTable const& enemies
);

/// Serialize a Level to a JSON string (for the level editor's save feature).
std::string save_level_json(Level const& level);

/// Save a Level to a JSON file.
void save_level(std::string const& path, Level const& level);

/// Build a Tile from a terrain name ("grass"/"fertile"/"rock"/"ice") at `center`.
/// Shared by the level loader and the level editor so the factor table lives once.
Tile tile_from_terrain(std::string const& terrain, Vec2 center);

/// Reverse of tile_from_terrain: infer the terrain name from a Tile's factors.
std::string terrain_name(Tile const& tile);

}  // namespace config
