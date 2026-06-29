use std::collections::HashMap;

use tjudge_store::schema::{ContestantTaskResult, TestCaseResult, TestCaseVerdict};

/// A single judging request: judge one contestant's submission for one task.
#[derive(Debug, Clone)]
pub struct JudgeRequest {
    pub contestant_id: String,
    pub task_id: String,
}

/// The outcome of judging a single [`JudgeRequest`].
#[derive(Debug, Clone)]
pub struct JudgeOutcome {
    pub contestant_id: String,
    pub task_id: String,
    /// None if compilation succeeded (or no compilation needed); Some(msg) on failure.
    pub compilation_error: Option<String>,
    /// Per-test-case results, keyed by test case ID.
    pub test_case_results: HashMap<String, TestCaseResult>,
}

impl From<JudgeOutcome> for ContestantTaskResult {
    fn from(o: JudgeOutcome) -> Self {
        ContestantTaskResult {
            contestant_id: o.contestant_id,
            task_id: o.task_id,
            compilation_error: o.compilation_error,
            test_case_results: o.test_case_results,
        }
    }
}

/// Errors that can occur during a judge run.
#[derive(Debug, thiserror::Error)]
pub enum JudgeError {
    #[error("task not found: {0}")]
    TaskNotFound(String),
    #[error("contestant not found: {0}")]
    ContestantNotFound(String),
    #[error("no language configured for contestant {contestant} on task {task}")]
    NoLanguage { contestant: String, task: String },
    #[error("no run config for language {language} on task {task}")]
    NoRunConfig { language: String, task: String },
    #[error("sandbox error: {0}")]
    Sandbox(String),
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
}

/// Convenience result type.
pub type JudgeResult<T> = Result<T, JudgeError>;

/// Events emitted by the judge (`judge_batch`) as judging progresses. One
/// `TestCase` event is emitted for every test case of every request (including
/// skipped / file-error / compilation-error cases), so a consumer can track
/// per-test-case progress; `TaskComplete` carries the task-level
/// `compilation_error` and signals that no more events will arrive for that
/// (contestant, task) pair.
#[derive(Debug, Clone)]
pub enum JudgeEvent {
    TestCase {
        contestant_id: String,
        task_id: String,
        test_case_id: String,
        result: TestCaseResult,
    },
    TaskComplete {
        contestant_id: String,
        task_id: String,
        compilation_error: Option<String>,
    },
    Error(String),
}

/// Constructs a `TestCaseResult` for an internal-error scenario.
pub fn internal_error_result(msg: impl Into<String>) -> TestCaseResult {
    TestCaseResult {
        verdict: TestCaseVerdict::InternalError,
        time_used_ms: None,
        memory_used_kb: None,
        message: Some(msg.into()),
    }
}

/// Constructs a `TestCaseResult` for a skipped test case.
pub fn skipped_result() -> TestCaseResult {
    TestCaseResult {
        verdict: TestCaseVerdict::Skipped,
        time_used_ms: None,
        memory_used_kb: None,
        message: None,
    }
}

/// Constructs a `TestCaseResult` for a missing-file scenario. Scores 0.
pub fn file_error_result(msg: impl Into<String>) -> TestCaseResult {
    TestCaseResult {
        verdict: TestCaseVerdict::FileError,
        time_used_ms: None,
        memory_used_kb: None,
        message: Some(msg.into()),
    }
}
