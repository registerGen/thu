#pragma once

#include <cstddef>
#include <vector>

#include "geometry.h"

/// A polyline through the grid that enemies follow from entrance to exit.
/// Stores waypoints and precomputed cumulative segment lengths so that
/// `position_at(distance)` is O(N) via linear scan (fine for small maps).
class Path {
  std::vector<Vec2> waypoints_;
  std::vector<float> cumulative_;  // cumulative_[i] = length up to waypoint i
  float total_ = 0.0f;

public:
  Path() = default;
  explicit Path(std::vector<Vec2> const& waypoints);

  std::vector<Vec2> const& waypoints() const;
  float total_length() const;
  /// Distance along the path at waypoint `i` (0 at the first waypoint).
  float cumulative_at(std::size_t i) const;
  /// Position along the path at the given distance. Clamped to [0, total_].
  Vec2 position_at(float distance) const;
};
