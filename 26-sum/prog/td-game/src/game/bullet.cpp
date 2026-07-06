#include "bullet.h"

#include <algorithm>
#include <cmath>

Bullet::Bullet(Vec2 position, Vec2 velocity) : position_(position), velocity_(velocity) { }
Bullet::~Bullet() = default;

Vec2 Bullet::position() const { return position_; }
Vec2 Bullet::velocity() const { return velocity_; }
void Bullet::update(float dt) { position_ += velocity_ * dt; }

bool Bullet::can_explode(std::vector<std::shared_ptr<Enemy>> const& enemies) const {
  for (auto const& enemy : enemies) {
    if (enemy->is_destroyed()) continue;
    if (enemy->bounds().contains(position_)) return true;
  }
  return false;
}

bool Bullet::effective(Enemy const& enemy) const { return enemy.bounds().contains(position_); }
bool Bullet::pierces() const { return false; }

NormalBullet::NormalBullet(Vec2 position, Vec2 velocity, int health_damage)
    : Bullet(position, velocity), health_damage_(health_damage) { }

void NormalBullet::effect(Enemy& enemy) const { enemy.decrease_health(health_damage_); }

SlowBullet::SlowBullet(Vec2 position, Vec2 velocity, SlowEffect const& slow_effect, Map const& map)
    : Bullet(position, velocity), slow_effect_(slow_effect), map_(map) { }

void SlowBullet::effect(Enemy& enemy) const {
  Tile const* tile = map_.tile_at(enemy.position());
  if (!tile) return;

  SlowEffect adjusted = slow_effect_;
  adjusted.scale_factor(tile->slow_bullet_factor());
  enemy.apply_status_effect(std::make_unique<SlowEffect>(adjusted));
}

PoisonBullet::PoisonBullet(Vec2 position, Vec2 velocity, PoisonEffect const& poison_effect)
    : Bullet(position, velocity), poison_effect_(poison_effect) { }

void PoisonBullet::effect(Enemy& enemy) const {
  enemy.apply_status_effect(std::make_unique<PoisonEffect>(poison_effect_));
}

ExplosiveBullet::ExplosiveBullet(Vec2 position, Vec2 velocity, float radius, int health_damage)
    : Bullet(position, velocity), radius_(radius), health_damage_(health_damage) { }

float ExplosiveBullet::radius() const { return radius_; }

bool ExplosiveBullet::effective(Enemy const& enemy) const {
  return enemy.position().distance(position()) <= radius_;
}

void ExplosiveBullet::effect(Enemy& enemy) const {
  enemy.decrease_health(
    std::max(1, static_cast<int>(static_cast<float>(health_damage_) * enemy.splash_damage_factor()))
  );
}

bool ExplosiveBullet::pierces() const { return true; }

LaserBullet::LaserBullet(Vec2 position, Vec2 velocity, float width, int health_damage)
    : Bullet(position, velocity), width_(width), health_damage_(health_damage) { }

bool LaserBullet::effective(Enemy const& enemy) const {
  Vec2 enemy_center = enemy.position();
  Vec2 bullet_direction = velocity().normalized();
  if (bullet_direction.length_sq() == 0.0f) return false;

  Vec2 bullet_to_enemy = enemy_center - position();
  // Check if the enemy is in front of the bullet (dot product > 0)
  if (bullet_to_enemy.dot(bullet_direction) < 0.0f) return false;

  float distance_to_line = std::abs((enemy_center - position()).cross(bullet_direction));
  return distance_to_line <= width_ / 2.0f;
}

void LaserBullet::effect(Enemy& enemy) const { enemy.decrease_health(health_damage_); }
bool LaserBullet::pierces() const { return true; }
