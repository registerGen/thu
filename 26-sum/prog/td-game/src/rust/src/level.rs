use std::{fs, sync::Arc};

use serde::{Deserialize, Serialize};

use crate::{
    config::{
        ConfigError, EnemyConfigTable, TowerConfigTable, load_enemies, load_level, load_towers,
    },
    map::Map,
    path::Path,
    resource::Resource,
    wave::WaveSpec,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[cfg_attr(feature = "web", derive(tsify::Tsify), tsify(into_wasm_abi))]
pub struct LevelInfo {
    pub name: String,
    /// 1-based for official levels; -1 for custom (editor-created).
    pub index: i32,
}

/// A complete level: map, paths, waves, economy, and the configuration tables
/// shared by towers/enemies via `Arc`.
#[derive(Debug, Clone)]
pub struct Level {
    pub info: LevelInfo,
    pub map: Map,
    pub paths: Vec<Path>,
    pub waves: WaveSpec,
    pub resource: Resource,
    pub available_towers: Vec<String>,
    pub tower_configs: Arc<TowerConfigTable>,
    pub enemy_configs: Arc<EnemyConfigTable>,
}

/// Owns all levels and tracks the current one for progression.
#[derive(Debug, Clone, Default)]
pub struct LevelRegistry {
    pub levels: Vec<Level>,
    pub current: usize,
}

impl LevelRegistry {
    pub fn load_from_dir(&mut self, config_dir: &std::path::Path) -> Result<(), ConfigError> {
        self.load_from_jsons(
            &fs::read_to_string(config_dir.join("towers.json"))?,
            &fs::read_to_string(config_dir.join("enemies.json"))?,
            fs::read_dir(config_dir.join("levels"))
                .into_iter()
                .flatten()
                .filter_map(|entry| {
                    // Silently ignore malformed levels.
                    let path = entry.ok()?.path();
                    if !path.is_file()
                        || path.extension().and_then(|ext| ext.to_str()) != Some("json")
                    {
                        return None;
                    }
                    fs::read_to_string(&path).ok()
                }),
        )
    }

    #[cfg(feature = "web")]
    pub fn load_from_embedded(&mut self) -> Result<(), ConfigError> {
        const TOWERS: &str = include_str!("../../config/towers.json");
        const ENEMIES: &str = include_str!("../../config/enemies.json");
        // Explicit list: `include_str!` takes a literal path, not a glob. Add
        // new level files here as they're created.
        const LEVELS: &[&str] = &[
            include_str!("../../config/levels/01-meadow.json"),
            include_str!("../../config/levels/02-switchback.json"),
            include_str!("../../config/levels/03-glacier.json"),
        ];
        self.load_from_jsons(TOWERS, ENEMIES, LEVELS.iter().copied())
    }

    fn load_from_jsons(
        &mut self,
        towers_json: &str,
        enemies_json: &str,
        level_jsons: impl Iterator<Item = impl AsRef<str>>,
    ) -> Result<(), ConfigError> {
        self.levels.clear();
        self.current = 0;

        let tower_configs = Arc::new(load_towers(towers_json)?);
        let enemy_configs = Arc::new(load_enemies(enemies_json)?);

        let levels: Vec<Level> = level_jsons
            .filter_map(|json| {
                load_level(
                    json.as_ref(),
                    Arc::clone(&tower_configs),
                    Arc::clone(&enemy_configs),
                )
                .ok()
            })
            .collect();

        let (mut officials, mut customs): (Vec<_>, Vec<_>) =
            levels.into_iter().partition(|level| level.info.index >= 1);

        officials.sort_by_key(|level| level.info.index);
        // Drop duplicate official indices (keep the first).
        officials.dedup_by_key(|level| level.info.index);
        customs.sort_by(|lhs, rhs| lhs.info.name.cmp(&rhs.info.name));
        self.levels = officials;
        self.levels.append(&mut customs);

        if self.levels.is_empty() {
            return Err(ConfigError::BadConfig("no levels found".into()));
        }

        Ok(())
    }

    pub fn current_level(&self) -> &Level {
        &self.levels[self.current]
    }

    /// True if the next slot exists AND is an official level (index >= 1).
    pub fn has_next_official(&self) -> bool {
        self.current + 1 < self.levels.len() && self.levels[self.current + 1].info.index >= 1
    }

    /// Advance only if the next official level exists.
    pub fn advance(&mut self) {
        if self.has_next_official() {
            self.current += 1;
        }
    }

    /// Jump to a level by slot (clamped to [0, size)).
    pub fn select(&mut self, index: i32) {
        if self.levels.is_empty() {
            return;
        }
        self.current = index.clamp(0, self.levels.len() as i32 - 1) as usize;
    }

    pub fn infos(&self) -> Vec<LevelInfo> {
        self.levels.iter().map(|level| level.info.clone()).collect()
    }

    pub fn official_infos(&self) -> Vec<LevelInfo> {
        self.levels
            .iter()
            .filter_map(|level| {
                if level.info.index >= 1 {
                    Some(level.info.clone())
                } else {
                    None
                }
            })
            .collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_util::config_dir;
    use std::fs;

    /// A registry loaded from the real config directory.
    fn registry() -> LevelRegistry {
        let mut reg = LevelRegistry::default();
        reg.load_from_dir(&config_dir()).unwrap();
        reg
    }

    #[test]
    fn navigates_and_clamps() {
        let mut reg = registry();
        assert!(reg.levels.len() >= 3);
        assert_eq!(reg.current, 0);
        assert!(reg.has_next_official());

        reg.advance();
        assert_eq!(reg.current, 1);

        let infos = reg.infos();
        let official_infos = reg.official_infos();
        // Official levels come first; any custom levels (index == -1) follow.
        assert_eq!(infos.len(), reg.levels.len());
        assert!(official_infos.len() <= infos.len());
        assert_eq!(&infos[..official_infos.len()], &official_infos[..]);
        for info in &infos[official_infos.len()..] {
            assert_eq!(info.index, -1);
        }
        assert_eq!(infos[1].name, reg.current_level().info.name);

        reg.select(9999); // clamp high -> last
        assert_eq!(reg.current, reg.levels.len() - 1);
        reg.select(-3); // clamp low -> first
        assert_eq!(reg.current, 0);
    }

    #[test]
    fn loads_and_sorts_official_levels_by_index() {
        let reg = registry();
        assert!(reg.levels.len() >= 3);
        assert_eq!(reg.levels[0].info.index, 1);
        assert_eq!(reg.levels[1].info.index, 2);
        assert_eq!(reg.levels[2].info.index, 3);
        assert_eq!(reg.levels[0].info.name, "Meadow");
        assert_eq!(reg.levels[1].info.name, "Switchback");
        assert_eq!(reg.levels[2].info.name, "Glacier");
        // Any custom levels (index == -1) come after the official ones.
        for level in &reg.levels[3..] {
            assert_eq!(level.info.index, -1);
        }
    }

    #[test]
    fn errors_when_no_levels_are_found() {
        // A config dir with stats files but no levels/ directory.
        let tmp = std::env::temp_dir().join("td_empty_levels");
        let _ = fs::remove_dir_all(&tmp);
        fs::create_dir_all(&tmp).unwrap();
        fs::copy(config_dir().join("towers.json"), tmp.join("towers.json")).unwrap();
        fs::copy(config_dir().join("enemies.json"), tmp.join("enemies.json")).unwrap();
        let _ = fs::remove_dir_all(tmp.join("levels"));

        let mut reg = LevelRegistry::default();
        let result = reg.load_from_dir(&tmp);
        assert!(result.is_err(), "a config dir with no levels should error");

        let _ = fs::remove_dir_all(&tmp);
    }
}
