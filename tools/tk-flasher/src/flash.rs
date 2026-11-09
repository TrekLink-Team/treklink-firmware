//! Install, Update, Verify and Erase, run on a worker thread with espflash as a library.
//!
//! Always 115200 baud: a switch to a higher rate failed once on a v2, and the native USB
//! port does not need it. ESP32-S3 native USB uses the USB Serial/JTAG reset sequence;
//! USB-UART bridges use the classic DTR/RTS sequence.

use std::sync::mpsc::Sender;

use espflash::connection::{Connection, ResetAfterOperation, ResetBeforeOperation};
use espflash::flasher::Flasher;
use espflash::image_format::Segment;
use espflash::target::ProgressCallbacks;
use md5::{Digest, Md5};
use serialport::{FlowControl, UsbPortInfo};

use crate::layout::{self, FirmwareImage, Region};
use crate::ports::PortEntry;
use crate::variant::{self, Variant, chip_display};

pub const BAUD: u32 = 115_200;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Action {
    Install,
    Update,
    Verify,
    Erase,
}

impl Action {
    pub fn title(self) -> &'static str {
        match self {
            Action::Install => "Install",
            Action::Update => "Update",
            Action::Verify => "Verify",
            Action::Erase => "Erase only",
        }
    }

    pub fn needs_image(self) -> bool {
        !matches!(self, Action::Erase)
    }

    pub fn destructive(self) -> bool {
        !matches!(self, Action::Verify)
    }

    /// The region plan for this action, so the UI can show bars before the job starts.
    pub fn plan(
        self,
        img: Option<&FirmwareImage>,
        variant: Variant,
    ) -> Result<Vec<Region>, String> {
        let chip = variant.chip();
        match (self, img) {
            (Action::Erase, _) => Ok(Vec::new()),
            (_, None) => Err("Choose an image first.".into()),
            (Action::Install, Some(i)) => layout::install_plan(i, chip),
            (Action::Update, Some(i)) => layout::update_plan(i, chip),
            (Action::Verify, Some(i)) => layout::verify_plan(i, chip),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RegionState {
    Pending,
    Writing,
    Verifying,
    Done,
    Mismatch,
}

/// Messages from the worker to the UI.
#[derive(Debug)]
pub enum Event {
    Log(String),
    Stage(String),
    Progress {
        index: usize,
        current: usize,
        total: usize,
    },
    RegionState {
        index: usize,
        state: RegionState,
    },
    Finished(Result<String, String>),
}

pub struct Job {
    pub port: PortEntry,
    pub variant: Variant,
    pub action: Action,
    pub regions: Vec<Region>,
}

struct Progress<'a> {
    tx: &'a Sender<Event>,
    regions: &'a [Region],
    index: usize,
    total: usize,
}

impl ProgressCallbacks for Progress<'_> {
    fn init(&mut self, addr: u32, total: usize) {
        self.index = self
            .regions
            .iter()
            .position(|r| r.offset == addr)
            .unwrap_or(0);
        self.total = total.max(1);
        let _ = self.tx.send(Event::RegionState {
            index: self.index,
            state: RegionState::Writing,
        });
        let _ = self.tx.send(Event::Progress {
            index: self.index,
            current: 0,
            total: self.total,
        });
    }

    fn update(&mut self, current: usize) {
        let _ = self.tx.send(Event::Progress {
            index: self.index,
            current,
            total: self.total,
        });
    }

    fn verifying(&mut self) {
        let _ = self.tx.send(Event::RegionState {
            index: self.index,
            state: RegionState::Verifying,
        });
    }

    fn finish(&mut self, _skipped: bool) {
        let _ = self.tx.send(Event::Progress {
            index: self.index,
            current: self.total,
            total: self.total,
        });
        let _ = self.tx.send(Event::RegionState {
            index: self.index,
            state: RegionState::Done,
        });
    }
}

fn say(tx: &Sender<Event>, s: impl Into<String>) {
    let _ = tx.send(Event::Log(s.into()));
}

fn stage(tx: &Sender<Event>, s: impl Into<String>) {
    let _ = tx.send(Event::Stage(s.into()));
}

pub fn md5_of(data: &[u8]) -> u128 {
    u128::from_be_bytes(Md5::digest(data).into())
}

fn connect(job: &Job, tx: &Sender<Event>) -> Result<Flasher, String> {
    let serial = serialport::new(&job.port.name, BAUD)
        .flow_control(FlowControl::None)
        .open_native()
        .map_err(|e| open_error(&job.port.name, &e))?;
    let usb = job.port.usb.clone().unwrap_or(UsbPortInfo {
        vid: 0,
        pid: 0,
        serial_number: None,
        manufacturer: None,
        product: None,
    });
    let before = if job.port.is_native_usb() {
        say(tx, "Reset method: USB Serial/JTAG (native USB)");
        ResetBeforeOperation::UsbReset
    } else {
        say(tx, "Reset method: classic DTR/RTS (USB-UART bridge)");
        ResetBeforeOperation::DefaultReset
    };
    let conn = Connection::new(serial, usb, ResetAfterOperation::HardReset, before, BAUD);
    stage(
        tx,
        format!(
            "Connecting to the bootloader on {} at {BAUD} baud",
            job.port.name
        ),
    );
    Flasher::connect(conn, true, true, false, None, None).map_err(|e| {
        format!(
            "Could not talk to the bootloader: {e}. Check the cable and port. If it keeps failing, hold BOOT, tap RESET, release BOOT, then retry (a v2 also needs the power button held)."
        )
    })
}

pub fn open_error(port: &str, e: &serialport::Error) -> String {
    let base = format!("Cannot open {port}: {e}.");
    let hint = if cfg!(target_os = "linux") {
        " On Linux, add yourself to the dialout group (or uucp on Arch) and log in again. Close any other program using the port."
    } else if cfg!(windows) {
        " Close any other program using the port (Arduino IDE, a serial monitor) and check the driver in Device Manager."
    } else {
        " Close any other program using the port."
    };
    format!("{base}{hint}")
}

/// Run one job. Sends `Finished` exactly once.
pub fn run(job: Job, tx: Sender<Event>) {
    let result = run_inner(&job, &tx);
    match &result {
        Ok(s) => crate::applog::log(&format!("RESULT ok: {s}")),
        Err(s) => crate::applog::log(&format!("RESULT failed: {s}")),
    }
    let _ = tx.send(Event::Finished(result));
}

fn run_inner(job: &Job, tx: &Sender<Event>) -> Result<String, String> {
    let mut flasher = connect(job, tx)?;
    let detected = flasher.chip();
    say(tx, format!("Detected chip: {}", chip_display(detected)));

    if let Err(reason) = variant::check_chip(job.variant, detected) {
        let _ = flasher.connection().reset_after(true, detected);
        return Err(reason);
    }
    let info = flasher
        .device_info()
        .map_err(|e| format!("Could not read chip details: {e}"))?;
    let flash_bytes = info.flash_size.size();
    say(
        tx,
        format!(
            "Flash: {} MB, revision: {}, MAC: {}",
            flash_bytes / (1024 * 1024),
            info.revision
                .map_or("unknown".into(), |(a, b)| format!("v{a}.{b}")),
            info.mac_address.as_deref().unwrap_or("unknown")
        ),
    );
    if let Err(reason) = variant::check_flash_size(job.variant, flash_bytes) {
        let _ = flasher.connection().reset_after(true, detected);
        return Err(reason);
    }

    let outcome = match job.action {
        Action::Erase => {
            stage(
                tx,
                "Erasing the whole flash (up to a minute, do not unplug)",
            );
            flasher
                .erase_flash()
                .map_err(|e| format!("Erase failed: {e}"))?;
            Ok(
                "Erase finished. The unit has no firmware now and will not boot until you Install."
                    .to_string(),
            )
        }
        Action::Install => {
            stage(
                tx,
                "Erasing the whole flash (up to a minute, do not unplug)",
            );
            flasher
                .erase_flash()
                .map_err(|e| format!("Erase failed: {e}"))?;
            write(&mut flasher, &job.regions, tx)?;
            Ok(format!(
                "Install finished: {} region(s) written and verified by MD5. Settings were erased.",
                job.regions.len()
            ))
        }
        Action::Update => {
            write(&mut flasher, &job.regions, tx)?;
            Ok(format!(
                "Update finished: {} region(s) written and verified by MD5. Settings were kept.",
                job.regions.len()
            ))
        }
        Action::Verify => verify(&mut flasher, &job.regions, tx),
    };

    stage(tx, "Restarting the unit");
    if let Err(e) = flasher.connection().reset_after(true, detected) {
        say(
            tx,
            format!("Reset after the action failed: {e}. Press RESET on the unit."),
        );
    }
    outcome
}

fn write(flasher: &mut Flasher, regions: &[Region], tx: &Sender<Event>) -> Result<(), String> {
    for r in regions {
        say(
            tx,
            format!(
                "Plan: {} at 0x{:05x}, {} bytes, MD5 {:032x}",
                r.name,
                r.offset,
                r.len(),
                md5_of(&r.data)
            ),
        );
    }
    stage(tx, "Writing");
    let segments: Vec<Segment<'_>> = regions
        .iter()
        .map(|r| Segment::new(r.offset, &r.data))
        .collect();
    let mut progress = Progress {
        tx,
        regions,
        index: 0,
        total: 1,
    };
    flasher
        .write_bins_to_flash(&segments, &mut progress)
        .map_err(|e| format!("Write failed: {e}. The unit may not boot; run Install again."))
}

fn verify(flasher: &mut Flasher, regions: &[Region], tx: &Sender<Event>) -> Result<String, String> {
    stage(tx, "Comparing the MD5 of each region with the file");
    let mut bad = Vec::new();
    for (index, r) in regions.iter().enumerate() {
        let _ = tx.send(Event::RegionState {
            index,
            state: RegionState::Verifying,
        });
        let want = md5_of(&r.data);
        let got = flasher
            .checksum_md5(r.offset, r.len())
            .map_err(|e| format!("MD5 read of {} failed: {e}", r.name))?;
        let ok = want == got;
        say(
            tx,
            format!(
                "{} at 0x{:05x}: file {:032x}, flash {:032x}, {}",
                r.name,
                r.offset,
                want,
                got,
                if ok { "match" } else { "MISMATCH" }
            ),
        );
        let _ = tx.send(Event::Progress {
            index,
            current: 1,
            total: 1,
        });
        let _ = tx.send(Event::RegionState {
            index,
            state: if ok {
                RegionState::Done
            } else {
                RegionState::Mismatch
            },
        });
        if !ok {
            bad.push(r.name);
        }
    }
    if bad.is_empty() {
        Ok(format!(
            "Verify passed: all {} region(s) match the file.",
            regions.len()
        ))
    } else {
        let otadata_only = bad == ["boot_app0"];
        Err(if otadata_only {
            "Verify: everything matches except boot_app0 (otadata). That changes when the unit switches app slots, for example after a Bluetooth OTA; the firmware itself matches.".to_string()
        } else {
            format!("Verify FAILED: {} differ from the file.", bad.join(", "))
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::layout::tests::fake_factory;

    #[test]
    fn md5_is_big_endian_like_espflash() {
        // espflash compares `digest[..] == flash_md5.to_be_bytes()`.
        assert_eq!(md5_of(b""), 0xd41d8cd98f00b204e9800998ecf8427e);
    }

    #[test]
    fn plans_per_action() {
        let (factory, _) = fake_factory(espflash::target::Chip::Esp32, 0x20000);
        let img = FirmwareImage {
            factory: Some(factory),
            ..Default::default()
        };
        assert_eq!(
            Action::Install.plan(Some(&img), Variant::V3).unwrap().len(),
            1
        );
        assert_eq!(
            Action::Update.plan(Some(&img), Variant::V3).unwrap().len(),
            4
        );
        assert_eq!(
            Action::Verify.plan(Some(&img), Variant::V3).unwrap().len(),
            4
        );
        assert!(Action::Erase.plan(None, Variant::V3).unwrap().is_empty());
        assert!(Action::Update.plan(None, Variant::V3).is_err());
        // An ESP32 image for an ESP32-S3 variant never gets a plan.
        assert!(Action::Install.plan(Some(&img), Variant::V4).is_err());
    }
}
