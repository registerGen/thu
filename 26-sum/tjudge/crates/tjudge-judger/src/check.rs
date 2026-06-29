use std::{
    io::{BufRead, BufReader},
    path::Path,
};

use tjudge_store::schema::TestCaseVerdict;

use crate::sandbox::{Sandbox, SandboxRunConfig, SandboxStatus};

/// Result of running a checker.
#[derive(Debug, Clone)]
pub struct CheckResult {
    pub verdict: TestCaseVerdict,
    /// Human-readable message from the checker (for partial/WA/etc.).
    pub message: Option<String>,
}

// ---------------------------------------------------------------------------
// Token checker (built-in, no sandbox)
// ---------------------------------------------------------------------------

/// Compare expected and actual output token-by-token, ignoring whitespace.
pub fn token_check(answer_path: &Path, output_path: &Path) -> CheckResult {
    let expected = match tokenize_file(answer_path) {
        Ok(t) => t,
        Err(e) => {
            return CheckResult {
                verdict: TestCaseVerdict::InternalError,
                message: Some(format!("failed to read answer file: {e}")),
            };
        }
    };
    let actual = match tokenize_file(output_path) {
        Ok(t) => t,
        Err(e) => {
            return CheckResult {
                verdict: TestCaseVerdict::InternalError,
                message: Some(format!("failed to read output file: {e}")),
            };
        }
    };

    if expected.len() != actual.len() {
        return CheckResult {
            verdict: TestCaseVerdict::WrongAnswer,
            message: Some(format!(
                "number of tokens differ: expected {}, got {}",
                expected.len(),
                actual.len()
            )),
        };
    }

    // Report the first mismatch, if any.
    for (i, (e, a)) in expected.iter().zip(actual.iter()).enumerate() {
        if e != a {
            return CheckResult {
                verdict: TestCaseVerdict::WrongAnswer,
                message: Some(format!(
                    "token differs at position {i}: expected {e}, got {a}"
                )),
            };
        }
    }
    CheckResult {
        verdict: TestCaseVerdict::Accepted,
        message: None,
    }
}

fn tokenize_file(path: &Path) -> std::io::Result<Vec<String>> {
    let f = std::fs::File::open(path)?;
    let reader = BufReader::new(f);
    let mut tokens = Vec::new();
    for line in reader.lines() {
        for tok in line?.split_whitespace() {
            tokens.push(tok.to_string());
        }
    }
    Ok(tokens)
}

// ---------------------------------------------------------------------------
// Testlib checker (run in sandbox)
// ---------------------------------------------------------------------------

/// Testlib standard exit codes.
const EXIT_WA: i32 = 1; // Wrong Answer
const EXIT_PE: i32 = 2; // Presentation Error (treated as WA)
const EXIT_FAIL: i32 = 3; // Checker internal failure → InternalError
const EXIT_POINTS: i32 = 7; // Partially Correct — report file: `<points> [message]`
const EXIT_UNEXPECTED_EOF: i32 = 8; // Unexpected EOF (treated as WA)

/// Run a testlib-style checker inside the sandbox.
///
/// The checker is invoked as:
/// ```text
/// <checker> --testset <test_set_id> <input> <output> <answer> <report>
/// ```
///
/// Verdict is determined by the checker's **exit code**:
/// - `0` → Accepted
/// - `1` / `2` → Wrong Answer (2 = presentation error, treated as WA)
/// - `3` → InternalError (checker `_fail`)
/// - `7` → PartiallyCorrect — report file contains `<points> [message]` (points ∈ [0,1])
/// - other → InternalError
#[allow(clippy::too_many_arguments)]
pub fn testlib_check(
    sandbox: &dyn Sandbox,
    checker_path: &Path,
    test_set_id: &str,
    input_path: &Path,
    output_path: &Path,
    answer_path: &Path,
    timeout_ms: u64,
    memory_limit_kb: u64,
    work_dir: &Path,
    extra_bind_dirs: &[std::path::PathBuf],
) -> CheckResult {
    let report_file = match tempfile::NamedTempFile::new() {
        Ok(f) => f,
        Err(e) => {
            return CheckResult {
                verdict: TestCaseVerdict::InternalError,
                message: Some(format!("could not create checker report file: {e}")),
            };
        }
    };

    let mut bind_mounts: Vec<(std::path::PathBuf, std::path::PathBuf)> = vec![
        (
            checker_path
                .parent()
                .unwrap_or(Path::new("/"))
                .to_path_buf(),
            checker_path
                .parent()
                .unwrap_or(Path::new("/"))
                .to_path_buf(),
        ),
        (
            input_path.parent().unwrap_or(Path::new("/")).to_path_buf(),
            input_path.parent().unwrap_or(Path::new("/")).to_path_buf(),
        ),
        (
            output_path.parent().unwrap_or(Path::new("/")).to_path_buf(),
            output_path.parent().unwrap_or(Path::new("/")).to_path_buf(),
        ),
        (
            answer_path.parent().unwrap_or(Path::new("/")).to_path_buf(),
            answer_path.parent().unwrap_or(Path::new("/")).to_path_buf(),
        ),
        (
            report_file
                .path()
                .parent()
                .unwrap_or(Path::new("/"))
                .to_path_buf(),
            report_file
                .path()
                .parent()
                .unwrap_or(Path::new("/"))
                .to_path_buf(),
        ),
    ];
    for dir in extra_bind_dirs {
        if !bind_mounts.iter().any(|(h, _)| h == dir) {
            bind_mounts.push((dir.clone(), dir.clone()));
        }
    }

    let cfg = SandboxRunConfig {
        command: vec![
            checker_path.to_string_lossy().into_owned(),
            "--testset".into(),
            test_set_id.to_string(),
            input_path.to_string_lossy().into_owned(),
            output_path.to_string_lossy().into_owned(),
            answer_path.to_string_lossy().into_owned(),
            report_file.path().to_string_lossy().into_owned(),
        ],
        work_dir: work_dir.to_path_buf(),
        bind_mounts,
        env: Default::default(),
        time_limit_ms: timeout_ms,
        memory_limit_kb,
        max_processes: 1,
        stdin: None,
        stdout: None,
        stderr: Some(work_dir.join("checker.err")),
    };

    let result = sandbox.run(&cfg);

    match result.status {
        SandboxStatus::Ok => {
            // Exit code 0 → Accepted
            CheckResult {
                verdict: TestCaseVerdict::Accepted,
                message: None,
            }
        }
        SandboxStatus::RuntimeError { exit_code } => match exit_code {
            Some(EXIT_WA) | Some(EXIT_PE) | Some(EXIT_UNEXPECTED_EOF) => {
                let msg = read_report_message(report_file.path());
                CheckResult {
                    verdict: TestCaseVerdict::WrongAnswer,
                    message: msg,
                }
            }
            Some(EXIT_FAIL) => {
                let msg = read_report_message(report_file.path())
                    .unwrap_or_else(|| "checker _fail".into());
                CheckResult {
                    verdict: TestCaseVerdict::InternalError,
                    message: Some(format!("checker _fail: {msg}")),
                }
            }
            Some(EXIT_POINTS) => parse_pc_report(report_file.path()),
            Some(code) => CheckResult {
                verdict: TestCaseVerdict::InternalError,
                message: Some(format!("checker exited with unexpected code {code}")),
            },
            None => CheckResult {
                verdict: TestCaseVerdict::InternalError,
                message: Some("checker killed by signal".into()),
            },
        },
        SandboxStatus::TimeLimitExceeded => CheckResult {
            verdict: TestCaseVerdict::InternalError,
            message: Some("checker time limit exceeded".into()),
        },
        SandboxStatus::MemoryLimitExceeded => CheckResult {
            verdict: TestCaseVerdict::InternalError,
            message: Some("checker memory limit exceeded".into()),
        },
        SandboxStatus::InternalError(msg) => CheckResult {
            verdict: TestCaseVerdict::InternalError,
            message: Some(format!("checker sandbox error: {msg}")),
        },
    }
}

/// Read the first line of the report file as a plain message (for WA / _fail).
fn read_report_message(path: &Path) -> Option<String> {
    let content = std::fs::read_to_string(path).ok()?;
    let line = content.lines().next()?.trim().to_string();
    if line.is_empty() { None } else { Some(line) }
}

/// Parse a partially-correct report written by the checker for exit code 7 (`_points`).
///
/// Format: `<points> [message]` on a single line, where `points` is a float in [0, 1].
fn parse_pc_report(path: &Path) -> CheckResult {
    let content = match std::fs::read_to_string(path) {
        Ok(c) => c,
        Err(e) => {
            return CheckResult {
                verdict: TestCaseVerdict::InternalError,
                message: Some(format!("could not read checker report: {e}")),
            };
        }
    };

    let line = match content.lines().next() {
        Some(l) => l.trim().to_string(),
        None => {
            return CheckResult {
                verdict: TestCaseVerdict::InternalError,
                message: Some("checker produced empty _points report".into()),
            };
        }
    };

    let mut parts = line.splitn(2, char::is_whitespace);
    let score_str = parts.next().unwrap_or("");
    let message = parts
        .next()
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty());

    match score_str.parse::<f64>() {
        Ok(s) if (0.0..=1.0).contains(&s) => {
            let verdict = if (s - 1.0).abs() < f64::EPSILON {
                TestCaseVerdict::Accepted
            } else {
                TestCaseVerdict::PartiallyCorrect(s)
            };
            CheckResult { verdict, message }
        }
        Ok(s) => CheckResult {
            verdict: TestCaseVerdict::InternalError,
            message: Some(format!("checker _points report has out-of-range score {s}")),
        },
        Err(_) => CheckResult {
            verdict: TestCaseVerdict::InternalError,
            message: Some(format!(
                "checker _points report has invalid score: {score_str:?}"
            )),
        },
    }
}
