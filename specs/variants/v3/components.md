---
id: v3-components
type: design
status: draft
owner: KhoaDD
summary: What the v3 LilyGo T-Beam v1.2 unit is, which stock definitions TrekLink inherits, and which TrekLink capabilities the v3 image enables. Authoritative over requirements.md and design.md in this folder where they disagree.
variant: v3
support: active
hw_verified: none
verified: {commit: 54e0d8d, date: 2026-10-31, scope: "pins against upstream variants/esp32/tbeam/variant.h at v2.7.26"}
---

# v3 components

| | |
|---|---|
| Board | LilyGo T-Beam v1.2 (off the shelf) |
| Env | `treklink-v3`, `variants/esp32/treklink_v3/platformio.ini`, extends stock `env:tbeam` (board `ttgo-tbeam`) |
| USB identity | the board's USB-UART bridge (CP2104 or CH9102, depending on the batch; read from the unit); the client API reports `pio_env` `treklink-v3` |
| Stock pin file | `variants/esp32/tbeam/variant.h`, included unchanged |

## Parts

| Function | Part | Bus and pins | Source (stock `tbeam/variant.h`) |
|---|---|---|---|
| MCU | ESP32 | | `board = ttgo-tbeam` |
| LoRa radio | SX1276, SX1262 or SX1268 module, probed at run time; the 433 MHz order is SX1278 or SX1268 | CS (stock `LORA_CS`), RESET 23, DIO0 26, DIO1 33, BUSY 32; SX126x TCXO 1.8 V, DIO2 RF switch | `:16-33` |
| GNSS | u-blox | UART: RX 34, TX 12 | `:42-44` |
| PMU | AXP192 or AXP2101, probed by stock code | I2C | `:41` |
| Display | optional OLED on I2C | SDA 21, SCL 22 | `:3-4` |
| Buttons | user button 38 (active low); the power and reset buttons are wired to the PMU and the reset line, not to a GPIO | | `:6-8` |
| LED | `LED_POWER` 4, active low (named `LED_PIN` before upstream renamed it) | GPIO 4 | `:11-12` |
| IMU | none | | |
| Buzzer, vibration | none | | |

## Capabilities

| Capability | Value |
|---|---|
| `TREKLINK_VARIANT_NAME` | `"TrekLink v3 (T-Beam v1.2)"` |
| `TREKLINK_SOS_BUTTON_PIN` | absent: SOS on the user button (the trigger gesture is being redesigned, see below) |
| `PIN_BUZZER`, `PIN_VIBRATOR` | absent |
| `TREKLINK_SOS_LED_PIN` | 4 (`LED_POWER`) |
| `TREKLINK_FALL_SENSOR` | `TREKLINK_FALL_NONE` |
| `TREKLINK_ONBOARD_QUEUE` | 1, `TrekLinkQueueConfig.h` defaults |
| Region | `REGULATORY_LORA_REGIONCODE` MY_433 (VN 433) |

## Shared resources to watch

| Resource | Other users |
|---|---|
| User button 38 | stock `UserButtonThread`: 500 ms hold SELECT, release after 3.9 s SHUTDOWN (`InputBroker.cpp:305-307`, `ButtonThread.h:24`). The leader ruled on 2026-10-31 that a long hold on this button is not an acceptable SOS trigger; a new trigger is specified in the SOS module spec before the fix |
| GPIO 4 LED | stock power LED at boot (`main.cpp:320-322`) and `StatusLEDModule` charge indication (`StatusLEDModule.cpp:218-219`). Upstream renamed this pin from `LED_PIN` to `LED_POWER` after 2.7.19, so the old `#ifdef LED_PIN` SOS strobe would have compiled out silently on 2.7.26; TrekLink now names it `TREKLINK_SOS_LED_PIN` |

## Known gaps

No local alarm beyond the LED. No fall detection. Never run on hardware with a logged image.
