#pragma once

#include <vector>

#include "geometry.h"
#include "tile.h"

/// A map is a rectangular area that contains tiles, defining the playable area.
class Map {
  float width_;
  float height_;
  std::vector<Tile> tiles_;

public:
  Map(float width, float height, std::vector<Tile> const& tiles = {});

  float width() const;
  float height() const;
  std::vector<Tile> const& tiles() const;

  /// Reset all tiles' tower-occupancy flags (used on restart). Const because
  /// occupancy is mutable runtime state.
  void clear_occupancy() const;

  Tile const* tile_at(Vec2 position) const;
};
