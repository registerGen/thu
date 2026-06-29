use ratatui::{
    Frame,
    layout::{Constraint, Direction, Layout, Rect},
    style::{Color, Modifier, Style},
    text::{Line, Span},
    widgets::{Block, Borders, Clear, Paragraph},
};

use crate::{
    app::{App, Mode, Panel},
    panels,
    popup::{render_confirm, render_error_flash, render_popup},
};

pub fn render(f: &mut Frame, app: &App) {
    let size = f.area();

    let chunks = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(1),
            Constraint::Min(0),
            Constraint::Length(1),
        ])
        .split(size);

    render_header(f, app, chunks[0]);

    // The contestant details panel takes over the main area when active: clear
    // the rank table first so it doesn't show through, then draw the panel.
    if let Mode::ContestantDetail(state) = &app.mode {
        f.render_widget(Clear, chunks[1]);
        panels::results::render_contestant_detail(f, app, state, chunks[1]);
    } else {
        render_main(f, app, chunks[1]);
    }
    render_footer(f, app, chunks[2]);

    // Overlays (rendered on top). Judging shows its progress in the footer
    // instead of an overlay, so the panels stay visible while judging.
    match &app.mode {
        Mode::Startup { input, error } => render_startup(f, input, error.as_deref(), size),
        Mode::PopupForm(state) => render_popup(f, state, size),
        Mode::ConfirmDelete { message, .. } => render_confirm(f, message, size),
        _ => {}
    }

    if let Some(err) = &app.error_flash {
        render_error_flash(f, err, size);
    }
}

fn render_header(f: &mut Frame, app: &App, area: Rect) {
    let path = app.config_path.display().to_string();
    let path_str = if path.is_empty() {
        "No file loaded".to_string()
    } else {
        path
    };

    let panels = [Panel::Tasks, Panel::Contestants, Panel::Results];
    let mut spans: Vec<Span> = Vec::new();
    spans.push(Span::styled(
        " tjudge ",
        Style::default()
            .fg(Color::Yellow)
            .add_modifier(Modifier::BOLD),
    ));
    spans.push(Span::raw(" │ "));
    for panel in &panels {
        let label = match panel {
            Panel::Tasks => "Tasks",
            Panel::Contestants => "Contestants",
            Panel::Results => "Results",
        };
        let active = *panel == app.active_panel && !matches!(app.mode, Mode::Startup { .. });
        let s = if active {
            Span::styled(
                format!(" {label} "),
                Style::default()
                    .fg(Color::Black)
                    .bg(Color::Cyan)
                    .add_modifier(Modifier::BOLD),
            )
        } else {
            Span::styled(format!(" {label} "), Style::default().fg(Color::Gray))
        };
        spans.push(s);
        spans.push(Span::raw(" "));
    }
    spans.push(Span::styled(
        format!(" {path_str} "),
        Style::default().fg(Color::DarkGray),
    ));

    f.render_widget(
        Paragraph::new(Line::from(spans)).style(Style::default().bg(Color::DarkGray)),
        area,
    );
}

fn render_footer(f: &mut Frame, app: &App, area: Rect) {
    let hint: String = match &app.mode {
        Mode::Startup { .. } => " Enter path to contest.toml and press <Enter> ".to_string(),
        Mode::Normal => match app.active_panel {
            Panel::Tasks => if app.tasks.focus_right {
                " <Tab>/<S-Tab> sub-tab  <←> back  <↑/↓> navigate  <n> new  <e> edit  <d> del  <q> quit "
            } else {
                " <Tab>/<S-Tab> switch panel  <→> detail  <↑/↓> navigate  <n> new  <e> edit  <d> del  <q> quit "
            }.to_string(),
            Panel::Contestants => " <Tab>/<S-Tab> switch panel  <→/←> focus  <↑/↓> navigate  <n> new  <e> edit  <d> del  <q> quit ".to_string(),
            Panel::Results => " <Tab>/<S-Tab> switch panel  <↑/↓/←/→> move cell  <j> judge  <J> judge all  <Enter> details  <q> quit ".to_string(),
        },
        Mode::PopupForm(_) => " <Tab> next field  <S-Tab> prev  <Enter> submit  <Esc> cancel ".to_string(),
        Mode::ContestantDetail(_) => " <↑/↓> select test case  <PgUp/PgDn> scroll message  <Esc>/<q> back ".to_string(),
        Mode::ConfirmDelete { .. } => " <y> confirm delete  <n>/<Esc> cancel ".to_string(),
        Mode::Judging => format!(
            " Judging… {} test case(s) remaining  <Esc>/<c> cancel ",
            app.judge_pending
        ),
        Mode::Quit => String::new(),
    };
    f.render_widget(
        Paragraph::new(Span::styled(hint, Style::default().fg(Color::White)))
            .style(Style::default().bg(Color::DarkGray)),
        area,
    );
}

fn render_main(f: &mut Frame, app: &App, area: Rect) {
    if matches!(app.mode, Mode::Startup { .. }) {
        // Main area blank while startup prompt overlay is showing
        f.render_widget(
            Block::default().style(Style::default().bg(Color::Reset)),
            area,
        );
        return;
    }
    match app.active_panel {
        Panel::Tasks => panels::tasks::render(f, app, area),
        Panel::Contestants => panels::contestants::render(f, app, area),
        Panel::Results => panels::results::render(f, app, area),
    }
}

fn render_startup(f: &mut Frame, input: &tui_input::Input, error: Option<&str>, area: Rect) {
    use crate::popup::centered_rect;
    use ratatui::widgets::Clear;

    let height = if error.is_some() { 7u16 } else { 5u16 };
    let width = 60u16.min(area.width.saturating_sub(4));
    let popup_area = centered_rect(width, height, area);

    f.render_widget(Clear, popup_area);
    let block = Block::default()
        .title(" Load Contest ")
        .borders(Borders::ALL)
        .border_style(Style::default().fg(Color::Yellow));
    let inner = block.inner(popup_area);
    f.render_widget(block, popup_area);

    let layout = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(1),
            Constraint::Length(1),
            Constraint::Min(0),
        ])
        .split(inner);

    f.render_widget(Paragraph::new("Path to contest.toml:"), layout[0]);

    let val = input.value();
    // `Input::cursor()` is a char index; convert to a byte offset for slicing.
    let cursor = input.cursor();
    let byte = val
        .char_indices()
        .nth(cursor)
        .map(|(b, _)| b)
        .unwrap_or(val.len());
    let before = &val[..byte];
    let cur_char = val
        .chars()
        .nth(cursor)
        .map(|c| c.to_string())
        .unwrap_or_else(|| " ".to_string());
    let after: String = val.chars().skip(cursor + 1).collect();
    let input_line = Line::from(vec![
        Span::raw(before),
        Span::styled(cur_char, Style::default().bg(Color::White).fg(Color::Black)),
        Span::raw(after),
    ]);
    f.render_widget(Paragraph::new(input_line), layout[1]);

    if let Some(err) = error {
        f.render_widget(
            Paragraph::new(Span::styled(err, Style::default().fg(Color::Red))),
            layout[2],
        );
    }
}
