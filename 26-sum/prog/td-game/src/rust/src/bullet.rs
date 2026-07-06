use crate::{
    enemy::{Enemy, EnemyId},
    geometry::Vec2,
    map::Map,
    status_effect::{StatusEffect, StatusEffectKind},
};

#[derive(Debug, Clone)]
pub enum BulletKind {
    /// Normal bullet decreases health of a enemy.
    Normal { health_damage: i32 },
    /// Slow bullet applies a slow effect to a enemy.
    Slow { factor: f32, duration: f32 },
    /// Poison bullet applies a poison effect to a enemy.
    Poison { damage: i32, duration: f32 },
    /// Splash bullet can attack multiple enemies in a radius.
    Splash { radius: f32, health_damage: i32 },
    /// Laser bullet can attack multiple enemies in a ray.
    Laser { width: f32, health_damage: i32 },
}

impl BulletKind {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Normal { .. } => "normal",
            Self::Slow { .. } => "slow",
            Self::Poison { .. } => "poison",
            Self::Splash { .. } => "splash",
            Self::Laser { .. } => "laser",
        }
    }
}

/// A bullet is a projectile that can be fired by a tower to attack enemies.
#[derive(Debug, Clone)]
pub struct Bullet {
    pub position: Vec2,
    pub velocity: Vec2,
    pub kind: BulletKind,
}

slotmap::new_key_type! { pub struct BulletId; }

impl Bullet {
    pub fn new(position: Vec2, velocity: Vec2, kind: BulletKind) -> Self {
        Self {
            position,
            velocity,
            kind,
        }
    }
    pub fn update(&mut self, dt: f32) {
        self.position += self.velocity * dt;
    }

    /// Whether the bullet should activate this frame. Default: the bullet's
    /// position is inside some enemy's bounds (physical contact).
    pub fn can_explode<'a>(&self, mut enemies: impl Iterator<Item = (EnemyId, &'a Enemy)>) -> bool {
        enemies.any(|(_, enemy)| !enemy.is_destroyed() && enemy.bounds.contains(self.position))
    }

    /// Whether the bullet can attack the enemy. Default: the bullet's point is
    /// inside the enemy's bounds. Splash/Laser override with radius/ray tests.
    pub fn effective(&self, enemy: &Enemy) -> bool {
        match self.kind {
            BulletKind::Splash { radius, .. } => enemy.position().distance(self.position) <= radius,
            BulletKind::Laser { width, .. } => {
                let bullet_direction = self.velocity.normalized();
                if bullet_direction.length_sq() == 0.0 {
                    return false;
                }

                let bullet_to_enemy = enemy.position() - self.position;
                // Check if the enemy is in front of the bullet (dot product >= 0)
                if bullet_to_enemy.dot(bullet_direction) < 0.0 {
                    return false;
                }

                let distance_to_line = bullet_to_enemy.cross(bullet_direction).abs();
                distance_to_line <= width / 2.0
            }
            _ => enemy.bounds.contains(self.position),
        }
    }

    /// Apply the bullet's effect to the enemy.
    pub fn impact(&self, enemy: &mut Enemy, map: &Map) {
        match self.kind {
            BulletKind::Normal { health_damage } | BulletKind::Laser { health_damage, .. } => {
                enemy.decrease_health(health_damage)
            }

            BulletKind::Splash { health_damage, .. } => {
                let scaled = (health_damage as f32 * enemy.splash_damage_factor()) as i32;
                enemy.decrease_health(scaled.max(1));
            }

            BulletKind::Slow { factor, duration } => {
                if let Some(tile) = map.tile_at(enemy.position()) {
                    enemy.apply_status_effect(StatusEffect::new(
                        duration,
                        StatusEffectKind::Slow {
                            factor: factor * tile.slow_bullet_factor,
                        },
                    ));
                }
            }

            BulletKind::Poison { damage, duration } => {
                enemy.apply_status_effect(StatusEffect::new(
                    duration,
                    StatusEffectKind::Poison { damage },
                ));
            }
        }
    }

    /// Whether the bullet hits multiple enemies in a single frame (e.g. splash,
    /// laser). Single-target bullets return false so the collision loop can stop
    /// after the first hit.
    pub fn pierces(&self) -> bool {
        matches!(
            self.kind,
            BulletKind::Splash { .. } | BulletKind::Laser { .. }
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{enemy::EnemyKind, geometry::Rect};

    /// A normal enemy centered at `pos` (0.5x0.5 bounds) — enough to drive the
    /// `effective` geometry checks; the other stats are irrelevant.
    fn enemy_at(pos: Vec2) -> Enemy {
        Enemy::new(
            Rect::new(pos, 0.5, 0.5),
            1.0,
            50,
            0,
            0,
            0.0,
            0,
            EnemyKind::Normal,
        )
    }

    #[test]
    fn splash_only_hits_enemies_within_its_radius() {
        let splash = Bullet::new(
            Vec2::new(0.0, 0.0),
            Vec2::new(1.0, 0.0),
            BulletKind::Splash {
                radius: 2.5,
                health_damage: 15,
            },
        );

        // Enemy within radius -> hit.
        assert!(splash.effective(&enemy_at(Vec2::new(2.0, 0.0))));
        // Enemy beyond radius -> not hit.
        assert!(!splash.effective(&enemy_at(Vec2::new(3.0, 0.0))));
    }

    #[test]
    fn laser_only_hits_enemies_in_its_forward_direction() {
        // Bullet at the origin moving right (+x); the beam extends forward only.
        let laser = Bullet::new(
            Vec2::new(0.0, 0.0),
            Vec2::new(1.0, 0.0),
            BulletKind::Laser {
                width: 1.0,
                health_damage: 10,
            },
        );

        // Enemy in front on the beam -> hit.
        assert!(laser.effective(&enemy_at(Vec2::new(5.0, 0.0))));
        // Enemy behind the bullet -> not hit.
        assert!(!laser.effective(&enemy_at(Vec2::new(-5.0, 0.0))));
    }
}
