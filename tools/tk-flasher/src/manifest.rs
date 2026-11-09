//! `treklink-manifest.json`, written by `bin/treklink_manifest.py` in the release workflow.
//!
//! Shape: `{"version": tag, "variants": [{"variantCode", "pioEnv", "chip",
//! "app": {"fileName", "offset", "size", "sha256"}, "factory": {...}}]}`.

use serde::Deserialize;

use crate::layout::{APP_OFFSET, FACTORY_OFFSET};
use crate::variant::Variant;

#[derive(Debug, Clone, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "camelCase")]
pub struct ManifestImage {
    pub file_name: String,
    pub offset: u32,
    pub size: u64,
    pub sha256: String,
}

#[derive(Debug, Clone, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "camelCase")]
pub struct ManifestVariant {
    pub variant_code: String,
    pub pio_env: String,
    pub chip: String,
    pub app: ManifestImage,
    pub factory: ManifestImage,
}

#[derive(Debug, Clone, Deserialize, PartialEq, Eq)]
pub struct Manifest {
    pub version: String,
    pub variants: Vec<ManifestVariant>,
}

pub const MANIFEST_ASSET: &str = "treklink-manifest.json";

pub fn parse(json: &str) -> Result<Manifest, String> {
    let m: Manifest = serde_json::from_str(json)
        .map_err(|e| format!("The release manifest is not valid: {e}"))?;
    if m.version.trim().is_empty() {
        return Err("The release manifest has an empty version.".into());
    }
    Ok(m)
}

fn check_image(what: &str, img: &ManifestImage, offset: u32) -> Result<(), String> {
    if img.offset != offset {
        return Err(format!(
            "The manifest puts the {what} image at 0x{:x}; this tool expects 0x{offset:x}.",
            img.offset
        ));
    }
    if img.sha256.len() != 64 || !img.sha256.bytes().all(|b| b.is_ascii_hexdigit()) {
        return Err(format!(
            "The manifest's SHA-256 for {} is malformed.",
            img.file_name
        ));
    }
    if img.file_name.is_empty() || img.file_name.contains('/') || img.file_name.contains('\\') {
        return Err(format!(
            "The manifest's file name '{}' is not a plain asset name.",
            img.file_name
        ));
    }
    if img.size == 0 {
        return Err(format!("The manifest lists {} with size 0.", img.file_name));
    }
    Ok(())
}

impl Manifest {
    /// The entry for a variant, checked against what this tool knows about the variant.
    pub fn select(&self, variant: Variant) -> Result<&ManifestVariant, String> {
        let entry = self
            .variants
            .iter()
            .find(|v| v.variant_code == variant.code())
            .ok_or_else(|| {
                let codes: Vec<&str> = self
                    .variants
                    .iter()
                    .map(|v| v.variant_code.as_str())
                    .collect();
                format!(
                    "Release {} has no image for {} (it has: {}).",
                    self.version,
                    variant.code(),
                    codes.join(", ")
                )
            })?;
        if entry.chip != variant.chip_name() {
            return Err(format!(
                "The manifest says {} is for chip '{}', but {} uses '{}'. Refusing.",
                entry.variant_code,
                entry.chip,
                variant.code(),
                variant.chip_name()
            ));
        }
        if entry.pio_env != variant.pio_env() {
            return Err(format!(
                "The manifest builds {} from env '{}', expected '{}'. Refusing.",
                entry.variant_code,
                entry.pio_env,
                variant.pio_env()
            ));
        }
        check_image("application", &entry.app, APP_OFFSET)?;
        check_image("factory", &entry.factory, FACTORY_OFFSET)?;
        Ok(entry)
    }
}

/// Check downloaded bytes against a manifest entry.
pub fn check_download(img: &ManifestImage, data: &[u8]) -> Result<(), String> {
    use sha2::{Digest, Sha256};
    if data.len() as u64 != img.size {
        return Err(format!(
            "{} is {} bytes, the manifest says {}.",
            img.file_name,
            data.len(),
            img.size
        ));
    }
    let digest = hex::encode(Sha256::digest(data));
    if !digest.eq_ignore_ascii_case(&img.sha256) {
        return Err(format!("{} failed its SHA-256 check.", img.file_name));
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The exact shape `bin/treklink_manifest.py` writes (see its self_test).
    const SAMPLE: &str = r#"{
  "version": "v2.7.26-treklink.1",
  "variants": [
    {
      "variantCode": "v2",
      "pioEnv": "treklink-v2",
      "chip": "esp32s3",
      "app": {"fileName": "firmware-treklink-v2.bin", "offset": 65536, "size": 12,
              "sha256": "8f2a6c0e7d40cbbd4b0a8c45c2b78f07b8a0e1b3c9d2f6a4e5b1c7d8e9f0a1b2"},
      "factory": {"fileName": "firmware-treklink-v2.factory.bin", "offset": 0, "size": 16,
              "sha256": "0000000000000000000000000000000000000000000000000000000000000000"}
    },
    {
      "variantCode": "v3",
      "pioEnv": "treklink-v3-tbeam",
      "chip": "esp32",
      "app": {"fileName": "firmware-treklink-v3-tbeam.bin", "offset": 65536, "size": 1,
              "sha256": "1111111111111111111111111111111111111111111111111111111111111111"},
      "factory": {"fileName": "firmware-treklink-v3-tbeam.factory.bin", "offset": 0, "size": 1,
              "sha256": "2222222222222222222222222222222222222222222222222222222222222222"}
    },
    {
      "variantCode": "v4",
      "pioEnv": "treklink-v4-supreme",
      "chip": "esp32s3",
      "app": {"fileName": "firmware-treklink-v4-supreme.bin", "offset": 65536, "size": 1,
              "sha256": "3333333333333333333333333333333333333333333333333333333333333333"},
      "factory": {"fileName": "firmware-treklink-v4-supreme.factory.bin", "offset": 0, "size": 1,
              "sha256": "4444444444444444444444444444444444444444444444444444444444444444"}
    }
  ]
}"#;

    #[test]
    fn parses_the_release_workflow_manifest() {
        let m = parse(SAMPLE).unwrap();
        assert_eq!(m.version, "v2.7.26-treklink.1");
        assert_eq!(m.variants.len(), 3);
        let v4 = m.select(Variant::V4).unwrap();
        assert_eq!(
            v4.factory.file_name,
            "firmware-treklink-v4-supreme.factory.bin"
        );
        assert_eq!(v4.app.offset, 0x10000);
        assert_eq!(m.select(Variant::V3).unwrap().chip, "esp32");
        assert_eq!(m.select(Variant::V2).unwrap().pio_env, "treklink-v2");
    }

    #[test]
    fn v1_is_not_in_the_release() {
        let m = parse(SAMPLE).unwrap();
        let err = m.select(Variant::V1).unwrap_err();
        assert!(err.contains("no image for v1"));
        assert!(err.contains("v2, v3, v4"));
    }

    #[test]
    fn mismatched_chip_or_offset_is_refused() {
        let bad_chip = SAMPLE.replacen("\"chip\": \"esp32s3\"", "\"chip\": \"esp32\"", 1);
        assert!(
            parse(&bad_chip)
                .unwrap()
                .select(Variant::V2)
                .unwrap_err()
                .contains("chip")
        );
        let bad_off = SAMPLE.replacen("\"offset\": 65536", "\"offset\": 4096", 1);
        assert!(
            parse(&bad_off)
                .unwrap()
                .select(Variant::V2)
                .unwrap_err()
                .contains("0x1000")
        );
        let bad_env = SAMPLE.replacen("treklink-v3-tbeam\"", "tbeam\"", 1);
        assert!(
            parse(&bad_env)
                .unwrap()
                .select(Variant::V3)
                .unwrap_err()
                .contains("env")
        );
    }

    #[test]
    fn malformed_manifests_are_errors() {
        assert!(parse("{}").is_err());
        assert!(parse("not json").is_err());
        assert!(parse(r#"{"version": " ", "variants": []}"#).is_err());
        let bad_sha = SAMPLE.replacen("8f2a6c0e", "zz2a6c0e", 1);
        assert!(
            parse(&bad_sha)
                .unwrap()
                .select(Variant::V2)
                .unwrap_err()
                .contains("SHA-256")
        );
    }

    #[test]
    fn download_check_uses_size_and_sha256() {
        use sha2::{Digest, Sha256};
        let data = b"treklink-app";
        let img = ManifestImage {
            file_name: "app.bin".into(),
            offset: 0x10000,
            size: data.len() as u64,
            sha256: hex::encode(Sha256::digest(data)),
        };
        assert!(check_download(&img, data).is_ok());
        assert!(
            check_download(&img, b"treklink-apq")
                .unwrap_err()
                .contains("SHA-256")
        );
        assert!(
            check_download(&img, b"short")
                .unwrap_err()
                .contains("bytes")
        );
    }
}
