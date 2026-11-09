//! Latest TrekLink release from GitHub: the manifest first, then the two images it names.
//!
//! The public REST API needs no token for a public repository. For a private repository
//! the tool reads `GITHUB_TOKEN` from the environment and sends it as a bearer token.
//! Assets are downloaded through the API asset URL with `Accept: application/octet-stream`,
//! which works with and without a token; ureq drops the `Authorization` header when GitHub
//! redirects to its storage host.

use std::path::{Path, PathBuf};
use std::time::Duration;

use serde::Deserialize;

use crate::layout::FirmwareImage;
use crate::manifest::{self, MANIFEST_ASSET};
use crate::variant::Variant;

pub const DEFAULT_REPO: &str = "TrekLink-Team/treklink-firmware";
const API: &str = "https://api.github.com";
const MAX_ASSET_BYTES: u64 = 32 * 1024 * 1024;

/// `owner/name`, overridable with `TK_FLASHER_REPO` for a fork.
pub fn repo() -> String {
    std::env::var("TK_FLASHER_REPO")
        .ok()
        .filter(|r| r.contains('/'))
        .unwrap_or_else(|| DEFAULT_REPO.to_string())
}

pub fn token() -> Option<String> {
    std::env::var("GITHUB_TOKEN")
        .ok()
        .filter(|t| !t.trim().is_empty())
}

#[derive(Debug, Clone, Deserialize, PartialEq, Eq)]
pub struct Asset {
    pub name: String,
    /// The API URL of the asset.
    pub url: String,
    pub size: u64,
}

#[derive(Debug, Clone, Deserialize, PartialEq, Eq)]
pub struct Release {
    pub tag_name: String,
    #[serde(default)]
    pub html_url: String,
    #[serde(default)]
    pub assets: Vec<Asset>,
}

pub fn parse_release(json: &str) -> Result<Release, String> {
    serde_json::from_str(json)
        .map_err(|e| format!("GitHub returned a release this tool cannot read: {e}"))
}

impl Release {
    pub fn asset(&self, name: &str) -> Result<&Asset, String> {
        self.assets.iter().find(|a| a.name == name).ok_or_else(|| {
            format!(
                "Release {} has no asset named {name}. Was it published by the TrekLink release workflow?",
                self.tag_name
            )
        })
    }
}

/// The result of a successful fetch.
pub struct Fetched {
    pub tag: String,
    pub image: FirmwareImage,
    pub saved_to: Option<PathBuf>,
}

fn agent() -> ureq::Agent {
    let tls = ureq::tls::TlsConfig::builder()
        .root_certs(ureq::tls::RootCerts::PlatformVerifier)
        .build();
    ureq::Agent::config_builder()
        .timeout_global(Some(Duration::from_secs(180)))
        .http_status_as_error(false)
        .user_agent(concat!("tk-flasher/", env!("CARGO_PKG_VERSION")))
        .tls_config(tls)
        .build()
        .into()
}

fn get(agent: &ureq::Agent, url: &str, accept: &str) -> Result<Vec<u8>, String> {
    let mut req = agent
        .get(url)
        .header("Accept", accept)
        .header("X-GitHub-Api-Version", "2022-11-28");
    if let Some(t) = token() {
        req = req.header("Authorization", &format!("Bearer {t}"));
    }
    let mut resp = req
        .call()
        .map_err(|e| format!("Network error talking to GitHub: {e}"))?;
    let status = resp.status().as_u16();
    match status {
        200 => resp
            .body_mut()
            .with_config()
            .limit(MAX_ASSET_BYTES)
            .read_to_vec()
            .map_err(|e| format!("Download from GitHub failed: {e}")),
        401 => Err("GitHub rejected GITHUB_TOKEN (401). Check that the token is valid.".into()),
        403 | 429 => Err(format!(
            "GitHub refused the request ({status}), most likely the hourly rate limit for requests without a token. Set GITHUB_TOKEN or try later."
        )),
        404 if token().is_none() => Err(format!(
            "GitHub returned 404 for {}. Either it has no release yet, or the repository is private: set GITHUB_TOKEN to a token that can read it and start the tool again.",
            repo()
        )),
        404 => Err(format!(
            "GitHub returned 404 for {} even with GITHUB_TOKEN. Check the token can read this repository and that a release exists.",
            repo()
        )),
        other => Err(format!("GitHub answered HTTP {other} for {url}.")),
    }
}

/// Fetch the latest release and the images for one variant. `say` reports each step.
pub fn fetch_latest(
    variant: Variant,
    save_dir: Option<&Path>,
    say: &dyn Fn(String),
) -> Result<Fetched, String> {
    if !variant.in_release() {
        return Err(format!(
            "The release workflow does not build {} (env {}). Use a local image for this variant.",
            variant.code(),
            variant.pio_env()
        ));
    }
    let agent = agent();
    let repo = repo();
    say(format!("Asking GitHub for the latest release of {repo}"));
    let body = get(
        &agent,
        &format!("{API}/repos/{repo}/releases/latest"),
        "application/vnd.github+json",
    )?;
    let release = parse_release(&String::from_utf8_lossy(&body))?;
    say(format!("Latest release: {}", release.tag_name));

    let manifest_asset = release.asset(MANIFEST_ASSET)?;
    let manifest_bytes = get(&agent, &manifest_asset.url, "application/octet-stream")?;
    let manifest = manifest::parse(&String::from_utf8_lossy(&manifest_bytes))?;
    let entry = manifest.select(variant)?;

    let mut images = Vec::new();
    for img in [&entry.factory, &entry.app] {
        let asset = release.asset(&img.file_name)?;
        say(format!(
            "Downloading {} ({} KiB)",
            img.file_name,
            img.size / 1024
        ));
        let data = get(&agent, &asset.url, "application/octet-stream")?;
        manifest::check_download(img, &data)?;
        say(format!("{} matches the manifest SHA-256", img.file_name));
        images.push(data);
    }
    let app = images.pop();
    let factory = images.pop();

    let mut saved_to = None;
    if let Some(dir) = save_dir {
        let dir = dir.join(sanitize(&release.tag_name));
        let ok = std::fs::create_dir_all(&dir).is_ok()
            && std::fs::write(
                dir.join(&entry.factory.file_name),
                factory.as_deref().unwrap_or_default(),
            )
            .is_ok()
            && std::fs::write(
                dir.join(&entry.app.file_name),
                app.as_deref().unwrap_or_default(),
            )
            .is_ok()
            && std::fs::write(dir.join(MANIFEST_ASSET), &manifest_bytes).is_ok();
        if ok {
            saved_to = Some(dir);
        }
    }

    Ok(Fetched {
        tag: release.tag_name.clone(),
        image: FirmwareImage {
            label: format!(
                "GitHub release {} ({})",
                release.tag_name, entry.factory.file_name
            ),
            factory,
            app,
            ..Default::default()
        },
        saved_to,
    })
}

fn sanitize(tag: &str) -> String {
    tag.chars()
        .map(|c| {
            if c.is_ascii_alphanumeric() || "._-".contains(c) {
                c
            } else {
                '_'
            }
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_a_release_and_finds_assets() {
        let json = r#"{
            "tag_name": "v2.7.26-treklink.1",
            "html_url": "https://github.com/TrekLink-Team/treklink-firmware/releases/tag/v2.7.26-treklink.1",
            "draft": false,
            "assets": [
                {"name": "treklink-manifest.json", "url": "https://api.github.com/repos/x/y/releases/assets/1", "size": 900, "browser_download_url": "https://github.com/x"},
                {"name": "firmware-treklink-v2.factory.bin", "url": "https://api.github.com/repos/x/y/releases/assets/2", "size": 2000000}
            ]
        }"#;
        let r = parse_release(json).unwrap();
        assert_eq!(r.tag_name, "v2.7.26-treklink.1");
        assert_eq!(r.asset(MANIFEST_ASSET).unwrap().size, 900);
        assert!(
            r.asset("firmware-treklink-v4-supreme.bin")
                .unwrap_err()
                .contains("no asset")
        );
    }

    #[test]
    fn release_without_assets_parses() {
        let r = parse_release(r#"{"tag_name": "v1"}"#).unwrap();
        assert!(r.assets.is_empty());
        assert!(parse_release("[]").is_err());
    }

    #[test]
    fn tag_is_sanitized_for_a_directory_name() {
        assert_eq!(sanitize("v2.7.26-treklink.1"), "v2.7.26-treklink.1");
        assert_eq!(sanitize("../x y"), ".._x_y");
    }

    /// Network test against the real latest release. Run with
    /// `cargo test -- --ignored live_release`.
    #[test]
    #[ignore]
    fn live_release_images_produce_valid_plans() {
        use crate::layout;
        for v in [Variant::V2, Variant::V3, Variant::V4] {
            let fetched = fetch_latest(v, None, &|s| println!("{s}")).unwrap();
            let img = &fetched.image;
            let install = layout::install_plan(img, v.chip()).unwrap();
            let update = layout::update_plan(img, v.chip()).unwrap();
            println!("{} {}: install {:?}", v.code(), fetched.tag, install);
            println!("{} {}: update {:?}", v.code(), fetched.tag, update);
            assert_eq!(install[0].offset, 0);
            assert_eq!(update[0].offset, layout::bootloader_offset(v.chip()));
            assert_eq!(
                update[3].data.len(),
                img.app.as_ref().unwrap().len().next_multiple_of(4)
            );
            assert_eq!(update[2].data, layout::default_boot_app0());
            let other = if v.chip() == espflash::target::Chip::Esp32 {
                espflash::target::Chip::Esp32s3
            } else {
                espflash::target::Chip::Esp32
            };
            assert!(layout::install_plan(img, other).is_err());
            let entries = layout::parse_partitions(&update[1].data).unwrap();
            for e in &entries {
                println!(
                    "  partition {:<10} type {} sub 0x{:02x} at 0x{:06x} size 0x{:06x}",
                    e.label, e.kind, e.subtype, e.offset, e.size
                );
            }
        }
    }

    #[test]
    fn v1_is_refused_before_any_network_call() {
        let err = match fetch_latest(Variant::V1, None, &|_| {}) {
            Err(e) => e,
            Ok(_) => panic!("v1 must be refused"),
        };
        assert!(err.contains("does not build v1"));
    }
}
