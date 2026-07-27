# TrekLink v4.0 Implementation Tasks

> **Scope:** T-Beam Supreme COTS overlay — branding, SOS gesture, QMI8658 fall detection  
> **Status:** DRAFT — Awaiting Review

---

- [x] 1. Create `treklink_v4_supreme` overlay directory and files
  - Create `variants/esp32s3/treklink_v4_supreme/variant.h` that includes stock `../tbeam-s3-core/variant.h`
  - Add TrekLink defines: `TREKLINK_VARIANT`, region `MY_433`, timezone `ICT-7`, canned messages
  - Do NOT define `BUTTON_PIN_SOS` (triggers gesture-based SOS)
  - _Requirements: REQ-V4-ENV-01, REQ-V4-ID-01_

- [x] 2. Create `treklink-v4-supreme` PlatformIO environment
  - Create `variants/esp32s3/treklink_v4_supreme/platformio.ini`
  - `[env:treklink-v4-supreme]` extends `env:tbeam-s3-core`
  - Build flags: `-D TREKLINK_VARIANT -D TREKLINK_V4 -I variants/esp32s3/treklink_v4_supreme`
  - Verify stock `env:tbeam-s3-core` still compiles independently
  - _Requirements: REQ-V4-ENV-01_

- [x] 3. Implement `QMI8658FallSensor` adapter
  - New file: `src/modules/sensors/QMI8658FallSensor.h/.cpp`
  - Implement `FallSensorInterface` using SensorLib's QMI8658 driver
  - Map QMI8658 accel to g-force, gyro to rad/s
  - Use T-Beam Supreme I2C bus (SDA=17, SCL=18)
  - Guard with `#ifdef TREKLINK_V4` or SensorLib availability check
  - _Requirements: REQ-V4-FALL-01_

- [x] 4. Wire QMI8658 sensor in `Modules.cpp`
  - In `#ifdef TREKLINK_V4` block: `new FallDetectionModule(new QMI8658FallSensor())`
  - Verify fall detection state machine works with QMI8658 data
  - _Requirements: REQ-V4-FALL-01_

- [x] 5. Verify branding activates on T-Beam Supreme
  - Same verification as v3.0: splash screen, BLE name, node name
  - _Requirements: REQ-V4-ID-01_

- [x] 6. Verify v4.0 environment compiles
  - Run `pio run -e treklink-v4-supreme` — must compile for ESP32-S3
  - Run `pio run -e tbeam-s3-core` — stock regression
  - _Requirements: REQ-V4-ENV-01_

