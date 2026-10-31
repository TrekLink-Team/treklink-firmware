---
id: v4-components
type: design
status: draft
owner: KhoaDD
summary: What the v4 LilyGo T-Beam Supreme unit is, which stock definitions TrekLink inherits, and which TrekLink capabilities the v4 image enables. Authoritative over requirements.md and design.md in this folder where they disagree.
variant: v4
support: active
hw_verified: none
verified: {commit: 54e0d8d, date: 2026-10-31, scope: "pins against upstream variants/esp32s3/tbeam-s3-core/variant.h at v2.7.26"}
---

# v4 components

| | |
|---|---|
| Board | LilyGo T-Beam Supreme (off the shelf) |
| Env | `treklink-v4`, `variants/esp32s3/treklink_v4/platformio.ini`, extends stock `env:tbeam-s3-core` (board `tbeam-s3-core`, 8 MB partitions) |
| USB identity | Espressif USB-JTAG/serial `303a:1001`, the same as v2; the client API reports `pio_env` `treklink-v4`, which is how a tool tells them apart |
| Stock pin file | `variants/esp32s3/tbeam-s3-core/variant.h`, included unchanged |

## Parts

| Function | Part | Bus and pins | Source (stock `tbeam-s3-core/variant.h`) |
|---|---|---|---|
| MCU | ESP32-S3 | | `board = tbeam-s3-core` |
| LoRa radio | SX1262, SX1268 or LR1121, probed at run time | SPI: SCK 12, MISO 13, MOSI 11, CS 10, RESET 5, DIO1 1, BUSY 4; SX126x TCXO 1.8 V | `:14-46`, `:61-64` |
| GNSS | GNSS module on UART | RX 9, TX 8, wake 7, 1PPS 6 | `:66-69` |
| PMU and RTC | AXP2101, PCF8563 at 0x51 | second I2C bus: SDA 42, SCL 41 | `:3-4`, `:52-59` |
| Display | SH1106 OLED | I2C: SDA 17, SCL 18 | `:6-7`, `:77` |
| Sensors on the main I2C bus | QMC6310 magnetometer (stock comment) | SDA 17, SCL 18 | `:6-7` |
| IMU | QMI8658. Bus not settled: TrekLink code probes I2C 0x6B on 17/18 (`treklink/src/fall/sensors/QMI8658FallSensor.cpp`), LilyGo's documentation puts it on SPI (CS 34). Stock 2.7.26 has no QMI8658 motion driver and only lists the chip in the I2C scan | | `docs/variants.md` gap 6; decided by the first v4 boot log |
| SD card | SPI slot | | `:71-72` |
| Buttons | user button 0 (also the ESP32-S3 boot strap); the power button is wired to the PMU | | `:9` |
| LED, buzzer, vibration | none | | |

## Capabilities

| Capability | Value |
|---|---|
| `TREKLINK_VARIANT_NAME` | `"TrekLink v4 (T-Beam Supreme)"` |
| `TREKLINK_SOS_BUTTON_PIN` | absent: SOS on the user button (trigger being redesigned, as for v3) |
| `PIN_BUZZER`, `PIN_VIBRATOR`, `TREKLINK_SOS_LED_PIN` | absent: the only local SOS feedback is the screen |
| `TREKLINK_FALL_SENSOR` | `TREKLINK_FALL_QMI8658` |
| `TREKLINK_ONBOARD_QUEUE` | 1, 1000 entries, 64 in RAM, 512 KiB flash budget |
| Region | `REGULATORY_LORA_REGIONCODE` MY_433 (VN 433) |

## Shared resources to watch

| Resource | Other users |
|---|---|
| User button 0 | stock `UserButtonThread`, the same SELECT and SHUTDOWN timings as v3; boot strap pin, so holding it during reset enters the ROM bootloader |
| Main I2C bus 17/18 | OLED, QMC6310, any I2C IMU probe |

## Known gaps

No local alarm at all. Fall detection unverified and probably probing the wrong bus. Never run on
hardware with a logged image.
