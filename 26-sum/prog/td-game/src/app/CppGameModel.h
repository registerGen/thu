#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "GameModel.h"
#include "game/game.h"   // Game, LevelRegistry
#include "game/stats.h"  // TowerStatsTable, EnemyStatsTable

/// `GameModel` backed by the C++ model (`game/`). Owns the `Game` and
/// `LevelRegistry` that previously lived directly on `GameController`.
/// Compiled only when `TD_USE_RUST_MODEL` is OFF.
class CppGameModel : public GameModel {
public:
  explicit CppGameModel(std::string config_dir);

  // --- GameModel ---
  bool tick(float dt) override;
  void restart() override;
  void pause() override;
  void resume() override;
  void apply_cheat(std::string code) override;
  bool start_level_json(std::string json) override;
  bool advance_level() override;
  void select_level(int index) override;
  int place_tower(std::string type, Vec2 pos) override;
  bool can_place_at(Vec2 pos) const override;
  GameState state() const override;
  bool paused() const override;
  bool over() const override;
  int score() const override;
  float elapsed_time() const override;
  int current_wave() const override;
  int resource_amount() const override;
  int level_index() const override;
  std::string level_name() const override;
  float map_width() const override;
  float map_height() const override;
  bool has_next_level() const override;
  std::vector<TowerView> tower_views() const override;
  std::vector<EnemyView> enemy_views() const override;
  std::vector<BulletView> bullet_views() const override;
  LevelView level_view() const override;
  GameResultView last_result_view() const override;
  std::vector<GameEventView> take_events() override;
  int current_level_index() const override;
  std::size_t level_count() const override;
  std::vector<LevelInfoView> official_infos() const override;
  std::vector<LevelInfoView> infos() const override;

  // --- editor-only (not on the interface; used via downcast) ---
  TowerStatsTable const& towerStats() const;
  EnemyStatsTable const& enemyStats() const;
  std::string levelJsonAt(int slot) const;
  bool reloadLevels();

private:
  std::string config_dir_;
  LevelRegistry registry_;
  Game game_;
};
