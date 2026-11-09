//! tk-flasher: a keyboard-only terminal tool that installs, updates, verifies and erases
//! TrekLink firmware, with a serial monitor and a Meshtastic node info screen.

mod app;
mod applog;
mod browser;
mod flash;
mod github;
mod layout;
mod manifest;
mod monitor;
mod nodeinfo;
mod ports;
mod ui;
mod variant;

use std::time::Duration;

use crossterm::event::{self, Event, KeyEventKind};

fn main() -> std::io::Result<()> {
    if let Some(arg) = std::env::args().nth(1) {
        match arg.as_str() {
            "--version" | "-V" => {
                println!("tk-flasher {}", env!("CARGO_PKG_VERSION"));
                return Ok(());
            }
            "--help" | "-h" => {
                println!(
                    "tk-flasher {}\n\nRun without arguments to open the terminal UI.\nLog file: {}\nOptional environment: GITHUB_TOKEN (private repository), TK_FLASHER_REPO (owner/name of a fork).",
                    env!("CARGO_PKG_VERSION"),
                    applog::log_path().display()
                );
                return Ok(());
            }
            "--list-ports" => {
                match ports::list() {
                    Ok(list) if list.is_empty() => println!("No serial ports found."),
                    Ok(list) => {
                        for p in list {
                            println!("{:<24} {}", p.name, p.describe());
                        }
                    }
                    Err(e) => eprintln!("{e}"),
                }
                return Ok(());
            }
            other => {
                eprintln!("Unknown argument {other}. Try --help.");
                std::process::exit(2);
            }
        }
    }

    applog::init();
    let mut terminal = ratatui::init();
    let mut app = app::App::new();
    let result = run(&mut terminal, &mut app);
    ratatui::restore();
    applog::log("tk-flasher exited");
    result
}

fn run(terminal: &mut ratatui::DefaultTerminal, app: &mut app::App) -> std::io::Result<()> {
    while !app.quit {
        app.tick();
        terminal.draw(|f| ui::draw(f, app))?;
        if event::poll(Duration::from_millis(50))?
            && let Event::Key(key) = event::read()?
            && key.kind == KeyEventKind::Press
        {
            app.on_key(key);
        }
    }
    Ok(())
}
