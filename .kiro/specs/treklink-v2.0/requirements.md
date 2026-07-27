# TrekLink v2.0 Requirements Specification

> **Hardware:** Custom PCB — ESP32-S3 + SX1268 433MHz + ICM-20948 IMU  
> **Version:** 1.0  
> **Date:** June 11, 2026  
> **Status:** DRAFT — Awaiting Review

> [!CAUTION]
> **CRITICAL, must attention!**
> **Hardware Caution Note (v2.0 Custom PCB ONLY):**
> Due to a known hardware design limitation in the v2.0 custom PCB, the power latch circuit cannot maintain power until the firmware drives GPIO 9 high. Additionally, Linux `ModemManager` probes native ESP32-S3 USB CDC on boot, toggling DTR/RTS and inducing a reset loop.
> **Flashing/Recovery Boot Sequence:**
> 1. Stop ModemManager on the host PC: `sudo systemctl stop ModemManager`
> 2. Select the appropriate sequence:
>    - **If brand new (unflashed):** Press and hold both `POWER` and `SELECT` (GPIO 0) simultaneously, then immediately click `RESET`. Release `SELECT`, keeping `POWER` held. Clamp the `POWER` button to hold it down physically, then release manual pressure. Run: `pio run -e treklink-v2 -t upload` (keep clamp on until completed).
>    - **If already flashed (reflashing):** Clamp the `POWER` button physically *before* starting the upload. (Erasing/writing the flash drops the GPIO 9 firmware latch instantly, causing a power cut unless held down physically). Run: `pio run -e treklink-v2 -t upload` (keep clamp on until completed).
> 3. Release power (remove clamp) and verify that the device stays on automatically (via GPIO 9 latch) and the OLED displays the UI.

---

## Introduction

TrekLink v2.0 is the first custom-designed PCB variant, replacing the v1.0 perfboard prototype. It migrates from ESP32-WROOM-32 to ESP32-S3, from SX1278 (Ra-02) to SX1268, and from MPU6050 to ICM-20948 IMU. The hardware pin assignments are frozen per the JLCPCB manufacturing order. This specification defines requirements unique to the v2.0 hardware that differentiate it from v1.0.

All v1.0 base requirements (mesh protocol, encryption, canned messages, branding, fall detection, SOS behavior) carry forward unchanged. This document only covers **new or changed** requirements specific to the v2.0 hardware.

---

## Requirements

### Requirement 1: ESP32-S3 Variant Definition (REQ-V2-HW-01)
**User Story:** As a firmware developer, I want the v2.0 custom PCB fully defined as a Meshtastic variant, so that the firmware compiles and runs correctly on the new hardware.

#### Acceptance Criteria
1. WHEN the firmware is compiled for TrekLink v2.0, THEN the build system SHALL target ESP32-S3 (not ESP32-WROOM-32).
2. WHEN the firmware boots on v2.0 hardware, THEN the system SHALL initialize I2C on SDA=5, SCL=6.
3. WHEN the firmware boots, THEN the system SHALL initialize GPS UART on RX=16, TX=17.
4. WHEN the firmware boots, THEN the system SHALL initialize SX1268 LoRa SPI with SCK=21, MOSI=38, MISO=39, CS=14, DIO1=42, BUSY=41, NRST=40, RXEN=43.
5. WHEN the firmware boots, THEN the system SHALL recognize buttons at SELECT=0, SOS=4, UP=7, DOWN=8.
6. WHEN the firmware boots, THEN the system SHALL initialize peripherals at BUZZER=11, VIBRATOR=12, LED=2.
7. WHEN the firmware boots, THEN the system SHALL read battery voltage from ADC on GPIO 1.

### Requirement 2: Power Latch Circuit (REQ-V2-PWR-01)
**User Story:** As a user, I want the device to stay powered on after I press the power button and release it, so that I don't have to hold the button continuously.

#### Acceptance Criteria
1. WHEN the device powers on (cold start from power button press), THEN the firmware SHALL set GPIO 9 (POWER_LATCH) HIGH within the first 100ms to latch power on.
2. WHILE the device is running, THEN GPIO 9 SHALL remain HIGH to maintain power.
3. WHEN a software shutdown is requested (e.g., long-press menu option), THEN the firmware SHALL set GPIO 9 LOW to cut power.
4. IF GPIO 9 is not set HIGH during boot, THEN the device SHALL lose power when the user releases the power button.

### Requirement 3: Power Sense Input (REQ-V2-PWR-02)
**User Story:** As a developer, I want to detect when the power button is released after cold start, so that the firmware can distinguish cold boot from reset.

#### Acceptance Criteria
1. WHEN the device boots, THEN the firmware SHALL read GPIO 13 (POWER_SENSE) as an Active-Low input.
2. IF GPIO 13 reads LOW, THEN the power button is currently pressed (cold start in progress).
3. IF GPIO 13 reads HIGH after boot, THEN the power button has been released (normal operation).

### Requirement 4: GPS Power Enable (REQ-V2-GPS-01)
**User Story:** As a user, I want the GPS module to power on at boot, so that I get a position fix without manual intervention.

#### Acceptance Criteria
1. WHEN the device boots, THEN the firmware SHALL set GPIO 15 (GPS_PWR_EN) HIGH to enable GPS power.
2. WHILE GPS is required (normal operation), THEN GPIO 15 SHALL remain HIGH.
3. WHEN Meshtastic enters GPS sleep mode, THEN the firmware MAY set GPIO 15 LOW to save power (respecting hot-start backup battery).

### Requirement 5: SX1268 Radio Configuration (REQ-V2-RF-01)
**User Story:** As a user, I want the v2.0 device to use the improved SX1268 radio for better range and lower power consumption.

#### Acceptance Criteria
1. WHEN the firmware initializes the radio, THEN it SHALL use the SX1268 driver (not SX1278/RF95).
2. WHEN the radio is configured, THEN it SHALL use RXEN on GPIO 43 for the external RF switch.
3. WHEN the firmware boots, THEN the LoRa region SHALL default to MY_433 (433.0-435.0 MHz, 20 dBm).
4. IF the SX1268 fails to initialize, THEN the firmware SHALL log an error and continue booting (degraded mode).

### Requirement 6: ICM-20948 IMU Integration (REQ-V2-IMU-01)
**User Story:** As a user, I want fall detection to work with the v2.0's ICM-20948 IMU (replacing the v1.0 MPU6050).

#### Acceptance Criteria
1. WHEN the firmware initializes fall detection on v2.0 hardware, THEN it SHALL use the ICM-20948 sensor driver (not Adafruit_MPU6050).
2. WHEN the ICM-20948 provides accelerometer data, THEN the system SHALL normalize readings to g-force units compatible with the existing fall detection thresholds.
3. WHEN the ICM-20948 provides gyroscope data, THEN the system SHALL normalize readings to rad/s compatible with the existing inactivity detection.
4. IF the ICM-20948 is not detected on I2C, THEN the firmware SHALL log a warning and disable fall detection.

### Requirement 7: Button Pin Remapping (REQ-V2-BTN-01)
**User Story:** As a developer, I want the button GPIO assignments to match the v2.0 PCB layout without code changes to button logic.

#### Acceptance Criteria
1. WHEN building for v2.0, THEN the SOS button SHALL be mapped to GPIO 4 (not GPIO 34 as in v1.0).
2. WHEN building for v2.0, THEN the UP button SHALL be mapped to GPIO 7 (not GPIO 32).
3. WHEN building for v2.0, THEN the DOWN button SHALL be mapped to GPIO 8 (not GPIO 35).
4. WHEN building for v2.0, THEN the SELECT/MENU button SHALL be mapped to GPIO 0 (not GPIO 25).
5. WHEN configuring v2.0 button GPIOs, THEN the firmware SHALL use INPUT_PULLUP mode (v2.0 GPIOs support internal pull-ups, unlike v1.0 input-only GPIOs 34/35).

### Requirement 8: PlatformIO Build Environment (REQ-V2-ENV-01)
**User Story:** As a developer, I want a dedicated PlatformIO build environment for v2.0, so that I can compile and flash firmware specifically for the custom PCB.

#### Acceptance Criteria
1. WHEN running `pio run -e treklink-v2`, THEN the firmware SHALL compile successfully for ESP32-S3.
2. WHEN the build completes, THEN the binary SHALL include `-D TREKLINK_VARIANT -D TREKLINK_V2` compile flags.
3. WHEN flashing the v2.0 firmware, THEN it SHALL NOT break v1.0 firmware compilation (`pio run -e treklink` SHALL still succeed).
