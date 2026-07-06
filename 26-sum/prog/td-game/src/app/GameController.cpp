#include "GameController.h"

#include "Theme.h"

GameController::GameController(QString config_dir, QObject* parent)
    : QObject(parent), config_dir_(config_dir) {
  registry_.load_from_config(config_dir_.toStdString());
  game_.add_observer(this);
  game_.start_level(registry_.current());
}

void GameController::tick(float dt) {
  if (dt > 0.0f) {
    float instant = 1.0f / dt;
    fps_ = fps_ == 0.0f ? instant : fps_ * 0.9f + instant * 0.1f;
  }

  bool over = game_.update(dt);
  emit ticked();
  if (over) emit stateChanged(game_.state());
}

void GameController::selectTowerType(QString type) { selected_tower_type_ = type; }

bool GameController::placeAt(Vec2 map_pos) {
  if (selected_tower_type_.isEmpty()) return false;
  if (game_.paused() || game_.over()) return false;

  std::string type = selected_tower_type_.toStdString();
  int cost = game_.placement_cost(type, map_pos);
  if (cost < 0) {
    emit placementFailed();  // invalid tile/type
    return false;
  }
  if (game_.resource().amount() < cost) {
    emit insufficientResource();
    return false;
  }
  bool ok = game_.place_tower(type, map_pos);
  if (!ok) emit placementFailed();  // e.g. tile occupied or not placeable
  return ok;
}

void GameController::clearEntities() { game_.clear_entities(); }

void GameController::togglePause() {
  if (game_.paused())
    game_.resume();
  else
    game_.pause();
}

void GameController::startLevelTransition_(std::function<void()> transition) {
  selected_tower_type_.clear();
  transition();
  emit levelStarted();
}

void GameController::restartLevel() {
  startLevelTransition_([this] { game_.restart(); });
}
bool GameController::nextLevel() {
  bool advanced = false;
  startLevelTransition_([&] {
    if (registry_.has_next_official()) {
      registry_.advance();
      game_.start_level(registry_.current());
      advanced = true;
    }
  });
  return advanced;
}
void GameController::selectLevel(int index) {
  startLevelTransition_([this, index] {
    registry_.select(index);
    game_.start_level(registry_.current());
  });
}
void GameController::playCustomLevel(Level const& level) {
  startLevelTransition_([this, &level] { game_.start_level(level); });
}
void GameController::applyCheat(QString const& code) {
  // Game::update will automatically handle the state change when the
  // instant win cheat code is applied.
  game_.apply_cheat(code.toStdString());
}

int GameController::current_level_index() const { return registry_.current_index(); }
std::size_t GameController::level_count() const { return registry_.size(); }
bool GameController::has_next_level() const { return registry_.has_next_official(); }
std::vector<LevelRegistry::Info> GameController::official_level_infos() const {
  return registry_.official_infos();
}
std::vector<LevelRegistry::Info> GameController::level_infos() const { return registry_.infos(); }
Level const& GameController::level_at(int slot) const {
  return registry_.at(static_cast<std::size_t>(slot));
}

void GameController::reloadLevels() {
  int slot = registry_.current_index();
  registry_.load_from_config(config_dir_.toStdString());
  if (slot >= static_cast<int>(registry_.size())) slot = static_cast<int>(registry_.size()) - 1;
  if (slot < 0) slot = 0;
  registry_.select(slot);
  // Do not start_level: the editor is the active screen, no game in progress.
}

void GameController::on_tower_placed(Tower const& t, int cost) {
  emit towerPlaced(t.tile()->position(), cost, Qt::darkGray);
}

void GameController::on_enemy_killed(Enemy const& e) {
  emit enemyKilled(e.position(), e.score_value(), theme::enemyColor(e));
}

void GameController::on_wave_started(int wave, bool has_boss, bool is_last) {
  emit waveStarted(wave, has_boss, is_last);
}
