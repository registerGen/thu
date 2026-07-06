use rand::RngExt;
use serde::{Deserialize, Serialize};

use crate::{
    geometry::{Rect, Vec2},
    map::Map,
    path::Path,
    status_effect::{StatusEffect, StatusEffectKind},
    timer::CountdownTimer,
};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChildSpec {
    pub width: f32,
    pub height: f32,
    pub speed: f32,
    pub max_health: i32,
    pub score: i32,
    pub tower_damage: i32,
    pub tower_damage_interval: f32,
    pub count: i32,
    /// Max random offset (in path-distance units) from parent.
    pub perturbation: f32,
}

#[derive(Debug, Clone)]
pub enum EnemyKind {
    Normal,
    Fast,
    Armored,
    /// Resistant to slow and to splash damage.
    Resistant {
        slow_resist: f32,
        splash_resist: f32,
    },
    /// Spawns smaller enemies on death.
    Splitter {
        child_spec: ChildSpec,
    },
    /// Significantly stronger enemy with a damage-absorbing shield and regeneration.
    Boss {
        /// Reduces incoming damage by `shield`.
        shield: i32,
        /// Amount of health to regenerate per second.
        regen_amount: i32,
        /// Duration of the regeneration effect.
        regen_duration: f32,
        /// Apply regen effect every `interval` seconds, 0.0 = no regeneration.
        regen_interval: f32,
        regen_cooldown_timer: CountdownTimer,
    },
}

impl EnemyKind {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Normal => "normal",
            Self::Fast => "fast",
            Self::Armored => "armored",
            Self::Resistant { .. } => "resistant",
            Self::Splitter { .. } => "splitter",
            Self::Boss { .. } => "boss",
        }
    }
}

/// An enemy follows the map's path toward the exit and can attack towers and be
/// attacked by bullets.
#[derive(Debug, Clone)]
pub struct Enemy {
    pub bounds: Rect,
    pub speed: f32,
    pub health: i32,
    pub score: i32,
    pub max_health: i32,
    pub tower_damage: i32,
    pub tower_damage_interval: f32,
    pub path_index: usize,
    pub path_distance: f32,
    pub portal_cooldown_timer: CountdownTimer,
    pub status_effects: Vec<StatusEffect>,
    pub kind: EnemyKind,
}

slotmap::new_key_type! { pub struct EnemyId; }

/// Read-only hint of which status effects are currently active (for rendering).
#[derive(Debug, Clone, Default, Serialize)]
#[cfg_attr(feature = "web", derive(tsify::Tsify))]
pub struct StatusHint {
    pub slow: bool,
    pub poison: bool,
    pub regen: bool,
}

impl Enemy {
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        bounds: Rect,
        speed: f32,
        max_health: i32,
        score: i32,
        tower_damage: i32,
        tower_damage_interval: f32,
        path_index: usize,
        kind: EnemyKind,
    ) -> Self {
        Self {
            bounds,
            speed,
            health: max_health,
            score,
            max_health,
            tower_damage,
            tower_damage_interval,
            path_index,
            path_distance: 0.0,
            portal_cooldown_timer: CountdownTimer::new(0.0, false),
            status_effects: vec![],
            kind,
        }
    }

    pub fn position(&self) -> Vec2 {
        self.bounds.center
    }

    pub fn increase_health(&mut self, amount: i32) {
        self.health = (self.health + amount).min(self.max_health);
    }

    /// Reduce incoming bullet damage (e.g. shield).
    pub fn mitigate(&self, damage: i32) -> i32 {
        match self.kind {
            EnemyKind::Boss { shield, .. } => (damage - shield).max(0),
            _ => damage.max(0),
        }
    }

    pub fn decrease_health(&mut self, amount: i32) {
        self.health -= self.mitigate(amount);
    }

    pub fn is_destroyed(&self) -> bool {
        self.health <= 0
    }

    pub fn path<'a>(&self, paths: &'a [Path]) -> &'a Path {
        &paths[self.path_index]
    }

    /// Recompute bounds center from the current path distance.
    pub fn sync_position(&mut self, paths: &[Path]) {
        self.bounds.center = self.path(paths).position_at(self.path_distance);
    }

    /// True when the portal-teleport cooldown has elapsed (ready to teleport).
    pub fn portal_ready(&self) -> bool {
        self.portal_cooldown_timer.is_finished()
    }

    /// Arm the portal cooldown for `duration` seconds after a teleport. The cooldown
    /// mechanism prevents an enemy from moving back and forth between a portal pair.
    pub fn start_portal_cooldown(&mut self, duration: f32) {
        self.portal_cooldown_timer = CountdownTimer::new(duration, true);
    }

    /// Predicted position `time_ahead` seconds from now, following the path at
    /// current speed. Used by towers to lead moving targets.
    pub fn predicted_position(&self, time_ahead: f32, paths: &[Path]) -> Vec2 {
        self.path(paths)
            .position_at(self.path_distance + self.speed * time_ahead)
    }

    /// Resistance to slow effects (>=1.0; higher = less slowed).
    pub fn slow_resist_factor(&self) -> f32 {
        match self.kind {
            EnemyKind::Resistant { slow_resist, .. } => slow_resist,
            _ => 1.0,
        }
    }

    /// Multiplier applied to splash damage taken (<=1.0).
    pub fn splash_damage_factor(&self) -> f32 {
        match self.kind {
            EnemyKind::Resistant { splash_resist, .. } => splash_resist,
            _ => 1.0,
        }
    }

    pub fn on_death(
        &self,
        mut spawn_enemy: impl FnMut(Enemy),
        paths: &[Path],
        rng: &mut impl RngExt,
    ) {
        if let EnemyKind::Splitter { child_spec } = &self.kind {
            for _ in 0..child_spec.count {
                let mut child = Enemy::new(
                    Rect::new(self.bounds.center, child_spec.width, child_spec.height),
                    child_spec.speed,
                    child_spec.max_health,
                    child_spec.score,
                    child_spec.tower_damage,
                    child_spec.tower_damage_interval,
                    self.path_index,
                    EnemyKind::Normal,
                );

                let mut distance = self.path_distance;
                if child_spec.perturbation > 0.0 {
                    let offset =
                        rng.random_range(-child_spec.perturbation..=child_spec.perturbation);
                    // do not reach the exit
                    distance = (distance + offset)
                        .min(self.path(paths).total_length() - 0.01)
                        .max(0.0);
                }
                child.path_distance = distance;
                child.sync_position(paths);

                spawn_enemy(child);
            }
        }
    }

    pub fn apply_status_effect(&mut self, effect: StatusEffect) {
        effect.on_apply(self);
        self.status_effects.push(effect);
    }

    pub fn status_hint(&self) -> StatusHint {
        let mut hint = StatusHint::default();
        for effect in &self.status_effects {
            if !effect.expired() {
                match effect.kind {
                    StatusEffectKind::Slow { .. } => hint.slow = true,
                    StatusEffectKind::Poison { .. } => hint.poison = true,
                    StatusEffectKind::Regen { .. } => hint.regen = true,
                }
            }
        }
        hint
    }

    pub fn update(&mut self, dt: f32, paths: &[Path], map: &Map) {
        // Compute the effective speed multiplier from all active status effects and terrain.
        let slow_mult = self.status_effects.iter().fold(1.0, |acc, effect| {
            acc * if !effect.expired() {
                effect.speed_multiplier()
            } else {
                1.0
            }
        });

        // Update velocity factor and position based on terrain and status effects.
        let terrain_speed_factor = map
            .tile_at(self.position())
            .map_or(1.0, |tile| tile.enemy_speed_factor);
        let velocity_factor =
            terrain_speed_factor * (slow_mult * self.slow_resist_factor()).min(1.0);
        self.path_distance += self.speed * velocity_factor * dt;
        self.sync_position(paths);

        // Update all status effects and remove expired ones.
        let mut effects = std::mem::take(&mut self.status_effects);
        effects
            .iter_mut()
            .for_each(|effect| effect.update(self, dt));
        effects.retain(|effect| !effect.expired());

        // Update the portal cooldown timer.
        self.portal_cooldown_timer.update(dt);

        // Update boss health regeneration timer.
        if let EnemyKind::Boss {
            regen_amount,
            regen_duration,
            regen_interval,
            regen_cooldown_timer,
            ..
        } = &mut self.kind
            && *regen_interval > 0.0
            && regen_cooldown_timer.update(dt)
        {
            effects.push(StatusEffect::new(
                *regen_duration,
                StatusEffectKind::Regen {
                    regen_amount: *regen_amount,
                    accumulator: 0.0,
                },
            ));
            regen_cooldown_timer.reset();
        }

        self.status_effects = effects;
    }
}
