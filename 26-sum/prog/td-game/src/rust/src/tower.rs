use std::collections::HashMap;

use rand::RngExt;
use serde::{Deserialize, Serialize};

use crate::{
    bullet::{Bullet, BulletKind},
    enemy::{Enemy, EnemyId},
    geometry::Vec2,
    map::Map,
    path::Path,
    resource::Resource,
    tile::Tile,
    timer::CountdownTimer,
};

/// Damage to the tower applied by `enemy`: HP reduced by `damage` per
/// `interval` (stored in the timer) seconds until the enemy dies.
#[derive(Debug, Clone)]
pub struct TowerDamage {
    damage: i32,
    cooldown_timer: CountdownTimer,
}

impl TowerDamage {
    pub fn new(damage: i32, interval: f32) -> Self {
        Self {
            damage,
            cooldown_timer: CountdownTimer::new(interval, true),
        }
    }
}

/// How an attack tower picks which enemy to shoot.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Targeting {
    /// Enemy furthest along the path (nearest the exit).
    First,
    /// Nearest by Euclidean distance.
    Closest,
    /// Highest current HP.
    Strongest,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BulletSpec {
    pub min_speed: f32,
    pub max_speed: f32,
    pub max_angle_deviation: f32,
}

impl BulletSpec {
    #[allow(dead_code)]
    pub fn new(min_speed: f32, max_speed: f32, max_angle_deviation: f32) -> Self {
        Self {
            min_speed,
            max_speed,
            max_angle_deviation,
        }
    }

    /// Velocity aimed at `target` from `source`, with random speed and spread.
    pub fn aimed_velocity(&self, source: Vec2, target: Vec2, rng: &mut impl RngExt) -> Vec2 {
        let mut direction = (target - source).normalized();
        if direction.length_sq() == 0.0 {
            direction = Vec2::new(1.0, 0.0);
        }
        let speed = rng.random_range(self.min_speed..=self.max_speed);
        let angle = rng.random_range(-self.max_angle_deviation..=self.max_angle_deviation);

        direction.rotated(angle) * speed
    }
}

#[derive(Debug, Clone)]
pub enum TowerKind {
    /// Tower that can release bullets to attack enemies.
    Attack {
        attack_range: f32,
        targeting: Targeting,
        bullet_kind: BulletKind,
        bullet_spec: BulletSpec,
        aim: Vec2,
        attack_cooldown_timer: CountdownTimer,
    },
    Resource {
        resource_inc_amount: i32,
        resource_inc_cooldown_timer: CountdownTimer,
    },
    Wall,
}

impl TowerKind {
    pub fn as_str(&self) -> &'static str {
        match self {
            TowerKind::Attack { bullet_kind, .. } => bullet_kind.as_str(),
            TowerKind::Resource { .. } => "resource",
            TowerKind::Wall => "wall",
        }
    }
}

/// A tower is a defensive structure that can be placed on a tile.
#[derive(Debug, Clone)]
pub struct Tower {
    pub tile_index: usize,
    pub health: i32,
    pub max_health: i32,
    pub resource_cost: i32,
    pub tower_damages: HashMap<EnemyId, TowerDamage>,
    pub kind: TowerKind,
}

slotmap::new_key_type! { pub struct TowerId; }

impl Tower {
    pub fn new(tile_index: usize, max_health: i32, resource_cost: i32, kind: TowerKind) -> Self {
        Self {
            tile_index,
            health: max_health,
            max_health,
            resource_cost,
            tower_damages: HashMap::new(),
            kind,
        }
    }

    pub fn tile<'a>(&self, map: &'a Map) -> &'a Tile {
        &map.tiles[self.tile_index]
    }

    #[allow(dead_code)]
    pub fn increase_health(&mut self, amount: i32) {
        self.health = (self.health + amount).min(self.max_health);
    }
    pub fn is_destroyed(&self) -> bool {
        self.health <= 0
    }

    pub fn effective_resource_cost(&self, map: &Map) -> i32 {
        ((self.resource_cost as f32 * self.tile(map).resource_cost_factor) as i32).max(1)
    }

    pub fn apply_tower_damage(&mut self, enemy_id: EnemyId, damage: i32, interval: f32) {
        // A tower should not be attacked by the same enemy twice.
        self.tower_damages
            .entry(enemy_id)
            .or_insert(TowerDamage::new(damage, interval));
    }

    #[allow(clippy::too_many_arguments)]
    pub fn update<'a>(
        &mut self,
        dt: f32,
        map: &Map,
        paths: &[Path],
        enemies: impl Iterator<Item = (EnemyId, &'a Enemy)>,
        mut spawn_bullet: impl FnMut(Bullet),
        resource: &mut Resource,
        is_alive: impl Fn(EnemyId) -> bool,
        rng: &mut impl RngExt,
    ) {
        self.tower_damages.retain(|id, damage| {
            if !is_alive(*id) {
                return false;
            }

            if damage.cooldown_timer.update(dt) {
                self.health -= damage.damage;
                damage.cooldown_timer.reset();
            }
            true
        });

        let source = self.tile(map).position();
        if let TowerKind::Attack {
            attack_range,
            targeting,
            bullet_kind,
            bullet_spec,
            aim,
            attack_cooldown_timer,
        } = &mut self.kind
            && attack_cooldown_timer.update(dt)
            && let Some(target) = select_target(source, *attack_range, *targeting, enemies)
        {
            // Lead the target: aim where it will be when the bullet arrives.
            // Refine once: the bullet travels to the predicted point, whose distance
            // differs from the current distance, so recompute the lead from that.
            let speed = (bullet_spec.min_speed + bullet_spec.max_speed) / 2.0;
            let mut lead = if speed > 0.0 {
                (target.position() - source).length() / speed
            } else {
                0.0
            };
            let mut predicted = target.predicted_position(lead, paths);
            if speed > 0.0 {
                lead = (predicted - source).length() / speed;
                predicted = target.predicted_position(lead, paths);
            }
            *aim = (predicted - source).normalized();
            if aim.length_sq() == 0.0 {
                *aim = Vec2::new(1.0, 0.0);
            }

            spawn_bullet(release_bullet(
                source,
                predicted,
                bullet_kind,
                bullet_spec,
                rng,
            ));
            attack_cooldown_timer.reset();
        }

        if let TowerKind::Resource {
            resource_inc_amount,
            resource_inc_cooldown_timer,
        } = &mut self.kind
            && resource_inc_cooldown_timer.update(dt)
        {
            resource.increase(*resource_inc_amount);
            resource_inc_cooldown_timer.reset();
        }
    }
}

/// Pick a target enemy within range, or None, based on the targeting policy.
pub fn select_target<'a>(
    source: Vec2,
    attack_range: f32,
    targeting: Targeting,
    enemies: impl Iterator<Item = (EnemyId, &'a Enemy)>,
) -> Option<&'a Enemy> {
    Some(
        enemies
            .filter(|(_, enemy)| {
                !enemy.is_destroyed() && enemy.position().distance(source) <= attack_range
            })
            .max_by(|(_, lhs), (_, rhs)| match targeting {
                Targeting::First => lhs.path_distance.total_cmp(&rhs.path_distance),
                Targeting::Closest => rhs
                    .position()
                    .distance(source)
                    .total_cmp(&lhs.position().distance(source)),
                Targeting::Strongest => lhs.health.cmp(&rhs.health),
            })?
            .1,
    )
}

fn release_bullet(
    source: Vec2,
    target: Vec2,
    kind: &BulletKind,
    spec: &BulletSpec,
    rng: &mut impl RngExt,
) -> Bullet {
    Bullet::new(
        source,
        spec.aimed_velocity(source, target, rng),
        kind.clone(),
    )
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_util::assert_approx_eq;
    use crate::{
        enemy::{Enemy, EnemyId, EnemyKind},
        geometry::{Rect, Vec2},
    };
    use rand::{SeedableRng, rngs::StdRng};
    use slotmap::SlotMap;

    #[test]
    fn aimed_velocity_points_left() {
        let cfg = BulletSpec::new(5.0, 5.0, 0.0); // fixed speed, no spread
        let mut rng = StdRng::seed_from_u64(42);
        let src = Vec2::new(4.0, 2.0);
        let v = cfg.aimed_velocity(src, Vec2::new(2.0, 2.0), &mut rng);
        assert!(v.x < 0.0);
        assert_approx_eq!(v.y, 0.0);
        assert_approx_eq!(v.length(), 5.0);
    }

    #[test]
    fn aimed_velocity_points_right() {
        let cfg = BulletSpec::new(5.0, 5.0, 0.0);
        let mut rng = StdRng::seed_from_u64(42);
        let v = cfg.aimed_velocity(Vec2::new(4.0, 2.0), Vec2::new(6.0, 2.0), &mut rng);
        assert!(v.x > 0.0);
        assert_approx_eq!(v.y, 0.0);
    }

    #[test]
    fn aimed_velocity_points_up() {
        let cfg = BulletSpec::new(5.0, 5.0, 0.0);
        let mut rng = StdRng::seed_from_u64(42);
        let v = cfg.aimed_velocity(Vec2::new(4.0, 2.0), Vec2::new(4.0, 0.0), &mut rng);
        assert!(v.y < 0.0);
        assert_approx_eq!(v.x, 0.0);
    }

    #[test]
    fn aimed_velocity_diagonal_normalizes_direction() {
        let cfg = BulletSpec::new(5.0, 5.0, 0.0);
        let mut rng = StdRng::seed_from_u64(42);
        // direction (3,4) normalized * speed 5 = (3,4)
        let v = cfg.aimed_velocity(Vec2::new(0.0, 0.0), Vec2::new(3.0, 4.0), &mut rng);
        assert_approx_eq!(v.x, 3.0);
        assert_approx_eq!(v.y, 4.0);
    }

    #[test]
    fn aimed_velocity_applies_random_speed_within_range() {
        let cfg = BulletSpec::new(3.0, 7.0, 0.0);
        let mut rng = StdRng::seed_from_u64(7);
        for _ in 0..50 {
            let v = cfg.aimed_velocity(Vec2::new(0.0, 0.0), Vec2::new(1.0, 0.0), &mut rng);
            let speed = v.length();
            assert!(speed >= 3.0);
            assert!(speed <= 7.0);
            assert!(v.x > 0.0); // still aimed right
        }
    }

    #[test]
    fn aimed_velocity_applies_angular_spread() {
        let cfg = BulletSpec::new(5.0, 5.0, 0.5); // up to ~0.5 rad spread
        let mut rng = StdRng::seed_from_u64(99);
        let src = Vec2::new(0.0, 0.0);
        let target = Vec2::new(10.0, 0.0);

        let mut saw_nonzero_y = false;
        for _ in 0..100 {
            let v = cfg.aimed_velocity(src, target, &mut rng);
            assert!(v.x > 0.0); // still generally rightward
            if v.y.abs() > 1e-4 {
                saw_nonzero_y = true;
            }
        }
        assert!(saw_nonzero_y); // spread produced some off-axis shots
    }

    #[test]
    fn aimed_velocity_handles_zero_target_offset() {
        let cfg = BulletSpec::new(5.0, 5.0, 0.0);
        let mut rng = StdRng::seed_from_u64(1);
        // Falls back to rightward rather than a zero/NaN velocity.
        let v = cfg.aimed_velocity(Vec2::new(0.0, 0.0), Vec2::new(0.0, 0.0), &mut rng);
        assert_approx_eq!(v.length(), 5.0);
        assert!(v.x.is_finite());
        assert!(v.y.is_finite());
    }

    /// Enemy centered at `pos` with given `health` (0.5x0.5 bounds).
    fn enemy_at(pos: Vec2, health: i32) -> Enemy {
        Enemy::new(
            Rect::new(pos, 0.5, 0.5),
            1.0,
            health,
            0,
            0,
            0.0,
            0,
            EnemyKind::Normal,
        )
    }

    #[test]
    fn strongest_targeting_picks_highest_hp_enemy_in_range() {
        let mut enemies: SlotMap<EnemyId, Enemy> = SlotMap::default();
        let strong = enemies.insert(enemy_at(Vec2::new(1.5, 0.5), 50));
        let weak = enemies.insert(enemy_at(Vec2::new(2.5, 0.5), 10));
        let _ = (strong, weak);

        let source = Vec2::new(0.5, 0.5);
        let target = select_target(source, 5.0, Targeting::Strongest, enemies.iter());
        let target = target.expect("a target in range");
        assert_eq!(target.health, 50); // picked the strongest
    }
}
