//! Serial port listing with USB VID:PID and a friendly name.

use serialport::{SerialPortType, UsbPortInfo};

use crate::variant::UsbLink;

#[derive(Debug, Clone)]
pub struct PortEntry {
    pub name: String,
    pub usb: Option<UsbPortInfo>,
}

/// What a USB serial device most likely is, from its VID:PID.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Classified {
    pub friendly: &'static str,
    pub link: Option<UsbLink>,
    /// Which TrekLink variants normally show up with this ID.
    pub hint: &'static str,
}

pub const ESPRESSIF_VID: u16 = 0x303a;
pub const USB_SERIAL_JTAG_PID: u16 = 0x1001;

pub fn classify(vid: u16, pid: u16) -> Classified {
    match (vid, pid) {
        (ESPRESSIF_VID, USB_SERIAL_JTAG_PID) => Classified {
            friendly: "ESP32-S3 native USB (USB Serial/JTAG)",
            link: Some(UsbLink::NativeUsb),
            hint: "v2 or v4",
        },
        (ESPRESSIF_VID, _) => Classified {
            friendly: "Espressif native USB (TinyUSB CDC)",
            link: Some(UsbLink::NativeUsb),
            hint: "v2 or v4",
        },
        (0x10c4, 0xea60) => Classified {
            friendly: "Silicon Labs CP210x USB-UART",
            link: Some(UsbLink::UsbUart),
            hint: "v1 or v3",
        },
        (0x1a86, 0x55d4) => Classified {
            friendly: "WCH CH9102 (CH34x family) USB-UART",
            link: Some(UsbLink::UsbUart),
            hint: "v1 or v3",
        },
        (0x1a86, 0x55d3) => Classified {
            friendly: "WCH CH343 (CH34x family) USB-UART",
            link: Some(UsbLink::UsbUart),
            hint: "v1 or v3",
        },
        (0x1a86, 0x7523) => Classified {
            friendly: "WCH CH340 USB-UART",
            link: Some(UsbLink::UsbUart),
            hint: "v1 or v3",
        },
        _ => Classified {
            friendly: "USB serial device (not a known TrekLink bridge)",
            link: None,
            hint: "unknown",
        },
    }
}

impl PortEntry {
    pub fn vid_pid(&self) -> Option<(u16, u16)> {
        self.usb.as_ref().map(|u| (u.vid, u.pid))
    }

    pub fn classified(&self) -> Option<Classified> {
        self.vid_pid().map(|(v, p)| classify(v, p))
    }

    pub fn is_native_usb(&self) -> bool {
        matches!(self.vid_pid(), Some((ESPRESSIF_VID, _)))
    }

    pub fn describe(&self) -> String {
        match &self.usb {
            Some(u) => {
                let c = classify(u.vid, u.pid);
                let product = u.product.as_deref().unwrap_or("");
                let mut s = format!("{:04x}:{:04x}  {}  [{}]", u.vid, u.pid, c.friendly, c.hint);
                if !product.is_empty() {
                    s.push_str(&format!("  \"{product}\""));
                }
                s
            }
            None => "no USB information (built-in or Bluetooth port)".to_string(),
        }
    }
}

/// List serial ports, USB ones first. On macOS each device appears as both `/dev/tty.*`
/// and `/dev/cu.*`; only the `cu` node is kept because opening `tty` waits for carrier.
pub fn list() -> Result<Vec<PortEntry>, String> {
    let ports =
        serialport::available_ports().map_err(|e| format!("Could not list serial ports: {e}"))?;
    let mut out: Vec<PortEntry> = ports
        .into_iter()
        .filter(|p| !(cfg!(target_os = "macos") && p.port_name.starts_with("/dev/tty.")))
        .map(|p| PortEntry {
            usb: match p.port_type {
                SerialPortType::UsbPort(info) => Some(info),
                _ => None,
            },
            name: p.port_name,
        })
        .collect();
    out.sort_by_key(|p| (p.usb.is_none(), p.name.clone()));
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn known_bridges_are_classified() {
        let c = classify(0x303a, 0x1001);
        assert_eq!(c.link, Some(UsbLink::NativeUsb));
        assert_eq!(c.hint, "v2 or v4");
        for (v, p) in [(0x10c4, 0xea60), (0x1a86, 0x55d4), (0x1a86, 0x7523)] {
            let c = classify(v, p);
            assert_eq!(c.link, Some(UsbLink::UsbUart), "{v:04x}:{p:04x}");
            assert_eq!(c.hint, "v1 or v3");
        }
        assert_eq!(classify(0x0403, 0x6001).link, None);
    }

    #[test]
    fn describe_includes_vid_pid() {
        let p = PortEntry {
            name: "COM3".into(),
            usb: Some(UsbPortInfo {
                vid: 0x10c4,
                pid: 0xea60,
                serial_number: None,
                manufacturer: None,
                product: Some("CP2102N".into()),
            }),
        };
        let d = p.describe();
        assert!(d.starts_with("10c4:ea60"));
        assert!(d.contains("CP210x"));
        assert!(d.contains("CP2102N"));
        assert!(!p.is_native_usb());
    }
}
