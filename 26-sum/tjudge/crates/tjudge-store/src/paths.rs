use std::path::{Path, PathBuf};

use crate::schema::{
    CheckerConfig, ContestantConfig, LibraryConfig, RunConfig, SubmissionConfig, TaskConfig,
    TestCaseConfig,
};

/// Resolves all contest-relative paths to absolute (or contest-root-relative)
/// `PathBuf`s.
///
/// # Layout conventions
///
/// - Contest root: the directory that contains `contest.toml`.
/// - Organizer task workspace: `<contest_root>/<task.workspace_root>/`
/// - Contestant workspace: `<contest_root>/<contestant.workspace_root>/`
/// - Submission files are relative to `<contestant_workspace>/`.
/// - Library, checker, and test-case files are relative to the organizer task
///   workspace.
#[derive(Debug, Clone)]
pub struct ContestPaths {
    /// Absolute path to the contest root directory (the directory containing
    /// `contest.toml`).
    pub root: PathBuf,
}

impl ContestPaths {
    /// Creates a new `ContestPaths` anchored at `root`.
    pub fn new(root: impl Into<PathBuf>) -> Self {
        Self { root: root.into() }
    }

    /// Creates a `ContestPaths` anchored at the parent of `contest_toml_path`.
    ///
    /// ```no_run
    /// # use std::path::Path;
    /// # use tjudge_store::ContestPaths;
    /// let paths = ContestPaths::from_config_path(Path::new("/contests/ioi/contest.toml"));
    /// assert_eq!(paths.root, std::path::PathBuf::from("/contests/ioi"));
    /// ```
    pub fn from_config_path(contest_toml_path: &Path) -> Self {
        let root = contest_toml_path
            .parent()
            .unwrap_or(Path::new("."))
            .to_path_buf();
        Self { root }
    }

    // ── Organizer (task) paths ────────────────────────────────────────────────

    /// Absolute path to the organizer workspace directory for `task`.
    pub fn task_workspace(&self, task: &TaskConfig) -> PathBuf {
        self.root.join(&task.workspace_root)
    }

    /// Absolute path to the testlib checker executable for `task`, or `None`
    /// if the task uses the built-in [`CheckerConfig::Token`] checker.
    pub fn checker_path(&self, task: &TaskConfig) -> Option<PathBuf> {
        match &task.checker_config {
            CheckerConfig::Token => None,
            CheckerConfig::Testlib { path, .. } => Some(self.task_workspace(task).join(path)),
        }
    }

    /// Absolute path to the input file for `tc` within `task`.
    pub fn test_case_input(&self, task: &TaskConfig, tc: &TestCaseConfig) -> PathBuf {
        self.task_workspace(task).join(&tc.input_file)
    }

    /// Absolute path to the answer file for `tc` within `task`.
    pub fn test_case_answer(&self, task: &TaskConfig, tc: &TestCaseConfig) -> PathBuf {
        self.task_workspace(task).join(&tc.answer_file)
    }

    /// Absolute paths to library files for `task` and `language`, or `None`
    /// if no library config exists for that language.
    pub fn library_files(&self, task: &TaskConfig, language: &str) -> Option<Vec<PathBuf>> {
        task.library_config.get(language).map(|lc: &LibraryConfig| {
            let base = self.task_workspace(task);
            lc.files.iter().map(|f| base.join(f)).collect()
        })
    }

    // ── Contestant paths ──────────────────────────────────────────────────────

    /// Absolute path to the contestant's workspace root directory.
    pub fn contestant_workspace(&self, contestant: &ContestantConfig) -> PathBuf {
        self.root.join(&contestant.workspace_root)
    }

    /// Absolute paths to the submission files for `contestant` on `task`.
    /// Returns `None` if the contestant has no language configured for this task,
    /// or if the task has no submission config for that language.
    ///
    /// Submission files are resolved relative to the contestant's workspace root.
    pub fn submission_files(
        &self,
        contestant: &ContestantConfig,
        task: &TaskConfig,
    ) -> Option<Vec<PathBuf>> {
        let language = contestant.language.get(&task.id)?;
        task.submission_config
            .get(language)
            .map(|sc: &SubmissionConfig| {
                let base = self.contestant_workspace(contestant);
                sc.files.iter().map(|f| base.join(f)).collect()
            })
    }

    // ── Run configuration ─────────────────────────────────────────────────────

    /// Returns the [`RunConfig`] for `contestant`'s language on `task`, if one exists.
    pub fn run_config<'a>(
        &self,
        contestant: &ContestantConfig,
        task: &'a TaskConfig,
    ) -> Option<&'a RunConfig> {
        let language = contestant.language.get(&task.id)?;
        task.run_config.get(language)
    }
}

#[cfg(test)]
mod tests {
    use std::collections::HashMap;

    use super::*;
    use crate::schema::*;

    fn make_task(id: &str, workspace_root: &str) -> TaskConfig {
        TaskConfig {
            id: id.to_string(),
            title: "Task".to_string(),
            workspace_root: workspace_root.to_string(),
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

    fn make_contestant(id: &str, workspace_root: &str) -> ContestantConfig {
        ContestantConfig {
            id: id.to_string(),
            name: id.to_string(),
            language: HashMap::new(),
            workspace_root: workspace_root.to_string(),
        }
    }

    #[test]
    fn task_workspace_joins_root() {
        let paths = ContestPaths::new("/contests/ioi");
        let task = make_task("t1", "tasks/t1");
        assert_eq!(
            paths.task_workspace(&task),
            PathBuf::from("/contests/ioi/tasks/t1")
        );
    }

    #[test]
    fn token_checker_path_is_none() {
        let paths = ContestPaths::new("/contests/ioi");
        let task = make_task("t1", "tasks/t1");
        assert_eq!(paths.checker_path(&task), None);
    }

    #[test]
    fn testlib_checker_path_resolves() {
        let paths = ContestPaths::new("/contests/ioi");
        let mut task = make_task("t1", "tasks/t1");
        task.checker_config = CheckerConfig::Testlib {
            path: "checker".to_string(),
            timeout_ms: 10000,
            memory_limit_kb: 1024 * 1024,
            bind_dirs: vec![],
        };
        assert_eq!(
            paths.checker_path(&task),
            Some(PathBuf::from("/contests/ioi/tasks/t1/checker"))
        );
    }

    #[test]
    fn test_case_input_resolves() {
        let paths = ContestPaths::new("/contests/ioi");
        let task = make_task("t1", "tasks/t1");
        let tc = TestCaseConfig {
            id: "tc1".to_string(),
            input_file: "tests/1.in".to_string(),
            answer_file: "tests/1.ans".to_string(),
            time_limit_ms: 1000,
            memory_limit_kb: 262144,
        };
        assert_eq!(
            paths.test_case_input(&task, &tc),
            PathBuf::from("/contests/ioi/tasks/t1/tests/1.in")
        );
        assert_eq!(
            paths.test_case_answer(&task, &tc),
            PathBuf::from("/contests/ioi/tasks/t1/tests/1.ans")
        );
    }

    #[test]
    fn submission_files_none_when_language_missing() {
        let paths = ContestPaths::new("/contests/ioi");
        let task = make_task("t1", "tasks/t1");
        // contestant has no language entry for task "t1"
        let contestant = make_contestant("s1", "contestants/s1");
        assert!(paths.submission_files(&contestant, &task).is_none());
    }

    #[test]
    fn submission_files_resolves() {
        let paths = ContestPaths::new("/contests/ioi");
        let mut task = make_task("t1", "tasks/t1");
        task.submission_config.insert(
            "cpp".to_string(),
            SubmissionConfig {
                files: vec!["solution.cpp".to_string()],
            },
        );
        let mut contestant = make_contestant("s1", "contestants/s1");
        contestant
            .language
            .insert("t1".to_string(), "cpp".to_string());
        let files = paths.submission_files(&contestant, &task).unwrap();
        assert_eq!(
            files,
            vec![PathBuf::from("/contests/ioi/contestants/s1/solution.cpp")]
        );
    }

    #[test]
    fn from_config_path_strips_filename() {
        let paths = ContestPaths::from_config_path(Path::new("/contests/ioi/contest.toml"));
        assert_eq!(paths.root, PathBuf::from("/contests/ioi"));
    }

    #[test]
    fn library_files_resolves() {
        let paths = ContestPaths::new("/contests/ioi");
        let mut task = make_task("t1", "tasks/t1");
        task.library_config.insert(
            "cpp".to_string(),
            LibraryConfig {
                files: vec!["grader.cpp".to_string(), "grader.h".to_string()],
            },
        );
        let files = paths.library_files(&task, "cpp").unwrap();
        assert_eq!(
            files,
            vec![
                PathBuf::from("/contests/ioi/tasks/t1/grader.cpp"),
                PathBuf::from("/contests/ioi/tasks/t1/grader.h"),
            ]
        );
    }
}
