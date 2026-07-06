//! Rust rewrite of the `game/` model core. Framework-agnostic.

mod bullet;
mod config;
mod enemy;
mod game;
mod geometry;
mod level;
mod map;
mod path;
mod resource;
mod status_effect;
mod tile;
mod timer;
mod tower;
mod wave;

#[cfg(test)]
mod test_util;

#[cfg(feature = "cxx-bridge")]
mod cxxbridge;

#[cfg(feature = "web")]
mod web;

pub use bullet::{Bullet, BulletId, BulletKind};
pub use config::{
    ConfigError, EnemyConfig, EnemyConfigTable, EnemyKindSpec, TowerAttackSpec, TowerConfig,
    TowerConfigTable, TowerKindSpec, load_enemies, load_level, load_towers,
};
pub use enemy::{ChildSpec, Enemy, EnemyId, EnemyKind, StatusHint};
pub use game::{Game, GameEvent, GameResult, GameState, TowerPlaceError};
pub use geometry::{Rect, Vec2};
pub use level::{Level, LevelInfo, LevelRegistry};
pub use map::Map;
pub use path::Path;
pub use resource::Resource;
pub use status_effect::{StatusEffect, StatusEffectKind};
pub use tile::Tile;
pub use tower::{BulletSpec, Targeting, Tower, TowerDamage, TowerId, TowerKind};
pub use wave::{Wave, WaveSpec};
