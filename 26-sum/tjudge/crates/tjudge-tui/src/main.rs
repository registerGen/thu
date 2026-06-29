use std::io;
use std::time::Duration;

use crossterm::{
    event::{self, DisableMouseCapture, EnableMouseCapture, Event, KeyEventKind},
    execute,
    terminal::{EnterAlternateScreen, LeaveAlternateScreen, disable_raw_mode, enable_raw_mode},
};
use ratatui::{Terminal, backend::CrosstermBackend};

mod app;
mod events;
mod judging;
mod panels;
mod popup;
mod ui;

use app::{App, Mode};
use judging::poll_judge_results;

fn main() -> io::Result<()> {
    // Panic hook: restore terminal before printing the panic message so the
    // screen doesn't become garbled.
    let original_hook = std::panic::take_hook();
    std::panic::set_hook(Box::new(move |info| {
        let _ = disable_raw_mode();
        let _ = execute!(io::stdout(), LeaveAlternateScreen, DisableMouseCapture);
        original_hook(info);
    }));

    // Set up terminal
    enable_raw_mode()?;
    let mut stdout = io::stdout();
    execute!(stdout, EnterAlternateScreen, EnableMouseCapture)?;
    let backend = CrosstermBackend::new(stdout);
    let mut terminal = Terminal::new(backend)?;

    let mut app = App::new_startup();
    let res = run_app(&mut terminal, &mut app);

    // Restore terminal
    disable_raw_mode()?;
    execute!(
        terminal.backend_mut(),
        LeaveAlternateScreen,
        DisableMouseCapture
    )?;
    terminal.show_cursor()?;

    if let Err(e) = res {
        eprintln!("Error: {e}");
    }
    Ok(())
}

fn run_app(terminal: &mut Terminal<CrosstermBackend<io::Stdout>>, app: &mut App) -> io::Result<()> {
    loop {
        terminal.draw(|f| ui::render(f, app))?;

        if matches!(app.mode, Mode::Quit) {
            return Ok(());
        }

        // Poll judge channel (non-blocking)
        if matches!(app.mode, Mode::Judging) {
            let done = poll_judge_results(app);
            if done {
                app.mode = Mode::Normal;
            }
        }

        // Wait for an event (with 100ms timeout so judging can update)
        if event::poll(Duration::from_millis(100))?
            && let Event::Key(key) = event::read()?
        {
            // Only handle KeyPress events (not release/repeat on some terminals)
            if key.kind == KeyEventKind::Press {
                events::handle_key(app, key);
            }
        }
    }
}
