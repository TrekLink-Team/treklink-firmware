---
id: v2-components
type: design
status: draft
owner: KhoaDD
summary: What the v2 custom board is made of, which pin each part uses, and which TrekLink capabilities the v2 image enables. Authoritative over requirements.md and design.md in this folder where they disagree.
variant: v2
support: active
hw_verified: partial
verified: {commit: 54e0d8d, date: 2026-10-31, scope: "pins against variants/esp32s3/treklink_v2/variant.h; parts against docs/hardware/v2/"}
---

# v2 components

| | |
|---|---|
| Board | TrekLink custom 4-layer PCB, revision 2.0 |
| Env | `treklink-v2`, `variants/esp32s3/treklink_v2/platformio.ini`, extends `esp32s3_base`, PlatformIO board `esp32-s3-devkitc-1`, partitions `default_8MB.csv` |
| USB identity | Espressif USB-JTAG/serial `303a:1001`; the client API reports `pio_env` `treklink-v2` |
| Log console | none on USB in the release image (finding F1); a diagnostic image built with `-DARDUINO_USB_CDC_ON_BOOT=1` logs over USB |

## Parts

| Function | Part | Bus and pins | Source |
|---|---|---|---|
| MCU | ESP32-S3-WROOM-1, 8 MB flash, 8 MB PSRAM | | `docs/hardware/v2/v2.0-component-list.md:42` |
| LoRa radio | Ebyte E22-400M22S (SX1268, 433 MHz), TCXO 1.8 V on DIO3 | SPI: SCK 21, MOSI 38, MISO 39, CS 14, RESET 40, DIO1 42, BUSY 41, RXEN 43 | `variant.h` LoRa block; `docs/hardware/v2/v2.0-pcb-design-requirements.md:46` |
| GNSS | u-blox NEO-M9N | UART: RX 16, TX 17; power enable 15 (active high) | `variant.h` GPS block; design requirements `:47` |
| IMU | TDK ICM-20948 (accelerometer, gyroscope, magnetometer) | I2C at 0x68, SDA 5, SCL 6 | `variant.h` I2C block; design requirements `:48` |
| Display | 128x64 OLED; detected as SH1106 at 0x3c | I2C, shared with the IMU | `docs/test-runs/logs/2026-10-29-v2/cold1.log:22` (the hardware documents say SSD1306) |
| Buttons | MENU (select) 0, SOS 4, UP 7, DOWN 8, power-button sense 13; all active low | GPIO | `variant.h` button block |
| Buzzer | passive buzzer | GPIO 11, LEDC channel 0 | `variant.h` notifications block |
| Vibration | motor | GPIO 12 | `variant.h` |
| LED | status LED, active low | GPIO 2 | `variant.h` |
| Power | 1S2P 21700 pack, BQ24074 charger, TPS63802 buck-boost whose enable is the power latch | latch GPIO 9 (`PIN_POWER_EN`, active high), battery ADC GPIO 1 with a 2:1 divider | `docs/hardware/v2/v2.0-pcb-design-requirements.md:139-140`; `variant.h` power block |

## Capabilities

| Capability | Value |
|---|---|
| `TREKLINK_VARIANT_NAME` | `"TrekLink v2 (custom S3)"` |
| `TREKLINK_SOS_BUTTON_PIN` | 4: dedicated SOS button |
| `PIN_BUZZER`, `PIN_VIBRATOR` | 11, 12 |
| `TREKLINK_SOS_LED_PIN` | 2 |
| `TREKLINK_FALL_SENSOR` | `TREKLINK_FALL_ICM20948` |
| `TREKLINK_ONBOARD_QUEUE` | 1, 1000 entries, 64 in RAM, 512 KiB flash budget |
| Region | `REGULATORY_LORA_REGIONCODE` MY_433 (shown to people as VN 433) |
| Navigation | stock user button on 0 for MENU; stock two-way rocker input for UP 7 and DOWN 8 (architecture design §4.2 O-06) |

## Shared resources to watch

| Resource | Other users |
|---|---|
| LEDC channel 0 and GPIO 11 | stock `tone()` (Arduino-ESP32 `Tone.cpp`) for button feedback and ExternalNotification; finding F2 |
| ICM-20948 | stock `AccelerometerThread` and compass; findings F5, F7 |
| GPIO 2 LED | stock status LED handling |
| GPIO 9 latch | any reset releases it: hold the power button during a flash or reset (chapter 17 §7) |

## Known open findings

F1 to F13 of `docs/test-runs/2026-10-29-v2-hardware-run.md`, except F8 and F9, which upstream
v2.7.26 brings fixes for, and F10, which the move to the stock user button replaces (architecture
design §6).
