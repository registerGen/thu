#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "map.h"
#include "path.h"
#include "stats.h"
#include "wave.h"

/// A complete level: map, one or more paths, waves, economy, and the stats
/// tables shared by towers/enemies. Loaded from JSON config. Each path carries
/// its own portal links (directional waypoint-index pairs on the Path).
struct Level {
  std::string name;
  int index = -1;  // 1-based for official levels; -1 for custom (editor-created).
  Map map;
  std::vector<Path> paths;
  std::vector<Wave> waves;
  int starting_resources = 0;
  int resource_auto_inc_amount = 0;
  float resource_auto_inc_interval = 1.0f;
  std::vector<std::string> available_towers;
  TowerStatsTable tower_stats;
  EnemyStatsTable enemy_stats;
};

/// Owns all levels and tracks the current one for progression.
class LevelRegistry {
  std::vector<Level> levels_;
  int current_ = 0;

public:
  /// Load towers.json, enemies.json, and levels/level*.json from `config_dir`.
  void load_from_dir(std::string const& config_dir);
  Level const& current() const;
  Level const& at(std::size_t slot) const;
  int current_index() const;
  /// True if the next slot exists AND is an official level (index >= 1).
  /// Prevents progression from crossing into the custom-level tail.
  bool has_next_official() const;
  /// Advance only if the next official level exists.
  void advance();
  /// Jump to a level by slot (clamped to [0, size)). For the level-select UI.
  void select(int index);
  /// {index, name} of all levels, in order (index is the Level's data field).
  struct Info {
    int index;
    std::string name;
  };
  std::vector<Info> infos() const;
  /// Infos for official levels only (index >= 1).
  std::vector<Info> official_infos() const;
  std::size_t size() const;
};
