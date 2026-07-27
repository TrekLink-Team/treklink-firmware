# TrekLink v2.0 Implementation Tasks

> **Scope:** Custom PCB variant — ESP32-S3 + SX1268 + ICM-20948  
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

- [x] 1. Create `treklink_v2_0` variant directory and files
  - Create `variants/esp32s3/treklink_v2_0/variant.h` with frozen pin map:
    - I2C: SDA=5, SCL=6
    - GPS UART: RX=16, TX=17, PWR_EN=15
    - LoRa SX1268 SPI: SCK=21, MOSI=38, MISO=39, CS=14, DIO1=42, BUSY=41, NRST=40, RXEN=43
    - Buttons: SELECT=0, SOS=4, UP=7, DOWN=8, POWER_SENSE=13 (Active-Low)
    - Power: LATCH=9 (Active-High), BATTERY_PIN=1 (ADC)
    - Peripherals: BUZZER=11, VIBRATOR=12, LED=2
  - Create `variants/esp32s3/treklink_v2_0/pins_arduino.h` (standard ESP32-S3 pin stub)
  - Define `TREKLINK_VARIANT`, `TREKLINK_V2`, `USE_SX1268`, region code `MY_433`
  - _Requirements: REQ-V2-HW-01, REQ-V2-RF-01_

- [x] 2. Create `treklink_v2_0` PlatformIO build environment
  - Create `variants/esp32s3/treklink_v2_0/platformio.ini`
  - Environment name: `[env:treklink-v2]`, extends `esp32s3_base`
  - Build flags: `-D TREKLINK_VARIANT -D TREKLINK_V2 -D HAS_SCREEN=1`
  - Include proper `lib_deps` (radiolib, environmental, networking)
  - Board definition: either existing S3 board or custom JSON
  - Verify env is picked up by root `platformio.ini` glob `variants/*/*.ini`
  - _Requirements: REQ-V2-ENV-01_

- [x] 3. Implement power latch and power-sense logic
  - Create `variants/esp32s3/treklink_v2_0/variant.cpp` with `earlyInitVariant()`
  - Set GPIO 9 (POWER_LATCH) HIGH within first 100ms of boot
  - Read GPIO 13 (POWER_SENSE) as Active-Low input for cold-start detection
  - Implement software shutdown: set LATCH LOW to cut power
  - Guard with `#ifdef TREKLINK_V2`
  - _Requirements: REQ-V2-PWR-01, REQ-V2-PWR-02_

- [x] 4. Implement GPS power enable control
  - Add `#define GPS_EN_PIN 15` to `variant.h`
  - Drive GPIO 15 HIGH in `earlyInitVariant()` to power GPS module
  - Integrate with Meshtastic GPS power management if possible
  - Ensure GPS powers on at boot and respects sleep settings
  - _Requirements: REQ-V2-GPS-01_

- [x] 5. Adapt `TrekLinkButtonModule` for v2.0 pin remapping
  - Replace hardcoded `#define BTN_SOS 34` with `variant.h`-sourced `BUTTON_PIN_SOS`
  - Update `pinMode()`: v2.0 SOS pin (GPIO 4) uses `INPUT_PULLUP` (v1.0 GPIO 34 is input-only)
  - Ensure `PIN_VIBRATOR`, `PIN_BUZZER`, `LED_PIN` read from `variant.h`
  - _Requirements: REQ-V2-BTN-01, REQ-V2-HW-01.6_

- [x] 6. Adapt `TrekLinkButtonInput` for v2.0 pin remapping
  - Current code hardcodes v1.0 GPIOs (UP=32, DOWN=35, MENU=25)
  - Use `variant.h` defines: `BUTTON_PIN_UP=7`, `BUTTON_PIN_DOWN=8`, `BUTTON_PIN=0`
  - Update ISR attachment to use variant-defined pins
  - Handle pull-up mode differences between v1.0 and v2.0
  - _Requirements: REQ-V2-BTN-01_

- [x] 7. Implement `ICM20948FallSensor` adapter
  - New file: `src/modules/sensors/ICM20948FallSensor.h/.cpp`
  - Implement `FallSensorInterface` using `SparkFun ICM-20948` library
  - Map ICM-20948 accel reads to g-force, gyro reads to rad/s
  - Guard with `#ifdef TREKLINK_V2` or `__has_include`
  - _Requirements: REQ-V2-IMU-01_

- [x] 8. Verify v2.0 environment compiles
  - Run `pio run -e treklink-v2` — must compile cleanly for ESP32-S3
  - Run `pio run -e treklink` — v1.0 regression check
  - Fix include path, symbol resolution, or link errors
  - _Requirements: REQ-V2-ENV-01.3_
