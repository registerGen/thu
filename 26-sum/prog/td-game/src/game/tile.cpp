#include "tile.h"

Tile::Tile(
  Rect bounds,
  float resource_cost_factor,
  bool can_place_tower,
  float enemy_speed_factor,
  float slow_bullet_factor,
  bool is_portal
)
    : bounds_(bounds),
      resource_cost_factor_(resource_cost_factor),
      placeable_(can_place_tower),
      enemy_speed_factor_(enemy_speed_factor),
      slow_bullet_factor_(slow_bullet_factor),
      is_portal_(is_portal) { }

Rect Tile::bounds() const { return bounds_; }
Vec2 Tile::position() const { return bounds_.center; }
float Tile::resource_cost_factor() const { return resource_cost_factor_; }
bool Tile::can_place_tower() const { return placeable_ && !is_portal_; }
float Tile::enemy_speed_factor() const { return enemy_speed_factor_; }
float Tile::slow_bullet_factor() const { return slow_bullet_factor_; }
bool Tile::is_portal() const { return is_portal_; }
void Tile::set_is_portal(bool v) { is_portal_ = v; }
bool Tile::occupied_by_tower() const { return occupied_by_tower_; }
void Tile::set_occupied_by_tower(bool v) const { occupied_by_tower_ = v; }
