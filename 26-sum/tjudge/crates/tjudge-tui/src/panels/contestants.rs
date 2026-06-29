use ratatui::{
    Frame,
    layout::{Constraint, Direction, Layout, Rect},
    style::{Color, Modifier, Style},
    text::Span,
    widgets::{Block, Borders, List, ListItem, Paragraph, Row, Table},
};

use crate::app::App;

pub fn render(f: &mut Frame, app: &App, area: Rect) {
    let chunks = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Percentage(40), Constraint::Percentage(60)])
        .split(area);

    render_list(f, app, chunks[0]);
    render_language_map(f, app, chunks[1]);
}

fn render_list(f: &mut Frame, app: &App, area: Rect) {
    let focused = !app.contestants.focus_right;
    let border_style = if focused {
        Style::default().fg(Color::Cyan)
    } else {
        Style::default().fg(Color::Gray)
    };
    let block = Block::default()
        .title(" Contestants ")
        .borders(Borders::ALL)
        .border_style(border_style);
    let inner = block.inner(area);
    f.render_widget(block, area);

    let items: Vec<ListItem> = app
        .config
        .contestants
        .iter()
        .enumerate()
        .map(|(i, c)| {
            let style = if i == app.contestants.list.selected && focused {
                Style::default().add_modifier(Modifier::REVERSED)
            } else if i == app.contestants.list.selected {
                Style::default().add_modifier(Modifier::BOLD)
            } else {
                Style::default()
            };
            ListItem::new(format!("{} — {}  ({})", c.id, c.name, c.workspace_root)).style(style)
        })
        .collect();
    f.render_widget(List::new(items), inner);

    let hint = if focused {
        " <n> new  <e> edit  <d> del  <→> lang map "
    } else {
        " <←> back "
    };
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

fn render_language_map(f: &mut Frame, app: &App, area: Rect) {
    let focused = app.contestants.focus_right;
    let border_style = if focused {
        Style::default().fg(Color::Cyan)
    } else {
        Style::default().fg(Color::Gray)
    };
    let block = Block::default()
        .title(" Language per Task ")
        .borders(Borders::ALL)
        .border_style(border_style);
    let inner = block.inner(area);
    f.render_widget(block, area);

    let contestant = match app.selected_contestant() {
        Some(c) => c,
        None => {
            f.render_widget(Paragraph::new("No contestant selected"), inner);
            return;
        }
    };

    let header = Row::new(vec!["Task ID", "Language"])
        .style(Style::default().add_modifier(Modifier::BOLD))
        .bottom_margin(1);

    let mut task_ids: Vec<&str> = app.config.tasks.iter().map(|t| t.id.as_str()).collect();
    task_ids.sort_unstable();

    let rows: Vec<Row> = task_ids
        .iter()
        .enumerate()
        .map(|(i, tid)| {
            let lang = contestant.language.get(*tid).cloned().unwrap_or_default();
            let style = if i == app.contestants.lang_list.selected && focused {
                Style::default().add_modifier(Modifier::REVERSED)
            } else {
                Style::default()
            };
            Row::new(vec![tid.to_string(), lang]).style(style)
        })
        .collect();

    let widths = [Constraint::Percentage(40), Constraint::Percentage(60)];
    f.render_widget(Table::new(rows, widths).header(header), inner);

    if focused {
        let hint = " <e> edit language ";
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
}
