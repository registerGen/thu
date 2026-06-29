pub mod paths;
pub mod schema;
pub mod scoring;
pub mod store;
pub mod validate;

pub use paths::ContestPaths;
pub use schema::*;
pub use scoring::{
    ContestantScoreBreakdown, GroupScore, TaskScoreBreakdown, compute_contestant_score,
    compute_task_score, group_score, test_case_relative_score, test_set_relative_score,
};
pub use store::{StoreError, load_config, load_results, save_config, save_results};
pub use validate::{
    SUPPORTED_SCHEMA_VERSION, ValidationError, collect_unique_ids, validate_config,
    validate_results, validate_task,
};

#[test]
fn test_load_demo_contest() {
    let p = std::path::Path::new("/home/registergen/thu/26-sum/demo-contest/contest.toml");
    let result = load_config(p);
    match &result {
        Ok(c) => println!(
            "Loaded OK: {} tasks, {} contestants",
            c.tasks.len(),
            c.contestants.len()
        ),
        Err(e) => println!("LOAD ERROR: {}", e),
    }
    assert!(result.is_ok(), "failed to load demo contest: {:?}", result);
}

#[test]
#[ignore]
fn test_load_demo_and_check() {
    let p = std::path::Path::new("/home/registergen/thu/26-sum/demo-contest/contest.toml");
    let c = load_config(p).unwrap();
    // Verify add task has correct test case inputs
    let add_task = c.task("add").unwrap();
    eprintln!(
        "add test cases: {:?}",
        add_task
            .test_cases
            .iter()
            .map(|t| &t.id)
            .collect::<Vec<_>>()
    );
}
