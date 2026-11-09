//! TrekLink hardware variants and the chip check that gates every flash action.
//!
//! Each entry is taken from the firmware repository:
//! - v1: `variants/esp32/treklink_v1_0/platformio.ini`, env `treklink`, board `treklink-esp32`
//!   (`boards/treklink-esp32.json`, mcu esp32, 4 MB flash).
//! - v2: `variants/esp32s3/treklink_v2_0/platformio.ini`, env `treklink-v2`, board
//!   `esp32-s3-devkitc-1` with `default_8MB.csv`, `ARDUINO_USB_CDC_ON_BOOT=1` (native USB).
//! - v3: `variants/esp32/treklink_v3_tbeam/platformio.ini`, env `treklink-v3-tbeam`, extends
//!   `env:tbeam` (board `ttgo-tbeam`, esp32, `partition-table.csv` for 4 MB).
//! - v4: `variants/esp32s3/treklink_v4_supreme/platformio.ini`, env `treklink-v4-supreme`,
//!   extends `env:tbeam-s3-core` (esp32s3, `default_8MB.csv`).

use espflash::target::Chip;

/// How the board is wired to the computer.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UsbLink {
    /// The ESP32-S3 USB Serial/JTAG peripheral, VID:PID 303a:1001.
    NativeUsb,
    /// A USB-to-UART bridge (CP210x, CH34x) with the classic DTR/RTS auto-reset circuit.
    UsbUart,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Variant {
    V1,
    V2,
    V3,
    V4,
}

impl Variant {
    pub const ALL: [Variant; 4] = [Variant::V1, Variant::V2, Variant::V3, Variant::V4];

    /// The short code used by the release manifest (`variantCode`).
    pub fn code(self) -> &'static str {
        match self {
            Variant::V1 => "v1",
            Variant::V2 => "v2",
            Variant::V3 => "v3",
            Variant::V4 => "v4",
        }
    }

    pub fn title(self) -> &'static str {
        match self {
            Variant::V1 => "v1  TrekLink v1.0 custom PCB (ESP32)",
            Variant::V2 => "v2  TrekLink v2.0 custom PCB (ESP32-S3, power latch)",
            Variant::V3 => "v3  LilyGo T-Beam (ESP32)",
            Variant::V4 => "v4  LilyGo T-Beam Supreme (ESP32-S3)",
        }
    }

    /// The PlatformIO environment that builds this variant.
    pub fn pio_env(self) -> &'static str {
        match self {
            Variant::V1 => "treklink",
            Variant::V2 => "treklink-v2",
            Variant::V3 => "treklink-v3-tbeam",
            Variant::V4 => "treklink-v4-supreme",
        }
    }

    pub fn chip(self) -> Chip {
        match self {
            Variant::V1 | Variant::V3 => Chip::Esp32,
            Variant::V2 | Variant::V4 => Chip::Esp32s3,
        }
    }

    /// The manifest spelling of the chip (`chip` field of `treklink-manifest.json`).
    pub fn chip_name(self) -> &'static str {
        match self.chip() {
            Chip::Esp32 => "esp32",
            _ => "esp32s3",
        }
    }

    pub fn usb_link(self) -> UsbLink {
        match self {
            Variant::V1 | Variant::V3 => UsbLink::UsbUart,
            Variant::V2 | Variant::V4 => UsbLink::NativeUsb,
        }
    }

    /// The smallest flash the variant's partition table fits in, in bytes.
    pub fn min_flash_bytes(self) -> u32 {
        match self {
            Variant::V1 | Variant::V3 => 4 * 1024 * 1024,
            Variant::V2 | Variant::V4 => 8 * 1024 * 1024,
        }
    }

    /// v2 keeps itself powered through a latch that the firmware holds. A reset drops the
    /// latch, so the power button must be held for the whole operation.
    pub fn has_power_latch(self) -> bool {
        matches!(self, Variant::V2)
    }

    /// Whether the release workflow (`.github/workflows/treklink_release.yml`) builds it.
    pub fn in_release(self) -> bool {
        !matches!(self, Variant::V1)
    }
}

/// Human name for a chip as esptool prints it.
pub fn chip_display(chip: Chip) -> &'static str {
    match chip {
        Chip::Esp32 => "ESP32",
        Chip::Esp32s3 => "ESP32-S3",
        Chip::Esp32s2 => "ESP32-S2",
        Chip::Esp32c2 => "ESP32-C2",
        Chip::Esp32c3 => "ESP32-C3",
        Chip::Esp32c5 => "ESP32-C5",
        Chip::Esp32c6 => "ESP32-C6",
        Chip::Esp32h2 => "ESP32-H2",
        Chip::Esp32p4 => "ESP32-P4",
        _ => "an unknown Espressif chip",
    }
}

/// Refuse to flash when the detected chip is not the variant's chip. Returns the reason as
/// a sentence meant for the result line.
pub fn check_chip(variant: Variant, detected: Chip) -> Result<(), String> {
    if detected == variant.chip() {
        return Ok(());
    }
    let hint = match detected {
        Chip::Esp32 => " This looks like a v1 or v3 board.",
        Chip::Esp32s3 => " This looks like a v2 or v4 board.",
        _ => " No TrekLink variant uses this chip.",
    };
    Err(format!(
        "Refused: variant {} needs an {}, but the connected chip is an {}.{} Pick the right variant and try again.",
        variant.code(),
        chip_display(variant.chip()),
        chip_display(detected),
        hint
    ))
}

/// Refuse when the detected flash is too small for the variant's partition table.
pub fn check_flash_size(variant: Variant, detected_bytes: u32) -> Result<(), String> {
    if detected_bytes >= variant.min_flash_bytes() {
        return Ok(());
    }
    Err(format!(
        "Refused: variant {} needs at least {} MB of flash, but the chip reports {} MB.",
        variant.code(),
        variant.min_flash_bytes() / (1024 * 1024),
        detected_bytes / (1024 * 1024)
    ))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn chips_match_the_repository_envs() {
        assert_eq!(Variant::V1.chip(), Chip::Esp32);
        assert_eq!(Variant::V2.chip(), Chip::Esp32s3);
        assert_eq!(Variant::V3.chip(), Chip::Esp32);
        assert_eq!(Variant::V4.chip(), Chip::Esp32s3);
        assert_eq!(Variant::V2.pio_env(), "treklink-v2");
        assert_eq!(Variant::V3.pio_env(), "treklink-v3-tbeam");
        assert_eq!(Variant::V4.pio_env(), "treklink-v4-supreme");
        assert_eq!(Variant::V1.pio_env(), "treklink");
    }

    #[test]
    fn chip_check_accepts_the_right_chip() {
        for v in Variant::ALL {
            assert!(check_chip(v, v.chip()).is_ok());
        }
    }

    #[test]
    fn chip_check_refuses_the_wrong_chip_and_says_why() {
        let err = check_chip(Variant::V2, Chip::Esp32).unwrap_err();
        assert!(err.contains("v2"));
        assert!(err.contains("ESP32-S3"));
        assert!(err.contains("v1 or v3"));

        let err = check_chip(Variant::V3, Chip::Esp32s3).unwrap_err();
        assert!(err.contains("v2 or v4"));

        let err = check_chip(Variant::V1, Chip::Esp32c3).unwrap_err();
        assert!(err.contains("No TrekLink variant"));
    }

    #[test]
    fn flash_size_check() {
        assert!(check_flash_size(Variant::V4, 8 * 1024 * 1024).is_ok());
        assert!(check_flash_size(Variant::V4, 16 * 1024 * 1024).is_ok());
        assert!(check_flash_size(Variant::V4, 4 * 1024 * 1024).is_err());
        assert!(check_flash_size(Variant::V3, 4 * 1024 * 1024).is_ok());
    }

    #[test]
    fn only_v2_has_a_power_latch_and_v1_is_not_released() {
        assert!(Variant::V2.has_power_latch());
        assert!(!Variant::V4.has_power_latch());
        assert!(!Variant::V1.in_release());
        assert!(Variant::V3.in_release());
    }
}
