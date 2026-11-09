//! Node info over the Meshtastic serial API (the `want_config_id` handshake).
//!
//! The port is opened the same way as the monitor (RTS low first, DTR high), so the unit
//! is not reset. If nothing that parses as a Meshtastic frame arrives within a few seconds,
//! the unit is reported as not running Meshtastic.

use std::io;
use std::pin::Pin;
use std::sync::{Arc, Mutex};
use std::task::{Context, Poll};
use std::time::{Duration, Instant};

use meshtastic::api::{StreamApi, StreamHandle};
use meshtastic::protobufs::{self, from_radio};
use tokio::io::{AsyncRead, AsyncWrite, ReadBuf};
use tokio_serial::{SerialPort, SerialStream};

const FIRST_REPLY: Duration = Duration::from_secs(6);
const WHOLE_HANDSHAKE: Duration = Duration::from_secs(20);
const TEXT_CAP: usize = 64 * 1024;

#[derive(Debug, Default, Clone, PartialEq)]
pub struct NodeReport {
    pub node_num: Option<u32>,
    pub long_name: Option<String>,
    pub short_name: Option<String>,
    pub firmware: Option<String>,
    pub hw_model: Option<String>,
    pub battery: Option<String>,
    pub region: Option<String>,
    pub variant: Option<String>,
    pub complete: bool,
}

impl NodeReport {
    pub fn rows(&self) -> Vec<(&'static str, String)> {
        let unknown = || "not reported".to_string();
        vec![
            (
                "Node number",
                self.node_num
                    .map_or_else(unknown, |n| format!("{n} (!{n:08x})")),
            ),
            ("Long name", self.long_name.clone().unwrap_or_else(unknown)),
            (
                "Short name",
                self.short_name.clone().unwrap_or_else(unknown),
            ),
            ("Firmware", self.firmware.clone().unwrap_or_else(unknown)),
            (
                "Hardware model",
                self.hw_model.clone().unwrap_or_else(unknown),
            ),
            ("Battery", self.battery.clone().unwrap_or_else(unknown)),
            ("LoRa region", self.region.clone().unwrap_or_else(unknown)),
            (
                "TrekLink variant",
                self.variant.clone().unwrap_or_else(unknown),
            ),
        ]
    }
}

/// Copies every byte read into a buffer so plain-text boot logs (which carry the
/// `Variant: TrekLink ...` line) can be searched after the handshake.
struct Tee {
    inner: SerialStream,
    seen: Arc<Mutex<Vec<u8>>>,
}

impl AsyncRead for Tee {
    fn poll_read(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        buf: &mut ReadBuf<'_>,
    ) -> Poll<io::Result<()>> {
        let before = buf.filled().len();
        let r = Pin::new(&mut self.inner).poll_read(cx, buf);
        if let Poll::Ready(Ok(())) = &r
            && let Ok(mut seen) = self.seen.lock()
            && seen.len() < TEXT_CAP
        {
            seen.extend_from_slice(&buf.filled()[before..]);
        }
        r
    }
}

impl AsyncWrite for Tee {
    fn poll_write(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        data: &[u8],
    ) -> Poll<io::Result<usize>> {
        Pin::new(&mut self.inner).poll_write(cx, data)
    }

    fn poll_flush(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<io::Result<()>> {
        Pin::new(&mut self.inner).poll_flush(cx)
    }

    fn poll_shutdown(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<io::Result<()>> {
        Pin::new(&mut self.inner).poll_shutdown(cx)
    }
}

/// The variant name logged at boot by `src/modules/Modules.cpp` (`Variant: %s`).
pub fn variant_from_text(text: &str) -> Option<String> {
    let idx = text.find("Variant: TrekLink")?;
    let rest = &text[idx + "Variant: ".len()..];
    let end = rest.find(['\r', '\n', '\x1b']).unwrap_or(rest.len());
    Some(rest[..end].trim().to_string())
}

/// The protobuf carries no TrekLink variant field, so infer one from the hardware model
/// (`src/platform/esp32/architecture.h`) and say that it is inferred.
pub fn variant_from_hw(hw: &str, long_name: Option<&str>) -> Option<String> {
    let treklink = long_name.is_some_and(|n| n.starts_with("TrekLink"));
    match hw {
        "TBEAM" => Some("v3 (inferred from hardware model TBEAM)".into()),
        "LILYGO_TBEAM_S3_CORE" => {
            Some("v4 (inferred from hardware model LILYGO_TBEAM_S3_CORE)".into())
        }
        _ if treklink => Some("TrekLink custom board, v1 or v2 (not reported over the API)".into()),
        _ => None,
    }
}

fn apply(report: &mut NodeReport, fr: protobufs::FromRadio, config_id: u32) {
    let Some(payload) = fr.payload_variant else {
        return;
    };
    match payload {
        from_radio::PayloadVariant::MyInfo(info) => report.node_num = Some(info.my_node_num),
        from_radio::PayloadVariant::NodeInfo(node) => {
            if Some(node.num) != report.node_num {
                return;
            }
            if let Some(user) = node.user {
                report.long_name = Some(user.long_name);
                report.short_name = Some(user.short_name);
                if report.hw_model.is_none() {
                    report.hw_model = hw_name(user.hw_model);
                }
            }
            if let Some(m) = node.device_metrics {
                report.battery = match (m.battery_level, m.voltage) {
                    (Some(101), Some(v)) => Some(format!("external power ({v:.2} V)")),
                    (Some(l), Some(v)) => Some(format!("{l}% ({v:.2} V)")),
                    (Some(101), None) => Some("external power".into()),
                    (Some(l), None) => Some(format!("{l}%")),
                    (None, Some(v)) => Some(format!("{v:.2} V")),
                    (None, None) => None,
                };
            }
        }
        from_radio::PayloadVariant::Metadata(meta) => {
            report.firmware = Some(meta.firmware_version);
            report.hw_model = hw_name(meta.hw_model).or(report.hw_model.take());
        }
        from_radio::PayloadVariant::Config(cfg) => {
            if let Some(protobufs::config::PayloadVariant::Lora(lora)) = cfg.payload_variant {
                report.region = Some(
                    protobufs::config::lo_ra_config::RegionCode::try_from(lora.region)
                        .map(|r| r.as_str_name().to_string())
                        .unwrap_or_else(|_| format!("code {}", lora.region)),
                );
            }
        }
        from_radio::PayloadVariant::LogRecord(rec) => {
            if report.variant.is_none() {
                report.variant = variant_from_text(&rec.message);
            }
        }
        from_radio::PayloadVariant::ConfigCompleteId(id) if id == config_id => {
            report.complete = true
        }
        _ => {}
    }
}

fn hw_name(code: i32) -> Option<String> {
    protobufs::HardwareModel::try_from(code)
        .ok()
        .map(|h| h.as_str_name().to_string())
}

/// Run the handshake on a fresh single-threaded runtime. Blocks the calling thread.
pub fn query(port: &str) -> Result<NodeReport, String> {
    let rt = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .map_err(|e| format!("Could not start the async runtime: {e}"))?;
    rt.block_on(query_async(port))
}

async fn query_async(port: &str) -> Result<NodeReport, String> {
    let builder = tokio_serial::new(port, crate::flash::BAUD)
        .flow_control(tokio_serial::FlowControl::None)
        .timeout(Duration::from_millis(10));
    let mut stream =
        SerialStream::open(&builder).map_err(|e| crate::flash::open_error(port, &e))?;
    stream
        .write_request_to_send(false)
        .map_err(|e| format!("Could not set RTS: {e}"))?;
    stream
        .write_data_terminal_ready(true)
        .map_err(|e| format!("Could not set DTR: {e}"))?;

    let seen = Arc::new(Mutex::new(Vec::new()));
    let tee = Tee {
        inner: stream,
        seen: seen.clone(),
    };
    let (mut rx, api) = StreamApi::new()
        .connect(StreamHandle::from_stream(tee))
        .await;
    let config_id: u32 = meshtastic::utils::generate_rand_id();
    let api = api
        .configure(config_id)
        .await
        .map_err(|e| format!("Could not send the config request: {e}"))?;

    let start = Instant::now();
    let mut report = NodeReport::default();
    let mut got_any = false;
    let mut timed_out = false;
    while !report.complete {
        let limit = if got_any {
            WHOLE_HANDSHAKE
        } else {
            FIRST_REPLY
        };
        let Some(wait) = limit.checked_sub(start.elapsed()) else {
            timed_out = true;
            break;
        };
        match tokio::time::timeout(wait, rx.recv()).await {
            Err(_) => {
                timed_out = true;
                break;
            }
            Ok(None) => break,
            Ok(Some(fr)) => {
                got_any = true;
                apply(&mut report, fr, config_id);
            }
        }
    }
    let _ = api.disconnect().await;

    if !got_any {
        return Err(format!(
            "No Meshtastic reply within {} s. The unit is not running Meshtastic, is still booting, or has serial disabled. Try the serial monitor to see what it prints.",
            FIRST_REPLY.as_secs()
        ));
    }
    if report.variant.is_none()
        && let Ok(bytes) = seen.lock()
    {
        report.variant = variant_from_text(&String::from_utf8_lossy(&bytes));
    }
    if report.variant.is_none()
        && let Some(hw) = &report.hw_model
    {
        report.variant = variant_from_hw(hw, report.long_name.as_deref());
    }
    if timed_out && !report.complete {
        crate::applog::log(
            "node info: handshake timed out before config_complete_id; showing partial data",
        );
    }
    Ok(report)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn variant_line_is_found_in_boot_text() {
        let text =
            "\x1b[34mINFO  | 00:00:01 1 Variant: TrekLink v4.0 (T-Beam Supreme)\r\nINFO | next";
        assert_eq!(
            variant_from_text(text).as_deref(),
            Some("TrekLink v4.0 (T-Beam Supreme)")
        );
        assert_eq!(variant_from_text("Variant: other"), None);
    }

    #[test]
    fn variant_is_inferred_from_hardware_model() {
        assert!(variant_from_hw("TBEAM", None).unwrap().starts_with("v3"));
        assert!(
            variant_from_hw("LILYGO_TBEAM_S3_CORE", None)
                .unwrap()
                .starts_with("v4")
        );
        assert!(
            variant_from_hw("PRIVATE_HW", Some("TrekLink 1a2b"))
                .unwrap()
                .contains("v1 or v2")
        );
        assert_eq!(variant_from_hw("HELTEC_V3", Some("Meshtastic 1a2b")), None);
    }

    #[test]
    fn handshake_packets_fill_the_report() {
        let mut r = NodeReport::default();
        let fr = |p| protobufs::FromRadio {
            payload_variant: Some(p),
            ..Default::default()
        };
        apply(
            &mut r,
            fr(from_radio::PayloadVariant::MyInfo(protobufs::MyNodeInfo {
                my_node_num: 0x1234_5678,
                ..Default::default()
            })),
            7,
        );
        apply(
            &mut r,
            fr(from_radio::PayloadVariant::NodeInfo(protobufs::NodeInfo {
                num: 0x1234_5678,
                user: Some(protobufs::User {
                    long_name: "TrekLink 5678".into(),
                    short_name: "5678".into(),
                    hw_model: protobufs::HardwareModel::Tbeam as i32,
                    ..Default::default()
                }),
                device_metrics: Some(protobufs::DeviceMetrics {
                    battery_level: Some(87),
                    voltage: Some(4.01),
                    ..Default::default()
                }),
                ..Default::default()
            })),
            7,
        );
        apply(
            &mut r,
            fr(from_radio::PayloadVariant::Metadata(
                protobufs::DeviceMetadata {
                    firmware_version: "2.7.26.abcdef".into(),
                    hw_model: protobufs::HardwareModel::Tbeam as i32,
                    ..Default::default()
                },
            )),
            7,
        );
        apply(
            &mut r,
            fr(from_radio::PayloadVariant::ConfigCompleteId(8)),
            7,
        );
        assert!(!r.complete);
        apply(
            &mut r,
            fr(from_radio::PayloadVariant::ConfigCompleteId(7)),
            7,
        );
        assert!(r.complete);
        assert_eq!(r.node_num, Some(0x1234_5678));
        assert_eq!(r.long_name.as_deref(), Some("TrekLink 5678"));
        assert_eq!(r.battery.as_deref(), Some("87% (4.01 V)"));
        assert_eq!(r.hw_model.as_deref(), Some("TBEAM"));
        assert_eq!(r.firmware.as_deref(), Some("2.7.26.abcdef"));
        assert_eq!(r.rows().len(), 8);
    }
}
