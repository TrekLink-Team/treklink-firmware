//! Flash offset tables, image checks and the region plans for each action.
//!
//! Offsets follow the Arduino-ESP32 layout that PlatformIO uses for every TrekLink env:
//! bootloader at the chip's boot address (0x1000 on ESP32, 0x0 on ESP32-S3), partition table
//! at 0x8000, `boot_app0` (the otadata partition) at 0xe000 and the application at 0x10000.
//! The `.factory.bin` image that `bin/build-esp32.sh` copies is all of these merged at 0x0, so
//! on an ESP32 its first 0x1000 bytes are padding.
//!
//! Update writes only the application at 0x10000 and `boot_app0`, like `bin/device-update.sh`
//! writes only the application. It never touches the bootloader, the partition table, NVS
//! (0x9000) or the LittleFS partition, so settings survive. Because it keeps the partition table
//! on the unit, the caller first compares that table with the image's and refuses on a mismatch.

use std::fmt;

use espflash::target::Chip;

pub const FACTORY_OFFSET: u32 = 0x0;
pub const PARTITIONS_OFFSET: u32 = 0x8000;
pub const PARTITIONS_MAX_LEN: usize = 0x1000;
pub const NVS_FIRST_OFFSET: u32 = 0x9000;
pub const BOOT_APP0_OFFSET: u32 = 0xe000;
pub const BOOT_APP0_LEN: usize = 0x2000;
pub const APP_OFFSET: u32 = 0x10000;
pub const SECTOR: usize = 0x1000;

const ESP_IMAGE_MAGIC: u8 = 0xE9;
const PARTITION_MAGIC: [u8; 2] = [0xAA, 0x50];

/// The address the ROM loads the second stage bootloader from.
pub fn bootloader_offset(chip: Chip) -> u32 {
    match chip {
        Chip::Esp32 | Chip::Esp32s2 => 0x1000,
        _ => 0x0,
    }
}

/// The `chip_id` field of the ESP image extended header (offset 12, little endian).
pub fn image_chip_id(chip: Chip) -> u16 {
    match chip {
        Chip::Esp32 => 0x0000,
        Chip::Esp32s2 => 0x0002,
        Chip::Esp32c3 => 0x0005,
        Chip::Esp32s3 => 0x0009,
        Chip::Esp32c2 => 0x000C,
        Chip::Esp32c6 => 0x000D,
        Chip::Esp32h2 => 0x0010,
        Chip::Esp32p4 => 0x0012,
        _ => 0xFFFF,
    }
}

fn chip_from_image_id(id: u16) -> &'static str {
    match id {
        0x0000 => "ESP32",
        0x0002 => "ESP32-S2",
        0x0005 => "ESP32-C3",
        0x0009 => "ESP32-S3",
        0x000C => "ESP32-C2",
        0x000D => "ESP32-C6",
        0x0010 => "ESP32-H2",
        0x0012 => "ESP32-P4",
        _ => "an unknown chip",
    }
}

/// The default arduino-esp32 `boot_app0.bin`: otadata whose first entry selects `ota_0`
/// (sequence 1, CRC 0x4743989a) and whose second entry is unused (sequence 0). This is the
/// file PlatformIO writes at 0xe000 and that the factory image contains.
pub fn default_boot_app0() -> Vec<u8> {
    let mut data = vec![0xFF; BOOT_APP0_LEN];
    data[0..4].copy_from_slice(&1u32.to_le_bytes());
    data[28..32].copy_from_slice(&0x4743_989Au32.to_le_bytes());
    data[0x1000..0x1004].copy_from_slice(&0u32.to_le_bytes());
    data
}

/// One contiguous write or check.
#[derive(Clone, PartialEq, Eq)]
pub struct Region {
    pub name: &'static str,
    pub offset: u32,
    pub data: Vec<u8>,
}

impl fmt::Debug for Region {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{} @ 0x{:x} ({} bytes)",
            self.name,
            self.offset,
            self.data.len()
        )
    }
}

impl Region {
    fn new(name: &'static str, offset: u32, mut data: Vec<u8>) -> Self {
        // The ROM and the stub write in 4 byte words; pad like esptool does.
        while !data.len().is_multiple_of(4) {
            data.push(0xFF);
        }
        Region { name, offset, data }
    }

    pub fn len(&self) -> u32 {
        self.data.len() as u32
    }
}

/// The firmware the user picked, from a local file or from a release.
#[derive(Debug, Clone, Default)]
pub struct FirmwareImage {
    /// Where it came from, for the header line and the log.
    pub label: String,
    pub factory: Option<Vec<u8>>,
    pub app: Option<Vec<u8>>,
    pub bootloader: Option<Vec<u8>>,
    pub partitions: Option<Vec<u8>>,
    pub boot_app0: Option<Vec<u8>>,
    /// Facts the user should see before flashing (for example a generated boot_app0).
    pub notes: Vec<String>,
}

impl FirmwareImage {
    pub fn has_parts(&self) -> bool {
        self.app.is_some() && self.bootloader.is_some() && self.partitions.is_some()
    }
}

/// Drop trailing erased bytes, keeping whole 4 KiB sectors so a written sector never keeps
/// stale data from the previous image.
pub fn trim_erased(data: &[u8]) -> &[u8] {
    let used = data.iter().rposition(|&b| b != 0xFF).map_or(0, |i| i + 1);
    let rounded = used.div_ceil(SECTOR).max(1) * SECTOR;
    &data[..rounded.min(data.len())]
}

fn check_esp_image(name: &str, data: &[u8], chip: Chip) -> Result<(), String> {
    if data.len() < 24 || data[0] != ESP_IMAGE_MAGIC {
        return Err(format!(
            "The {name} is not an ESP application image (no 0xE9 header)."
        ));
    }
    let id = u16::from_le_bytes([data[12], data[13]]);
    if id != image_chip_id(chip) {
        return Err(format!(
            "The {name} was built for {}, not for this variant's chip.",
            chip_from_image_id(id)
        ));
    }
    Ok(())
}

/// A merged factory image picked where an application image belongs. An ESP32-S3 factory
/// image starts with the bootloader header, so the header check alone would let it through.
fn check_not_factory(app: &[u8]) -> Result<(), String> {
    let p = PARTITIONS_OFFSET as usize;
    if app.len() > APP_OFFSET as usize && check_partitions(&app[p..p + PARTITIONS_MAX_LEN]).is_ok()
    {
        return Err("This is a merged factory image, not an application image. Pick it as a local factory image instead.".into());
    }
    Ok(())
}

/// One entry of an ESP partition table.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PartitionEntry {
    pub kind: u8,
    pub subtype: u8,
    pub offset: u32,
    pub size: u32,
    pub label: String,
}

/// Parse a binary partition table (32 byte entries starting with 0xAA 0x50).
pub fn parse_partitions(data: &[u8]) -> Result<Vec<PartitionEntry>, String> {
    if data.len() < 32 || data[0..2] != PARTITION_MAGIC {
        return Err("The partition table does not start with 0xAA 0x50.".into());
    }
    let mut out = Vec::new();
    for chunk in data.chunks_exact(32) {
        if chunk[0..2] != PARTITION_MAGIC {
            break;
        }
        let label_bytes = &chunk[12..28];
        let end = label_bytes.iter().position(|&b| b == 0).unwrap_or(16);
        out.push(PartitionEntry {
            kind: chunk[2],
            subtype: chunk[3],
            offset: u32::from_le_bytes(chunk[4..8].try_into().unwrap()),
            size: u32::from_le_bytes(chunk[8..12].try_into().unwrap()),
            label: String::from_utf8_lossy(&label_bytes[..end]).into_owned(),
        });
    }
    if out.is_empty() {
        return Err("The partition table has no entries.".into());
    }
    Ok(out)
}

fn check_partitions(data: &[u8]) -> Result<Vec<PartitionEntry>, String> {
    if data.len() > PARTITIONS_MAX_LEN {
        return Err(format!(
            "The partition table file is {} bytes; it must fit in 0x8000..0x9000.",
            data.len()
        ));
    }
    let entries = parse_partitions(data)?;
    if let Some(p) = entries.iter().find(|p| p.offset < NVS_FIRST_OFFSET) {
        return Err(format!(
            "Partition '{}' starts at 0x{:x}, inside the bootloader area. This is not a TrekLink layout.",
            p.label, p.offset
        ));
    }
    if !entries
        .iter()
        .any(|p| p.kind == 0 && p.offset == APP_OFFSET)
    {
        return Err("The partition table has no application partition at 0x10000.".into());
    }
    Ok(entries)
}

fn check_app_fits(app: &[u8], entries: &[PartitionEntry]) -> Result<(), String> {
    let slot = entries
        .iter()
        .find(|p| p.kind == 0 && p.offset == APP_OFFSET)
        .map(|p| p.size)
        .unwrap_or(0);
    if app.len() as u64 > slot as u64 {
        return Err(format!(
            "The application is {} bytes but its slot at 0x10000 holds only {} bytes.",
            app.len(),
            slot
        ));
    }
    Ok(())
}

/// Check a merged factory image and return its parts, sliced at the standard offsets.
pub struct FactoryParts<'a> {
    pub bootloader: &'a [u8],
    pub partitions: &'a [u8],
    pub boot_app0: &'a [u8],
    pub app: &'a [u8],
}

pub fn split_factory(factory: &[u8], chip: Chip) -> Result<FactoryParts<'_>, String> {
    let app_start = APP_OFFSET as usize;
    if factory.len() <= app_start + 24 {
        return Err(format!(
            "The factory image is only {} bytes; a merged image is larger than 0x10000.",
            factory.len()
        ));
    }
    let boot = bootloader_offset(chip) as usize;
    let bootloader = trim_erased(&factory[boot..PARTITIONS_OFFSET as usize]);
    check_esp_image("factory image bootloader", bootloader, chip)?;
    let partitions =
        &factory[PARTITIONS_OFFSET as usize..PARTITIONS_OFFSET as usize + PARTITIONS_MAX_LEN];
    let entries = check_partitions(partitions)?;
    let boot_app0 = &factory[BOOT_APP0_OFFSET as usize..BOOT_APP0_OFFSET as usize + BOOT_APP0_LEN];
    let app = trim_erased(&factory[app_start..]);
    check_esp_image("factory image application", app, chip)?;
    check_app_fits(app, &entries)?;
    Ok(FactoryParts {
        bootloader,
        partitions,
        boot_app0,
        app,
    })
}

/// The four regions of an image at their standard offsets: bootloader, partitions, boot_app0
/// and application, checked against the chip and against each other.
pub fn image_regions(img: &FirmwareImage, chip: Chip) -> Result<Vec<Region>, String> {
    let boot_off = bootloader_offset(chip);
    if img.has_parts() {
        let bootloader = img.bootloader.as_ref().unwrap();
        let partitions = img.partitions.as_ref().unwrap();
        let app = img.app.as_ref().unwrap();
        check_esp_image("bootloader", bootloader, chip)?;
        if bootloader.len() > (PARTITIONS_OFFSET - boot_off) as usize {
            return Err(
                "The bootloader is too large; it would overwrite the partition table.".into(),
            );
        }
        let entries = check_partitions(partitions)?;
        check_esp_image("application", app, chip)?;
        check_not_factory(app)?;
        check_app_fits(app, &entries)?;
        let boot_app0 = match &img.boot_app0 {
            Some(b) if b.len() != BOOT_APP0_LEN => {
                return Err(format!(
                    "boot_app0 must be {BOOT_APP0_LEN} bytes, this one is {}.",
                    b.len()
                ));
            }
            Some(b) => b.clone(),
            None => default_boot_app0(),
        };
        return Ok(vec![
            Region::new("bootloader", boot_off, bootloader.clone()),
            Region::new("partitions", PARTITIONS_OFFSET, partitions.clone()),
            Region::new("boot_app0", BOOT_APP0_OFFSET, boot_app0),
            Region::new("application", APP_OFFSET, app.clone()),
        ]);
    }
    let Some(factory) = &img.factory else {
        return Err(
            "Update needs a factory image, or the application with its bootloader and partitions."
                .into(),
        );
    };
    let parts = split_factory(factory, chip)?;
    let app = match &img.app {
        Some(app) => {
            check_esp_image("application", app, chip)?;
            check_not_factory(app)?;
            let n = app.len();
            let in_factory = &factory[APP_OFFSET as usize..];
            if in_factory.len() < n || in_factory[..n] != app[..] {
                return Err("The application image does not match the application inside the factory image. They come from different builds.".into());
            }
            app.clone()
        }
        None => parts.app.to_vec(),
    };
    Ok(vec![
        Region::new("bootloader", boot_off, parts.bootloader.to_vec()),
        Region::new("partitions", PARTITIONS_OFFSET, parts.partitions.to_vec()),
        Region::new("boot_app0", BOOT_APP0_OFFSET, parts.boot_app0.to_vec()),
        Region::new("application", APP_OFFSET, app),
    ])
}

fn take(regions: &[Region], name: &str) -> Region {
    regions
        .iter()
        .find(|r| r.name == name)
        .cloned()
        .unwrap_or_else(|| unreachable!("image_regions always returns {name}"))
}

/// The regions Update writes: the application first, then `boot_app0`, so the boot selection
/// points at slot 0 only once the application in it is complete.
pub fn update_plan(img: &FirmwareImage, chip: Chip) -> Result<Vec<Region>, String> {
    let regions = image_regions(img, chip)?;
    Ok(vec![
        take(&regions, "application"),
        take(&regions, "boot_app0"),
    ])
}

/// The image's partition table as it sits in flash after a write: one 4 KiB sector, padded
/// with erased bytes. Update compares the MD5 of 0x8000..0x9000 on the unit with this.
pub fn partition_sector(img: &FirmwareImage, chip: Chip) -> Result<Vec<u8>, String> {
    let mut table = take(&image_regions(img, chip)?, "partitions").data;
    table.resize(PARTITIONS_MAX_LEN, 0xFF);
    Ok(table)
}

/// The `ota_1` partition (`flashApp` in the 4 MB table, `app1` in the 8 MB table), which
/// `bin/device-install.sh` fills with the Meshtastic OTA loader.
pub fn ota1_partition(img: &FirmwareImage, chip: Chip) -> Option<PartitionEntry> {
    let table = partition_sector(img, chip).ok()?;
    parse_partitions(&table)
        .ok()?
        .into_iter()
        .find(|p| p.kind == 0 && p.subtype == 0x11)
}

/// The regions Install writes after a full erase: the factory image at 0x0, or the four
/// parts when only parts were given.
pub fn install_plan(img: &FirmwareImage, chip: Chip) -> Result<Vec<Region>, String> {
    if let Some(factory) = &img.factory {
        split_factory(factory, chip)?;
        let data = trim_erased(factory).to_vec();
        return Ok(vec![Region::new("factory image", FACTORY_OFFSET, data)]);
    }
    image_regions(img, chip)
}

/// Verify checks the four regions that do not change while the unit runs. The factory image
/// as a whole cannot be compared after first boot because NVS and LittleFS fill in.
pub fn verify_plan(img: &FirmwareImage, chip: Chip) -> Result<Vec<Region>, String> {
    image_regions(img, chip)
}

/// Whether a Verify mismatch in this region fails the check. The bootloader is only written by
/// Install, so after an Update it may be an older build; otadata changes when the unit switches
/// app slots. Both are reported but do not fail Verify.
pub fn verify_required(name: &str) -> bool {
    matches!(name, "application" | "partitions" | "factory image")
}

#[cfg(test)]
pub mod tests {
    use super::*;
    use sha2::Digest;

    pub fn fake_esp_image(chip: Chip, len: usize, fill: u8) -> Vec<u8> {
        let mut d = vec![fill; len];
        d[0] = ESP_IMAGE_MAGIC;
        d[1] = 3;
        d[12..14].copy_from_slice(&image_chip_id(chip).to_le_bytes());
        // A non-erased last byte so trimming keeps the full length in sectors.
        let last = len - 1;
        d[last] = 0x5A;
        d
    }

    fn entry(kind: u8, subtype: u8, offset: u32, size: u32, label: &str) -> [u8; 32] {
        let mut e = [0u8; 32];
        e[0..2].copy_from_slice(&PARTITION_MAGIC);
        e[2] = kind;
        e[3] = subtype;
        e[4..8].copy_from_slice(&offset.to_le_bytes());
        e[8..12].copy_from_slice(&size.to_le_bytes());
        e[12..12 + label.len()].copy_from_slice(label.as_bytes());
        e
    }

    /// The repository's `partition-table.csv` (4 MB) as a binary table.
    pub fn table_4mb() -> Vec<u8> {
        let mut t = Vec::new();
        t.extend(entry(1, 2, 0x9000, 0x5000, "nvs"));
        t.extend(entry(1, 0, 0xe000, 0x2000, "otadata"));
        t.extend(entry(0, 0x10, 0x10000, 0x250000, "app"));
        t.extend(entry(0, 0x11, 0x260000, 0xA0000, "flashApp"));
        t.extend(entry(1, 0x82, 0x300000, 0x100000, "spiffs"));
        t.resize(0xC00, 0xFF);
        t
    }

    pub fn fake_factory(chip: Chip, app_len: usize) -> (Vec<u8>, Vec<u8>) {
        let boot = bootloader_offset(chip) as usize;
        let mut f = vec![0xFF; APP_OFFSET as usize];
        let bl = fake_esp_image(chip, 0x5000, 0x11);
        f[boot..boot + bl.len()].copy_from_slice(&bl);
        let t = table_4mb();
        f[0x8000..0x8000 + t.len()].copy_from_slice(&t);
        f[0xe000..0x10000].copy_from_slice(&default_boot_app0());
        let app = fake_esp_image(chip, app_len, 0x22);
        f.extend_from_slice(&app);
        (f, app)
    }

    #[test]
    fn offset_table_matches_espressif() {
        assert_eq!(bootloader_offset(Chip::Esp32), 0x1000);
        assert_eq!(bootloader_offset(Chip::Esp32s3), 0x0);
        assert_eq!(bootloader_offset(Chip::Esp32), Chip::Esp32.boot_address());
        assert_eq!(
            bootloader_offset(Chip::Esp32s3),
            Chip::Esp32s3.boot_address()
        );
        assert_eq!(PARTITIONS_OFFSET, 0x8000);
        assert_eq!(BOOT_APP0_OFFSET, 0xe000);
        assert_eq!(APP_OFFSET, 0x10000);
        assert_eq!(image_chip_id(Chip::Esp32), Chip::Esp32.id());
        assert_eq!(image_chip_id(Chip::Esp32s3), Chip::Esp32s3.id());
    }

    #[test]
    fn default_boot_app0_matches_arduino_esp32() {
        // sha256 of tools/partitions/boot_app0.bin in espressif/arduino-esp32.
        let digest = hex::encode(sha2::Sha256::digest(default_boot_app0()));
        assert_eq!(
            digest,
            "f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0"
        );
    }

    #[test]
    fn trim_keeps_whole_sectors() {
        let mut d = vec![0xFF; 0x7000];
        d[0x1234] = 0;
        assert_eq!(trim_erased(&d).len(), 0x2000);
        assert_eq!(trim_erased(&vec![0xFF; 0x3000]).len(), 0x1000);
        assert_eq!(trim_erased(&[1, 2, 3]).len(), 3);
    }

    #[test]
    fn image_regions_from_factory_use_standard_offsets() {
        for chip in [Chip::Esp32, Chip::Esp32s3] {
            let (factory, app) = fake_factory(chip, 0x30000);
            let img = FirmwareImage {
                factory: Some(factory),
                ..Default::default()
            };
            let regions = image_regions(&img, chip).unwrap();
            let offsets: Vec<u32> = regions.iter().map(|r| r.offset).collect();
            assert_eq!(
                offsets,
                vec![bootloader_offset(chip), 0x8000, 0xe000, 0x10000]
            );
            assert_eq!(regions[3].data, app);
            assert_eq!(regions[2].data, default_boot_app0());
            assert_eq!(regions[1].data.len(), 0x1000);
            assert_eq!(regions[0].data.len() % SECTOR, 0);
            assert!(regions.iter().all(|r| r.data.len() % 4 == 0));
            assert_eq!(verify_plan(&img, chip).unwrap(), regions);
        }
    }

    #[test]
    fn update_writes_only_the_application_then_boot_app0() {
        for chip in [Chip::Esp32, Chip::Esp32s3] {
            let (factory, app) = fake_factory(chip, 0x30000);
            let img = FirmwareImage {
                factory: Some(factory),
                ..Default::default()
            };
            let plan = update_plan(&img, chip).unwrap();
            let names: Vec<&str> = plan.iter().map(|r| r.name).collect();
            assert_eq!(names, vec!["application", "boot_app0"]);
            assert_eq!(plan[0].offset, 0x10000);
            assert_eq!(plan[0].data, app);
            assert_eq!(plan[1].offset, 0xe000);
            assert_eq!(plan[1].data, default_boot_app0());
        }
    }

    #[test]
    fn partition_sector_is_padded_to_one_sector() {
        let chip = Chip::Esp32;
        let (factory, _) = fake_factory(chip, 0x20000);
        let from_factory = partition_sector(
            &FirmwareImage {
                factory: Some(factory),
                ..Default::default()
            },
            chip,
        )
        .unwrap();
        // A PlatformIO partitions.bin is 0xC00 bytes; in flash the rest of the sector is erased.
        let from_parts = partition_sector(
            &FirmwareImage {
                app: Some(fake_esp_image(chip, 0x20000, 0x33)),
                bootloader: Some(fake_esp_image(chip, 0x4000, 0x44)),
                partitions: Some(table_4mb()),
                ..Default::default()
            },
            chip,
        )
        .unwrap();
        assert_eq!(from_factory.len(), 0x1000);
        assert_eq!(from_factory, from_parts);
        assert!(from_parts[0xC00..].iter().all(|&b| b == 0xFF));
    }

    #[test]
    fn ota1_is_read_from_the_partition_table() {
        let chip = Chip::Esp32;
        let (factory, _) = fake_factory(chip, 0x20000);
        let img = FirmwareImage {
            factory: Some(factory),
            ..Default::default()
        };
        let ota1 = ota1_partition(&img, chip).unwrap();
        assert_eq!(ota1.label, "flashApp");
        assert_eq!(ota1.offset, 0x260000);
        assert!(ota1_partition(&FirmwareImage::default(), chip).is_none());
    }

    #[test]
    fn verify_fails_only_on_application_or_partitions() {
        assert!(verify_required("application"));
        assert!(verify_required("partitions"));
        assert!(!verify_required("bootloader"));
        assert!(!verify_required("boot_app0"));
    }

    #[test]
    fn update_plan_checks_app_against_factory() {
        let chip = Chip::Esp32s3;
        let (factory, app) = fake_factory(chip, 0x20000);
        let img = FirmwareImage {
            factory: Some(factory.clone()),
            app: Some(app.clone()),
            ..Default::default()
        };
        assert_eq!(update_plan(&img, chip).unwrap()[0].data, app);

        let mut other = app.clone();
        other[100] ^= 0xFF;
        let img = FirmwareImage {
            factory: Some(factory),
            app: Some(other),
            ..Default::default()
        };
        assert!(
            update_plan(&img, chip)
                .unwrap_err()
                .contains("different builds")
        );
    }

    #[test]
    fn wrong_chip_images_are_refused() {
        let (factory, _) = fake_factory(Chip::Esp32s3, 0x20000);
        let img = FirmwareImage {
            factory: Some(factory),
            ..Default::default()
        };
        // An S3 image has its bootloader at 0x0; read as ESP32 the 0x1000 slot is not a header.
        assert!(install_plan(&img, Chip::Esp32).is_err());
        let (factory, _) = fake_factory(Chip::Esp32, 0x20000);
        let img = FirmwareImage {
            factory: Some(factory),
            ..Default::default()
        };
        assert!(install_plan(&img, Chip::Esp32s3).is_err());
    }

    #[test]
    fn install_plan_writes_factory_at_zero() {
        let (factory, _) = fake_factory(Chip::Esp32, 0x20000);
        let img = FirmwareImage {
            factory: Some(factory.clone()),
            ..Default::default()
        };
        let plan = install_plan(&img, Chip::Esp32).unwrap();
        assert_eq!(plan.len(), 1);
        assert_eq!(plan[0].offset, 0);
        assert_eq!(plan[0].data, factory);
    }

    #[test]
    fn parts_plan_generates_boot_app0_and_pads() {
        let chip = Chip::Esp32;
        let mut app = fake_esp_image(chip, 0x12345, 0x33);
        app.truncate(0x12345);
        let img = FirmwareImage {
            app: Some(app),
            bootloader: Some(fake_esp_image(chip, 0x4000, 0x44)),
            partitions: Some(table_4mb()),
            ..Default::default()
        };
        let regions = image_regions(&img, chip).unwrap();
        assert_eq!(regions[0].offset, 0x1000);
        assert_eq!(regions[2].data, default_boot_app0());
        assert_eq!(regions[3].data.len(), 0x12348);
        assert_eq!(&regions[3].data[0x12345..], &[0xFF, 0xFF, 0xFF]);
        // Install with parts writes the same four regions.
        assert_eq!(install_plan(&img, chip).unwrap(), regions);
    }

    #[test]
    fn factory_image_in_the_application_slot_is_refused() {
        let chip = Chip::Esp32s3;
        let (factory, _) = fake_factory(chip, 0x20000);
        let img = FirmwareImage {
            app: Some(factory),
            bootloader: Some(fake_esp_image(chip, 0x4000, 0x44)),
            partitions: Some(table_4mb()),
            ..Default::default()
        };
        assert!(
            update_plan(&img, chip)
                .unwrap_err()
                .contains("merged factory image")
        );
    }

    #[test]
    fn oversized_app_is_refused() {
        let chip = Chip::Esp32;
        let img = FirmwareImage {
            app: Some(fake_esp_image(chip, 0x260000, 0x33)),
            bootloader: Some(fake_esp_image(chip, 0x4000, 0x44)),
            partitions: Some(table_4mb()),
            ..Default::default()
        };
        assert!(update_plan(&img, chip).unwrap_err().contains("slot"));
    }

    #[test]
    fn partition_table_parses() {
        let entries = parse_partitions(&table_4mb()).unwrap();
        assert_eq!(entries.len(), 5);
        assert_eq!(entries[2].label, "app");
        assert_eq!(entries[2].size, 0x250000);
        assert!(parse_partitions(&[0u8; 64]).is_err());
    }
}
