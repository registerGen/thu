#pragma once

#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "bullet.h"
#include "enemy.h"
#include "level.h"
#include "resource.h"
#include "tower.h"
#include "views.h"
#include "wave.h"

/// The whole game state, source of truth.
/// Framework-agnostic: Qt drives it via update() and the command/query API.
class Game {
public:
  /// Game outcome states.
  enum class State { Playing, Victory, Defeat };

  /// Recorded result of a finished level (cleared or not, time, score).
  struct Result {
    bool cleared = false;
    bool cheated = false;
    float time = 0.0f;
    int score = 0;
  };

  /// Default-constructed; no level is active until `start_level` is called
  /// (the app layer owns the LevelRegistry and starts a level).
  Game() = default;

  Game(Game const&) = delete;
  Game& operator=(Game const&) = delete;
  Game(Game&&) = delete;
  Game& operator=(Game&&) = delete;

  // --- commands (called by the controller / Qt) -----------------------------
  /// Grant resources to the player (used by resource towers).
  void grant_resource(int amount);
  /// Place a tower of `type` at the map position. Returns 0 if the tower is
  /// successfully placed, 1 if the tile is invalid/occupied or the type isn't
  /// available this level, 2 if resources are insufficient.
  int place_tower(std::string const& type, Vec2 position);
  /// Queue an enemy to spawn at the start of the next update.
  void spawn_enemy(std::shared_ptr<Enemy> enemy);
  /// Add a bullet to the world (used by attack towers).
  void spawn_bullet(std::unique_ptr<Bullet> bullet);

  /// Begin (or restart) simulating `level`.
  void start_level(Level const& level);

  void pause();
  void resume();
  void restart();  // reload the current level from scratch

  void apply_cheat(std::string_view code);

  // --- main loop ------------------------------------------------------------
  /// Advance the simulation by dt seconds. Returns true if the level has ended
  /// (victory or defeat); the caller may then check state().
  bool update(float dt);

  // --- queries (read by the view / Qt) --------------------------------------
  Level const& level() const;
  Map const& map() const;
  Resource const& resource() const;
  std::vector<std::unique_ptr<Tower>> const& towers() const;
  std::vector<std::shared_ptr<Enemy>> const& enemies() const;
  std::vector<std::unique_ptr<Bullet>> const& bullets() const;
  Game::State state() const;
  bool over() const;
  bool paused() const;
  int score() const;
  float elapsed_time() const;
  int current_wave() const;
  bool all_waves_done() const;
  Result last_result() const;
  std::mt19937& rng();

  // --- view producers (temporary C++; replaced by FFI when the model is Rust) ---
  /// Per-frame entity snapshots for rendering. The app/ reads these instead of
  /// iterating the model's containers or dynamic_casting its class hierarchy.
  std::vector<TowerView> tower_views() const;
  std::vector<EnemyView> enemy_views() const;
  std::vector<BulletView> bullet_views() const;
  /// Static per-level data (map terrain, path waypoints, tower costs, etc.).
  LevelView level_view() const;
  /// Drain the discrete-event queue (replaces GameObserver).
  std::vector<GameEventView> take_events();
  /// Result of the finished level, as a flat POD.
  GameResultView last_result_view() const;
  /// Check if a tower may be placed at `position` for ghost hover rendering,
  /// taking terrain and occupancy into account. The resource amount is not
  /// considered here.
  bool can_place_at(Vec2 pos) const;
  /// Push a wave-started event (called by WaveManager via Game&).
  void push_wave_started(int wave, bool has_boss, bool is_last);

private:
  void end_level(bool cleared);  // set state, record result, notify observers
  void clear_entities();

  bool update_movables(float dt);  // Return true if an enemy reached the exit.
  void check_collisions();
  void update_towers(float dt);

  static constexpr float PORTAL_COOLDOWN = 0.5f;

  std::unique_ptr<Level> level_;  // the level being simulated (owned by Game)
  Resource resource_{0, Resource::AutoIncrease{0, 1.0f}};
  std::vector<std::unique_ptr<Tower>> towers_;
  std::vector<std::shared_ptr<Enemy>> enemies_;  // shared_ptr because TowerDamage observes it
  std::vector<std::unique_ptr<Bullet>> bullets_;
  WaveManager waves_;
  std::mt19937 rng_{std::random_device{}()};
  std::vector<std::shared_ptr<Enemy>> pending_spawns_;  // flushed at the start of update()
  int score_ = 0;
  float elapsed_time_ = 0.0f;
  bool paused_ = false;
  Game::State state_ = Game::State::Playing;
  bool cheated_ = false;
  Result last_result_;
  std::vector<GameEventView> events_;  // discrete-event queue (drained by take_events)
};
