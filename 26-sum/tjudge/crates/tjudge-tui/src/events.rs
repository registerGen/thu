use std::collections::{HashMap, HashSet};

use crossterm::event::{KeyCode, KeyEvent, KeyModifiers};
use tui_input::backend::crossterm::EventHandler;

use tjudge_store::schema::{
    CheckerConfig, CompilationConfig, ContestConfig, ContestResults, ContestantConfig, GroupConfig,
    RunConfig, TaskConfig, TestCaseConfig, TestSetConfig,
};
use tjudge_store::{ValidationError, collect_unique_ids, validate_results, validate_task};

use crate::app::{
    App, ChecklistField, ContestantDetailState, DeleteTarget, FormField, FormKind, FormState, Mode,
    Panel, TaskSubTab,
};
use crate::judging::spawn_judge_all;
use tjudge_judger::JudgeRequest;

pub fn handle_key(app: &mut App, key: KeyEvent) {
    // Clear any stale error flash on keypress
    app.error_flash = None;

    match &app.mode {
        Mode::Startup { .. } => handle_startup(app, key),
        Mode::Normal => handle_normal(app, key),
        Mode::PopupForm(_) => handle_popup(app, key),
        Mode::ConfirmDelete { .. } => handle_confirm(app, key),
        Mode::ContestantDetail(_) => handle_contestant_detail(app, key),
        Mode::Judging => handle_judging(app, key),
        Mode::Quit => {}
    }
}

// ---------------------------------------------------------------------------
// Judging mode (only cancel is available)
// ---------------------------------------------------------------------------

fn handle_judging(app: &mut App, key: KeyEvent) {
    match key.code {
        KeyCode::Esc | KeyCode::Char('c') | KeyCode::Char('C') => {
            crate::judging::cancel_judging(app);
        }
        _ => {}
    }
}

// ---------------------------------------------------------------------------
// Startup
// ---------------------------------------------------------------------------

fn handle_startup(app: &mut App, key: KeyEvent) {
    match key.code {
        KeyCode::Enter => {
            let path_str = match &app.mode {
                Mode::Startup { input, .. } => input.value().trim().to_string(),
                _ => return,
            };
            let path = std::path::PathBuf::from(&path_str);
            // Canonicalize so all derived paths (bind mounts, stdin, stdout) are absolute.
            let path = match path.canonicalize() {
                Ok(p) => p,
                Err(e) => {
                    let msg = format!("Cannot resolve path: {e}");
                    let input = match &app.mode {
                        Mode::Startup { input, .. } => input.clone(),
                        _ => return,
                    };
                    app.mode = Mode::Startup {
                        input,
                        error: Some(msg),
                    };
                    return;
                }
            };
            match tjudge_store::load_config(&path) {
                Ok(config) => {
                    let results_path = path
                        .parent()
                        .unwrap_or(std::path::Path::new("."))
                        .join("results.toml");
                    let results = tjudge_store::load_results(&results_path)
                        .unwrap_or(tjudge_store::schema::ContestResults { results: vec![] });
                    app.config = config;
                    app.results = results;
                    app.config_path = path;
                    app.results_path = results_path;
                    app.mode = Mode::Normal;
                }
                Err(e) => {
                    let msg = e.to_string();
                    let input = match &app.mode {
                        Mode::Startup { input, .. } => input.clone(),
                        _ => return,
                    };
                    app.mode = Mode::Startup {
                        input,
                        error: Some(msg),
                    };
                }
            }
        }
        KeyCode::Esc => {
            app.mode = Mode::Quit;
        }
        _ => {
            if let Mode::Startup { input, .. } = &mut app.mode {
                input.handle_event(&crossterm::event::Event::Key(key));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Normal mode
// ---------------------------------------------------------------------------

fn handle_normal(app: &mut App, key: KeyEvent) {
    match key.code {
        KeyCode::Char('q') | KeyCode::Esc => {
            app.mode = Mode::Quit;
        }
        KeyCode::Tab => {
            // When in Tasks panel with focus on the right, Tab cycles sub-tabs.
            if matches!(app.active_panel, Panel::Tasks) && app.tasks.focus_right {
                app.tasks.sub_tab = app.tasks.sub_tab.next();
            } else {
                app.active_panel = app.active_panel.next();
            }
        }
        KeyCode::BackTab => {
            // When in Tasks panel with focus on the right, Shift-Tab cycles sub-tabs backward.
            if matches!(app.active_panel, Panel::Tasks) && app.tasks.focus_right {
                app.tasks.sub_tab = app.tasks.sub_tab.prev();
            } else {
                // Global reverse panel cycling.
                app.active_panel = app.active_panel.prev();
            }
        }
        _ => match app.active_panel.clone() {
            Panel::Tasks => handle_tasks(app, key),
            Panel::Contestants => handle_contestants(app, key),
            Panel::Results => handle_results(app, key),
        },
    }
}

// ---------------------------------------------------------------------------
// Tasks panel
// ---------------------------------------------------------------------------

fn handle_tasks(app: &mut App, key: KeyEvent) {
    match key.code {
        KeyCode::Left | KeyCode::Char('h') => {
            app.tasks.focus_right = false;
        }
        KeyCode::Right | KeyCode::Char('l') => {
            if !app.config.tasks.is_empty() {
                app.tasks.focus_right = true;
            }
        }
        KeyCode::Up | KeyCode::Char('k') => {
            if app.tasks.focus_right {
                tasks_detail_up(app);
            } else {
                let len = app.config.tasks.len();
                app.tasks.task_list.select_up(len);
                clamp_task_sublists(app);
            }
        }
        KeyCode::Down | KeyCode::Char('j') => {
            if app.tasks.focus_right {
                tasks_detail_down(app);
            } else {
                let len = app.config.tasks.len();
                app.tasks.task_list.select_down(len);
                clamp_task_sublists(app);
            }
        }
        KeyCode::Char('n') => open_new_task_form(app),
        KeyCode::Char('e') | KeyCode::Enter => open_edit_task(app),
        KeyCode::Char('d') => confirm_delete_task(app),
        _ => {}
    }
}

fn tasks_detail_up(app: &mut App) {
    let tidx = app.tasks.task_list.selected;
    let task = match app.config.tasks.get(tidx) {
        Some(t) => t,
        None => return,
    };
    match app.tasks.sub_tab {
        TaskSubTab::TestCases => app.tasks.tc_list.select_up(task.test_cases.len()),
        TaskSubTab::TestSets => app.tasks.ts_list.select_up(task.test_sets.len()),
        TaskSubTab::Groups => app.tasks.group_list.select_up(task.groups.len()),
        TaskSubTab::RunConfigs => app.tasks.rc_list.select_up(task.run_config.len()),
    }
}

fn tasks_detail_down(app: &mut App) {
    let tidx = app.tasks.task_list.selected;
    let task = match app.config.tasks.get(tidx) {
        Some(t) => t,
        None => return,
    };
    match app.tasks.sub_tab {
        TaskSubTab::TestCases => app.tasks.tc_list.select_down(task.test_cases.len()),
        TaskSubTab::TestSets => app.tasks.ts_list.select_down(task.test_sets.len()),
        TaskSubTab::Groups => app.tasks.group_list.select_down(task.groups.len()),
        TaskSubTab::RunConfigs => app.tasks.rc_list.select_down(task.run_config.len()),
    }
}

fn clamp_task_sublists(app: &mut App) {
    let tidx = app.tasks.task_list.selected;
    if let Some(task) = app.config.tasks.get(tidx) {
        app.tasks.tc_list.clamp(task.test_cases.len());
        app.tasks.ts_list.clamp(task.test_sets.len());
        app.tasks.group_list.clamp(task.groups.len());
        app.tasks.rc_list.clamp(task.run_config.len());
    }
}

fn open_new_task_form(app: &mut App) {
    let state = FormState {
        title: "New Task",
        kind: FormKind::NewTask,
        fields: vec![
            FormField::new("ID", "", true),
            FormField::new("Title", "", true),
            FormField::new("Workspace Root", "", true),
            FormField::new("Checker (token / testlib:<path>)", "token", false),
        ],
        checklist: None,
        focused: 0,
        in_checklist: false,
    };
    app.mode = Mode::PopupForm(state);
}

fn open_edit_task(app: &mut App) {
    let tidx = app.tasks.task_list.selected;
    if app.tasks.focus_right {
        // Edit sub-tab item
        open_edit_subtab(app);
        return;
    }
    let task = match app.config.tasks.get(tidx) {
        Some(t) => t,
        None => return,
    };
    let checker_str = match &task.checker_config {
        CheckerConfig::Token => "token".to_string(),
        CheckerConfig::Testlib { path, .. } => format!("testlib:{path}"),
    };
    let state = FormState {
        title: "Edit Task",
        kind: FormKind::EditTask(tidx),
        fields: vec![
            FormField::new("ID", task.id.clone(), true),
            FormField::new("Title", task.title.clone(), true),
            FormField::new("Workspace Root", task.workspace_root.clone(), true),
            FormField::new("Checker (token / testlib:<path>)", checker_str, false),
        ],
        checklist: None,
        focused: 0,
        in_checklist: false,
    };
    app.mode = Mode::PopupForm(state);
}

fn open_edit_subtab(app: &mut App) {
    let tidx = app.tasks.task_list.selected;
    match app.tasks.sub_tab {
        TaskSubTab::TestCases => {
            let tc_idx = app.tasks.tc_list.selected;
            let task = match app.config.tasks.get(tidx) {
                Some(t) => t,
                None => return,
            };
            if let Some(tc) = task.test_cases.get(tc_idx) {
                let state = FormState {
                    title: "Edit Test Case",
                    kind: FormKind::EditTestCase {
                        task_idx: tidx,
                        tc_idx,
                    },
                    fields: vec![
                        FormField::new("ID", tc.id.clone(), true),
                        FormField::new("Input File", tc.input_file.clone(), true),
                        FormField::new("Answer File", tc.answer_file.clone(), true),
                        FormField::new("Time Limit (ms)", tc.time_limit_ms.to_string(), true),
                        FormField::new("Memory Limit (KiB)", tc.memory_limit_kb.to_string(), true),
                    ],
                    checklist: None,
                    focused: 0,
                    in_checklist: false,
                };
                app.mode = Mode::PopupForm(state);
            } else {
                open_new_subtab(app);
            }
        }
        TaskSubTab::TestSets => {
            let ts_idx = app.tasks.ts_list.selected;
            let task = match app.config.tasks.get(tidx) {
                Some(t) => t,
                None => return,
            };
            if let Some(ts) = task.test_sets.get(ts_idx) {
                let all_tc_ids: Vec<String> =
                    task.test_cases.iter().map(|tc| tc.id.clone()).collect();
                let state = FormState {
                    title: "Edit Test Set",
                    kind: FormKind::EditTestSet {
                        task_idx: tidx,
                        ts_idx,
                    },
                    fields: vec![FormField::new("ID", ts.id.clone(), true)],
                    checklist: Some(ChecklistField::new(
                        "Test Case Members",
                        all_tc_ids,
                        &ts.test_case_ids,
                    )),
                    focused: 0,
                    in_checklist: false,
                };
                app.mode = Mode::PopupForm(state);
            } else {
                open_new_subtab(app);
            }
        }
        TaskSubTab::Groups => {
            let g_idx = app.tasks.group_list.selected;
            let task = match app.config.tasks.get(tidx) {
                Some(t) => t,
                None => return,
            };
            if let Some(g) = task.groups.get(g_idx) {
                let all_ts_ids: Vec<String> =
                    task.test_sets.iter().map(|ts| ts.id.clone()).collect();
                let state = FormState {
                    title: "Edit Group",
                    kind: FormKind::EditGroup {
                        task_idx: tidx,
                        group_idx: g_idx,
                    },
                    fields: vec![
                        FormField::new("ID", g.id.clone(), true),
                        FormField::new("Max Score", g.max_score.to_string(), true),
                    ],
                    checklist: Some(ChecklistField::new(
                        "Test Set Members",
                        all_ts_ids,
                        &g.test_set_ids,
                    )),
                    focused: 0,
                    in_checklist: false,
                };
                app.mode = Mode::PopupForm(state);
            } else {
                open_new_subtab(app);
            }
        }
        TaskSubTab::RunConfigs => {
            let rc_idx = app.tasks.rc_list.selected;
            let task = match app.config.tasks.get(tidx) {
                Some(t) => t,
                None => return,
            };
            let mut entries: Vec<(&String, &RunConfig)> = task.run_config.iter().collect();
            entries.sort_by_key(|(k, _)| k.as_str());
            if let Some((lang, rc)) = entries.get(rc_idx) {
                let compile_cmd = rc
                    .compilation_config
                    .as_ref()
                    .map(|c| c.compile_command.join(" "))
                    .unwrap_or_default();
                let compile_output = rc
                    .compilation_config
                    .as_ref()
                    .map(|c| c.compilation_output_file.clone())
                    .unwrap_or_default();
                let state = FormState {
                    title: "Edit Run Config",
                    kind: FormKind::EditRunConfig {
                        task_idx: tidx,
                        lang: lang.to_string(),
                    },
                    fields: vec![
                        FormField::new("Language", lang.to_string(), true),
                        FormField::new(
                            "Run Command (space-separated)",
                            rc.run_command.join(" "),
                            true,
                        ),
                        FormField::new("Compile Command (empty = interpreted)", compile_cmd, false),
                        FormField::new("Compile Output File", compile_output, false),
                        FormField::new("Max Processes", rc.max_processes.to_string(), false),
                    ],
                    checklist: None,
                    focused: 0,
                    in_checklist: false,
                };
                app.mode = Mode::PopupForm(state);
            } else {
                open_new_subtab(app);
            }
        }
    }
}

fn open_new_subtab(app: &mut App) {
    let tidx = app.tasks.task_list.selected;
    if app.config.tasks.get(tidx).is_none() {
        return;
    }
    match app.tasks.sub_tab {
        TaskSubTab::TestCases => {
            let state = FormState {
                title: "New Test Case",
                kind: FormKind::NewTestCase { task_idx: tidx },
                fields: vec![
                    FormField::new("ID", "", true),
                    FormField::new("Input File", "", true),
                    FormField::new("Answer File", "", true),
                    FormField::new("Time Limit (ms)", "1000", true),
                    FormField::new("Memory Limit (KiB)", "1048576", true),
                ],
                checklist: None,
                focused: 0,
                in_checklist: false,
            };
            app.mode = Mode::PopupForm(state);
        }
        TaskSubTab::TestSets => {
            let all_tc_ids: Vec<String> = app.config.tasks[tidx]
                .test_cases
                .iter()
                .map(|tc| tc.id.clone())
                .collect();
            let state = FormState {
                title: "New Test Set",
                kind: FormKind::NewTestSet { task_idx: tidx },
                fields: vec![FormField::new("ID", "", true)],
                checklist: Some(ChecklistField::new("Test Case Members", all_tc_ids, &[])),
                focused: 0,
                in_checklist: false,
            };
            app.mode = Mode::PopupForm(state);
        }
        TaskSubTab::Groups => {
            let all_ts_ids: Vec<String> = app.config.tasks[tidx]
                .test_sets
                .iter()
                .map(|ts| ts.id.clone())
                .collect();
            let state = FormState {
                title: "New Group",
                kind: FormKind::NewGroup { task_idx: tidx },
                fields: vec![
                    FormField::new("ID", "", true),
                    FormField::new("Max Score", "100", true),
                ],
                checklist: Some(ChecklistField::new("Test Set Members", all_ts_ids, &[])),
                focused: 0,
                in_checklist: false,
            };
            app.mode = Mode::PopupForm(state);
        }
        TaskSubTab::RunConfigs => {
            let state = FormState {
                title: "New Run Config",
                kind: FormKind::NewRunConfig { task_idx: tidx },
                fields: vec![
                    FormField::new("Language", "", true),
                    FormField::new("Run Command (space-separated)", "", true),
                    FormField::new("Compile Command (empty = interpreted)", "", false),
                    FormField::new("Compile Output File", "", false),
                    FormField::new("Max Processes", "1", false),
                ],
                checklist: None,
                focused: 0,
                in_checklist: false,
            };
            app.mode = Mode::PopupForm(state);
        }
    }
}

fn confirm_delete_task(app: &mut App) {
    if app.tasks.focus_right {
        confirm_delete_subtab(app);
        return;
    }
    let tidx = app.tasks.task_list.selected;
    let task = match app.config.tasks.get(tidx) {
        Some(t) => t,
        None => return,
    };
    let msg = format!("Delete task '{}'?", task.id);
    app.mode = Mode::ConfirmDelete {
        message: msg,
        target: DeleteTarget::Task(tidx),
    };
}

fn confirm_delete_subtab(app: &mut App) {
    let tidx = app.tasks.task_list.selected;
    let task = match app.config.tasks.get(tidx) {
        Some(t) => t,
        None => return,
    };
    match app.tasks.sub_tab {
        TaskSubTab::TestCases => {
            let idx = app.tasks.tc_list.selected;
            if let Some(tc) = task.test_cases.get(idx) {
                let msg = format!("Delete test case '{}'?", tc.id);
                app.mode = Mode::ConfirmDelete {
                    message: msg,
                    target: DeleteTarget::TestCase {
                        task_idx: tidx,
                        tc_idx: idx,
                    },
                };
            }
        }
        TaskSubTab::TestSets => {
            let idx = app.tasks.ts_list.selected;
            if let Some(ts) = task.test_sets.get(idx) {
                let msg = format!("Delete test set '{}'?", ts.id);
                app.mode = Mode::ConfirmDelete {
                    message: msg,
                    target: DeleteTarget::TestSet {
                        task_idx: tidx,
                        ts_idx: idx,
                    },
                };
            }
        }
        TaskSubTab::Groups => {
            let idx = app.tasks.group_list.selected;
            if let Some(g) = task.groups.get(idx) {
                let msg = format!("Delete group '{}'?", g.id);
                app.mode = Mode::ConfirmDelete {
                    message: msg,
                    target: DeleteTarget::Group {
                        task_idx: tidx,
                        group_idx: idx,
                    },
                };
            }
        }
        TaskSubTab::RunConfigs => {
            let idx = app.tasks.rc_list.selected;
            let mut entries: Vec<&String> = task.run_config.keys().collect();
            entries.sort();
            if let Some(lang) = entries.get(idx) {
                let msg = format!("Delete run config for '{}'?", lang);
                app.mode = Mode::ConfirmDelete {
                    message: msg,
                    target: DeleteTarget::RunConfig {
                        task_idx: tidx,
                        lang: lang.to_string(),
                    },
                };
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Contestants panel
// ---------------------------------------------------------------------------

fn handle_contestants(app: &mut App, key: KeyEvent) {
    match key.code {
        KeyCode::Left | KeyCode::Char('h') => app.contestants.focus_right = false,
        KeyCode::Right | KeyCode::Char('l') => {
            if !app.config.contestants.is_empty() {
                app.contestants.focus_right = true;
            }
        }
        KeyCode::Up | KeyCode::Char('k') => {
            if app.contestants.focus_right {
                let len = app.config.tasks.len();
                app.contestants.lang_list.select_up(len);
            } else {
                let len = app.config.contestants.len();
                app.contestants.list.select_up(len);
            }
        }
        KeyCode::Down | KeyCode::Char('j') => {
            if app.contestants.focus_right {
                let len = app.config.tasks.len();
                app.contestants.lang_list.select_down(len);
            } else {
                let len = app.config.contestants.len();
                app.contestants.list.select_down(len);
            }
        }
        KeyCode::Char('n') => {
            if !app.contestants.focus_right {
                let state = FormState {
                    title: "New Contestant",
                    kind: FormKind::NewContestant,
                    fields: vec![
                        FormField::new("ID", "", true),
                        FormField::new("Name", "", true),
                        FormField::new("Workspace Root", "", true),
                    ],
                    checklist: None,
                    focused: 0,
                    in_checklist: false,
                };
                app.mode = Mode::PopupForm(state);
            }
        }
        KeyCode::Char('e') | KeyCode::Enter => {
            if app.contestants.focus_right {
                // Edit language for selected task
                let cidx = app.contestants.list.selected;
                let contestant = match app.config.contestants.get(cidx) {
                    Some(c) => c,
                    None => return,
                };
                let task_ids: Vec<&str> = {
                    let mut ids: Vec<&str> =
                        app.config.tasks.iter().map(|t| t.id.as_str()).collect();
                    ids.sort_unstable();
                    ids
                };
                let lang_idx = app.contestants.lang_list.selected;
                if let Some(task_id) = task_ids.get(lang_idx) {
                    let current_lang = contestant
                        .language
                        .get(*task_id)
                        .cloned()
                        .unwrap_or_default();
                    let state = FormState {
                        title: "Edit Language",
                        kind: FormKind::EditContestantLanguage {
                            contestant_idx: cidx,
                            task_id: task_id.to_string(),
                        },
                        fields: vec![FormField::new("Language", current_lang, true)],
                        checklist: None,
                        focused: 0,
                        in_checklist: false,
                    };
                    app.mode = Mode::PopupForm(state);
                }
            } else {
                let cidx = app.contestants.list.selected;
                if let Some(c) = app.config.contestants.get(cidx) {
                    let state = FormState {
                        title: "Edit Contestant",
                        kind: FormKind::EditContestant(cidx),
                        fields: vec![
                            FormField::new("ID", c.id.clone(), true),
                            FormField::new("Name", c.name.clone(), true),
                            FormField::new("Workspace Root", c.workspace_root.clone(), true),
                        ],
                        checklist: None,
                        focused: 0,
                        in_checklist: false,
                    };
                    app.mode = Mode::PopupForm(state);
                }
            }
        }
        KeyCode::Char('d') if !app.contestants.focus_right => {
            let cidx = app.contestants.list.selected;
            if let Some(c) = app.config.contestants.get(cidx) {
                let msg = format!("Delete contestant '{}'?", c.id);
                app.mode = Mode::ConfirmDelete {
                    message: msg,
                    target: DeleteTarget::Contestant(cidx),
                };
            }
        }
        _ => {}
    }
}

// ---------------------------------------------------------------------------
// Results panel
// ---------------------------------------------------------------------------

fn handle_results(app: &mut App, key: KeyEvent) {
    let task_count = app.config.tasks.len();
    let col_count = app.results_state.col_count(task_count);
    let row_count = app.config.contestants.len() + 1; // +1 for the header row

    match key.code {
        KeyCode::Up => {
            if app.results_state.cursor_row > 0 {
                app.results_state.cursor_row -= 1;
            }
            app.results_state.detail_scroll = 0;
        }
        KeyCode::Down => {
            if app.results_state.cursor_row + 1 < row_count {
                app.results_state.cursor_row += 1;
            }
            app.results_state.detail_scroll = 0;
        }
        KeyCode::Left => {
            if app.results_state.cursor_col > 0 {
                app.results_state.cursor_col -= 1;
            }
            app.results_state.detail_scroll = 0;
        }
        KeyCode::Right => {
            if app.results_state.cursor_col + 1 < col_count {
                app.results_state.cursor_col += 1;
            }
            app.results_state.detail_scroll = 0;
        }
        KeyCode::PageDown => {
            app.results_state.detail_scroll = app.results_state.detail_scroll.saturating_add(10);
        }
        KeyCode::PageUp => {
            app.results_state.detail_scroll = app.results_state.detail_scroll.saturating_sub(10);
        }
        KeyCode::Char('j') if key.modifiers.is_empty() => {
            judge_from_cursor(app);
        }
        KeyCode::Char('J') | KeyCode::Char('j') if key.modifiers.contains(KeyModifiers::SHIFT) => {
            judge_all(app);
        }
        KeyCode::Enter => {
            // Open the contestant details panel when the cursor is on a
            // contestant row (not the header line). A score cell (task column)
            // opens just that task; name/rank/total open all of the
            // contestant's tasks.
            if let Some(rank_pos) = app.results_state.rank_pos() {
                use crate::panels::results::rank_sorted;
                if let Some(&orig_idx) = rank_sorted(&app.config, &app.results).get(rank_pos) {
                    let task_filter = if app.results_state.cursor_col >= 3 {
                        Some(app.results_state.cursor_col - 3)
                    } else {
                        None
                    };
                    app.mode = Mode::ContestantDetail(ContestantDetailState {
                        contestant_idx: orig_idx,
                        task_filter,
                        selected: 0,
                        msg_scroll: 0,
                    });
                }
            }
        }
        _ => {}
    }
}

/// Resolve what to (re)judge from the current rank-table cursor cell:
/// - header row, task column  → judge that task for all contestants
/// - header row, other column  → judge all
/// - contestant row, name/total column → judge that contestant
/// - contestant row, task column → judge that task of that contestant
/// - contestant row, rank column → judge all
fn judge_from_cursor(app: &mut App) {
    use crate::panels::results::rank_sorted;
    let col = app.results_state.cursor_col;
    let task_idx = col.checked_sub(3);

    match (app.results_state.rank_pos(), task_idx) {
        (Some(rank_pos), Some(t_idx)) => {
            // Contestant row + task column → judge that (contestant, task).
            let sorted = rank_sorted(&app.config, &app.results);
            let orig_idx = match sorted.get(rank_pos) {
                Some(&i) => i,
                None => return,
            };
            let task = match app.config.tasks.get(t_idx) {
                Some(t) => t,
                None => return,
            };
            let contestant = &app.config.contestants[orig_idx];
            spawn_judge_all(
                app,
                vec![JudgeRequest {
                    contestant_id: contestant.id.clone(),
                    task_id: task.id.clone(),
                }],
            );
            app.mode = Mode::Judging;
        }
        (Some(rank_pos), None) if col == 1 || col == 2 => {
            // Contestant row + name/total column → judge that contestant.
            let sorted = rank_sorted(&app.config, &app.results);
            let orig_idx = match sorted.get(rank_pos) {
                Some(&i) => i,
                None => return,
            };
            let contestant = &app.config.contestants[orig_idx];
            let requests: Vec<JudgeRequest> = app
                .config
                .tasks
                .iter()
                .map(|t| JudgeRequest {
                    contestant_id: contestant.id.clone(),
                    task_id: t.id.clone(),
                })
                .collect();
            if requests.is_empty() {
                return;
            }
            spawn_judge_all(app, requests);
            app.mode = Mode::Judging;
        }
        (None, Some(t_idx)) => {
            // Header row + task column → judge that task for all contestants.
            let task = match app.config.tasks.get(t_idx) {
                Some(t) => t,
                None => return,
            };
            let requests: Vec<JudgeRequest> = app
                .config
                .contestants
                .iter()
                .map(|c| JudgeRequest {
                    contestant_id: c.id.clone(),
                    task_id: task.id.clone(),
                })
                .collect();
            if requests.is_empty() {
                return;
            }
            spawn_judge_all(app, requests);
            app.mode = Mode::Judging;
        }
        _ => {
            // Otherwise → judge all.
            judge_all(app);
        }
    }
}

fn judge_all(app: &mut App) {
    let requests: Vec<JudgeRequest> = app
        .config
        .contestants
        .iter()
        .flat_map(|c| {
            app.config.tasks.iter().map(|t| JudgeRequest {
                contestant_id: c.id.clone(),
                task_id: t.id.clone(),
            })
        })
        .collect();
    if requests.is_empty() {
        return;
    }
    spawn_judge_all(app, requests);
    app.mode = Mode::Judging;
}

// ---------------------------------------------------------------------------
// Contestant details panel
// ---------------------------------------------------------------------------

fn handle_contestant_detail(app: &mut App, key: KeyEvent) {
    let (contestant_idx, task_filter) = match &app.mode {
        Mode::ContestantDetail(s) => (s.contestant_idx, s.task_filter),
        _ => return,
    };
    let entry_count = crate::panels::results::detail_lines(
        &app.config,
        &app.results,
        contestant_idx,
        task_filter,
    )
    .len();

    let Mode::ContestantDetail(state) = &mut app.mode else {
        return;
    };
    match key.code {
        KeyCode::Esc | KeyCode::Char('q') => {
            app.mode = Mode::Normal;
        }
        KeyCode::Up | KeyCode::Char('k') => {
            if state.selected > 0 {
                state.selected -= 1;
            }
            state.msg_scroll = 0;
        }
        KeyCode::Down | KeyCode::Char('j') => {
            if state.selected + 1 < entry_count {
                state.selected += 1;
            }
            state.msg_scroll = 0;
        }
        KeyCode::PageDown => {
            state.msg_scroll = state.msg_scroll.saturating_add(10);
        }
        KeyCode::PageUp => {
            state.msg_scroll = state.msg_scroll.saturating_sub(10);
        }
        _ => {}
    }
}

// ---------------------------------------------------------------------------
// Popup form handling
// ---------------------------------------------------------------------------

fn handle_popup(app: &mut App, key: KeyEvent) {
    let Mode::PopupForm(_state) = &mut app.mode else {
        return;
    };

    match key.code {
        KeyCode::Esc => {
            app.mode = Mode::Normal;
            return;
        }
        KeyCode::Tab => {
            let state = if let Mode::PopupForm(s) = &mut app.mode {
                s
            } else {
                return;
            };
            state.next_field();
            return;
        }
        KeyCode::BackTab => {
            let state = if let Mode::PopupForm(s) = &mut app.mode {
                s
            } else {
                return;
            };
            state.prev_field();
            return;
        }
        KeyCode::Enter => {
            submit_form(app);
            return;
        }
        KeyCode::Up => {
            if let Mode::PopupForm(state) = &mut app.mode
                && state.in_checklist
            {
                if let Some(cl) = &mut state.checklist {
                    cl.up();
                }
                return;
            }
        }
        KeyCode::Down => {
            if let Mode::PopupForm(state) = &mut app.mode
                && state.in_checklist
            {
                if let Some(cl) = &mut state.checklist {
                    cl.down();
                }
                return;
            }
        }
        KeyCode::Char(' ') => {
            if let Mode::PopupForm(state) = &mut app.mode
                && state.in_checklist
            {
                if let Some(cl) = &mut state.checklist {
                    cl.toggle_current();
                }
                return;
            }
        }
        _ => {}
    }

    // Route key to focused text field
    if let Mode::PopupForm(state) = &mut app.mode
        && !state.in_checklist
    {
        let focused = state.focused;
        if let Some(field) = state.fields.get_mut(focused) {
            field.input.handle_event(&crossterm::event::Event::Key(key));
        }
    }
}

fn submit_form(app: &mut App) {
    // Clone state to avoid borrow issues
    let state = match &app.mode {
        Mode::PopupForm(s) => s.clone(),
        _ => return,
    };

    // Validate required fields
    for (i, field) in state.fields.iter().enumerate() {
        if field.required && field.value().trim().is_empty() {
            if let Mode::PopupForm(s) = &mut app.mode {
                s.fields[i].error = Some(format!("{} is required", field.label));
            }
            return;
        }
    }

    let f = |idx: usize| state.fields[idx].value().trim().to_string();

    // Apply the form's mutation to a candidate clone so we can validate the
    // result via the store's validate APIs before committing it.
    let mut candidate = app.config.clone();
    match state.kind {
        FormKind::NewTask => {
            let checker = parse_checker(&f(3));
            candidate.tasks.push(TaskConfig {
                id: f(0),
                title: f(1),
                workspace_root: f(2),
                checker_config: checker,
                submission_config: HashMap::new(),
                library_config: HashMap::new(),
                run_config: HashMap::new(),
                test_cases: vec![],
                test_sets: vec![],
                groups: vec![],
                file_io: None,
            });
        }
        FormKind::EditTask(tidx) => {
            if let Some(task) = candidate.tasks.get_mut(tidx) {
                task.id = f(0);
                task.title = f(1);
                task.workspace_root = f(2);
                task.checker_config = parse_checker(&f(3));
            }
        }
        FormKind::NewTestCase { task_idx } => {
            let tc = TestCaseConfig {
                id: f(0),
                input_file: f(1),
                answer_file: f(2),
                time_limit_ms: f(3).parse().unwrap_or(1000),
                memory_limit_kb: f(4).parse().unwrap_or(1048576),
            };
            if let Some(task) = candidate.tasks.get_mut(task_idx) {
                task.test_cases.push(tc);
            }
        }
        FormKind::EditTestCase { task_idx, tc_idx } => {
            if let Some(task) = candidate.tasks.get_mut(task_idx)
                && let Some(tc) = task.test_cases.get_mut(tc_idx)
            {
                tc.id = f(0);
                tc.input_file = f(1);
                tc.answer_file = f(2);
                tc.time_limit_ms = f(3).parse().unwrap_or(tc.time_limit_ms);
                tc.memory_limit_kb = f(4).parse().unwrap_or(tc.memory_limit_kb);
            }
        }
        FormKind::NewTestSet { task_idx } => {
            let members = state
                .checklist
                .as_ref()
                .map(|cl| cl.selected_ids())
                .unwrap_or_default();
            let ts = TestSetConfig {
                id: f(0),
                test_case_ids: members,
            };
            if let Some(task) = candidate.tasks.get_mut(task_idx) {
                task.test_sets.push(ts);
            }
        }
        FormKind::EditTestSet { task_idx, ts_idx } => {
            let members = state
                .checklist
                .as_ref()
                .map(|cl| cl.selected_ids())
                .unwrap_or_default();
            if let Some(task) = candidate.tasks.get_mut(task_idx)
                && let Some(ts) = task.test_sets.get_mut(ts_idx)
            {
                ts.id = f(0);
                ts.test_case_ids = members;
            }
        }
        FormKind::NewGroup { task_idx } => {
            let members = state
                .checklist
                .as_ref()
                .map(|cl| cl.selected_ids())
                .unwrap_or_default();
            let g = GroupConfig {
                id: f(0),
                max_score: f(1).parse().unwrap_or(100.0),
                test_set_ids: members,
            };
            if let Some(task) = candidate.tasks.get_mut(task_idx) {
                task.groups.push(g);
            }
        }
        FormKind::EditGroup {
            task_idx,
            group_idx,
        } => {
            let members = state
                .checklist
                .as_ref()
                .map(|cl| cl.selected_ids())
                .unwrap_or_default();
            if let Some(task) = candidate.tasks.get_mut(task_idx)
                && let Some(g) = task.groups.get_mut(group_idx)
            {
                g.id = f(0);
                g.max_score = f(1).parse().unwrap_or(g.max_score);
                g.test_set_ids = members;
            }
        }
        FormKind::NewRunConfig { task_idx } => {
            let lang = f(0);
            let rc = build_run_config(&state);
            if let Some(task) = candidate.tasks.get_mut(task_idx) {
                task.run_config.insert(lang, rc);
            }
        }
        FormKind::EditRunConfig { task_idx, ref lang } => {
            let old_lang = lang.clone();
            let new_lang = f(0);
            let rc = build_run_config(&state);
            if let Some(task) = candidate.tasks.get_mut(task_idx) {
                if old_lang != new_lang {
                    task.run_config.remove(&old_lang);
                }
                task.run_config.insert(new_lang, rc);
            }
        }
        FormKind::NewContestant => {
            candidate.contestants.push(ContestantConfig {
                id: f(0),
                name: f(1),
                workspace_root: f(2),
                language: HashMap::new(),
            });
        }
        FormKind::EditContestant(cidx) => {
            if let Some(c) = candidate.contestants.get_mut(cidx) {
                c.id = f(0);
                c.name = f(1);
                c.workspace_root = f(2);
            }
        }
        FormKind::EditContestantLanguage {
            contestant_idx,
            ref task_id,
        } => {
            let task_id = task_id.clone();
            let lang = f(0);
            if let Some(c) = candidate.contestants.get_mut(contestant_idx) {
                if lang.is_empty() {
                    c.language.remove(&task_id);
                } else {
                    c.language.insert(task_id, lang);
                }
            }
        }
    }

    // Validate the candidate using the store's validate APIs.
    let errors = validate_candidate(&state.kind, &app.config, &candidate, &app.results);
    if !errors.is_empty() {
        if let Mode::PopupForm(s) = &mut app.mode {
            for field in s.fields.iter_mut() {
                field.error = None;
            }
            // Attach the first duplicate-ID error to the ID field (field 0).
            if let Some(dup) = errors.iter().find_map(|e| match e {
                ValidationError::DuplicateId { .. } => Some(e.to_string()),
                _ => None,
            }) && let Some(id_field) = s.fields.get_mut(0)
            {
                id_field.error = Some(dup);
            }
        }
        let messages: Vec<String> = errors.iter().map(|e| e.to_string()).collect();
        app.error_flash = Some(format!("Invalid: {}", messages.join("; ")));
        return;
    }

    // Commit the validated candidate.
    app.config = candidate;
    app.auto_save_config();
    app.mode = Mode::Normal;
}

/// Validates a candidate config produced by a form submission.
///
/// Config-side checks are scoped to the entity the form edits, using the
/// store's [`validate_task`] for task-scoped forms and [`collect_unique_ids`]
/// for top-level (task / contestant) ID uniqueness.
///
/// Results-side checks use [`validate_results`] over the whole candidate, but
/// only errors *newly introduced* by this edit are surfaced — the baseline
/// (current config) errors are subtracted so pre-existing results issues don't
/// block unrelated edits. This catches e.g. editing a contestant's language to
/// one with no `run_config`, renaming a contestant/task/test-case id that
/// existing results reference, or removing a `run_config` language that judged
/// results depend on.
fn validate_candidate(
    kind: &FormKind,
    baseline: &ContestConfig,
    candidate: &ContestConfig,
    results: &ContestResults,
) -> Vec<ValidationError> {
    let mut errors = Vec::new();

    // Top-level ID uniqueness across the contest.
    match kind {
        FormKind::NewTask | FormKind::EditTask(_) => {
            collect_unique_ids(
                candidate.tasks.iter().map(|t| t.id.as_str()),
                "task",
                &mut errors,
            );
        }
        FormKind::NewContestant | FormKind::EditContestant(_) => {
            collect_unique_ids(
                candidate.contestants.iter().map(|c| c.id.as_str()),
                "contestant",
                &mut errors,
            );
        }
        _ => {}
    }

    // Task-scoped validation: sub-entity ID uniqueness, cross-references,
    // run-config empties, and negative max_score.
    let task_idx: Option<usize> = match kind {
        FormKind::NewTask => candidate.tasks.len().checked_sub(1),
        FormKind::EditTask(i)
        | FormKind::NewTestCase { task_idx: i }
        | FormKind::EditTestCase { task_idx: i, .. }
        | FormKind::NewTestSet { task_idx: i }
        | FormKind::EditTestSet { task_idx: i, .. }
        | FormKind::NewGroup { task_idx: i }
        | FormKind::EditGroup { task_idx: i, .. }
        | FormKind::NewRunConfig { task_idx: i }
        | FormKind::EditRunConfig { task_idx: i, .. } => Some(*i),
        _ => None,
    };
    if let Some(i) = task_idx
        && let Some(task) = candidate.tasks.get(i)
    {
        errors.extend(validate_task(task));
    }

    // Results-side validation: only surface errors newly introduced by this edit.
    let baseline_results_errors: HashSet<String> = validate_results(baseline, results)
        .iter()
        .map(|e| e.to_string())
        .collect();
    for e in validate_results(candidate, results) {
        if !baseline_results_errors.contains(&e.to_string()) {
            errors.push(e);
        }
    }

    errors
}

fn build_run_config(state: &FormState) -> RunConfig {
    let run_cmd: Vec<String> = state.fields[1]
        .value()
        .split_whitespace()
        .map(|s| s.to_string())
        .collect();
    let compile_cmd_str = state.fields[2].value().trim();
    let compile_output = state.fields[3].value().trim().to_string();
    let max_proc: u32 = state
        .fields
        .get(4)
        .and_then(|f| f.value().parse().ok())
        .unwrap_or(1);

    let compilation_config = if compile_cmd_str.is_empty() {
        None
    } else {
        Some(CompilationConfig {
            compile_command: compile_cmd_str
                .split_whitespace()
                .map(|s| s.to_string())
                .collect(),
            compilation_timeout_ms: 10000,
            compilation_memory_limit_kb: 1024 * 1024,
            compilation_output_file: compile_output,
        })
    };

    RunConfig {
        compilation_config,
        run_command: run_cmd,
        max_processes: max_proc,
        bind_dirs: vec![],
    }
}

fn parse_checker(s: &str) -> CheckerConfig {
    if s.starts_with("testlib:") {
        CheckerConfig::Testlib {
            path: s.trim_start_matches("testlib:").to_string(),
            timeout_ms: 10000,
            memory_limit_kb: 1024 * 1024,
            bind_dirs: vec![],
        }
    } else {
        CheckerConfig::Token
    }
}

// ---------------------------------------------------------------------------
// Confirm delete
// ---------------------------------------------------------------------------

fn handle_confirm(app: &mut App, key: KeyEvent) {
    let target = match &app.mode {
        Mode::ConfirmDelete { target, .. } => target.clone(),
        _ => return,
    };
    match key.code {
        KeyCode::Char('y') | KeyCode::Char('Y') => {
            execute_delete(app, target);
            app.mode = Mode::Normal;
        }
        KeyCode::Char('n') | KeyCode::Esc => {
            app.mode = Mode::Normal;
        }
        _ => {}
    }
}

fn execute_delete(app: &mut App, target: DeleteTarget) {
    match target {
        DeleteTarget::Task(idx) => {
            if idx < app.config.tasks.len() {
                app.config.tasks.remove(idx);
                app.tasks.task_list.clamp(app.config.tasks.len());
            }
        }
        DeleteTarget::TestCase { task_idx, tc_idx } => {
            if let Some(task) = app.config.tasks.get_mut(task_idx)
                && tc_idx < task.test_cases.len()
            {
                task.test_cases.remove(tc_idx);
                app.tasks.tc_list.clamp(task.test_cases.len());
            }
        }
        DeleteTarget::TestSet { task_idx, ts_idx } => {
            if let Some(task) = app.config.tasks.get_mut(task_idx)
                && ts_idx < task.test_sets.len()
            {
                task.test_sets.remove(ts_idx);
                app.tasks.ts_list.clamp(task.test_sets.len());
            }
        }
        DeleteTarget::Group {
            task_idx,
            group_idx,
        } => {
            if let Some(task) = app.config.tasks.get_mut(task_idx)
                && group_idx < task.groups.len()
            {
                task.groups.remove(group_idx);
                app.tasks.group_list.clamp(task.groups.len());
            }
        }
        DeleteTarget::RunConfig { task_idx, ref lang } => {
            if let Some(task) = app.config.tasks.get_mut(task_idx) {
                task.run_config.remove(lang);
            }
        }
        DeleteTarget::Contestant(idx) => {
            if idx < app.config.contestants.len() {
                app.config.contestants.remove(idx);
                app.contestants.list.clamp(app.config.contestants.len());
            }
        }
    }
    app.auto_save_config();
}
