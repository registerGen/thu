#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <functional>

#include "game/game.h"
#include "game/geometry.h"
#include "game/observer.h"

/// A lightweight wrapper over the Game model: it exposes the command/query API
/// the UI uses and translates model outcomes into Qt signals. The widget drives
/// the simulation cadence (it owns the QTimer) and calls `tick(dt)` each frame.
class GameController : public QObject, public GameObserver {
  Q_OBJECT

public:
  explicit GameController(QString config_dir, QObject* parent = nullptr);

  Game const& game() const { return game_; }

  /// Advance the simulation by `dt` seconds (driven by the widget's timer).
  /// Emits `ticked` afterwards, and `stateChanged` if the level just ended.
  void tick(float dt);

  // --- commands (called by the UI) ---
  void selectTowerType(QString type);
  bool placeAt(Vec2 map_pos);
  void clearEntities();
  void togglePause();
  void restartLevel();
  bool nextLevel();
  void selectLevel(int index);
  void playCustomLevel(Level const& level);
  void applyCheat(QString const& code);

  QString selectedTowerType() const { return selected_tower_type_; }
  /// Smoothed frames-per-second of the game loop (for the HUD).
  float fps() const { return fps_; }

  // --- level-collection queries (backed by the LevelRegistry) ---
  int current_level_index() const;
  std::size_t level_count() const;
  /// True if the next level in the registry is official (for "Next Level" visibility).
  bool has_next_level() const;
  /// {index, name} of official levels only — for the start menu.
  std::vector<LevelRegistry::Info> official_level_infos() const;
  /// {index, name} of every level (the editor filters to custom by slot).
  std::vector<LevelRegistry::Info> level_infos() const;
  /// The level at `slot` (0-based position) — for the editor's custom-level picker.
  Level const& level_at(int slot) const;
  /// Re-read config/levels/ (e.g. after the editor saves a custom level).
  void reloadLevels();

  // --- GameObserver overrides (forward model events as Qt signals) ---
  void on_tower_placed(Tower const& t, int cost) override;
  void on_enemy_killed(Enemy const& e) override;
  void on_wave_started(int wave, bool has_boss, bool is_last) override;

signals:
  /// Emitted after each frame; the widget repaint/refresh.
  void ticked();
  void stateChanged(Game::State state);
  void placementFailed();
  /// Emitted when the player tries to build but can't afford the tower.
  void insufficientResource();
  void towerPlaced(Vec2 pos, int cost, QColor color);
  void enemyKilled(Vec2 pos, int score, QColor color);
  void waveStarted(int wave, bool has_boss, bool is_last);
  /// Emitted whenever a level (re)starts — restart, next, select, or custom.
  /// Drives the palette rebuild and the HUD/view transient-state reset, so all
  /// level transitions reset consistently without callers remembering to.
  void levelStarted();

private:
  LevelRegistry registry_;
  Game game_;
  QString config_dir_;
  QString selected_tower_type_;
  float fps_ = 0.0f;

  /// Common level-transition entry point: clears the tower selection, runs the
  /// transition, and emits `levelStarted`. Every public transition funnels
  /// through here so the reset can't be forgotten.
  void startLevelTransition_(std::function<void()> transition);
};
