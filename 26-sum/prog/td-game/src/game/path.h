#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "geometry.h"

/// A polyline through the grid that enemies follow from entrance to exit.
/// Stores waypoints, precomputed cumulative segment lengths, and the portal
/// links that sit on the path.
class Path {
  std::vector<Vec2> waypoints_;
  std::vector<float> cumulative_;  // cumulative_[i] = length up to waypoint i
  /// Directional portal pairs: src waypoint index -> tgt waypoint index.
  std::vector<std::pair<std::size_t, std::size_t>> portal_pairs_;
  float total_ = 0.0f;

public:
  Path();
  explicit Path(
    std::vector<Vec2> const& waypoints,
    std::vector<std::pair<std::size_t, std::size_t>> portal_pairs = {}
  );

  std::vector<Vec2> const& waypoints() const;
  float total_length() const;
  /// Distance along the path at waypoint `i` (0 at the first waypoint).
  float cumulative_at(std::size_t i) const;
  /// Position along the path at the given distance. Clamped to [0, total_].
  Vec2 position_at(float distance) const;
  std::vector<std::pair<std::size_t, std::size_t>> const& portal_pairs() const;
  /// Path-distance of the partner portal for `position` (a portal source), or
  /// -1.0f if `position` is not a portal source.
  float paired_portal_distance(Vec2 position) const;
};
