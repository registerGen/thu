#pragma once

#include <memory>
#include <vector>

#include "enemy.h"
#include "geometry.h"
#include "map.h"
#include "status_effect.h"

/// A bullet is a projectile that can be fired by a tower to attack enemies.
class Bullet {
  Vec2 position_;
  Vec2 velocity_;

public:
  Bullet(Vec2 position, Vec2 velocity);
  virtual ~Bullet();

  Vec2 position() const;
  Vec2 velocity() const;
  void update(float dt);

  /// Whether the bullet should activate this frame. Default: the bullet's
  /// position is inside some enemy's bounds (physical contact).
  virtual bool can_explode(std::vector<std::shared_ptr<Enemy>> const& enemies) const;
  /// Whether the bullet can attack the enemy. Default: the bullet's point is
  /// inside the enemy's bounds. Explosive/Laser override with radius/ray tests.
  virtual bool effective(Enemy const& enemy) const;
  /// Apply the bullet's effect to the enemy.
  virtual void effect(Enemy& enemy) const = 0;
  /// Whether the bullet hits multiple enemies in a single frame (e.g. splash,
  /// laser). Single-target bullets return false so the collision loop can stop
  /// after the first hit.
  virtual bool pierces() const;
};

/// Normal bullet decreases health of a enemy.
class NormalBullet : public Bullet {
  int health_damage_;

public:
  NormalBullet(Vec2 position, Vec2 velocity, int health_damage);

  void effect(Enemy& enemy) const override;
};

/// Slow bullet applies a slow effect to a enemy.
class SlowBullet : public Bullet {
  SlowEffect slow_effect_;
  Map const& map_;

public:
  SlowBullet(Vec2 position, Vec2 velocity, SlowEffect const& slow_effect, Map const& map);

  void effect(Enemy& enemy) const override;
};

/// Poison bullet applies a poison effect to a enemy.
class PoisonBullet : public Bullet {
  PoisonEffect poison_effect_;

public:
  PoisonBullet(Vec2 position, Vec2 velocity, PoisonEffect const& poison_effect);

  void effect(Enemy& enemy) const override;
};

/// Explosive bullet can attack multiple enemies in a radius.
class ExplosiveBullet : public Bullet {
  float radius_;
  int health_damage_;

public:
  ExplosiveBullet(Vec2 position, Vec2 velocity, float radius, int health_damage);

  float radius() const;
  bool effective(Enemy const& enemy) const override;
  void effect(Enemy& enemy) const override;
  bool pierces() const override;
};

/// Laser bullet can attack multiple enemies in a ray.
class LaserBullet : public Bullet {
  float width_;
  int health_damage_;

public:
  LaserBullet(Vec2 position, Vec2 velocity, float width, int health_damage);

  bool effective(Enemy const& enemy) const override;
  void effect(Enemy& enemy) const override;
  bool pierces() const override;
};
