#pragma once

#include <memory>
#include <string>
#include <vector>

#include "geometry.h"
#include "status_effect.h"
#include "timer.h"

class Game;
class Path;

/// An enemy follows the map's path toward the exit and can attack towers and be
/// attacked by bullets. Abstract base; concrete types subclass it and override
/// the virtual hooks (slow resistance, splash resistance, shield, on-death
/// spawns, regeneration, ...).
class Enemy {
public:
  Enemy(
    std::string const& type,
    Rect bounds,
    float speed,
    int health,
    int tower_damage,
    float tower_damage_cooldown
  );
  virtual ~Enemy();

  std::string const& type() const;
  Rect bounds() const;
  Vec2 position() const;
  int health() const;
  int max_health() const;
  void set_health(int health);
  void increase_health(int amount);
  void decrease_health(int amount);
  bool is_destroyed() const;
  int tower_damage() const;
  float tower_damage_cooldown() const;
  int score_value() const;
  void set_score(int s);

  Path const* path() const;
  void set_path(Path const* path);
  float path_distance() const;
  void set_path_distance(float d);
  /// Recompute bounds center from the current path distance.
  void sync_position();
  /// True when the portal-teleport cooldown has elapsed (ready to teleport).
  bool portal_ready() const;
  /// Arm the portal cooldown for `duration` seconds after a teleport. The cooldown
  /// mechanism prevents an enemy from moving back and forth between a portal pair.
  void start_portal_cooldown(float duration);

  /// Predicted position `time_ahead` seconds from now, following the path at
  /// current speed. Used by towers to lead moving targets.
  Vec2 predicted_position(float time_ahead) const;

  /// Resistance to slow effects (>=1.0; higher = less slowed).
  virtual float slow_resist_factor() const;
  /// Multiplier applied to splash/explosive damage taken (<=1.0).
  virtual float splash_damage_factor() const;
  /// Reduce incoming bullet damage (e.g. shield). Default: none.
  virtual int mitigate(int damage) const;
  /// Called once when the enemy dies (e.g. splitter spawns children).
  virtual void on_death(Game&);

  void apply_status_effect(std::unique_ptr<StatusEffect> effect);

  /// Read-only hint of which status effects are currently active (for rendering).
  struct StatusHint {
    bool slow = false, poison = false, regen = false;
  };
  StatusHint status_hint() const;

  /// Advance along the path, tick status effects, remove expired ones.
  virtual void update(Game& game, float dt);

protected:
  std::string type_;
  Rect bounds_;
  float speed_;
  int health_;
  int max_health_;
  int tower_damage_;
  float tower_damage_cooldown_;
  int score_ = 0;
  Path const* path_ = nullptr;
  float path_distance_ = 0.0f;
  CountdownTimer portal_cooldown_{0.0f, false};  // ready until armed after a teleport
  std::vector<std::unique_ptr<StatusEffect>> status_effects_;
};

/// Resistant to slow and to splash/explosive damage.
class ResistantEnemy : public Enemy {
  float slow_resist_;
  float splash_resist_;

public:
  ResistantEnemy(
    std::string const& type,
    Rect bounds,
    float speed,
    int health,
    int tower_damage,
    float tower_damage_cooldown,
    float slow_resist,
    float splash_resist
  );

  float slow_resist_factor() const override;
  float splash_damage_factor() const override;
};

/// Spawns smaller enemies on death.
class SplitterEnemy : public Enemy {
public:
  struct ChildSpec {
    float speed;
    int health;
    int tower_damage;
    float tower_damage_cooldown;
    float width;
    float height;
    int count;
    int score;
    float perturbation;  // max random offset (in path-distance units) from parent
  };

private:
  ChildSpec child_spec_;

public:
  SplitterEnemy(
    std::string const& type,
    Rect bounds,
    float speed,
    int health,
    int tower_damage,
    float tower_damage_cooldown,
    ChildSpec const& child_spec
  );

  void on_death(Game& game) override;
};

/// Significantly stronger enemy with a damage-absorbing shield and regeneration.
class BossEnemy : public Enemy {
  int shield_;            // reduces incoming damage by `shield`
  float regen_amount_;    // amount of health to regenerate per second
  float regen_duration_;  // duration of the regeneration effect
  float regen_interval_;  // apply regen effect every `interval` seconds, 0.0 = no regeneration
  CountdownTimer regen_timer_;

public:
  BossEnemy(
    std::string const& type,
    Rect bounds,
    float speed,
    int health,
    int tower_damage,
    float tower_damage_cooldown,
    int shield,
    float regen_amount,
    float regen_duration,
    float regen_interval
  );

  int mitigate(int damage) const override;
  void update(Game& game, float dt) override;
};
