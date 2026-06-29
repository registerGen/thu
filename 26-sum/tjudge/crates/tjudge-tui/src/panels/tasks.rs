use ratatui::{
    Frame,
    layout::{Constraint, Direction, Layout, Rect},
    style::{Color, Modifier, Style},
    text::{Line, Span},
    widgets::{Block, Borders, List, ListItem, Paragraph, Row, Table},
};

use tjudge_store::schema::{RunConfig, TaskConfig};

use crate::app::{App, TaskSubTab};

pub fn render(f: &mut Frame, app: &App, area: Rect) {
    let chunks = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Percentage(30), Constraint::Percentage(70)])
        .split(area);

    render_task_list(f, app, chunks[0]);
    render_task_detail(f, app, chunks[1]);
}

fn render_task_list(f: &mut Frame, app: &App, area: Rect) {
    let focused = !app.tasks.focus_right;
    let border_style = if focused {
        Style::default().fg(Color::Cyan)
    } else {
        Style::default().fg(Color::Gray)
    };
    let block = Block::default()
        .title(" Tasks ")
        .borders(Borders::ALL)
        .border_style(border_style);
    let inner = block.inner(area);
    f.render_widget(block, area);

    let items: Vec<ListItem> = app
        .config
        .tasks
        .iter()
        .enumerate()
        .map(|(i, t)| {
            let style = if i == app.tasks.task_list.selected && focused {
                Style::default().add_modifier(Modifier::REVERSED)
            } else if i == app.tasks.task_list.selected {
                Style::default().add_modifier(Modifier::BOLD)
            } else {
                Style::default()
            };
            ListItem::new(format!("{} — {}", t.id, t.title)).style(style)
        })
        .collect();

    f.render_widget(List::new(items), inner);

    let hint = if focused {
        " <n> new  <e> edit  <d> del  <→> detail "
    } else {
        " <←> tasks  <Tab> sub-tab "
    };
    render_hint(f, hint, area);
}

fn render_task_detail(f: &mut Frame, app: &App, area: Rect) {
    let task = match app.selected_task() {
        Some(t) => t,
        None => {
            f.render_widget(
                Paragraph::new("No task selected").block(Block::default().borders(Borders::ALL)),
                area,
            );
            return;
        }
    };

    // Sub-tab header
    let header_chunks = Layout::default()
        .direction(Direction::Vertical)
        .constraints([Constraint::Length(3), Constraint::Min(0)])
        .split(area);

    render_sub_tab_header(f, app, header_chunks[0]);
    render_sub_tab_content(f, app, task, header_chunks[1]);
}

fn render_sub_tab_header(f: &mut Frame, app: &App, area: Rect) {
    let tabs = TaskSubTab::all();
    let spans: Vec<Span> = tabs
        .iter()
        .flat_map(|tab| {
            let active = *tab == app.tasks.sub_tab;
            let s = if active {
                Span::styled(
                    format!(" {} ", tab.label()),
                    Style::default()
                        .fg(Color::Black)
                        .bg(Color::Cyan)
                        .add_modifier(Modifier::BOLD),
                )
            } else {
                Span::styled(
                    format!(" {} ", tab.label()),
                    Style::default().fg(Color::Gray),
                )
            };
            vec![s, Span::raw(" ")]
        })
        .collect();
    let line = Line::from(spans);
    let block = Block::default()
        .borders(Borders::ALL)
        .border_style(Style::default().fg(Color::Gray));
    let inner = block.inner(area);
    f.render_widget(block, area);
    f.render_widget(Paragraph::new(line), inner);
}

fn render_sub_tab_content(f: &mut Frame, app: &App, task: &TaskConfig, area: Rect) {
    let focused = app.tasks.focus_right;
    let border_style = if focused {
        Style::default().fg(Color::Cyan)
    } else {
        Style::default().fg(Color::Gray)
    };

    match app.tasks.sub_tab {
        TaskSubTab::TestCases => {
            let block = Block::default()
                .title(format!(" Test Cases ({}) ", task.test_cases.len()))
                .borders(Borders::ALL)
                .border_style(border_style);
            let inner = block.inner(area);
            f.render_widget(block, area);

            let header = Row::new(vec!["ID", "Input", "Answer", "Time(ms)", "Mem(KiB)"])
                .style(Style::default().add_modifier(Modifier::BOLD))
                .bottom_margin(1);
            let rows: Vec<Row> = task
                .test_cases
                .iter()
                .enumerate()
                .map(|(i, tc)| {
                    let style = if i == app.tasks.tc_list.selected && focused {
                        Style::default().add_modifier(Modifier::REVERSED)
                    } else {
                        Style::default()
                    };
                    Row::new(vec![
                        tc.id.clone(),
                        tc.input_file.clone(),
                        tc.answer_file.clone(),
                        tc.time_limit_ms.to_string(),
                        tc.memory_limit_kb.to_string(),
                    ])
                    .style(style)
                })
                .collect();
            let widths = [
                Constraint::Percentage(15),
                Constraint::Percentage(30),
                Constraint::Percentage(30),
                Constraint::Percentage(12),
                Constraint::Percentage(13),
            ];
            f.render_widget(Table::new(rows, widths).header(header), inner);
            let hint = " <n> new  <e> edit  <d> del ";
            render_hint(f, hint, area);
        }
        TaskSubTab::TestSets => {
            let block = Block::default()
                .title(format!(" Test Sets ({}) ", task.test_sets.len()))
                .borders(Borders::ALL)
                .border_style(border_style);
            let inner = block.inner(area);
            f.render_widget(block, area);

            let chunks = Layout::default()
                .direction(Direction::Horizontal)
                .constraints([Constraint::Percentage(40), Constraint::Percentage(60)])
                .split(inner);

            let items: Vec<ListItem> = task
                .test_sets
                .iter()
                .enumerate()
                .map(|(i, ts)| {
                    let style = if i == app.tasks.ts_list.selected && focused {
                        Style::default().add_modifier(Modifier::REVERSED)
                    } else {
                        Style::default()
                    };
                    ListItem::new(ts.id.clone()).style(style)
                })
                .collect();
            f.render_widget(List::new(items), chunks[0]);

            // Show members of selected test set
            if let Some(ts) = task.test_sets.get(app.tasks.ts_list.selected) {
                let member_items: Vec<ListItem> = ts
                    .test_case_ids
                    .iter()
                    .map(|id| ListItem::new(id.clone()))
                    .collect();
                let member_block = Block::default().title(" Members ").borders(Borders::LEFT);
                let member_inner = member_block.inner(chunks[1]);
                f.render_widget(member_block, chunks[1]);
                f.render_widget(List::new(member_items), member_inner);
            }
            let hint = " <n> new  <e> edit  <d> del ";
            render_hint(f, hint, area);
        }
        TaskSubTab::Groups => {
            let block = Block::default()
                .title(format!(" Groups ({}) ", task.groups.len()))
                .borders(Borders::ALL)
                .border_style(border_style);
            let inner = block.inner(area);
            f.render_widget(block, area);

            let chunks = Layout::default()
                .direction(Direction::Horizontal)
                .constraints([Constraint::Percentage(40), Constraint::Percentage(60)])
                .split(inner);

            let items: Vec<ListItem> = task
                .groups
                .iter()
                .enumerate()
                .map(|(i, g)| {
                    let style = if i == app.tasks.group_list.selected && focused {
                        Style::default().add_modifier(Modifier::REVERSED)
                    } else {
                        Style::default()
                    };
                    ListItem::new(format!("{} ({} pts)", g.id, g.max_score)).style(style)
                })
                .collect();
            f.render_widget(List::new(items), chunks[0]);

            if let Some(g) = task.groups.get(app.tasks.group_list.selected) {
                let member_items: Vec<ListItem> = g
                    .test_set_ids
                    .iter()
                    .map(|id| ListItem::new(id.clone()))
                    .collect();
                let member_block = Block::default().title(" Test Sets ").borders(Borders::LEFT);
                let member_inner = member_block.inner(chunks[1]);
                f.render_widget(member_block, chunks[1]);
                f.render_widget(List::new(member_items), member_inner);
            }
            let hint = " <n> new  <e> edit  <d> del ";
            render_hint(f, hint, area);
        }
        TaskSubTab::RunConfigs => {
            let block = Block::default()
                .title(format!(" Run Configs ({}) ", task.run_config.len()))
                .borders(Borders::ALL)
                .border_style(border_style);
            let inner = block.inner(area);
            f.render_widget(block, area);

            let header = Row::new(vec![
                "Language",
                "Run Command",
                "Compile Command",
                "Output File",
            ])
            .style(Style::default().add_modifier(Modifier::BOLD))
            .bottom_margin(1);
            let mut entries: Vec<(&String, &RunConfig)> = task.run_config.iter().collect();
            entries.sort_by_key(|(k, _)| k.as_str());
            let rows: Vec<Row> = entries
                .iter()
                .enumerate()
                .map(|(i, (lang, rc))| {
                    let style = if i == app.tasks.rc_list.selected && focused {
                        Style::default().add_modifier(Modifier::REVERSED)
                    } else {
                        Style::default()
                    };
                    let compile = rc
                        .compilation_config
                        .as_ref()
                        .map(|c| c.compile_command.join(" "))
                        .unwrap_or_default();
                    let output = rc
                        .compilation_config
                        .as_ref()
                        .map(|c| c.compilation_output_file.clone())
                        .unwrap_or_default();
                    Row::new(vec![
                        lang.to_string(),
                        rc.run_command.join(" "),
                        compile,
                        output,
                    ])
                    .style(style)
                })
                .collect();
            let widths = [
                Constraint::Percentage(15),
                Constraint::Percentage(30),
                Constraint::Percentage(35),
                Constraint::Percentage(20),
            ];
            f.render_widget(Table::new(rows, widths).header(header), inner);
            let hint = " <n> new  <e> edit  <d> del ";
            render_hint(f, hint, area);
        }
    }
}

fn render_hint(f: &mut Frame, hint: &str, area: Rect) {
    let y = area.y + area.height.saturating_sub(1);
    if y >= area.y + area.height {
        return;
    }
    let hint_area = Rect::new(area.x + 1, y, area.width.saturating_sub(2), 1);
    f.render_widget(
        Paragraph::new(Span::styled(hint, Style::default().fg(Color::DarkGray))),
        hint_area,
    );
}
