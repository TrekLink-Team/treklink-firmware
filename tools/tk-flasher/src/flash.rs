//! Install, Update, Verify and Erase, run on a worker thread with espflash as a library.
//!
//! The connection starts at 115200 baud. Native USB stays there: a switch to a higher rate
//! failed once on a v2, and USB Serial/JTAG gains nothing from it. A USB-UART bridge may switch
//! to 460800 after connecting when the user asks for it. ESP32-S3 native USB uses the USB
//! Serial/JTAG reset sequence; USB-UART bridges use the classic DTR/RTS sequence.

use std::sync::mpsc::Sender;

use espflash::connection::{Connection, ResetAfterOperation, ResetBeforeOperation};
use espflash::flasher::Flasher;
use espflash::image_format::Segment;
use espflash::target::{Chip, ProgressCallbacks};
use md5::{Digest, Md5};
use serialport::{FlowControl, UsbPortInfo};

use crate::layout::{self, FirmwareImage, PartitionEntry, Region};
use crate::ports::PortEntry;
use crate::variant::{self, Variant, chip_display};

/// The connection baud, and the only flash baud on native USB.
pub const BAUD: u32 = 115_200;
/// The optional flash baud for USB-UART bridges.
pub const FAST_UART_BAUD: u32 = 460_800;

/// The flash baud to use: the faster rate only on a USB-UART bridge, never on native USB.
pub fn flash_baud(port: &PortEntry, fast_requested: bool) -> u32 {
    if fast_requested && !port.is_native_usb() {
        FAST_UART_BAUD
    } else {
        BAUD
    }
}

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
    /// Flash baud from [`flash_baud`].
    pub baud: u32,
    /// For Update: the image's partition table sector, compared with 0x8000 on the unit.
    pub partition_sector: Option<Vec<u8>>,
    /// For Install: the image's ota_1 partition, named in the result line.
    pub ota1: Option<PartitionEntry>,
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
    let switch = (job.baud > BAUD).then_some(job.baud);
    if let Some(b) = switch {
        say(
            tx,
            format!("Switching to {b} baud after connecting (USB-UART bridge)"),
        );
    }
    Flasher::connect(conn, true, true, false, None, switch).map_err(|e| {
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

    let outcome = act(&mut flasher, job, tx);

    stage(tx, "Restarting the unit");
    if let Err(e) = flasher.connection().reset_after(true, detected) {
        say(
            tx,
            format!("Reset after the action failed: {e}. Press RESET on the unit."),
        );
    }
    outcome
}

/// The action itself. Every exit, refusals included, goes back through the reset in `run_inner`.
fn act(flasher: &mut Flasher, job: &Job, tx: &Sender<Event>) -> Result<String, String> {
    match job.action {
        Action::Erase => {
            erase_all(flasher, tx)?;
            Ok(
                "Erase finished. The unit has no firmware now and will not boot until you Install."
                    .to_string(),
            )
        }
        Action::Install => {
            erase_all(flasher, tx)?;
            write(flasher, &job.regions, tx)?;
            Ok(install_result(
                job.regions.len(),
                job.ota1.as_ref(),
                job.variant.chip(),
            ))
        }
        Action::Update => {
            let Some(sector) = &job.partition_sector else {
                return Err("Internal error: Update has no partition table to compare.".into());
            };
            check_partition_table(flasher, sector, tx)?;
            write(flasher, &job.regions, tx)?;
            Ok(
                "Update finished: application and boot_app0 written and verified by MD5. Settings were kept."
                    .to_string(),
            )
        }
        Action::Verify => verify(flasher, &job.regions, tx),
    }
}

fn erase_all(flasher: &mut Flasher, tx: &Sender<Event>) -> Result<(), String> {
    stage(
        tx,
        "Erasing the whole flash (up to a minute, do not unplug)",
    );
    flasher
        .erase_flash()
        .map_err(|e| format!("Erase failed: {e}"))
}

/// The OTA loader file `bin/device-install.sh` writes to ota_1 (`mt-${MCU}-ota.bin`).
pub fn ota_loader_name(chip: Chip) -> &'static str {
    match chip {
        Chip::Esp32 => "mt-esp32-ota.bin",
        _ => "mt-esp32s3-ota.bin",
    }
}

/// The Install result line. It states that ota_1 stays empty: this tool does not write the
/// Meshtastic OTA loader that `bin/device-install.sh` writes there.
pub fn install_result(regions: usize, ota1: Option<&PartitionEntry>, chip: Chip) -> String {
    let slot = ota1.map_or("ota_1".to_string(), |p| {
        format!("ota_1 ({} at 0x{:x})", p.label, p.offset)
    });
    format!(
        "Install finished: {regions} region(s) written and verified by MD5. Settings were erased. {slot} is left empty: the OTA loader {} is not written, so over-the-air firmware updates will not work until it is. USB Update is not affected.",
        ota_loader_name(chip)
    )
}

/// Update keeps the partition table on the unit. Refuse when it is not the image's table,
/// because the application and boot_app0 would then be written into a different layout.
fn check_partition_table(
    flasher: &mut Flasher,
    sector: &[u8],
    tx: &Sender<Event>,
) -> Result<(), String> {
    stage(
        tx,
        "Comparing the partition table on the unit with the image",
    );
    let want = md5_of(sector);
    let got = flasher
        .checksum_md5(layout::PARTITIONS_OFFSET, sector.len() as u32)
        .map_err(|e| format!("MD5 read of the partition table failed: {e}"))?;
    say(
        tx,
        format!(
            "partitions at 0x08000: image {want:032x}, unit {got:032x}, {}",
            if want == got { "match" } else { "MISMATCH" }
        ),
    );
    partition_gate(want, got)
}

/// Pure decision for [`check_partition_table`].
pub fn partition_gate(want: u128, got: u128) -> Result<(), String> {
    if want == got {
        Ok(())
    } else {
        Err("Refused: the partition table on the unit differs from the image's. Update keeps the table on the unit, so it cannot change the layout. Use Install instead (it erases settings). Nothing was written.".into())
    }
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
    verify_result(regions.len(), &bad)
}

/// The Verify result line. Only the application and the partition table fail Verify; see
/// [`layout::verify_required`].
pub fn verify_result(total: usize, bad: &[&str]) -> Result<String, String> {
    let required: Vec<&str> = bad
        .iter()
        .copied()
        .filter(|n| layout::verify_required(n))
        .collect();
    let advisory: Vec<String> = bad
        .iter()
        .filter(|n| !layout::verify_required(n))
        .map(|n| match *n {
            "bootloader" => "bootloader (only Install writes it; after an Update it can be an older build)".to_string(),
            "boot_app0" => "boot_app0 (otadata changes when the unit switches app slots, for example after an OTA update)".to_string(),
            other => other.to_string(),
        })
        .collect();
    if !required.is_empty() {
        return Err(format!(
            "Verify FAILED: {} differ from the file.",
            required.join(", ")
        ));
    }
    if advisory.is_empty() {
        Ok(format!(
            "Verify passed: all {total} region(s) match the file."
        ))
    } else {
        Ok(format!(
            "Verify passed: the application and the partition table match. Also differ, which does not fail Verify: {}.",
            advisory.join("; ")
        ))
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
            2
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

    #[test]
    fn partition_gate_refuses_a_different_table() {
        assert!(partition_gate(7, 7).is_ok());
        let err = partition_gate(7, 8).unwrap_err();
        assert!(err.contains("Use Install"));
        assert!(err.contains("Nothing was written"));
    }

    #[test]
    fn verify_result_fails_only_on_required_regions() {
        assert!(verify_result(4, &[]).unwrap().contains("all 4"));
        let ok = verify_result(4, &["bootloader", "boot_app0"]).unwrap();
        assert!(ok.contains("bootloader") && ok.contains("boot_app0"));
        assert!(
            verify_result(4, &["application"])
                .unwrap_err()
                .contains("FAILED")
        );
        assert!(
            verify_result(4, &["partitions", "boot_app0"])
                .unwrap_err()
                .contains("partitions")
        );
    }

    #[test]
    fn install_result_states_the_empty_ota1() {
        let ota1 = PartitionEntry {
            kind: 0,
            subtype: 0x11,
            offset: 0x260000,
            size: 0xA0000,
            label: "flashApp".into(),
        };
        let s = install_result(1, Some(&ota1), Chip::Esp32);
        assert!(s.contains("flashApp at 0x260000"));
        assert!(s.contains("mt-esp32-ota.bin"));
        assert!(install_result(1, None, Chip::Esp32s3).contains("mt-esp32s3-ota.bin"));
    }

    #[test]
    fn fast_baud_only_on_usb_uart() {
        use serialport::UsbPortInfo;
        let port = |vid, pid| PortEntry {
            name: "p".into(),
            usb: Some(UsbPortInfo {
                vid,
                pid,
                serial_number: None,
                manufacturer: None,
                product: None,
            }),
        };
        assert_eq!(flash_baud(&port(0x10c4, 0xea60), true), FAST_UART_BAUD);
        assert_eq!(flash_baud(&port(0x10c4, 0xea60), false), BAUD);
        assert_eq!(flash_baud(&port(0x303a, 0x1001), true), BAUD);
    }
}
