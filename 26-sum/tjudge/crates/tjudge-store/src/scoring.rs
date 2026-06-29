use std::collections::HashMap;

use crate::schema::{
    ContestConfig, ContestResults, ContestantTaskResult, GroupConfig, TaskConfig, TestCaseVerdict,
    TestSetConfig,
};

/// Score breakdown for a single task.
#[derive(Debug, Clone)]
pub struct TaskScoreBreakdown {
    /// Total score for this task (sum of group scores).
    pub total: f64,
    /// Score per group, ordered by the task's group declaration order.
    pub by_group: Vec<GroupScore>,
}

#[derive(Debug, Clone)]
pub struct GroupScore {
    pub group_id: String,
    /// Actual score earned (≤ `max_score`).
    pub score: f64,
    /// Maximum achievable score for this group.
    pub max_score: f64,
}

/// Score breakdown for a contestant across the whole contest.
#[derive(Debug, Clone)]
pub struct ContestantScoreBreakdown {
    /// Total score across all tasks.
    pub total: f64,
    /// Score per task, keyed by task ID.
    pub by_task: HashMap<String, TaskScoreBreakdown>,
}

/// Returns the relative score in `[0.0, 1.0]` for a single test case verdict.
///
/// - `Accepted` → `1.0`
/// - `PartiallyCorrect(s)` → `s`
/// - everything else (including `Skipped`) → `0.0`
pub fn test_case_relative_score(verdict: &TestCaseVerdict) -> f64 {
    match verdict {
        TestCaseVerdict::Accepted => 1.0,
        TestCaseVerdict::PartiallyCorrect(s) => *s,
        _ => 0.0,
    }
}

/// Returns the relative score `[0.0, 1.0]` for a test set — the minimum
/// relative score across all its test cases.  An empty test set returns `1.0`
/// (vacuous truth; it cannot lower a group's score).
pub fn test_set_relative_score(ts: &TestSetConfig, result: &ContestantTaskResult) -> f64 {
    if ts.test_case_ids.is_empty() {
        return 1.0;
    }
    ts.test_case_ids
        .iter()
        .map(|tc_id| {
            result
                .test_case_results
                .get(tc_id)
                .map(|r| test_case_relative_score(&r.verdict))
                .unwrap_or(0.0)
        })
        .reduce(f64::min)
        .unwrap_or(1.0)
}

/// Returns the score earned for a single group.
///
/// Group score = `max_score` × min(test set relative scores).
/// An empty group (no test sets) earns `0.0`.
pub fn group_score(task: &TaskConfig, group: &GroupConfig, result: &ContestantTaskResult) -> f64 {
    if group.test_set_ids.is_empty() {
        return 0.0;
    }
    let ts_map: HashMap<&str, &TestSetConfig> = task
        .test_sets
        .iter()
        .map(|ts| (ts.id.as_str(), ts))
        .collect();

    let min_ts_score = group
        .test_set_ids
        .iter()
        .map(|ts_id| {
            ts_map
                .get(ts_id.as_str())
                .map(|ts| test_set_relative_score(ts, result))
                .unwrap_or(0.0)
        })
        .reduce(f64::min)
        .unwrap_or(0.0);

    group.max_score * min_ts_score
}

/// Computes the full score breakdown for a contestant's result on one task.
///
/// If `result.compilation_error` is set, all groups score `0` immediately —
/// no test cases were run.
pub fn compute_task_score(task: &TaskConfig, result: &ContestantTaskResult) -> TaskScoreBreakdown {
    if result.compilation_error.is_some() {
        let by_group = task
            .groups
            .iter()
            .map(|g| GroupScore {
                group_id: g.id.clone(),
                score: 0.0,
                max_score: g.max_score,
            })
            .collect();
        return TaskScoreBreakdown {
            total: 0.0,
            by_group,
        };
    }

    let by_group: Vec<GroupScore> = task
        .groups
        .iter()
        .map(|g| GroupScore {
            group_id: g.id.clone(),
            score: group_score(task, g, result),
            max_score: g.max_score,
        })
        .collect();

    let total = by_group.iter().map(|g| g.score).sum();
    TaskScoreBreakdown { total, by_group }
}

/// Computes the score breakdown for a contestant across **all tasks** in the
/// contest.  Tasks for which no result is recorded receive a zero score.
pub fn compute_contestant_score(
    config: &ContestConfig,
    contestant_id: &str,
    results: &ContestResults,
) -> ContestantScoreBreakdown {
    let results_by_task: HashMap<&str, &ContestantTaskResult> = results
        .results
        .iter()
        .filter(|r| r.contestant_id == contestant_id)
        .map(|r| (r.task_id.as_str(), r))
        .collect();

    let by_task: HashMap<String, TaskScoreBreakdown> = config
        .tasks
        .iter()
        .map(|task| {
            let breakdown = if let Some(&r) = results_by_task.get(task.id.as_str()) {
                compute_task_score(task, r)
            } else {
                // No result recorded: treat as all test cases missing (score 0).
                let empty = ContestantTaskResult {
                    contestant_id: contestant_id.to_string(),
                    task_id: task.id.clone(),
                    compilation_error: None,
                    test_case_results: HashMap::new(),
                };
                compute_task_score(task, &empty)
            };
            (task.id.clone(), breakdown)
        })
        .collect();

    let total = by_task.values().map(|t| t.total).sum();
    ContestantScoreBreakdown { total, by_task }
}

#[cfg(test)]
mod tests {
    use std::collections::HashMap;

    use super::*;
    use crate::schema::*;
    use crate::validate::SUPPORTED_SCHEMA_VERSION;

    fn make_task(
        test_cases: &[&str],
        test_sets: &[(&str, &[&str])],
        groups: &[(&str, f64, &[&str])],
    ) -> TaskConfig {
        TaskConfig {
            id: "t1".to_string(),
            title: "Task 1".to_string(),
            workspace_root: "tasks/t1".to_string(),
            checker_config: CheckerConfig::Token,
            submission_config: HashMap::new(),
            library_config: HashMap::new(),
            run_config: HashMap::new(),
            test_cases: test_cases
                .iter()
                .map(|id| TestCaseConfig {
                    id: id.to_string(),
                    input_file: format!("{}.in", id),
                    answer_file: format!("{}.ans", id),
                    time_limit_ms: 1000,
                    memory_limit_kb: 262144,
                })
                .collect(),
            test_sets: test_sets
                .iter()
                .map(|(id, tc_ids)| TestSetConfig {
                    id: id.to_string(),
                    test_case_ids: tc_ids.iter().map(|s| s.to_string()).collect(),
                })
                .collect(),
            groups: groups
                .iter()
                .map(|(id, max, ts_ids)| GroupConfig {
                    id: id.to_string(),
                    max_score: *max,
                    test_set_ids: ts_ids.iter().map(|s| s.to_string()).collect(),
                })
                .collect(),
            file_io: None,
        }
    }

    fn result_with(task_id: &str, verdicts: &[(&str, TestCaseVerdict)]) -> ContestantTaskResult {
        let mut map = HashMap::new();
        for (id, v) in verdicts {
            map.insert(
                id.to_string(),
                TestCaseResult {
                    verdict: v.clone(),
                    time_used_ms: None,
                    memory_used_kb: None,
                    message: None,
                },
            );
        }
        ContestantTaskResult {
            contestant_id: "s1".to_string(),
            task_id: task_id.to_string(),
            compilation_error: None,
            test_case_results: map,
        }
    }

    #[test]
    fn all_accepted_full_score() {
        let task = make_task(
            &["tc1", "tc2"],
            &[("ts1", &["tc1", "tc2"])],
            &[("g1", 100.0, &["ts1"])],
        );
        let result = result_with(
            "t1",
            &[
                ("tc1", TestCaseVerdict::Accepted),
                ("tc2", TestCaseVerdict::Accepted),
            ],
        );
        let breakdown = compute_task_score(&task, &result);
        assert!((breakdown.total - 100.0).abs() < 1e-9);
    }

    #[test]
    fn one_wrong_answer_zeroes_group() {
        let task = make_task(
            &["tc1", "tc2"],
            &[("ts1", &["tc1", "tc2"])],
            &[("g1", 100.0, &["ts1"])],
        );
        let result = result_with(
            "t1",
            &[
                ("tc1", TestCaseVerdict::Accepted),
                ("tc2", TestCaseVerdict::WrongAnswer),
            ],
        );
        let breakdown = compute_task_score(&task, &result);
        assert!((breakdown.total - 0.0).abs() < 1e-9);
    }

    #[test]
    fn partial_score_propagates() {
        // ts1: min(1.0, 0.5) = 0.5; ts2: 1.0 → group = 100 × min(0.5, 1.0) = 50
        let task = make_task(
            &["tc1", "tc2", "tc3"],
            &[("ts1", &["tc1", "tc2"]), ("ts2", &["tc3"])],
            &[("g1", 100.0, &["ts1", "ts2"])],
        );
        let result = result_with(
            "t1",
            &[
                ("tc1", TestCaseVerdict::Accepted),
                ("tc2", TestCaseVerdict::PartiallyCorrect(0.5)),
                ("tc3", TestCaseVerdict::Accepted),
            ],
        );
        let breakdown = compute_task_score(&task, &result);
        assert!((breakdown.total - 50.0).abs() < 1e-9);
    }

    #[test]
    fn missing_result_gives_zero() {
        let task = make_task(&["tc1"], &[("ts1", &["tc1"])], &[("g1", 100.0, &["ts1"])]);
        let result = result_with("t1", &[]); // no test case results
        let breakdown = compute_task_score(&task, &result);
        assert!((breakdown.total - 0.0).abs() < 1e-9);
    }

    #[test]
    fn compilation_error_gives_zero() {
        let task = make_task(&["tc1"], &[("ts1", &["tc1"])], &[("g1", 100.0, &["ts1"])]);
        let mut result = result_with("t1", &[("tc1", TestCaseVerdict::Accepted)]);
        result.compilation_error = Some("syntax error".to_string());
        let breakdown = compute_task_score(&task, &result);
        assert!((breakdown.total - 0.0).abs() < 1e-9);
        assert!(breakdown.by_group.iter().all(|g| g.score == 0.0));
    }

    #[test]
    fn multi_group_score_sums() {
        let task = make_task(
            &["tc1", "tc2"],
            &[("ts1", &["tc1"]), ("ts2", &["tc2"])],
            &[("g1", 50.0, &["ts1"]), ("g2", 50.0, &["ts2"])],
        );
        let result = result_with(
            "t1",
            &[
                ("tc1", TestCaseVerdict::Accepted),
                ("tc2", TestCaseVerdict::Accepted),
            ],
        );
        let breakdown = compute_task_score(&task, &result);
        assert!((breakdown.total - 100.0).abs() < 1e-9);
        assert_eq!(breakdown.by_group.len(), 2);
    }

    #[test]
    fn compute_contestant_score_sums_tasks() {
        let config = ContestConfig {
            meta: ContestMeta {
                id: "c1".to_string(),
                title: "Test Contest".to_string(),
                schema_version: SUPPORTED_SCHEMA_VERSION.to_string(),
            },
            tasks: vec![
                make_task(&["tc1"], &[("ts1", &["tc1"])], &[("g1", 100.0, &["ts1"])]),
                {
                    let mut t =
                        make_task(&["tc2"], &[("ts2", &["tc2"])], &[("g2", 200.0, &["ts2"])]);
                    t.id = "t2".to_string();
                    t
                },
            ],
            contestants: vec![ContestantConfig {
                id: "s1".to_string(),
                name: "Student".to_string(),
                workspace_root: "contestants/s1".to_string(),
                language: HashMap::from([
                    ("t1".to_string(), "cpp".to_string()),
                    ("t2".to_string(), "cpp".to_string()),
                ]),
            }],
        };
        let results = ContestResults {
            results: vec![
                result_with("t1", &[("tc1", TestCaseVerdict::Accepted)]),
                result_with("t2", &[("tc2", TestCaseVerdict::Accepted)]),
            ],
        };
        let breakdown = compute_contestant_score(&config, "s1", &results);
        assert!((breakdown.total - 300.0).abs() < 1e-9);
    }
}
