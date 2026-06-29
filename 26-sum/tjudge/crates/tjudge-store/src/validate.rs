use std::collections::{HashMap, HashSet};

use thiserror::Error;

use crate::schema::{ContestConfig, ContestResults, TaskConfig, TestCaseVerdict};

pub const SUPPORTED_SCHEMA_VERSION: &str = "1.0";

#[derive(Debug, Clone, Error)]
pub enum ValidationError {
    #[error("unsupported schema version `{version}` (supported: `{supported}`)")]
    UnsupportedSchemaVersion { version: String, supported: String },

    #[error("duplicate {kind} ID: `{id}`")]
    DuplicateId { kind: &'static str, id: String },

    #[error("unknown {kind} ID `{id}` referenced in {context}")]
    UnknownReference {
        kind: &'static str,
        id: String,
        context: String,
    },

    #[error("partially correct score {score} in {context} is out of range [0, 1]")]
    InvalidPartialScore { score: f64, context: String },

    #[error("max_score {score} for group `{group_id}` in task `{task_id}` must be non-negative")]
    NegativeMaxScore {
        score: f64,
        group_id: String,
        task_id: String,
    },

    #[error("contestant `{contestant_id}` has no language configured for task `{task_id}`")]
    MissingLanguage {
        contestant_id: String,
        task_id: String,
    },

    #[error(
        "language `{language}` for contestant `{contestant_id}` on task `{task_id}` \
         has no run_config"
    )]
    LanguageNotConfigured {
        contestant_id: String,
        task_id: String,
        language: String,
    },

    #[error("run_command for language `{language}` in task `{task_id}` must not be empty")]
    EmptyRunCommand { language: String, task_id: String },

    #[error(
        "compile_command for language `{language}` in task `{task_id}` must not be empty \
         when compilation_config is present"
    )]
    EmptyCompileCommand { language: String, task_id: String },

    #[error(
        "compilation_output_file for language `{language}` in task `{task_id}` must not be \
         empty when compilation_config is present"
    )]
    EmptyCompilationOutputFile { language: String, task_id: String },
}

/// Validates the contest configuration and returns all discovered errors.
/// An empty `Vec` means the configuration is valid.
/// Call [`validate_results`] separately to validate a [`ContestResults`].
pub fn validate_config(config: &ContestConfig) -> Vec<ValidationError> {
    let mut errors = Vec::new();

    if config.meta.schema_version != SUPPORTED_SCHEMA_VERSION {
        errors.push(ValidationError::UnsupportedSchemaVersion {
            version: config.meta.schema_version.clone(),
            supported: SUPPORTED_SCHEMA_VERSION.to_string(),
        });
    }

    collect_unique_ids(
        config.tasks.iter().map(|t| t.id.as_str()),
        "task",
        &mut errors,
    );

    collect_unique_ids(
        config.contestants.iter().map(|c| c.id.as_str()),
        "contestant",
        &mut errors,
    );

    for task in &config.tasks {
        errors.extend(validate_task(task));
    }

    errors
}

/// Validates a [`ContestResults`] against its [`ContestConfig`] and returns all
/// discovered errors.  An empty `Vec` means the results are valid.
pub fn validate_results(config: &ContestConfig, results: &ContestResults) -> Vec<ValidationError> {
    let mut errors = Vec::new();

    let task_ids: HashSet<&str> = config.tasks.iter().map(|t| t.id.as_str()).collect();
    let contestant_ids: HashSet<&str> = config.contestants.iter().map(|c| c.id.as_str()).collect();

    // task_id → set of valid test case IDs
    let task_test_case_ids: HashMap<&str, HashSet<&str>> = config
        .tasks
        .iter()
        .map(|t| {
            let tc_ids = t.test_cases.iter().map(|tc| tc.id.as_str()).collect();
            (t.id.as_str(), tc_ids)
        })
        .collect();

    // contestant_id → ContestantConfig
    let contestant_map: HashMap<&str, &crate::schema::ContestantConfig> = config
        .contestants
        .iter()
        .map(|c| (c.id.as_str(), c))
        .collect();

    // task_id → TaskConfig
    let task_map: HashMap<&str, &crate::schema::TaskConfig> =
        config.tasks.iter().map(|t| (t.id.as_str(), t)).collect();

    let mut result_keys: HashSet<(&str, &str)> = HashSet::new();
    for result in &results.results {
        let key = (result.contestant_id.as_str(), result.task_id.as_str());
        if !result_keys.insert(key) {
            errors.push(ValidationError::DuplicateId {
                kind: "result (contestant_id, task_id)",
                id: format!("({}, {})", result.contestant_id, result.task_id),
            });
        }

        if !contestant_ids.contains(result.contestant_id.as_str()) {
            errors.push(ValidationError::UnknownReference {
                kind: "contestant",
                id: result.contestant_id.clone(),
                context: format!("result for task `{}`", result.task_id),
            });
        }

        if !task_ids.contains(result.task_id.as_str()) {
            errors.push(ValidationError::UnknownReference {
                kind: "task",
                id: result.task_id.clone(),
                context: format!("result for contestant `{}`", result.contestant_id),
            });
        }

        // Validate language configuration (only when both contestant and task are known).
        if let (Some(contestant), Some(task)) = (
            contestant_map.get(result.contestant_id.as_str()),
            task_map.get(result.task_id.as_str()),
        ) {
            match contestant.language.get(&result.task_id) {
                None => errors.push(ValidationError::MissingLanguage {
                    contestant_id: result.contestant_id.clone(),
                    task_id: result.task_id.clone(),
                }),
                Some(lang) => {
                    if !task.run_config.contains_key(lang) {
                        errors.push(ValidationError::LanguageNotConfigured {
                            contestant_id: result.contestant_id.clone(),
                            task_id: result.task_id.clone(),
                            language: lang.clone(),
                        });
                    }
                }
            }
        }

        // Only validate test case results when there is no compilation error;
        // a compilation error means no test cases were run.
        if result.compilation_error.is_none() {
            for (tc_id, tc_result) in &result.test_case_results {
                if let Some(known_tc_ids) = task_test_case_ids.get(result.task_id.as_str())
                    && !known_tc_ids.contains(tc_id.as_str())
                {
                    errors.push(ValidationError::UnknownReference {
                        kind: "test_case",
                        id: tc_id.clone(),
                        context: format!(
                            "result for contestant `{}`, task `{}`",
                            result.contestant_id, result.task_id
                        ),
                    });
                }

                if let TestCaseVerdict::PartiallyCorrect(score) = tc_result.verdict
                    && !(0.0..=1.0).contains(&score)
                {
                    errors.push(ValidationError::InvalidPartialScore {
                        score,
                        context: format!(
                            "contestant `{}`, task `{}`, test case `{}`",
                            result.contestant_id, result.task_id, tc_id
                        ),
                    });
                }
            }
        }
    }

    errors
}

pub fn validate_task(task: &TaskConfig) -> Vec<ValidationError> {
    let mut errors = Vec::new();

    // Validate run_config entries.
    for (language, run_cfg) in &task.run_config {
        if run_cfg.run_command.is_empty() {
            errors.push(ValidationError::EmptyRunCommand {
                language: language.clone(),
                task_id: task.id.clone(),
            });
        }
        if let Some(cc) = &run_cfg.compilation_config {
            if cc.compile_command.is_empty() {
                errors.push(ValidationError::EmptyCompileCommand {
                    language: language.clone(),
                    task_id: task.id.clone(),
                });
            }
            if cc.compilation_output_file.is_empty() {
                errors.push(ValidationError::EmptyCompilationOutputFile {
                    language: language.clone(),
                    task_id: task.id.clone(),
                });
            }
        }
    }

    let tc_ids = collect_unique_ids(
        task.test_cases.iter().map(|tc| tc.id.as_str()),
        "test_case",
        &mut errors,
    );

    let ts_ids = collect_unique_ids(
        task.test_sets.iter().map(|ts| ts.id.as_str()),
        "test_set",
        &mut errors,
    );

    for ts in &task.test_sets {
        for tc_id in &ts.test_case_ids {
            if !tc_ids.contains(tc_id.as_str()) {
                errors.push(ValidationError::UnknownReference {
                    kind: "test_case",
                    id: tc_id.clone(),
                    context: format!("test_set `{}` in task `{}`", ts.id, task.id),
                });
            }
        }
    }

    collect_unique_ids(
        task.groups.iter().map(|g| g.id.as_str()),
        "group",
        &mut errors,
    );

    for group in &task.groups {
        if group.max_score < 0.0 {
            errors.push(ValidationError::NegativeMaxScore {
                score: group.max_score,
                group_id: group.id.clone(),
                task_id: task.id.clone(),
            });
        }

        for ts_id in &group.test_set_ids {
            if !ts_ids.contains(ts_id.as_str()) {
                errors.push(ValidationError::UnknownReference {
                    kind: "test_set",
                    id: ts_id.clone(),
                    context: format!("group `{}` in task `{}`", group.id, task.id),
                });
            }
        }
    }

    errors
}

/// Inserts all items into a `HashSet`, pushing a `DuplicateId` error for each
/// duplicate. Returns the set of unique IDs seen.
pub fn collect_unique_ids<'a>(
    ids: impl Iterator<Item = &'a str>,
    kind: &'static str,
    errors: &mut Vec<ValidationError>,
) -> HashSet<&'a str> {
    let mut seen = HashSet::new();
    for id in ids {
        if !seen.insert(id) {
            errors.push(ValidationError::DuplicateId {
                kind,
                id: id.to_string(),
            });
        }
    }
    seen
}

#[cfg(test)]
mod tests {
    use std::collections::HashMap;

    use super::*;
    use crate::schema::*;

    fn minimal_contest() -> ContestConfig {
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

    fn empty_results() -> ContestResults {
        ContestResults { results: vec![] }
    }

    fn make_task(id: &str) -> TaskConfig {
        TaskConfig {
            id: id.to_string(),
            title: "Task".to_string(),
            workspace_root: format!("tasks/{}", id),
            checker_config: CheckerConfig::Token,
            submission_config: HashMap::new(),
            library_config: HashMap::new(),
            run_config: HashMap::new(),
            test_cases: vec![],
            test_sets: vec![],
            groups: vec![],
            file_io: None,
        }
    }

    // ── ContestConfig validation ──────────────────────────────────────────────

    #[test]
    fn valid_empty_contest_passes() {
        assert!(validate_config(&minimal_contest()).is_empty());
    }

    #[test]
    fn unsupported_schema_version() {
        let mut c = minimal_contest();
        c.meta.schema_version = "99.0".to_string();
        let errs = validate_config(&c);
        assert!(
            errs.iter()
                .any(|e| matches!(e, ValidationError::UnsupportedSchemaVersion { .. }))
        );
    }

    #[test]
    fn duplicate_task_id() {
        let mut c = minimal_contest();
        c.tasks.push(make_task("t1"));
        c.tasks.push(make_task("t1"));
        let errs = validate_config(&c);
        assert!(
            errs.iter()
                .any(|e| matches!(e, ValidationError::DuplicateId { kind: "task", .. }))
        );
    }

    #[test]
    fn test_set_references_unknown_test_case() {
        let mut c = minimal_contest();
        let mut task = make_task("t1");
        task.test_sets.push(TestSetConfig {
            id: "ts1".to_string(),
            test_case_ids: vec!["nonexistent".to_string()],
        });
        c.tasks.push(task);
        let errs = validate_config(&c);
        assert!(errs.iter().any(|e| matches!(
            e,
            ValidationError::UnknownReference {
                kind: "test_case",
                ..
            }
        )));
    }

    #[test]
    fn negative_max_score() {
        let mut c = minimal_contest();
        let mut task = make_task("t1");
        task.groups.push(GroupConfig {
            id: "g1".to_string(),
            test_set_ids: vec![],
            max_score: -10.0,
        });
        c.tasks.push(task);
        let errs = validate_config(&c);
        assert!(
            errs.iter()
                .any(|e| matches!(e, ValidationError::NegativeMaxScore { .. }))
        );
    }

    // ── ContestResults validation ─────────────────────────────────────────────

    #[test]
    fn valid_empty_results_passes() {
        assert!(validate_results(&minimal_contest(), &empty_results()).is_empty());
    }

    #[test]
    fn result_references_unknown_contestant() {
        let c = minimal_contest();
        let results = ContestResults {
            results: vec![ContestantTaskResult {
                contestant_id: "ghost".to_string(),
                task_id: "t1".to_string(),
                compilation_error: None,
                test_case_results: HashMap::new(),
            }],
        };
        let errs = validate_results(&c, &results);
        assert!(errs.iter().any(|e| matches!(
            e,
            ValidationError::UnknownReference {
                kind: "contestant",
                ..
            }
        )));
    }

    #[test]
    fn invalid_partial_score() {
        let mut c = minimal_contest();
        c.contestants.push(ContestantConfig {
            id: "s1".to_string(),
            name: "Student 1".to_string(),
            workspace_root: "contestants/s1".to_string(),
            language: HashMap::from([("t1".to_string(), "cpp".to_string())]),
        });
        let mut task = make_task("t1");
        task.test_cases.push(TestCaseConfig {
            id: "tc1".to_string(),
            input_file: "1.in".to_string(),
            answer_file: "1.ans".to_string(),
            time_limit_ms: 1000,
            memory_limit_kb: 262144,
        });
        c.tasks.push(task);

        let mut tc_results = HashMap::new();
        tc_results.insert(
            "tc1".to_string(),
            TestCaseResult {
                verdict: TestCaseVerdict::PartiallyCorrect(1.5),
                time_used_ms: None,
                memory_used_kb: None,
                message: None,
            },
        );
        let results = ContestResults {
            results: vec![ContestantTaskResult {
                contestant_id: "s1".to_string(),
                task_id: "t1".to_string(),
                compilation_error: None,
                test_case_results: tc_results,
            }],
        };
        let errs = validate_results(&c, &results);
        assert!(
            errs.iter()
                .any(|e| matches!(e, ValidationError::InvalidPartialScore { .. }))
        );
    }

    #[test]
    fn test_case_results_skipped_when_compilation_error() {
        let mut c = minimal_contest();
        c.contestants.push(ContestantConfig {
            id: "s1".to_string(),
            name: "Student 1".to_string(),
            workspace_root: "contestants/s1".to_string(),
            language: HashMap::from([("t1".to_string(), "cpp".to_string())]),
        });
        let mut task = make_task("t1");
        task.run_config.insert(
            "cpp".to_string(),
            RunConfig {
                compilation_config: None,
                run_command: vec!["./solution".to_string()],
                max_processes: 1,
                bind_dirs: vec![],
            },
        );
        c.tasks.push(task);

        // A result with a compilation error and a spurious test case result
        // for a non-existent test case should not produce a validation error.
        let mut tc_results = HashMap::new();
        tc_results.insert(
            "nonexistent_tc".to_string(),
            TestCaseResult {
                verdict: TestCaseVerdict::Skipped,
                time_used_ms: None,
                memory_used_kb: None,
                message: None,
            },
        );
        let results = ContestResults {
            results: vec![ContestantTaskResult {
                contestant_id: "s1".to_string(),
                task_id: "t1".to_string(),
                compilation_error: Some("error: undeclared identifier".to_string()),
                test_case_results: tc_results,
            }],
        };
        assert!(validate_results(&c, &results).is_empty());
    }

    // ── RunConfig / CompilationConfig validation ──────────────────────────────

    fn make_run_config(
        compile_command: Option<Vec<String>>,
        compile_output: Option<&str>,
        run_command: Vec<String>,
    ) -> RunConfig {
        RunConfig {
            compilation_config: compile_command.map(|cmd| CompilationConfig {
                compile_command: cmd,
                compilation_timeout_ms: 10000,
                compilation_memory_limit_kb: 1024 * 1024,
                compilation_output_file: compile_output.unwrap_or("").to_string(),
            }),
            run_command,
            max_processes: 1,
            bind_dirs: vec![],
        }
    }

    #[test]
    fn valid_run_config_passes() {
        let mut c = minimal_contest();
        let mut task = make_task("t1");
        task.run_config.insert(
            "cpp".to_string(),
            make_run_config(
                Some(vec![
                    "g++".to_string(),
                    "sol.cpp".to_string(),
                    "-o".to_string(),
                    "sol".to_string(),
                ]),
                Some("sol"),
                vec!["./sol".to_string()],
            ),
        );
        c.tasks.push(task);
        assert!(validate_config(&c).is_empty());
    }

    #[test]
    fn empty_run_command_errors() {
        let mut c = minimal_contest();
        let mut task = make_task("t1");
        task.run_config
            .insert("cpp".to_string(), make_run_config(None, None, vec![]));
        c.tasks.push(task);
        let errs = validate_config(&c);
        assert!(
            errs.iter()
                .any(|e| matches!(e, ValidationError::EmptyRunCommand { .. }))
        );
    }

    #[test]
    fn empty_compile_command_errors() {
        let mut c = minimal_contest();
        let mut task = make_task("t1");
        task.run_config.insert(
            "cpp".to_string(),
            make_run_config(Some(vec![]), Some("sol"), vec!["./sol".to_string()]),
        );
        c.tasks.push(task);
        let errs = validate_config(&c);
        assert!(
            errs.iter()
                .any(|e| matches!(e, ValidationError::EmptyCompileCommand { .. }))
        );
    }

    #[test]
    fn empty_compilation_output_file_errors() {
        let mut c = minimal_contest();
        let mut task = make_task("t1");
        task.run_config.insert(
            "cpp".to_string(),
            make_run_config(
                Some(vec!["g++".to_string(), "sol.cpp".to_string()]),
                None, // empty output file
                vec!["./sol".to_string()],
            ),
        );
        c.tasks.push(task);
        let errs = validate_config(&c);
        assert!(
            errs.iter()
                .any(|e| matches!(e, ValidationError::EmptyCompilationOutputFile { .. }))
        );
    }

    #[test]
    fn no_compilation_config_is_valid() {
        // Interpreted language: no compilation, just a run command.
        let mut c = minimal_contest();
        let mut task = make_task("t1");
        task.run_config.insert(
            "python".to_string(),
            make_run_config(
                None,
                None,
                vec!["python3".to_string(), "solution.py".to_string()],
            ),
        );
        c.tasks.push(task);
        assert!(validate_config(&c).is_empty());
    }

    // ── Language validation ───────────────────────────────────────────────────

    #[test]
    fn missing_language_for_task_errors() {
        let mut c = minimal_contest();
        c.contestants.push(ContestantConfig {
            id: "s1".to_string(),
            name: "Student".to_string(),
            workspace_root: "contestants/s1".to_string(),
            language: HashMap::new(), // no language for any task
        });
        c.tasks.push(make_task("t1"));
        let results = ContestResults {
            results: vec![ContestantTaskResult {
                contestant_id: "s1".to_string(),
                task_id: "t1".to_string(),
                compilation_error: None,
                test_case_results: HashMap::new(),
            }],
        };
        let errs = validate_results(&c, &results);
        assert!(
            errs.iter()
                .any(|e| matches!(e, ValidationError::MissingLanguage { .. }))
        );
    }

    #[test]
    fn language_not_in_run_config_errors() {
        let mut c = minimal_contest();
        c.contestants.push(ContestantConfig {
            id: "s1".to_string(),
            name: "Student".to_string(),
            workspace_root: "contestants/s1".to_string(),
            language: HashMap::from([("t1".to_string(), "java".to_string())]),
        });
        c.tasks.push(make_task("t1")); // task has no run_config for "java"
        let results = ContestResults {
            results: vec![ContestantTaskResult {
                contestant_id: "s1".to_string(),
                task_id: "t1".to_string(),
                compilation_error: None,
                test_case_results: HashMap::new(),
            }],
        };
        let errs = validate_results(&c, &results);
        assert!(
            errs.iter()
                .any(|e| matches!(e, ValidationError::LanguageNotConfigured { .. }))
        );
    }
}
