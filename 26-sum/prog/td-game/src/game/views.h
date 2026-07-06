#pragma once

#include <string>
#include <vector>

#include "game/geometry.h"  // Vec2

/// Flat POD views: the model -> view contract. The app/ renders from these and
/// never inspects the model's C++ class hierarchy.

// Kind tags are strings (the config/type name): e.g. tower "normal"/"slow"/
// "poison"/"splash"/"laser"/"resource"/"wall"; enemy "normal"/"fast"/"armored"/
// "resistant"/"splitter"/"boss"; bullet "normal"/"slow"/"poison"/"splash"/"laser";
// tile terrain "grass"/"fertile"/"rock"/"ice"/"portal". Event `kind` is
// "tower_placed"/"enemy_killed"/"wave_started"; `type_tag` carries the entity
// kind name (empty for wave_started).

struct TowerView {
  Vec2 pos;
  Vec2 aim;  // muzzle direction (attack towers); {0,0} for others
  std::string kind;
  int health;
  int max_health;
};

/// Active status-effect flags for rendering (mirrors the model's StatusHint).
struct StatusFlags {
  bool slow = false;
  bool poison = false;
  bool regen = false;
};

struct EnemyView {
  Vec2 pos;
  float half_width;   // bounds half-extent (width/2)
  float half_height;  // bounds half-extent (height/2)
  std::string kind;
  int health;
  int max_health;
  StatusFlags status_hint;
};

struct BulletView {
  Vec2 pos;
  Vec2 vel;  // velocity (for laser direction)
  std::string kind;
  float radius;  // splash radius (0 for non-splash)
};

struct TileView {
  std::string terrain;  // grass/fertile/rock/ice/portal
};

struct LevelInfoView {
  int index;
  std::string name;
};

struct TowerCostView {
  std::string kind;
  int cost;
};

/// Static per-level data (built on level start; cached by the controller).
struct LevelView {
  int index;
  std::string name;
  float map_width;
  float map_height;
  std::vector<TileView> terrain;                  // row-major
  std::vector<std::vector<Vec2>> path_waypoints;  // per path
  std::vector<std::string> available_towers;
  std::vector<TowerCostView> tower_costs;
};

/// Game outcome states (mirrors Game::State; app/ uses this, not Game::State).
enum class GameState { Playing, Victory, Defeat };

/// Discrete game event (replaces GameObserver). Drained each tick by the controller.
struct GameEventView {
  std::string kind;      // "tower_placed"/"enemy_killed"/"wave_started"
  Vec2 pos;              // tower/enemy position ({0,0} for wave_started)
  int a;                 // cost | score | wave number
  std::string type_tag;  // tower/enemy kind name (for color); empty for wave_started
  bool has_boss;         // wave_started only
  bool is_last;          // wave_started only
};

/// Result of a finished level.
struct GameResultView {
  bool cleared;
  bool cheated;
  float time;
  int score;
};
