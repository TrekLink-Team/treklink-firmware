//! Drawing. Every screen ends with a help line.

use ratatui::Frame;
use ratatui::layout::{Constraint, Layout, Rect};
use ratatui::style::{Color, Modifier, Style, Stylize};
use ratatui::text::{Line, Span};
use ratatui::widgets::{Block, Borders, Clear, Gauge, List, ListItem, Paragraph, Row, Table, Wrap};

use crate::app::{App, MenuItem, Pick, Screen, SourceItem};
use crate::flash::{Action, RegionState};
use crate::github;
use crate::variant::{Variant, chip_display};

const HL: Style = Style::new()
    .fg(Color::Black)
    .bg(Color::Cyan)
    .add_modifier(Modifier::BOLD);

pub fn draw(f: &mut Frame, app: &mut App) {
    let [header, body, status, help] = Layout::vertical([
        Constraint::Length(3),
        Constraint::Min(5),
        Constraint::Length(2),
        Constraint::Length(1),
    ])
    .areas(f.area());

    draw_header(f, app, header);
    match app.screen {
        Screen::Ports => draw_ports(f, app, body),
        Screen::Variants => draw_variants(f, app, body),
        Screen::Menu => draw_menu(f, app, body),
        Screen::ImageSource => draw_source(f, app, body),
        Screen::Browser => draw_browser(f, app, body),
        Screen::Fetching => draw_fetch(f, app, body),
        Screen::Confirm => draw_confirm(f, app, body),
        Screen::Latch => draw_latch(f, body),
        Screen::Running => draw_running(f, app, body),
        Screen::Monitor => draw_monitor(f, app, body),
        Screen::NodeInfo => draw_node(f, app, body),
    }
    draw_status(f, app, status);
    f.render_widget(
        Paragraph::new(help_text(app)).style(Style::new().fg(Color::Black).bg(Color::Gray)),
        help,
    );
}

fn help_text(app: &App) -> &'static str {
    match app.screen {
        Screen::Ports => " Up/Down select   Enter choose   r refresh   q quit",
        Screen::Variants => " Up/Down or 1-4 select   Enter choose   Esc back to ports   q quit",
        Screen::Menu => " Up/Down or 1-9 select   Enter run   q quit",
        Screen::ImageSource => " Up/Down or 1-3 select   Enter choose   Esc back",
        Screen::Browser => {
            if app.browser.as_ref().is_some_and(|b| b.goto.is_some()) {
                " Type a folder or file path   Enter go   Esc cancel"
            } else {
                " Up/Down select   Enter open or pick   Backspace parent folder   / type a path   Esc back"
            }
        }
        Screen::Fetching => " Downloading. On an error, Enter or Esc goes back",
        Screen::Confirm => " y proceed   any other key cancels",
        Screen::Latch => " Enter: I am holding the power button   Esc cancel",
        Screen::Running => {
            if app.run.as_ref().is_some_and(|r| r.result.is_some()) {
                " Enter back to the menu"
            } else {
                " Working. Do not unplug the unit. Keys are ignored until the result line appears"
            }
        }
        Screen::Monitor => {
            " Space pause   Up/Down/PgUp/PgDn scroll   End follow   s save   c clear   q back"
        }
        Screen::NodeInfo => " Enter or Esc back   r retry",
    }
}

fn draw_header(f: &mut Frame, app: &App, area: Rect) {
    let port = app
        .port
        .as_ref()
        .map_or("not chosen".to_string(), |p| p.name.clone());
    let variant = app
        .variant
        .map_or("not chosen".to_string(), |v| v.title().to_string());
    let image = app
        .image
        .as_ref()
        .map_or("not chosen".to_string(), |i| i.label.clone());
    let lines = vec![
        Line::from(vec![
            Span::styled(" TrekLink flasher ", Style::new().bold().fg(Color::Cyan)),
            Span::raw(format!(" Port: {port}   Variant: {variant}")),
        ]),
        Line::from(format!(" Image: {image}")),
    ];
    f.render_widget(
        Paragraph::new(lines).block(Block::new().borders(Borders::BOTTOM)),
        area,
    );
}

fn draw_status(f: &mut Frame, app: &App, area: Rect) {
    let warn = matches!(app.screen, Screen::Menu | Screen::Confirm)
        .then(|| app.port_warning())
        .flatten();
    let (text, style) = match (&app.message, warn) {
        (Some((m, true)), _) => (m.clone(), Style::new().fg(Color::Red).bold()),
        (Some((m, false)), _) => (m.clone(), Style::new().fg(Color::Green)),
        (None, Some(w)) => (w, Style::new().fg(Color::Yellow)),
        (None, None) => (String::new(), Style::new()),
    };
    f.render_widget(
        Paragraph::new(text).style(style).wrap(Wrap { trim: true }),
        area,
    );
}

fn titled(title: &str) -> Block<'_> {
    Block::bordered().title(Span::styled(format!(" {title} "), Style::new().bold()))
}

fn draw_ports(f: &mut Frame, app: &mut App, area: Rect) {
    let block = titled("1. Serial port");
    if let Some(e) = &app.port_error {
        f.render_widget(Paragraph::new(e.as_str()).red().block(block), area);
        return;
    }
    if app.ports.is_empty() {
        let text = "No serial ports found. Plug the unit in with a data cable and press r.\n\nLinux: check that you are in the dialout group. Windows: install the CP210x or CH34x driver for v1 and v3. macOS: ports appear as /dev/cu.usbserial-* or /dev/cu.usbmodem*.";
        f.render_widget(
            Paragraph::new(text).wrap(Wrap { trim: true }).block(block),
            area,
        );
        return;
    }
    let items: Vec<ListItem> = app
        .ports
        .iter()
        .map(|p| ListItem::new(format!("{:<24} {}", p.name, p.describe())))
        .collect();
    f.render_stateful_widget(
        List::new(items)
            .block(block)
            .highlight_style(HL)
            .highlight_symbol("> "),
        area,
        &mut app.port_state,
    );
}

fn draw_variants(f: &mut Frame, app: &mut App, area: Rect) {
    let [list, detail] = Layout::vertical([Constraint::Length(6), Constraint::Min(3)]).areas(area);
    let items: Vec<ListItem> = Variant::ALL
        .iter()
        .map(|v| ListItem::new(v.title()))
        .collect();
    f.render_stateful_widget(
        List::new(items)
            .block(titled("2. Variant"))
            .highlight_style(HL)
            .highlight_symbol("> "),
        list,
        &mut app.variant_state,
    );
    let v = Variant::ALL[app.variant_state.selected().unwrap_or(0)];
    let mut lines = vec![
        Line::from(format!(
            "Chip {}, PlatformIO env {}, at least {} MB flash.",
            chip_display(v.chip()),
            v.pio_env(),
            v.min_flash_bytes() / (1024 * 1024)
        )),
        Line::from(match v.usb_link() {
            crate::variant::UsbLink::NativeUsb => "Connects over ESP32-S3 native USB (303a:1001).",
            crate::variant::UsbLink::UsbUart => {
                "Connects over a USB-UART bridge (CP210x or CH34x)."
            }
        }),
    ];
    if v.has_power_latch() {
        lines.push(
            Line::from("Power latch: hold the power button for the whole flash operation.")
                .yellow(),
        );
    }
    if !v.in_release() {
        lines.push(Line::from("Not built by the release workflow: use a local image.").yellow());
    }
    if let Some(port) = &app.port
        && let Some(c) = port.classified()
        && c.link.is_some_and(|l| l != v.usb_link())
    {
        lines.push(
            Line::from(format!(
                "The chosen port looks like {} ({}).",
                c.friendly, c.hint
            ))
            .yellow(),
        );
    }
    f.render_widget(
        Paragraph::new(lines)
            .wrap(Wrap { trim: true })
            .block(titled("Details")),
        detail,
    );
}

fn draw_menu(f: &mut Frame, app: &mut App, area: Rect) {
    let [list, notes] = Layout::vertical([Constraint::Length(12), Constraint::Min(2)]).areas(area);
    let items: Vec<ListItem> = MenuItem::ALL
        .iter()
        .map(|m| ListItem::new(m.label()))
        .collect();
    f.render_stateful_widget(
        List::new(items)
            .block(titled("Menu"))
            .highlight_style(HL)
            .highlight_symbol("> "),
        list,
        &mut app.menu_state,
    );
    let mut lines: Vec<Line> = Vec::new();
    if let Some(img) = &app.image {
        for n in &img.notes {
            lines.push(Line::from(format!("- {n}")));
        }
    }
    lines
        .push(Line::from(format!("Log file: {}", crate::applog::log_path().display())).dark_gray());
    f.render_widget(
        Paragraph::new(lines)
            .wrap(Wrap { trim: true })
            .block(titled("Notes")),
        notes,
    );
}

fn draw_source(f: &mut Frame, app: &mut App, area: Rect) {
    let [list, detail] = Layout::vertical([Constraint::Length(5), Constraint::Min(3)]).areas(area);
    let v = app.variant.unwrap_or(Variant::V2);
    let items: Vec<ListItem> = SourceItem::ALL
        .iter()
        .map(|s| {
            ListItem::new(match s {
                SourceItem::Release => format!(
                    "1  Latest GitHub release for {} ({})",
                    v.code(),
                    github::repo()
                ),
                SourceItem::Factory => {
                    "2  Local factory image (.factory.bin, written at 0x0)".to_string()
                }
                SourceItem::Parts => {
                    "3  Local application .bin plus bootloader.bin and partitions.bin".to_string()
                }
            })
        })
        .collect();
    f.render_stateful_widget(
        List::new(items)
            .block(titled("3. Image"))
            .highlight_style(HL)
            .highlight_symbol("> "),
        list,
        &mut app.source_state,
    );
    let token = if github::token().is_some() {
        "GITHUB_TOKEN is set: it is sent to api.github.com, so a private repository works."
            .to_string()
    } else {
        "GITHUB_TOKEN is not set: only a public repository can be read. If the repository is private, quit, set GITHUB_TOKEN in the environment and start again.".to_string()
    };
    let mut lines = vec![Line::from(token)];
    if !v.in_release() {
        lines.push(
            Line::from(format!(
                "{} is not in the release; pick a local image.",
                v.code()
            ))
            .yellow(),
        );
    }
    lines.push(Line::from(
        "Parts: pick the application .bin; bootloader.bin, partitions.bin and boot_app0.bin are taken from the same folder when present (a PlatformIO .pio/build/<env> folder has the first two).",
    ));
    f.render_widget(
        Paragraph::new(lines)
            .wrap(Wrap { trim: true })
            .block(titled("Details")),
        detail,
    );
}

fn draw_browser(f: &mut Frame, app: &mut App, area: Rect) {
    let pick = app.pick;
    let Some(b) = &mut app.browser else { return };
    let [info, list] = Layout::vertical([Constraint::Length(3), Constraint::Min(3)]).areas(area);
    let mut text = vec![Line::from(pick.prompt()).bold()];
    match (&b.goto, &b.error) {
        (Some(g), _) => text.push(Line::from(format!("Go to: {g}_")).yellow()),
        (None, Some(e)) => text.push(Line::from(e.as_str()).red()),
        (None, None) => text.push(Line::from(format!("Folder: {}", b.dir.display()))),
    }
    f.render_widget(Paragraph::new(text), info);
    let items: Vec<ListItem> = b
        .entries
        .iter()
        .map(|e| {
            if e.is_dir {
                ListItem::new(format!("[{}]", e.name)).fg(Color::Blue)
            } else {
                ListItem::new(format!("{:<56} {:>10} B", e.name, e.size))
            }
        })
        .collect();
    let title = match pick {
        Pick::Factory => "Factory image",
        Pick::App => "Application image",
        Pick::Bootloader => "Bootloader",
        Pick::Partitions => "Partition table",
    };
    f.render_stateful_widget(
        List::new(items)
            .block(titled(title))
            .highlight_style(HL)
            .highlight_symbol("> "),
        list,
        &mut b.state,
    );
}

fn draw_fetch(f: &mut Frame, app: &App, area: Rect) {
    let Some(fv) = &app.fetch else { return };
    let mut lines: Vec<Line> = fv.log.iter().map(|s| Line::from(s.as_str())).collect();
    match &fv.error {
        Some(e) => lines.push(Line::from(e.as_str()).red().bold()),
        None => lines.push(Line::from("Working...").dark_gray()),
    }
    f.render_widget(
        Paragraph::new(lines)
            .wrap(Wrap { trim: true })
            .block(titled("Latest GitHub release")),
        area,
    );
}

fn plan_rows(regions: &[crate::layout::Region]) -> Vec<Line<'static>> {
    regions
        .iter()
        .map(|r| {
            Line::from(format!(
                "  {:<14} at 0x{:05x}  {:>9} bytes",
                r.name,
                r.offset,
                r.len()
            ))
        })
        .collect()
}

fn draw_confirm(f: &mut Frame, app: &App, area: Rect) {
    let Some((action, regions)) = &app.pending else {
        return;
    };
    let port = app
        .port
        .as_ref()
        .map_or("?".to_string(), |p| p.name.clone());
    let variant = app.variant.map_or("?", Variant::code);
    let mut lines = vec![Line::from(format!("{} on {port} as {variant}", action.title())).bold()];
    lines.push(Line::from(""));
    match action {
        Action::Install => {
            lines.push(Line::from("This ERASES THE WHOLE FLASH: settings, node keys, channels and stored messages are lost.").red().bold());
            lines.push(Line::from("Then it writes:"));
            lines.extend(plan_rows(regions));
        }
        Action::Update => {
            lines.push(Line::from(
                "Writes the regions below. Settings (NVS at 0x9000) and the file system are kept.",
            ));
            lines.extend(plan_rows(regions));
        }
        Action::Erase => {
            lines.push(
                Line::from(
                    "This ERASES THE WHOLE FLASH. The unit will not boot until you run Install.",
                )
                .red()
                .bold(),
            );
        }
        Action::Verify => {}
    }
    lines.push(Line::from(""));
    if let Some(w) = app.port_warning() {
        lines.push(Line::from(w).yellow());
    }
    lines.push(Line::from("The chip is detected first; a chip that does not match the variant is refused before anything is written."));
    lines.push(Line::from(""));
    lines.push(Line::from("Press y to proceed. Any other key cancels.").bold());
    f.render_widget(
        Paragraph::new(lines)
            .wrap(Wrap { trim: true })
            .block(titled("Confirm")),
        area,
    );
}

fn draw_latch(f: &mut Frame, area: Rect) {
    let lines = vec![
        Line::from(""),
        Line::from("v2 power latch").bold(),
        Line::from(""),
        Line::from("Hold the power button now and keep holding until done.")
            .yellow()
            .bold(),
        Line::from(""),
        Line::from(
            "The unit resets into the bootloader and back. Without the button held it loses power on reset.",
        ),
        Line::from("Keep holding until the result line appears on this screen."),
        Line::from(""),
        Line::from("Press Enter while holding the button. Esc cancels."),
    ];
    let popup = centered(area, 80, 12);
    f.render_widget(Clear, popup);
    f.render_widget(
        Paragraph::new(lines)
            .wrap(Wrap { trim: true })
            .block(titled("Before reset")),
        popup,
    );
}

fn centered(area: Rect, w: u16, h: u16) -> Rect {
    let w = w.min(area.width);
    let h = h.min(area.height);
    Rect::new(
        area.x + (area.width - w) / 2,
        area.y + (area.height - h) / 2,
        w,
        h,
    )
}

fn draw_running(f: &mut Frame, app: &App, area: Rect) {
    let Some(run) = &app.run else { return };
    let n = run.regions.len() as u16;
    let [top, bars, logs, result] = Layout::vertical([
        Constraint::Length(if run.latch { 2 } else { 1 }),
        Constraint::Length(n.max(1)),
        Constraint::Min(3),
        Constraint::Length(2),
    ])
    .areas(area);

    let mut top_lines = vec![Line::from(format!("{}: {}", run.action.title(), run.stage)).bold()];
    if run.latch {
        top_lines.push(if run.result.is_none() {
            Line::from("Keep holding the power button until the result line appears.")
                .yellow()
                .bold()
        } else {
            Line::from("You can release the power button once the unit shows it has booted.")
                .yellow()
        });
    }
    f.render_widget(Paragraph::new(top_lines), top);

    if run.regions.is_empty() {
        f.render_widget(
            Paragraph::new("Whole-flash erase, no per-region progress.").dark_gray(),
            bars,
        );
    }
    for (i, r) in run.regions.iter().enumerate().take(bars.height as usize) {
        let row = Rect::new(bars.x, bars.y + i as u16, bars.width, 1);
        let ratio = (r.current as f64 / r.total.max(1) as f64).clamp(0.0, 1.0);
        let (state, color) = match r.state {
            RegionState::Pending => ("waiting", Color::DarkGray),
            RegionState::Writing => ("writing", Color::Cyan),
            RegionState::Verifying => ("checking MD5", Color::Yellow),
            RegionState::Done => ("ok", Color::Green),
            RegionState::Mismatch => ("MISMATCH", Color::Red),
        };
        let label = format!(
            "{:<12} 0x{:05x} {:>8} B  {:<12} {:>3}%",
            r.name,
            r.offset,
            r.len,
            state,
            (ratio * 100.0) as u32
        );
        f.render_widget(
            Gauge::default()
                .ratio(ratio)
                .label(label)
                .gauge_style(Style::new().fg(color).bg(Color::Black)),
            row,
        );
    }

    let height = logs.height.saturating_sub(2) as usize;
    let start = run.log.len().saturating_sub(height);
    let lines: Vec<Line> = run.log[start..]
        .iter()
        .map(|s| Line::from(s.as_str()))
        .collect();
    f.render_widget(Paragraph::new(lines).block(titled("Log")), logs);

    let line = match &run.result {
        None => Line::from("Working. Do not unplug.").dark_gray(),
        Some(Ok(s)) => Line::from(format!("OK  {s}")).green().bold(),
        Some(Err(s)) => Line::from(format!("FAILED  {s}")).red().bold(),
    };
    f.render_widget(Paragraph::new(line).wrap(Wrap { trim: true }), result);
}

fn draw_monitor(f: &mut Frame, app: &App, area: Rect) {
    let Some(m) = &app.monitor else { return };
    let [body, status] = Layout::vertical([Constraint::Min(3), Constraint::Length(1)]).areas(area);
    let all = m.sb.snapshot();
    let end = m
        .paused_at
        .unwrap_or(all.len())
        .min(all.len())
        .saturating_sub(m.scroll);
    let height = body.height.saturating_sub(2) as usize;
    let start = end.saturating_sub(height);
    let lines: Vec<Line> = all[start..end].iter().map(|s| Line::from(*s)).collect();
    let title = if m.paused_at.is_some() {
        format!(
            "Serial monitor (PAUSED, {} lines, {} new)",
            all.len(),
            all.len().saturating_sub(end)
        )
    } else {
        format!("Serial monitor ({} lines)", all.len())
    };
    f.render_widget(Paragraph::new(lines).block(titled(&title)), body);
    f.render_widget(Paragraph::new(m.status.as_str()).dark_gray(), status);
}

fn draw_node(f: &mut Frame, app: &App, area: Rect) {
    let Some(n) = &app.node else { return };
    let block = titled("Node info");
    match &n.result {
        None => f.render_widget(
            Paragraph::new("Connecting with the Meshtastic serial API (config handshake). Times out after 6 s without a reply.")
                .wrap(Wrap { trim: true })
                .block(block),
            area,
        ),
        Some(Err(e)) => f.render_widget(Paragraph::new(e.as_str()).red().wrap(Wrap { trim: true }).block(block), area),
        Some(Ok(rep)) => {
            let mut rows: Vec<Row> = rep
                .rows()
                .into_iter()
                .map(|(k, v)| Row::new(vec![k.to_string(), v]))
                .collect();
            if !rep.complete {
                rows.push(Row::new(vec!["Note".to_string(), "Handshake did not finish; data may be partial.".to_string()]).yellow());
            }
            f.render_widget(Table::new(rows, [Constraint::Length(18), Constraint::Min(10)]).block(block), area);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::app::App;
    use crate::layout::FirmwareImage;
    use crate::layout::tests::fake_factory;
    use ratatui::Terminal;
    use ratatui::backend::TestBackend;
    use std::sync::Arc;

    fn render(app: &mut App, w: u16, h: u16) -> String {
        let mut t = Terminal::new(TestBackend::new(w, h)).unwrap();
        t.draw(|f| draw(f, app)).unwrap();
        let buf = t.backend().buffer().clone();
        buf.content().iter().map(|c| c.symbol()).collect()
    }

    #[test]
    fn every_screen_renders_at_any_size() {
        let mut app = App::new();
        app.variant = Some(Variant::V2);
        let (factory, _) = fake_factory(espflash::target::Chip::Esp32s3, 0x20000);
        app.image = Some(Arc::new(FirmwareImage {
            label: "test image".into(),
            factory: Some(factory),
            ..Default::default()
        }));
        let regions = Action::Update
            .plan(app.image.as_deref(), Variant::V2)
            .unwrap();
        app.pending = Some((Action::Update, regions));
        let screens = [
            Screen::Ports,
            Screen::Variants,
            Screen::Menu,
            Screen::ImageSource,
            Screen::Confirm,
            Screen::Latch,
            Screen::Running,
            Screen::Monitor,
            Screen::NodeInfo,
            Screen::Fetching,
            Screen::Browser,
        ];
        for s in screens {
            app.screen = s;
            for (w, h) in [(100, 30), (80, 24), (30, 8), (1, 1)] {
                let _ = render(&mut app, w, h);
            }
        }
        let (_tx, rx) = std::sync::mpsc::channel();
        app.run = Some(crate::app::RunView {
            action: Action::Update,
            regions: vec![
                crate::app::RegionView {
                    name: "bootloader",
                    offset: 0,
                    len: 0x4000,
                    current: 3,
                    total: 4,
                    state: RegionState::Writing,
                },
                crate::app::RegionView {
                    name: "application",
                    offset: 0x10000,
                    len: 0x20000,
                    current: 0,
                    total: 1,
                    state: RegionState::Mismatch,
                },
            ],
            stage: "Writing".into(),
            log: (0..50).map(|i| format!("line {i}")).collect(),
            result: Some(Err(
                "Verify FAILED: application differ from the file.".into()
            )),
            rx,
            latch: true,
        });
        app.screen = Screen::Running;
        for (w, h) in [(100, 30), (80, 24), (30, 8), (10, 3), (1, 1)] {
            let _ = render(&mut app, w, h);
        }
        let text = render(&mut app, 100, 30);
        assert!(text.contains("FAILED"));
        assert!(text.contains("75%"));
        app.screen = Screen::Latch;
        let text = render(&mut app, 100, 30);
        assert!(text.contains("Hold the power button now"));
        app.screen = Screen::Confirm;
        let text = render(&mut app, 120, 30);
        assert!(text.contains("Press y to proceed"));
        assert!(text.contains("0x10000"));
    }
}
