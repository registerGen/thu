use std::sync::Arc;

use wasm_bindgen::prelude::*;

use crate::{
    config::load_level,
    game::{Game, GameEvent, GameResult, GameState, TowerPlaceError},
    geometry::Vec2,
    level::{LevelInfo, LevelRegistry},
};

mod ffi {
    use serde::Serialize;
    use tsify::Tsify;

    use crate::{
        bullet::{Bullet, BulletKind},
        config::TowerConfig,
        enemy::{Enemy, StatusHint},
        geometry::Vec2,
        level::{Level, LevelInfo},
        map::Map,
        tower::{Tower, TowerKind},
    };

    #[derive(Clone, Serialize, Tsify)]
    #[tsify(into_wasm_abi)]
    #[serde(rename_all = "camelCase")]
    pub struct TowerView {
        pub pos: Vec2,
        pub aim: Option<Vec2>,
        pub kind: String,
        pub health: i32,
        pub max_health: i32,
    }

    impl From<(&Tower, &Map)> for TowerView {
        fn from((tower, map): (&Tower, &Map)) -> Self {
            Self {
                pos: map.tiles[tower.tile_index].position(),
                aim: if let TowerKind::Attack { aim, .. } = tower.kind {
                    Some(aim)
                } else {
                    None
                },
                kind: tower.kind.as_str().into(),
                health: tower.health,
                max_health: tower.max_health,
            }
        }
    }

    #[derive(Clone, Serialize, Tsify)]
    #[tsify(into_wasm_abi)]
    #[serde(rename_all = "camelCase")]
    pub struct EnemyView {
        pub pos: Vec2,
        pub half_width: f32,
        pub half_height: f32,
        pub kind: String,
        pub health: i32,
        pub max_health: i32,
        pub status_hint: StatusHint,
    }

    impl From<&Enemy> for EnemyView {
        fn from(enemy: &Enemy) -> Self {
            Self {
                pos: enemy.position(),
                half_width: enemy.bounds.width * 0.5,
                half_height: enemy.bounds.height * 0.5,
                kind: enemy.kind.as_str().into(),
                health: enemy.health,
                max_health: enemy.max_health,
                status_hint: enemy.status_hint(),
            }
        }
    }

    #[derive(Clone, Serialize, Tsify)]
    #[tsify(into_wasm_abi)]
    #[serde(rename_all = "camelCase")]
    pub struct BulletView {
        pub pos: Vec2,
        pub vel: Vec2,
        pub kind: String,
        pub radius: Option<f32>,
    }

    impl From<&Bullet> for BulletView {
        fn from(bullet: &Bullet) -> Self {
            Self {
                pos: bullet.position,
                vel: bullet.velocity,
                kind: bullet.kind.as_str().into(),
                radius: if let BulletKind::Splash { radius, .. } = bullet.kind {
                    Some(radius)
                } else {
                    None
                },
            }
        }
    }

    #[derive(Clone, Serialize, Tsify)]
    #[tsify(into_wasm_abi)]
    #[serde(rename_all = "camelCase")]
    pub struct TowerCostView {
        pub kind: String,
        pub cost: i32,
    }

    impl From<&TowerConfig> for TowerCostView {
        fn from(tower: &TowerConfig) -> Self {
            Self {
                kind: tower.kind.as_str().into(),
                cost: tower.resource_cost,
            }
        }
    }

    #[derive(Serialize, Tsify)]
    #[tsify(into_wasm_abi)]
    #[serde(rename_all = "camelCase")]
    pub struct LevelView {
        pub info: LevelInfo,
        pub map_width: f32,
        pub map_height: f32,
        pub terrain: Vec<String>,
        pub path_waypoints: Vec<Vec<Vec2>>,
        pub available_towers: Vec<String>,
        pub tower_costs: Vec<TowerCostView>,
    }

    impl From<&Level> for LevelView {
        fn from(level: &Level) -> Self {
            Self {
                info: level.info.clone(),
                map_width: level.map.width,
                map_height: level.map.height,
                terrain: level
                    .map
                    .tiles
                    .iter()
                    .map(|tile| tile.terrain_name().into())
                    .collect(),
                path_waypoints: level
                    .paths
                    .iter()
                    .map(|path| path.waypoints.clone())
                    .collect(),
                available_towers: level.available_towers.clone(),
                tower_costs: level
                    .available_towers
                    .iter()
                    .filter_map(|kind| level.tower_configs.get(kind).map(Into::into))
                    .collect(),
            }
        }
    }
}

#[wasm_bindgen]
pub struct WebApp {
    game: Game,
    registry: LevelRegistry,
}

#[wasm_bindgen]
impl WebApp {
    /// Load the embedded levels and initialize the first one.
    #[wasm_bindgen(constructor)]
    pub fn new() -> WebApp {
        let mut registry = LevelRegistry::default();
        registry
            .load_from_embedded()
            .expect("embedded levels failed to load");
        let game = Game::new(registry.current_level().clone());
        WebApp { game, registry }
    }

    pub fn tick(&mut self, dt: f32) -> bool {
        self.game.update(dt)
    }
    pub fn restart(&mut self) {
        self.game.restart();
    }
    pub fn pause(&mut self) {
        self.game.paused = true;
    }
    pub fn resume(&mut self) {
        self.game.paused = false;
    }
    #[wasm_bindgen(js_name = startLevelJson)]
    pub fn start_level_json(&mut self, json: String) -> Result<(), String> {
        let level = load_level(
            &json,
            Arc::clone(&self.game.level.tower_configs),
            Arc::clone(&self.game.level.enemy_configs),
        )
        .map_err(|err| err.to_string())?;
        self.game = Game::new(level);
        Ok(())
    }
    #[wasm_bindgen(js_name = validateLevelJson)]
    pub fn validate_level_json(&self, json: String) -> Result<(), String> {
        load_level(
            &json,
            Arc::clone(&self.game.level.tower_configs),
            Arc::clone(&self.game.level.enemy_configs),
        )
        .map_err(|err| err.to_string())?;
        Ok(())
    }

    #[wasm_bindgen(js_name = placeTower)]
    pub fn place_tower(&mut self, kind: String, pos: Vec2) -> Result<(), TowerPlaceError> {
        self.game.place_tower(&kind, pos)
    }
    #[wasm_bindgen(js_name = canPlaceAt)]
    pub fn can_place_at(&self, pos: Vec2) -> bool {
        self.game.can_place_at(pos)
    }
    #[wasm_bindgen(js_name = applyCheat)]
    pub fn apply_cheat(&mut self, code: String) {
        self.game.apply_cheat(&code);
    }

    pub fn state(&self) -> GameState {
        self.game.state
    }
    pub fn paused(&self) -> bool {
        self.game.paused
    }
    pub fn score(&self) -> i32 {
        self.game.result.score
    }
    #[wasm_bindgen(js_name = elapsedTime)]
    pub fn elapsed_time(&self) -> f32 {
        self.game.result.time
    }
    #[wasm_bindgen(js_name = currentWave)]
    pub fn current_wave(&self) -> i32 {
        self.game.waves.current_wave_display()
    }
    #[wasm_bindgen(js_name = resourceAmount)]
    pub fn resource_amount(&self) -> i32 {
        self.game.resource.amount
    }
    #[wasm_bindgen(js_name = levelName)]
    pub fn level_name(&self) -> String {
        self.game.level.info.name.clone()
    }
    #[wasm_bindgen(js_name = levelIndex)]
    pub fn level_index(&self) -> i32 {
        self.game.level.info.index
    }
    #[wasm_bindgen(js_name = mapWidth)]
    pub fn map_width(&self) -> f32 {
        self.game.level.map.width
    }
    #[wasm_bindgen(js_name = mapHeight)]
    pub fn map_height(&self) -> f32 {
        self.game.level.map.height
    }
    #[wasm_bindgen(js_name = towerViews)]
    pub fn tower_views(&self) -> Vec<ffi::TowerView> {
        self.game
            .towers
            .values()
            .zip(std::iter::repeat(&self.game.level.map))
            .map(Into::into)
            .collect()
    }
    #[wasm_bindgen(js_name = enemyViews)]
    pub fn enemy_views(&self) -> Vec<ffi::EnemyView> {
        self.game
            .enemies
            .values()
            .filter(|enemy| !enemy.is_destroyed())
            .map(Into::into)
            .collect()
    }
    #[wasm_bindgen(js_name = bulletViews)]
    pub fn bullet_views(&self) -> Vec<ffi::BulletView> {
        self.game.bullets.values().map(Into::into).collect()
    }
    #[wasm_bindgen(js_name = levelView)]
    pub fn level_view(&self) -> ffi::LevelView {
        (&self.game.level).into()
    }
    #[wasm_bindgen(js_name = lastResult)]
    pub fn last_result(&self) -> GameResult {
        self.game.result
    }
    #[wasm_bindgen(js_name = takeEvents)]
    pub fn take_events(&mut self) -> Vec<GameEvent> {
        self.game.take_events()
    }

    #[wasm_bindgen(js_name = currentLevelIndex)]
    pub fn current_level_index(&self) -> i32 {
        self.registry.current as i32
    }
    #[wasm_bindgen(js_name = hasNextOfficial)]
    pub fn has_next_official(&self) -> bool {
        self.registry.has_next_official()
    }
    #[wasm_bindgen(js_name = advanceLevel)]
    pub fn advance_level(&mut self) {
        self.registry.advance();
    }
    #[wasm_bindgen(js_name = selectLevel)]
    pub fn select_level(&mut self, index: i32) {
        self.registry.select(index);
    }
    #[wasm_bindgen(js_name = registrySize)]
    pub fn registry_size(&self) -> usize {
        self.registry.levels.len()
    }
    #[wasm_bindgen(js_name = registryInfos)]
    pub fn registry_infos(&self) -> Vec<LevelInfo> {
        self.registry.infos()
    }
    #[wasm_bindgen(js_name = officialInfos)]
    pub fn official_infos(&self) -> Vec<LevelInfo> {
        self.registry.official_infos()
    }
    #[wasm_bindgen(js_name = startCurrentLevel)]
    pub fn start_current_level(&mut self) {
        self.game = Game::new(self.registry.current_level().clone());
    }
    #[wasm_bindgen(js_name = advanceAndStartLevel)]
    pub fn advance_and_start_level(&mut self) {
        self.advance_level();
        self.start_current_level();
    }
    #[wasm_bindgen(js_name = selectAndStartLevel)]
    pub fn select_and_start_level(&mut self, index: i32) {
        self.select_level(index);
        self.start_current_level();
    }
}

#[cfg(test)]
mod tests {
    use super::ffi;
    use crate::bullet::{Bullet, BulletKind};
    use crate::config::{make_enemy, make_tower};
    use crate::geometry::Vec2;
    use crate::test_util::*; // load_stats, load_level_by_name, assert_approx_eq

    #[test]
    fn tower_view_reflects_a_tower() {
        let (towers, _) = load_stats();
        let level = load_level_by_name("levels/01-meadow.json");
        let tower = make_tower("normal", &towers, 0).unwrap();
        let view = ffi::TowerView::from((&tower, &level.map));
        assert_eq!(view.kind, "normal");
        assert_eq!(view.health, view.max_health);
        // Fresh attack tower aims along +x -> unit vector (Option<Vec2>).
        let aim = view.aim.expect("attack tower has an aim");
        assert_approx_eq!(aim.x * aim.x + aim.y * aim.y, 1.0);
        // pos = tile 0's center.
        assert_approx_eq!(view.pos.x, level.map.tiles[0].position().x);
        assert_approx_eq!(view.pos.y, level.map.tiles[0].position().y);
    }

    #[test]
    fn enemy_view_reflects_an_enemy() {
        let (_, enemies) = load_stats();
        let level = load_level_by_name("levels/01-meadow.json");
        let enemy = make_enemy("normal", &enemies, 0, &level.paths).unwrap();
        let view = ffi::EnemyView::from(&enemy);
        assert_eq!(view.kind, "normal");
        assert_eq!(view.health, view.max_health);
        assert!(view.half_width > 0.0);
        assert!(view.half_height > 0.0);
        // No status effects active (StatusHint bools, not bitflags).
        assert!(!view.status_hint.slow && !view.status_hint.poison && !view.status_hint.regen);
    }

    #[test]
    fn bullet_view_reflects_a_bullet() {
        let bullet = Bullet::new(
            Vec2::new(1.0, 1.0),
            Vec2::new(5.0, 0.0),
            BulletKind::Normal { health_damage: 10 },
        );
        let view = ffi::BulletView::from(&bullet);
        assert_eq!(view.kind, "normal");
        assert_eq!(view.radius, None); // non-splash -> None
        assert_approx_eq!(view.vel.x, 5.0);
    }

    #[test]
    fn level_view_has_correct_map_and_path_data() {
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
