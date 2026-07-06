#include "path.h"

Path::Path(std::vector<Vec2> const& waypoints) : waypoints_(waypoints) {
  cumulative_.resize(waypoints_.size());
  cumulative_[0] = 0.0f;
  for (std::size_t i = 1; i < waypoints_.size(); ++i) {
    cumulative_[i] = cumulative_[i - 1] + (waypoints_[i] - waypoints_[i - 1]).length();
  }
  total_ = cumulative_.empty() ? 0.0f : cumulative_.back();
}

std::vector<Vec2> const& Path::waypoints() const { return waypoints_; }
float Path::total_length() const { return total_; }
float Path::cumulative_at(std::size_t i) const { return cumulative_[i]; }

Vec2 Path::position_at(float distance) const {
  if (waypoints_.size() == 1) return waypoints_.front();
  if (distance <= 0.0f) return waypoints_.front();
  if (distance >= total_) return waypoints_.back();

  std::size_t i = 1;
  while (i < cumulative_.size() && cumulative_[i] < distance) ++i;
  float seg_start = cumulative_[i - 1];
  float seg_len = cumulative_[i] - seg_start;
  float t = seg_len > 0.0f ? (distance - seg_start) / seg_len : 0.0f;
  return waypoints_[i - 1] + (waypoints_[i] - waypoints_[i - 1]) * t;
}
