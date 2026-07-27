# TrekLink v3.0 Design Document

> **Hardware:** COTS — LilyGo T-Beam V1.2  
> **Version:** 1.0  
> **Date:** June 11, 2026  
> **Status:** DRAFT — Awaiting Review

---

## 1. Overview

v3.0 is a pure firmware overlay on the stock Meshtastic T-Beam target. It adds TrekLink branding, SOS gesture, and regional defaults without modifying any T-Beam hardware pins or base configuration.

**Design Principle:** The overlay extends the base T-Beam environment using PlatformIO environment inheritance. A minimal `variant.h` overlay re-exports the stock T-Beam pins and adds TrekLink-specific defines.

---

## 2. Architecture

### 2.1 Overlay File Structure

```
variants/esp32/treklink_v3_tbeam/
├── variant.h          # Includes stock tbeam/variant.h + TrekLink defines
└── platformio.ini     # [env:treklink-v3-tbeam] extends env:tbeam
```

### 2.2 Include Path Strategy

The overlay `variant.h` uses the `#include` directive to pull in the stock T-Beam variant, then appends TrekLink defines:

```cpp
// variants/esp32/treklink_v3_tbeam/variant.h
#pragma once

// Import full stock T-Beam V1.2 pin definitions
#include "../tbeam/variant.h"

// TrekLink overlay defines
#ifndef TREKLINK_VARIANT
#define TREKLINK_VARIANT
#endif

// Regional defaults
#define REGULATORY_LORA_REGIONCODE meshtastic_Config_LoRaConfig_RegionCode_MY_433
#define DEFAULT_TIMEZONE "ICT-7"

// Canned messages
#define CANNED_MESSAGE_MODULE_MESSAGES_DEFAULT \
    "MEDICAL-HELP|LOST-HELP|SAFE|EVAC|LOW BAT|COMING|FOUND|LOST|" \
    "OK|NO|CHECK?|STOP|GO|WAIT ME|COME2ME|TURNBACK|MEET AT|REQST|" \
    "N|S|E|W|R|L|TRAIL|CHKPT|CAMP|DANGR|LO|HI|SPPLY|FD|WTER"
```

**PlatformIO include path**: `-I variants/esp32/treklink_v3_tbeam` is placed **before** the T-Beam include path, so the overlay `variant.h` is found first.

### 2.3 Build Environment

```ini
[env:treklink-v3-tbeam]
extends = env:tbeam
build_flags =
  ${env:tbeam.build_flags}
  -D TREKLINK_VARIANT
  -D TREKLINK_V3
  -I variants/esp32/treklink_v3_tbeam
lib_deps =
  ${env:tbeam.lib_deps}
```

---

## 3. Components and Interfaces

### 3.1 Single-Button SOS Gesture

The T-Beam has only `BUTTON_PIN=38`. The `TrekLinkSOSGesture` module (defined in multi-variant spec) monitors this pin for 3-second holds.

**Activation guard:**
```cpp
#if defined(TREKLINK_VARIANT) && !defined(BUTTON_PIN_SOS)
// No dedicated SOS button → use gesture module
```

Since `BUTTON_PIN_SOS` is NOT defined in the T-Beam overlay, the gesture module activates automatically.

**Coexistence with OneButton:** The gesture module only acts on 3s+ holds. Short presses are NOT consumed, so Meshtastic's `OneButton` handler continues to process them for screen navigation.

### 3.2 Fall Detection

Fall detection is excluded at the `Modules.cpp` level:
- v3.0 has no IMU → `FallDetectionModule` is not instantiated
- Controlled by: T-Beam V1.2 has no `accelerometer_found` → `AccelerometerThread` is not created → fall detection has no sensor to use

---

## 4. Data Models

No changes from base Meshtastic or v1.0 TrekLink.

---

## 5. Error Handling

| Error Condition | Handling |
|---|---|
| Stock T-Beam variant.h changes in upstream Meshtastic | Overlay re-exports it — transparent. Monitor for breaking pin renames. |
| SOS gesture conflicts with OneButton long-press | Gesture module uses 3s threshold; OneButton's default long-press is 1s for screen wake. Distinct thresholds prevent conflict. |

---

## 6. Testing Strategy

- **Compile test**: `pio run -e treklink-v3-tbeam` succeeds AND `pio run -e tbeam` still succeeds
- **Branding test**: Boot shows "TrekLink" splash, BLE name is "TrekLink_xxxx"
- **SOS gesture**: Hold button 3s → SOS triggers; short press → screen navigates normally
- **Interop test**: v3.0 device sends/receives messages to v1.0 and v2.0 devices
