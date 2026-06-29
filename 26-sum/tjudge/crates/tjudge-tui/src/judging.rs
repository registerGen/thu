use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc;
use std::sync::{Arc, Mutex};
use std::thread;

use tjudge_judger::{JudgeEvent, JudgeRequest, Judger};
use tjudge_store::ContestPaths;
use tjudge_store::schema::ContestantTaskResult;

use crate::app::{App, JudgeMsg, Mode};

/// Spawn a background thread to judge all given requests, streaming results
/// back per test case as they complete (rather than blocking until the whole
/// batch finishes). Returns immediately; results arrive via `app.judge_rx`.
pub fn spawn_judge_all(app: &mut App, requests: Vec<JudgeRequest>) {
    let config = app.config.clone();
    let paths = ContestPaths::new(
        app.config_path
            .parent()
            .unwrap_or(std::path::Path::new(".")),
    );

    // Clear the recorded results for every (contestant, task) about to be
    // (re)judged, so the rank table shows those cells as "N/A" while judging is
    // in progress. They get repopulated as test-case results stream back in.
    app.results.results.retain(|r| {
        !requests
            .iter()
            .any(|req| req.contestant_id == r.contestant_id && req.task_id == r.task_id)
    });

    // Progress unit = one test case. Every request emits exactly one event per
    // test case (including skipped / file-error / compile-error cases), so this
    // total reconciles exactly as judging progresses.
    let total: usize = requests
        .iter()
        .map(|r| {
            config
                .task(&r.task_id)
                .map(|t| t.test_cases.len())
                .unwrap_or(0)
        })
        .sum();

    let (tx, rx) = mpsc::channel::<JudgeMsg>();
    app.judge_pending = total;
    app.judge_rx = Some(rx);

    let cancel = Arc::new(AtomicBool::new(false));
    app.judge_cancel = Some(cancel.clone());

    let tx = Arc::new(Mutex::new(tx));

    thread::spawn(move || {
        let judger = Judger::new(config, paths);
        let sender = tx.clone();
        judger.judge_batch(requests, cancel, move |ev| {
            let msg = match ev {
                JudgeEvent::TestCase {
                    contestant_id,
                    task_id,
                    test_case_id,
                    result,
                } => JudgeMsg::TestCase {
                    contestant_id,
                    task_id,
                    test_case_id,
                    result,
                },
                JudgeEvent::TaskComplete {
                    contestant_id,
                    task_id,
                    compilation_error,
                } => JudgeMsg::TaskComplete {
                    contestant_id,
                    task_id,
                    compilation_error,
                },
                JudgeEvent::Error(m) => JudgeMsg::Error(m),
            };
            let _ = sender.lock().unwrap().send(msg);
        });
        let _ = tx.lock().unwrap().send(JudgeMsg::Done);
    });
}

/// Cancel the in-progress judge run: signals the background thread to stop
/// dispatching new work, drops the result receiver (so late results are
/// ignored), saves what has been judged so far, and returns to Normal mode.
pub fn cancel_judging(app: &mut App) {
    if let Some(c) = app.judge_cancel.take() {
        c.store(true, Ordering::Relaxed);
    }
    app.judge_rx = None;
    app.judge_pending = 0;
    app.auto_save_results();
    app.mode = Mode::Normal;
}

/// Poll the judge channel and process completed results. Returns true if judging is done.
pub fn poll_judge_results(app: &mut App) -> bool {
    let rx = match app.judge_rx.take() {
        Some(rx) => rx,
        None => return true,
    };

    loop {
        match rx.try_recv() {
            Ok(JudgeMsg::TestCase {
                contestant_id,
                task_id,
                test_case_id,
                result,
            }) => {
                upsert_test_case(app, &contestant_id, &task_id, test_case_id, result);
                if app.judge_pending > 0 {
                    app.judge_pending -= 1;
                }
            }
            Ok(JudgeMsg::TaskComplete {
                contestant_id,
                task_id,
                compilation_error,
            }) => {
                upsert_task_complete(app, &contestant_id, &task_id, compilation_error);
            }
            Ok(JudgeMsg::Error(e)) => {
                app.error_flash = Some(format!("Judge error: {e}"));
            }
            Ok(JudgeMsg::Done) | Err(mpsc::TryRecvError::Disconnected) => {
                app.judge_pending = 0;
                app.auto_save_results();
                return true;
            }
            Err(mpsc::TryRecvError::Empty) => {
                // Nothing ready yet; put receiver back
                app.judge_rx = Some(rx);
                return false;
            }
        }
    }
}

/// Insert (or replace) a single test case result for a (contestant, task),
/// creating the result entry if it does not yet exist.
fn upsert_test_case(
    app: &mut App,
    contestant_id: &str,
    task_id: &str,
    test_case_id: String,
    result: tjudge_store::schema::TestCaseResult,
) {
    let entry = app
        .results
        .results
        .iter_mut()
        .find(|r| r.contestant_id == contestant_id && r.task_id == task_id);
    match entry {
        Some(r) => {
            r.test_case_results.insert(test_case_id, result);
        }
        None => {
            let mut map = HashMap::new();
            map.insert(test_case_id, result);
            app.results.results.push(ContestantTaskResult {
                contestant_id: contestant_id.to_string(),
                task_id: task_id.to_string(),
                compilation_error: None,
                test_case_results: map,
            });
        }
    }
}

/// Set the task-level compilation error for a (contestant, task), creating an
/// empty result entry if it does not yet exist.
fn upsert_task_complete(
    app: &mut App,
    contestant_id: &str,
    task_id: &str,
    compilation_error: Option<String>,
) {
    let entry = app
        .results
        .results
        .iter_mut()
        .find(|r| r.contestant_id == contestant_id && r.task_id == task_id);
    match entry {
        Some(r) => {
            r.compilation_error = compilation_error;
        }
        None => {
            app.results.results.push(ContestantTaskResult {
                contestant_id: contestant_id.to_string(),
                task_id: task_id.to_string(),
                compilation_error,
                test_case_results: HashMap::new(),
            });
        }
    }
}
