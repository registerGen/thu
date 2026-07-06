#include "map.h"

Map::Map(float width, float height, std::vector<Tile> const& tiles)
    : width_(width), height_(height), tiles_(tiles) { }

float Map::width() const { return width_; }
float Map::height() const { return height_; }
std::vector<Tile> const& Map::tiles() const { return tiles_; }

void Map::clear_occupancy() const {
  for (auto const& tile : tiles_) {
    tile.set_occupied_by_tower(false);
  }
}

Tile const* Map::tile_at(Vec2 position) const {
  for (auto const& tile : tiles_) {
    if (tile.bounds().contains(position)) {
      return &tile;
    }
  }
  return nullptr;
}
