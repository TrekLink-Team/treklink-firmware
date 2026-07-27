# AGENT.md — TrekLink Firmware Context

## 1. Developer Identity & Project Scope

| Attribute | Value |
|-----------|-------|
| Organization | **TrekLink-Team** |
| Repository | `treklink-firmware` |
| Primary Branches | `main` (production), `dev` (integration) |
| Architecture | ESP32 / NRF52 Embedded Firmware (C++17 / PlatformIO) |
| Core Modules | LoRa Mesh Networking, BLE communication, GPS/GNSS tracking, Protobuf serialization |

---

## 2. Repository Structure

```
treklink-firmware/
├── platformio.ini         # PlatformIO project configuration & environments
├── src/                   # Source code (.cpp, .h)
│   ├── main.cpp           # Main loop & setup
│   └── mesh/              # Mesh networking logic
├── boards/                # Board definitions
├── variants/              # Pinout definitions per hardware target
├── protobufs/             # Protobuf definitions
├── extra_scripts/         # PlatformIO custom build scripts
├── data/                  # SPIFFS assets
└── ignore/                # Project documentation & guidelines
    ├── AGENT.md           # This file — project context
    ├── commit-rules.md    # Git branch & commit standards
    ├── conventions.md     # C++ / Firmware coding conventions
    ├── development-rules.md # PlatformIO build & CLI commands
    └── lifecycle.md       # Development lifecycle & workflow
```

---

## 3. Architecture & Standards

- **Build System**: PlatformIO Core (`pio run`, `pio test`).
- **Language Standards**: C++17 / C11.
- **Threading Model**: FreeRTOS tasks & queue messaging.
- **Git Standards**: Conventional Commits, `dev` integration branch, Pull Requests via GitHub.

---

## 4. Key References & URLs

- **GitHub Org**: https://github.com/TrekLink-Team
- **Repository**: https://github.com/TrekLink-Team/treklink-firmware
- **Target Branch**: `dev`
