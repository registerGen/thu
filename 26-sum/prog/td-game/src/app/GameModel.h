#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Views.h"  // Vec2, GameState, *View types (game/views.h in both backends)

/// Backend-agnostic model interface. `GameController` owns a
/// `std::unique_ptr<GameModel>`; the impl is `CppGameModel` (C++ model) or
/// `RustGameModel` (Rust core via cxx), selected at CMake time by
/// `TD_USE_RUST_MODEL`. View types are the `game/views.h` POD structs in both
/// backends — `RustGameModel` converts the cxx `td::ffi::*` types at the
/// boundary.
class GameModel {
public:
  virtual ~GameModel() = default;

  // --- lifecycle ---
  /// Advance the simulation by `dt` seconds. Returns true if the level ended.
  virtual bool tick(float dt) = 0;
  virtual void restart() = 0;
  virtual void pause() = 0;
  virtual void resume() = 0;
  virtual void apply_cheat(std::string code) = 0;
  /// Load + start a custom level from JSON. Returns false on parse failure.
  virtual bool start_level_json(std::string json) = 0;
  /// Advance to the next official level. Returns false if there isn't one.
  virtual bool advance_level() = 0;
  virtual void select_level(int index) = 0;

  // --- commands ---
  /// 0 = placed, 1 = not placeable, 2 = insufficient resources.
  virtual int place_tower(std::string type, Vec2 pos) = 0;
  virtual bool can_place_at(Vec2 pos) const = 0;

  // --- scalar queries ---
  virtual GameState state() const = 0;
  virtual bool paused() const = 0;
  virtual bool over() const = 0;
  virtual int score() const = 0;
  virtual float elapsed_time() const = 0;
  virtual int current_wave() const = 0;
  virtual int resource_amount() const = 0;
  virtual int level_index() const = 0;
  virtual std::string level_name() const = 0;
  virtual float map_width() const = 0;
  virtual float map_height() const = 0;
  virtual bool has_next_level() const = 0;

  // --- view snapshots ---
  virtual std::vector<TowerView> tower_views() const = 0;
  virtual std::vector<EnemyView> enemy_views() const = 0;
  virtual std::vector<BulletView> bullet_views() const = 0;
  virtual LevelView level_view() const = 0;
  virtual GameResultView last_result_view() const = 0;
  virtual std::vector<GameEventView> take_events() = 0;

  // --- registry queries ---
  virtual int current_level_index() const = 0;
  virtual std::size_t level_count() const = 0;
  virtual std::vector<LevelInfoView> official_infos() const = 0;
  virtual std::vector<LevelInfoView> infos() const = 0;
};
