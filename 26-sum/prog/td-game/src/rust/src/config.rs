use std::{collections::HashMap, sync::Arc};

use serde::{Deserialize, Serialize};
use thiserror::Error;

use crate::{
    bullet::BulletKind,
    enemy::{ChildSpec, Enemy, EnemyKind},
    geometry::{Rect, Vec2},
    level::{Level, LevelInfo},
    map::Map,
    path::Path,
    resource::Resource,
    tile::Tile,
    timer::CountdownTimer,
    tower::{BulletSpec, Targeting, Tower, TowerKind},
    wave::WaveSpec,
};

#[derive(Debug, Error)]
pub enum ConfigError {
    #[error("io: {0}")]
    Io(#[from] std::io::Error),
    #[error("json: {0}")]
    Json(#[from] serde_json::Error),
    #[error("bad config: {0}")]
    BadConfig(String),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TowerConfig {
    pub max_health: i32,
    pub resource_cost: i32,
    #[serde(flatten)]
    pub kind: TowerKindSpec,
}

pub type TowerConfigTable = HashMap<String, TowerConfig>;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TowerAttackSpec {
    pub attack_range: f32,
    pub attack_interval: f32,
    pub targeting: Targeting,
    #[serde(flatten)]
    pub bullet_spec: BulletSpec,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "lowercase")]
pub enum TowerKindSpec {
    Normal {
        #[serde(flatten)]
        attack_spec: TowerAttackSpec,
        health_damage: i32,
    },
    Slow {
        #[serde(flatten)]
        attack_spec: TowerAttackSpec,
        slow_factor: f32,
        slow_duration: f32,
    },
    Poison {
        #[serde(flatten)]
        attack_spec: TowerAttackSpec,
        poison_damage: i32,
        poison_duration: f32,
    },
    Splash {
        #[serde(flatten)]
        attack_spec: TowerAttackSpec,
        radius: f32,
        health_damage: i32,
    },
    Laser {
        #[serde(flatten)]
        attack_spec: TowerAttackSpec,
        width: f32,
        health_damage: i32,
    },
    Resource {
        resource_inc_amount: i32,
        resource_inc_interval: f32,
    },
    Wall,
}

impl TowerKindSpec {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Normal { .. } => "normal",
            Self::Slow { .. } => "slow",
            Self::Poison { .. } => "poison",
            Self::Splash { .. } => "splash",
            Self::Laser { .. } => "laser",
            Self::Resource { .. } => "resource",
            Self::Wall => "wall",
        }
    }
}

impl From<TowerKindSpec> for TowerKind {
    fn from(spec: TowerKindSpec) -> Self {
        let make_attack = |attack_spec: TowerAttackSpec, bullet_kind: BulletKind| Self::Attack {
            attack_range: attack_spec.attack_range,
            targeting: attack_spec.targeting,
            bullet_kind,
            bullet_spec: attack_spec.bullet_spec,
            aim: Vec2::new(1.0, 0.0),
            // Set start_now to false to fire immediately when placed.
            attack_cooldown_timer: CountdownTimer::new(attack_spec.attack_interval, false),
        };

        match spec {
            TowerKindSpec::Normal {
                attack_spec,
                health_damage,
            } => make_attack(attack_spec, BulletKind::Normal { health_damage }),
            TowerKindSpec::Slow {
                attack_spec,
                slow_factor,
                slow_duration,
            } => make_attack(
                attack_spec,
                BulletKind::Slow {
                    factor: slow_factor,
                    duration: slow_duration,
                },
            ),
            TowerKindSpec::Poison {
                attack_spec,
                poison_damage,
                poison_duration,
            } => make_attack(
                attack_spec,
                BulletKind::Poison {
                    damage: poison_damage,
                    duration: poison_duration,
                },
            ),
            TowerKindSpec::Splash {
                attack_spec,
                radius,
                health_damage,
            } => make_attack(
                attack_spec,
                BulletKind::Splash {
                    radius,
                    health_damage,
                },
            ),
            TowerKindSpec::Laser {
                attack_spec,
                width,
                health_damage,
            } => make_attack(
                attack_spec,
                BulletKind::Laser {
                    width,
                    health_damage,
                },
            ),
            TowerKindSpec::Resource {
                resource_inc_amount,
                resource_inc_interval,
            } => Self::Resource {
                resource_inc_amount,
                // Set start_now to false to grant resource immediately when placed.
                resource_inc_cooldown_timer: CountdownTimer::new(resource_inc_interval, false),
            },
            TowerKindSpec::Wall => Self::Wall,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EnemyConfig {
    pub max_health: i32,
    pub speed: f32,
    pub tower_damage: i32,
    pub tower_damage_interval: f32,
    pub width: f32,
    pub height: f32,
    pub score: i32,
    #[serde(flatten)]
    pub kind: EnemyKindSpec,
}

pub type EnemyConfigTable = HashMap<String, EnemyConfig>;

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "lowercase")]
pub enum EnemyKindSpec {
    Normal,
    Fast,
    Armored,
    Resistant {
        slow_resist: f32,
        splash_resist: f32,
    },
    Splitter {
        child: ChildSpec,
    },
    Boss {
        shield: i32,
        regen_amount: i32,
        regen_duration: f32,
        regen_interval: f32,
    },
}

impl EnemyKindSpec {
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

impl From<EnemyKindSpec> for EnemyKind {
    fn from(spec: EnemyKindSpec) -> Self {
        match spec {
            EnemyKindSpec::Normal => EnemyKind::Normal,
            EnemyKindSpec::Fast => EnemyKind::Fast,
            EnemyKindSpec::Armored => EnemyKind::Armored,
            EnemyKindSpec::Resistant {
                slow_resist,
                splash_resist,
            } => EnemyKind::Resistant {
                slow_resist,
                splash_resist,
            },
            EnemyKindSpec::Splitter { child } => EnemyKind::Splitter { child_spec: child },
            EnemyKindSpec::Boss {
                shield,
                regen_amount,
                regen_duration,
                regen_interval,
            } => EnemyKind::Boss {
                shield,
                regen_amount,
                regen_duration,
                regen_interval,
                regen_cooldown_timer: CountdownTimer::new(regen_interval, true),
            },
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LevelConfig {
    #[serde(flatten)]
    pub info: LevelInfo,
    pub map: LevelMapConfig,
    pub paths: Vec<LevelPathConfig>,
    pub starting_resources: i32,
    pub resource_auto_inc_amount: i32,
    pub resource_auto_inc_interval: f32,
    pub available_towers: Vec<String>,
    pub waves: Vec<LevelWaveConfig>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LevelMapConfig {
    pub rows: usize,
    pub cols: usize,
    pub terrain: Vec<Vec<String>>,
}

impl TryFrom<LevelMapConfig> for Map {
    type Error = ConfigError;
    fn try_from(config: LevelMapConfig) -> Result<Self, Self::Error> {
        if config.terrain.len() != config.rows
            || config.terrain.iter().any(|row| row.len() != config.cols)
        {
            return Err(ConfigError::BadConfig("map size does not match".into()));
        }
        if config.terrain.is_empty() || config.terrain.len() * config.terrain[0].len() <= 1 {
            return Err(ConfigError::BadConfig("map size too small".into()));
        }

        let tile_from_terrain = |terrain: &str, center: Vec2| {
            let (
                mut resource_cost_factor,
                mut enemy_speed_factor,
                mut slow_bullet_factor,
                mut placeable,
            ) = (1.0, 1.0, 1.0, true);
            match terrain {
                "grass" | "portal" => {} // defaults; the portal flags set in load_level
                "fertile" => resource_cost_factor = 0.7,
                "rock" => placeable = false,
                "ice" => (enemy_speed_factor, slow_bullet_factor) = (1.5, 0.5),
                _ => {
                    return Err(ConfigError::BadConfig(format!(
                        "unknown terrain \"{}\"",
                        terrain
                    )));
                }
            }
            Ok(Tile::new(
                Rect::new(center, 1.0, 1.0),
                resource_cost_factor,
                placeable,
                enemy_speed_factor,
                slow_bullet_factor,
                false,
            ))
        };

        let mut tiles = vec![];
        for (row_idx, row) in config.terrain.into_iter().enumerate() {
            for (col_idx, terrain) in row.into_iter().enumerate() {
                tiles.push(tile_from_terrain(
                    &terrain,
                    Vec2::new(col_idx as f32 + 0.5, row_idx as f32 + 0.5),
                )?);
            }
        }
        Ok(Self::new(config.cols as f32, config.rows as f32, tiles))
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LevelPathConfig {
    pub waypoints: Vec<(f32, f32)>,
    pub portals: Vec<(usize, usize)>,
}

impl TryFrom<LevelPathConfig> for Path {
    type Error = ConfigError;
    fn try_from(config: LevelPathConfig) -> Result<Self, Self::Error> {
        if config.waypoints.len() <= 1 {
            return Err(ConfigError::BadConfig(
                "at least 2 waypoints required for a path".into(),
            ));
        }
        if config
            .portals
            .iter()
            .flat_map(|(i, j)| [i, j])
            .any(|idx| *idx >= config.waypoints.len())
        {
            return Err(ConfigError::BadConfig(
                "invalid waypoint index for portal".into(),
            ));
        }
        Ok(Self::new(
            config
                .waypoints
                .iter()
                .map(|(x, y)| Vec2::new(*x + 0.5, *y + 0.5))
                .collect(),
            config.portals,
        ))
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EnemySpawnSpec {
    pub kind: String,
    pub time: f32,
    pub path: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LevelWaveConfig {
    pub gap: f32,
    pub spawns: Vec<EnemySpawnSpec>,
}

impl TryFrom<Vec<LevelWaveConfig>> for WaveSpec {
    type Error = ConfigError;
    fn try_from(config: Vec<LevelWaveConfig>) -> Result<Self, Self::Error> {
        if config.is_empty() {
            return Err(ConfigError::BadConfig("empty wave config".into()));
        }
        let (waves, gaps): (Vec<_>, Vec<_>) = config
            .into_iter()
            .map(|wave| {
                (
                    wave.spawns
                        .into_iter()
                        .map(|spawn| (spawn.kind, spawn.time, spawn.path))
                        .collect(),
                    wave.gap,
                )
            })
            .unzip();
        Ok(Self::new(waves, gaps))
    }
}

pub fn load_towers(json: &str) -> Result<TowerConfigTable, ConfigError> {
    let list: Vec<TowerConfig> = serde_json::from_str(json)?;
    list.into_iter()
        .map(|config| Ok((config.kind.as_str().to_owned(), config)))
        .collect()
}

pub fn make_tower(kind: &str, config_table: &TowerConfigTable, tile_index: usize) -> Option<Tower> {
    let config = config_table.get(kind)?;
    Some(Tower::new(
        tile_index,
        config.max_health,
        config.resource_cost,
        config.kind.clone().into(),
    ))
}

pub fn load_enemies(json: &str) -> Result<EnemyConfigTable, ConfigError> {
    let list: Vec<EnemyConfig> = serde_json::from_str(json)?;
    list.into_iter()
        .map(|config| Ok((config.kind.as_str().to_owned(), config)))
        .collect()
}

pub fn make_enemy(
    kind: &str,
    config_table: &EnemyConfigTable,
    path_index: usize,
    paths: &[Path],
) -> Option<Enemy> {
    let config = config_table.get(kind)?;
    let start = paths[path_index].position_at(0.0);
    Some(Enemy::new(
        Rect::new(start, config.width, config.height),
        config.speed,
        config.max_health,
        config.score,
        config.tower_damage,
        config.tower_damage_interval,
        path_index,
        config.kind.clone().into(),
    ))
}

pub fn load_level(
    json: &str,
    tower_configs: Arc<TowerConfigTable>,
    enemy_configs: Arc<EnemyConfigTable>,
) -> Result<Level, ConfigError> {
    let config: LevelConfig = serde_json::from_str(json)?;
    let paths: Vec<Path> = config
        .paths
        .into_iter()
        .map(TryInto::try_into)
        .collect::<Result<_, _>>()?;
    let mut map: Map = config.map.try_into()?;
    let waves: WaveSpec = config.waves.try_into()?;

    for path in &paths {
        for idx in path.portal_pairs.iter().flat_map(|(i, j)| [i, j]) {
            map.tile_at_mut(path.waypoints[*idx])
                .ok_or_else(|| ConfigError::BadConfig("no tile at position".into()))?
                .is_portal = true;
        }
    }

    if waves
        .waves
        .iter()
        .flatten()
        .any(|(_, _, path_idx)| *path_idx >= paths.len())
    {
        return Err(ConfigError::BadConfig(
            "unknown path referenced by wave".into(),
        ));
    }

    Ok(Level {
        info: config.info,
        map,
        paths,
        waves,
        available_towers: config.available_towers,
        resource: Resource::new(
            config.starting_resources,
            config.resource_auto_inc_amount,
            config.resource_auto_inc_interval,
        ),
        enemy_configs,
        tower_configs,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_util::*;

    // --- Tower stats ---

    #[test]
    fn tower_stats_load() {
        let table = load_towers(&read("towers.json")).unwrap();
        for kind in [
            "normal", "slow", "poison", "splash", "laser", "resource", "wall",
        ] {
            assert!(table.contains_key(kind), "missing tower kind: {kind}");
        }
    }

    #[test]
    fn normal_tower_values() {
        let table = load_towers(&read("towers.json")).unwrap();
        let c = &table["normal"];
        assert_eq!(c.max_health, 100);
        assert_eq!(c.resource_cost, 50);
        match &c.kind {
            TowerKindSpec::Normal {
                attack_spec,
                health_damage,
            } => {
                assert_eq!(*health_damage, 15);
                assert_approx_eq!(attack_spec.attack_interval, 0.5);
                assert_approx_eq!(attack_spec.attack_range, 2.5);
                assert_eq!(attack_spec.targeting, Targeting::First);
                assert_approx_eq!(attack_spec.bullet_spec.min_speed, 7.0);
                assert_approx_eq!(attack_spec.bullet_spec.max_speed, 9.0);
            }
            _ => panic!("expected Normal"),
        }
    }

    #[test]
    fn slow_tower_carries_slow_effect() {
        let table = load_towers(&read("towers.json")).unwrap();
        let c = &table["slow"];
        match &c.kind {
            TowerKindSpec::Slow {
                slow_factor,
                slow_duration,
                ..
            } => {
                assert_approx_eq!(*slow_factor, 0.4);
                assert_approx_eq!(*slow_duration, 2.5);
            }
            _ => panic!("expected Slow"),
        }
    }

    #[test]
    fn splash_tower_has_radius_and_closest_targeting() {
        let table = load_towers(&read("towers.json")).unwrap();
        let c = &table["splash"];
        match &c.kind {
            TowerKindSpec::Splash {
                radius,
                attack_spec,
                ..
            } => {
                assert_approx_eq!(*radius, 2.0);
                assert_eq!(attack_spec.targeting, Targeting::Closest);
            }
            _ => panic!("expected Splash"),
        }
    }

    #[test]
    fn wall_has_no_attack_fields() {
        let table = load_towers(&read("towers.json")).unwrap();
        let c = &table["wall"];
        assert_eq!(c.max_health, 400);
        assert_eq!(c.resource_cost, 25);
        assert!(matches!(c.kind, TowerKindSpec::Wall));
    }

    #[test]
    fn resource_tower_values() {
        let table = load_towers(&read("towers.json")).unwrap();
        let c = &table["resource"];
        match &c.kind {
            TowerKindSpec::Resource {
                resource_inc_amount,
                resource_inc_interval,
            } => {
                assert_eq!(*resource_inc_amount, 8);
                assert_approx_eq!(*resource_inc_interval, 2.5);
            }
            _ => panic!("expected Resource"),
        }
    }

    // --- Enemy stats ---

    #[test]
    fn enemy_stats_load() {
        let table = load_enemies(&read("enemies.json")).unwrap();
        for kind in ["normal", "fast", "armored", "resistant", "splitter", "boss"] {
            assert!(table.contains_key(kind), "missing enemy kind: {kind}");
        }
    }

    #[test]
    fn fast_is_fast_and_frail() {
        let table = load_enemies(&read("enemies.json")).unwrap();
        let f = &table["fast"];
        let n = &table["normal"];
        assert!(f.speed > n.speed);
        assert!(f.max_health < n.max_health);
    }

    #[test]
    fn armored_is_slow_and_tanky() {
        let table = load_enemies(&read("enemies.json")).unwrap();
        let a = &table["armored"];
        let n = &table["normal"];
        assert!(a.speed < n.speed);
        assert!(a.max_health > n.max_health);
    }

    #[test]
    fn resistant_has_resist_factors() {
        let table = load_enemies(&read("enemies.json")).unwrap();
        let c = &table["resistant"];
        match &c.kind {
            EnemyKindSpec::Resistant {
                slow_resist,
                splash_resist,
            } => {
                assert!(*slow_resist > 1.0);
                assert!(*splash_resist < 1.0);
            }
            _ => panic!("expected Resistant"),
        }
    }

    #[test]
    fn splitter_has_children() {
        let table = load_enemies(&read("enemies.json")).unwrap();
        let c = &table["splitter"];
        match &c.kind {
            EnemyKindSpec::Splitter { child } => {
                assert!(child.count > 0);
                assert!(child.max_health > 0);
            }
            _ => panic!("expected Splitter"),
        }
    }

    #[test]
    fn boss_has_shield_and_regen() {
        let table = load_enemies(&read("enemies.json")).unwrap();
        let c = &table["boss"];
        match &c.kind {
            EnemyKindSpec::Boss {
                shield,
                regen_amount,
                regen_interval,
                ..
            } => {
                assert!(*shield > 0);
                assert!(*regen_amount > 0);
                assert!(*regen_interval > 0.0);
            }
            _ => panic!("expected Boss"),
        }
    }

    #[test]
    fn score_differs_by_type() {
        let table = load_enemies(&read("enemies.json")).unwrap();
        let n = &table["normal"];
        let b = &table["boss"];
        assert!(n.score > 0);
        assert!(b.score > n.score);
    }

    // --- Level loading ---

    #[test]
    fn level1_loads() {
        let (towers, enemies) = load_stats();
        let level = load_level(&read("levels/01-meadow.json"), towers, enemies).unwrap();

        assert_eq!(level.info.name, "Meadow");
        assert_eq!(level.info.index, 1);
        assert_eq!(level.resource.amount, 150);
        assert!(level.waves.waves.len() >= 5);
        assert!(level.available_towers.len() >= 4);
        assert_approx_eq!(level.map.width, 12.0);
        assert_approx_eq!(level.map.height, 7.0);
        assert!(level.paths.len() == 1);
        assert!(level.paths[0].total_length() > 0.0);
        // Entrance on the right, exit on the left.
        assert!(
            level.paths[0].position_at(0.0).x
                > level.paths[0].position_at(level.paths[0].total_length()).x
        );
    }

    #[test]
    fn level2_has_portals() {
        let (towers, enemies) = load_stats();
        let level = load_level(&read("levels/02-switchback.json"), towers, enemies).unwrap();
        assert!(!level.paths[0].portal_pairs.is_empty());
        assert_eq!(level.paths[0].portal_pairs.len(), 1);
    }

    #[test]
    fn portal_tiles_marked() {
        let (towers, enemies) = load_stats();
        let level = load_level(&read("levels/02-switchback.json"), towers, enemies).unwrap();

        // Portal pair [[1,4]]: waypoint 1 = (11,3) -> world (11.5, 3.5),
        // waypoint 4 = (6,3) -> world (6.5, 3.5). Both tiles must be flagged.
        let src = level.map.tile_at(Vec2::new(11.5, 3.5)).unwrap();
        assert!(src.is_portal);

        let tgt = level.map.tile_at(Vec2::new(6.5, 3.5)).unwrap();
        assert!(tgt.is_portal);

        // A non-portal tile should not be flagged.
        let grass = level.map.tile_at(Vec2::new(0.5, 0.5)).unwrap();
        assert!(!grass.is_portal);
    }

    #[test]
    fn level3_has_ice_terrain() {
        let (towers, enemies) = load_stats();
        let level = load_level(&read("levels/03-glacier.json"), towers, enemies).unwrap();
        // Row 3 cols 4-9 are ice: tile at (4.5, 3.5) should be ice.
        let ice = level.map.tile_at(Vec2::new(4.5, 3.5)).unwrap();
        assert!(ice.enemy_speed_factor > 1.0);
        assert!(ice.slow_bullet_factor < 1.0);
        assert!(ice.can_place_tower()); // ice is placeable
    }

    #[test]
    fn per_wave_gaps_loaded() {
        let (towers, enemies) = load_stats();
        let level = load_level(&read("levels/01-meadow.json"), towers, enemies).unwrap();
        assert!(level.waves.gaps.len() >= 5);
        for g in &level.waves.gaps {
            assert!(*g > 0.0);
        }
        // Gaps are non-uniform.
        let any_diff = level.waves.gaps[1..]
            .iter()
            .any(|g| *g != level.waves.gaps[0]);
        assert!(any_diff);
    }

    /// The showcase level is the one presented for homework demos: it must
    /// exercise every terrain, every tower, and every enemy kind.
    #[test]
    fn showcase_level_covers_everything() {
        let (towers, enemies) = load_stats();
        let level = load_level(&read("levels/showcase.json"), towers, enemies).unwrap();

        // It is a custom (editor-created) level, not an official one.
        assert_eq!(level.info.index, -1);

        // Every tower is available to place.
        for kind in [
            "normal", "slow", "poison", "splash", "laser", "resource", "wall",
        ] {
            assert!(
                level.available_towers.iter().any(|t| t == kind),
                "showcase missing tower: {kind}"
            );
        }

        // Every terrain appears on the map.
        let mut terrains: std::collections::HashSet<&str> = std::collections::HashSet::new();
        for tile in &level.map.tiles {
            terrains.insert(tile.terrain_name());
        }
        for t in ["grass", "fertile", "rock", "ice", "portal"] {
            assert!(terrains.contains(t), "showcase missing terrain: {t}");
        }

        // The path crosses a portal pair.
        assert!(!level.paths[0].portal_pairs.is_empty());

        // Every enemy kind is spawned across the waves.
        let mut spawned: std::collections::HashSet<&str> = std::collections::HashSet::new();
        for wave in &level.waves.waves {
            for (kind, _, _) in wave {
                spawned.insert(kind.as_str());
            }
        }
        for kind in ["normal", "fast", "armored", "resistant", "splitter", "boss"] {
            assert!(spawned.contains(kind), "showcase missing enemy: {kind}");
        }
    }

    #[test]
    fn waves_reference_known_enemy_kinds() {
        let (towers, enemies) = load_stats();
        let level = load_level(
            &read("levels/01-meadow.json"),
            towers.clone(),
            enemies.clone(),
        )
        .unwrap();
        for wave in &level.waves.waves {
            for (kind, _, _) in wave {
                assert!(enemies.contains_key(kind), "unknown enemy kind: {kind}");
            }
        }
    }

    // --- Validation ---

    #[test]
    fn spawn_missing_path_rejected() {
        let (towers, enemies) = load_stats();
        let json = r#"{
            "name":"t","index":-1,
            "map":{"rows":1,"cols":2,"terrain":[["grass","grass"]]},
            "paths":[{"waypoints":[[1,0],[0,0]],"portals":[]}],
            "starting_resources":100,"resource_auto_inc_amount":0,"resource_auto_inc_interval":1.0,
            "available_towers":["normal"],
            "waves":[{"gap":0.0,"spawns":[{"kind":"normal","time":0.0}]}]
        }"#;
        let result = load_level(json, towers, enemies);
        assert!(result.is_err(), "missing path key should be rejected");
    }

    #[test]
    fn spawn_out_of_range_path_rejected() {
        let (towers, enemies) = load_stats();
        let json = r#"{
            "name":"t","index":-1,
            "map":{"rows":1,"cols":2,"terrain":[["grass","grass"]]},
            "paths":[{"waypoints":[[1,0],[0,0]],"portals":[]}],
            "starting_resources":100,"resource_auto_inc_amount":0,"resource_auto_inc_interval":1.0,
            "available_towers":["normal"],
            "waves":[{"gap":0.0,"spawns":[{"kind":"normal","time":0.0,"path":5}]}]
        }"#;
        let result = load_level(json, towers, enemies);
        assert!(result.is_err(), "out-of-range path should be rejected");
    }

    #[test]
    fn portal_out_of_range_rejected() {
        let (towers, enemies) = load_stats();
        let json = r#"{
            "name":"t","index":-1,
            "map":{"rows":1,"cols":3,"terrain":[["grass","grass","grass"]]},
            "paths":[{"waypoints":[[0,0],[1,0],[2,0]],"portals":[[0,9]]}],
            "starting_resources":100,"resource_auto_inc_amount":0,"resource_auto_inc_interval":1.0,
            "available_towers":["normal"],
            "waves":[{"gap":0.0,"spawns":[{"kind":"normal","time":0.0,"path":0}]}]
        }"#;
        let result = load_level(json, towers, enemies);
        assert!(
            result.is_err(),
            "out-of-range portal index should be rejected"
        );
    }

    #[test]
    fn unknown_terrain_rejected() {
        let (towers, enemies) = load_stats();
        let json = r#"{
            "name":"t","index":-1,
            "map":{"rows":1,"cols":2,"terrain":[["grass","bog"]]},
            "paths":[{"waypoints":[[1,0],[0,0]],"portals":[]}],
            "starting_resources":100,"resource_auto_inc_amount":0,"resource_auto_inc_interval":1.0,
            "available_towers":["normal"],
            "waves":[{"gap":0.0,"spawns":[{"kind":"normal","time":0.0,"path":0}]}]
        }"#;
        let result = load_level(json, towers, enemies);
        assert!(result.is_err(), "unknown terrain should be rejected");
    }

    #[test]
    fn empty_waves_rejected() {
        let (towers, enemies) = load_stats();
        let json = r#"{
            "name":"t","index":-1,
            "map":{"rows":1,"cols":2,"terrain":[["grass","grass"]]},
            "paths":[{"waypoints":[[1,0],[0,0]],"portals":[]}],
            "starting_resources":100,"resource_auto_inc_amount":0,"resource_auto_inc_interval":1.0,
            "available_towers":["normal"],
            "waves":[]
        }"#;
        let result = load_level(json, towers, enemies);
        assert!(result.is_err(), "empty waves should be rejected");
    }

    // --- Factory (make_tower / make_enemy) ---
    //
    // The Rust factory is config-driven (it builds towers/enemies from the
    // loaded JSON tables), so these tests assert the built entities match the
    // config values rather than arbitrary stats like the C++ counterpart.

    fn straight_paths() -> Vec<Path> {
        vec![Path::new(
            vec![Vec2::new(0.0, 0.0), Vec2::new(10.0, 0.0)],
            vec![],
        )]
    }

    #[test]
    fn make_tower_builds_each_tower_type() {
        let table = load_towers(&read("towers.json")).unwrap();
        let tile_index = 0;

        let normal = make_tower("normal", &table, tile_index).unwrap();
        assert_eq!(normal.health, 100);
        assert_eq!(normal.max_health, 100);
        assert!(matches!(
            normal.kind,
            TowerKind::Attack {
                bullet_kind: BulletKind::Normal { health_damage: 15 },
                ..
            }
        ));

        let slow = make_tower("slow", &table, tile_index).unwrap();
        match &slow.kind {
            TowerKind::Attack {
                bullet_kind: BulletKind::Slow { factor, duration },
                ..
            } => {
                assert_approx_eq!(*factor, 0.4);
                assert_approx_eq!(*duration, 2.5);
            }
            _ => panic!("expected Slow attack tower"),
        }

        let poison = make_tower("poison", &table, tile_index).unwrap();
        assert!(matches!(
            poison.kind,
            TowerKind::Attack {
                bullet_kind: BulletKind::Poison { damage: 20, .. },
                ..
            }
        ));

        let splash = make_tower("splash", &table, tile_index).unwrap();
        assert!(matches!(
            splash.kind,
            TowerKind::Attack {
                bullet_kind: BulletKind::Splash {
                    radius: 2.0,
                    health_damage: 15
                },
                ..
            }
        ));

        let laser = make_tower("laser", &table, tile_index).unwrap();
        assert!(matches!(
            laser.kind,
            TowerKind::Attack {
                bullet_kind: BulletKind::Laser {
                    width: 0.5,
                    health_damage: 20
                },
                ..
            }
        ));

        let resource = make_tower("resource", &table, tile_index).unwrap();
        assert_eq!(resource.health, 100);
        assert!(matches!(
            resource.kind,
            TowerKind::Resource {
                resource_inc_amount: 8,
                ..
            }
        ));

        let wall = make_tower("wall", &table, tile_index).unwrap();
        assert_eq!(wall.health, 400);
        assert!(matches!(wall.kind, TowerKind::Wall));

        // Unknown kind -> None (the C++ factory throws; Rust returns None).
        assert!(make_tower("bogus", &table, tile_index).is_none());
    }

    #[test]
    fn tile_placement_rules() {
        // grass: placeable; rock: not; portal: not.
        let grass = Tile::new(
            Rect::new(Vec2::new(0.5, 0.5), 1.0, 1.0),
            1.0,
            true,
            1.0,
            1.0,
            false,
        );
        assert!(grass.can_place_tower());

        let rock = Tile::new(
            Rect::new(Vec2::new(1.5, 0.5), 1.0, 1.0),
            1.0,
            false,
            1.0,
            1.0,
            false,
        );
        assert!(!rock.can_place_tower());

        let portal = Tile::new(
            Rect::new(Vec2::new(2.5, 0.5), 1.0, 1.0),
            1.0,
            true,
            1.0,
            1.0,
            true,
        );
        assert!(!portal.can_place_tower());
    }

    #[test]
    fn make_enemy_builds_each_enemy_type_with_stats() {
        let table = load_enemies(&read("enemies.json")).unwrap();
        let paths = straight_paths();

        let normal = make_enemy("normal", &table, 0, &paths).unwrap();
        assert!(matches!(normal.kind, EnemyKind::Normal));
        assert_eq!(normal.health, 25);
        assert_eq!(normal.max_health, 25);
        assert_approx_eq!(normal.path_distance, 0.0);
        assert_approx_eq!(normal.position().x, 0.0); // at path start

        let fast = make_enemy("fast", &table, 0, &paths).unwrap();
        assert!(matches!(fast.kind, EnemyKind::Fast));

        let armored = make_enemy("armored", &table, 0, &paths).unwrap();
        assert!(matches!(armored.kind, EnemyKind::Armored));

        let resistant = make_enemy("resistant", &table, 0, &paths).unwrap();
        assert!(matches!(resistant.kind, EnemyKind::Resistant { .. }));
        assert_approx_eq!(resistant.slow_resist_factor(), 2.0);
        assert_approx_eq!(resistant.splash_damage_factor(), 0.3);

        let splitter = make_enemy("splitter", &table, 0, &paths).unwrap();
        let EnemyKind::Splitter { child_spec } = &splitter.kind else {
            panic!("expected Splitter");
        };
        assert!(child_spec.count > 0);

        // Boss: shield flatly absorbs part of each hit.
        let mut boss = make_enemy("boss", &table, 0, &paths).unwrap();
        assert!(matches!(boss.kind, EnemyKind::Boss { .. }));
        assert_eq!(boss.health, 500);
        boss.decrease_health(5); // fully absorbed by the shield (10)
        assert_eq!(boss.health, 500);
        boss.decrease_health(15); // 15 - 10 shield = 5 through
        assert_eq!(boss.health, 495);

        // Unknown kind -> None.
        assert!(make_enemy("bogus", &table, 0, &paths).is_none());
    }
}
