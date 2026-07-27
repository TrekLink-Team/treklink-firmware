# TrekLink v3.0 Implementation Tasks

> **Scope:** T-Beam V1.2 COTS overlay — branding, SOS gesture, regional defaults  
> **Status:** DRAFT — Awaiting Review

---

- [x] 1. Create `treklink_v3_tbeam` overlay directory and files
  - Create `variants/esp32/treklink_v3_tbeam/variant.h` that includes stock `../tbeam/variant.h`
  - Add TrekLink defines: `TREKLINK_VARIANT`, region `MY_433`, timezone `ICT-7`, canned messages
  - Do NOT define `BUTTON_PIN_SOS` (triggers gesture-based SOS in multi-variant module)
  - _Requirements: REQ-V3-ENV-01, REQ-V3-ID-01_

- [x] 2. Create `treklink-v3-tbeam` PlatformIO environment
  - Create `variants/esp32/treklink_v3_tbeam/platformio.ini`
  - `[env:treklink-v3-tbeam]` extends `env:tbeam`
  - Build flags: `-D TREKLINK_VARIANT -D TREKLINK_V3 -I variants/esp32/treklink_v3_tbeam`
  - Verify stock `env:tbeam` still compiles independently
  - _Requirements: REQ-V3-ENV-01_

- [x] 3. Verify branding activates on T-Beam
  - Confirm `TREKLINK_VARIANT` flag triggers all branding code paths:
    - Splash screen: `UIRenderer.cpp` → "TrekLink"
    - BLE name: `main.cpp` → "TrekLink_xxxx"
    - Node name: `NodeDB.cpp` → "TrekLink xxxx"
  - No code changes needed if `TREKLINK_VARIANT` is already wired (verify only)
  - _Requirements: REQ-V3-ID-01_

- [x] 4. Verify fall detection exclusion
  - Confirm `Modules.cpp` does NOT instantiate `FallDetectionModule` when no IMU is present
  - Add explicit v3.0 log message: "FallDetection: Disabled (no IMU on v3.0)"
  - _Requirements: REQ-V3-FALL-01_

- [x] 5. Verify v3.0 environment compiles
  - Run `pio run -e treklink-v3-tbeam` — must compile for ESP32
  - Run `pio run -e tbeam` — stock T-Beam regression
  - _Requirements: REQ-V3-ENV-01_
