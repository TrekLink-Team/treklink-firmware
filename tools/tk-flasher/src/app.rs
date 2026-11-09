//! Application state and keyboard handling. Drawing lives in `ui.rs`.

use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::sync::mpsc::{self, Receiver, TryRecvError};

use crossterm::event::{KeyCode, KeyEvent, KeyModifiers};
use ratatui::widgets::ListState;

use crate::applog::{self, log};
use crate::browser::{self, Browser, Outcome};
use crate::flash::{self, Action, Job, RegionState};
use crate::github;
use crate::layout::{self, FirmwareImage, Region};
use crate::monitor::{Monitor, MonitorEvent, Scrollback};
use crate::nodeinfo::{self, NodeReport};
use crate::ports::{self, PortEntry};
use crate::variant::Variant;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Screen {
    Ports,
    Variants,
    Menu,
    ImageSource,
    Browser,
    Fetching,
    Confirm,
    Latch,
    Running,
    Monitor,
    NodeInfo,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MenuItem {
    Image,
    Install,
    Update,
    Verify,
    Erase,
    Monitor,
    NodeInfo,
    Port,
    Variant,
    Quit,
}

impl MenuItem {
    pub const ALL: [MenuItem; 10] = [
        MenuItem::Image,
        MenuItem::Install,
        MenuItem::Update,
        MenuItem::Verify,
        MenuItem::Erase,
        MenuItem::Monitor,
        MenuItem::NodeInfo,
        MenuItem::Port,
        MenuItem::Variant,
        MenuItem::Quit,
    ];

    pub fn label(self) -> &'static str {
        match self {
            MenuItem::Image => "1  Choose image (local file or latest GitHub release)",
            MenuItem::Install => "2  Install: erase all, write the factory image at 0x0",
            MenuItem::Update => {
                "3  Update: keep settings, write bootloader, partitions, boot_app0, app"
            }
            MenuItem::Verify => "4  Verify: compare the MD5 of each region with the file",
            MenuItem::Erase => "5  Erase only (asks to confirm)",
            MenuItem::Monitor => "6  Serial monitor (no reset)",
            MenuItem::NodeInfo => "7  Node info (Meshtastic handshake)",
            MenuItem::Port => "8  Change port",
            MenuItem::Variant => "9  Change variant",
            MenuItem::Quit => "q  Quit",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SourceItem {
    Release,
    Factory,
    Parts,
}

impl SourceItem {
    pub const ALL: [SourceItem; 3] = [SourceItem::Release, SourceItem::Factory, SourceItem::Parts];
}

/// What the file browser is picking.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Pick {
    Factory,
    App,
    Bootloader,
    Partitions,
}

impl Pick {
    pub fn prompt(self) -> &'static str {
        match self {
            Pick::Factory => "Pick the factory image (firmware-*.factory.bin), written at 0x0",
            Pick::App => {
                "Pick the application image (firmware-*.bin or firmware.bin), written at 0x10000"
            }
            Pick::Bootloader => "Pick bootloader.bin (not found next to the application)",
            Pick::Partitions => "Pick partitions.bin (not found next to the application)",
        }
    }
}

#[derive(Default)]
pub struct PartsDraft {
    pub app: Option<(PathBuf, Vec<u8>)>,
    pub bootloader: Option<(PathBuf, Vec<u8>)>,
    pub partitions: Option<(PathBuf, Vec<u8>)>,
    pub boot_app0: Option<(PathBuf, Vec<u8>)>,
}

pub struct RegionView {
    pub name: &'static str,
    pub offset: u32,
    pub len: u32,
    pub current: usize,
    pub total: usize,
    pub state: RegionState,
}

pub struct RunView {
    pub action: Action,
    pub regions: Vec<RegionView>,
    pub stage: String,
    pub log: Vec<String>,
    pub result: Option<Result<String, String>>,
    pub rx: Receiver<flash::Event>,
    pub latch: bool,
}

pub struct MonitorView {
    pub _mon: Monitor,
    pub rx: Receiver<MonitorEvent>,
    pub sb: Scrollback,
    pub paused_at: Option<usize>,
    pub scroll: usize,
    pub status: String,
}

pub struct NodeView {
    pub rx: Receiver<Result<NodeReport, String>>,
    pub result: Option<Result<NodeReport, String>>,
}

pub enum FetchMsg {
    Log(String),
    Done(Result<github::Fetched, String>),
}

pub struct FetchView {
    pub rx: Receiver<FetchMsg>,
    pub log: Vec<String>,
    pub error: Option<String>,
}

pub struct App {
    pub screen: Screen,
    pub quit: bool,

    pub ports: Vec<PortEntry>,
    pub port_state: ListState,
    pub port_error: Option<String>,
    pub port: Option<PortEntry>,

    pub variant_state: ListState,
    pub variant: Option<Variant>,

    pub menu_state: ListState,
    pub message: Option<(String, bool)>,

    pub image: Option<Arc<FirmwareImage>>,
    pub source_state: ListState,
    pub browser: Option<Browser>,
    pub pick: Pick,
    pub parts: PartsDraft,
    pub last_dir: PathBuf,
    pub fetch: Option<FetchView>,

    pub pending: Option<(Action, Vec<Region>)>,
    pub run: Option<RunView>,
    pub monitor: Option<MonitorView>,
    pub node: Option<NodeView>,
}

fn selected(state: &ListState) -> usize {
    state.selected().unwrap_or(0)
}

fn step(state: &mut ListState, len: usize, delta: isize) {
    if len == 0 {
        state.select(None);
        return;
    }
    let cur = state.selected().unwrap_or(0) as isize;
    let next = (cur + delta).rem_euclid(len as isize);
    state.select(Some(next as usize));
}

impl App {
    pub fn new() -> Self {
        let mut app = App {
            screen: Screen::Ports,
            quit: false,
            ports: Vec::new(),
            port_state: ListState::default(),
            port_error: None,
            port: None,
            variant_state: ListState::default().with_selected(Some(0)),
            variant: None,
            menu_state: ListState::default().with_selected(Some(0)),
            message: None,
            image: None,
            source_state: ListState::default().with_selected(Some(0)),
            browser: None,
            pick: Pick::Factory,
            parts: PartsDraft::default(),
            last_dir: std::env::current_dir().unwrap_or_else(|_| applog::base_dir().to_path_buf()),
            fetch: None,
            pending: None,
            run: None,
            monitor: None,
            node: None,
        };
        app.refresh_ports();
        app
    }

    pub fn info(&mut self, s: impl Into<String>) {
        let s = s.into();
        log(&s);
        self.message = Some((s, false));
    }

    pub fn error(&mut self, s: impl Into<String>) {
        let s = s.into();
        log(&format!("ERROR {s}"));
        self.message = Some((s, true));
    }

    pub fn refresh_ports(&mut self) {
        match ports::list() {
            Ok(p) => {
                log(&format!("ports: {} found", p.len()));
                for e in &p {
                    log(&format!("  {} {}", e.name, e.describe()));
                }
                self.ports = p;
                self.port_error = None;
            }
            Err(e) => {
                self.ports.clear();
                self.port_error = Some(e);
            }
        }
        let keep = self
            .port
            .as_ref()
            .and_then(|cur| self.ports.iter().position(|p| p.name == cur.name));
        self.port_state
            .select(keep.or(if self.ports.is_empty() { None } else { Some(0) }));
    }

    /// Drain worker channels. Called every UI tick.
    pub fn tick(&mut self) {
        self.tick_run();
        self.tick_fetch();
        self.tick_monitor();
        if let Some(n) = &mut self.node
            && n.result.is_none()
            && let Ok(r) = n.rx.try_recv()
        {
            match &r {
                Ok(rep) => log(&format!("node info: {rep:?}")),
                Err(e) => log(&format!("node info failed: {e}")),
            }
            n.result = Some(r);
        }
    }

    fn tick_run(&mut self) {
        let Some(run) = &mut self.run else { return };
        loop {
            match run.rx.try_recv() {
                Ok(flash::Event::Log(s)) => {
                    log(&s);
                    run.log.push(s);
                }
                Ok(flash::Event::Stage(s)) => {
                    log(&format!("stage: {s}"));
                    run.stage = s;
                }
                Ok(flash::Event::Progress {
                    index,
                    current,
                    total,
                }) => {
                    if let Some(r) = run.regions.get_mut(index) {
                        r.current = current;
                        r.total = total;
                    }
                }
                Ok(flash::Event::RegionState { index, state }) => {
                    if let Some(r) = run.regions.get_mut(index) {
                        r.state = state;
                    }
                }
                Ok(flash::Event::Finished(res)) => {
                    run.stage = "Finished".into();
                    run.result = Some(res);
                }
                Err(TryRecvError::Empty) => break,
                Err(TryRecvError::Disconnected) => {
                    if run.result.is_none() {
                        run.result = Some(Err(
                            "The worker stopped without a result. See the log file.".into(),
                        ));
                    }
                    break;
                }
            }
        }
    }

    fn tick_fetch(&mut self) {
        let Some(f) = &mut self.fetch else { return };
        let mut done = None;
        while let Ok(m) = f.rx.try_recv() {
            match m {
                FetchMsg::Log(s) => {
                    log(&s);
                    f.log.push(s);
                }
                FetchMsg::Done(r) => done = Some(r),
            }
        }
        match done {
            Some(Ok(fetched)) => {
                let mut img = fetched.image;
                img.notes.push(
                    "Release images carry no BLE OTA loader and no LittleFS image. After Install those partitions are blank; the firmware formats LittleFS on first boot (src/FSCommon.h).".into(),
                );
                if let Some(dir) = &fetched.saved_to {
                    img.notes
                        .push(format!("A copy was saved in {}", dir.display()));
                }
                let tag = fetched.tag.clone();
                if self.accept_image(img) {
                    self.fetch = None;
                    self.screen = Screen::Menu;
                    self.info(format!("Image ready: release {tag}"));
                } else if let Some(f) = &mut self.fetch {
                    f.error = self.message.as_ref().map(|m| m.0.clone());
                }
            }
            Some(Err(e)) => {
                log(&format!("ERROR fetch: {e}"));
                f.error = Some(e);
            }
            None => {}
        }
    }

    fn tick_monitor(&mut self) {
        let Some(m) = &mut self.monitor else { return };
        while let Ok(ev) = m.rx.try_recv() {
            match ev {
                MonitorEvent::Data(b) => m.sb.push_bytes(&b),
                MonitorEvent::Closed(s) => {
                    log(&s);
                    m.status = s;
                }
            }
        }
    }

    /// Check an image against the variant before keeping it.
    fn accept_image(&mut self, img: FirmwareImage) -> bool {
        let Some(variant) = self.variant else {
            self.error("Choose a variant first.");
            return false;
        };
        let chip = variant.chip();
        let check = if img.factory.is_some() {
            layout::install_plan(&img, chip).and_then(|_| layout::update_plan(&img, chip))
        } else {
            layout::update_plan(&img, chip)
        };
        match check {
            Ok(_) => {
                log(&format!("image accepted: {}", img.label));
                for n in &img.notes {
                    log(&format!("  note: {n}"));
                }
                self.image = Some(Arc::new(img));
                true
            }
            Err(e) => {
                self.error(format!("Image rejected for {}: {e}", variant.code()));
                false
            }
        }
    }

    pub fn on_key(&mut self, key: KeyEvent) {
        if key.modifiers.contains(KeyModifiers::CONTROL) && key.code == KeyCode::Char('c') {
            if self.screen == Screen::Running
                && self.run.as_ref().is_some_and(|r| r.result.is_none())
            {
                self.error("A flash job is running. Wait for the result line; stopping now could leave the unit unbootable.");
                return;
            }
            self.quit = true;
            return;
        }
        match self.screen {
            Screen::Ports => self.key_ports(key),
            Screen::Variants => self.key_variants(key),
            Screen::Menu => self.key_menu(key),
            Screen::ImageSource => self.key_source(key),
            Screen::Browser => self.key_browser(key),
            Screen::Fetching => {
                if self.fetch.as_ref().is_some_and(|f| f.error.is_some())
                    && matches!(key.code, KeyCode::Esc | KeyCode::Enter)
                {
                    self.fetch = None;
                    self.screen = Screen::ImageSource;
                }
            }
            Screen::Confirm => self.key_confirm(key),
            Screen::Latch => self.key_latch(key),
            Screen::Running => {
                if self.run.as_ref().is_some_and(|r| r.result.is_some())
                    && matches!(key.code, KeyCode::Enter | KeyCode::Esc | KeyCode::Char('q'))
                {
                    if let Some(Some(res)) = self.run.take().map(|r| r.result) {
                        match res {
                            Ok(s) => self.info(s),
                            Err(s) => self.error(s),
                        }
                    }
                    self.screen = Screen::Menu;
                }
            }
            Screen::Monitor => self.key_monitor(key),
            Screen::NodeInfo => match key.code {
                KeyCode::Esc | KeyCode::Char('q') | KeyCode::Enter => {
                    if self.node.as_ref().is_some_and(|n| n.result.is_some()) {
                        self.node = None;
                        self.screen = Screen::Menu;
                    }
                }
                KeyCode::Char('r') if self.node.as_ref().is_some_and(|n| n.result.is_some()) => {
                    self.start_node_info()
                }
                _ => {}
            },
        }
    }

    fn key_ports(&mut self, key: KeyEvent) {
        match key.code {
            KeyCode::Up | KeyCode::Char('k') => step(&mut self.port_state, self.ports.len(), -1),
            KeyCode::Down | KeyCode::Char('j') => step(&mut self.port_state, self.ports.len(), 1),
            KeyCode::Char('r') => self.refresh_ports(),
            KeyCode::Enter => {
                if let Some(p) = self.ports.get(selected(&self.port_state)).cloned() {
                    log(&format!("port chosen: {} {}", p.name, p.describe()));
                    self.port = Some(p);
                    self.screen = if self.variant.is_some() {
                        Screen::Menu
                    } else {
                        Screen::Variants
                    };
                }
            }
            KeyCode::Char('q') => self.quit = true,
            KeyCode::Esc if self.port.is_some() && self.variant.is_some() => {
                self.screen = Screen::Menu
            }
            _ => {}
        }
    }

    fn key_variants(&mut self, key: KeyEvent) {
        match key.code {
            KeyCode::Up | KeyCode::Char('k') => step(&mut self.variant_state, 4, -1),
            KeyCode::Down | KeyCode::Char('j') => step(&mut self.variant_state, 4, 1),
            KeyCode::Char(c @ '1'..='4') => {
                self.variant_state.select(Some(c as usize - '1' as usize));
                self.choose_variant();
            }
            KeyCode::Enter => self.choose_variant(),
            KeyCode::Esc => self.screen = Screen::Ports,
            KeyCode::Char('q') => self.quit = true,
            _ => {}
        }
    }

    fn choose_variant(&mut self) {
        let v = Variant::ALL[selected(&self.variant_state)];
        if self.variant != Some(v) && self.image.is_some() {
            self.image = None;
            self.info(format!(
                "Variant changed to {}; choose the image again.",
                v.code()
            ));
        } else {
            self.info(format!("Variant: {}", v.title()));
        }
        self.variant = Some(v);
        self.screen = Screen::Menu;
    }

    /// A one-line warning when the port's USB ID does not fit the variant.
    pub fn port_warning(&self) -> Option<String> {
        let (port, variant) = (self.port.as_ref()?, self.variant?);
        let c = port.classified()?;
        let link = c.link?;
        (link != variant.usb_link()).then(|| {
            format!(
                "This port looks like {} ({}), but {} connects over {}. The chip check will refuse a wrong board.",
                c.friendly,
                c.hint,
                variant.code(),
                match variant.usb_link() {
                    crate::variant::UsbLink::NativeUsb => "ESP32-S3 native USB",
                    crate::variant::UsbLink::UsbUart => "a USB-UART bridge",
                }
            )
        })
    }

    fn key_menu(&mut self, key: KeyEvent) {
        let n = MenuItem::ALL.len();
        let item = match key.code {
            KeyCode::Up | KeyCode::Char('k') => {
                step(&mut self.menu_state, n, -1);
                return;
            }
            KeyCode::Down | KeyCode::Char('j') => {
                step(&mut self.menu_state, n, 1);
                return;
            }
            KeyCode::Char(c @ '1'..='9') => {
                let i = c as usize - '1' as usize;
                self.menu_state.select(Some(i));
                MenuItem::ALL[i]
            }
            KeyCode::Char('q') => MenuItem::Quit,
            KeyCode::Enter => MenuItem::ALL[selected(&self.menu_state)],
            _ => return,
        };
        self.message = None;
        match item {
            MenuItem::Image => self.screen = Screen::ImageSource,
            MenuItem::Install => self.prepare(Action::Install),
            MenuItem::Update => self.prepare(Action::Update),
            MenuItem::Verify => self.prepare(Action::Verify),
            MenuItem::Erase => self.prepare(Action::Erase),
            MenuItem::Monitor => self.start_monitor(),
            MenuItem::NodeInfo => self.start_node_info(),
            MenuItem::Port => {
                self.refresh_ports();
                self.screen = Screen::Ports;
            }
            MenuItem::Variant => self.screen = Screen::Variants,
            MenuItem::Quit => self.quit = true,
        }
    }

    fn key_source(&mut self, key: KeyEvent) {
        match key.code {
            KeyCode::Up | KeyCode::Char('k') => step(&mut self.source_state, 3, -1),
            KeyCode::Down | KeyCode::Char('j') => step(&mut self.source_state, 3, 1),
            KeyCode::Char(c @ '1'..='3') => {
                self.source_state.select(Some(c as usize - '1' as usize));
                self.choose_source();
            }
            KeyCode::Enter => self.choose_source(),
            KeyCode::Esc | KeyCode::Char('q') => self.screen = Screen::Menu,
            _ => {}
        }
    }

    fn choose_source(&mut self) {
        let Some(variant) = self.variant else { return };
        match SourceItem::ALL[selected(&self.source_state)] {
            SourceItem::Release => {
                if !variant.in_release() {
                    self.error(format!(
                        "The release workflow does not build {} (env {}). Use a local image.",
                        variant.code(),
                        variant.pio_env()
                    ));
                    return;
                }
                log(&format!("fetching latest release for {}", variant.code()));
                let (tx, rx) = mpsc::channel();
                let save = applog::base_dir().join("tk-flasher-downloads");
                std::thread::spawn(move || {
                    let say_tx = tx.clone();
                    let say = move |s: String| {
                        let _ = say_tx.send(FetchMsg::Log(s));
                    };
                    let r = github::fetch_latest(variant, Some(&save), &say);
                    let _ = tx.send(FetchMsg::Done(r));
                });
                self.fetch = Some(FetchView {
                    rx,
                    log: Vec::new(),
                    error: None,
                });
                self.screen = Screen::Fetching;
            }
            SourceItem::Factory => self.open_browser(Pick::Factory),
            SourceItem::Parts => {
                self.parts = PartsDraft::default();
                self.open_browser(Pick::App);
            }
        }
    }

    fn open_browser(&mut self, pick: Pick) {
        self.pick = pick;
        let dir = self
            .browser
            .as_ref()
            .map(|b| b.dir.clone())
            .unwrap_or_else(|| self.last_dir.clone());
        self.browser = Some(Browser::new(dir));
        self.screen = Screen::Browser;
    }

    fn key_browser(&mut self, key: KeyEvent) {
        let Some(b) = &mut self.browser else { return };
        if let Some(text) = &mut b.goto {
            match key.code {
                KeyCode::Esc => b.goto = None,
                KeyCode::Backspace => {
                    text.pop();
                }
                KeyCode::Char(c) => text.push(c),
                KeyCode::Enter => {
                    let t = std::mem::take(text);
                    b.goto = None;
                    if let Outcome::Picked(p) = b.jump(&t) {
                        self.picked(p);
                    }
                }
                _ => {}
            }
            return;
        }
        match key.code {
            KeyCode::Up | KeyCode::Char('k') => b.move_by(-1),
            KeyCode::Down | KeyCode::Char('j') => b.move_by(1),
            KeyCode::PageUp => b.move_by(-10),
            KeyCode::PageDown => b.move_by(10),
            KeyCode::Backspace | KeyCode::Left => b.up(),
            KeyCode::Char('/') => b.goto = Some(String::new()),
            KeyCode::Enter | KeyCode::Right => {
                if let Outcome::Picked(p) = b.enter() {
                    self.picked(p);
                }
            }
            KeyCode::Esc | KeyCode::Char('q') => self.screen = Screen::ImageSource,
            _ => {}
        }
    }

    fn picked(&mut self, path: PathBuf) {
        if let Some(dir) = path.parent() {
            self.last_dir = dir.to_path_buf();
        }
        let data = match std::fs::read(&path) {
            Ok(d) => d,
            Err(e) => {
                self.error(format!("Cannot read {}: {e}", path.display()));
                return;
            }
        };
        log(&format!(
            "picked {:?} for {:?} ({} bytes)",
            path,
            self.pick,
            data.len()
        ));
        match self.pick {
            Pick::Factory => {
                let img = FirmwareImage {
                    label: format!("local factory image {}", display_name(&path)),
                    factory: Some(data),
                    ..Default::default()
                };
                if self.accept_image(img) {
                    self.screen = Screen::Menu;
                    self.info(format!("Image ready: {}", path.display()));
                }
            }
            Pick::App => {
                self.parts.app = Some((path.clone(), data));
                for (name, slot) in [
                    ("bootloader.bin", 0),
                    ("partitions.bin", 1),
                    ("boot_app0.bin", 2),
                ] {
                    if let Some(p) = browser::sibling(&path, name)
                        && let Ok(d) = std::fs::read(&p)
                    {
                        let v = Some((p, d));
                        match slot {
                            0 => self.parts.bootloader = v,
                            1 => self.parts.partitions = v,
                            _ => self.parts.boot_app0 = v,
                        }
                    }
                }
                self.next_part();
            }
            Pick::Bootloader => {
                self.parts.bootloader = Some((path, data));
                self.next_part();
            }
            Pick::Partitions => {
                self.parts.partitions = Some((path, data));
                self.next_part();
            }
        }
    }

    fn next_part(&mut self) {
        if self.parts.bootloader.is_none() {
            self.open_browser(Pick::Bootloader);
            return;
        }
        if self.parts.partitions.is_none() {
            self.open_browser(Pick::Partitions);
            return;
        }
        let parts = std::mem::take(&mut self.parts);
        let (app_path, app) = parts.app.unwrap();
        let mut notes = Vec::new();
        let names =
            |o: &Option<(PathBuf, Vec<u8>)>| o.as_ref().map(|(p, _)| p.display().to_string());
        notes.push(format!(
            "bootloader: {}",
            names(&parts.bootloader).unwrap_or_default()
        ));
        notes.push(format!(
            "partitions: {}",
            names(&parts.partitions).unwrap_or_default()
        ));
        match names(&parts.boot_app0) {
            Some(p) => notes.push(format!("boot_app0: {p}")),
            None => notes.push(
                "boot_app0.bin was not next to the application; the standard arduino-esp32 boot_app0 (selects ota_0) is used.".into(),
            ),
        }
        let img = FirmwareImage {
            label: format!("local parts, application {}", display_name(&app_path)),
            factory: None,
            app: Some(app),
            bootloader: parts.bootloader.map(|x| x.1),
            partitions: parts.partitions.map(|x| x.1),
            boot_app0: parts.boot_app0.map(|x| x.1),
            notes,
        };
        if self.accept_image(img) {
            self.screen = Screen::Menu;
            self.info(format!("Image ready: {}", app_path.display()));
        } else {
            self.screen = Screen::Menu;
        }
    }

    fn prepare(&mut self, action: Action) {
        let (Some(_), Some(variant)) = (&self.port, self.variant) else {
            self.error("Choose a port and a variant first.");
            return;
        };
        if action.needs_image() && self.image.is_none() {
            self.error(format!(
                "{} needs an image. Choose one with 1 first.",
                action.title()
            ));
            return;
        }
        match action.plan(self.image.as_deref(), variant) {
            Ok(regions) => {
                log(&format!(
                    "prepare {}: {} region(s)",
                    action.title(),
                    regions.len()
                ));
                self.pending = Some((action, regions));
                if action.destructive() {
                    self.screen = Screen::Confirm;
                } else {
                    self.after_confirm();
                }
            }
            Err(e) => self.error(format!("{} refused: {e}", action.title())),
        }
    }

    fn after_confirm(&mut self) {
        if self.variant.is_some_and(Variant::has_power_latch) {
            self.screen = Screen::Latch;
        } else {
            self.start_job();
        }
    }

    fn key_confirm(&mut self, key: KeyEvent) {
        match key.code {
            KeyCode::Char('y') | KeyCode::Char('Y') => {
                log("confirmed");
                self.after_confirm();
            }
            _ => {
                log("cancelled at confirm");
                self.pending = None;
                self.screen = Screen::Menu;
                self.info("Cancelled. Nothing was written.");
            }
        }
    }

    fn key_latch(&mut self, key: KeyEvent) {
        match key.code {
            KeyCode::Enter => {
                log("v2 power latch: user says the button is held");
                self.start_job();
            }
            KeyCode::Esc | KeyCode::Char('q') => {
                self.pending = None;
                self.screen = Screen::Menu;
                self.info("Cancelled. Nothing was written.");
            }
            _ => {}
        }
    }

    fn start_job(&mut self) {
        let (Some((action, regions)), Some(port), Some(variant)) =
            (self.pending.take(), self.port.clone(), self.variant)
        else {
            return;
        };
        log(&format!(
            "START {} on {} as {} with {}",
            action.title(),
            port.name,
            variant.code(),
            self.image
                .as_ref()
                .map_or("no image".into(), |i| i.label.clone())
        ));
        let views = regions
            .iter()
            .map(|r| RegionView {
                name: r.name,
                offset: r.offset,
                len: r.len(),
                current: 0,
                total: 1,
                state: RegionState::Pending,
            })
            .collect();
        let (tx, rx) = mpsc::channel();
        let job = Job {
            port,
            variant,
            action,
            regions,
        };
        std::thread::spawn(move || flash::run(job, tx));
        self.run = Some(RunView {
            action,
            regions: views,
            stage: "Starting".into(),
            log: Vec::new(),
            result: None,
            rx,
            latch: variant.has_power_latch(),
        });
        self.screen = Screen::Running;
    }

    fn start_monitor(&mut self) {
        let Some(port) = self.port.clone() else {
            self.error("Choose a port first.");
            return;
        };
        let (tx, rx) = mpsc::channel();
        match Monitor::start(&port.name, flash::BAUD, tx) {
            Ok(m) => {
                log(&format!("monitor opened on {}", port.name));
                self.monitor = Some(MonitorView {
                    _mon: m,
                    rx,
                    sb: Scrollback::new(20_000),
                    paused_at: None,
                    scroll: 0,
                    status: format!(
                        "Monitoring {} at {} baud. DTR high, RTS low, no reset.",
                        port.name,
                        flash::BAUD
                    ),
                });
                self.screen = Screen::Monitor;
            }
            Err(e) => self.error(e),
        }
    }

    fn key_monitor(&mut self, key: KeyEvent) {
        let Some(m) = &mut self.monitor else { return };
        match key.code {
            KeyCode::Char('q') | KeyCode::Esc => {
                log("monitor closed");
                self.monitor = None;
                self.screen = Screen::Menu;
            }
            KeyCode::Char(' ') | KeyCode::Char('p') => {
                if m.paused_at.is_some() {
                    m.paused_at = None;
                    m.scroll = 0;
                } else {
                    m.paused_at = Some(m.sb.len());
                }
            }
            KeyCode::Up | KeyCode::Char('k') => scroll(m, 1),
            KeyCode::Down | KeyCode::Char('j') => scroll(m, -1),
            KeyCode::PageUp => scroll(m, 20),
            KeyCode::PageDown => scroll(m, -20),
            KeyCode::End | KeyCode::Char('G') => {
                m.paused_at = None;
                m.scroll = 0;
            }
            KeyCode::Char('c') => {
                m.sb = Scrollback::new(20_000);
                m.paused_at = None;
                m.scroll = 0;
            }
            KeyCode::Char('s') => {
                let path = applog::base_dir().join(format!("monitor-{}.log", applog::file_stamp()));
                m.status = match std::fs::write(&path, m.sb.to_text()) {
                    Ok(()) => format!("Saved {} lines to {}", m.sb.len(), path.display()),
                    Err(e) => format!("Could not save to {}: {e}", path.display()),
                };
                log(&m.status);
            }
            _ => {}
        }
    }

    fn start_node_info(&mut self) {
        let Some(port) = self.port.clone() else {
            self.error("Choose a port first.");
            return;
        };
        log(&format!("node info on {}", port.name));
        let (tx, rx) = mpsc::channel();
        std::thread::spawn(move || {
            let _ = tx.send(nodeinfo::query(&port.name));
        });
        self.node = Some(NodeView { rx, result: None });
        self.screen = Screen::NodeInfo;
    }
}

fn scroll(m: &mut MonitorView, delta: isize) {
    if m.paused_at.is_none() {
        m.paused_at = Some(m.sb.len());
    }
    let max = m.paused_at.unwrap_or(0).saturating_sub(1);
    m.scroll = (m.scroll as isize + delta).clamp(0, max as isize) as usize;
}

fn display_name(p: &Path) -> String {
    p.file_name().map_or_else(
        || p.display().to_string(),
        |n| n.to_string_lossy().into_owned(),
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn list_stepping_wraps() {
        let mut s = ListState::default().with_selected(Some(0));
        step(&mut s, 3, -1);
        assert_eq!(s.selected(), Some(2));
        step(&mut s, 3, 1);
        assert_eq!(s.selected(), Some(0));
        step(&mut s, 0, 1);
        assert_eq!(s.selected(), None);
    }

    #[test]
    fn menu_shortcuts_cover_nine_items_and_quit() {
        assert_eq!(MenuItem::ALL.len(), 10);
        assert_eq!(MenuItem::ALL[8], MenuItem::Variant);
        assert_eq!(MenuItem::ALL[9], MenuItem::Quit);
    }
}
