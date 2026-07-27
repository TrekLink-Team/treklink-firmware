# TrekLink v4.0 Design Document

> **Hardware:** COTS — LilyGo T-Beam Supreme (tbeam-s3-core)  
> **Version:** 1.0  
> **Date:** June 11, 2026  
> **Status:** DRAFT — Awaiting Review

---

## 1. Overview

v4.0 is a firmware overlay on the stock Meshtastic T-Beam Supreme target. It follows the same overlay pattern as v3.0 but additionally enables fall detection via the onboard QMI8658 IMU.

**Key Capabilities vs Other Variants:**

| Feature | v1.0 | v2.0 | v3.0 | v4.0 |
|---------|------|------|------|------|
| MCU | ESP32 | ESP32-S3 | ESP32 | ESP32-S3 |
| SOS Button | Dedicated (GPIO 34) | Dedicated (GPIO 4) | Gesture (3s hold) | Gesture (3s hold) |
| Fall Detection | ✅ MPU6050 | ✅ ICM-20948 | ❌ No IMU | ✅ QMI8658 |
| Custom PCB | ✅ Perfboard | ✅ JLCPCB | ❌ COTS | ❌ COTS |

---

## 2. Architecture

### 2.1 Overlay File Structure

```
variants/esp32s3/treklink_v4_supreme/
├── variant.h          # Includes stock tbeam-s3-core/variant.h + TrekLink defines
└── platformio.ini     # [env:treklink-v4-supreme] extends env:tbeam-s3-core
```

### 2.2 Include Path Strategy

Same pattern as v3.0:

```cpp
// variants/esp32s3/treklink_v4_supreme/variant.h
#pragma once

// Import full stock T-Beam Supreme pin definitions
#include "../tbeam-s3-core/variant.h"

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

### 2.3 Build Environment

```ini
[env:treklink-v4-supreme]
extends = env:tbeam-s3-core
build_flags =
  ${env:tbeam-s3-core.build_flags}
  -D TREKLINK_VARIANT
  -D TREKLINK_V4
  -I variants/esp32s3/treklink_v4_supreme
lib_deps =
  ${env:tbeam-s3-core.lib_deps}
```

**Note:** `SensorLib` (which provides QMI8658 driver) is already in `tbeam-s3-core` lib_deps.

---

## 3. Components and Interfaces

### 3.1 QMI8658 Fall Sensor Adapter

The T-Beam Supreme has a QMI8658 on I2C bus (SDA=17, SCL=18). A `QMI8658FallSensor` adapter implements `FallSensorInterface`:

```cpp
class QMI8658FallSensor : public FallSensorInterface {
    SensorQMI8658 qmi;
public:
    bool init() override {
        // Init using SensorLib API on I2C address 0x6B
        return qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
    }
    bool readAccel(float &x, float &y, float &z) override {
        // Read accel, convert to g-force
    }
    bool readGyro(float &x, float &y, float &z) override {
        // Read gyro, convert to rad/s
    }
};
```

**Sensor wiring:** Uses the T-Beam Supreme's secondary I2C bus (SDA=17, SCL=18 — NOT I2C_SDA1/SCL1 which is for PMU).

### 3.2 SOS Gesture

Same as v3.0 — `TrekLinkSOSGesture` module monitors `BUTTON_PIN=0` for 3s holds. `BUTTON_PIN_SOS` is NOT defined, so the gesture module activates.

### 3.3 Module Registration in Modules.cpp

```cpp
#ifdef TREKLINK_VARIANT
  #ifdef TREKLINK_V4
    auto* fallSensor = new QMI8658FallSensor();
    new FallDetectionModule(fallSensor);  // Fall detection with QMI8658
  #endif
  #if !defined(BUTTON_PIN_SOS)
    new TrekLinkSOSGesture();  // Gesture SOS for single-button devices
  #endif
#endif
```

---

## 4. Data Models

No changes from base Meshtastic or v1.0 TrekLink.

---

## 5. Error Handling

| Error Condition | Handling |
|---|---|
| QMI8658 not detected | Log warning, disable fall detection, manual SOS still works |
| SensorLib API changed | Adapter isolates impact — only `QMI8658FallSensor` needs update |
| Stock tbeam-s3-core variant.h changes | Overlay re-exports — transparent to TrekLink code |

---

## 6. Testing Strategy

- **Compile test**: `pio run -e treklink-v4-supreme` succeeds AND `pio run -e tbeam-s3-core` still succeeds
- **Fall detection**: QMI8658 adapter returns normalized g-force/rad-s data; fall state machine triggers correctly
- **SOS gesture**: Same test as v3.0 — 3s hold → SOS, short press → screen navigation
- **Interop**: v4.0 device communicates with v1.0, v2.0, v3.0 devices on same mesh
