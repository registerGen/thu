use std::sync::Arc;

use crate::{
    bullet::{Bullet, BulletKind},
    config::{TowerConfig, load_level},
    enemy::{Enemy, StatusHint},
    game::{Game, GameEvent, GameResult, GameState, TowerPlaceError},
    geometry::Vec2,
    level::{Level, LevelInfo, LevelRegistry},
    map::Map,
    path::Path,
    tile::Tile,
    tower::{Tower, TowerKind},
};

#[cxx::bridge(namespace = "td::ffi")]
mod ffi {
    #[derive(Clone, Copy)]
    struct Vec2 {
        x: f32,
        y: f32,
    }

    #[derive(Clone)]
    struct TowerView {
        pos: Vec2,
        aim: Vec2,
        kind: String,
        health: i32,
        max_health: i32,
    }

    #[derive(Clone, Copy)]
    struct StatusFlags {
        slow: bool,
        poison: bool,
        regen: bool,
    }

    #[derive(Clone)]
    struct EnemyView {
        pos: Vec2,
        half_width: f32,
        half_height: f32,
        kind: String,
        health: i32,
        max_health: i32,
        status_hint: StatusFlags,
    }

    #[derive(Clone)]
    struct BulletView {
        pos: Vec2,
        vel: Vec2,
        kind: String,
        radius: f32,
    }

    #[derive(Clone)]
    struct TileView {
        terrain: String,
    }

    #[derive(Clone)]
    struct LevelInfoView {
        name: String,
        index: i32,
    }

    #[derive(Clone)]
    struct TowerCostView {
        kind: String,
        cost: i32,
    }

    #[derive(Clone)]
    struct PathWaypointsView {
        waypoints: Vec<Vec2>,
    }

    #[derive(Clone)]
    struct LevelView {
        name: String,
        index: i32,
        map_width: f32,
        map_height: f32,
        terrain: Vec<TileView>,
        path_waypoints: Vec<PathWaypointsView>,
        available_towers: Vec<String>,
        tower_costs: Vec<TowerCostView>,
    }

    #[derive(Clone, Copy)]
    enum GameState {
        Playing,
        Victory,
        Defeat,
    }

    #[derive(Clone, Copy)]
    struct GameResultView {
        cleared: bool,
        cheated: bool,
        time: f32,
        score: i32,
    }

    #[derive(Clone)]
    struct GameEventView {
        kind: String,
        pos: Vec2,
        a: i32,
        type_tag: String,
        has_boss: bool,
        is_last: bool,
    }

    extern "Rust" {
        type Game;
        type LevelRegistry;

        // --- Game: lifecycle ---
        fn new_game(registry: &LevelRegistry) -> Box<Game>;
        fn tick(game: &mut Game, dt: f32) -> bool;
        fn restart(game: &mut Game);
        fn pause(game: &mut Game);
        fn resume(game: &mut Game);
        fn start_level_json(game: &mut Game, json: String) -> Result<()>;

        // --- Game: commands ---
        /// 0 = placed, 1 = not placeable, 2 = insufficient resources.
        fn place_tower(game: &mut Game, kind: String, pos: Vec2) -> i32;
        fn can_place_at(game: &Game, pos: Vec2) -> bool;
        fn apply_cheat(game: &mut Game, code: String);

        // --- Game: scalar queries ---
        fn state(game: &Game) -> GameState;
        fn paused(game: &Game) -> bool;
        fn score(game: &Game) -> i32;
        fn elapsed_time(game: &Game) -> f32;
        fn current_wave(game: &Game) -> i32;
        fn resource_amount(game: &Game) -> i32;
        fn level_name(game: &Game) -> String;
        fn level_index(game: &Game) -> i32;
        fn map_width(game: &Game) -> f32;
        fn map_height(game: &Game) -> f32;

        // --- Game: view snapshots ---
        fn tower_views(game: &Game) -> Vec<TowerView>;
        fn enemy_views(game: &Game) -> Vec<EnemyView>;
        fn bullet_views(game: &Game) -> Vec<BulletView>;
        fn level_view(game: &Game) -> LevelView;
        fn last_result_view(game: &Game) -> GameResultView;
        fn take_events(game: &mut Game) -> Vec<GameEventView>;

        // --- LevelRegistry ---
        fn new_registry(config_dir: String) -> Result<Box<LevelRegistry>>;
        fn current_level_index(registry: &LevelRegistry) -> i32;
        fn has_next_official(registry: &LevelRegistry) -> bool;
        fn advance(registry: &mut LevelRegistry);
        fn select(registry: &mut LevelRegistry, index: i32);
        fn registry_size(registry: &LevelRegistry) -> usize;
        fn registry_infos(registry: &LevelRegistry) -> Vec<LevelInfoView>;
        fn official_infos(registry: &LevelRegistry) -> Vec<LevelInfoView>;

        // --- Level lifecycle (take both handles; clone Rust-side) ---
        fn start_current_level(game: &mut Game, registry: &LevelRegistry);
        fn advance_level(game: &mut Game, registry: &mut LevelRegistry);
        fn select_level(game: &mut Game, registry: &mut LevelRegistry, index: i32);
    }
}

impl From<Vec2> for ffi::Vec2 {
    fn from(value: Vec2) -> Self {
        Self {
            x: value.x,
            y: value.y,
        }
    }
}

impl From<&Vec2> for ffi::Vec2 {
    fn from(value: &Vec2) -> Self {
        (*value).into() // delegate to From<Vec2>
    }
}

impl From<(&Tower, &Map)> for ffi::TowerView {
    fn from((tower, map): (&Tower, &Map)) -> Self {
        Self {
            pos: map.tiles[tower.tile_index].position().into(),
            aim: if let TowerKind::Attack { aim, .. } = tower.kind {
                aim.into()
            } else {
                ffi::Vec2 { x: 0.0, y: 0.0 }
            },
            kind: tower.kind.as_str().into(),
            health: tower.health,
            max_health: tower.max_health,
        }
    }
}

impl From<StatusHint> for ffi::StatusFlags {
    fn from(hint: StatusHint) -> Self {
        Self {
            slow: hint.slow,
            poison: hint.poison,
            regen: hint.regen,
        }
    }
}

impl From<&Enemy> for ffi::EnemyView {
    fn from(enemy: &Enemy) -> Self {
        Self {
            pos: enemy.position().into(),
            half_width: enemy.bounds.width * 0.5,
            half_height: enemy.bounds.height * 0.5,
            kind: enemy.kind.as_str().into(),
            health: enemy.health,
            max_health: enemy.max_health,
            status_hint: enemy.status_hint().into(),
        }
    }
}

impl From<&Bullet> for ffi::BulletView {
    fn from(bullet: &Bullet) -> Self {
        Self {
            pos: bullet.position.into(),
            vel: bullet.velocity.into(),
            kind: bullet.kind.as_str().into(),
            radius: if let BulletKind::Splash { radius, .. } = bullet.kind {
                radius
            } else {
                0.0
            },
        }
    }
}

impl From<&Tile> for ffi::TileView {
    fn from(tile: &Tile) -> Self {
        Self {
            terrain: tile.terrain_name().into(),
        }
    }
}

impl From<&LevelInfo> for ffi::LevelInfoView {
    fn from(info: &LevelInfo) -> Self {
        Self {
            name: info.name.clone(),
            index: info.index,
        }
    }
}

impl From<&TowerConfig> for ffi::TowerCostView {
    fn from(tower: &TowerConfig) -> Self {
        Self {
            kind: tower.kind.as_str().into(),
            cost: tower.resource_cost,
        }
    }
}

impl From<&Path> for ffi::PathWaypointsView {
    fn from(path: &Path) -> Self {
        Self {
            waypoints: path.waypoints.iter().map(Into::into).collect(),
        }
    }
}

impl From<&Level> for ffi::LevelView {
    fn from(level: &Level) -> Self {
        Self {
            name: level.info.name.clone(),
            index: level.info.index,
            map_width: level.map.width,
            map_height: level.map.height,
            terrain: level.map.tiles.iter().map(Into::into).collect(),
            path_waypoints: level.paths.iter().map(Into::into).collect(),
            available_towers: level.available_towers.clone(),
            tower_costs: level
                .available_towers
                .iter()
                .filter_map(|kind| level.tower_configs.get(kind).map(Into::into))
                .collect(),
        }
    }
}

impl From<GameState> for ffi::GameState {
    fn from(state: GameState) -> Self {
        match state {
            GameState::Playing => Self::Playing,
            GameState::Victory => Self::Victory,
            GameState::Defeat => Self::Defeat,
        }
    }
}

impl From<GameResult> for ffi::GameResultView {
    fn from(result: GameResult) -> Self {
        Self {
            cleared: result.cleared,
            cheated: result.cheated,
            time: result.time,
            score: result.score,
        }
    }
}

impl From<GameEvent> for ffi::GameEventView {
    fn from(event: GameEvent) -> Self {
        match event {
            GameEvent::TowerPlaced { kind, pos, cost } => Self {
                kind: "tower_placed".into(),
                pos: pos.into(),
                a: cost,
                type_tag: kind,
                has_boss: false,
                is_last: false,
            },
            GameEvent::EnemyKilled { kind, pos, score } => Self {
                kind: "enemy_killed".into(),
                pos: pos.into(),
                a: score,
                type_tag: kind,
                has_boss: false,
                is_last: false,
            },
            GameEvent::WaveStarted {
                index,
                has_boss,
                is_last,
            } => Self {
                kind: "wave_started".into(),
                pos: ffi::Vec2 { x: 0.0, y: 0.0 },
                a: index,
                type_tag: String::new(),
                has_boss,
                is_last,
            },
        }
    }
}

pub fn new_game(registry: &LevelRegistry) -> Box<Game> {
    Box::new(Game::new(registry.current_level().clone()))
}

pub fn new_registry(config_dir: String) -> Result<Box<LevelRegistry>, Box<dyn std::error::Error>> {
    let mut registry = LevelRegistry::default();
    registry.load_from_dir(std::path::Path::new(&config_dir))?;
    Ok(Box::new(registry))
}

pub fn tick(game: &mut Game, dt: f32) -> bool {
    game.update(dt)
}
pub fn restart(game: &mut Game) {
    game.restart();
}
pub fn pause(game: &mut Game) {
    game.paused = true;
}
pub fn resume(game: &mut Game) {
    game.paused = false;
}
pub fn start_level_json(game: &mut Game, json: String) -> Result<(), Box<dyn std::error::Error>> {
    let level = load_level(
        &json,
        Arc::clone(&game.level.tower_configs),
        Arc::clone(&game.level.enemy_configs),
    )?;
    *game = Game::new(level);
    Ok(())
}

pub fn place_tower(game: &mut Game, kind: String, pos: ffi::Vec2) -> i32 {
    match game.place_tower(&kind, Vec2::new(pos.x, pos.y)) {
        Ok(()) => 0,
        Err(TowerPlaceError::NotPlaceable) => 1,
        Err(TowerPlaceError::NotEnoughResource) => 2,
    }
}
pub fn can_place_at(game: &Game, pos: ffi::Vec2) -> bool {
    let pos = Vec2::new(pos.x, pos.y);
    game.can_place_at(pos)
}
pub fn apply_cheat(game: &mut Game, code: String) {
    game.apply_cheat(&code);
}

pub fn state(game: &Game) -> ffi::GameState {
    game.state.into()
}
pub fn paused(game: &Game) -> bool {
    game.paused
}
pub fn score(game: &Game) -> i32 {
    game.result.score
}
pub fn elapsed_time(game: &Game) -> f32 {
    game.result.time
}
pub fn current_wave(game: &Game) -> i32 {
    game.waves.current_wave_display()
}
pub fn resource_amount(game: &Game) -> i32 {
    game.resource.amount
}
pub fn level_name(game: &Game) -> String {
    game.level.info.name.clone()
}
pub fn level_index(game: &Game) -> i32 {
    game.level.info.index
}
pub fn map_width(game: &Game) -> f32 {
    game.level.map.width
}
pub fn map_height(game: &Game) -> f32 {
    game.level.map.height
}

pub fn tower_views(game: &Game) -> Vec<ffi::TowerView> {
    game.towers
        .values()
        .zip(std::iter::repeat(&game.level.map))
        .map(Into::into)
        .collect()
}
pub fn enemy_views(game: &Game) -> Vec<ffi::EnemyView> {
    game.enemies
        .values()
        .filter(|enemy| !enemy.is_destroyed())
        .map(Into::into)
        .collect()
}
pub fn bullet_views(game: &Game) -> Vec<ffi::BulletView> {
    game.bullets.values().map(Into::into).collect()
}
pub fn level_view(game: &Game) -> ffi::LevelView {
    (&game.level).into()
}
pub fn last_result_view(game: &Game) -> ffi::GameResultView {
    game.result.into()
}
pub fn take_events(game: &mut Game) -> Vec<ffi::GameEventView> {
    game.take_events().into_iter().map(Into::into).collect()
}

pub fn current_level_index(registry: &LevelRegistry) -> i32 {
    registry.current as i32
}
pub fn has_next_official(registry: &LevelRegistry) -> bool {
    registry.has_next_official()
}
pub fn advance(registry: &mut LevelRegistry) {
    registry.advance();
}
pub fn select(registry: &mut LevelRegistry, index: i32) {
    registry.select(index);
}
pub fn registry_size(registry: &LevelRegistry) -> usize {
    registry.levels.len()
}
pub fn registry_infos(registry: &LevelRegistry) -> Vec<ffi::LevelInfoView> {
    registry.infos().iter().map(Into::into).collect()
}
pub fn official_infos(registry: &LevelRegistry) -> Vec<ffi::LevelInfoView> {
    registry.official_infos().iter().map(Into::into).collect()
}

pub fn start_current_level(game: &mut Game, registry: &LevelRegistry) {
    *game = Game::new(registry.current_level().clone());
}
pub fn advance_level(game: &mut Game, registry: &mut LevelRegistry) {
    registry.advance();
    *game = Game::new(registry.current_level().clone());
}
pub fn select_level(game: &mut Game, registry: &mut LevelRegistry, index: i32) {
    registry.select(index);
    *game = Game::new(registry.current_level().clone());
}

#[cfg(test)]
mod tests {
    use super::*; // tower_ffi_view + the `ffi` bridge module
    use crate::bullet::{Bullet, BulletKind};
    use crate::config::{make_enemy, make_tower};
    use crate::geometry::Vec2;
    use crate::test_util::*; // load_stats, load_level_by_name, assert_approx_eq

    #[test]
    fn tower_ffi_view_reflects_a_tower() {
        let (towers, _) = load_stats();
        let level = load_level_by_name("levels/01-meadow.json");
        let tower = make_tower("normal", &towers, 0).unwrap();
        let view = ffi::TowerView::from((&tower, &level.map));
        assert_eq!(view.kind, "normal");
        assert_eq!(view.health, view.max_health);
        // Fresh attack tower aims along +x -> unit vector.
        assert_approx_eq!(view.aim.x * view.aim.x + view.aim.y * view.aim.y, 1.0);
        // pos = tile 0's center.
        assert_approx_eq!(view.pos.x, level.map.tiles[0].position().x);
        assert_approx_eq!(view.pos.y, level.map.tiles[0].position().y);
    }

    #[test]
    fn enemy_ffi_view_reflects_an_enemy() {
        let (_, enemies) = load_stats();
        let level = load_level_by_name("levels/01-meadow.json");
        let enemy = make_enemy("normal", &enemies, 0, &level.paths).unwrap();
        let view = ffi::EnemyView::from(&enemy);
        assert_eq!(view.kind, "normal");
        assert_eq!(view.health, view.max_health);
        assert!(view.half_width > 0.0);
        assert!(view.half_height > 0.0);
        // No status effects active.
        assert!(!view.status_hint.slow && !view.status_hint.poison && !view.status_hint.regen);
    }

    #[test]
    fn bullet_ffi_view_reflects_a_bullet() {
        let bullet = Bullet::new(
            Vec2::new(1.0, 1.0),
            Vec2::new(5.0, 0.0),
            BulletKind::Normal { health_damage: 10 },
        );
        let view = ffi::BulletView::from(&bullet);
        assert_eq!(view.kind, "normal");
        assert_eq!(view.radius, 0.0); // non-splash
        assert_approx_eq!(view.vel.x, 5.0);
    }

    #[test]
    fn level_ffi_view_has_correct_map_and_path_data() {
        let level = load_level_by_name("levels/01-meadow.json");
        let view = ffi::LevelView::from(&level);
        assert!(view.map_width > 0.0);
        assert!(view.map_height > 0.0);
        assert_eq!(
            view.terrain.len(),
            (view.map_width * view.map_height) as usize
        );
        assert!(!view.path_waypoints.is_empty());
        assert!(!view.available_towers.is_empty());
        assert_eq!(view.tower_costs.len(), view.available_towers.len());
        for tc in &view.tower_costs {
            assert!(view.available_towers.contains(&tc.kind));
            assert!(tc.cost > 0);
        }
    }
}
