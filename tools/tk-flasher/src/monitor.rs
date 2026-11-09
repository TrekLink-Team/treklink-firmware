//! Serial monitor that does not reset the unit, and the line settings it shares with node info.
//!
//! Ports open with both DTR and RTS asserted on most systems. RTS is always released first,
//! so no step passes through RTS asserted with DTR released, which is a reset on both links.
//! - Native USB (ESP32-S3 USB Serial/JTAG): DTR stays asserted so a CDC device still sees a
//!   host. The boot strap is only sampled at reset, so DTR does not hold GPIO0 at run time.
//! - USB-UART bridge with the two-transistor auto-reset circuit (v1, v3): DTR asserted with RTS
//!   released pulls GPIO0 low, and any reboot would then land in ROM download mode. DTR is
//!   released as well, which leaves both EN and GPIO0 free.

use std::collections::VecDeque;
use std::io::Read;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::Sender;
use std::thread::JoinHandle;
use std::time::Duration;

use serialport::{FlowControl, SerialPort};

use crate::variant::UsbLink;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CtrlLine {
    Rts,
    Dtr,
}

/// The order and levels to set after opening a port, so the unit is neither reset nor left
/// with GPIO0 held low.
pub fn quiet_lines(link: UsbLink) -> [(CtrlLine, bool); 2] {
    match link {
        UsbLink::NativeUsb => [(CtrlLine::Rts, false), (CtrlLine::Dtr, true)],
        UsbLink::UsbUart => [(CtrlLine::Rts, false), (CtrlLine::Dtr, false)],
    }
}

/// Apply [`quiet_lines`] to an open port.
pub fn set_quiet_lines<P: SerialPort + ?Sized>(port: &mut P, link: UsbLink) -> Result<(), String> {
    for (line, level) in quiet_lines(link) {
        match line {
            CtrlLine::Rts => port
                .write_request_to_send(level)
                .map_err(|e| format!("Could not set RTS: {e}"))?,
            CtrlLine::Dtr => port
                .write_data_terminal_ready(level)
                .map_err(|e| format!("Could not set DTR: {e}"))?,
        }
    }
    Ok(())
}

/// How the lines end up, for the status line.
pub fn describe_lines(link: UsbLink) -> &'static str {
    match link {
        UsbLink::NativeUsb => "RTS low then DTR high (native USB), no reset",
        UsbLink::UsbUart => "RTS low then DTR low (USB-UART), no reset, GPIO0 free",
    }
}

pub enum MonitorEvent {
    Data(Vec<u8>),
    Closed(String),
}

pub struct Monitor {
    stop: Arc<AtomicBool>,
    handle: Option<JoinHandle<()>>,
}

impl Monitor {
    pub fn start(
        port: &str,
        baud: u32,
        link: UsbLink,
        tx: Sender<MonitorEvent>,
    ) -> Result<Monitor, String> {
        let mut serial = serialport::new(port, baud)
            .flow_control(FlowControl::None)
            .timeout(Duration::from_millis(100))
            .open()
            .map_err(|e| crate::flash::open_error(port, &e))?;
        set_quiet_lines(serial.as_mut(), link)?;
        let stop = Arc::new(AtomicBool::new(false));
        let flag = stop.clone();
        let handle = std::thread::spawn(move || {
            let mut buf = [0u8; 2048];
            while !flag.load(Ordering::Relaxed) {
                match serial.read(&mut buf) {
                    Ok(0) => {}
                    Ok(n) => {
                        if tx.send(MonitorEvent::Data(buf[..n].to_vec())).is_err() {
                            break;
                        }
                    }
                    Err(e) if e.kind() == std::io::ErrorKind::TimedOut => {}
                    Err(e) if e.kind() == std::io::ErrorKind::Interrupted => {}
                    Err(e) => {
                        let _ = tx.send(MonitorEvent::Closed(format!(
                            "The port closed: {e}. The unit may have been unplugged or reset."
                        )));
                        break;
                    }
                }
            }
        });
        Ok(Monitor {
            stop,
            handle: Some(handle),
        })
    }
}

impl Drop for Monitor {
    fn drop(&mut self) {
        self.stop.store(true, Ordering::Relaxed);
        if let Some(h) = self.handle.take() {
            let _ = h.join();
        }
    }
}

/// Scrollback that turns a byte stream into lines, drops ANSI colour codes and caps memory.
pub struct Scrollback {
    pub lines: VecDeque<String>,
    partial: String,
    in_escape: bool,
    cap: usize,
}

impl Scrollback {
    pub fn new(cap: usize) -> Self {
        Scrollback {
            lines: VecDeque::new(),
            partial: String::new(),
            in_escape: false,
            cap,
        }
    }

    pub fn push_bytes(&mut self, bytes: &[u8]) {
        let text = String::from_utf8_lossy(bytes);
        for ch in text.chars() {
            if self.in_escape {
                // CSI sequences end with a byte in 0x40..=0x7E (letters, @, ~, and so on).
                if ch.is_ascii_alphabetic() || ch == '~' || ch == '@' {
                    self.in_escape = false;
                }
                continue;
            }
            match ch {
                '\x1b' => self.in_escape = true,
                '\n' => self.end_line(),
                '\r' => {}
                c if c.is_control() && c != '\t' => {}
                c => self.partial.push(c),
            }
        }
    }

    fn end_line(&mut self) {
        let line = std::mem::take(&mut self.partial);
        self.lines.push_back(line);
        while self.lines.len() > self.cap {
            self.lines.pop_front();
        }
    }

    /// All complete lines plus the line still being received.
    pub fn snapshot(&self) -> Vec<&str> {
        let mut v: Vec<&str> = self.lines.iter().map(String::as_str).collect();
        if !self.partial.is_empty() {
            v.push(&self.partial);
        }
        v
    }

    pub fn len(&self) -> usize {
        self.lines.len() + usize::from(!self.partial.is_empty())
    }

    pub fn to_text(&self) -> String {
        let mut s = self.snapshot().join("\n");
        s.push('\n');
        s
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// (DTR, RTS) after each step from a start state. Reset on both links is RTS asserted
    /// with DTR released; GPIO0 low on a USB-UART bridge is DTR asserted with RTS released.
    fn walk(link: UsbLink, start: (bool, bool)) -> Vec<(bool, bool)> {
        let (mut dtr, mut rts) = start;
        quiet_lines(link)
            .iter()
            .map(|&(line, level)| {
                match line {
                    CtrlLine::Dtr => dtr = level,
                    CtrlLine::Rts => rts = level,
                }
                (dtr, rts)
            })
            .collect()
    }

    #[test]
    fn line_sequences_never_reset_and_free_gpio0_on_usb_uart() {
        for start in [(true, true), (false, false), (true, false)] {
            for link in [UsbLink::NativeUsb, UsbLink::UsbUart] {
                for (dtr, rts) in walk(link, start) {
                    assert!(
                        !(rts && !dtr),
                        "{link:?} from {start:?} passes a reset state"
                    );
                }
            }
            assert_eq!(
                *walk(UsbLink::UsbUart, start).last().unwrap(),
                (false, false)
            );
            assert_eq!(
                *walk(UsbLink::NativeUsb, start).last().unwrap(),
                (true, false)
            );
        }
        assert_eq!(quiet_lines(UsbLink::UsbUart)[0], (CtrlLine::Rts, false));
    }

    #[test]
    fn scrollback_splits_lines_and_strips_colour() {
        let mut s = Scrollback::new(3);
        s.push_bytes(b"\x1b[34mINFO \x1b[0m| boot\r\nsec");
        s.push_bytes(b"ond\nthird\nfourth\npartial");
        assert_eq!(s.snapshot(), vec!["second", "third", "fourth", "partial"]);
        assert_eq!(s.len(), 4);
        let mut t = Scrollback::new(10);
        t.push_bytes(b"\x1b[34mINFO \x1b[0m| boot\n");
        assert_eq!(t.snapshot(), vec!["INFO | boot"]);
    }
}
