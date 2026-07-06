#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Views.h"  // view types (game/views.h in both backends)

#ifndef TD_USE_RUST_MODEL
# include "game/stats.h"  // TowerStatsTable, EnemyStatsTable (editor accessors)
#endif

class GameModel;

/// A lightweight wrapper over the model: it exposes the command/query API the
/// UI uses and translates model outcomes into Qt signals. The widget drives the
/// simulation cadence (it owns the QTimer) and calls `tick(dt)` each frame.
/// The app/ never touches the model directly — it reads flat POD views and
/// drains the event queue via this controller.
class GameController : public QObject {
  Q_OBJECT

public:
  explicit GameController(QString config_dir, QObject* parent = nullptr);
  ~GameController();  // defined in the .cpp (where GameModel is complete)

  /// Advance the simulation by `dt` seconds. Drains the event queue and emits
  /// towerPlaced/enemyKilled/waveStarted/stateChanged as appropriate.
  void tick(float dt);

  // --- commands (called by the UI) ---
  void selectTowerType(QString type);
  int placeAt(Vec2 map_pos);
  void togglePause();
  void restartLevel();
  bool nextLevel();
  void selectLevel(int index);
  bool playCustomLevel(std::string json);
  void applyCheat(QString const& code);

  QString selectedTowerType() const { return selected_tower_type_; }
  float fps() const { return fps_; }

  // --- scalar queries ---
  GameState state() const;
  bool paused() const;
  int score() const;
  float elapsedTime() const;
  int currentWave() const;
  int resourceAmount() const;
  int levelIndex() const;
  std::string levelName() const;
  float mapWidth() const;
  float mapHeight() const;
  bool hasNextLevel() const;

  // --- view snapshots (for rendering) ---
  std::vector<TowerView> towerViews() const;
  std::vector<EnemyView> enemyViews() const;
  std::vector<BulletView> bulletViews() const;
  LevelView levelView() const;
  GameResultView lastResultView() const;
  bool canPlaceAt(Vec2 pos) const;

  // --- registry queries ---
  int currentLevelIndex() const;
  std::size_t levelCount() const;
  std::vector<LevelInfoView> officialInfos() const;
  std::vector<LevelInfoView> infos() const;

#ifndef TD_USE_RUST_MODEL
  // --- editor-only (backed by the C++ model; the LevelEditor is gated off
  // when the Rust backend is active). ---
  std::string levelJsonAt(int slot) const;
  TowerStatsTable const& towerStats() const;
  EnemyStatsTable const& enemyStats() const;
  bool reloadLevels();
#endif

signals:
  void ticked();
  void stateChanged(GameState state);
  void placementFailed();
  void insufficientResource();
  void towerPlaced(Vec2 pos, int cost, QColor color);
  void enemyKilled(Vec2 pos, int score, QColor color);
  void waveStarted(int wave, bool has_boss, bool is_last);
  void levelStarted();

private:
  std::unique_ptr<GameModel> model_;
  QString config_dir_;
  QString selected_tower_type_;
  float fps_ = 0.0f;

  void startLevelTransition_(std::function<void()> transition);
};
