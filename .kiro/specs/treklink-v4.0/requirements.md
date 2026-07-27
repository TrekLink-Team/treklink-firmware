# TrekLink v4.0 Requirements Specification

> **Hardware:** COTS — LilyGo T-Beam Supreme (ESP32-S3 + SX1262/LR1121 + AXP2101 PMU + QMI8658 IMU)  
> **Version:** 1.0  
> **Date:** June 11, 2026  
> **Status:** DRAFT — Awaiting Review

---

## Introduction

TrekLink v4.0 is a COTS variant using the LilyGo T-Beam Supreme (tbeam-s3-core). Like v3.0, it is a firmware-only overlay requiring no hardware modifications. Unlike v3.0, the T-Beam Supreme includes an onboard QMI8658 6-axis IMU, enabling **fall detection** on this variant.

The T-Beam Supreme has 3 physical buttons (Power, User, Reset), but only the **User button (GPIO 0)** is firmware-accessible — Power and Reset are hardware-only. SOS triggering uses the same gesture-based 3-second hold as v3.0. The key differentiator is IMU-based fall detection via the `QMI8658FallSensor` adapter.

---

## Requirements

### Requirement 1: Firmware Overlay Environment (REQ-V4-ENV-01)
**User Story:** As a developer, I want a TrekLink-branded T-Beam Supreme build environment that extends the stock target.

#### Acceptance Criteria
1. WHEN running `pio run -e treklink-v4-supreme`, THEN the firmware SHALL compile for ESP32-S3.
2. WHEN the build completes, THEN the binary SHALL include `-D TREKLINK_VARIANT -D TREKLINK_V4`.
3. WHEN building treklink-v4-supreme, THEN the stock `env:tbeam-s3-core` SHALL remain unmodified.

### Requirement 2: TrekLink Branding on T-Beam Supreme (REQ-V4-ID-01)
**User Story:** As a TrekLink user with a T-Beam Supreme, I want the same TrekLink branding as other variants.

#### Acceptance Criteria
1. WHEN the device boots, THEN the OLED splash screen SHALL display "TrekLink".
2. WHEN the device advertises via Bluetooth, THEN the BLE name SHALL be "TrekLink_xxxx".
3. WHEN the device boots, THEN the LoRa region SHALL default to MY_433.
4. WHEN the device boots, THEN the timezone SHALL default to ICT-7 (GMT+7).

### Requirement 3: Single-Button SOS Gesture (REQ-V4-SOS-01)
**User Story:** As a T-Beam Supreme user, I want to trigger SOS by holding the user button for 3 seconds.

#### Acceptance Criteria
1. WHEN the user holds the User Button (GPIO 0 — the only firmware-accessible button) for 3 seconds, THEN the system SHALL trigger the SOS routine.
2. WHEN the user short-presses the User Button, THEN the system SHALL pass the event to normal screen navigation.
3. WHEN SOS is active AND the user holds the button for 3 seconds, THEN the system SHALL cancel the SOS.
4. WHILE the gesture module monitors the button, THEN it SHALL NOT interfere with Meshtastic's OneButton handling of short presses and double presses.

> **Hardware Note:** T-Beam Supreme has 3 physical buttons total (User/Program, Power, Reset). Power (middle) and Reset (right) are hardware-only — not routable to GPIO. Only the User button (left position, GPIO 0) is available to firmware.

### Requirement 4: QMI8658 Fall Detection (REQ-V4-FALL-01)
**User Story:** As a hiker with a T-Beam Supreme, I want fall detection using the onboard QMI8658 IMU.

#### Acceptance Criteria
1. WHEN the firmware initializes fall detection on v4.0, THEN it SHALL use the QMI8658 sensor driver (via SensorLib).
2. WHEN the QMI8658 provides accelerometer data, THEN the system SHALL normalize readings to g-force compatible with existing fall detection thresholds.
3. WHEN the QMI8658 provides gyroscope data, THEN the system SHALL normalize readings to rad/s compatible with inactivity detection.
4. IF the QMI8658 is not detected on I2C, THEN the firmware SHALL log a warning and disable fall detection.
5. WHEN a fall is detected, THEN the pre-alarm and auto-SOS behavior SHALL be identical to v1.0/v2.0.

### Requirement 5: Canned Messages (REQ-V4-MSG-01)
**User Story:** As a SAR operator with a T-Beam Supreme, I want the same default canned message set.

#### Acceptance Criteria
1. WHEN the device boots with default settings, THEN the canned message list SHALL match the TrekLink standard set.
