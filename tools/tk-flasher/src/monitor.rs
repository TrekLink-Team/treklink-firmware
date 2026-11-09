//! Serial monitor that does not reset the unit.
//!
//! An ESP32-S3 on native USB resets when RTS is high while DTR is low; a USB-UART bridge
//! resets on RTS asserted with DTR released. Ports open with both lines asserted on most
//! systems, so RTS is dropped first and DTR stays asserted. Neither step passes through a
//! reset state, and DTR stays high so a CDC device still sees a host.

use std::collections::VecDeque;
use std::io::Read;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::Sender;
use std::thread::JoinHandle;
use std::time::Duration;

use serialport::FlowControl;

pub enum MonitorEvent {
    Data(Vec<u8>),
    Closed(String),
}

pub struct Monitor {
    stop: Arc<AtomicBool>,
    handle: Option<JoinHandle<()>>,
}

impl Monitor {
    pub fn start(port: &str, baud: u32, tx: Sender<MonitorEvent>) -> Result<Monitor, String> {
        let mut serial = serialport::new(port, baud)
            .flow_control(FlowControl::None)
            .timeout(Duration::from_millis(100))
            .open()
            .map_err(|e| crate::flash::open_error(port, &e))?;
        serial
            .write_request_to_send(false)
            .map_err(|e| format!("Could not set RTS: {e}"))?;
        serial
            .write_data_terminal_ready(true)
            .map_err(|e| format!("Could not set DTR: {e}"))?;
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
