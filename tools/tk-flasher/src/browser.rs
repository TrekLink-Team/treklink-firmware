//! A keyboard file browser that lists folders and `.bin` files.

use std::path::{Path, PathBuf};

use ratatui::widgets::ListState;

#[derive(Debug, Clone)]
pub struct Entry {
    pub name: String,
    pub path: PathBuf,
    pub is_dir: bool,
    pub size: u64,
}

pub struct Browser {
    pub dir: PathBuf,
    pub entries: Vec<Entry>,
    pub state: ListState,
    pub error: Option<String>,
    /// Text typed after pressing `/` to jump to a path (also how to change drive on Windows).
    pub goto: Option<String>,
}

pub enum Outcome {
    Stay,
    Picked(PathBuf),
}

pub fn is_bin(name: &str) -> bool {
    name.to_ascii_lowercase().ends_with(".bin")
}

impl Browser {
    pub fn new(dir: PathBuf) -> Self {
        let mut b = Browser {
            dir,
            entries: Vec::new(),
            state: ListState::default(),
            error: None,
            goto: None,
        };
        b.reload();
        b
    }

    pub fn reload(&mut self) {
        self.entries.clear();
        self.error = None;
        if let Some(parent) = self.dir.parent() {
            self.entries.push(Entry {
                name: "..".into(),
                path: parent.to_path_buf(),
                is_dir: true,
                size: 0,
            });
        }
        match std::fs::read_dir(&self.dir) {
            Ok(rd) => {
                let mut items: Vec<Entry> = rd
                    .filter_map(Result::ok)
                    .filter_map(|e| {
                        let name = e.file_name().to_string_lossy().into_owned();
                        let meta = std::fs::metadata(e.path()).ok()?;
                        let is_dir = meta.is_dir();
                        if name.starts_with('.') && name != ".pio" {
                            return None;
                        }
                        if !is_dir && !is_bin(&name) {
                            return None;
                        }
                        Some(Entry {
                            name,
                            path: e.path(),
                            is_dir,
                            size: meta.len(),
                        })
                    })
                    .collect();
                items.sort_by(|a, b| {
                    b.is_dir
                        .cmp(&a.is_dir)
                        .then(a.name.to_lowercase().cmp(&b.name.to_lowercase()))
                });
                self.entries.extend(items);
            }
            Err(e) => self.error = Some(format!("Cannot read {}: {e}", self.dir.display())),
        }
        // Start on the first file when there is one, otherwise on the first folder.
        let first_file = self.entries.iter().position(|e| !e.is_dir);
        self.state.select(first_file.or(if self.entries.is_empty() {
            None
        } else {
            Some(0)
        }));
    }

    pub fn up(&mut self) {
        if let Some(parent) = self.dir.parent() {
            self.dir = parent.to_path_buf();
            self.reload();
        }
    }

    pub fn enter(&mut self) -> Outcome {
        let Some(entry) = self
            .state
            .selected()
            .and_then(|i| self.entries.get(i))
            .cloned()
        else {
            return Outcome::Stay;
        };
        if entry.is_dir {
            self.dir = entry.path;
            self.reload();
            Outcome::Stay
        } else {
            Outcome::Picked(entry.path)
        }
    }

    pub fn jump(&mut self, text: &str) -> Outcome {
        let p = expand(text.trim());
        if p.is_dir() {
            self.dir = p;
            self.reload();
            Outcome::Stay
        } else if p.is_file() {
            Outcome::Picked(p)
        } else {
            self.error = Some(format!("{} is not a folder or file.", p.display()));
            Outcome::Stay
        }
    }

    pub fn move_by(&mut self, delta: isize) {
        if self.entries.is_empty() {
            return;
        }
        let cur = self.state.selected().unwrap_or(0) as isize;
        let next = (cur + delta).clamp(0, self.entries.len() as isize - 1);
        self.state.select(Some(next as usize));
    }
}

fn expand(text: &str) -> PathBuf {
    if let Some(rest) = text.strip_prefix('~')
        && let Some(home) = std::env::var_os("HOME").or_else(|| std::env::var_os("USERPROFILE"))
    {
        return Path::new(&home).join(rest.trim_start_matches(['/', '\\']));
    }
    PathBuf::from(text)
}

/// Find a sibling file by exact name (case-insensitive) in the same folder.
pub fn sibling(path: &Path, name: &str) -> Option<PathBuf> {
    let dir = path.parent()?;
    std::fs::read_dir(dir)
        .ok()?
        .filter_map(Result::ok)
        .find(|e| e.file_name().to_string_lossy().eq_ignore_ascii_case(name))
        .map(|e| e.path())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn bin_filter() {
        assert!(is_bin("firmware-treklink-v2.factory.bin"));
        assert!(is_bin("BOOTLOADER.BIN"));
        assert!(!is_bin("firmware.elf"));
    }
}
