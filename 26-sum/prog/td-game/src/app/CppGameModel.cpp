#include "CppGameModel.h"

#include <utility>

#include "game/config.h"

CppGameModel::CppGameModel(std::string config_dir) : config_dir_(std::move(config_dir)) {
  registry_.load_from_dir(config_dir_);
  game_.start_level(registry_.current());
}

bool CppGameModel::tick(float dt) { return game_.update(dt); }

void CppGameModel::restart() { game_.restart(); }

void CppGameModel::pause() { game_.pause(); }
void CppGameModel::resume() { game_.resume(); }

void CppGameModel::apply_cheat(std::string code) { game_.apply_cheat(std::move(code)); }

bool CppGameModel::start_level_json(std::string json) {
  try {
    Level lvl = config::load_level_json(json, game_.level().tower_stats, game_.level().enemy_stats);
    game_.start_level(std::move(lvl));
    return true;
  } catch (...) {
    return false;
  }
}

bool CppGameModel::advance_level() {
  if (!registry_.has_next_official()) return false;
  registry_.advance();
  game_.start_level(registry_.current());
  return true;
}

void CppGameModel::select_level(int index) {
  registry_.select(index);
  game_.start_level(registry_.current());
}

int CppGameModel::place_tower(std::string type, Vec2 pos) {
  return game_.place_tower(std::move(type), pos);
}

bool CppGameModel::can_place_at(Vec2 pos) const { return game_.can_place_at(pos); }

GameState CppGameModel::state() const {
  switch (game_.state()) {
  case Game::State::Victory:
    return GameState::Victory;
  case Game::State::Defeat:
    return GameState::Defeat;
  default:
    return GameState::Playing;
  }
}

bool CppGameModel::paused() const { return game_.paused(); }
bool CppGameModel::over() const { return game_.over(); }
int CppGameModel::score() const { return game_.score(); }
float CppGameModel::elapsed_time() const { return game_.elapsed_time(); }
int CppGameModel::current_wave() const { return game_.current_wave(); }
int CppGameModel::resource_amount() const { return game_.resource().amount(); }
int CppGameModel::level_index() const { return game_.level().index; }
std::string CppGameModel::level_name() const { return game_.level().name; }
float CppGameModel::map_width() const { return game_.map().width(); }
float CppGameModel::map_height() const { return game_.map().height(); }
bool CppGameModel::has_next_level() const { return registry_.has_next_official(); }

std::vector<TowerView> CppGameModel::tower_views() const { return game_.tower_views(); }
std::vector<EnemyView> CppGameModel::enemy_views() const { return game_.enemy_views(); }
std::vector<BulletView> CppGameModel::bullet_views() const { return game_.bullet_views(); }
LevelView CppGameModel::level_view() const { return game_.level_view(); }
GameResultView CppGameModel::last_result_view() const { return game_.last_result_view(); }
std::vector<GameEventView> CppGameModel::take_events() { return game_.take_events(); }

int CppGameModel::current_level_index() const { return registry_.current_index(); }
std::size_t CppGameModel::level_count() const { return registry_.size(); }

std::vector<LevelInfoView> CppGameModel::official_infos() const {
  std::vector<LevelInfoView> out;
  for (auto const& info : registry_.official_infos()) out.push_back({info.index, info.name});
  return out;
}
std::vector<LevelInfoView> CppGameModel::infos() const {
  std::vector<LevelInfoView> out;
  for (auto const& info : registry_.infos()) out.push_back({info.index, info.name});
  return out;
}

// --- editor-only ---

TowerStatsTable const& CppGameModel::towerStats() const { return game_.level().tower_stats; }
EnemyStatsTable const& CppGameModel::enemyStats() const { return game_.level().enemy_stats; }

std::string CppGameModel::levelJsonAt(int slot) const {
  return config::save_level_json(registry_.at(static_cast<std::size_t>(slot)));
}

bool CppGameModel::reloadLevels() {
  int slot = registry_.current_index();
  // Load into a fresh registry and swap only on success: load_from_dir clears
  // its target first, so a failure mid-load would otherwise leave registry_
  // empty and the editor operating on a broken registry.
  LevelRegistry fresh;
  try {
    fresh.load_from_dir(config_dir_);
  } catch (...) {
    return false;
  }
  if (slot >= static_cast<int>(fresh.size())) slot = static_cast<int>(fresh.size()) - 1;
  if (slot < 0) slot = 0;
  fresh.select(slot);
  registry_ = std::move(fresh);
  return true;
}
