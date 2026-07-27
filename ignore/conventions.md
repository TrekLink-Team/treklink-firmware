# Codebase Conventions — TrekLink Firmware

> **Note**: Git, branching, and PR rules are documented in [commit-rules.md](./commit-rules.md). This document covers code-level conventions and firmware standards.

---

## 1. Project Architecture Overview

```
treklink-firmware/
├── platformio.ini         # PlatformIO project configuration & environment definitions
├── src/                   # Core application source code (.cpp / .h)
│   ├── main.cpp           # Main entry point (setup / loop)
│   └── mesh/              # Mesh networking & node protocols
├── boards/                # Board-specific variant definitions & pins
├── variants/              # Hardware target pinout definitions
├── protobufs/             # Protocol Buffer message definitions (.proto)
├── extra_scripts/         # PlatformIO custom python build scripts
├── data/                  # SPIFFS / LittleFS file system assets
├── test/                  # Unit and integration test suites
└── ignore/                # Project documentation & developer guidelines
    ├── AGENT.md           # AI Agent context & project specs
    ├── commit-rules.md    # Git branch & commit rules
    ├── conventions.md     # ← This file
    ├── development-rules.md # Build & CLI reference commands
    └── lifecycle.md       # Firmware development lifecycle
```

---

## 2. Firmware C / C++ Coding Standards

### 2.1 Code Formatting & Structure
- Standard: **C++17** for C++ files, **C11** for standard C headers/sources.
- Use explicit header guards or `#pragma once` at the top of every header file.
- Header order:
  1. System & STL headers (`<stdint.h>`, `<vector>`)
  2. Framework / Library headers (`<Arduino.h>`, `<RadioLib.h>`)
  3. Internal application headers (`"mesh/Node.h"`)

### 2.2 Naming Conventions
- **Files**: `snake_case` or `PascalCase` matching module names (e.g. `mesh_packet.cpp`, `MeshNode.h`).
- **Classes / Structs / Enums**: `PascalCase` (e.g., `PacketRouter`, `NodeStatus`).
- **Functions / Methods**: `camelCase` (e.g., `sendPacket()`, `processIncomingMessage()`).
- **Variables**: `camelCase` (e.g., `nodeId`, `rssiValue`).
- **Private Member Variables**: `_` suffix or `m_` prefix (e.g., `txPower_`, `m_connected`).
- **Constants / Macros**: `ALL_CAPS` with underscores (e.g., `MAX_RETRY_COUNT`, `DEFAULT_FREQUENCY`).

### 2.3 Memory Management & Safety
- **Avoid Dynamic Memory Allocation**: In embedded loops, minimize `malloc()`, `free()`, `new`, and `delete`. Prefer static or stack allocation to avoid memory fragmentation.
- **Buffer Bound Safety**: Use bounded string and memory operations (`snprintf`, `strncpy`, `memcpy` with size bounds). Never use `sprintf` or `strcpy`.
- **FreeRTOS Task Safety**: Protect shared resources accessed across tasks using FreeRTOS mutexes or semaphores. Use atomic primitives where appropriate.
- **Volatile Keyword**: Mark hardware register access or variables modified inside Interrupt Service Routines (ISRs) as `volatile`.

### 2.4 Error Handling & Logging
- **Return Status Codes**: Use enums or `bool` return types for functions that can fail. Avoid throwing C++ exceptions.
- **Logging**: Use structured logging macros (e.g., `LOG_DEBUG`, `LOG_INFO`, `LOG_ERROR`) rather than raw `Serial.print`.

---

## 3. Hardware & Platform Abstraction

- **Board Variant Isolation**: Hardware-specific pin mappings must be defined in `variants/` or `boards/` configuration files, never hardcoded inside core `src/` application logic.
- **Conditional Compilation**: Use feature macros (e.g., `#ifdef HAS_GPS`, `#ifdef HAS_LORA`) defined in environment configurations (`platformio.ini`).

---

## 4. Verification & Quality Gates

Before committing firmware code:
1. **Compilation**: Run `pio run` to verify clean compilation across all supported environment targets.
2. **Static Analysis**: Resolve warnings flagged by GCC / Clang during compilation.
3. **No Credential Leaks**: Never commit Wi-Fi passwords, API keys, or private keys to repository tracking.
