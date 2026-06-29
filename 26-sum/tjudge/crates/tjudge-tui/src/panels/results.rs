use ratatui::{
    Frame,
    layout::{Alignment, Constraint, Direction, Layout, Rect},
    style::{Color, Modifier, Style},
    text::{Line, Span, Text},
    widgets::{Block, Borders, Cell, Paragraph, Row, Table, TableState},
};

use tjudge_store::{
    compute_contestant_score, group_score,
    schema::{ContestConfig, ContestResults, ContestantTaskResult, TestCaseVerdict},
    test_case_relative_score, test_set_relative_score,
};

use crate::app::{App, ContestantDetailState};

pub fn render(f: &mut Frame, app: &App, area: Rect) {
    render_rank_table(f, app, area);
}

/// Returns contestant indices sorted by score (rank order), ties broken by name.
pub fn rank_sorted(config: &ContestConfig, results: &ContestResults) -> Vec<usize> {
    let mut indices: Vec<usize> = (0..config.contestants.len()).collect();
    let scores: Vec<f64> = config
        .contestants
        .iter()
        .map(|c| compute_contestant_score(config, &c.id, results).total)
        .collect();
    indices.sort_by(|&a, &b| {
        scores[b]
            .partial_cmp(&scores[a])
            .unwrap_or(std::cmp::Ordering::Equal)
            .then_with(|| config.contestants[a].name.cmp(&config.contestants[b].name))
    });
    indices
}

/// Competition ranks for the rank-sorted order: contestants with equal scores
/// share a rank (1, 1, 3, …). `sorted` is descending by score with equal scores
/// adjacent, so a position shares the previous rank iff its score equals the
/// previous position's.
pub fn competition_ranks(sorted: &[usize], scores: &[f64]) -> Vec<usize> {
    let mut out = Vec::with_capacity(sorted.len());
    for (i, &orig_idx) in sorted.iter().enumerate() {
        let s = scores[orig_idx];
        let rank = if i > 0 && scores[sorted[i - 1]] == s {
            *out.last().unwrap()
        } else {
            i + 1
        };
        out.push(rank);
    }
    out
}

/// Look up a contestant's recorded result for a task (if any).
fn task_result<'a>(
    results: &'a ContestResults,
    contestant_id: &str,
    task_id: &str,
) -> Option<&'a ContestantTaskResult> {
    results
        .results
        .iter()
        .find(|r| r.contestant_id == contestant_id && r.task_id == task_id)
}

fn render_rank_table(f: &mut Frame, app: &App, area: Rect) {
    let block = Block::default()
        .title(" Rank Table ")
        .borders(Borders::ALL)
        .border_style(Style::default().fg(Color::Cyan));
    let inner = block.inner(area);
    f.render_widget(block, area);

    let config = &app.config;
    let results = &app.results;

    if config.contestants.is_empty() {
        f.render_widget(Paragraph::new("No contestants"), inner);
        return;
    }

    let sorted = rank_sorted(config, results);
    let scores: Vec<f64> = config
        .contestants
        .iter()
        .map(|c| compute_contestant_score(config, &c.id, results).total)
        .collect();
    // Per-task score, or None when the task has not been judged at all.
    let per_task_scores: Vec<Vec<Option<f64>>> = config
        .contestants
        .iter()
        .map(|c| {
            let b = compute_contestant_score(config, &c.id, results);
            config
                .tasks
                .iter()
                .map(|t| {
                    let judged = task_result(results, &c.id, &t.id).is_some();
                    if judged {
                        Some(b.by_task.get(&t.id).map(|x| x.total).unwrap_or(0.0))
                    } else {
                        None
                    }
                })
                .collect()
        })
        .collect();

    let cursor_row = app.results_state.cursor_row;
    let cursor_col = app.results_state.cursor_col;

    // Header is rendered as the first navigable row (row 0) so the cursor can
    // land on task-name cells (header + task column → judge that task).
    let header_cells: Vec<Cell> = {
        let mk = |text: &str, col: usize, align: Alignment| {
            let style = if cursor_row == 0 && cursor_col == col {
                Style::default()
                    .fg(Color::Black)
                    .bg(Color::Cyan)
                    .add_modifier(Modifier::BOLD)
            } else {
                Style::default().add_modifier(Modifier::BOLD)
            };
            Cell::from(Text::from(text.to_string()).alignment(align)).style(style)
        };
        let mut cells = vec![
            mk("Rank", 0, Alignment::Left),
            mk("Name", 1, Alignment::Left),
            mk("Total", 2, Alignment::Right),
        ];
        for (i, t) in config.tasks.iter().enumerate() {
            cells.push(mk(&t.id, 3 + i, Alignment::Right));
        }
        cells
    };
    let header = Row::new(header_cells).bottom_margin(1).height(1);

    // Competition ranking: contestants with the same total share a rank
    // (1, 1, 3, …). `sorted` is by score desc, so equal scores are adjacent.
    let comp_ranks = competition_ranks(&sorted, &scores);

    let rows: Vec<Row> = sorted
        .iter()
        .enumerate()
        .map(|(rank, &orig_idx)| {
            let contestant = &config.contestants[orig_idx];
            let row_idx = rank + 1; // +1 because row 0 is the header
            let on_row = cursor_row == row_idx;

            let cell_style = |col: usize, base: Style| {
                if on_row && cursor_col == col {
                    base.add_modifier(Modifier::REVERSED)
                } else if on_row {
                    base.add_modifier(Modifier::BOLD)
                } else {
                    base
                }
            };
            // Right-aligned numeric cell: alignment is set on the Text content
            // rather than faked with format-string padding.
            let right = |text: String, col: usize, base: Style| {
                Cell::from(Text::from(text).alignment(Alignment::Right))
                    .style(cell_style(col, base))
            };

            let mut cells = vec![
                Cell::from(format!("{}", comp_ranks[rank])).style(cell_style(0, Style::default())),
                Cell::from(contestant.name.clone()).style(cell_style(1, Style::default())),
                right(format!("{:.2}", scores[orig_idx]), 2, Style::default()),
            ];
            for (i, score_opt) in per_task_scores[orig_idx].iter().enumerate() {
                let col = 3 + i;
                let (text, base) = match score_opt {
                    Some(s) => (format!("{:.2}", s), Style::default().bg(score_bg_color(*s))),
                    None => ("N/A".to_string(), Style::default().fg(Color::DarkGray)),
                };
                cells.push(right(text, col, base));
            }
            Row::new(cells).height(1)
        })
        .collect();

    let task_count = config.tasks.len();
    let mut widths = vec![
        Constraint::Length(5),
        Constraint::Fill(1),
        Constraint::Length(7),
    ];
    for _ in 0..task_count {
        widths.push(Constraint::Length(10));
    }

    // Use a TableState solely to drive vertical scrolling so the cursor row
    // stays visible; cell highlighting is applied manually above.
    let mut state = TableState::default();
    state.select(Some(cursor_row));
    f.render_stateful_widget(Table::new(rows, widths).header(header), inner, &mut state);

    let hint = " <↑/↓/←/→> move  <j> judge  <J> judge all  <Enter> details  <q> quit ";
    let hint_area = Rect::new(
        area.x + 1,
        area.y + area.height.saturating_sub(1),
        area.width.saturating_sub(2),
        1,
    );
    f.render_widget(
        Paragraph::new(Span::styled(hint, Style::default().fg(Color::DarkGray))),
        hint_area,
    );
}

// ---------------------------------------------------------------------------
// Full-screen contestant details panel
// ---------------------------------------------------------------------------

pub fn render_contestant_detail(
    f: &mut Frame,
    app: &App,
    state: &ContestantDetailState,
    area: Rect,
) {
    let contestant = match app.config.contestants.get(state.contestant_idx) {
        Some(c) => c,
        None => return,
    };

    let title = match state.task_filter.and_then(|i| app.config.tasks.get(i)) {
        Some(t) => format!(
            " Contestant: {} ({}) — task {} ",
            contestant.name, contestant.id, t.id
        ),
        None => format!(" Contestant: {} ({}) ", contestant.name, contestant.id),
    };
    let block = Block::default()
        .title(title)
        .borders(Borders::ALL)
        .border_style(Style::default().fg(Color::Cyan));
    let inner = block.inner(area);
    f.render_widget(block, area);

    let lines = detail_lines(
        &app.config,
        &app.results,
        state.contestant_idx,
        state.task_filter,
    );
    let selected = lines.len().saturating_sub(1).min(state.selected);

    // Layout: a compact summary line, the details table, then the message pane
    // (which occupies 20% of the available height).
    let chunks = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(1),
            Constraint::Min(0),
            Constraint::Percentage(20),
        ])
        .split(inner);

    // Summary line: per-task scores.
    let breakdown = compute_contestant_score(&app.config, &contestant.id, &app.results);
    let mut summary_spans = vec![Span::styled(
        format!("Total: {:.2}   ", breakdown.total),
        Style::default().add_modifier(Modifier::BOLD),
    )];
    for (i, t) in app.config.tasks.iter().enumerate() {
        if i > 0 {
            summary_spans.push(Span::raw("  "));
        }
        let judged = task_result(&app.results, &contestant.id, &t.id).is_some();
        summary_spans.push(Span::raw(format!("{}: ", t.id)));
        let val = match breakdown.by_task.get(&t.id) {
            Some(b) if judged => {
                Span::styled(format!("{:.2}", b.total), Style::default().fg(Color::Green))
            }
            _ => Span::styled("N/A".to_string(), Style::default().fg(Color::DarkGray)),
        };
        summary_spans.push(val);
    }
    f.render_widget(Paragraph::new(Line::from(summary_spans)), chunks[0]);

    // Details table. Rows are indented to convey the hierarchy:
    //   task > group > test set > test case. The Score column shows the score
    //   relevant to each level (task total, group points, set/test-case
    //   relative score in [0,1]).
    let header = Row::new(vec![
        Cell::from("Item").style(Style::default().add_modifier(Modifier::BOLD)),
        Cell::from(Text::from("Status").alignment(Alignment::Right))
            .style(Style::default().add_modifier(Modifier::BOLD)),
        Cell::from(Text::from("Score").alignment(Alignment::Right))
            .style(Style::default().add_modifier(Modifier::BOLD)),
        Cell::from(Text::from("Time").alignment(Alignment::Right))
            .style(Style::default().add_modifier(Modifier::BOLD)),
        Cell::from(Text::from("Memory").alignment(Alignment::Right))
            .style(Style::default().add_modifier(Modifier::BOLD)),
    ])
    .bottom_margin(1);

    let table_rows: Vec<Row> = lines
        .iter()
        .enumerate()
        .map(|(i, line)| {
            let sel_style = if i == selected {
                Style::default().add_modifier(Modifier::REVERSED)
            } else {
                Style::default()
            };
            // Right-align the numeric/status columns (Status, Score, Time, Memory).
            let right = |text: String, style: Style| {
                Cell::from(Text::from(text).alignment(Alignment::Right)).style(style)
            };
            match line {
                DetailLine::TaskHeader {
                    id,
                    title,
                    status,
                    score,
                } => Row::new(vec![
                    Cell::from(format!("{id} — {title}")).style(
                        Style::default()
                            .fg(Color::Yellow)
                            .add_modifier(Modifier::BOLD),
                    ),
                    right(status.clone(), Style::default().fg(Color::DarkGray)),
                    right(score.clone(), Style::default()),
                    right(String::new(), Style::default()),
                    right(String::new(), Style::default()),
                ])
                .style(sel_style),
                DetailLine::Group { id, score, max } => Row::new(vec![
                    Cell::from(format!("  group {id}")).style(Style::default().fg(Color::Cyan)),
                    right(String::new(), Style::default()),
                    right(
                        format!("{:.2}/{:.0}", score, max),
                        Style::default().fg(group_color(*score, *max)),
                    ),
                    right(String::new(), Style::default()),
                    right(String::new(), Style::default()),
                ])
                .style(sel_style),
                DetailLine::TestSet {
                    id,
                    rel,
                    passed,
                    total,
                } => Row::new(vec![
                    Cell::from(format!("    set {id}")).style(Style::default().fg(Color::Blue)),
                    right(format!("{passed}/{total}"), Style::default()),
                    right(format!("{:.2}", rel), Style::default()),
                    right(String::new(), Style::default()),
                    right(String::new(), Style::default()),
                ])
                .style(sel_style),
                DetailLine::TestCase(r) => Row::new(vec![
                    Cell::from(format!("        {}", r.tc_id)),
                    right(r.verdict.clone(), Style::default().fg(r.verdict_color)),
                    right(format!("{:.2}", r.score), Style::default()),
                    right(r.time.clone(), Style::default()),
                    right(r.mem.clone(), Style::default()),
                ])
                .style(sel_style),
            }
        })
        .collect();

    let widths = [
        Constraint::Fill(1),
        Constraint::Length(12),
        Constraint::Length(10),
        Constraint::Length(9),
        Constraint::Length(10),
    ];

    let mut ts = TableState::default();
    ts.select(Some(selected));
    f.render_stateful_widget(
        Table::new(table_rows, widths).header(header),
        chunks[1],
        &mut ts,
    );

    // Message pane for the selected line.
    let msg_block = Block::default()
        .title(" Message ")
        .borders(Borders::ALL)
        .border_style(Style::default().fg(Color::Gray));
    let msg_inner = msg_block.inner(chunks[2]);
    f.render_widget(msg_block, chunks[2]);

    let msg_lines: Vec<Line> = message_for(lines.get(selected));
    let scroll = state.msg_scroll.min(msg_lines.len().saturating_sub(1)) as u16;
    f.render_widget(Paragraph::new(msg_lines).scroll((scroll, 0)), msg_inner);
}

/// Build the message-pane contents for a selected detail line.
fn message_for(line: Option<&DetailLine>) -> Vec<Line<'_>> {
    let dim = |s: String| Line::from(Span::styled(s, Style::default().fg(Color::DarkGray)));
    match line {
        Some(DetailLine::TaskHeader {
            id,
            title,
            status,
            score,
        }) => vec![
            Line::from(Span::styled(
                format!("Task {id}: {title}"),
                Style::default().add_modifier(Modifier::BOLD),
            )),
            dim(format!("Score: {score}")),
            dim(format!("Status: {status}")),
        ],
        Some(DetailLine::Group { id, score, max }) => vec![
            Line::from(Span::styled(
                format!("Group {id}"),
                Style::default().add_modifier(Modifier::BOLD),
            )),
            dim(format!("Score: {:.1} / {:.0}", score, max)),
            dim(format!(
                "Relative: {:.0}%",
                if *max > 0.0 { score / max * 100.0 } else { 0.0 }
            )),
        ],
        Some(DetailLine::TestSet {
            id,
            rel,
            passed,
            total,
        }) => vec![
            Line::from(Span::styled(
                format!("Test set {id}"),
                Style::default().add_modifier(Modifier::BOLD),
            )),
            dim(format!("Relative score: {:.2}", rel)),
            dim(format!("Test cases passed: {passed}/{total}")),
        ],
        Some(DetailLine::TestCase(r)) => {
            let mut out = vec![
                Line::from(Span::styled(
                    format!("Test case {}", r.tc_id),
                    Style::default().add_modifier(Modifier::BOLD),
                )),
                dim(format!("Verdict: {}   Relative: {:.2}", r.verdict, r.score)),
            ];
            match &r.message {
                Some(m) if !m.trim().is_empty() => {
                    out.push(Line::from(""));
                    out.extend(m.lines().map(Line::from));
                }
                _ => out.push(dim("(no message)".to_string())),
            }
            out
        }
        None => vec![Line::from("(no test cases)")],
    }
}

/// One line in the contestant details list. The hierarchy is task > group >
/// test set > test case; each carries the score info for its level.
pub enum DetailLine {
    /// A task section header: id, title, status summary, and total score.
    TaskHeader {
        id: String,
        title: String,
        status: String,
        score: String,
    },
    /// A group row: points earned out of the group's max.
    Group { id: String, score: f64, max: f64 },
    /// A test-set row: relative score in [0,1] and passed/total test cases.
    TestSet {
        id: String,
        rel: f64,
        passed: usize,
        total: usize,
    },
    /// A single test-case result row.
    TestCase(DetailRow),
}

/// A single rendered test-case row for the detail views.
pub struct DetailRow {
    pub tc_id: String,
    pub verdict: String,
    pub verdict_color: Color,
    /// Relative score in [0,1].
    pub score: f64,
    pub time: String,
    pub mem: String,
    pub message: Option<String>,
}

/// An empty `ContestantTaskResult` used when a task has not been judged, so the
/// scoring helpers (which expect a result) treat every test case as missing.
fn empty_result(contestant_id: &str, task_id: &str) -> ContestantTaskResult {
    ContestantTaskResult {
        contestant_id: contestant_id.to_string(),
        task_id: task_id.to_string(),
        compilation_error: None,
        test_case_results: std::collections::HashMap::new(),
    }
}

/// Builds the ordered list of detail lines for a contestant. For each task:
/// a task header, then its groups (with points), its test sets (with relative
/// scores), then its test cases (with verdicts + relative scores). Tasks with
/// no recorded result produce "waiting" rows and zero scores.
pub fn detail_lines(
    config: &ContestConfig,
    results: &ContestResults,
    contestant_idx: usize,
    task_filter: Option<usize>,
) -> Vec<DetailLine> {
    let contestant = match config.contestants.get(contestant_idx) {
        Some(c) => c,
        None => return Vec::new(),
    };
    let breakdown = compute_contestant_score(config, &contestant.id, results);
    let mut lines = Vec::new();
    for (tidx, task) in config.tasks.iter().enumerate() {
        // When filtered to a single task, skip the others.
        if task_filter.is_some_and(|f| f != tidx) {
            continue;
        }
        let result_opt = task_result(results, &contestant.id, &task.id);
        // The scoring helpers need a result reference; use an empty one when
        // the task hasn't been judged so they report zeros consistently.
        let empty = empty_result(&contestant.id, &task.id);
        let result: &ContestantTaskResult = result_opt.unwrap_or(&empty);

        let total = breakdown
            .by_task
            .get(&task.id)
            .map(|b| b.total)
            .unwrap_or(0.0);
        let judged = result_opt.is_some();
        let status = if !judged {
            "waiting".to_string()
        } else if result.compilation_error.is_some() {
            "compile error".to_string()
        } else {
            let n_tc = task.test_cases.len();
            let passed = result
                .test_case_results
                .values()
                .filter(|tc| matches!(tc.verdict, TestCaseVerdict::Accepted))
                .count();
            format!("{}/{} AC", passed, n_tc)
        };
        lines.push(DetailLine::TaskHeader {
            id: task.id.clone(),
            title: task.title.clone(),
            status,
            score: if judged {
                format!("{:.2}", total)
            } else {
                "N/A".to_string()
            },
        });

        // Groups → points earned / max.
        for g in &task.groups {
            let score = if judged && result.compilation_error.is_none() {
                group_score(task, g, result)
            } else {
                0.0
            };
            lines.push(DetailLine::Group {
                id: g.id.clone(),
                score,
                max: g.max_score,
            });
        }

        // Test sets → relative score (min over test cases) + passed/total.
        for ts in &task.test_sets {
            let rel = if judged && result.compilation_error.is_none() {
                test_set_relative_score(ts, result)
            } else {
                0.0
            };
            let total_tc = ts.test_case_ids.len();
            let passed = if judged && result.compilation_error.is_none() {
                ts.test_case_ids
                    .iter()
                    .filter(|tc_id| {
                        result
                            .test_case_results
                            .get(*tc_id)
                            .map(|r| test_case_relative_score(&r.verdict) >= 1.0)
                            .unwrap_or(false)
                    })
                    .count()
            } else {
                0
            };
            lines.push(DetailLine::TestSet {
                id: ts.id.clone(),
                rel,
                passed,
                total: total_tc,
            });
        }

        // Test cases → verdict + relative score.
        for tc in &task.test_cases {
            let row = if !judged {
                DetailRow {
                    tc_id: tc.id.clone(),
                    verdict: "N/A".to_string(),
                    verdict_color: Color::DarkGray,
                    score: 0.0,
                    time: String::new(),
                    mem: String::new(),
                    message: None,
                }
            } else if let Some(ce) = &result.compilation_error {
                DetailRow {
                    tc_id: tc.id.clone(),
                    verdict: "CE".to_string(),
                    verdict_color: Color::Red,
                    score: 0.0,
                    time: String::new(),
                    mem: String::new(),
                    message: Some(ce.clone()),
                }
            } else if let Some(tcr) = result.test_case_results.get(&tc.id) {
                DetailRow {
                    tc_id: tc.id.clone(),
                    verdict: verdict_short(&tcr.verdict),
                    verdict_color: verdict_color(&tcr.verdict),
                    score: test_case_relative_score(&tcr.verdict),
                    time: tcr
                        .time_used_ms
                        .map(|t| format!("{t}ms"))
                        .unwrap_or_default(),
                    mem: tcr
                        .memory_used_kb
                        .map(|m| format!("{:.1}MiB", m as f64 / 1024.0))
                        .unwrap_or_default(),
                    message: tcr.message.clone(),
                }
            } else {
                DetailRow {
                    tc_id: tc.id.clone(),
                    verdict: "N/A".to_string(),
                    verdict_color: Color::DarkGray,
                    score: 0.0,
                    time: String::new(),
                    mem: String::new(),
                    message: None,
                }
            };
            lines.push(DetailLine::TestCase(row));
        }
    }
    lines
}

/// Color for a group's earned/max ratio: red → yellow → green.
fn group_color(score: f64, max: f64) -> Color {
    if max <= 0.0 {
        return Color::DarkGray;
    }
    let ratio = (score / max).clamp(0.0, 1.0);
    match ratio {
        r if r < 0.2 => Color::Red,
        r if r < 0.4 => Color::LightRed,
        r if r < 0.6 => Color::Yellow,
        r if r < 0.8 => Color::LightGreen,
        _ => Color::Green,
    }
}

/// Maps a 0–100 score to a background color along a Red→Yellow→Green gradient using
/// terminal palette colors: `0` = Red, `50` = Yellow, `100` = Green. Values outside
/// `[0, 100]` are clamped, and intermediate scores fall into the nearest band.
fn score_bg_color(score: f64) -> Color {
    match score.clamp(0.0, 100.0) {
        s if s < 20.0 => Color::Red,
        s if s < 40.0 => Color::LightRed,
        s if s < 60.0 => Color::Yellow,
        s if s < 80.0 => Color::LightGreen,
        _ => Color::Green,
    }
}

fn verdict_short(v: &TestCaseVerdict) -> String {
    match v {
        TestCaseVerdict::Accepted => "AC".to_string(),
        TestCaseVerdict::WrongAnswer => "WA".to_string(),
        TestCaseVerdict::PartiallyCorrect(s) => format!("PC({:.0}%)", s * 100.0),
        TestCaseVerdict::TimeLimitExceeded => "TLE".to_string(),
        TestCaseVerdict::MemoryLimitExceeded => "MLE".to_string(),
        TestCaseVerdict::RuntimeError => "RE".to_string(),
        TestCaseVerdict::InternalError => "IE".to_string(),
        TestCaseVerdict::FileError => "FE".to_string(),
        TestCaseVerdict::Skipped => "SKP".to_string(),
    }
}

fn verdict_color(v: &TestCaseVerdict) -> Color {
    match v {
        TestCaseVerdict::Accepted => Color::Green,
        TestCaseVerdict::WrongAnswer => Color::Red,
        TestCaseVerdict::PartiallyCorrect(_) => Color::Cyan,
        TestCaseVerdict::TimeLimitExceeded => Color::Magenta,
        TestCaseVerdict::MemoryLimitExceeded => Color::Magenta,
        TestCaseVerdict::RuntimeError => Color::Yellow,
        TestCaseVerdict::InternalError => Color::DarkGray,
        TestCaseVerdict::FileError => Color::LightRed,
        TestCaseVerdict::Skipped => Color::DarkGray,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::app::{App, ContestantDetailState, Mode, Panel, ResultsPanelState};
    use ratatui::{Terminal, backend::TestBackend};
    use std::collections::HashMap;
    use tjudge_store::schema::{
        ContestMeta, ContestantConfig, GroupConfig, TaskConfig, TestCaseConfig, TestCaseResult,
        TestSetConfig,
    };

    fn mk_app() -> App {
        let mut app = App::new_startup();
        app.mode = Mode::Normal;
        app.active_panel = Panel::Results;
        app.config = ContestConfig {
            meta: ContestMeta {
                id: "c".into(),
                title: "T".into(),
                schema_version: "1.0".into(),
            },
            tasks: vec![TaskConfig {
                id: "add".into(),
                title: "Addition".into(),
                workspace_root: "tasks/add".into(),
                submission_config: HashMap::new(),
                library_config: HashMap::new(),
                run_config: HashMap::new(),
                checker_config: tjudge_store::schema::CheckerConfig::Token,
                test_cases: vec![
                    TestCaseConfig {
                        id: "1".into(),
                        input_file: "1.in".into(),
                        answer_file: "1.ans".into(),
                        time_limit_ms: 1000,
                        memory_limit_kb: 262144,
                    },
                    TestCaseConfig {
                        id: "2".into(),
                        input_file: "2.in".into(),
                        answer_file: "2.ans".into(),
                        time_limit_ms: 1000,
                        memory_limit_kb: 262144,
                    },
                ],
                test_sets: vec![TestSetConfig {
                    id: "ts".into(),
                    test_case_ids: vec!["1".into(), "2".into()],
                }],
                groups: vec![GroupConfig {
                    id: "g".into(),
                    test_set_ids: vec!["ts".into()],
                    max_score: 100.0,
                }],
                file_io: None,
            }],
            contestants: vec![
                ContestantConfig {
                    id: "alice".into(),
                    name: "Alice".into(),
                    workspace_root: "c/alice".into(),
                    language: HashMap::new(),
                },
                ContestantConfig {
                    id: "bob".into(),
                    name: "Bob".into(),
                    workspace_root: "c/bob".into(),
                    language: HashMap::new(),
                },
            ],
        };
        // Bob has judged results; Alice has not (should show "N/A").
        app.results = ContestResults {
            results: vec![ContestantTaskResult {
                contestant_id: "bob".into(),
                task_id: "add".into(),
                compilation_error: None,
                test_case_results: HashMap::from([
                    (
                        "1".into(),
                        TestCaseResult {
                            verdict: TestCaseVerdict::Accepted,
                            time_used_ms: Some(5),
                            memory_used_kb: Some(1024),
                            message: Some("ok".into()),
                        },
                    ),
                    (
                        "2".into(),
                        TestCaseResult {
                            verdict: TestCaseVerdict::WrongAnswer,
                            time_used_ms: Some(7),
                            memory_used_kb: Some(2048),
                            message: Some("bad output".into()),
                        },
                    ),
                ]),
            }],
        };
        app
    }

    #[test]
    fn render_results_panel_smoke() {
        let mut app = mk_app();
        // Cursor on header, then move down to a contestant, then to a task cell.
        for (row, col) in [(0, 0), (1, 1), (1, 3), (2, 3)] {
            app.results_state = ResultsPanelState {
                cursor_row: row,
                cursor_col: col,
                detail_scroll: 0,
            };
            let backend = TestBackend::new(120, 30);
            let mut term = Terminal::new(backend).unwrap();
            term.draw(|f| render(f, &app, f.area())).unwrap();
        }
    }

    #[test]
    fn render_contestant_detail_smoke() {
        let app = mk_app();
        let lines = detail_lines(&app.config, &app.results, 1, None); // bob
        assert!(lines.len() >= 3, "header + 2 test cases");
        // Select each line and render.
        for sel in 0..lines.len() {
            let mut app = mk_app();
            app.mode = Mode::ContestantDetail(ContestantDetailState {
                contestant_idx: 1,
                task_filter: None,
                selected: sel,
                msg_scroll: 0,
            });
            let backend = TestBackend::new(100, 40);
            let mut term = Terminal::new(backend).unwrap();
            let state = match &app.mode {
                Mode::ContestantDetail(s) => s.clone(),
                _ => unreachable!(),
            };
            term.draw(|f| render_contestant_detail(f, &app, &state, f.area()))
                .unwrap();
        }
    }

    #[test]
    fn render_contestant_detail_filtered_smoke() {
        // Filtered to a single task: render with a 20%-height message pane.
        let mut app = mk_app();
        app.mode = Mode::ContestantDetail(ContestantDetailState {
            contestant_idx: 1,
            task_filter: Some(0),
            selected: 0,
            msg_scroll: 0,
        });
        let backend = TestBackend::new(100, 40);
        let mut term = Terminal::new(backend).unwrap();
        let state = match &app.mode {
            Mode::ContestantDetail(s) => s.clone(),
            _ => unreachable!(),
        };
        term.draw(|f| render_contestant_detail(f, &app, &state, f.area()))
            .unwrap();
    }

    #[test]
    fn not_judged_shows_waiting_row() {
        let app = mk_app();
        // alice (idx 0) has no result → test cases are "N/A".
        let lines = detail_lines(&app.config, &app.results, 0, None);
        let tcs: Vec<&DetailRow> = lines
            .iter()
            .filter_map(|l| match l {
                DetailLine::TestCase(r) => Some(r),
                _ => None,
            })
            .collect();
        assert_eq!(tcs.len(), 2);
        for r in tcs {
            assert_eq!(r.verdict, "N/A");
        }
    }

    #[test]
    fn detail_lines_carry_group_set_case_scores() {
        let app = mk_app();
        // bob (idx 1): tc1 AC (rel 1.0), tc2 WA (rel 0.0), both in set ts, group g max 100.
        let lines = detail_lines(&app.config, &app.results, 1, None);

        // Group: min(1.0, 0.0) = 0 → 0/100.
        let group = lines
            .iter()
            .find(|l| matches!(l, DetailLine::Group { id, .. } if id == "g"))
            .expect("group line present");
        match group {
            DetailLine::Group { score, max, .. } => {
                assert!((*score - 0.0).abs() < 1e-9);
                assert!((*max - 100.0).abs() < 1e-9);
            }
            _ => unreachable!(),
        }

        // Test set: min(1.0, 0.0) = 0.0, 1/2 passed.
        let set = lines
            .iter()
            .find(|l| matches!(l, DetailLine::TestSet { id, .. } if id == "ts"))
            .expect("test set line present");
        match set {
            DetailLine::TestSet {
                rel, passed, total, ..
            } => {
                assert!((*rel - 0.0).abs() < 1e-9);
                assert_eq!(*passed, 1);
                assert_eq!(*total, 2);
            }
            _ => unreachable!(),
        }

        // Test cases: tc1 rel 1.0, tc2 rel 0.0.
        let rel = |id: &str| {
            lines
                .iter()
                .find_map(|l| match l {
                    DetailLine::TestCase(r) if r.tc_id == id => Some(r.score),
                    _ => None,
                })
                .unwrap()
        };
        assert!((rel("1") - 1.0).abs() < 1e-9);
        assert!((rel("2") - 0.0).abs() < 1e-9);
    }

    #[test]
    fn competition_ranks_tie_share_rank() {
        // scores indexed by original idx: idx0=100, idx1=100, idx2=90, idx3=80
        let scores = vec![100.0, 100.0, 90.0, 80.0];
        // sorted desc by score → [0, 1, 2, 3]
        let sorted = vec![0, 1, 2, 3];
        assert_eq!(competition_ranks(&sorted, &scores), vec![1, 1, 3, 4]);

        // A three-way tie at the top: 1, 1, 1, 4.
        let scores = vec![50.0, 50.0, 50.0, 0.0];
        let sorted = vec![0, 1, 2, 3];
        assert_eq!(competition_ranks(&sorted, &scores), vec![1, 1, 1, 4]);

        // No ties: 1, 2, 3.
        let scores = vec![30.0, 20.0, 10.0];
        let sorted = vec![0, 1, 2];
        assert_eq!(competition_ranks(&sorted, &scores), vec![1, 2, 3]);
    }

    #[test]
    fn detail_lines_task_filter_shows_one_task() {
        let app = mk_app();
        // bob (idx 1), filtered to task 0 → exactly one TaskHeader.
        let lines = detail_lines(&app.config, &app.results, 1, Some(0));
        let headers = lines
            .iter()
            .filter(|l| matches!(l, DetailLine::TaskHeader { .. }))
            .count();
        assert_eq!(headers, 1);
    }
}
