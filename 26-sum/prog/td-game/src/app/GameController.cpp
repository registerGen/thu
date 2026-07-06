#include "GameController.h"

#include "Theme.h"

#ifdef TD_USE_RUST_MODEL
# include "RustGameModel.h"
#else
# include "CppGameModel.h"
# include "game/stats.h"  // TowerStatsTable, EnemyStatsTable (editor accessors)
#endif

GameController::GameController(QString config_dir, QObject* parent)
    : QObject(parent), config_dir_(config_dir) {
#ifdef TD_USE_RUST_MODEL
  model_ = RustGameModel::create(config_dir_.toStdString());
#else
  model_ = std::make_unique<CppGameModel>(config_dir_.toStdString());
#endif
}

GameController::~GameController() = default;

void GameController::tick(float dt) {
  if (dt > 0.0f) {
    float instant = 1.0f / dt;
    fps_ = fps_ == 0.0f ? instant : fps_ * 0.9f + instant * 0.1f;
  }

  bool over = model_->tick(dt);

  // Drain the event queue (replaces GameObserver).
  for (auto const& e : model_->take_events()) {
    if (e.kind == "tower_placed")
      emit towerPlaced(e.pos, e.a, theme::towerColorForType(e.type_tag));
    else if (e.kind == "enemy_killed")
      emit enemyKilled(e.pos, e.a, theme::enemyColorForType(e.type_tag));
    else if (e.kind == "wave_started")
      emit waveStarted(e.a, e.has_boss, e.is_last);
  }

  emit ticked();
  if (over) emit stateChanged(state());
}

void GameController::selectTowerType(QString type) { selected_tower_type_ = type; }

int GameController::placeAt(Vec2 map_pos) {
  if (selected_tower_type_.isEmpty()) return 0;
  if (model_->paused() || model_->over()) return 0;

  std::string type = selected_tower_type_.toStdString();
  int status = model_->place_tower(std::move(type), map_pos);
  if (status == 1)
    emit placementFailed();
  else if (status == 2)
    emit insufficientResource();
  return status;
}

void GameController::togglePause() {
  if (model_->paused())
    model_->resume();
  else
    model_->pause();
}

void GameController::startLevelTransition_(std::function<void()> transition) {
  selected_tower_type_.clear();
  transition();
  emit levelStarted();
}

void GameController::restartLevel() {
  startLevelTransition_([this] { model_->restart(); });
}

bool GameController::nextLevel() {
  bool advanced = false;
  startLevelTransition_([&] { advanced = model_->advance_level(); });
  return advanced;
}

void GameController::selectLevel(int index) {
  startLevelTransition_([this, index] { model_->select_level(index); });
}

bool GameController::playCustomLevel(std::string json) {
  selected_tower_type_.clear();
  if (!model_->start_level_json(std::move(json))) return false;
  emit levelStarted();
  return true;
}

void GameController::applyCheat(QString const& code) { model_->apply_cheat(code.toStdString()); }

// --- scalar queries ---
GameState GameController::state() const { return model_->state(); }
bool GameController::paused() const { return model_->paused(); }
int GameController::score() const { return model_->score(); }
float GameController::elapsedTime() const { return model_->elapsed_time(); }
int GameController::currentWave() const { return model_->current_wave(); }
int GameController::resourceAmount() const { return model_->resource_amount(); }
int GameController::levelIndex() const { return model_->level_index(); }
std::string GameController::levelName() const { return model_->level_name(); }
float GameController::mapWidth() const { return model_->map_width(); }
float GameController::mapHeight() const { return model_->map_height(); }
bool GameController::hasNextLevel() const { return model_->has_next_level(); }

// --- view snapshots ---
std::vector<TowerView> GameController::towerViews() const { return model_->tower_views(); }
std::vector<EnemyView> GameController::enemyViews() const { return model_->enemy_views(); }
std::vector<BulletView> GameController::bulletViews() const { return model_->bullet_views(); }
LevelView GameController::levelView() const { return model_->level_view(); }
GameResultView GameController::lastResultView() const { return model_->last_result_view(); }
bool GameController::canPlaceAt(Vec2 pos) const { return model_->can_place_at(pos); }

// --- registry queries ---
int GameController::currentLevelIndex() const { return model_->current_level_index(); }
std::size_t GameController::levelCount() const { return model_->level_count(); }
std::vector<LevelInfoView> GameController::officialInfos() const {
  return model_->official_infos();
}
std::vector<LevelInfoView> GameController::infos() const { return model_->infos(); }

#ifndef TD_USE_RUST_MODEL
// --- editor-only (C++ backend) ---
std::string GameController::levelJsonAt(int slot) const {
  return static_cast<CppGameModel*>(model_.get())->levelJsonAt(slot);
}
TowerStatsTable const& GameController::towerStats() const {
  return static_cast<CppGameModel*>(model_.get())->towerStats();
}
EnemyStatsTable const& GameController::enemyStats() const {
  return static_cast<CppGameModel*>(model_.get())->enemyStats();
}
bool GameController::reloadLevels() {
  return static_cast<CppGameModel*>(model_.get())->reloadLevels();
}
#endif
