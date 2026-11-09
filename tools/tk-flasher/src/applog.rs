//! Action log, written to `tk-flasher.log` next to the binary (or the working directory when
//! the binary's folder is read-only), plus small time helpers without extra dependencies.

use std::fs::{File, OpenOptions};
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::{Mutex, OnceLock};
use std::time::{SystemTime, UNIX_EPOCH};

static LOG: OnceLock<Mutex<Option<File>>> = OnceLock::new();
static BASE: OnceLock<PathBuf> = OnceLock::new();

/// The folder for the log, saved monitor output and downloaded releases.
pub fn base_dir() -> &'static Path {
    BASE.get_or_init(|| {
        let exe_dir = std::env::current_exe()
            .ok()
            .and_then(|p| p.parent().map(Path::to_path_buf));
        if let Some(dir) = exe_dir
            && writable(&dir)
        {
            return dir;
        }
        std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."))
    })
}

fn writable(dir: &Path) -> bool {
    let probe = dir.join(".tk-flasher-write-test");
    let ok = File::create(&probe).is_ok();
    let _ = std::fs::remove_file(&probe);
    ok
}

pub fn log_path() -> PathBuf {
    base_dir().join("tk-flasher.log")
}

pub fn init() {
    let file = OpenOptions::new()
        .create(true)
        .append(true)
        .open(log_path())
        .ok();
    let _ = LOG.set(Mutex::new(file));
    log(&format!("tk-flasher {} started", env!("CARGO_PKG_VERSION")));
}

pub fn log(line: &str) {
    if let Some(m) = LOG.get()
        && let Ok(mut guard) = m.lock()
        && let Some(f) = guard.as_mut()
    {
        let _ = writeln!(f, "{} {}", timestamp(), line);
        let _ = f.flush();
    }
}

fn now_secs() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0)
}

/// Civil date from days since 1970-01-01 (Howard Hinnant's algorithm).
pub fn civil(days: i64) -> (i64, u32, u32) {
    let z = days + 719_468;
    let era = z.div_euclid(146_097);
    let doe = z.rem_euclid(146_097);
    let yoe = (doe - doe / 1460 + doe / 36_524 - doe / 146_096) / 365;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = (doy - (153 * mp + 2) / 5 + 1) as u32;
    let m = if mp < 10 { mp + 3 } else { mp - 9 } as u32;
    let y = yoe + era * 400 + i64::from(m <= 2);
    (y, m, d)
}

fn split(secs: u64) -> (i64, u32, u32, u64, u64, u64) {
    let (y, m, d) = civil((secs / 86_400) as i64);
    let rem = secs % 86_400;
    (y, m, d, rem / 3600, (rem % 3600) / 60, rem % 60)
}

/// `2026-11-09T17:26:00Z`
pub fn timestamp() -> String {
    let (y, m, d, hh, mm, ss) = split(now_secs());
    format!("{y:04}-{m:02}-{d:02}T{hh:02}:{mm:02}:{ss:02}Z")
}

/// `20261109-172600`, for file names.
pub fn file_stamp() -> String {
    let (y, m, d, hh, mm, ss) = split(now_secs());
    format!("{y:04}{m:02}{d:02}-{hh:02}{mm:02}{ss:02}")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn civil_dates() {
        assert_eq!(civil(0), (1970, 1, 1));
        assert_eq!(civil(20_401), (2025, 11, 9));
        assert_eq!(civil(11_016), (2000, 2, 29));
    }
}
