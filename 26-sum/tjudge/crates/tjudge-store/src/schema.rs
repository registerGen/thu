use std::collections::HashMap;

use serde::{Deserialize, Serialize};

/// Contest configuration. All data is included here.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ContestConfig {
    pub meta: ContestMeta,
    #[serde(default)]
    pub tasks: Vec<TaskConfig>,
    #[serde(default)]
    pub contestants: Vec<ContestantConfig>,
}

/// Contest results.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ContestResults {
    #[serde(default)]
    pub results: Vec<ContestantTaskResult>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ContestMeta {
    pub id: String,
    pub title: String,
    #[serde(default = "default_schema_version")]
    pub schema_version: String,
}

fn default_schema_version() -> String {
    "1.0".to_string()
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TaskConfig {
    pub id: String,
    pub title: String,
    /// Contest organizer workspace root of the task, relative to the contest root, i.e.,
    /// where contest.toml is located.
    pub workspace_root: String,
    /// Submission configuration per language.
    #[serde(default)]
    pub submission_config: HashMap<String, SubmissionConfig>,
    /// Library configuration per language.
    #[serde(default)]
    pub library_config: HashMap<String, LibraryConfig>,
    /// Run configuration per language.
    #[serde(default)]
    pub run_config: HashMap<String, RunConfig>,
    #[serde(default)]
    pub checker_config: CheckerConfig,
    #[serde(default)]
    pub test_cases: Vec<TestCaseConfig>,
    #[serde(default)]
    pub test_sets: Vec<TestSetConfig>,
    #[serde(default)]
    pub groups: Vec<GroupConfig>,
    /// Optional file-I/O convention: if set, the submission reads from
    /// `input_file` and writes to `output_file` (paths relative to the
    /// sandbox root) instead of stdin/stdout. The judger places each test
    /// case's input at `input_file` and checks `output_file`.
    #[serde(default)]
    pub file_io: Option<FileIoConfig>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SubmissionConfig {
    /// Files to be submitted by contestants,
    /// relative to the contestant workspace root of that task.
    #[serde(default)]
    pub files: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LibraryConfig {
    /// Files provided by contest organizers,
    /// relative to the organizer workspace root of that task.
    #[serde(default)]
    pub files: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RunConfig {
    pub compilation_config: Option<CompilationConfig>,
    /// Command to run the submission
    /// When the submission is run, the following environment variables will be set:
    /// - `TJUDGE_BINARY`: path to the compiled binary, relative to the sandbox root.
    ///   If the submission is not compiled, this variable will not be set.
    /// - `TJUDGE_INPUT`: path to the input file, relative to the sandbox root.
    /// - `TJUDGE_OUTPUT`: path to the output file, relative to the sandbox root.
    pub run_command: Vec<String>,
    #[serde(default = "default_max_processes")]
    pub max_processes: u32,
    /// Extra host directories to bind-mount (read-only) into the sandbox.
    /// Use this to make interpreters or runtimes visible inside the box,
    /// e.g. `["/usr", "/lib", "/lib/x86_64-linux-gnu"]` for Python.
    #[serde(default)]
    pub bind_dirs: Vec<std::path::PathBuf>,
}

fn default_max_processes() -> u32 {
    1
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CompilationConfig {
    /// Command to compile the submission
    pub compile_command: Vec<String>,
    #[serde(default = "default_compilation_timeout_ms")]
    pub compilation_timeout_ms: u64,
    #[serde(default = "default_compilation_memory_limit_kb")]
    pub compilation_memory_limit_kb: u64,
    /// Path to the compilation output file, relative to the sandbox root.
    pub compilation_output_file: String,
}

fn default_compilation_timeout_ms() -> u64 {
    10000 // 10s
}

fn default_compilation_memory_limit_kb() -> u64 {
    1024 * 1024 // 1GiB
}

#[derive(Debug, Default, Clone, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum CheckerConfig {
    /// Compare the output file with the answer file token by token,
    /// ignoring whitespace differences.
    #[default]
    Token,
    /// Testlib checker.
    /// The checker path is relative to the organizer workspace root of that task.
    /// The checker will be run with the following arguments:
    /// checker --testset <test_set_id> <input_file> <output_file> <answer_file> <report_file>
    Testlib {
        path: String,
        #[serde(default = "default_checker_timeout_ms")]
        timeout_ms: u64,
        #[serde(default = "default_checker_memory_limit_kb")]
        memory_limit_kb: u64,
        /// Extra host directories to bind-mount (read-only) into the checker sandbox.
        #[serde(default)]
        bind_dirs: Vec<std::path::PathBuf>,
    },
}

fn default_checker_timeout_ms() -> u64 {
    10000 // 10s
}

fn default_checker_memory_limit_kb() -> u64 {
    1024 * 1024 // 1GiB
}

/// A test case is a single input-output pair.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FileIoConfig {
    /// File the submission reads its input from, relative to the sandbox root.
    pub input_file: String,
    /// File the submission writes its output to, relative to the sandbox root.
    pub output_file: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestCaseConfig {
    pub id: String,
    /// Input file path, relative to the organizer workspace root of that task.
    pub input_file: String,
    /// Answer file path, relative to the organizer workspace root of that task.
    pub answer_file: String,
    /// Time limit in milliseconds. If not specified, the default time limit will be used.
    #[serde(default = "default_time_limit_ms")]
    pub time_limit_ms: u64,
    /// Memory limit in kilobytes. If not specified, the default memory limit will be used.
    #[serde(default = "default_memory_limit_kb")]
    pub memory_limit_kb: u64,
}

fn default_time_limit_ms() -> u64 {
    1000 // 1s
}

fn default_memory_limit_kb() -> u64 {
    1024 * 1024 // 1GiB
}

/// A test set is a collection of test cases. A test case can only belong to one test set.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestSetConfig {
    pub id: String,
    #[serde(default)]
    pub test_case_ids: Vec<String>,
}

/// A group is a collection of test sets with a score assigned to it.
/// A test set can belong to multiple groups.
/// A relative score (between 0 and 1) is reported in a single test case.
/// The relative score of a test set is the minimum relative score of the test cases in it.
/// The score of a group is max_score * minimum relative score of the test sets in it.
/// The score of the task is the sum of the scores of all groups.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GroupConfig {
    pub id: String,
    #[serde(default)]
    pub test_set_ids: Vec<String>,
    pub max_score: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ContestantConfig {
    pub id: String,
    pub name: String,
    /// Contestant workspace root, relative to the contest root.
    pub workspace_root: String,
    /// Language of the contestant's submission, keyed by task ID.
    pub language: HashMap<String, String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ContestantTaskResult {
    pub contestant_id: String,
    pub task_id: String,
    /// Compilation error message, if the submission failed to compile.
    #[serde(default)]
    pub compilation_error: Option<String>,
    /// Test case results, keyed by test case ID.
    #[serde(default)]
    pub test_case_results: HashMap<String, TestCaseResult>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestCaseResult {
    pub verdict: TestCaseVerdict,
    pub time_used_ms: Option<u64>,
    pub memory_used_kb: Option<u64>,
    pub message: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum TestCaseVerdict {
    Accepted,
    WrongAnswer,
    PartiallyCorrect(f64),
    TimeLimitExceeded,
    MemoryLimitExceeded,
    RuntimeError,
    InternalError,
    /// A required file (submission source, test input/answer, etc.) was missing
    /// on disk. The test case scores 0; the `message` field carries the path.
    FileError,
    Skipped,
}

impl ContestConfig {
    pub fn task(&self, task_id: &str) -> Option<&TaskConfig> {
        self.tasks.iter().find(|t| t.id == task_id)
    }

    pub fn contestant(&self, contestant_id: &str) -> Option<&ContestantConfig> {
        self.contestants.iter().find(|c| c.id == contestant_id)
    }
}
