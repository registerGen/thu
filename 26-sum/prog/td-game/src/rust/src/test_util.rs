//! Test helpers shared across test modules (compiled only under `cfg(test)`).

use std::{path::PathBuf, sync::Arc};

use crate::{
    config::{EnemyConfigTable, TowerConfigTable, load_enemies, load_level, load_towers},
    level::Level,
};

/// Compare two floats (f32 or f64) within an absolute tolerance — the
/// Rust analogue of Catch2's `Approx(...).margin(...)`. Default 1e-5.
macro_rules! assert_approx_eq {
    ($a:expr, $b:expr $(,)?) => {
        assert!(
            (($a as f64) - ($b as f64)).abs() <= 1e-5,
            "{} != {} (within 1e-5)",
            $a,
            $b
        );
    };
    ($a:expr, $b:expr, $eps:expr $(,)?) => {
        assert!(
            (($a as f64) - ($b as f64)).abs() <= $eps as f64,
            "{} != {} (within {})",
            $a,
            $b,
            $eps
        );
    };
}
pub(crate) use assert_approx_eq;

/// Config dir resolved from `CARGO_MANIFEST_DIR` (the crate lives in `rust/`,
/// configs live in the sibling `config/` directory).
pub(crate) fn config_dir() -> PathBuf {
    std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("..")
        .join("config")
}

pub(crate) fn read(name: &str) -> String {
    std::fs::read_to_string(config_dir().join(name)).unwrap()
}

/// Load the tower/enemy config tables, wrapped in `Arc` so they can be shared
/// (read-only) across every level without per-level `HashMap` clones.
pub(crate) fn load_stats() -> (Arc<TowerConfigTable>, Arc<EnemyConfigTable>) {
    (
        Arc::new(load_towers(&read("towers.json")).unwrap()),
        Arc::new(load_enemies(&read("enemies.json")).unwrap()),
    )
}

pub(crate) fn load_level_by_name(name: &str) -> Level {
    let (towers, enemies) = load_stats();
    load_level(&read(name), towers, enemies).unwrap()
}
