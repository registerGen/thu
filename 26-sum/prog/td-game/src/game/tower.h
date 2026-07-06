#pragma once

#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "bullet.h"
#include "enemy.h"
#include "status_effect.h"
#include "tile.h"
#include "timer.h"

class Game;

/// A tower is a defensive structure that can be placed on a tile.
class Tower {
  /// Damage to the tower applied by `enemy`: HP reduced by `damage` per
  /// `duration` seconds until the enemy dies.
  struct TowerDamage : public CountdownTimer {
    int damage;
    std::weak_ptr<Enemy> enemy;  // to track whether the enemy is still alive
    TowerDamage(int damage, float duration, std::shared_ptr<Enemy> enemy);
  };

  std::string type_;
  Tile const* tile_;
  int health_;
  int max_health_;
  int resource_cost_;
  std::vector<TowerDamage> tower_damage_;

public:
  Tower(std::string const& type, Tile const& tile, int health, int resource_cost);
  virtual ~Tower();

  Tile const* tile() const;
  std::string const& type() const;
  int health() const;
  int max_health() const;
  void set_health(int health);
  void increase_health(int amount);
  void decrease_health(int amount);
  bool is_destroyed() const;
  int resource_cost() const;
  void apply_tower_damage(std::shared_ptr<Enemy> enemy);

  /// Whether this tower may be placed on the given tile. Default: placeable
  /// terrain and not occupied.
  virtual bool can_place_on(Tile const& tile) const;

  /// Per-tick update with game context. The base implementation ticks ongoing
  /// tower damage applied by enemies in contact. Subclasses override to attack
  /// or generate resources.
  virtual void update(Game& game, float dt);
};

/// Tower that can release bullets to attack enemies.
class AttackTower : public Tower {
public:
  /// How an attack tower picks which enemy to shoot.
  enum class Targeting {
    First,      // enemy furthest along the path (nearest the exit)
    Closest,    // nearest by Euclidean distance
    Strongest,  // highest current HP
  };

  /// Builds a bullet fired from `source` toward `target`. Constructed by
  /// make_tower from the per-type stats (capturing the rng/map it needs), so
  /// AttackTower itself stays type-agnostic — no per-bullet subclasses.
  using BulletFactory = std::function<std::unique_ptr<Bullet>(Vec2 source, Vec2 target)>;

private:
  bool can_attack() const;
  void reset_attack_cooldown();
  std::unique_ptr<Bullet> release_bullet(Vec2 target_pos);
  float bullet_speed() const;

  CountdownTimer attack_cooldown_;
  float range_;  // attack range of the tower
  Targeting targeting_;
  BulletFactory make_bullet_;
  float bullet_speed_;

public:
  AttackTower(
    std::string const& type,
    Tile const& tile,
    int health,
    int resource_cost,
    float attack_cooldown,
    float range,
    Targeting targeting,
    BulletFactory make_bullet,
    float bullet_speed
  );

  /// Direction the tower is currently aiming (unit vector); for rendering a muzzle.
  Vec2 aim() const;
  void update(Game& game, float dt) override;

protected:
  /// Pick a target enemy within range, or nullptr, based on the targeting policy.
  Enemy* select_target(std::vector<std::shared_ptr<Enemy>> const& enemies) const;

  Vec2 aim_{1.0f, 0.0f};
};

struct BulletSpecBase {
  float min_speed;
  float max_speed;
  float max_angle_deviation;

  /// Velocity aimed at `target` from `source`, with random speed and spread.
  Vec2 aimed_velocity(std::mt19937& rng, Vec2 source, Vec2 target) const;
};

/// Tower that can generate resources for the player.
class ResourceTower : public Tower {
  int resource_amount_;
  CountdownTimer resource_cooldown_;

  bool can_generate_resource() const;
  void reset_resource_cooldown();

public:
  ResourceTower(
    std::string const& type,
    Tile const& tile,
    int health,
    int resource_cost,
    int resource_amount,
    float resource_cooldown
  );

  int resource_amount() const;
  void update(Game& game, float dt) override;
};

/// Wall tower: high HP, no attack.
class WallTower : public Tower {
public:
  using Tower::Tower;
};
