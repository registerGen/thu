#pragma once

#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "bullet.h"
#include "enemy.h"
#include "level.h"
#include "observer.h"
#include "resource.h"
#include "tower.h"
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
  /// Register an observer to receive discrete game events.
  void add_observer(GameObserver* o);
  std::vector<GameObserver*> const& observers() const;
  /// Grant resources to the player (used by resource towers).
  void grant_resource(int amount);
  /// Place a tower of `type` at the map position. Returns false if the tile is
  /// invalid/occupied, the type isn't available this level, or resources are
  /// insufficient.
  bool place_tower(std::string const& type, Vec2 position);
  /// Queue an enemy to spawn at the start of the next update.
  void spawn_enemy(std::shared_ptr<Enemy> enemy);
  /// Add a bullet to the world (used by attack towers).
  void spawn_bullet(std::unique_ptr<Bullet> bullet);
  /// Clear all entities (towers, enemies, bullets) on the screen.
  void clear_entities();

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
  /// Cost to place `type` at `position` (scales with the tile's resource-cost
  /// factor), or -1 if the type or tile is invalid.
  int placement_cost(std::string const& type, Vec2 position) const;
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

private:
  void end_level(bool cleared);  // set state, record result, notify observers

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
  std::vector<GameObserver*> observers_;
};
