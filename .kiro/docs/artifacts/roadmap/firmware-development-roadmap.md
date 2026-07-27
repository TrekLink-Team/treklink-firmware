# TrekLink Firmware Development Roadmap

> **Repository:** `ruskicoder/treklink` (monorepo, single fork)  
> **Upstream Base:** Meshtastic firmware **v2.7.19** (forked ~Jan 2026)  
> **Latest Upstream:** Meshtastic **v2.7.24-alpha** (as of Jun 2026)  
> **Architecture:** Single monorepo with multiple PlatformIO build environments  

---

## Executive Summary

Three firmware variants, one codebase. Each variant targets different hardware but shares common TrekLink-specific modules (branding, SOS, fall detection where applicable).

| Version | Hardware | Status | Key Differentiator |
|---------|----------|--------|--------------------|
| **v2.0** | Custom PCB (ESP32-S3 + E22 + NEO-M9N + ICM-20948) | 🔧 In development | Full custom variant, 5 buttons, fall detection |
| **v3.0** | LILYGO T-Beam V1.2 (ESP32 + SX1262 + NEO-M8N) | 📋 Planned | Fork upstream `tbeam` variant + branding + SOS |
| **v4.0** | LILYGO T-Beam Supreme (ESP32-S3 + SX1262 + MAX-M10S + QMI8658) | 📋 Planned | Fork upstream `tbeam-s3-core` + branding + SOS + fall detection |

---

## Deliverables (All Variants)

### Core (Must Have)
1. **Functional Meshtastic firmware** — full LoRa mesh, GPS tracking, OLED display, Bluetooth pairing
2. **TrekLink branding** — splash screen, Bluetooth name, device name display "TrekLink" instead of "Meshtastic"
3. **Correct pin mapping** — variant-specific `variant.h` with designated GPIO assignments
4. **SOS function** — dedicated button (v2.0) or long-press gesture (v3.0/v4.0) sends location ping to mesh

### Stretch (Nice to Have)
5. **Fall detection** — TinyML-based (preferred) or algorithm-based freefall→impact→inactivity state machine
   - v2.0: ICM-20948 (via sensor-agnostic abstraction)
   - v3.0: ❌ No IMU — not applicable
   - v4.0: QMI8658 (via sensor-agnostic abstraction)
   - Fallback: Pure algorithmic detection with hardcoded thresholds

---

## Branding Touchpoints

All three variants share the same branding changes via the `TREKLINK_VARIANT` compile flag:

| Touchpoint | File | Current (Stock) | TrekLink Override |
|------------|------|-----------------|-------------------|
| **Splash screen title** | `src/graphics/draw/UIRenderer.cpp:946` | `"meshtastic.org"` | `"TrekLink"` |
| **Bluetooth device name** | `src/main.cpp:241` | `"Meshtastic_XXXX"` | `"TrekLink_XXXX"` |
| **Help text** | `src/graphics/draw/NotificationRenderer.cpp:679` | `"meshtastic.org"` | `"treklink"` (if applicable) |
| **Boot logo** | `branding/` folder | Meshtastic logo | TrekLink logo PNG (per resolution) |

**Implementation:** Already partially done. The `#ifdef TREKLINK_VARIANT` guard exists in `UIRenderer.cpp` and `main.cpp`. Each variant's `platformio.ini` adds `-D TREKLINK_VARIANT` to build flags.

---

## v2.0: Custom PCB Variant

### Hardware Profile
- **MCU:** ESP32-S3-WROOM-1 (N8R8)
- **LoRa:** EBYTE E22-400M22S (SX1268, 433 MHz, +22 dBm, 1W)
- **GPS:** u-blox NEO-M9N (4-constellation GNSS)
- **IMU:** ICM-20948 (9-axis, 1.8V VDDIO via AMS1117-1.8 + level shifters)
- **Display:** SSD1306 0.96" OLED (I2C, 0x3C)
- **Buttons:** 5 (SELECT=GPIO 0, SOS=GPIO 4, UP=GPIO 7, DOWN=GPIO 8, POWER=GPIO 13)
- **Notifications:** Buzzer (GPIO 11, PWM via MOSFET), Vibrator (GPIO 12, MOSFET), LED (GPIO 2)
- **Power:** BQ24074 charger → TPS63802 buck-boost → 3.3V rail, soft-latch (GPIO 9 hold, GPIO 13 sense)
- **Battery ADC:** GPIO 1 (ADC1_CH0, 100k/100k divider, multiplier = 2.0)

### Build Environment
```ini
[env:treklink-v2]
extends = esp32s3_base
board = treklink-esp32s3         ; new board JSON for ESP32-S3
build_flags =
  ${esp32s3_base.build_flags}
  -D TREKLINK_VARIANT
  -D TREKLINK_V2
  -I variants/esp32s3/treklink_v2_0
  -D HAS_SCREEN=1
  -D USE_SX1268
  -D GPS_UBLOX
  -D HAS_ICM20948                ; enables fall detection module
```

### Variant Pin Map (`variants/esp32s3/treklink_v2_0/variant.h`)

Source: v2.0-pcb-design-requirements.md §1.6

```
I2C:        SDA=5, SCL=6
GPS UART:   RX=16, TX=17, EN=15
LoRa SPI:   SCK=21, MOSI=38, MISO=39, CS=14, DIO1=42, BUSY=41, NRST=40, RXEN=43
Buttons:    SELECT=0, SOS=4, UP=7, DOWN=8, POWER_SENSE=13
Power:      LATCH=9, BATTERY_PIN=1 (ADC)
Notif:      BUZZER=11, VIBRATOR=12, LED=2
IMU INT:    3
USB:        D-=19, D+=20
```

### SOS Implementation
- Uses existing `TrekLinkButtonModule` (dedicated SOS on GPIO 4)
- Uses existing `TrekLinkButtonInput` (UP/DOWN/SELECT via InputBroker)
- Both modules already implemented and tested

### Fall Detection
- Current `FallDetectionModule` uses `Adafruit_MPU6050` directly
- **Must refactor** to use sensor-agnostic `MotionSensor` abstraction (see §Architecture below)
- ICM-20948 is already supported by Meshtastic's `AccelerometerThread` → `ICM20948Sensor`

---

## v3.0: LILYGO T-Beam V1.2 Variant

### Hardware Profile
- **MCU:** ESP32-D0WDQ6 (legacy dual-core)
- **LoRa:** SX1262 (+21 dBm, 433 MHz)
- **GPS:** NEO-M8N (dual-constellation: GPS + GLONASS)
- **IMU:** ❌ None onboard
- **Display:** Optional 0.96" OLED add-on (SSD1306 or SH1106)
- **Buttons:** 3 physical (Power, **User/Middle = GPIO 38**, Reset). Only GPIO 38 is programmable.
- **PMU:** AXP192

### Build Environment
```ini
[env:treklink-v3-tbeam]
extends = env:tbeam              ; inherit upstream T-Beam config
build_flags =
  ${env:tbeam.build_flags}
  -D TREKLINK_VARIANT
  -D TREKLINK_V3
```

### Variant Strategy
- **No new `variant.h` needed.** Fork the existing `variants/esp32/tbeam/` pin definitions as-is.
- Only add `TREKLINK_VARIANT` build flag to enable branding overrides.
- The existing upstream `variant.h` already defines `BUTTON_PIN 38`.

### SOS Implementation (Long-Press Gesture)
- Repurpose the single user button (GPIO 38) with gesture detection:
  - **Short press** → Normal Meshtastic screen navigation (default behavior, unchanged)
  - **Long press (3s hold)** → Trigger SOS (broadcast position + activate alarms)
  - **Long press while SOS active** → Cancel SOS
- Implementation: Adapt `TrekLinkButtonModule` to read from `BUTTON_PIN` (GPIO 38) instead of `BTN_SOS` (GPIO 34)
- The button module will detect hold duration and only trigger SOS on the 3-second threshold, passing through short presses to the standard Meshtastic input handler

### Fall Detection
- ❌ **Not applicable** — no IMU onboard. Module excluded at compile time via `#if` guards.

---

## v4.0: LILYGO T-Beam Supreme Variant

### Hardware Profile
- **MCU:** ESP32-S3 (WROOM-1)
- **LoRa:** SX1262 (+22 dBm)
- **GPS:** u-blox MAX-M10S (L1/L5, 4-constellation, best-in-class canopy accuracy)
- **IMU:** QMI8658 (6-axis accelerometer + gyroscope) + QMC6310 (magnetometer)
- **Display:** Optional OLED (SH1106)
- **Buttons:** 3 physical (Power, **BOOT = GPIO 0**, Reset). Only GPIO 0 is programmable.
- **PMU:** AXP2101
- **RTC:** PCF8563

### Build Environment
```ini
[env:treklink-v4-supreme]
extends = env:tbeam-s3-core      ; inherit upstream T-Beam Supreme config
build_flags =
  ${env:tbeam-s3-core.build_flags}
  -D TREKLINK_VARIANT
  -D TREKLINK_V4
  -D HAS_QMI8658                 ; enables fall detection with QMI8658
```

### Variant Strategy
- Fork existing `variants/esp32s3/tbeam-s3-core/` pin definitions as-is.
- Add `TREKLINK_VARIANT` build flag.
- Upstream `variant.h` already defines `BUTTON_PIN 0`.

### SOS Implementation (Long-Press Gesture)
- Same pattern as v3.0: long-press GPIO 0 for 3 seconds → SOS trigger.
- Short press → normal screen navigation.

### Fall Detection (Algorithm-Based)
- **Enabled** — QMI8658 provides 6-axis accel + gyro data
- Uses the refactored sensor-agnostic `FallDetectionModule` (see §Architecture)
- QMI8658 is already supported by Meshtastic's `AccelerometerThread`
- Detection pipeline: Freefall (<0.5g, 500ms) → Impact (>3g) → Inactivity (10s) → Pre-alarm (30s countdown) → Auto-SOS

---

## Architecture: Sensor-Agnostic Fall Detection Refactor

### Current State (Tightly Coupled)
```
FallDetectionModule → Adafruit_MPU6050 (direct instantiation)
                      Only works with MPU6050
```

### Target State (Abstracted)
```
FallDetectionModule → MotionSensor* (abstract interface)
                         ↑
            ┌────────────┼────────────┐
            │            │            │
      MPU6050Sensor  ICM20948Sensor  QMI8658Sensor  (+ any future sensor)
            │            │            │
      (v1.0 legacy) (v2.0 custom) (v4.0 Supreme)
```

### Refactoring Steps
1. Remove direct `Adafruit_MPU6050` dependency from `FallDetectionModule`
2. Accept a `MotionSensor*` pointer (from `AccelerometerThread`) at construction
3. Use `MotionSensor::getEvent()` to read accel/gyro data generically
4. Keep the freefall→impact→inactivity→pre-alarm→SOS state machine unchanged
5. Compile-time inclusion via `#if !MESHTASTIC_EXCLUDE_I2C && defined(HAS_ACCELEROMETER_FALL_DETECT)`

### Compile-Time Guards
```cpp
// In FallDetectionModule.h
#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C
// Module available on any board with a supported accelerometer
class FallDetectionModule : public SinglePortModule, public concurrency::OSThread { ... };
#endif
```

---

## Monorepo Build Structure

All variants coexist in a single repository. Flash any target with:

```bash
# v2.0 Custom PCB
pio run -e treklink-v2 -t upload

# v3.0 T-Beam V1.2
pio run -e treklink-v3-tbeam -t upload

# v4.0 T-Beam Supreme
pio run -e treklink-v4-supreme -t upload
```

### File Structure (New/Modified)
```
treklink/
├── boards/
│   ├── treklink-esp32.json           # v1.0 (existing, legacy)
│   └── treklink-esp32s3.json         # v2.0 (new board definition)
├── branding/
│   ├── README.md                      # Branding instructions (existing)
│   └── logo_128x64.png               # TrekLink boot logo (to create)
├── variants/
│   ├── esp32/
│   │   ├── tbeam/                     # upstream T-Beam (untouched)
│   │   ├── treklink_v1_0/            # v1.0 legacy (existing)
│   │   └── treklink_v3_0/            # v3.0 T-Beam fork (new, minimal)
│   │       └── platformio.ini        # extends env:tbeam + TREKLINK_VARIANT
│   └── esp32s3/
│       ├── tbeam-s3-core/            # upstream T-Beam Supreme (untouched)
│       ├── treklink_v2_0/            # v2.0 custom PCB (new)
│       │   ├── variant.h             # full custom pin map from PCB requirements
│       │   ├── pins_arduino.h
│       │   └── platformio.ini
│       └── treklink_v4_0/            # v4.0 T-Beam Supreme fork (new, minimal)
│           └── platformio.ini        # extends tbeam-s3-core + TREKLINK_VARIANT
├── src/
│   ├── modules/
│   │   ├── TrekLinkButtonModule.cpp  # SOS button (existing, adapt for v3/v4)
│   │   ├── TrekLinkButtonModule.h
│   │   ├── FallDetectionModule.cpp   # Refactor to sensor-agnostic (existing)
│   │   └── FallDetectionModule.h
│   ├── input/
│   │   ├── TrekLinkButtonInput.cpp   # Navigation buttons (existing)
│   │   └── TrekLinkButtonInput.h
│   └── main.cpp                       # getDeviceName() branding (existing)
```

---

## Upstream Sync Strategy

### Current State
- Fork base: **Meshtastic v2.7.19** (Jan 2026)
- Latest upstream: **v2.7.24-alpha** (Jun 2026)
- Delta: **5 minor versions behind**

### Recommended Approach
1. **Add upstream remote:**
   ```bash
   git remote add upstream https://github.com/meshtastic/firmware.git
   ```
2. **Periodic rebase** (every stable release):
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```
3. **Isolate TrekLink changes** into clearly marked commits with `[TrekLink]` prefix for easy conflict resolution during rebase.

### Risk Assessment
| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| Upstream refactors `Screen.cpp` / branding strings | Medium | Branding changes are `#ifdef` guarded, easy to re-apply |
| Upstream changes `MotionSensor` interface | Low | Our refactored fall detection uses the same interface |
| Upstream changes T-Beam variant pin maps | Very Low | We extend, not override, upstream variants |
| Upstream adds official fall detection | Low | We can adopt theirs and deprecate ours |

---

## Implementation Priority Order

| Priority | Task | Variant | Effort | Dependencies |
|----------|------|---------|--------|-------------|
| **P0** | Create v2.0 variant folder + `variant.h` from PCB requirements | v2.0 | Low | PCB pin map finalized ✅ |
| **P0** | Create v3.0 `platformio.ini` (extends `tbeam` + branding flag) | v3.0 | Trivial | None |
| **P0** | Create v4.0 `platformio.ini` (extends `tbeam-s3-core` + branding flag) | v4.0 | Trivial | None |
| **P1** | Adapt `TrekLinkButtonModule` for single-button SOS (v3.0/v4.0) | v3.0, v4.0 | Medium | Button GPIO mapping |
| **P1** | Create TrekLink boot logo PNG for `branding/` | All | Low | Design asset |
| **P2** | Refactor `FallDetectionModule` to sensor-agnostic architecture | v2.0, v4.0 | Medium-High | MotionSensor API understanding |
| **P2** | Add upstream remote + test rebase to v2.7.24 | All | Medium | Clean git history |
| **P3** | TinyML fall detection model (stretch goal) | v4.0 | High | Training data, TFLite Micro |
