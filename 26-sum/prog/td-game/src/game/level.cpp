#include "level.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "config.h"

float Level::paired_portal_distance(Path const* p, Vec2 pos) const {
  for (auto const& route : routes) {
    if (&route.path != p) continue;
    for (auto const& [q, d] : route.portal_dest) {
      if (q.x == pos.x && q.y == pos.y) return d;
    }
    return -1.0f;
  }
  return -1.0f;
}

Level const& LevelRegistry::current() const {
  return levels_.at(static_cast<std::size_t>(current_));
}
Level const& LevelRegistry::at(std::size_t slot) const { return levels_.at(slot); }
int LevelRegistry::current_index() const { return current_; }
bool LevelRegistry::has_next_official() const {
  int next = current_ + 1;
  return next < static_cast<int>(levels_.size()) &&
         levels_[static_cast<std::size_t>(next)].index >= 1;
}
void LevelRegistry::advance() {
  if (has_next_official()) ++current_;
}
void LevelRegistry::select(int index) {
  if (index < 0) index = 0;
  if (index >= static_cast<int>(levels_.size())) index = static_cast<int>(levels_.size()) - 1;
  current_ = index;
}
std::vector<LevelRegistry::Info> LevelRegistry::infos() const {
  std::vector<Info> out;
  out.reserve(levels_.size());
  for (auto const& lvl : levels_) out.push_back({lvl.index, lvl.name});
  return out;
}
std::vector<LevelRegistry::Info> LevelRegistry::official_infos() const {
  std::vector<Info> out;
  for (auto const& lvl : levels_)
    if (lvl.index >= 1) out.push_back({lvl.index, lvl.name});
  return out;
}
std::size_t LevelRegistry::size() const { return levels_.size(); }

void LevelRegistry::load_from_config(std::string const& config_dir) {
  levels_.clear();
  current_ = 0;

  TowerStatsTable towers = config::load_towers(config_dir + "/towers.json");
  EnemyStatsTable enemies = config::load_enemies(config_dir + "/enemies.json");

  // Glob config/levels/*.json, then sort: official levels (index >= 1) ascending
  // by index, custom levels (index == -1) by name, appended after official.
  namespace fs = std::filesystem;
  fs::path levels_dir = fs::path(config_dir) / "levels";
  std::vector<Level> official, custom;
  if (fs::exists(levels_dir)) {
    for (auto const& entry : fs::directory_iterator(levels_dir)) {
      if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
      try {
        Level lvl = config::load_level(entry.path().string(), towers, enemies);
        if (lvl.index >= 1)
          official.push_back(std::move(lvl));
        else
          custom.push_back(std::move(lvl));
      } catch (std::exception const&) {
        // Skip malformed/unreadable level files so one bad file (e.g. a
        // hand-edited custom level) doesn't prevent the rest from loading.
      }
    }
  }
  std::sort(official.begin(), official.end(), [](Level const& a, Level const& b) {
    return a.index < b.index;
  });
  // Drop duplicate official indices (keep the first) so two "Level 2" entries
  // can't coexist. Non-consecutive indices (1,3,5) are fine — the index is data.
  official.erase(
    std::unique(
      official.begin(),
      official.end(),
      [](Level const& a, Level const& b) { return a.index == b.index; }
    ),
    official.end()
  );
  std::sort(custom.begin(), custom.end(), [](Level const& a, Level const& b) {
    return a.name < b.name;
  });

  levels_ = std::move(official);
  levels_.insert(
    levels_.end(),
    std::make_move_iterator(custom.begin()),
    std::make_move_iterator(custom.end())
  );

  if (levels_.empty()) {
    throw std::runtime_error("no levels found in " + config_dir);
  }
}
