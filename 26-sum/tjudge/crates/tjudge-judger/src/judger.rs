use std::{
    collections::{HashMap, HashSet},
    path::PathBuf,
    sync::Arc,
    sync::atomic::{AtomicBool, Ordering},
};

use rayon::prelude::*;
use tjudge_store::{
    ContestPaths,
    schema::{CheckerConfig, ContestConfig, TestCaseVerdict},
};

use crate::{
    check::{testlib_check, token_check},
    sandbox::{JudgerSandbox, Sandbox, SandboxRunConfig, SandboxStatus},
    types::{
        JudgeError, JudgeEvent, JudgeOutcome, JudgeRequest, file_error_result,
        internal_error_result, skipped_result,
    },
};

/// Maximum number of concurrent judging threads. Each request (one
/// contestant × one task) runs on its own thread, so this bounds the number of
/// sandboxes / submissions running at once.
const MAX_JUDGE_THREADS: usize = 8;

/// The main judging engine.
///
/// Holds an immutable reference to the contest configuration and paths.
/// Call [`Judger::judge_batch`] to judge a list of requests in parallel.
pub struct Judger {
    config: Arc<ContestConfig>,
    paths: Arc<ContestPaths>,
    /// A dedicated rayon pool capped at [`MAX_JUDGE_THREADS`] so judging never
    /// spawns more than that many worker threads, regardless of CPU count.
    pool: rayon::ThreadPool,
}

impl Judger {
    pub fn new(config: ContestConfig, paths: ContestPaths) -> Self {
        let pool = rayon::ThreadPoolBuilder::new()
            .num_threads(MAX_JUDGE_THREADS)
            .build()
            .expect("failed to build judge thread pool");
        Self {
            config: Arc::new(config),
            paths: Arc::new(paths),
            pool,
        }
    }

    /// Judge a batch of requests in parallel, emitting [`JudgeEvent`]s to
    /// `emit` as each test case completes. Events flow concurrently from all
    /// worker threads, so a consumer can show per-test-case progress.
    ///
    /// `cancel` is polled before each request and between test cases; setting it
    /// stops dispatching new work (already-running test cases finish).
    pub fn judge_batch<F>(&self, requests: Vec<JudgeRequest>, cancel: Arc<AtomicBool>, emit: F)
    where
        F: Fn(JudgeEvent) + Sync + Send,
    {
        // Run the parallel iteration on this judger's bounded pool rather than
        // the global rayon pool, capping concurrency at MAX_JUDGE_THREADS.
        self.pool.install(|| {
            requests.into_par_iter().for_each(|req| {
                if cancel.load(Ordering::Relaxed) {
                    return;
                }
                self.judge_one(&req, &cancel, &emit);
            });
        });
    }

    /// Judge a single request, emitting [`JudgeEvent`]s — one `TestCase` per
    /// test case plus a final `TaskComplete` — instead of returning an outcome.
    fn judge_one<E: Fn(JudgeEvent)>(&self, req: &JudgeRequest, cancel: &AtomicBool, emit: &E) {
        if cancel.load(Ordering::Relaxed) {
            return;
        }
        let cid = req.contestant_id.clone();
        let tid = req.task_id.clone();

        let task = match self.config.task(&req.task_id) {
            Some(t) => t,
            None => {
                emit(JudgeEvent::Error(format!(
                    "task not found: {}",
                    req.task_id
                )));
                return;
            }
        };
        let contestant = match self.config.contestant(&req.contestant_id) {
            Some(c) => c,
            None => {
                emit(JudgeEvent::Error(format!(
                    "contestant not found: {}",
                    req.contestant_id
                )));
                return;
            }
        };
        let language = match contestant.language.get(&task.id) {
            Some(l) => l,
            None => {
                let msg = format!("no language configured for contestant '{cid}' on task '{tid}'");
                for tc in &task.test_cases {
                    emit(JudgeEvent::TestCase {
                        contestant_id: cid.clone(),
                        task_id: tid.clone(),
                        test_case_id: tc.id.clone(),
                        result: internal_error_result(msg.clone()),
                    });
                }
                emit(JudgeEvent::TaskComplete {
                    contestant_id: cid,
                    task_id: tid,
                    compilation_error: None,
                });
                return;
            }
        };
        let run_cfg = match task.run_config.get(language) {
            Some(cfg) => cfg,
            None => {
                let msg = format!(
                    "no run configuration for language '{language}' on task '{}'",
                    task.id
                );
                for tc in &task.test_cases {
                    emit(JudgeEvent::TestCase {
                        contestant_id: cid.clone(),
                        task_id: tid.clone(),
                        test_case_id: tc.id.clone(),
                        result: internal_error_result(msg.clone()),
                    });
                }
                emit(JudgeEvent::TaskComplete {
                    contestant_id: cid,
                    task_id: tid,
                    compilation_error: None,
                });
                return;
            }
        };

        // Missing submission source -> FileError on every test case.
        let submission_files = self
            .paths
            .submission_files(contestant, task)
            .unwrap_or_default();
        if let Some(missing) = submission_files.iter().find(|p| !p.exists()) {
            let msg = format!("submission file not found: {}", missing.display());
            for tc in &task.test_cases {
                emit(JudgeEvent::TestCase {
                    contestant_id: cid.clone(),
                    task_id: tid.clone(),
                    test_case_id: tc.id.clone(),
                    result: file_error_result(msg.clone()),
                });
            }
            emit(JudgeEvent::TaskComplete {
                contestant_id: cid,
                task_id: tid,
                compilation_error: None,
            });
            return;
        }

        let sandbox = match JudgerSandbox::new() {
            Ok(s) => s,
            Err(e) => {
                emit(JudgeEvent::Error(format!("sandbox error: {e}")));
                return;
            }
        };
        let box_work_dir = sandbox.box_dir().join("box");

        match self.judge_with_sandbox(
            req,
            task,
            contestant,
            run_cfg,
            &sandbox,
            &box_work_dir,
            cancel,
            emit,
        ) {
            Ok(o) => emit(JudgeEvent::TaskComplete {
                contestant_id: cid,
                task_id: tid,
                compilation_error: o.compilation_error,
            }),
            Err(e) => emit(JudgeEvent::Error(e.to_string())),
        }
    }

    /// Builds an outcome where every test case is a `FileError` (0 pts), e.g.
    /// when a task-level required file (submission source, library file) is
    /// missing.
    fn all_file_error_outcome(
        req: &JudgeRequest,
        task: &tjudge_store::schema::TaskConfig,
        msg: String,
    ) -> JudgeOutcome {
        let test_case_results = task
            .test_cases
            .iter()
            .map(|tc| (tc.id.clone(), file_error_result(msg.clone())))
            .collect();
        JudgeOutcome {
            contestant_id: req.contestant_id.clone(),
            task_id: req.task_id.clone(),
            compilation_error: None,
            test_case_results,
        }
    }

    /// Core judging logic, separated from sandbox creation so tests can inject a mock.
    #[allow(clippy::too_many_arguments)]
    pub(crate) fn judge_with_sandbox<E: Fn(JudgeEvent)>(
        &self,
        req: &JudgeRequest,
        task: &tjudge_store::schema::TaskConfig,
        contestant: &tjudge_store::schema::ContestantConfig,
        run_cfg: &tjudge_store::schema::RunConfig,
        sandbox: &dyn Sandbox,
        box_work_dir: &std::path::Path,
        cancel: &AtomicBool,
        emit: &E,
    ) -> Result<JudgeOutcome, JudgeError> {
        let box_work_dir = box_work_dir.to_path_buf();
        let language = contestant
            .language
            .get(&task.id)
            .ok_or_else(|| JudgeError::NoLanguage {
                contestant: req.contestant_id.clone(),
                task: req.task_id.clone(),
            })?;

        // Collect submission files into the box
        let submission_files = self
            .paths
            .submission_files(contestant, task)
            .unwrap_or_default();
        for src in &submission_files {
            let dst = box_work_dir.join(src.file_name().unwrap_or_default());
            std::fs::copy(src, &dst)?;
        }

        // Copy library files into the box. If any library file is missing,
        // judge the whole task as 0 (FileError) rather than failing.
        if let Some(lib_files) = self.paths.library_files(task, language) {
            if let Some(missing) = lib_files.iter().find(|p| !p.exists()) {
                let msg = format!("library file not found: {}", missing.display());
                for tc in &task.test_cases {
                    emit(JudgeEvent::TestCase {
                        contestant_id: req.contestant_id.clone(),
                        task_id: req.task_id.clone(),
                        test_case_id: tc.id.clone(),
                        result: file_error_result(msg.clone()),
                    });
                }
                return Ok(Self::all_file_error_outcome(req, task, msg));
            }
            for src in &lib_files {
                let dst = box_work_dir.join(src.file_name().unwrap_or_default());
                std::fs::copy(src, &dst)?;
            }
        }

        // ── Compilation ───────────────────────────────────────────────────────

        let binary_path: Option<PathBuf>;

        if let Some(comp_cfg) = &run_cfg.compilation_config {
            let compile_stderr = box_work_dir.join("compile.err");
            let compile_result = sandbox.run(&SandboxRunConfig {
                command: comp_cfg.compile_command.clone(),
                work_dir: box_work_dir.clone(),
                bind_mounts: std::iter::once((box_work_dir.clone(), box_work_dir.clone()))
                    .chain(run_cfg.bind_dirs.iter().map(|d| (d.clone(), d.clone())))
                    .collect(),
                env: {
                    let mut e = HashMap::new();
                    e.insert(
                        "PATH".into(),
                        "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin".into(),
                    );
                    // Redirect temp files into the box so g++ doesn't need /tmp
                    e.insert("TMPDIR".into(), box_work_dir.to_string_lossy().into_owned());
                    e
                },
                time_limit_ms: comp_cfg.compilation_timeout_ms,
                memory_limit_kb: comp_cfg.compilation_memory_limit_kb,
                max_processes: 8,
                stdin: None,
                stdout: None,
                stderr: Some(compile_stderr.clone()),
            });

            if compile_result.status != SandboxStatus::Ok {
                let detail = std::fs::read_to_string(&compile_stderr)
                    .ok()
                    .filter(|s| !s.trim().is_empty())
                    .map(|s| {
                        // Keep last 10 lines so the message stays reasonable
                        let lines: Vec<&str> = s.lines().collect();
                        lines[lines.len().saturating_sub(10)..].join("\n")
                    });
                let msg = match detail {
                    Some(d) => d,
                    None => format!("compilation failed: {:?}", compile_result.status),
                };
                for tc in &task.test_cases {
                    emit(JudgeEvent::TestCase {
                        contestant_id: req.contestant_id.clone(),
                        task_id: req.task_id.clone(),
                        test_case_id: tc.id.clone(),
                        result: skipped_result(),
                    });
                }
                return Ok(JudgeOutcome {
                    contestant_id: req.contestant_id.clone(),
                    task_id: req.task_id.clone(),
                    compilation_error: Some(msg),
                    test_case_results: HashMap::new(),
                });
            }

            binary_path = Some(box_work_dir.join(&comp_cfg.compilation_output_file));
        } else {
            binary_path = None;
        }

        // ── Run each test case ────────────────────────────────────────────────

        // Map from test_case_id → test_set_id for fast lookup.
        let tc_to_ts: HashMap<&str, &str> = task
            .test_sets
            .iter()
            .flat_map(|ts| {
                ts.test_case_ids
                    .iter()
                    .map(move |tc_id| (tc_id.as_str(), ts.id.as_str()))
            })
            .collect();

        let mut test_case_results = HashMap::new();
        // Test sets whose score has already dropped to 0 — remaining test cases
        // in them are skipped, since they cannot raise the set's minimum score.
        // (This is scoped per test set; a failure in one set never skips cases
        // in another, and a TLE does not globally skip the rest of the task.)
        let mut failed_test_sets: HashSet<String> = HashSet::new();

        for tc in &task.test_cases {
            // Stop dispatching new test cases if judging was cancelled.
            if cancel.load(Ordering::Relaxed) {
                break;
            }

            let ts_id = tc_to_ts.get(tc.id.as_str()).copied();

            // Skip remaining cases of a test set that has already scored 0.
            if ts_id
                .map(|id| failed_test_sets.contains(id))
                .unwrap_or(false)
            {
                let result = skipped_result();
                emit(JudgeEvent::TestCase {
                    contestant_id: req.contestant_id.clone(),
                    task_id: req.task_id.clone(),
                    test_case_id: tc.id.clone(),
                    result: result.clone(),
                });
                test_case_results.insert(tc.id.clone(), result);
                continue;
            }

            let input_path = self.paths.test_case_input(task, tc);
            let answer_path = self.paths.test_case_answer(task, tc);

            // If the input or answer file is missing, record a FileError
            // (0 pts) for this test case and skip running it.
            if !input_path.exists() || !answer_path.exists() {
                let missing = if !input_path.exists() {
                    &input_path
                } else {
                    &answer_path
                };
                let result = file_error_result(format!("file not found: {}", missing.display()));
                if let Some(id) = ts_id {
                    failed_test_sets.insert(id.to_string());
                }
                emit(JudgeEvent::TestCase {
                    contestant_id: req.contestant_id.clone(),
                    task_id: req.task_id.clone(),
                    test_case_id: tc.id.clone(),
                    result: result.clone(),
                });
                test_case_results.insert(tc.id.clone(), result);
                continue;
            }

            // Resolve where the submission reads input from and writes output to.
            // The input file is *copied* into the sandbox (rather than bind-
            // mounting its host directory) so the solution only ever sees that
            // one file, not the rest of the test-data directory.
            //
            // File-I/O tasks: copy the test input into the box under the
            // configured input filename and let the submission write the output
            // filename (e.g. `freopen("digits.in"/"digits.out")`).
            // Stdin/stdout tasks: copy the input into the box and feed it via
            // stdin, capturing stdout.
            //
            // stdin/stdout are always wired to a real file (never `None`, which
            // isolate would inherit from the controlling terminal) so a solution
            // that reads stdin or writes stdout cannot block forever.
            let (stdin_path, stdout_path, output_path) = match &task.file_io {
                Some(io) => {
                    let io_in = box_work_dir.join(&io.input_file);
                    let io_out = box_work_dir.join(&io.output_file);
                    std::fs::copy(&input_path, &io_in)?;
                    let _ = std::fs::remove_file(&io_out);
                    // Feed stdin from the in-box copy (file-I/O solutions ignore
                    // it after freopen) and capture stdout to a discard file
                    // (file-I/O solutions write `io_out` via freopen). The
                    // checker reads `io_out`.
                    let discard = box_work_dir.join(format!("{}.stdout", tc.id));
                    (Some(io_in), Some(discard), io_out)
                }
                None => {
                    let in_box = box_work_dir.join(format!("{}.in", tc.id));
                    std::fs::copy(&input_path, &in_box)?;
                    let out = box_work_dir.join(format!("{}.out", tc.id));
                    (Some(in_box), Some(out.clone()), out)
                }
            };

            // Build environment
            let mut env: HashMap<String, String> = HashMap::new();
            if let Some(bin) = &binary_path {
                env.insert(
                    "TJUDGE_BINARY".to_string(),
                    bin.to_string_lossy().into_owned(),
                );
            }
            // TJUDGE_INPUT points at the in-box copy of the input (absolute path
            // inside the sandbox, which is bind-mounted below).
            if let Some(stdin) = &stdin_path {
                env.insert(
                    "TJUDGE_INPUT".to_string(),
                    stdin.to_string_lossy().into_owned(),
                );
            }
            env.insert(
                "TJUDGE_OUTPUT".to_string(),
                output_path.to_string_lossy().into_owned(),
            );

            // Only the box working directory is mounted; the input file's host
            // directory is intentionally not exposed to the submission.
            let mut bind_mounts = vec![(box_work_dir.clone(), box_work_dir.clone())];

            // Extra dirs requested by the run config (e.g. /usr, /lib for interpreters)
            for dir in &run_cfg.bind_dirs {
                if !bind_mounts.iter().any(|(h, _)| h == dir) {
                    bind_mounts.push((dir.clone(), dir.clone()));
                }
            }

            let stderr_path = box_work_dir.join(format!("{}.err", tc.id));

            let run_result = sandbox.run(&SandboxRunConfig {
                command: run_cfg.run_command.clone(),
                work_dir: box_work_dir.clone(),
                bind_mounts,
                env,
                time_limit_ms: tc.time_limit_ms,
                memory_limit_kb: tc.memory_limit_kb,
                max_processes: run_cfg.max_processes,
                stdin: stdin_path.clone(),
                stdout: stdout_path.clone(),
                stderr: Some(stderr_path.clone()),
            });

            let result = match run_result.status {
                SandboxStatus::Ok => {
                    // Run checker
                    let check = match &task.checker_config {
                        CheckerConfig::Token => token_check(&answer_path, &output_path),
                        CheckerConfig::Testlib {
                            path,
                            timeout_ms,
                            memory_limit_kb,
                            bind_dirs,
                        } => {
                            let checker_abs = self.paths.task_workspace(task).join(path);
                            testlib_check(
                                sandbox,
                                &checker_abs,
                                ts_id.unwrap_or("unknown"),
                                &input_path,
                                &output_path,
                                &answer_path,
                                *timeout_ms,
                                *memory_limit_kb,
                                &box_work_dir,
                                bind_dirs,
                            )
                        }
                    };
                    tjudge_store::schema::TestCaseResult {
                        verdict: check.verdict,
                        time_used_ms: Some(run_result.cpu_time_ms),
                        memory_used_kb: Some(run_result.memory_kb),
                        message: check.message,
                    }
                }
                SandboxStatus::TimeLimitExceeded => tjudge_store::schema::TestCaseResult {
                    verdict: TestCaseVerdict::TimeLimitExceeded,
                    time_used_ms: Some(run_result.cpu_time_ms),
                    memory_used_kb: Some(run_result.memory_kb),
                    message: None,
                },
                SandboxStatus::MemoryLimitExceeded => tjudge_store::schema::TestCaseResult {
                    verdict: TestCaseVerdict::MemoryLimitExceeded,
                    time_used_ms: Some(run_result.cpu_time_ms),
                    memory_used_kb: Some(run_result.memory_kb),
                    message: None,
                },
                SandboxStatus::RuntimeError { exit_code } => {
                    // Read first line of stderr for a useful message (e.g. Python traceback last line)
                    let stderr_msg = std::fs::read_to_string(&stderr_path)
                        .ok()
                        .and_then(|s| s.lines().last().map(|l| l.trim().to_string()))
                        .filter(|s| !s.is_empty());
                    let message = match (exit_code, stderr_msg) {
                        (Some(c), Some(msg)) => Some(format!("exit code {c}: {msg}")),
                        (Some(c), None) => Some(format!("exit code {c}")),
                        (None, Some(msg)) => Some(msg),
                        (None, None) => None,
                    };
                    tjudge_store::schema::TestCaseResult {
                        verdict: TestCaseVerdict::RuntimeError,
                        time_used_ms: Some(run_result.cpu_time_ms),
                        memory_used_kb: Some(run_result.memory_kb),
                        message,
                    }
                }
                SandboxStatus::InternalError(msg) => internal_error_result(msg),
            };

            // A 0-scoring result drops this test set's minimum to 0; mark it so
            // the set's remaining cases are skipped.
            use tjudge_store::scoring::test_case_relative_score;
            if test_case_relative_score(&result.verdict) == 0.0
                && let Some(id) = ts_id
            {
                failed_test_sets.insert(id.to_string());
            }

            emit(JudgeEvent::TestCase {
                contestant_id: req.contestant_id.clone(),
                task_id: req.task_id.clone(),
                test_case_id: tc.id.clone(),
                result: result.clone(),
            });
            test_case_results.insert(tc.id.clone(), result);
        }

        Ok(JudgeOutcome {
            contestant_id: req.contestant_id.clone(),
            task_id: req.task_id.clone(),
            compilation_error: None,
            test_case_results,
        })
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use std::{collections::HashMap, sync::Mutex};

    use tjudge_store::{
        ContestPaths,
        schema::{
            CheckerConfig, CompilationConfig, ContestConfig, ContestMeta, ContestantConfig,
            GroupConfig, RunConfig, TaskConfig, TestCaseConfig, TestCaseVerdict, TestSetConfig,
        },
    };

    use super::*;
    use crate::{
        sandbox::{SandboxResult, SandboxRunConfig, SandboxStatus},
        types::JudgeRequest,
    };

    // ── MockSandbox ──────────────────────────────────────────────────────────

    /// A mock sandbox that returns pre-configured results per invocation index.
    struct MockSandbox {
        results: Mutex<Vec<SandboxResult>>,
    }

    impl MockSandbox {
        fn new(results: Vec<SandboxResult>) -> Self {
            Self {
                results: Mutex::new(results),
            }
        }

        fn ok(wall_time_ms: u64, memory_kb: u64) -> SandboxResult {
            SandboxResult {
                status: SandboxStatus::Ok,
                wall_time_ms,
                cpu_time_ms: 0,
                memory_kb,
            }
        }

        fn tle() -> SandboxResult {
            SandboxResult {
                status: SandboxStatus::TimeLimitExceeded,
                wall_time_ms: 1000,
                cpu_time_ms: 0,
                memory_kb: 0,
            }
        }

        fn re(exit_code: i32) -> SandboxResult {
            SandboxResult {
                status: SandboxStatus::RuntimeError {
                    exit_code: Some(exit_code),
                },
                wall_time_ms: 0,
                cpu_time_ms: 0,
                memory_kb: 0,
            }
        }

        fn compile_fail() -> SandboxResult {
            SandboxResult {
                status: SandboxStatus::RuntimeError { exit_code: Some(1) },
                wall_time_ms: 0,
                cpu_time_ms: 0,
                memory_kb: 0,
            }
        }

        fn internal(msg: &str) -> SandboxResult {
            SandboxResult {
                status: SandboxStatus::InternalError(msg.to_string()),
                wall_time_ms: 0,
                cpu_time_ms: 0,
                memory_kb: 0,
            }
        }
    }

    impl Sandbox for MockSandbox {
        fn run(&self, _cfg: &SandboxRunConfig) -> SandboxResult {
            self.results
                .lock()
                .unwrap()
                .pop()
                // If we run out of pre-configured results, return Ok with dummy values.
                .unwrap_or_else(|| MockSandbox::ok(0, 0))
        }

        fn box_dir(&self) -> &std::path::Path {
            // Mock tests pass an explicit work_dir; this is never used.
            std::path::Path::new("")
        }
    }

    // ── Helpers ──────────────────────────────────────────────────────────────

    /// Build a minimal `ContestConfig` with one task containing the given test cases,
    /// one test set per test case, and one group covering all test sets.
    fn make_config(tc_ids: &[&str], compile: bool) -> (ContestConfig, ContestPaths) {
        let test_cases: Vec<TestCaseConfig> = tc_ids
            .iter()
            .map(|id| TestCaseConfig {
                id: id.to_string(),
                input_file: format!("{id}.in"),
                answer_file: format!("{id}.ans"),
                time_limit_ms: 1000,
                memory_limit_kb: 262144,
            })
            .collect();

        let test_sets: Vec<TestSetConfig> = tc_ids
            .iter()
            .map(|id| TestSetConfig {
                id: format!("ts_{id}"),
                test_case_ids: vec![id.to_string()],
            })
            .collect();

        let groups = vec![GroupConfig {
            id: "g1".to_string(),
            test_set_ids: tc_ids.iter().map(|id| format!("ts_{id}")).collect(),
            max_score: 100.0,
        }];

        let compilation_config = if compile {
            Some(CompilationConfig {
                compile_command: vec!["g++".into(), "solution.cpp".into()],
                compilation_timeout_ms: 10000,
                compilation_memory_limit_kb: 1024 * 1024,
                compilation_output_file: "solution".into(),
            })
        } else {
            None
        };

        let run_config = RunConfig {
            compilation_config,
            run_command: vec!["./run.sh".into()],
            max_processes: 1,
            bind_dirs: vec![],
        };

        let task = TaskConfig {
            id: "t1".to_string(),
            title: "Task 1".to_string(),
            workspace_root: "tasks/t1".to_string(),
            submission_config: HashMap::new(),
            library_config: HashMap::new(),
            run_config: [("cpp".to_string(), run_config)].into(),
            checker_config: CheckerConfig::Token,
            test_cases,
            test_sets,
            groups,
            file_io: None,
        };

        let contestant = ContestantConfig {
            id: "s1".to_string(),
            name: "S1".to_string(),
            workspace_root: "contestants/s1".to_string(),
            language: [("t1".to_string(), "cpp".to_string())].into(),
        };

        let config = ContestConfig {
            meta: ContestMeta {
                id: "c1".to_string(),
                title: "Contest".to_string(),
                schema_version: "1.0".to_string(),
            },
            tasks: vec![task],
            contestants: vec![contestant],
        };

        // Use a temp dir as contest root; create empty test-data files so the
        // judger's missing-file checks pass and the mock sandbox actually runs.
        let root = tempfile::tempdir().unwrap().keep();
        let task_dir = root.join("tasks/t1");
        std::fs::create_dir_all(&task_dir).unwrap();
        for id in tc_ids {
            std::fs::write(task_dir.join(format!("{id}.in")), b"").unwrap();
            std::fs::write(task_dir.join(format!("{id}.ans")), b"").unwrap();
        }
        let paths = ContestPaths::new(root);
        (config, paths)
    }

    /// Run `judge_with_sandbox` directly using the mock, bypassing file I/O.
    fn run_mock(config: ContestConfig, paths: ContestPaths, sandbox: &MockSandbox) -> JudgeOutcome {
        let task = config.task("t1").unwrap().clone();
        let contestant = config.contestant("s1").unwrap().clone();
        let run_cfg = task.run_config.get("cpp").unwrap().clone();
        let judger = Judger::new(config, paths);
        let req = JudgeRequest {
            contestant_id: "s1".to_string(),
            task_id: "t1".to_string(),
        };
        let work_dir = tempfile::tempdir().unwrap().keep();
        let cancel = AtomicBool::new(false);
        judger
            .judge_with_sandbox(
                &req,
                &task,
                &contestant,
                &run_cfg,
                sandbox,
                &work_dir,
                &cancel,
                &|_| {},
            )
            .unwrap()
    }

    /// Run `judge_batch` (streaming) and collect all emitted events.
    fn collect_events(
        judger: &Judger,
        requests: Vec<JudgeRequest>,
        cancel: Arc<AtomicBool>,
    ) -> Vec<JudgeEvent> {
        let events: Arc<Mutex<Vec<JudgeEvent>>> = Arc::new(Mutex::new(Vec::new()));
        let sink = events.clone();
        judger.judge_batch(requests, cancel, move |e| {
            sink.lock().unwrap().push(e);
        });
        Arc::try_unwrap(events).ok().unwrap().into_inner().unwrap()
    }

    // ── Tests ────────────────────────────────────────────────────────────────

    #[test]
    fn all_accepted() {
        let (cfg, paths) = make_config(&["tc1", "tc2"], false);
        // Two test cases, both Ok; MockSandbox results are popped (LIFO), so push in reverse.
        let mock = MockSandbox::new(vec![MockSandbox::ok(50, 1024), MockSandbox::ok(60, 2048)]);
        let outcome = run_mock(cfg, paths, &mock);
        assert!(outcome.compilation_error.is_none());
        assert_eq!(outcome.test_case_results.len(), 2);
        for r in outcome.test_case_results.values() {
            // Token check on missing files → InternalError, but what matters for this test
            // is that the sandbox ran both test cases (not skipped).
            assert!(!matches!(r.verdict, TestCaseVerdict::Skipped));
        }
    }

    #[test]
    fn compilation_error_returns_early() {
        let (cfg, paths) = make_config(&["tc1"], true);
        // Only one sandbox call — the compile step; it fails.
        let mock = MockSandbox::new(vec![MockSandbox::compile_fail()]);
        let outcome = run_mock(cfg, paths, &mock);
        assert!(outcome.compilation_error.is_some());
        assert!(outcome.test_case_results.is_empty());
    }

    #[test]
    fn tle_does_not_skip_other_test_sets() {
        let (cfg, paths) = make_config(&["tc1", "tc2", "tc3"], false);
        // Each test case is in its own test set. tc1 → TLE must NOT skip tc2/tc3
        // (they live in different test sets). MockSandbox pops from end, so push
        // TLE last = first to be popped; tc2/tc3 then get the fallback Ok result.
        let mock = MockSandbox::new(vec![MockSandbox::tle()]);
        let outcome = run_mock(cfg, paths, &mock);
        assert_eq!(outcome.test_case_results.len(), 3);
        assert!(matches!(
            outcome.test_case_results["tc1"].verdict,
            TestCaseVerdict::TimeLimitExceeded
        ));
        // tc2/tc3 are still judged (not skipped): Ok run → token check on missing
        // output → InternalError.
        assert!(matches!(
            outcome.test_case_results["tc2"].verdict,
            TestCaseVerdict::InternalError
        ));
        assert!(matches!(
            outcome.test_case_results["tc3"].verdict,
            TestCaseVerdict::InternalError
        ));
    }

    #[test]
    fn wa_skips_rest_of_test_set() {
        // Two test cases in the SAME test set; tc1 → WA should skip tc2.
        let cfg_and_paths = {
            let (mut cfg, paths) = make_config(&["tc1", "tc2"], false);
            // Put both tc1 and tc2 in the same test set.
            cfg.tasks[0].test_sets = vec![TestSetConfig {
                id: "ts1".to_string(),
                test_case_ids: vec!["tc1".to_string(), "tc2".to_string()],
            }];
            cfg.tasks[0].groups[0].test_set_ids = vec!["ts1".to_string()];
            (cfg, paths)
        };

        // tc1 runs OK (but token check will see missing files → InternalError which scores 0
        // and should still trigger test-set skip).
        let mock = MockSandbox::new(vec![MockSandbox::ok(10, 100)]);
        let outcome = run_mock(cfg_and_paths.0, cfg_and_paths.1, &mock);
        assert_eq!(outcome.test_case_results.len(), 2);
        // tc1 scores 0 (InternalError from token check on missing files)
        let tc1_v = &outcome.test_case_results["tc1"].verdict;
        assert!(matches!(tc1_v, TestCaseVerdict::InternalError));
        // tc2 must be skipped because its test set failed
        assert!(matches!(
            outcome.test_case_results["tc2"].verdict,
            TestCaseVerdict::Skipped
        ));
    }

    #[test]
    fn runtime_error_verdict() {
        let (cfg, paths) = make_config(&["tc1"], false);
        let mock = MockSandbox::new(vec![MockSandbox::re(137)]);
        let outcome = run_mock(cfg, paths, &mock);
        assert!(matches!(
            outcome.test_case_results["tc1"].verdict,
            TestCaseVerdict::RuntimeError
        ));
    }

    #[test]
    fn internal_error_verdict() {
        let (cfg, paths) = make_config(&["tc1"], false);
        let mock = MockSandbox::new(vec![MockSandbox::internal("cgroup failure")]);
        let outcome = run_mock(cfg, paths, &mock);
        assert!(matches!(
            outcome.test_case_results["tc1"].verdict,
            TestCaseVerdict::InternalError
        ));
    }

    #[test]
    fn missing_submission_file_is_file_error() {
        use tjudge_store::schema::SubmissionConfig;
        let (mut cfg, paths) = make_config(&["tc1", "tc2"], false);
        // Point the submission at a source file that does not exist on disk.
        cfg.tasks[0].submission_config.insert(
            "cpp".to_string(),
            SubmissionConfig {
                files: vec!["nonexistent.cpp".to_string()],
            },
        );
        let judger = Judger::new(cfg, paths);
        let req = JudgeRequest {
            contestant_id: "s1".to_string(),
            task_id: "t1".to_string(),
        };
        // Emits a 0-pt FileError TestCase per test case + a TaskComplete, with
        // no sandbox created.
        let evs = collect_events(&judger, vec![req], Arc::new(AtomicBool::new(false)));
        let tcs: Vec<_> = evs
            .iter()
            .filter_map(|e| match e {
                JudgeEvent::TestCase { result, .. } => Some(result),
                _ => None,
            })
            .collect();
        assert_eq!(tcs.len(), 2);
        for r in &tcs {
            assert!(
                matches!(r.verdict, TestCaseVerdict::FileError),
                "got {:?}",
                r.verdict
            );
            assert!(
                r.message
                    .as_deref()
                    .unwrap()
                    .contains("submission file not found")
            );
        }
        assert!(evs.iter().any(|e| matches!(
            e,
            JudgeEvent::TaskComplete {
                compilation_error: None,
                ..
            }
        )));
    }

    #[test]
    fn missing_test_data_file_is_file_error() {
        let (cfg, paths) = make_config(&["tc1", "tc2"], false);
        // Remove tc2's answer file so it is missing on disk.
        std::fs::remove_file(paths.root.join("tasks/t1/tc2.ans")).unwrap();
        // One sandbox result for tc1 (tc2 never reaches the sandbox).
        let mock = MockSandbox::new(vec![MockSandbox::ok(10, 100)]);
        let outcome = run_mock(cfg, paths, &mock);
        // tc1 ran (token check on missing output → InternalError, scores 0).
        assert!(matches!(
            outcome.test_case_results["tc1"].verdict,
            TestCaseVerdict::InternalError
        ));
        // tc2's answer file is missing → FileError (0 pts).
        let tc2 = &outcome.test_case_results["tc2"];
        assert!(matches!(tc2.verdict, TestCaseVerdict::FileError));
        assert!(tc2.message.as_deref().unwrap().contains("file not found"));
    }

    #[test]
    fn task_not_found_error() {
        let (cfg, paths) = make_config(&[], false);
        let judger = Judger::new(cfg, paths);
        let req = JudgeRequest {
            contestant_id: "s1".to_string(),
            task_id: "nonexistent".to_string(),
        };
        let evs = collect_events(&judger, vec![req], Arc::new(AtomicBool::new(false)));
        assert!(
            evs.iter()
                .any(|e| matches!(e, JudgeEvent::Error(m) if m.contains("task not found"))),
            "expected an Error event for the missing task, got {evs:?}"
        );
    }

    #[test]
    fn streaming_emits_one_event_per_test_case() {
        use tjudge_store::schema::SubmissionConfig;
        let (mut cfg, paths) = make_config(&["tc1", "tc2", "tc3"], false);
        // Missing submission -> FileError on every test case, no sandbox needed.
        cfg.tasks[0].submission_config.insert(
            "cpp".to_string(),
            SubmissionConfig {
                files: vec!["nonexistent.cpp".to_string()],
            },
        );
        let judger = Judger::new(cfg, paths);
        let req = JudgeRequest {
            contestant_id: "s1".to_string(),
            task_id: "t1".to_string(),
        };
        let evs = collect_events(&judger, vec![req], Arc::new(AtomicBool::new(false)));
        let tc_count = evs
            .iter()
            .filter(|e| matches!(e, JudgeEvent::TestCase { .. }))
            .count();
        let done_count = evs
            .iter()
            .filter(|e| matches!(e, JudgeEvent::TaskComplete { .. }))
            .count();
        assert_eq!(tc_count, 3, "one TestCase event per test case");
        assert_eq!(done_count, 1, "exactly one TaskComplete");
        for e in evs.iter() {
            if let JudgeEvent::TestCase { result, .. } = e {
                assert!(matches!(result.verdict, TestCaseVerdict::FileError));
            }
        }
    }

    #[test]
    fn streaming_cancel_skips_all() {
        use tjudge_store::schema::SubmissionConfig;
        let (mut cfg, paths) = make_config(&["tc1", "tc2"], false);
        cfg.tasks[0].submission_config.insert(
            "cpp".to_string(),
            SubmissionConfig {
                files: vec!["nonexistent.cpp".to_string()],
            },
        );
        let judger = Judger::new(cfg, paths);
        let req = JudgeRequest {
            contestant_id: "s1".to_string(),
            task_id: "t1".to_string(),
        };
        let evs = collect_events(&judger, vec![req], Arc::new(AtomicBool::new(true)));
        assert!(evs.is_empty(), "a cancelled run should emit no events");
    }
}

#[cfg(test)]
mod integration_tests {
    use std::collections::HashMap;
    use std::sync::atomic::AtomicBool;
    use std::sync::{Arc, Mutex};

    use super::*;
    use crate::types::JudgeRequest;

    /// Per-(contestant, task) grouped outcome: compilation error + test-case results.
    type GroupedOutcomes = HashMap<
        (String, String),
        (
            Option<String>,
            Vec<(String, tjudge_store::schema::TestCaseResult)>,
        ),
    >;

    fn load(path: &str) -> (ContestConfig, tjudge_store::ContestPaths) {
        let p = std::path::Path::new(path);
        let config = tjudge_store::load_config(p).expect("load config");
        let paths = tjudge_store::ContestPaths::new(p.parent().unwrap());
        (config, paths)
    }

    /// Run the requests (streaming) and group events into per-(contestant, task)
    /// outcomes: `(compilation_error, sorted test-case results)`.
    fn run_grouped(judger: &Judger, requests: Vec<JudgeRequest>) -> GroupedOutcomes {
        let events: Arc<Mutex<Vec<JudgeEvent>>> = Arc::new(Mutex::new(vec![]));
        let sink = events.clone();
        judger.judge_batch(requests, Arc::new(AtomicBool::new(false)), move |e| {
            sink.lock().unwrap().push(e);
        });
        let evs = events.lock().unwrap();
        let mut map: GroupedOutcomes = HashMap::new();
        for e in evs.iter() {
            match e {
                JudgeEvent::TestCase {
                    contestant_id,
                    task_id,
                    test_case_id,
                    result,
                } => {
                    map.entry((contestant_id.clone(), task_id.clone()))
                        .or_insert_with(|| (None, vec![]))
                        .1
                        .push((test_case_id.clone(), result.clone()));
                }
                JudgeEvent::TaskComplete {
                    contestant_id,
                    task_id,
                    compilation_error,
                } => {
                    map.entry((contestant_id.clone(), task_id.clone()))
                        .or_insert_with(|| (None, vec![]))
                        .0 = compilation_error.clone();
                }
                JudgeEvent::Error(m) => eprintln!("judge error: {m}"),
            }
        }
        map
    }

    fn print_grouped(map: &GroupedOutcomes) {
        let mut keys: Vec<_> = map.keys().collect();
        keys.sort();
        for k in keys {
            let (ce, tcs) = &map[k];
            if let Some(ce) = ce {
                eprintln!("{}/{} CE: {}", k.0, k.1, &ce[..ce.len().min(120)]);
                continue;
            }
            let mut tcs = tcs.clone();
            tcs.sort_by(|a, b| a.0.cmp(&b.0));
            for (tc_id, tcr) in &tcs {
                eprintln!(
                    "{}/{} {}: {:?} msg={:?}",
                    k.0,
                    k.1,
                    tc_id,
                    tcr.verdict,
                    tcr.message.as_deref().map(|m| &m[..m.len().min(80)])
                );
            }
        }
    }

    #[test]
    #[ignore]
    fn test_judge_all_add() {
        let (config, paths) = load("/home/registergen/thu/26-sum/demo-contest/contest.toml");
        let judger = Judger::new(config, paths);
        let map = run_grouped(
            &judger,
            vec![
                JudgeRequest {
                    contestant_id: "alice".into(),
                    task_id: "add".into(),
                },
                JudgeRequest {
                    contestant_id: "bob".into(),
                    task_id: "add".into(),
                },
                JudgeRequest {
                    contestant_id: "charlie".into(),
                    task_id: "add".into(),
                },
                JudgeRequest {
                    contestant_id: "dave".into(),
                    task_id: "add".into(),
                },
            ],
        );
        print_grouped(&map);
    }

    #[test]
    #[ignore]
    fn test_judge_all_guess() {
        let (config, paths) = load("/home/registergen/thu/26-sum/demo-contest/contest.toml");
        let judger = Judger::new(config, paths);
        let map = run_grouped(
            &judger,
            vec![
                JudgeRequest {
                    contestant_id: "alice".into(),
                    task_id: "guess".into(),
                },
                JudgeRequest {
                    contestant_id: "charlie".into(),
                    task_id: "guess".into(),
                },
                JudgeRequest {
                    contestant_id: "dave".into(),
                    task_id: "guess".into(),
                },
            ],
        );
        print_grouped(&map);
    }

    /// Test all contestants × all tasks in parallel (exactly as the TUI does when J is pressed).
    #[test]
    #[ignore]
    fn test_judge_all_full() {
        let (config, paths) = load("/home/registergen/thu/26-sum/demo-contest/contest.toml");
        let judger = Judger::new(config.clone(), paths);
        let requests: Vec<JudgeRequest> = config
            .contestants
            .iter()
            .flat_map(|c| {
                config.tasks.iter().map(|t| JudgeRequest {
                    contestant_id: c.id.clone(),
                    task_id: t.id.clone(),
                })
            })
            .collect();
        let map = run_grouped(&judger, requests);
        print_grouped(&map);
    }

    /// Verify the no-isolate fallback: a shim `isolate` that fails makes
    /// `JudgerSandbox` use `DirectSandbox`, and a communication task (which
    /// forks Alice/Bob) still judges correctly end-to-end.
    #[test]
    #[ignore]
    fn flip_fallback_no_isolate() {
        use std::os::unix::fs::PermissionsExt;
        let shim = tempfile::tempdir().unwrap();
        let shim_isolate = shim.path().join("isolate");
        std::fs::write(&shim_isolate, "#!/bin/sh\nexit 1\n").unwrap();
        let mut perms = std::fs::metadata(&shim_isolate).unwrap().permissions();
        perms.set_mode(0o755);
        std::fs::set_permissions(&shim_isolate, perms).unwrap();
        let new_path = format!(
            "{}:{}",
            shim.path().display(),
            std::env::var("PATH").unwrap_or_default()
        );
        // SAFETY: this is the only thread in the test binary using env; the
        // OnceLock probe re-runs against the new PATH below.
        unsafe { std::env::set_var("PATH", &new_path) };

        let (config, paths) = load("/home/registergen/thu/26-sum/demo-contest/contest.toml");
        let judger = Judger::new(config.clone(), paths);
        let map = run_grouped(
            &judger,
            vec![JudgeRequest {
                contestant_id: "charlie".into(),
                task_id: "flip".into(),
            }],
        );
        let (ce, tcs) = &map[&("charlie".to_string(), "flip".to_string())];
        assert!(ce.is_none(), "unexpected CE: {:?}", ce);
        assert_eq!(tcs.len(), 5);
        for (_id, r) in tcs {
            assert!(
                matches!(r.verdict, TestCaseVerdict::Accepted),
                "expected AC, got {:?}",
                r.verdict
            );
        }
    }
}
