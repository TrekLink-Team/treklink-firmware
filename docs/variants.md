---
id: variants
type: reference
status: approved
owner: KhoaDD
summary: One-page matrix of the four TrekLink hardware variants, as the code builds them.
verified: {commit: ba1cd1d, date: 2026-10-31, scope: "env names, env files and capabilities on the overlay branch"}
---

# TrekLink hardware variants

This page is the single source of truth for what "v1" to "v4" mean in this repository. Since the
2026-10-31 overlay move (`specs/architecture`), each active variant's parts, pins and capabilities are
listed in `specs/variants/<vN>/components.md`; this page is the summary across them. v3 and v4
are the LilyGo off-the-shelf boards (T-Beam v1.2 and T-Beam Supreme), confirmed by the leader on
2026-10-29. The older hardware-tier roadmap that used v3 and v4 for custom pre-production PCBs is
kept in [`.deprecated/2026-10-29-hardware-tier-roadmap/`](.deprecated/2026-10-29-hardware-tier-roadmap/).

Wire-format facts (packets, priorities, portnums, MQTT topics) are not repeated here. They live in
`treklink-docs/_docs/00-project-context/04-firmware-ground-truth.md`.

## Matrix

| | v1 | v2 | v3 | v4 |
|---|---|---|---|---|
| Hardware | Perfboard prototype, ESP32-WROOM-32, Ra-02 SX1278 | Custom 4-layer PCB, ESP32-S3, SX1268 | LilyGo T-Beam v1.2, ESP32 | LilyGo T-Beam Supreme, ESP32-S3 |
| PlatformIO env | `treklink` (old history only) | `treklink-v2` | `treklink-v3` | `treklink-v4` |
| Env file | not ported | `variants/esp32s3/treklink_v2/platformio.ini` | `variants/esp32/treklink_v3/platformio.ini` | `variants/esp32s3/treklink_v4/platformio.ini` |
| Base | `esp32_base`, board `treklink-esp32` | `esp32s3_base`, board `esp32-s3-devkitc-1` | extends stock `env:tbeam` | extends stock `env:tbeam-s3-core` |
| Capabilities (`specs/architecture` design §3.2) | not applicable | `TREKLINK_SOS_BUTTON_PIN 4`, buzzer, vibrator, LED, `TREKLINK_FALL_SENSOR_ICM20948` | LED `LED_POWER` 4 | `TREKLINK_FALL_SENSOR_QMI8658` |
| SOS trigger | dedicated button, GPIO 34 (`treklink_v1_0/variant.h:30`) | dedicated button, GPIO 4 (`treklink_v2_0/variant.h:36`) | 3 s hold on the user button, GPIO 38 (`tbeam/variant.h:6`) | 3 s hold on the user button, GPIO 0 (`tbeam-s3-core/variant.h:9`) |
| SOS module | `TrekLinkButtonModule` | `TrekLinkButtonModule` | `TrekLinkSOSGesture` | `TrekLinkSOSGesture` |
| Local SOS alarm | buzzer GPIO 33, vibrator GPIO 4 | buzzer GPIO 11, vibrator GPIO 12 | LED GPIO 4 blinks; no buzzer, no vibrator | none: no buzzer, vibrator or `LED_PIN` |
| Fall detection IMU | MPU6050 | ICM-20948 | none, compiled out | QMI8658 |
| MQTT uplink | compiled out (`MESHTASTIC_EXCLUDE_MQTT=1`, `treklink_v1_0/platformio.ini:10`) | yes | yes | yes |
| Onboard queue (`specs/onboard-queue`) | no | yes, 1000 entries | yes, `TrekLinkQueueConfig.h` defaults | yes, 1000 entries |
| Support status | **deprecated**: kept as a mesh node, not catered for by treklink-web | active | active | active |
| Hardware SOS test | not tested this term | 2026-10-29 run at `ea37a06`: trigger works, buzzer silent after any stock beep, cancel local only, fall detection cannot trigger (see [test runs](test-runs/README.md)) | not tested | not tested |
| Spec | [specs/variants/v1](../specs/variants/v1/) | [specs/variants/v2](../specs/variants/v2/) | [specs/variants/v3](../specs/variants/v3/) | [specs/variants/v4](../specs/variants/v4/) |

Sources for the module and IMU rows: `treklink/src/TrekLink.cpp` (`treklink::start()`), which creates
the SOS and fall modules from the capabilities each `variant.h` declares; `treklink/src/Capabilities.h`
stops the build on a contradictory set. The alarm row follows from `TrekLinkSOSHelper::activateAlarms()`
(`treklink/src/sos/TrekLinkSOSHelper.cpp`), which drives only the pins a variant defines, and from the
LED strobe on `TREKLINK_SOS_LED_PIN`. The v1 column describes the old history (tag `archive/pre-overlay`).

## Known gaps between the specs and the code

1. **v3 and v4 promise alarms the boards cannot drive.** REQ-V3-SOS-01.1 and REQ-V4-SOS-01 say the
   hold triggers "position broadcast + alarms". The T-Beam variants define no buzzer or vibrator, so
   the only local feedback is the v3 LED and the screen banner.
2. **The v3 and v4 SOS hold shares its button with stock input handling.** Stock `InputBroker`
   maps the same pin to SELECT after a 500 ms hold and to SHUTDOWN on a long-long press
   (`src/input/InputBroker.cpp:305-307`). Details and status: firmware ground truth §2.
3. **Cancelling an SOS transmits nothing.** `TrekLinkSOSHelper::cancelSOS()` only silences local
   alarms (`TrekLinkSOSHelper.cpp:49-53`), so neither a cancel nor a unit stuck in SOS can be seen
   from the platform. A hardware test of cancel needs the serial log or the device itself.

4. **v2 fall detection cannot reach the pre-alarm, and v2 SOS can lose its buzzer.** Measured on
   2026-10-29: the stillness check starts at impact and cancels at once, the shared IMU sleeps while
   the screen is off, and any stock beep detaches the buzzer from `BuzzerManager`. Details in
   [test-runs/2026-10-29-v2-hardware-run.md](test-runs/2026-10-29-v2-hardware-run.md).
5. **v2 has no serial console on USB.** The `esp32-s3-devkitc-1` board leaves `Serial` on UART0, so
   v2 logs and the client API are not reachable over USB without `-DARDUINO_USB_CDC_ON_BOOT=1`.
6. **v4 fall detection probes its IMU on the wrong bus.** `QMI8658FallSensor::init()` uses I2C at
   0x6B (`src/modules/sensors/QMI8658FallSensor.cpp:25-33`); on the T-Beam Supreme the QMI8658 is on
   SPI (CS GPIO 34, INT GPIO 33) per LilyGo's hardware documentation. Not hardware-verified.

Code fixes for these are out of scope for documentation work; each needs a `fix/*` branch after a
hardware reproduction.
