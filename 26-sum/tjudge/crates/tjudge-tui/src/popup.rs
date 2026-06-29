use ratatui::{
    Frame,
    layout::{Constraint, Direction, Flex, Layout, Rect},
    style::{Color, Modifier, Style},
    text::{Line, Span},
    widgets::{Block, Borders, Clear, List, ListItem, Paragraph},
};

use crate::app::FormState;

/// Centre a rect of (width, height) within the given area.
pub fn centered_rect(width: u16, height: u16, area: Rect) -> Rect {
    let popup_layout = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Fill(1),
            Constraint::Length(height),
            Constraint::Fill(1),
        ])
        .flex(Flex::Center)
        .split(area);
    Layout::default()
        .direction(Direction::Horizontal)
        .constraints([
            Constraint::Fill(1),
            Constraint::Length(width),
            Constraint::Fill(1),
        ])
        .flex(Flex::Center)
        .split(popup_layout[1])[1]
}

/// Render a CRUD popup form.
pub fn render_popup(f: &mut Frame, state: &FormState, area: Rect) {
    let total_fields = state.fields.len() + state.checklist.is_some() as usize;
    let form_height = (total_fields as u16 * 3 + 4).min(area.height.saturating_sub(4));
    let form_width = (area.width * 3 / 4)
        .max(50)
        .min(area.width.saturating_sub(4));
    let popup_area = centered_rect(form_width, form_height, area);

    f.render_widget(Clear, popup_area);

    let block = Block::default()
        .title(format!(" {} ", state.title))
        .borders(Borders::ALL)
        .border_style(Style::default().fg(Color::Yellow));
    f.render_widget(block.clone(), popup_area);

    let inner = block.inner(popup_area);

    // Split inner into rows: 3 lines per text field, remainder for checklist
    let mut constraints = vec![Constraint::Length(3); state.fields.len()];
    if state.checklist.is_some() {
        constraints.push(Constraint::Min(3));
    }
    if constraints.is_empty() {
        return;
    }
    let rows = Layout::default()
        .direction(Direction::Vertical)
        .constraints(constraints)
        .split(inner);

    // Text fields
    for (i, field) in state.fields.iter().enumerate() {
        let focused = !state.in_checklist && state.focused == i;
        let border_style = if focused {
            Style::default().fg(Color::Cyan)
        } else {
            Style::default().fg(Color::Gray)
        };
        let label = if field.required {
            format!("{} *", field.label)
        } else {
            field.label.to_string()
        };
        let block = Block::default()
            .title(label)
            .borders(Borders::ALL)
            .border_style(border_style);
        let inner_r = block.inner(rows[i]);
        f.render_widget(block, rows[i]);

        // Cursor rendering. `Input::cursor()` is a char (codepoint) index, so
        // convert it to a byte offset before slicing — otherwise multibyte
        // fields (e.g. Chinese task titles) panic on a non-char boundary.
        let value = field.value();
        let cursor_pos = field.input.cursor();
        let byte_pos = value
            .char_indices()
            .nth(cursor_pos)
            .map(|(b, _)| b)
            .unwrap_or(value.len());
        let before = &value[..byte_pos];
        let cursor_char = value
            .chars()
            .nth(cursor_pos)
            .map(|c| c.to_string())
            .unwrap_or_else(|| " ".to_string());
        let after: String = value.chars().skip(cursor_pos + 1).collect();

        let text = if focused {
            Line::from(vec![
                Span::raw(before),
                Span::styled(
                    cursor_char,
                    Style::default().bg(Color::White).fg(Color::Black),
                ),
                Span::raw(after),
            ])
        } else {
            Line::from(Span::raw(value))
        };
        f.render_widget(Paragraph::new(text), inner_r);

        if let Some(err) = &field.error {
            // Show error below the field if there's space
            if rows[i].y + 2 < inner.y + inner.height {
                let err_area = Rect::new(
                    rows[i].x + 1,
                    rows[i].y + rows[i].height - 1,
                    rows[i].width.saturating_sub(2),
                    1,
                );
                f.render_widget(
                    Paragraph::new(Span::styled(err.as_str(), Style::default().fg(Color::Red))),
                    err_area,
                );
            }
        }
    }

    // Checklist field
    if let Some(cl) = &state.checklist {
        let idx = state.fields.len();
        if idx < rows.len() {
            let focused = state.in_checklist;
            let border_style = if focused {
                Style::default().fg(Color::Cyan)
            } else {
                Style::default().fg(Color::Gray)
            };
            let block = Block::default()
                .title(cl.label)
                .borders(Borders::ALL)
                .border_style(border_style);
            let cl_inner = block.inner(rows[idx]);
            f.render_widget(block, rows[idx]);

            let items: Vec<ListItem> = cl
                .items
                .iter()
                .enumerate()
                .map(|(i, (id, sel))| {
                    let prefix = if *sel { "[x] " } else { "[ ] " };
                    let style = if i == cl.cursor && focused {
                        Style::default().add_modifier(Modifier::REVERSED)
                    } else {
                        Style::default()
                    };
                    ListItem::new(format!("{prefix}{id}")).style(style)
                })
                .collect();
            f.render_widget(List::new(items), cl_inner);
        }
    }

    // Footer hint
    let hint = if state.in_checklist {
        " <Space> toggle  <↑/↓> navigate  <Tab> next field  <Enter> submit  <Esc> cancel "
    } else {
        " <Tab> next field  <Enter> submit  <Esc> cancel "
    };
    let hint_area = Rect::new(
        popup_area.x,
        popup_area.y + popup_area.height.saturating_sub(1),
        popup_area.width,
        1,
    );
    f.render_widget(
        Paragraph::new(Span::styled(hint, Style::default().fg(Color::DarkGray))),
        hint_area,
    );
}

/// Render a yes/no confirmation popup.
pub fn render_confirm(f: &mut Frame, message: &str, area: Rect) {
    let height = 5u16;
    let width = (message.len() as u16 + 10)
        .max(40)
        .min(area.width.saturating_sub(4));
    let popup_area = centered_rect(width, height, area);
    f.render_widget(Clear, popup_area);
    let block = Block::default()
        .title(" Confirm ")
        .borders(Borders::ALL)
        .border_style(Style::default().fg(Color::Red));
    let inner = block.inner(popup_area);
    f.render_widget(block, popup_area);
    let lines = vec![
        Line::from(Span::raw(message)),
        Line::from(""),
        Line::from(vec![
            Span::styled(
                " y ",
                Style::default()
                    .fg(Color::Green)
                    .add_modifier(Modifier::BOLD),
            ),
            Span::raw(" confirm  "),
            Span::styled(" n / Esc ", Style::default().fg(Color::Gray)),
            Span::raw(" cancel"),
        ]),
    ];
    f.render_widget(Paragraph::new(lines), inner);
}

/// Render a transient error flash at the bottom of the screen.
pub fn render_error_flash(f: &mut Frame, msg: &str, area: Rect) {
    let err_area = Rect::new(
        area.x,
        area.y + area.height.saturating_sub(1),
        area.width,
        1,
    );
    f.render_widget(
        Paragraph::new(Span::styled(
            format!(" ⚠ {msg} "),
            Style::default()
                .bg(Color::Red)
                .fg(Color::White)
                .add_modifier(Modifier::BOLD),
        )),
        err_area,
    );
}
