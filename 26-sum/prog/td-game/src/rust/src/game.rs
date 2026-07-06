use std::collections::HashSet;

use rand::{
    SeedableRng,
    rngs::{StdRng, SysRng},
};
use serde::Serialize;
use slotmap::SlotMap;

use crate::{
    bullet::{Bullet, BulletId},
    config::make_tower,
    enemy::{Enemy, EnemyId},
    geometry::Vec2,
    level::Level,
    resource::Resource,
    tower::{Tower, TowerId},
    wave::WaveSpec,
};

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Serialize)]
#[cfg_attr(feature = "web", derive(tsify::Tsify), tsify(into_wasm_abi))]
#[serde(rename_all = "lowercase")]
pub enum GameState {
    #[default]
    Playing,
    Victory,
    Defeat,
}

#[derive(Debug, Clone, Copy, Default, Serialize)]
#[cfg_attr(feature = "web", derive(tsify::Tsify), tsify(into_wasm_abi))]
pub struct GameResult {
    pub cleared: bool,
    pub cheated: bool,
    pub time: f32,
    pub score: i32,
}

#[derive(Debug, Clone, Serialize)]
#[cfg_attr(feature = "web", derive(tsify::Tsify), tsify(into_wasm_abi))]
#[serde(tag = "type", rename_all = "snake_case")]
#[cfg_attr(feature = "web", serde(rename_all_fields = "camelCase"))]
pub enum GameEvent {
    TowerPlaced {
        kind: String,
        pos: Vec2,
        cost: i32,
    },
    EnemyKilled {
        kind: String,
        pos: Vec2,
        score: i32,
    },
    WaveStarted {
        index: i32,
        has_boss: bool,
        is_last: bool,
    },
}

#[derive(Debug, Clone, Copy, Serialize)]
#[cfg_attr(feature = "web", derive(tsify::Tsify), tsify(into_wasm_abi))]
#[serde(rename_all = "snake_case")]
pub enum TowerPlaceError {
    NotPlaceable,
    NotEnoughResource,
}

/// The whole game state, source of truth.
/// Framework-agnostic: Qt drives it via update() and the command/query API.
#[derive(Debug)]
pub struct Game {
    pub level: Level,

    pub resource: Resource,
    pub waves: WaveSpec,
    pub towers: SlotMap<TowerId, Tower>,
    pub enemies: SlotMap<EnemyId, Enemy>,
    pub bullets: SlotMap<BulletId, Bullet>,
    rng: StdRng,

    pub state: GameState,
    pub paused: bool,
    pub result: GameResult,
    pub events: Vec<GameEvent>,

    pending_spawns: Vec<Enemy>,
    /// Indices of occupied tiles.
    occupied: HashSet<usize>,
}

impl Game {
    /// Construct a `Game` and begin simulating `level`.
    pub fn new(level: Level) -> Self {
        let waves = level.waves.clone();
        let resource = level.resource.clone();
        let mut game = Self {
            level,
            waves,
            resource,
            towers: SlotMap::default(),
            enemies: SlotMap::default(),
            bullets: SlotMap::default(),
            rng: StdRng::try_from_rng(&mut SysRng).unwrap(),
            state: GameState::Playing,
            paused: false,
            result: GameResult::default(),
            events: vec![],
            pending_spawns: vec![],
            occupied: HashSet::default(),
        };
        game.waves.start();
        game
    }

    /// Re-start the current level from its pristine definition (`level` is never
    /// mutated during gameplay).
    pub fn restart(&mut self) {
        *self = Game::new(self.level.clone());
    }

    /// Main loop: Advance the simulation by dt seconds. Returns true if the level
    /// has ended (victory or defeat); the caller may then check state().
    pub fn update(&mut self, dt: f32) -> bool {
        if self.state != GameState::Playing {
            return true;
        }
        if self.paused {
            return false;
        }

        self.result.time += dt;

        self.resource.update(dt); // automatic resource increase over time

        // Flush enemies queued since the last update (wave spawns + on-death spawns).
        std::mem::take(&mut self.pending_spawns)
            .into_iter()
            .for_each(|enemy| {
                self.enemies.insert(enemy);
            });

        self.waves.update(
            dt,
            &self.level.enemy_configs,
            &self.level.paths,
            |enemy| {
                self.pending_spawns.push(enemy);
            },
            |index, has_boss, is_last| {
                self.events.push(GameEvent::WaveStarted {
                    index,
                    has_boss,
                    is_last,
                });
            },
        );

        if self.update_movables(dt) {
            self.end_level(false); // an enemy reached the exit -> defeat
            return true;
        }
        self.check_collisions();
        self.update_towers(dt);

        // Victory: every wave's spawns issued and nothing left alive.
        if self.waves.all_done && self.enemies.is_empty() && self.pending_spawns.is_empty() {
            self.end_level(true);
            return true;
        }

        false
    }

    /// Place a tower of `kind` at the map position. Returns Err if the type is not
    /// available this level, the tile is invalid/occupied, or resources are insufficient.
    pub fn place_tower(&mut self, kind: &String, position: Vec2) -> Result<(), TowerPlaceError> {
        if self.state != GameState::Playing
            || self.paused
            || !self.level.available_towers.contains(kind)
        {
            return Err(TowerPlaceError::NotPlaceable);
        }

        let (tile_idx, tile) = self
            .level
            .map
            .tile_with_index_at(position)
            .ok_or(TowerPlaceError::NotPlaceable)?;
        if !tile.can_place_tower() || self.occupied.contains(&tile_idx) {
            return Err(TowerPlaceError::NotPlaceable);
        }

        let tower = make_tower(kind, &self.level.tower_configs, tile_idx)
            .ok_or(TowerPlaceError::NotPlaceable)?;
        let cost = tower.effective_resource_cost(&self.level.map);
        if !self.resource.decrease(cost) {
            return Err(TowerPlaceError::NotEnoughResource);
        }

        self.occupied.insert(tile_idx);
        self.events.push(GameEvent::TowerPlaced {
            kind: tower.kind.as_str().to_owned(),
            pos: tile.position(),
            cost,
        });
        self.towers.insert(tower);

        Ok(())
    }

    pub fn apply_cheat(&mut self, code: &str) {
        self.result.cheated = true;
        match code {
            "gold" => self.resource.increase(1000),
            "killall" => {
                self.enemies.clear();
                self.pending_spawns.clear();
            }
            "win" if self.state == GameState::Playing => {
                self.end_level(true);
            }
            _ => {}
        }
    }

    /// Check if a tower may be placed at `position` for ghost hover rendering,
    /// taking terrain and occupancy into account. The resource amount is not
    /// considered here.
    pub fn can_place_at(&self, position: Vec2) -> bool {
        self.level
            .map
            .tile_with_index_at(position)
            .is_some_and(|(tile_idx, tile)| {
                tile.can_place_tower() && !self.occupied.contains(&tile_idx)
            })
    }

    /// Drain and return the discrete event queue. The queue is emptied after each call.
    pub fn take_events(&mut self) -> Vec<GameEvent> {
        std::mem::take(&mut self.events)
    }

    /// Returns true if an enemy reaches the exit.
    fn update_movables(&mut self, dt: f32) -> bool {
        for (id, enemy) in &mut self.enemies {
            if enemy.is_destroyed() {
                continue; // destroyed enemies are cleaned up in check_collisions()
            }

            // Defeat: enemy reached the end of its route (the destination).
            if enemy.path_distance >= enemy.path(&self.level.paths).total_length() {
                return true;
            }

            // Update the enemy's position along its path.
            let old_distance = enemy.path_distance;
            enemy.update(dt, &self.level.paths, &self.level.map);

            if let Some((new_tile_idx, new_tile)) =
                self.level.map.tile_with_index_at(enemy.position())
            {
                if self.occupied.contains(&new_tile_idx) {
                    // Blocked by a tower: revert movement and attack the tower.
                    enemy.path_distance = old_distance;
                    enemy.sync_position(&self.level.paths);

                    self.towers.iter_mut().find_map(|(_, tower)| {
                        if tower.tile_index == new_tile_idx {
                            tower.apply_tower_damage(
                                id,
                                enemy.tower_damage,
                                enemy.tower_damage_interval,
                            );
                            Some(())
                        } else {
                            None
                        }
                    });
                } else if new_tile.is_portal
                    && enemy.portal_ready()
                    && let Some(distance) = enemy
                        .path(&self.level.paths)
                        .paired_portal_distance(new_tile.position())
                {
                    // Portal: jump to the paired portal's path distance on this enemy's route.
                    const PORTAL_COOLDOWN: f32 = 0.5;
                    enemy.path_distance = distance;
                    enemy.sync_position(&self.level.paths);
                    enemy.start_portal_cooldown(PORTAL_COOLDOWN);
                }
            }
        }

        // Remove bullets that are out of bounds.
        self.bullets.retain(|_, bullet| {
            if (0.0..=self.level.map.width).contains(&bullet.position.x)
                && (0.0..=self.level.map.height).contains(&bullet.position.y)
            {
                bullet.update(dt);
                true
            } else {
                false
            }
        });

        false
    }

    fn check_collisions(&mut self) {
        // Bullet vs enemy. Remove bullets that hit enemies.
        self.bullets.retain(|_, bullet| {
            if !bullet.can_explode(self.enemies.iter()) {
                return true; // not touching any enemy yet
            }

            let mut hit = false;
            for (_, enemy) in &mut self.enemies {
                if enemy.is_destroyed() || !bullet.effective(enemy) {
                    continue;
                }

                bullet.impact(enemy, &self.level.map);
                hit = true;
                if !bullet.pierces() {
                    break;
                }
            }

            !hit
        });

        // Remove all destroyed enemies (killed by bullets, poison, or tower damage),
        // triggering on-death effects (e.g. splitter spawns) and notifying observers.
        self.enemies.retain(|_, enemy| {
            if enemy.is_destroyed() {
                self.result.score += enemy.score;
                enemy.on_death(
                    |child| {
                        self.pending_spawns.push(child);
                    },
                    &self.level.paths,
                    &mut self.rng,
                );
                self.events.push(GameEvent::EnemyKilled {
                    kind: enemy.kind.as_str().to_owned(),
                    pos: enemy.position(),
                    score: enemy.score,
                });
                false
            } else {
                true
            }
        });
    }

    fn update_towers(&mut self, dt: f32) {
        self.towers.retain(|_, tower| {
            tower.update(
                dt,
                &self.level.map,
                &self.level.paths,
                self.enemies.iter(),
                |bullet| {
                    self.bullets.insert(bullet);
                },
                &mut self.resource,
                |enemy_id| self.enemies.contains_key(enemy_id),
                &mut self.rng,
            );

            if tower.is_destroyed() {
                self.occupied.remove(&tower.tile_index);
                false
            } else {
                true
            }
        });
    }

    fn end_level(&mut self, cleared: bool) {
        self.clear_entities();
        self.state = if cleared {
            GameState::Victory
        } else {
            GameState::Defeat
        };
        self.result.cleared = cleared;
    }

    fn clear_entities(&mut self) {
        self.towers.clear();
        self.enemies.clear();
        self.bullets.clear();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_util::*;
    use crate::{
        bullet::{Bullet, BulletKind},
        config::load_level,
        enemy::{Enemy, EnemyKind},
        geometry::Vec2,
        level::{Level, LevelRegistry},
    };
    use rand::SeedableRng;

    // --- shared fixtures ---

    /// Construct a `Game` started on `level`, with deterministic RNG for
    /// reproducible tests (`Game::new` seeds from the OS; override it here).
    fn game_for_level(level: Level) -> Game {
        let mut game = Game::new(level);
        game.rng = StdRng::seed_from_u64(0); // deterministic, overrides SysRng
        game
    }

    fn level1() -> Game {
        game_for_level(load_level_by_name("levels/01-meadow.json"))
    }

    /// Build an enemy of `kind` from the level's config, on path 0.
    fn make_enemy(game: &Game, kind: &str) -> Enemy {
        crate::config::make_enemy(kind, &game.level.enemy_configs, 0, &game.level.paths).unwrap()
    }

    /// A 7x12 all-grass level with one straight horizontal path on row 3 and
    /// every tower type available. The single wave spawns at t=100 so it never
    /// interferes with manual enemy spawns during a test.
    fn simple_level_json() -> String {
        let row = "[\"grass\",\"grass\",\"grass\",\"grass\",\"grass\",\"grass\",\"grass\",\"grass\",\"grass\",\"grass\",\"grass\",\"grass\"]";
        let terrain = format!("[{row},{row},{row},{row},{row},{row},{row}]");
        format!(
            "{{\"name\":\"Testfield\",\"index\":-1,\
             \"map\":{{\"rows\":7,\"cols\":12,\"terrain\":{terrain}}},\
             \"paths\":[{{\"waypoints\":[[0,3],[11,3]],\"portals\":[]}}],\
             \"starting_resources\":1000,\"resource_auto_inc_amount\":0,\"resource_auto_inc_interval\":1.0,\
             \"available_towers\":[\"normal\",\"slow\",\"poison\",\"splash\",\"laser\",\"resource\",\"wall\"],\
             \"waves\":[{{\"gap\":10.0,\"spawns\":[{{\"kind\":\"normal\",\"time\":100.0,\"path\":0}}]}}]}}"
        )
    }

    fn test_level() -> Game {
        let (towers, enemies) = load_stats();
        game_for_level(load_level(&simple_level_json(), towers, enemies).unwrap())
    }

    /// Run `seconds` at 60 fps, clearing enemies each frame so none leak to the exit.
    fn run_clearing(game: &mut Game, seconds: f32) {
        let dt = 1.0 / 60.0;
        let frames = (seconds / dt) as i32;
        for _ in 0..frames {
            if game.state != GameState::Playing {
                break;
            }
            game.update(dt);
            game.apply_cheat("killall");
        }
    }

    /// Run up to `frames` updates until `pred` is true (or the game ends).
    fn run_until(game: &mut Game, mut pred: impl FnMut(&Game) -> bool, frames: i32) {
        let dt = 1.0 / 60.0;
        for _ in 0..frames {
            if game.state != GameState::Playing || pred(game) {
                break;
            }
            game.update(dt);
        }
    }

    fn over(game: &Game) -> bool {
        game.state != GameState::Playing
    }

    // ===================== Wave scheduling =====================

    #[test]
    fn no_enemies_spawn_during_the_initial_pre_wave_gap() {
        let mut game = level1();
        let gap0 = game.waves.gaps[0]; // 3.0s for level 1

        let dt = 1.0 / 60.0;
        let frames = ((gap0 - 0.1) / dt) as i32;
        for _ in 0..frames {
            game.update(dt);
        }

        assert!(game.enemies.is_empty());
        assert!(!game.waves.all_done);
    }

    #[test]
    fn enemies_appear_after_the_initial_gap_elapses() {
        let mut game = level1();
        let gap0 = game.waves.gaps[0];

        let dt = 1.0 / 60.0;
        let frames = ((gap0 + 1.0) / dt) as i32;
        for _ in 0..frames {
            if over(&game) {
                break;
            }
            game.update(dt);
        }

        assert!(!game.enemies.is_empty());
        assert!(game.waves.current_wave_display() >= 1);
    }

    #[test]
    fn current_wave_progresses_through_multiple_waves() {
        let mut game = level1();
        run_clearing(&mut game, 15.0);
        assert!(game.waves.current_wave_display() >= 2);
        assert!(!game.waves.all_done);
    }

    #[test]
    fn all_waves_done_becomes_true_after_all_waves_are_issued() {
        let mut game = level1();
        run_clearing(&mut game, 60.0);
        assert!(game.waves.all_done);
    }

    #[test]
    fn gaps_are_non_uniform_between_waves() {
        let game = level1();
        let gaps = &game.waves.gaps;
        assert!(gaps.len() >= 3);
        let any_diff = gaps[1..].iter().any(|g| *g != gaps[0]);
        assert!(any_diff);
    }

    #[test]
    fn wave_progression_is_independent_of_enemy_state() {
        let mut game = level1();
        let dt = 1.0 / 60.0;
        let gap0 = game.waves.gaps[0];
        let gap1 = if game.waves.gaps.len() > 1 {
            game.waves.gaps[1]
        } else {
            3.0
        };
        // Last spawn time of wave 0.
        let last_spawn = game.waves.waves[0]
            .iter()
            .map(|(_, t, _)| *t)
            .fold(0.0f32, f32::max);
        let target = gap0 + last_spawn + gap1 + 1.0;
        let frames = (target / dt) as i32;
        for _ in 0..frames {
            if over(&game) {
                break;
            }
            game.update(dt);
        }
        // Wave 1 should have started, or the game ended — either way the wave
        // system ran.
        assert!(game.waves.current_wave_display() >= 2 || over(&game));
    }

    // ===================== place_tower validation =====================

    #[test]
    fn place_tower_valid_off_path_tile_succeeds() {
        let mut game = level1();
        assert!(
            game.place_tower(&"normal".to_string(), Vec2::new(0.5, 0.5))
                .is_ok()
        );
        assert_eq!(game.towers.len(), 1);
    }

    #[test]
    fn place_tower_on_the_path_blocks_enemies() {
        let mut game = level1();
        assert!(
            game.place_tower(&"normal".to_string(), Vec2::new(5.5, 3.5))
                .is_ok()
        );
        assert_eq!(game.towers.len(), 1);
    }

    #[test]
    fn place_tower_rock_tile_rejected() {
        let mut game = level1();
        assert!(
            game.place_tower(&"normal".to_string(), Vec2::new(1.5, 2.5))
                .is_err()
        );
        assert!(game.towers.is_empty());
    }

    #[test]
    fn place_tower_wall_on_the_path() {
        let mut game = level1();
        assert!(
            game.place_tower(&"wall".to_string(), Vec2::new(5.5, 3.5))
                .is_ok()
        );
        assert_eq!(game.towers.len(), 1);
    }

    #[test]
    fn place_tower_unavailable_type_rejected() {
        let mut game = level1(); // level 1 does not offer laser
        assert!(
            game.place_tower(&"laser".to_string(), Vec2::new(0.5, 0.5))
                .is_err()
        );
        assert!(game.towers.is_empty());
    }

    #[test]
    fn place_tower_insufficient_resources_rejected() {
        let mut game = level1(); // starting 150; normal costs 50 -> 3, then fail
        assert!(
            game.place_tower(&"normal".to_string(), Vec2::new(0.5, 0.5))
                .is_ok()
        );
        assert!(
            game.place_tower(&"normal".to_string(), Vec2::new(2.5, 0.5))
                .is_ok()
        );
        assert!(
            game.place_tower(&"normal".to_string(), Vec2::new(4.5, 0.5))
                .is_ok()
        );
        assert!(
            game.place_tower(&"normal".to_string(), Vec2::new(6.5, 0.5))
                .is_err()
        );
    }

    #[test]
    fn place_tower_rejected_when_paused() {
        let mut game = level1();
        game.paused = true;
        assert!(
            game.place_tower(&"normal".to_string(), Vec2::new(0.5, 0.5))
                .is_err()
        );
        game.paused = false;
        assert!(
            game.place_tower(&"normal".to_string(), Vec2::new(0.5, 0.5))
                .is_ok()
        );
    }

    // ===================== combat & flow =====================

    #[test]
    fn attack_tower_fires_a_bullet_aimed_at_an_in_range_enemy() {
        let mut game = level1();
        assert!(
            game.place_tower(&"normal".to_string(), Vec2::new(4.5, 2.5))
                .is_ok()
        );

        let mut enemy = make_enemy(&game, "normal");
        enemy.path_distance = 7.0; // position ~ (4.5, 3.5) — near the tower
        game.pending_spawns.push(enemy);

        game.update(1.0 / 60.0);
        assert!(!game.bullets.is_empty());
        // Enemy is below the tower -> bullet travels downward.
        let v = game.bullets.values().next().unwrap().velocity;
        assert!(v.y > 0.0);
    }

    #[test]
    fn wall_blocks_enemies_and_takes_damage() {
        let mut game = level1();
        assert!(
            game.place_tower(&"wall".to_string(), Vec2::new(5.5, 3.5))
                .is_ok()
        );
        let initial_hp: i32 = game
            .towers
            .values()
            .find(|t| matches!(t.kind, crate::tower::TowerKind::Wall))
            .map(|t| t.health)
            .unwrap();
        assert!(initial_hp > 0);

        let dt = 1.0 / 60.0;
        for _ in 0..(60 * 8) {
            if over(&game) {
                break;
            }
            game.update(dt);
        }
        let hp_now: i32 = game
            .towers
            .values()
            .find(|t| matches!(t.kind, crate::tower::TowerKind::Wall))
            .map(|t| t.health)
            .unwrap();
        assert!(hp_now < initial_hp);
    }

    #[test]
    fn any_tower_on_the_path_blocks_enemies_and_takes_damage() {
        let mut game = level1();
        assert!(
            game.place_tower(&"normal".to_string(), Vec2::new(5.5, 3.5))
                .is_ok()
        );
        let mut enemy = make_enemy(&game, "armored");
        enemy.path_distance = 5.5; // just upstream of the tower
        game.pending_spawns.push(enemy);

        let initial_hp: i32 = game
            .towers
            .values()
            .find(|t| t.kind.as_str() == "normal") // normal
            .map(|t| t.health)
            .unwrap();
        assert!(initial_hp > 0);

        let dt = 1.0 / 60.0;
        for _ in 0..(60 * 2) {
            if over(&game) {
                break;
            }
            game.update(dt);
        }
        let hp_now: i32 = game
            .towers
            .values()
            .find(|t| t.kind.as_str() == "normal") // normal
            .map(|t| t.health)
            .unwrap();
        assert!(hp_now < initial_hp);
    }

    #[test]
    fn portal_teleports_enemies_forward() {
        let mut game = game_for_level(load_level_by_name("levels/02-switchback.json"));
        let path = &game.level.paths[0];
        assert!(!path.portal_pairs.is_empty());
        let (src_idx, _tgt_idx) = path.portal_pairs[0];
        let portal_pos = path.waypoints[src_idx];
        let source_dist = path.cumulative[src_idx];
        let dest_dist = path.paired_portal_distance(portal_pos).unwrap();
        assert!(dest_dist > source_dist); // forward-only

        let mut enemy = make_enemy(&game, "normal");
        enemy.path_distance = source_dist;
        game.pending_spawns.push(enemy);

        game.update(1.0 / 60.0);
        // The enemy should have jumped to the destination distance.
        let d = game.enemies.values().next().unwrap().path_distance;
        assert!((d - dest_dist).abs() / dest_dist.max(1e-6) < 0.01);
    }

    #[test]
    fn defeat_when_an_enemy_reaches_the_exit() {
        let mut game = level1();
        let dt = 1.0 / 60.0;
        for _ in 0..(60 * 30) {
            if over(&game) {
                break;
            }
            game.update(dt);
        }
        assert_eq!(game.state, GameState::Defeat);
        assert!(!game.result.cleared);
    }

    #[test]
    fn victory_when_all_waves_are_cleared() {
        let mut game = level1();
        let dt = 1.0 / 60.0;
        for _ in 0..(60 * 120) {
            if over(&game) || game.waves.all_done {
                break;
            }
            game.update(dt);
            game.apply_cheat("killall");
        }
        assert!(game.waves.all_done);
        game.apply_cheat("killall");
        game.update(dt); // triggers the victory check
        assert_eq!(game.state, GameState::Victory);
        assert!(game.result.cleared);
    }

    #[test]
    fn pause_halts_the_simulation() {
        let mut game = level1();
        let t0 = game.result.time;
        game.paused = true;
        for _ in 0..60 {
            game.update(1.0 / 60.0);
        }
        assert_approx_eq!(game.result.time, t0);
        assert_eq!(game.state, GameState::Playing);

        game.paused = false;
        game.update(1.0 / 60.0);
        assert!(game.result.time > t0);
    }

    // ===================== cheats =====================

    #[test]
    fn cheat_gold_adds_1000_resources() {
        let mut game = level1();
        let before = game.resource.amount;
        game.apply_cheat("gold");
        assert_eq!(game.resource.amount, before + 1000);
    }

    #[test]
    fn cheat_killall_clears_enemies() {
        let mut game = level1();
        let e = make_enemy(&game, "normal");
        game.pending_spawns.push(e);
        game.update(1.0 / 60.0); // flush into enemies
        assert!(!game.enemies.is_empty());
        game.apply_cheat("killall");
        assert!(game.enemies.is_empty());
    }

    #[test]
    fn cheat_win_instantly_ends_the_level_in_victory() {
        let mut game = level1();
        game.apply_cheat("win");
        assert_eq!(game.state, GameState::Victory);
        assert!(game.result.cleared);
    }

    // ===================== enemies: splitter / boss / score =====================

    #[test]
    fn splitter_spawns_children_on_death() {
        let mut game = level1();
        let child_count = match &make_enemy(&game, "splitter").kind {
            EnemyKind::Splitter { child_spec } => child_spec.count,
            _ => panic!("expected Splitter"),
        };
        assert!(child_count > 0);

        let sp = make_enemy(&game, "splitter");
        let paths = game.level.paths.clone();
        let mut rng = StdRng::seed_from_u64(0);
        sp.on_death(|child| game.pending_spawns.push(child), &paths, &mut rng);
        game.update(0.0); // flush pending into enemies (waves haven't spawned yet)
        assert_eq!(game.enemies.len(), child_count as usize);
    }

    #[test]
    fn boss_regenerates_health_over_time() {
        let game = level1();
        let paths = game.level.paths.clone();
        let map = game.level.map.clone();
        let mut boss = make_enemy(&game, "boss");
        let regen_interval = match &boss.kind {
            EnemyKind::Boss { regen_interval, .. } => *regen_interval,
            _ => panic!("expected Boss"),
        };
        assert!(regen_interval > 0.0);
        boss.health = 100;
        let dt = 1.0 / 60.0;
        for _ in 0..((regen_interval / dt) as i32 + 60) {
            boss.update(dt, &paths, &map);
        }
        assert!(boss.health > 100);
    }

    #[test]
    fn killing_an_enemy_awards_its_per_type_score() {
        let mut game = level1();
        let expected = make_enemy(&game, "normal").score;
        assert!(expected > 0);

        let mut enemy = make_enemy(&game, "normal");
        enemy.health = 1;
        game.pending_spawns.push(enemy);
        game.update(1.0 / 60.0); // enemy now in enemies_
        assert_eq!(game.result.score, 0);

        for (_, e) in &mut game.enemies {
            e.health = 0;
        }
        game.update(1.0 / 60.0); // reap + award score
        assert_eq!(game.result.score, expected);

        // A boss is worth more than a normal enemy.
        let mut g2 = level1();
        let mut boss = make_enemy(&g2, "boss");
        boss.health = 1;
        g2.pending_spawns.push(boss);
        g2.update(1.0 / 60.0);
        for (_, e) in &mut g2.enemies {
            e.health = 0;
        }
        g2.update(1.0 / 60.0);
        assert!(g2.result.score > expected);
    }

    // ===================== integration: tower effects =====================

    #[test]
    fn resource_tower_generates_gold_over_time() {
        let mut game = test_level();
        assert!(
            game.place_tower(&"resource".to_string(), Vec2::new(2.5, 2.5))
                .is_ok()
        );
        assert_eq!(game.resource.amount, 950); // 1000 - 50, nothing granted yet

        game.update(1.0 / 60.0);
        assert_eq!(game.resource.amount, 958); // +8 immediately (cooldown starts finished)

        for _ in 0..(60 * 2 - 1) {
            game.update(1.0 / 60.0);
        } // ~2s total, still one grant
        assert_eq!(game.resource.amount, 958);

        for _ in 0..60 {
            game.update(1.0 / 60.0);
        } // pass the 2.5s mark -> second grant
        assert_eq!(game.resource.amount, 966);
    }

    #[test]
    fn splash_tower_damages_a_clustered_group() {
        let mut game = test_level();
        assert!(
            game.place_tower(&"splash".to_string(), Vec2::new(2.5, 2.5))
                .is_ok()
        );

        for _ in 0..3 {
            let mut e = make_enemy(&game, "normal");
            e.path_distance = 1.5; // overlapping at (2.0, 3.5), within the splash radius
            game.pending_spawns.push(e);
        }
        run_until(&mut game, |g| g.enemies.values().any(|e| e.health < 25), 90);
        // One splash shot deals 15 to every enemy in the radius (normal has 25 hp -> 10).
        for e in game.enemies.values() {
            assert_eq!(e.health, 10);
        }
    }

    #[test]
    fn laser_pierces_multiple_enemies_in_its_beam() {
        let mut game = test_level();
        assert!(
            game.place_tower(&"laser".to_string(), Vec2::new(2.5, 2.5))
                .is_ok()
        );

        for _ in 0..2 {
            let mut e = make_enemy(&game, "normal");
            e.path_distance = 1.5; // both on the beam
            game.pending_spawns.push(e);
        }
        run_until(
            &mut game,
            |g| !g.enemies.is_empty() && g.enemies.values().all(|e| e.health < 25),
            90,
        );
        // The piercing laser hits both (25 - 20 = 5).
        for e in game.enemies.values() {
            assert_eq!(e.health, 5);
        }
    }

    #[test]
    fn poison_deals_one_shot_damage_and_roots_the_enemy() {
        let mut game = test_level();
        assert!(
            game.place_tower(&"poison".to_string(), Vec2::new(2.5, 2.5))
                .is_ok()
        );

        let mut e = make_enemy(&game, "armored");
        e.path_distance = 1.5;
        game.pending_spawns.push(e);

        run_until(
            &mut game,
            |g| g.enemies.values().next().is_some_and(|e| e.health < 100),
            90,
        );
        assert_eq!(enemy_health(&game), 80); // armored 100 - 20 poison
        assert!(enemy_hint(&game).poison);

        // Rooted: the enemy does not advance while the poison root is active.
        let d = enemy_path_distance(&game);
        for _ in 0..30 {
            game.update(1.0 / 60.0);
        } // 0.5s, still rooted
        assert_approx_eq!(enemy_path_distance(&game), d);

        for _ in 0..120 {
            game.update(1.0 / 60.0);
        } // 2s more -> root expired, advances
        assert!(enemy_path_distance(&game) > d);
    }

    #[test]
    fn slow_tower_applies_a_slow_status_and_impedes_progress() {
        let mut game = test_level();
        assert!(
            game.place_tower(&"slow".to_string(), Vec2::new(2.5, 2.5))
                .is_ok()
        );
        let mut slowed = make_enemy(&game, "normal");
        slowed.path_distance = 1.5;
        game.pending_spawns.push(slowed);
        run_until(
            &mut game,
            |g| {
                g.enemies
                    .values()
                    .next()
                    .is_some_and(|e| e.status_hint().slow)
            },
            90,
        );
        assert!(enemy_hint(&game).slow);

        // Compare against an unslowed enemy in a fresh game over the same window.
        let d_slowed = enemy_path_distance(&game);
        let mut g2 = test_level();
        let mut free = make_enemy(&g2, "normal");
        free.path_distance = 1.5;
        let d_free = free.path_distance; // captured before spawning (still pending)
        g2.pending_spawns.push(free);

        for _ in 0..60 {
            game.update(1.0 / 60.0);
            g2.update(1.0 / 60.0);
        }
        assert!(enemy_path_distance(&game) - d_slowed < enemy_path_distance(&g2) - d_free);
    }

    #[test]
    fn resistant_enemy_reduces_splash_damage() {
        let mut game = test_level();
        assert!(
            game.place_tower(&"splash".to_string(), Vec2::new(2.5, 2.5))
                .is_ok()
        );
        let mut r = make_enemy(&game, "resistant");
        r.path_distance = 1.5;
        game.pending_spawns.push(r);
        run_until(
            &mut game,
            |g| g.enemies.values().next().is_some_and(|e| e.health < 50),
            90,
        );
        // Splash factor 0.3: 15 * 0.3 = 4.5 -> 4 damage (vs 15 for a normal enemy).
        assert_eq!(enemy_health(&game), 46);
    }

    #[test]
    fn boss_shield_flatly_reduces_every_hit() {
        let game = test_level();
        let mut boss = make_enemy(&game, "boss");
        assert_eq!(boss.health, 500);
        boss.decrease_health(15); // 15 - shield 10 = 5 through
        assert_eq!(boss.health, 495);
        boss.decrease_health(5); // max(0, 5 - 10) = 0, fully absorbed
        assert_eq!(boss.health, 495);
        boss.decrease_health(20); // 20 - 10 = 10 through
        assert_eq!(boss.health, 485);
        boss.decrease_health(11); // 11 - 10 = 1 through
        assert_eq!(boss.health, 484);
    }

    #[test]
    fn first_wave_spawns_its_full_complement_of_enemies() {
        let mut game = level1(); // wave 0 = 4 normals at t=3,4,5,6
        let dt = 1.0 / 60.0;
        for _ in 0..(60 * 7) {
            if over(&game) {
                break;
            }
            game.update(dt);
        } // t = 7s
        assert_eq!(game.enemies.len(), 4);
        assert_eq!(game.state, GameState::Playing);
    }

    // ===================== misc query / lifecycle =====================

    #[test]
    fn out_of_bounds_bullets_are_culled_each_update() {
        let mut game = test_level();
        game.bullets.insert(Bullet::new(
            Vec2::new(0.5, 3.5),
            Vec2::new(-50.0, 0.0),
            BulletKind::Normal { health_damage: 5 },
        ));
        assert_eq!(game.bullets.len(), 1);
        for _ in 0..3 {
            game.update(1.0 / 60.0);
        }
        assert!(game.bullets.is_empty());
    }

    #[test]
    fn tower_query_and_health_mutator_api() {
        let mut game = test_level();
        assert!(
            game.place_tower(&"normal".to_string(), Vec2::new(2.5, 2.5))
                .is_ok()
        );
        assert!(
            game.place_tower(&"resource".to_string(), Vec2::new(4.5, 2.5))
                .is_ok()
        );

        for t in game.towers.values_mut() {
            assert_eq!(t.max_health, t.health);
            t.health = 10;
            assert_eq!(t.health, 10);
            t.increase_health(5);
            assert_eq!(t.health, std::cmp::min(15, t.max_health));
        }
        for t in game.towers.values() {
            if let crate::tower::TowerKind::Attack { aim, .. } = &t.kind {
                assert_approx_eq!(aim.length_sq(), 1.0); // unit vector
            }
            if let crate::tower::TowerKind::Resource {
                resource_inc_amount,
                ..
            } = &t.kind
            {
                assert!(*resource_inc_amount > 0);
            }
        }
    }

    #[test]
    fn destroyed_tower_is_removed_from_the_board() {
        let mut game = test_level();
        assert!(
            game.place_tower(&"wall".to_string(), Vec2::new(5.5, 3.5))
                .is_ok()
        );
        for t in game.towers.values_mut() {
            if matches!(t.kind, crate::tower::TowerKind::Wall) {
                t.health = 1;
            }
        }
        let mut e = make_enemy(&game, "normal");
        e.path_distance = 4.0; // upstream of the wall; walks into and attacks it
        game.pending_spawns.push(e);

        run_until(&mut game, |g| g.towers.is_empty() || over(g), 60 * 5);
        assert!(game.towers.is_empty());
    }

    #[test]
    fn tower_stops_tracking_damage_once_its_attacker_dies() {
        let mut game = test_level();
        assert!(
            game.place_tower(&"wall".to_string(), Vec2::new(5.5, 3.5))
                .is_ok()
        );
        let mut e = make_enemy(&game, "normal");
        e.path_distance = 4.0;
        game.pending_spawns.push(e);

        run_until(
            &mut game,
            |g| {
                g.towers.values().any(|t| {
                    matches!(t.kind, crate::tower::TowerKind::Wall) && t.health < t.max_health
                })
            },
            60 * 5,
        );
        assert!(!game.enemies.is_empty());

        // Kill the attacker; the next update erases it and the wall drops the entry.
        for e in game.enemies.values_mut() {
            e.health = 0;
        }
        game.update(1.0 / 60.0);
        assert!(game.enemies.is_empty());
        assert!(!game.towers.is_empty()); // wall survived
    }

    #[test]
    fn boss_regeneration_advertises_a_regen_status_hint() {
        let game = level1();
        let paths = game.level.paths.clone();
        let map = game.level.map.clone();
        let mut boss = make_enemy(&game, "boss");
        let regen_interval = match &boss.kind {
            EnemyKind::Boss { regen_interval, .. } => *regen_interval,
            _ => panic!("expected Boss"),
        };
        boss.health = 100;
        assert!(!boss.status_hint().regen);

        let dt = 1.0 / 60.0;
        // Run just past one regen interval (well inside the 1s effect window).
        for _ in 0..((regen_interval / dt) as i32 + 30) {
            boss.update(dt, &paths, &map);
        }
        assert!(boss.status_hint().regen);
    }

    #[test]
    fn splitter_clamps_children_that_would_overshoot_the_exit() {
        let game = level1();
        let path = &game.level.paths[0];
        let total = path.total_length();
        let perturbation = 1.0;
        let child_spec = crate::enemy::ChildSpec {
            width: 0.4,
            height: 0.4,
            speed: 1.0,
            max_health: 1,
            score: 1,
            tower_damage: 0,
            tower_damage_interval: 0.0,
            count: 1,
            perturbation,
        };
        let mut sp = Enemy::new(
            crate::geometry::Rect::new(Vec2::new(0.5, 3.5), 0.5, 0.5),
            1.0,
            1,
            0,
            0,
            0.0,
            0,
            EnemyKind::Splitter { child_spec },
        );
        sp.path_distance = total + perturbation; // beyond the exit by >= perturbation
        let paths = game.level.paths.clone();
        let mut rng = StdRng::seed_from_u64(0);
        let mut game = game; // shadow to mut
        sp.on_death(|child| game.pending_spawns.push(child), &paths, &mut rng);
        game.update(0.0); // flush the child into enemies_

        assert_eq!(game.enemies.len(), 1);
        assert!(game.enemies.values().next().unwrap().path_distance < total);
    }

    // ===================== events / queries =====================

    #[test]
    fn take_events_reports_tower_placement_and_enemy_kill() {
        let mut game = level1();
        assert!(
            game.place_tower(&"normal".to_string(), Vec2::new(0.5, 0.5))
                .is_ok()
        );
        let events = game.take_events();
        assert!(!events.is_empty());
        let placed = events.iter().find_map(|e| match e {
            GameEvent::TowerPlaced { cost, .. } => Some(*cost),
            _ => None,
        });
        let cost = placed.expect("a TowerPlaced event");
        assert!(cost > 0);
        assert!(game.take_events().is_empty()); // drained

        // Kill an enemy -> EnemyKilled event.
        let mut enemy = make_enemy(&game, "normal");
        enemy.health = 1;
        game.pending_spawns.push(enemy);
        game.update(1.0 / 60.0); // flush into enemies_
        for e in game.enemies.values_mut() {
            e.health = 0;
        }
        game.update(1.0 / 60.0); // reap + event
        let events = game.take_events();
        let score = events
            .iter()
            .find_map(|e| match e {
                GameEvent::EnemyKilled { score, .. } => Some(*score),
                _ => None,
            })
            .expect("an EnemyKilled event");
        assert!(score > 0);
    }

    #[test]
    fn take_events_reports_wave_start() {
        let mut game = level1();
        let dt = 1.0 / 60.0;
        for _ in 0..(60 * 5) {
            if over(&game) {
                break;
            }
            game.update(dt);
        } // run past the first gap (3.0s)
        let events = game.take_events();
        let wave = events
            .iter()
            .find_map(|e| match e {
                GameEvent::WaveStarted { index, .. } => Some(*index),
                _ => None,
            })
            .expect("a WaveStarted event");
        assert!(wave >= 1); // 1-based
    }

    #[test]
    fn can_place_at_validates_tiles() {
        let mut game = level1();
        assert!(game.can_place_at(Vec2::new(0.5, 0.5))); // grass
        assert!(!game.can_place_at(Vec2::new(1.5, 2.5))); // rock
        assert!(!game.can_place_at(Vec2::new(-1.0, -1.0))); // out of bounds

        assert!(
            game.place_tower(&"normal".to_string(), Vec2::new(0.5, 0.5))
                .is_ok()
        );
        assert!(!game.can_place_at(Vec2::new(0.5, 0.5))); // now occupied
    }

    #[test]
    fn last_result_after_defeat_and_victory() {
        let mut game = level1();
        let dt = 1.0 / 60.0;
        for _ in 0..(60 * 30) {
            if over(&game) {
                break;
            }
            game.update(dt);
        }
        assert_eq!(game.state, GameState::Defeat);
        assert!(!game.result.cleared);
        assert!(game.result.time > 0.0);

        let mut g2 = level1();
        g2.apply_cheat("win");
        g2.update(dt);
        assert_eq!(g2.state, GameState::Victory);
        assert!(g2.result.cleared);
    }

    // --- registry + level-lifecycle fixture (mirrors C++ GameFixture) ---

    struct GameFixture {
        registry: LevelRegistry,
        game: Game,
    }

    impl GameFixture {
        fn new() -> Self {
            let mut registry = LevelRegistry::default();
            registry.load_from_dir(&config_dir()).unwrap();
            let game = Game::new(registry.current_level().clone());
            Self { registry, game }
        }

        fn advance(&mut self) -> bool {
            if !self.registry.has_next_official() {
                return false;
            }
            self.registry.advance();
            self.game = Game::new(self.registry.current_level().clone());
            true
        }

        fn select(&mut self, index: i32) {
            self.registry.select(index);
            self.game = Game::new(self.registry.current_level().clone());
        }

        /// Play a level directly without adding it to the registry (mirrors
        /// production `playCustomLevel`).
        fn play(&mut self, level: Level) {
            self.game = Game::new(level);
        }
    }

    #[test]
    fn game_loads_from_config() {
        let fx = GameFixture::new();
        assert!(fx.registry.levels.len() >= 3);
        assert_eq!(fx.registry.current, 0);
        assert_eq!(fx.game.state, GameState::Playing);
        assert!(!over(&fx.game));
    }

    #[test]
    fn restart_resets_the_level() {
        let mut game = level1();
        game.apply_cheat("gold");
        game.place_tower(&"normal".to_string(), Vec2::new(0.5, 0.5))
            .unwrap();
        for _ in 0..60 {
            if game.state != GameState::Playing {
                break;
            }
            game.update(1.0 / 60.0);
        }
        assert_eq!(game.towers.len(), 1);
        assert!(game.result.score >= 0);

        game.restart();
        assert_eq!(game.state, GameState::Playing);
        assert!(game.towers.is_empty());
        assert!(game.enemies.is_empty());
        assert_eq!(game.result.score, 0);
        assert_approx_eq!(game.result.time, 0.0);
    }

    #[test]
    fn level_progression() {
        let mut fx = GameFixture::new();
        assert_eq!(fx.registry.current, 0);
        let first = fx.game.level.info.name.clone();
        assert!(fx.advance());
        assert_eq!(fx.registry.current, 1);
        assert_ne!(fx.game.level.info.name, first);
        assert_eq!(fx.game.state, GameState::Playing);
        while fx.advance() {}
        assert!(!fx.advance()); // no more official levels
    }

    #[test]
    fn select_level_jumps_to_and_clamps() {
        let mut fx = GameFixture::new();
        let n = fx.registry.levels.len();
        fx.select((n - 1) as i32); // last
        assert_eq!(fx.registry.current, n - 1);
        fx.select((n + 100) as i32); // clamp high -> last
        assert_eq!(fx.registry.current, n - 1);
        fx.select(-5); // clamp low -> first
        assert_eq!(fx.registry.current, 0);
    }

    #[test]
    fn custom_level_plays_end_to_end() {
        let mut fx = GameFixture::new();
        let n = fx.registry.levels.len();
        let (towers, enemies) = load_stats();
        let level = load_level(&simple_level_json(), towers, enemies).unwrap();
        fx.play(level);
        // play() does NOT add to the registry.
        assert_eq!(fx.registry.levels.len(), n);
        assert_eq!(fx.registry.current, 0);
        assert_eq!(fx.game.level.info.name, "Testfield");
        assert_eq!(fx.game.state, GameState::Playing);

        fx.game.apply_cheat("win");
        assert_eq!(fx.game.state, GameState::Victory);
        assert!(fx.game.result.cleared);
        assert_eq!(fx.game.level.info.index, -1); // custom level
    }

    #[test]
    fn misc_query_api() {
        let mut fx = GameFixture::new();
        assert!(!fx.game.level.map.tiles.is_empty());
        assert!(fx.game.level.map.width > 0.0);
        assert!(!fx.game.paused);
        fx.game.paused = true;
        assert!(fx.game.paused);
        fx.game.paused = false;

        let infos = fx.registry.infos();
        assert_eq!(infos.len(), fx.registry.levels.len());
        assert_eq!(infos[0].name, fx.game.level.info.name);

        // take_events fires on a discrete event.
        assert!(
            fx.game
                .place_tower(&"normal".to_string(), Vec2::new(0.5, 0.5))
                .is_ok()
        );
        let events = fx.game.take_events();
        assert!(!events.is_empty());
        assert!(matches!(events[0], GameEvent::TowerPlaced { .. }));
    }

    // --- helpers for single-enemy integration tests ---

    fn enemy_health(game: &Game) -> i32 {
        game.enemies.values().next().unwrap().health
    }
    fn enemy_path_distance(game: &Game) -> f32 {
        game.enemies.values().next().unwrap().path_distance
    }
    fn enemy_hint(game: &Game) -> crate::enemy::StatusHint {
        game.enemies.values().next().unwrap().status_hint()
    }
}
