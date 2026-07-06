#include "factory.h"

#include <stdexcept>

std::unique_ptr<Tower> make_tower(
  std::string const& type,
  Tile const& tile,
  TowerStats const& s,
  std::mt19937& rng,
  Map const& map
) {
  // Shared bullet kinematics (speed range + spread) for every attack type.
  BulletSpecBase base{s.min_speed, s.max_speed, s.max_angle_deviation};

  // Each attack type builds a bullet from its per-type stats. The factory
  // captures only the fields it needs (and the rng/map refs, which outlive the
  // tower within the level), so AttackTower itself is type-agnostic.
  auto attack = [&](AttackTower::BulletFactory fire) {
    return std::make_unique<AttackTower>(
      type,
      tile,
      s.health,
      s.cost,
      s.attack_cooldown,
      s.range,
      s.targeting,
      std::move(fire),
      s.max_speed
    );
  };

  if (type == "normal") {
    return attack([base, dmg = s.health_damage, &rng](Vec2 src, Vec2 tgt) {
      return std::make_unique<NormalBullet>(src, base.aimed_velocity(rng, src, tgt), dmg);
    });
  }
  if (type == "slow") {
    return attack(
      [base, slow = SlowEffect{s.slow_factor, s.slow_duration}, &rng, &map](Vec2 src, Vec2 tgt) {
        return std::make_unique<SlowBullet>(src, base.aimed_velocity(rng, src, tgt), slow, map);
      }
    );
  }
  if (type == "poison") {
    return attack(
      [base, poison = PoisonEffect{s.poison_damage, s.poison_duration}, &rng](Vec2 src, Vec2 tgt) {
        return std::make_unique<PoisonBullet>(src, base.aimed_velocity(rng, src, tgt), poison);
      }
    );
  }
  if (type == "splash") {
    return attack([base, radius = s.radius, dmg = s.health_damage, &rng](Vec2 src, Vec2 tgt) {
      return std::make_unique<ExplosiveBullet>(
        src,
        base.aimed_velocity(rng, src, tgt),
        radius,
        dmg
      );
    });
  }
  if (type == "laser") {
    return attack([base, width = s.width, dmg = s.health_damage, &rng](Vec2 src, Vec2 tgt) {
      return std::make_unique<LaserBullet>(src, base.aimed_velocity(rng, src, tgt), width, dmg);
    });
  }
  if (type == "resource") {
    return std::make_unique<ResourceTower>(
      type,
      tile,
      s.health,
      s.cost,
      s.resource_amount,
      s.resource_cooldown
    );
  }
  if (type == "wall") {
    return std::make_unique<WallTower>(type, tile, s.health, s.cost);
  }
  throw std::runtime_error("unknown tower type: " + type);
}

std::shared_ptr<Enemy> make_enemy(std::string const& type, EnemyStats const& s, Path const* path) {
  Vec2 start = path ? path->position_at(0.0f) : Vec2{0.0f, 0.0f};
  Rect bounds(start, s.size, s.size);

  std::shared_ptr<Enemy> enemy;
  if (type == "normal" || type == "fast" || type == "armored") {
    // Standard enemies differ only by stats (data), not behavior — one class.
    enemy = std::make_shared<Enemy>(
      type,
      bounds,
      s.speed,
      s.health,
      s.tower_damage,
      s.tower_damage_cooldown
    );
  } else if (type == "resistant") {
    enemy = std::make_shared<ResistantEnemy>(
      type,
      bounds,
      s.speed,
      s.health,
      s.tower_damage,
      s.tower_damage_cooldown,
      s.slow_resist,
      s.splash_resist
    );
  } else if (type == "splitter") {
    SplitterEnemy::ChildSpec child{
      s.child_speed,
      s.child_health,
      s.child_tower_damage,
      s.child_tower_damage_cooldown,
      s.child_width,
      s.child_height,
      s.child_count,
      s.child_score,
      s.child_perturbation
    };
    enemy = std::make_shared<SplitterEnemy>(
      type,
      bounds,
      s.speed,
      s.health,
      s.tower_damage,
      s.tower_damage_cooldown,
      std::move(child)
    );
  } else if (type == "boss") {
    enemy = std::make_shared<BossEnemy>(
      type,
      bounds,
      s.speed,
      s.health,
      s.tower_damage,
      s.tower_damage_cooldown,
      s.shield,
      s.regen_amount,
      s.regen_duration,
      s.regen_interval
    );
  } else {
    throw std::runtime_error("unknown enemy type: " + type);
  }

  enemy->set_path(path);
  enemy->set_path_distance(0.0f);
  enemy->set_score(s.score);
  return enemy;
}
