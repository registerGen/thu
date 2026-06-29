use std::path::Path;

use thiserror::Error;

use crate::schema::{ContestConfig, ContestResults};

#[derive(Debug, Error)]
pub enum StoreError {
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),

    #[error("TOML parse error: {0}")]
    TomlParse(#[from] toml::de::Error),

    #[error("TOML serialize error: {0}")]
    TomlSerialize(#[from] toml::ser::Error),
}

/// Reads and deserializes a [`ContestConfig`] from a TOML file at `path`.
///
/// This does **not** run schema validation; call [`crate::validate`] separately
/// if you need to check referential integrity.
pub fn load_config(path: &Path) -> Result<ContestConfig, StoreError> {
    let text = std::fs::read_to_string(path)?;
    let config: ContestConfig = toml::from_str(&text)?;
    Ok(config)
}

/// Serializes `config` to TOML and writes it to `path`, creating or
/// truncating the file as needed.
pub fn save_config(config: &ContestConfig, path: &Path) -> Result<(), StoreError> {
    let text = toml::to_string_pretty(config)?;
    std::fs::write(path, text)?;
    Ok(())
}

/// Reads and deserializes a [`ContestResults`] from a TOML file at `path`.
///
/// This does **not** run validation; call [`crate::validate_results`] separately
/// if you need to check referential integrity against a [`ContestConfig`].
pub fn load_results(path: &Path) -> Result<ContestResults, StoreError> {
    let text = std::fs::read_to_string(path)?;
    let results: ContestResults = toml::from_str(&text)?;
    Ok(results)
}

/// Serializes `results` to TOML and writes it to `path`, creating or
/// truncating the file as needed.
pub fn save_results(results: &ContestResults, path: &Path) -> Result<(), StoreError> {
    let text = toml::to_string_pretty(results)?;
    std::fs::write(path, text)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::schema::{ContestMeta, ContestantTaskResult};
    use crate::validate::SUPPORTED_SCHEMA_VERSION;

    fn minimal_config() -> ContestConfig {
        ContestConfig {
            meta: ContestMeta {
                id: "c1".to_string(),
                title: "Contest 1".to_string(),
                schema_version: SUPPORTED_SCHEMA_VERSION.to_string(),
            },
            tasks: vec![],
            contestants: vec![],
        }
    }

    fn minimal_results() -> ContestResults {
        ContestResults { results: vec![] }
    }

    #[test]
    fn config_round_trip() {
        let dir = std::env::temp_dir();
        let path = dir.join("tjudge_store_test_config_round_trip.toml");

        let original = minimal_config();
        save_config(&original, &path).expect("save failed");

        let loaded = load_config(&path).expect("load failed");
        assert_eq!(original.meta.id, loaded.meta.id);
        assert_eq!(original.meta.title, loaded.meta.title);
        assert_eq!(original.meta.schema_version, loaded.meta.schema_version);

        std::fs::remove_file(&path).ok();
    }

    #[test]
    fn results_round_trip() {
        let dir = std::env::temp_dir();
        let path = dir.join("tjudge_store_test_results_round_trip.toml");

        let original = ContestResults {
            results: vec![ContestantTaskResult {
                contestant_id: "s1".to_string(),
                task_id: "t1".to_string(),
                compilation_error: Some("error: undeclared id".to_string()),
                test_case_results: std::collections::HashMap::new(),
            }],
        };
        save_results(&original, &path).expect("save_results failed");

        let loaded = load_results(&path).expect("load_results failed");
        assert_eq!(loaded.results.len(), 1);
        assert_eq!(loaded.results[0].contestant_id, "s1");
        assert_eq!(
            loaded.results[0].compilation_error.as_deref(),
            Some("error: undeclared id")
        );

        std::fs::remove_file(&path).ok();
    }

    #[test]
    fn load_missing_file_gives_io_error() {
        let result = load_config(Path::new("/nonexistent/path/contest.toml"));
        assert!(matches!(result, Err(StoreError::Io(_))));
    }

    #[test]
    fn load_invalid_toml_gives_parse_error() {
        let dir = std::env::temp_dir();
        let path = dir.join("tjudge_store_test_invalid.toml");
        std::fs::write(&path, b"not valid toml ][[[").unwrap();

        let result = load_config(&path);
        assert!(matches!(result, Err(StoreError::TomlParse(_))));

        std::fs::remove_file(&path).ok();
    }

    #[test]
    fn empty_results_round_trip() {
        let dir = std::env::temp_dir();
        let path = dir.join("tjudge_store_test_empty_results.toml");

        save_results(&minimal_results(), &path).expect("save_results failed");
        let loaded = load_results(&path).expect("load_results failed");
        assert!(loaded.results.is_empty());

        std::fs::remove_file(&path).ok();
    }
}
