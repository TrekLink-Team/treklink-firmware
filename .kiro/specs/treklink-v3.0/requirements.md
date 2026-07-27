# TrekLink v3.0 Requirements Specification

> **Hardware:** COTS — LilyGo T-Beam V1.2 (ESP32 + SX1262/SX1278 + AXP192 PMU)  
> **Version:** 1.0  
> **Date:** June 11, 2026  
> **Status:** DRAFT — Awaiting Review

---

## Introduction

TrekLink v3.0 is a COTS (Commercial Off-The-Shelf) variant using the LilyGo T-Beam V1.2 development board. No custom hardware modifications are needed — v3.0 is a firmware-only overlay on the existing Meshtastic T-Beam build target that adds TrekLink branding, canned messages, regional defaults, and SOS functionality.

The T-Beam V1.2 has 3 physical buttons (Power, User, Reset), but only the **User button (GPIO 38)** is firmware-accessible — Power and Reset are hardware-only. SOS triggering must use a gesture-based 3-second hold on this single programmable button. The T-Beam V1.2 has no onboard IMU, so fall detection is **not available** on this variant.

---

## Requirements

### Requirement 1: Firmware Overlay Environment (REQ-V3-ENV-01)
**User Story:** As a developer, I want a TrekLink-branded T-Beam build environment that extends the stock T-Beam target without modifying it.

#### Acceptance Criteria
1. WHEN running `pio run -e treklink-v3-tbeam`, THEN the firmware SHALL compile for ESP32 using the T-Beam V1.2 board definition.
2. WHEN the build completes, THEN the binary SHALL include `-D TREKLINK_VARIANT -D TREKLINK_V3`.
3. WHEN building treklink-v3-tbeam, THEN the stock `env:tbeam` SHALL remain unmodified and independently compilable.
4. WHEN the firmware boots, THEN it SHALL use the stock T-Beam variant.h pin definitions (no hardware changes).

### Requirement 2: TrekLink Branding on T-Beam (REQ-V3-ID-01)
**User Story:** As a TrekLink user with a T-Beam device, I want the same TrekLink branding experience as custom PCB variants.

#### Acceptance Criteria
1. WHEN the device boots, THEN the OLED splash screen SHALL display "TrekLink" (not "Meshtastic").
2. WHEN the device advertises via Bluetooth, THEN the BLE name SHALL be "TrekLink_xxxx".
3. WHEN the device broadcasts NodeInfo, THEN the default long name SHALL be "TrekLink xxxx".
4. WHEN the device boots, THEN the LoRa region SHALL default to MY_433.
5. WHEN the device boots, THEN the timezone SHALL default to ICT-7 (GMT+7).

### Requirement 3: Single-Button SOS Gesture (REQ-V3-SOS-01)
**User Story:** As a T-Beam user, I want to trigger SOS by holding the single user button for 3 seconds, so that I have emergency capability without a dedicated SOS button.

#### Acceptance Criteria
1. WHEN the user holds the User Button (GPIO 38 — the only firmware-accessible button) for 3 seconds, THEN the system SHALL trigger the SOS routine (position broadcast + alarms).
2. WHEN the user short-presses the User Button, THEN the system SHALL pass the event to Meshtastic's normal screen navigation (no SOS action).
3. WHEN SOS is active AND the user holds the User Button for 3 seconds, THEN the system SHALL cancel the SOS.
4. WHILE the SOS gesture module monitors the button, THEN it SHALL NOT interfere with Meshtastic's OneButton/InputBroker handling of short presses and double presses.
5. IF a fall detection pre-alarm is active, THEN any button press SHALL cancel the pre-alarm (not applicable on v3.0 — no IMU).

> **Hardware Note:** T-Beam V1.2 has 3 physical buttons total (Power, User/Program, Reset). Power and Reset are hardware-only — not routable to GPIO. Only the User button (middle position, GPIO 38) is available to firmware.

### Requirement 4: No Fall Detection (REQ-V3-FALL-01)
**User Story:** As a developer, I want the build to clearly exclude fall detection on T-Beam since there is no onboard IMU.

#### Acceptance Criteria
1. WHEN building for treklink-v3-tbeam, THEN the FallDetectionModule SHALL NOT be compiled or instantiated.
2. WHEN the device boots, THEN it SHALL log "FallDetection: Disabled (no IMU on v3.0)".

### Requirement 5: Canned Messages (REQ-V3-MSG-01)
**User Story:** As a SAR operator with a T-Beam, I want the same default canned message set as other TrekLink variants.

#### Acceptance Criteria
1. WHEN the device boots with default settings, THEN the canned message list SHALL match the TrekLink standard set defined in v1.0 REQ-MSG-02.
2. WHEN the user accesses canned messages, THEN navigation SHALL use the T-Beam's single button and Meshtastic's standard CannedMessage UI flow.
