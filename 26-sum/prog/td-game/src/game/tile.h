#pragma once

#include "geometry.h"

/// A tile is a grid square on the map. Any tower may be placed on any non-rock,
/// non-portal tile — including path tiles (a tower on the path blocks enemies,
/// who will stop and attack it until it is destroyed). Terrain is encoded as
/// factor flags set by the level loader (grass / fertile / rock / ice / portal).
class Tile {
  Rect bounds_;
  float resource_cost_factor_;  // fertile: <1 (cheaper towers), else 1
  bool placeable_;              // grass/fertile/ice: true; rock/portal: false
  float enemy_speed_factor_;    // ice: >1 (faster), else 1
  float slow_bullet_factor_;    // ice: <1 (stronger slow), else 1
  bool is_portal_;              // teleports enemies to the paired portal

  // Occupancy is runtime state, not level config: the level is logically const
  // once loaded, but towers come and go during play. Marking it `mutable` lets
  // Game mutate occupancy through a const Level& (no non-const level access needed).
  mutable bool occupied_by_tower_ = false;

public:
  Tile(
    Rect bounds,
    float resource_cost_factor,
    bool can_place_tower,
    float enemy_speed_factor,
    float slow_bullet_factor,
    bool is_portal
  );

  Rect bounds() const;
  Vec2 position() const;
  float resource_cost_factor() const;
  bool can_place_tower() const;
  float enemy_speed_factor() const;
  float slow_bullet_factor() const;
  bool is_portal() const;
  void set_is_portal(bool v);
  bool occupied_by_tower() const;
  void set_occupied_by_tower(bool v) const;
};
