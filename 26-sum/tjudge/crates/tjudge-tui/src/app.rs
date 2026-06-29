use std::path::PathBuf;
use std::sync::Arc;
use std::sync::atomic::AtomicBool;
use std::sync::mpsc;

use tui_input::Input;

use tjudge_store::schema::{
    ContestConfig, ContestResults, ContestantConfig, TaskConfig, TestCaseResult,
};
use tjudge_store::{save_config, save_results};

// ---------------------------------------------------------------------------
// Panel / sub-tab enums
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Panel {
    Tasks,
    Contestants,
    Results,
}

impl Panel {
    pub fn next(&self) -> Panel {
        match self {
            Panel::Tasks => Panel::Contestants,
            Panel::Contestants => Panel::Results,
            Panel::Results => Panel::Tasks,
        }
    }
    pub fn prev(&self) -> Panel {
        match self {
            Panel::Tasks => Panel::Results,
            Panel::Contestants => Panel::Tasks,
            Panel::Results => Panel::Contestants,
        }
    }
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub enum TaskSubTab {
    #[default]
    TestCases,
    TestSets,
    Groups,
    RunConfigs,
}

impl TaskSubTab {
    pub fn next(&self) -> TaskSubTab {
        match self {
            TaskSubTab::TestCases => TaskSubTab::TestSets,
            TaskSubTab::TestSets => TaskSubTab::Groups,
            TaskSubTab::Groups => TaskSubTab::RunConfigs,
            TaskSubTab::RunConfigs => TaskSubTab::TestCases,
        }
    }
    pub fn prev(&self) -> TaskSubTab {
        match self {
            TaskSubTab::TestCases => TaskSubTab::RunConfigs,
            TaskSubTab::TestSets => TaskSubTab::TestCases,
            TaskSubTab::Groups => TaskSubTab::TestSets,
            TaskSubTab::RunConfigs => TaskSubTab::Groups,
        }
    }
    pub fn all() -> &'static [TaskSubTab] {
        &[
            TaskSubTab::TestCases,
            TaskSubTab::TestSets,
            TaskSubTab::Groups,
            TaskSubTab::RunConfigs,
        ]
    }
    pub fn label(&self) -> &'static str {
        match self {
            TaskSubTab::TestCases => "Test Cases",
            TaskSubTab::TestSets => "Test Sets",
            TaskSubTab::Groups => "Groups",
            TaskSubTab::RunConfigs => "Run Configs",
        }
    }
}

// ---------------------------------------------------------------------------
// Delete target
// ---------------------------------------------------------------------------

#[derive(Debug, Clone)]
pub enum DeleteTarget {
    Task(usize),
    TestCase { task_idx: usize, tc_idx: usize },
    TestSet { task_idx: usize, ts_idx: usize },
    Group { task_idx: usize, group_idx: usize },
    RunConfig { task_idx: usize, lang: String },
    Contestant(usize),
}

// ---------------------------------------------------------------------------
// Form definitions
// ---------------------------------------------------------------------------

#[derive(Debug, Clone)]
pub enum FormKind {
    NewTask,
    EditTask(usize),
    NewTestCase {
        task_idx: usize,
    },
    EditTestCase {
        task_idx: usize,
        tc_idx: usize,
    },
    NewTestSet {
        task_idx: usize,
    },
    EditTestSet {
        task_idx: usize,
        ts_idx: usize,
    },
    NewGroup {
        task_idx: usize,
    },
    EditGroup {
        task_idx: usize,
        group_idx: usize,
    },
    NewRunConfig {
        task_idx: usize,
    },
    EditRunConfig {
        task_idx: usize,
        lang: String,
    },
    NewContestant,
    EditContestant(usize),
    EditContestantLanguage {
        contestant_idx: usize,
        task_id: String,
    },
}

#[derive(Debug, Clone)]
pub struct FormField {
    pub label: &'static str,
    pub input: Input,
    pub error: Option<String>,
    pub required: bool,
}

impl FormField {
    pub fn new(label: &'static str, value: impl Into<String>, required: bool) -> Self {
        Self {
            label,
            input: Input::new(value.into()),
            error: None,
            required,
        }
    }
    pub fn value(&self) -> &str {
        self.input.value()
    }
}

/// A checklist field where items can be toggled.
#[derive(Debug, Clone)]
pub struct ChecklistField {
    pub label: &'static str,
    pub items: Vec<(String, bool)>, // (id, selected)
    pub cursor: usize,
}

impl ChecklistField {
    pub fn new(label: &'static str, all_ids: Vec<String>, selected: &[String]) -> Self {
        let items = all_ids
            .into_iter()
            .map(|id| {
                let sel = selected.contains(&id);
                (id, sel)
            })
            .collect();
        Self {
            label,
            items,
            cursor: 0,
        }
    }
    pub fn selected_ids(&self) -> Vec<String> {
        self.items
            .iter()
            .filter(|(_, sel)| *sel)
            .map(|(id, _)| id.clone())
            .collect()
    }
    pub fn toggle_current(&mut self) {
        if let Some(item) = self.items.get_mut(self.cursor) {
            item.1 = !item.1;
        }
    }
    pub fn up(&mut self) {
        if self.cursor > 0 {
            self.cursor -= 1;
        }
    }
    pub fn down(&mut self) {
        if self.cursor + 1 < self.items.len() {
            self.cursor += 1;
        }
    }
}

/// A popup form. `checklist` is optional for forms that need a member-picker.
#[derive(Debug, Clone)]
pub struct FormState {
    pub title: &'static str,
    pub kind: FormKind,
    pub fields: Vec<FormField>,
    pub checklist: Option<ChecklistField>,
    /// 0..fields.len() = text fields; fields.len() = checklist (if present)
    pub focused: usize,
    pub in_checklist: bool,
}

impl FormState {
    pub fn field_count(&self) -> usize {
        self.fields.len() + self.checklist.is_some() as usize
    }
    pub fn next_field(&mut self) {
        let total = self.field_count();
        if total == 0 {
            return;
        }
        let next = (self.focused + 1) % total;
        self.focused = next;
        self.in_checklist = self.checklist.is_some() && next == self.fields.len();
    }
    pub fn prev_field(&mut self) {
        let total = self.field_count();
        if total == 0 {
            return;
        }
        let prev = if self.focused == 0 {
            total - 1
        } else {
            self.focused - 1
        };
        self.focused = prev;
        self.in_checklist = self.checklist.is_some() && prev == self.fields.len();
    }
}

// ---------------------------------------------------------------------------
// App mode
// ---------------------------------------------------------------------------

#[derive(Debug)]
pub enum Mode {
    Startup {
        input: Input,
        error: Option<String>,
    },
    Normal,
    PopupForm(FormState),
    ConfirmDelete {
        message: String,
        target: DeleteTarget,
    },
    /// Full-screen contestant details panel (entered from the Results panel
    /// with `d`). Carries its own navigation state.
    ContestantDetail(ContestantDetailState),
    Judging,
    Quit,
}

// ---------------------------------------------------------------------------
// Per-panel list state helper
// ---------------------------------------------------------------------------

#[derive(Debug, Default, Clone)]
pub struct ListState {
    pub selected: usize,
}

impl ListState {
    pub fn select_up(&mut self, len: usize) {
        if len == 0 {
            return;
        }
        if self.selected > 0 {
            self.selected -= 1;
        }
    }
    pub fn select_down(&mut self, len: usize) {
        if len == 0 {
            return;
        }
        if self.selected + 1 < len {
            self.selected += 1;
        }
    }
    pub fn clamp(&mut self, len: usize) {
        if len == 0 {
            self.selected = 0;
        } else if self.selected >= len {
            self.selected = len - 1;
        }
    }
}

// ---------------------------------------------------------------------------
// Tasks panel state
// ---------------------------------------------------------------------------

#[derive(Debug, Default)]
pub struct TasksPanelState {
    pub task_list: ListState,
    pub sub_tab: TaskSubTab,
    pub tc_list: ListState,
    pub ts_list: ListState,
    pub group_list: ListState,
    pub rc_list: ListState,
    /// Whether focus is on the left (task list) or right (sub-tab detail)
    pub focus_right: bool,
}

// ---------------------------------------------------------------------------
// Contestants panel state
// ---------------------------------------------------------------------------

#[derive(Debug, Default)]
pub struct ContestantsPanelState {
    pub list: ListState,
    pub lang_list: ListState,
    pub focus_right: bool,
}

// ---------------------------------------------------------------------------
// Results panel state
// ---------------------------------------------------------------------------

#[derive(Debug, Default, Clone)]
pub struct ResultsPanelState {
    /// Cursor row in the rank table. `0` is the header row; `1..=n` are rank
    /// positions (1 = top of the ranking).
    pub cursor_row: usize,
    /// Cursor column: `0` = Rank, `1` = Name, `2` = Total, `3..` = task columns
    /// (task index = `cursor_col - 3`).
    pub cursor_col: usize,
    /// Scroll offset of the right-side detail panel.
    pub detail_scroll: usize,
}

impl ResultsPanelState {
    /// Number of navigable columns: Rank + Name + Total + one per task.
    pub fn col_count(&self, task_count: usize) -> usize {
        3 + task_count
    }
    /// Rank position (0-based among contestants) of the cursor, or `None` when
    /// the cursor sits on the header row.
    pub fn rank_pos(&self) -> Option<usize> {
        if self.cursor_row == 0 {
            None
        } else {
            Some(self.cursor_row - 1)
        }
    }
}

/// State for the full-screen contestant details panel (entered with `d`).
#[derive(Debug, Clone)]
pub struct ContestantDetailState {
    /// Index into `config.contestants` (original, not rank-sorted).
    pub contestant_idx: usize,
    /// When set, only this task's detail is shown (entered from a score cell);
    /// `None` shows all of the contestant's tasks.
    pub task_filter: Option<usize>,
    /// Selected row in the flat test-case list.
    pub selected: usize,
    /// Scroll offset of the message pane.
    pub msg_scroll: usize,
}

// ---------------------------------------------------------------------------
// Judging channel messages
// ---------------------------------------------------------------------------

pub enum JudgeMsg {
    /// A single test case finished judging (streamed, for per-test-case progress).
    TestCase {
        contestant_id: String,
        task_id: String,
        test_case_id: String,
        result: TestCaseResult,
    },
    /// All test cases for a (contestant, task) have been reported; carries the
    /// task-level compilation error, if any.
    TaskComplete {
        contestant_id: String,
        task_id: String,
        compilation_error: Option<String>,
    },
    Error(String),
    Done,
}

// ---------------------------------------------------------------------------
// Main App struct
// ---------------------------------------------------------------------------

pub struct App {
    pub config: ContestConfig,
    pub results: ContestResults,
    pub config_path: PathBuf,
    pub results_path: PathBuf,
    pub mode: Mode,
    pub active_panel: Panel,
    pub tasks: TasksPanelState,
    pub contestants: ContestantsPanelState,
    pub results_state: ResultsPanelState,
    pub error_flash: Option<String>,
    pub judge_rx: Option<mpsc::Receiver<JudgeMsg>>,
    /// Shared cancel flag for the in-progress judge run; `None` when not judging.
    pub judge_cancel: Option<Arc<AtomicBool>>,
    pub judge_pending: usize,
}

impl App {
    pub fn new_startup() -> Self {
        Self {
            config: ContestConfig {
                meta: tjudge_store::schema::ContestMeta {
                    id: String::new(),
                    title: String::new(),
                    schema_version: "1.0".to_string(),
                },
                tasks: vec![],
                contestants: vec![],
            },
            results: ContestResults { results: vec![] },
            config_path: PathBuf::new(),
            results_path: PathBuf::new(),
            mode: Mode::Startup {
                input: Input::default(),
                error: None,
            },
            active_panel: Panel::Tasks,
            tasks: TasksPanelState::default(),
            contestants: ContestantsPanelState::default(),
            results_state: ResultsPanelState::default(),
            error_flash: None,
            judge_rx: None,
            judge_cancel: None,
            judge_pending: 0,
        }
    }

    /// Save config, flashing any error.
    pub fn auto_save_config(&mut self) {
        if let Err(e) = save_config(&self.config, &self.config_path) {
            self.error_flash = Some(format!("Save failed: {e}"));
        }
    }

    /// Save results, flashing any error.
    pub fn auto_save_results(&mut self) {
        if let Err(e) = save_results(&self.results, &self.results_path) {
            self.error_flash = Some(format!("Save results failed: {e}"));
        }
    }

    // ── Convenience accessors ────────────────────────────────────────────────

    pub fn selected_task(&self) -> Option<&TaskConfig> {
        self.config.tasks.get(self.tasks.task_list.selected)
    }

    #[allow(dead_code)]
    pub fn selected_task_mut(&mut self) -> Option<&mut TaskConfig> {
        let idx = self.tasks.task_list.selected;
        self.config.tasks.get_mut(idx)
    }

    pub fn selected_contestant(&self) -> Option<&ContestantConfig> {
        self.config.contestants.get(self.contestants.list.selected)
    }
}
