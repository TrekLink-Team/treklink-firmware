# TrekLink v2.0 — Hardware Specification (Finalized)

> **Revision:** 2.1  
> **Date:** 2026-05-28  
> **Budget Ref:** `actual-available-funds.md` — 49,803,700₫ total project  
> **PCB Service Cap:** 13,000,000₫ (~$520 USD) for design service  
> **Batch:** 7 units, 1 revision cycle  
> **Status:** **Finalized & Ready for Freelancer Hand-off**

---

## 1. Architecture Overview

```
┌────────────────────────────────────────────────────────────────────────┐
│                        TrekLink v2.0 PCB (52.5×70.0mm)                 │
│                                                                        │
│  ESP32-S3-WROOM-1 (N8R8)       ←SPI→     E22-400M22S (SX1268)          │
│          │                                   │                         │
│          ├─UART1→ NEO-M9N GPS                └─ Built-in IPEX ── SMA   │
│          ├─I2C→ Level Shifter → ICM-20948 IMU                          │
│          ├─I2C→ SSD1306 OLED (solder pads)                             │
│          ├─GPIO→ 5× Buttons (Test + Breakout)                          │
│          ├─PWM→ Passive Buzzer                                         │
│          ├─GPIO→ Vibrator Driver (AO3400A MOSFET)                      │
│          ├─GPIO→ Status LED                                            │
│          └─ADC→ Battery Divider (100k/100k)                            │
│                                                                        │
│  BQ24074 (Charger) ── [PTC Fuse] ── [4-Pin USB Breakout]               │
│      │                                                                 │
│      ├─→ TPS63802 (3.3V Buck-Boost) ──┬── [EN Pin Soft-Latch via GPIO 9]│
│      │                                └──→ AMS1117-1.8 ──→ 1.8V (IMU)  │
│      │                                                                 │
│      └─→ DW01A + FS8205A (Protection) ── [1S2P 21700 Battery, 10Ah]    │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Component Selection (Final)

### 2.1 MCU — ESP32-S3-WROOM-1-N8R8

| Spec | Value |
|------|-------|
| **Core** | Xtensa LX7, dual-core 240MHz |
| **Flash/PSRAM** | 8MB / 8MB |
| **Bluetooth** | BT 5.0 LE |
| **USB** | Native USB-OTG (no CP2102 needed) |
| **Package** | Castellated SMD (18×25.5×3.2mm) |
| **LCSC** | C2913201 (Basic/Extended) |
| **JLCPCB PCBA** | ✅ Supported |

**Firmware migration:** Create `variants/esp32s3/treklink_v2_0/` based on EBYTE_ESP32-S3 variant template. Change from `esp32_base` to `esp32s3_base`. All pin mappings via `variant.h`.

---

### 2.2 LoRa Radio — CDEBYTE E22-400M22S (SX1268, SPI)

| Spec | Value |
|------|-------|
| **Chip** | SX1268 (Semtech) |
| **Interface** | SPI (NSS, SCK, MISO, MOSI, DIO1, BUSY, NRST) |
| **Frequency** | 410–493 MHz (covers VN_433) |
| **TX Power** | +22 dBm (158mW) |
| **RX Current** | ~5mA |
| **Package** | Castellated SMD (14×20mm) |
| **TCXO** | Internal, controlled via DIO3 (1.8V) |
| **RF Switch** | DIO2-controlled (SX126X_DIO2_AS_RF_SWITCH) |

**SPI Pin Mapping (E22-400M22S → ESP32-S3):**

| E22 Pin | Function | ESP32-S3 GPIO |
|---------|----------|----------------|
| NSS | Chip Select | GPIO 14 |
| SCK | SPI Clock | GPIO 21 |
| MOSI | Data In | GPIO 38 |
| MISO | Data Out | GPIO 39 |
| NRST | Reset | GPIO 40 |
| BUSY | Status | GPIO 41 |
| DIO1 | IRQ | GPIO 42 |

---

### 2.3 GPS — u-blox NEO-M9N (Integrated SMD)

| Spec | Value |
|------|-------|
| **Constellations** | GPS + GLONASS + BeiDou + Galileo (4 concurrent) |
| **Accuracy** | 1.5m CEP |
| **Sensitivity** | -167 dBm |
| **Interface** | UART 9600/115200 |
| **Backup Battery** | CR1220 tabbed coin cell on PCB (solder pads) |
| **Antenna Connection** | On-board IPEX/U.FL receptacle |
| **LCSC** | C5119087 |

**ESP32-S3 UART mapping:**

| GPS Pin | ESP32-S3 GPIO |
|---------|---------------|
| TX | GPIO 16 (ESP32-S3 RX) |
| RX | GPIO 17 (ESP32-S3 TX) |
| EN | GPIO 15 (power enable, active HIGH) |

---

### 2.4 IMU — ICM-20948 (PCBA SMT)

| Spec | Value |
|------|-------|
| **Axes** | 9 (Accel + Gyro + Magnetometer AK09916) |
| **VDDIO Logic Level** | 1.71V–1.95V MAX (requires level shifting to 3.3V) |
| **Level Shifters** | 3× 2N7002 N-MOSFETs (SDA, SCL, INT) + AMS1117-1.8 LDO |
| **I2C Address** | 0x68 (no conflict with OLED 0x3C) |
| **LCSC** | C726001 |

**I2C mapping (via level-shifters):**

| Signal | ESP32-S3 GPIO | Logic Level |
|--------|---------------|-------------|
| SDA | GPIO 5 | 3.3V (translates to 1.8V at IMU) |
| SCL | GPIO 6 | 3.3V (translates to 1.8V at IMU) |

---

### 2.5 Display — SSD1306 0.96" I2C OLED (Direct Solder)

The SSD1306 0.96" module is laid flat on the top-center face of the PCB. It connects to the I2C bus at 0x3C via 4-pin direct solder plated through-holes (Option A) to prevent mechanical failure.

---

### 2.6 Power System (Custom PCB, Full PCBA)

#### TPS63802 Buck-Boost Regulator
- **IC:** TPS63802DLAT (TI)
- **Package:** VSON-10 (3×2mm)
- **Input:** 1.3–5.5V (full Li-Ion range)
- **Output:** 3.3V (feedback resistor divider: 510kΩ / 91kΩ)
- **LCSC:** C1849531

#### BQ24074 Battery Charger + Power Path
- **IC:** BQ24074RGTR (TI)
- **Charge Current:** 1.0A (programmed via 887Ω ISET resistor)
- **USB Input Limit:** 500mA (programmed via 1.6kΩ ISET2 resistor)
- **LCSC:** C54313

#### Battery Protection — DW01A + FS8205A
- **Protection combo:** Over-discharge, over-charge, short circuit protection.
- **FS8205A Replacement Part:** UMW FS8205A (**LCSC C351449**).

---

## 3. Physical Layout & Stacking

### Side-by-Side Enclosure Integration
The design fits a prebuilt enclosure from thegioiic (124.6 × 79.6 × 33.7 mm).
- **PCB Space:** 52.5 mm (W) × 70.0 mm (H).
- **Battery Space:** 44.8 mm (W) × 73.8 mm (H).
- The 1S2P 21700 battery pack sits to the left of the PCB.
- This side-by-side vertical configuration yields a device profile thickness under 30mm.

---

## 4. Pin Mappings (ESP32-S3-WROOM-1)

- **SPI (LoRa):** SCK=21, MOSI=38, MISO=39, CS=14, RESET=40, BUSY=41, DIO1=42, RXEN=43 (TXEN connected to DIO2).
- **I2C (OLED & IMU):** SDA=5, SCL=6, IMU_INT=3 (active-LOW, 1.8V level-shifted).
- **UART (GNSS GPS):** RX=16, TX=17, EN=15.
- **USB Data:** D−=19, D+=20.
- **Notification:** Buzzer=11 (PWM, via Q9 MOSFET), Vibrator=12 (via Q10 MOSFET).
- **Status LED:** GPIO 2 (Green LED).
- **Battery ADC:** GPIO 1 (ADC1_CH0) via 100kΩ/100kΩ voltage divider.
- **Buttons:** UP=7, SELECT=0, DOWN=8, SOS=4, POWER_LATCH=9 (latch output, active HIGH), POWER_SENSE=13 (sense input, active LOW).

---

## 5. Firmware Variant Config (`variant.h`)

Create `variants/esp32s3/treklink_v2_0/variant.h` with the following configuration:

```cpp
#pragma once

#define TREKLINK_VARIANT
#define TREKLINK_V2

// Radio: SX1268 via E22-400M22S
#define USE_SX1268
#define SX126X_CS 14
#define LORA_SCK 21
#define LORA_MOSI 38
#define LORA_MISO 39
#define SX126X_RESET 40
#define SX126X_BUSY 41
#define SX126X_DIO1 42
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_RXEN 43
#define SX126X_DIO3_TCXO_VOLTAGE 1.8
#define SX126X_MAX_POWER 22
#define LORA_CS SX126X_CS
#define LORA_DIO1 SX126X_DIO1

// I2C (OLED 0x3C + ICM-20948 0x68)
#define I2C_SDA 5
#define I2C_SCL 6
#define IMU_INT_PIN 3

// GPS (UART)
#define HAS_GPS 1
#define GPS_UBLOX
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
#define PIN_GPS_EN 15
#define GPS_EN_ACTIVE 1

// Display
#define HAS_SCREEN 1
#define USE_SSD1306

// Buttons
#define BUTTON_PIN 0      // SELECT (BOOT button w/ pull-up, active-LOW)
#define BUTTON_PIN_SOS 4  // SOS
#define BUTTON_PIN_UP 7   // UP
#define BUTTON_PIN_DOWN 8 // DOWN
#define BUTTON_PIN_POWER 13 // POWER Sense (active LOW)
#define PIN_POWER_LATCH 9   // POWER Latch (active HIGH)

// Notifications
#define PIN_BUZZER 11
#define PIN_VIBRATOR 12
#define PIN_VIBRATION PIN_VIBRATOR
#define LED_PIN 2

// Battery
#define BATTERY_PIN 1
#define ADC_CHANNEL ADC1_GPIO1_CHANNEL
#define ADC_MULTIPLIER 2.0
```