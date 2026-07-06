use crate::{enemy::Enemy, timer::CountdownTimer};

#[derive(Debug, Clone)]
pub enum StatusEffectKind {
    /// Reduces an enemy's movement speed for a duration.
    Slow { factor: f32 },
    /// Deals one-shot damage on application and roots the enemy for a duration.
    Poison { damage: i32 },
    /// Heals the enemy `regen_amount` per second for a duration.
    Regen { regen_amount: i32, accumulator: f32 },
}

/// A status effect applied to an enemy (e.g. slow, poison, regen).
/// Expires automatically when its timer runs out.
#[derive(Debug, Clone)]
pub struct StatusEffect {
    pub cooldown_timer: CountdownTimer,
    pub kind: StatusEffectKind,
}

impl StatusEffect {
    pub fn new(duration: f32, kind: StatusEffectKind) -> Self {
        Self {
            cooldown_timer: CountdownTimer::new(duration, true),
            kind,
        }
    }

    /// Called once when the effect is first applied to the enemy.
    pub fn on_apply(&self, enemy: &mut Enemy) {
        if let StatusEffectKind::Poison { damage, .. } = self.kind {
            enemy.decrease_health(damage);
        }
    }

    pub fn update(&mut self, enemy: &mut Enemy, dt: f32) {
        self.cooldown_timer.update(dt);

        if let StatusEffectKind::Regen {
            regen_amount,
            accumulator,
        } = &mut self.kind
        {
            *accumulator += *regen_amount as f32 * dt;
            let heal = *accumulator as i32;
            if heal > 0 {
                enemy.increase_health(heal);
                *accumulator -= heal as f32;
            }
        }
    }

    /// Whether the effect has run its course and should be removed.
    pub fn expired(&self) -> bool {
        self.cooldown_timer.is_finished()
    }

    /// Movement speed multiplier while active (<1 slows, 1 = no effect, 0 roots).
    pub fn speed_multiplier(&self) -> f32 {
        match self.kind {
            StatusEffectKind::Slow { factor } => factor,
            StatusEffectKind::Poison { .. } => 0.0,
            _ => 1.0,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_util::assert_approx_eq;
    use crate::{
        enemy::{Enemy, EnemyKind},
        geometry::{Rect, Vec2},
        map::Map,
        path::Path,
    };

    /// Straight horizontal path along y = 0; the enemy starts at the origin.
    fn straight_paths() -> Vec<Path> {
        vec![Path::new(
            vec![Vec2::new(0.0, 0.0), Vec2::new(10.0, 0.0)],
            vec![],
        )]
    }

    /// A normal enemy of `speed` / `max_health` centered at the path start.
    fn make_enemy(speed: f32, max_health: i32) -> Enemy {
        Enemy::new(
            Rect::new(Vec2::new(0.0, 0.0), 0.5, 0.5),
            speed,
            max_health,
            0,
            0,
            0.0,
            0,
            EnemyKind::Normal,
        )
    }

    /// An empty map: `tile_at` returns `None`, so terrain factors default to
    /// 1.0 — equivalent to the C++ tests' all-grass level-1 row 0.
    fn grass_map() -> Map {
        Map::new(0.0, 0.0, vec![])
    }

    #[test]
    fn slow_effect_reduces_movement_speed() {
        let paths = straight_paths();
        let map = grass_map();

        let mut slowed = make_enemy(2.0, 100);
        slowed.apply_status_effect(StatusEffect::new(
            10.0,
            StatusEffectKind::Slow { factor: 0.5 },
        ));
        slowed.update(1.0, &paths, &map);
        assert_approx_eq!(slowed.position().x, 1.0); // 2 * 1 * 0.5

        let mut unslowed = make_enemy(2.0, 100);
        unslowed.update(1.0, &paths, &map);
        assert_approx_eq!(unslowed.position().x, 2.0);
    }

    #[test]
    fn slow_effect_is_not_a_root_and_expires_after_its_duration() {
        let slow = StatusEffect::new(1.0, StatusEffectKind::Slow { factor: 0.5 });
        assert!(!slow.expired());

        let paths = straight_paths();
        let map = grass_map();
        let mut enemy = make_enemy(1.0, 100);
        enemy.apply_status_effect(StatusEffect::new(
            1.0,
            StatusEffectKind::Slow { factor: 0.5 },
        ));
        enemy.update(1.1, &paths, &map); // expire the slow
        let before = enemy.position().x;
        enemy.update(1.0, &paths, &map);
        assert_approx_eq!(enemy.position().x - before, 1.0); // full speed again
    }

    #[test]
    fn poison_effect_deals_one_shot_damage_on_application() {
        let paths = straight_paths();
        let map = grass_map();
        let mut enemy = make_enemy(1.0, 100);
        assert_eq!(enemy.health, 100);

        enemy.apply_status_effect(StatusEffect::new(
            2.0,
            StatusEffectKind::Poison { damage: 25 },
        ));
        assert_eq!(enemy.health, 75); // one-shot, immediate

        enemy.update(0.5, &paths, &map);
        assert_eq!(enemy.health, 75); // no further tick damage
    }

    #[test]
    fn poison_effect_roots_the_enemy_for_its_duration() {
        let paths = straight_paths();
        let map = grass_map();
        let mut enemy = make_enemy(2.0, 100);
        let start_x = enemy.position().x;

        enemy.apply_status_effect(StatusEffect::new(
            1.0,
            StatusEffectKind::Poison { damage: 0 }, // no damage, just root
        ));
        enemy.update(0.5, &paths, &map);
        assert_approx_eq!(enemy.position().x, start_x); // rooted

        // The effect expires *during* the next update (rooted check is at the
        // start), so the enemy is still held this frame...
        enemy.update(1.0, &paths, &map);
        assert_approx_eq!(enemy.position().x, start_x);

        // ...and only moves on the following update once the effect is gone.
        enemy.update(1.0, &paths, &map);
        assert!(enemy.position().x > start_x);
    }

    #[test]
    fn regeneration_effect_heals_over_time() {
        let paths = straight_paths();
        let map = grass_map();
        let mut enemy = make_enemy(1.0, 60);
        enemy.health = 40;
        assert_eq!(enemy.health, 40);

        enemy.apply_status_effect(StatusEffect::new(
            1.0,
            StatusEffectKind::Regen {
                regen_amount: 10,
                accumulator: 0.0,
            }, // 10 hp/s for 1s
        ));
        enemy.update(0.5, &paths, &map);
        assert_eq!(enemy.health, 45); // +5
        enemy.update(0.5, &paths, &map);
        assert_eq!(enemy.health, 50); // +5 more

        enemy.update(1.0, &paths, &map); // expired
        assert_eq!(enemy.health, 50);

        enemy.apply_status_effect(StatusEffect::new(
            1.0,
            StatusEffectKind::Regen {
                regen_amount: 20,
                accumulator: 0.0,
            }, // 20 hp/s for 1s
        ));
        enemy.update(0.5, &paths, &map);
        assert_eq!(enemy.health, 60); // +10
        enemy.update(0.5, &paths, &map);
        assert_eq!(enemy.health, 60); // cannot exceed max health
    }

    #[test]
    fn multiple_slow_effects_stack_multiplicatively() {
        let paths = straight_paths();
        let map = grass_map();
        let mut enemy = make_enemy(2.0, 100);
        enemy.apply_status_effect(StatusEffect::new(
            10.0,
            StatusEffectKind::Slow { factor: 0.5 },
        ));
        enemy.apply_status_effect(StatusEffect::new(
            10.0,
            StatusEffectKind::Slow { factor: 0.5 },
        ));
        enemy.update(1.0, &paths, &map);
        assert_approx_eq!(enemy.position().x, 0.5); // 2 * 1 * 0.5 * 0.5
    }
}
