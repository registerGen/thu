#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "map.h"
#include "path.h"
#include "resource.h"
#include "stats.h"
#include "wave.h"

/// One enemy route through the map: a polyline path plus the portal links that
/// sit on it. A level may have several routes; each enemy follows one.
struct LevelRoute {
  Path path;
  /// Portal tile pairs (tile centers) on this route, preserved for
  /// serialization/editing round-trips.
  std::vector<std::pair<Vec2, Vec2>> portal_pairs;
  /// For each portal tile position on this route, the path-distance of its
  /// partner portal (forward-only, to avoid soft-lock loops).
  std::vector<std::pair<Vec2, float>> portal_dest;
};

/// A complete level: map, one or more routes, waves, economy, and the stats
/// tables shared by towers/enemies. Loaded from JSON config.
struct Level {
  std::string name;
  int index = -1;  // 1-based for official levels; -1 for custom (editor-created).
  Map map;
  std::vector<LevelRoute> routes;
  std::vector<Wave> waves;
  /// Per-wave pre-delay (length == waves.size()). `gaps[i]` is the time waited
  /// before wave `i` begins: `gaps[0]` from game start to the first wave,
  /// `gaps[i]` (i>0) from the previous wave's last spawn to wave `i`. Gaps are
  /// configurable per wave and need not be uniform.
  /// The order: gap 0 -> wave 0 -> gap 1 -> wave 1 -> ...
  std::vector<float> gaps;
  int starting_resources = 0;
  Resource::AutoIncrease auto_increase{0, 1.0f};
  std::vector<std::string> available_towers;
  TowerStatsTable tower_stats;
  EnemyStatsTable enemy_stats;

  /// Path-distance of the partner portal for a portal tile on the route whose
  /// path is `p`, or -1 if there is no matching portal.
  float paired_portal_distance(Path const* p, Vec2 pos) const;
};

/// Owns all levels and tracks the current one for progression.
class LevelRegistry {
  std::vector<Level> levels_;
  int current_ = 0;

public:
  /// Load towers.json, enemies.json, and levels/level*.json from `config_dir`.
  void load_from_config(std::string const& config_dir);
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
