# TrekLink

> **Meshtastic-Powered Off-Grid Search & Rescue (SAR) Firmware**  
> Unified PlatformIO pipeline for multi-generational TrekLink devices (v1.0, v2.0, v3.0, v4.0)

TrekLink is a custom branded firmware fork of [Meshtastic](https://meshtastic.org/) optimized for Search & Rescue (SAR) operations and backcountry expeditions. It adds custom branding, a unified SOS emergency trigger/cancel system, IMU-based fall detection, regional defaults, and optimized canned messages.

[![License: GPL-3.0](https://img.shields.io/badge/License-GPL%203.0-blue.svg)](LICENSE)
[![Platform: ESP32/ESP32-S3](https://img.shields.io/badge/Platform-ESP32%20%2F%20ESP32--S3-blue.svg)](https://www.espressif.com/)
[![Meshtastic: 2.7.19](https://img.shields.io/badge/Meshtastic-2.7.19-green.svg)](https://meshtastic.org/)
[![Radio: 433MHz](https://img.shields.io/badge/Radio-433MHz%20LoRa-orange.svg)]()

---

## 📱 Hardware Variants

TrekLink supports four device generations deployed across custom hardware and commercial off-the-shelf (COTS) hardware. The full matrix, with pins, MQTT support and test status, is [docs/variants.md](docs/variants.md).

| Variant                          | Hardware Platform     | MCU      | IMU Sensor | SOS Trigger Method            | Buzzer |
| -------------------------------- | --------------------- | -------- | ---------- | ----------------------------- | ------ |
| **v1.0** (deprecated, mesh only) | Perfboard prototype   | ESP32    | MPU6050    | Dedicated Button (GPIO 34)    | Yes    |
| **v2.0**                         | Custom PCB            | ESP32-S3 | ICM-20948  | Dedicated Button (GPIO 4)     | Yes    |
| **v3.0**                         | LilyGo T-Beam v1.2    | ESP32    | _None_     | 3s User Button Hold (GPIO 38) | No     |
| **v4.0**                         | LilyGo T-Beam Supreme | ESP32-S3 | QMI8658    | 3s User Button Hold (GPIO 0)  | No     |

---

## 🚀 Quick Start & Build Targets

Firmware builds are managed via PlatformIO. To avoid LittleFS compilation issues on standard toolchains, use the `-t buildprog` target to build the firmware binary alone.

```bash
# Clone the repository
git clone https://github.com/TrekLink-Team/treklink-firmware.git
cd treklink-firmware

# Build specific TrekLink variants
pio run -e treklink -t buildprog            # v1.0 ESP32 perfboard (deprecated)
pio run -e treklink-v2 -t buildprog         # v2.0 Custom ESP32-S3 PCB
pio run -e treklink-v3-tbeam -t buildprog   # v3.0 LilyGo T-Beam v1.2
pio run -e treklink-v4-supreme -t buildprog # v4.0 LilyGo T-Beam Supreme

# Flash to device (e.g. v4.0)
pio run -e treklink-v4-supreme -t upload

# Verify stock Meshtastic regression targets compile cleanly
pio run -e tbeam -t buildprog
pio run -e tbeam-s3-core -t buildprog
```

---

## ✨ Features

- **Branding Integration:** Auto-applies "TrekLink" node naming prefix, BLE name, and splash screen logo to distinguish SAR devices.
- **Centralized SOS System (`TrekLinkSOSHelper`):** Central state machine handles triggering/canceling emergency status, broadcasting GPS/text packets, and driving buzzers/vibrators.
- **IMU-Agnostic Fall Detection:** Unified `FallDetectionModule` utilizing a common `FallSensorInterface`. Supports `MPU6050`, `ICM20948`, and `QMI8658` (via `SensorLib`).
- **Gesture SOS (`TrekLinkSOSGesture`):** Polls single-button COTS hardware (v3.0/v4.0) for a 3-second hold to trigger/cancel emergency mode, leaving short presses unhindered for screen navigation.
- **Strict Compile-Time Safety (`TrekLinkVariantValidation`):** Static assert system prevents flashing mismatching configurations or running multiple variant flags simultaneously.
- **Local Defaults:** Auto-configured to Malaysia/Vietnam 433MHz region (`MY_433`), timezone `ICT-7`, and pre-loaded with Search & Rescue brevity canned messages.

---

## 🏗️ Project Structure

```
treklink-firmware/
├── variants/
│   ├── esp32/
│   │   ├── treklink_v1_0/          # v1.0 variant overlay & build flags
│   │   └── treklink_v3_tbeam/      # v3.0 T-Beam v1.2 overlay
│   └── esp32s3/
│       ├── treklink_v2_0/          # v2.0 Custom PCB S3 overlay
│       └── treklink_v4_supreme/    # v4.0 T-Beam Supreme S3 overlay
├── src/
│   ├── modules/
│   │   ├── sensors/
│   │   │   ├── MPU6050FallSensor   # v1.0 IMU adapter
│   │   │   ├── ICM20948FallSensor  # v2.0 IMU adapter
│   │   │   └── QMI8658FallSensor   # v4.0 IMU adapter
│   │   ├── FallSensorInterface.h   # Sensor abstraction layer
│   │   ├── FallDetectionModule     # Thread for fall logic & alarm
│   │   ├── TrekLinkSOSHelper       # Centralized SOS state & actions
│   │   ├── TrekLinkButtonModule    # Dedicated button handler (v1.0/v2.0)
│   │   ├── TrekLinkSOSGesture      # Single button gesture handler (v3.0/v4.0)
│   │   ├── BuzzerManager.h         # Shared thread-safe buzzer singleton
│   │   └── TrekLinkVariantValidation.h # Compile-time configuration check
│   └── main.cpp                    # BLE name and early init
├── docs/                           # Firmware documentation index: docs/README.md
├── specs/                          # Specs per variant and per module: specs/README.md
├── platformio.ini                  # Core build environment settings
└── README.md                       # This file
```

---

## 🧪 Verification & Boot Logs

When a device boots, it checks the configured hardware profile and prints the active variant to the serial monitor:

```
[INFO] Variant: TrekLink v4.0 (T-Beam Supreme)
[INFO] TrekLink SOS Gesture Module initialized (BUTTON_PIN=0)
[INFO] TrekLink Fall Detection Module initialized (QMI8658)
```

Pairing via the mobile app will advertise the BLE name as `TrekLink_XXXX` (based on MAC address).

---

## 🔌 Flashing

Flashing procedures for every variant, including the v2.0 power-latch caution and the LilyGo
bootloader sequences, are in [docs/build-and-flash.md](docs/build-and-flash.md#flashing).

---

## 📜 License

- **TrekLink Custom Additions:** MIT License
- **Meshtastic Core Codebase:** GPL-3.0 License

---

## 📞 Support & Resources

- **Issues:** [GitHub Issues](https://github.com/TrekLink-Team/treklink-firmware/issues)
- **Meshtastic Discord:** [Join Community](https://discord.gg/meshtastic)
- **Documentation:** [Meshtastic Docs](https://meshtastic.org/docs/)

---

**Happy Meshing!** 📡🏔️
