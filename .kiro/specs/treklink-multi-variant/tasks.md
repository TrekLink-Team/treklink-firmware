# TrekLink Multi-Variant Cross-Cutting Tasks

> **Scope:** Shared infrastructure spanning all variants — SOS unification, FallDetection refactor, build verification  
> **Baseline:** `2.0.0-predev` (Commit `3aa286ea`)  
> **Status:** DRAFT — Awaiting Review  
> **Note:** Tasks-only spec. Version-specific R/D/T specs are in their respective folders.

---

## Related Version Specs

| Spec | Location | Status |
|------|----------|--------|
| v1.0 (Perfboard) | [.kiro/specs/treklink-v1.0/](../treklink-v1.0/) | ✅ COMPLETE |
| v2.0 (Custom PCB) | [.kiro/specs/treklink-v2.0/](../treklink-v2.0/) | 📝 DRAFT |
| v3.0 (T-Beam V1.2) | [.kiro/specs/treklink-v3.0/](../treklink-v3.0/) | 📝 DRAFT |
| v4.0 (T-Beam Supreme) | [.kiro/specs/treklink-v4.0/](../treklink-v4.0/) | 📝 DRAFT |

---

## Global Execution Order

All tasks across ALL specs must be executed in this precise order. Each step references which spec file contains the full task details.

```
STAGE 1: Cross-Cutting Foundation (this file)
  MV-1  Extract shared SOS helper
  MV-2  Define FallSensorInterface abstraction
  MV-3  Implement MPU6050FallSensor adapter
  MV-4  Refactor FallDetectionModule to use FallSensorInterface
  MV-5  Create TrekLinkSOSGesture module
  MV-6  Register TrekLinkSOSGesture in Modules.cpp

STAGE 2: v2.0 Custom PCB (treklink-v2.0/tasks.md)
  V2-1  Create treklink_v2_0 variant directory and files
  V2-2  Create treklink_v2_0 PlatformIO build environment
  V2-3  Implement power latch and power-sense logic
  V2-4  Implement GPS power enable control
  V2-5  Adapt TrekLinkButtonModule for v2.0 pin remapping
  V2-6  Adapt TrekLinkButtonInput for v2.0 pin remapping
  V2-7  Implement ICM20948FallSensor adapter
  V2-8  Verify v2.0 environment compiles

STAGE 3: v3.0 T-Beam Overlay (treklink-v3.0/tasks.md)
  V3-1  Create treklink_v3_tbeam overlay directory and files
  V3-2  Create treklink-v3-tbeam PlatformIO environment
  V3-3  Verify branding activates on T-Beam
  V3-4  Verify fall detection exclusion
  V3-5  Verify v3.0 environment compiles

STAGE 4: v4.0 T-Beam Supreme Overlay (treklink-v4.0/tasks.md)
  V4-1  Create treklink_v4_supreme overlay directory and files
  V4-2  Create treklink-v4-supreme PlatformIO environment
  V4-3  Implement QMI8658FallSensor adapter
  V4-4  Wire QMI8658 sensor in Modules.cpp
  V4-5  Verify branding activates on T-Beam Supreme
  V4-6  Verify v4.0 environment compiles

STAGE 5: Final Validation (this file)
  MV-7  Wire all sensor selection in Modules.cpp
  MV-8  Verify ALL environments compile (full regression)
  MV-9  Add compile-time variant validation
```

**Rationale:**
- Stage 1 first: shared infrastructure (SOS helper, FallSensorInterface, gesture module) must exist before any variant can use it.
- Stage 2 before 3/4: v2.0 is the most complex variant and exercises the most code paths (pin remapping, power latch, sensor adapter).
- Stage 3 before 4: v3.0 is simpler (no IMU), validates overlay pattern before v4.0 adds QMI8658.
- Stage 5 last: full cross-variant regression after all variants are wired up.

---

## Hardware Button Reference

| Device | Physical Buttons | Firmware-Accessible | GPIO |
|--------|-----------------|---------------------|------|
| v1.0 Perfboard | 4 (MENU, SOS, UP, DOWN) + slide switch | 4 | 25, 34, 32, 35 |
| v2.0 Custom PCB | 5 (SELECT, SOS, UP, DOWN, POWER) | 4 (POWER is latch, not button input) | 0, 4, 7, 8 |
| v3.0 T-Beam V1.2 | 3 (Power, User, Reset) | **1 (User only)** | 38 |
| v4.0 T-Beam Supreme | 3 (Power, User, Reset) | **1 (User only)** | 0 |

> **Note:** T-Beam Power and Reset buttons are hardware-only (not GPIO-routable). Only the User/Program button is available to firmware. SOS on v3.0/v4.0 uses a 3-second hold gesture on this single firmware-accessible button.

---

## Stage 1: Cross-Cutting Foundation

- [x] MV-1. Extract shared SOS logic into a common helper
  - Current SOS trigger/cancel code is duplicated between `TrekLinkButtonModule` and `FallDetectionModule`
  - Extract into `src/modules/TrekLinkSOSHelper.h/.cpp`:
    - `triggerSOS()` — send position + text packet, activate alarms
    - `cancelSOS()` — silence alarms, reset state
    - `activateAlarms()` / `deactivateAlarms()`
    - `broadcastPosition()`
  - Refactor both `TrekLinkButtonModule` and `FallDetectionModule` to delegate to helper
  - `TrekLinkSOSGesture` (MV-5) will also use this helper
  - _References: TrekLinkButtonModule.cpp L239-L309, FallDetectionModule.cpp L171-L256_

- [x] MV-2. Define `FallSensorInterface` abstraction
  - New file: `src/modules/FallSensorInterface.h`
  - Pure virtual interface with `init()`, `readAccel(x,y,z)`, `readGyro(x,y,z)`
  - Decouples `FallDetectionModule` from any specific IMU library
  - _References: MotionSensor.h base class pattern_

- [x] MV-3. Implement `MPU6050FallSensor` adapter
  - New file: `src/modules/sensors/MPU6050FallSensor.h/.cpp`
  - Implements `FallSensorInterface` using existing `Adafruit_MPU6050` library
  - Used by v1.0 (perfboard with MPU6050)
  - _References: FallDetectionModule.cpp L278-L298_

- [x] MV-4. Refactor `FallDetectionModule` to use `FallSensorInterface`
  - Replace `Adafruit_MPU6050 mpu;` member with `FallSensorInterface* sensor;`
  - Replace `mpu.getEvent()` with `sensor->readAccel()`/`sensor->readGyro()`
  - Accept sensor via constructor: `FallDetectionModule(FallSensorInterface* sensor)`
  - _References: FallDetectionModule.h L7-L11, L65_

- [x] MV-5. Create `TrekLinkSOSGesture` module for single-button SOS
  - New file: `src/modules/TrekLinkSOSGesture.h/.cpp`
  - Polls `BUTTON_PIN` at 20ms, detects 3-second hold
  - On 3s hold: calls `TrekLinkSOSHelper::triggerSOS()` / `cancelSOS()`
  - Short press: NOT consumed — passes through to Meshtastic's OneButton
  - Guard: `#if defined(TREKLINK_VARIANT) && !defined(BUTTON_PIN_SOS)`
  - _References: v3.0/v4.0 SOS requirements, TrekLinkButtonModule.cpp debounce_

- [x] MV-6. Register `TrekLinkSOSGesture` in `Modules.cpp`
  - `#if !defined(BUTTON_PIN_SOS)` → gesture module; else → `TrekLinkButtonModule`
  - _References: Modules.cpp L184-L194_

---

## Stage 5: Final Validation

- [x] MV-7. Wire all sensor selection in `Modules.cpp`
  - `#ifdef TREKLINK_V2` → `new ICM20948FallSensor()`
  - `#ifdef TREKLINK_V4` → `new QMI8658FallSensor()`
  - Default (v1.0) → `new MPU6050FallSensor()`
  - v3.0 → skip fall detection (no IMU)
  - _References: AccelerometerThread.h pattern_

- [x] MV-8. Verify ALL environments compile (full regression)
  - `pio run -e treklink` (v1.0)
  - `pio run -e treklink-v2` (v2.0)
  - `pio run -e treklink-v3-tbeam` (v3.0)
  - `pio run -e treklink-v4-supreme` (v4.0)
  - Also verify stock targets: `pio run -e tbeam`, `pio run -e tbeam-s3-core`

- [x] MV-9. Add compile-time variant validation
  - `static_assert` guards for GPIO defines per variant
  - Boot-time log: print active variant name
  - _References: variant.h defines per environment_
