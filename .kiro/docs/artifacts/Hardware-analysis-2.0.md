# TrekLink v2.0 Hardware Analysis & Upgrade Plan

> **Revision:** v2.0 (Best Quality Prototype — Custom PCB)
> **Date:** 2026-05-14
> **Status:** RESEARCH COMPLETE — AWAITING USER INPUT

---

## Context Summary

**v1.0 (Done):** Perfboard prototype using consumer modules (ESP32-WROOM-32 + Ra-02 SX1278 + Neo-6M + SSD1306 + MPU6050), jumper wiring, plastic project box. Functional but crude.

**v2.0 Goal (Per Roadmap):** Single custom PCB, PCBA for all SMD passives, through-hole solder for modules. Best-quality components. 3D-printed enclosure. Castellated/DIP module design for repairability. Target: **investor/stakeholder demo units**.


---

## Proposed Changes

### 1. MCU — ESP32-S3-WROOM-1 (N8R8)

> [!IMPORTANT]
> This is the **single most impactful upgrade** and requires user confirmation.

#### Why Upgrade from ESP32-WROOM-32

| Factor | ESP32 (Current) | ESP32-S3-WROOM-1 |
|--------|-----------------|-------------------|
| **Architecture** | Xtensa LX6 | Xtensa LX7 (faster, more efficient) |
| **Bluetooth** | BT 4.2 | **BT 5.0 LE** (better range for app pairing) |
| **Flash/PSRAM** | 4MB flash / no PSRAM | **8MB flash / 8MB PSRAM** (N8R8) |
| **USB** | Requires external CP2102 | **Native USB-OTG** (no UART-USB bridge chip!) |
| **GPIO Count** | 34 (many restricted) | **45** (more flexible) |
| **Power** | Higher idle current | Better deep sleep optimization |
| **Meshtastic Status** | Legacy / Supported | **Recommended / Active development** |
| **HTTP Server** | Deprecated | Fully supported |
| **JLCPCB/LCSC** | Available | **Available** (Extended parts) |

#### Firmware Impact

- Meshtastic firmware **fully supports ESP32-S3** — many official boards already use it (Heltec V3, T-Deck, XIAO S3)
- Need to create `variants/esp32s3/treklink_v2_0/variant.h` (different from current `variants/esp32/treklink_v1_0/`)
- SPI pin mapping is flexible on S3 (any GPIO can be SPI)
- Native USB eliminates CP2102 from BOM entirely
- **Code change effort: LOW** — only `variant.h` and `platformio.ini` need updates

#### Risk

- GPIO numbering differs → requires new pin mapping
- Some ESP32 strapping pin constraints change on S3
- Existing v1.0 variant won't compile for S3 (expected; they're separate targets)

---

### 2. LoRa Radio — SX1262 Module (E22-400T22D or Ra-01SC/SC-H)

> [!WARNING]
> **SX1278 (Ra-02) is now legacy in Meshtastic.** Firmware support is diminishing. Upgrading to SX1262 is **strongly recommended** for v2.0.

#### Why SX1262 over SX1278

| Factor | SX1278 (Ra-02, Current) | SX1262 (Recommended) |
|--------|-------------------------|----------------------|
| **RX Current** | ~12mA | **~5mA** (58% reduction!) |
| **Max TX Power** | +20 dBm | **+22 dBm** |
| **Sensitivity** | -148 dBm | **-148 dBm** (comparable) |
| **Spreading Factors** | SF6–SF12 | **SF5–SF12** (faster modes) |
| **TCXO Support** | External only | **Built-in configuration** |
| **Meshtastic Status** | Legacy | **Recommended standard** |
| **RadioLib Support** | Full | **Full (optimized)** |
| **JLCPCB** | Available | **Available** |

#### Module Options for Through-Hole v2.0

| Module | Chip | Interface | Freq | IPEX | Notes |
|--------|------|-----------|------|------|-------|
| **EBYTE E22-400T22D** | SX1262 | SPI | 410-493MHz | Yes | ✅ Most popular for Meshtastic DIY |
| **Ai-Thinker Ra-01SC** | SX1262 | SPI | 410-525MHz | Yes | Direct successor to Ra-02 |
| **CDEBYTE E22-400M22S** | SX1262 | SPI | 410-493MHz | Yes | Castellated (for v3.0) |

#### Firmware Impact

- Meshtastic natively supports SX1262 via RadioLib
- `variant.h` changes: `USE_SX1262` instead of `USE_RF95`
- New pins required: `LORA_DIO1` and `LORA_BUSY` (SX1262 uses DIO1 not DIO0)
- **Code change effort: LOW** — standard Meshtastic variant configuration

```cpp
// v2.0 variant.h (SX1262)
#define USE_SX1262
#define LORA_SCK  <pin>
#define LORA_MISO <pin>
#define LORA_MOSI <pin>
#define LORA_CS   <pin>
#define LORA_DIO1 <pin>   // NEW: was DIO0 on SX1278
#define LORA_BUSY <pin>   // NEW: SX1262-specific
#define LORA_RESET <pin>
#define SX126X_DIO2_AS_RF_SWITCH  // If module uses DIO2 for TX/RX switch
```

---

### 3. GPS — u-blox NEO-M9N

> [!TIP]
> The NEO-M9N is pin-compatible with NEO-6M/M8N but offers **dramatically better performance**.

| Factor | Neo-6M (Current) | NEO-M9N (Recommended) |
|--------|-------------------|----------------------|
| **Constellations** | GPS only | **GPS + GLONASS + BeiDou + Galileo** (concurrent 4-system) |
| **Accuracy (CEP)** | ~2.5m | **~1.5m** |
| **Cold Start** | 26s | **24s** |
| **Hot Start** | 1s | **2s** (comparable) |
| **Sensitivity** | -161 dBm | **-167 dBm** |
| **Anti-Jamming** | No | **Yes (built-in)** |
| **Interface** | UART | UART (same) |
| **Supply** | 3.3V | 3.3V (same) |
| **JLCPCB** | Module N/A | Module available |

#### Module Options

| Module | Notes |
|--------|-------|
| **u-blox NEO-M9N breakout** | Best accuracy, ~$15-20 |
| **ATGM336H** | Budget multi-GNSS, ~$3-5, good enough for v3.0/v4.0 |
| **Beitian BN-880Q** | NEO-M9N + compass, ~$20-25 |

**Recommendation:** NEO-M9N breakout module for v2.0 (premium tier). No firmware changes needed (same NMEA UART protocol).

---

### 4. IMU — MPU6050 → BMI270 or ICM-20948

| Factor | MPU6050 (Current) | BMI270 | ICM-20948 |
|--------|-------------------|--------|-----------|
| **Axes** | 6 (Accel+Gyro) | 6 (Accel+Gyro) | **9 (Accel+Gyro+Mag)** |
| **Power** | ~3.9mA | **~0.68mA** | ~1.2mA |
| **Resolution** | 16-bit | 16-bit | 16-bit |
| **DMP** | Basic | Advanced | Advanced |
| **Step Counter** | No | **Yes (hardware)** | Yes |
| **I2C** | 0x68 | 0x68 (same!) | 0x68/0x69 |
| **JLCPCB/LCSC** | Available | Available | Available |
| **Meshtastic** | Supported | Supported | **Supported** (lib included) |

#### Recommendation

**ICM-20948** for v2.0 (premium):
- 9-axis = built-in compass for heading display (REQ-NAV-03 direction arrows)
- Lower power than MPU6050
- Meshtastic already includes SparkFun ICM-20948 library in `platformio.ini`
- Same I2C address (0x68) → **no bus conflict**
- Future dead reckoning benefits from magnetometer

**Firmware impact:** Minimal. Meshtastic already has ICM-20948 support via the SparkFun library included in the build. Just need to enable it in variant configuration.

---

### 5. Power Management — TPS63802 (Custom Circuit on PCB)

Per roadmap: analog/power circuits are **fully custom-designed on PCB** for v2.0.

#### Design

```
Battery (3.0V-4.2V) → TPS63802 (buck-boost) → 3.3V Rail
```

| Component | Part Number | Notes |
|-----------|-------------|-------|
| **Buck-Boost IC** | TPS63802DLAT | 3mm×2mm VSON-10, available on LCSC |
| **Inductor** | 0.47µH | Per TI reference design |
| **Output Cap** | 22µF ceramic | X5R/X7R |
| **Feedback Dividers** | Per TI WEBENCH calc | For 3.3V output |

This replaces the Mini360/MT3608 modules entirely.

#### Battery Charging — TP5100 → TP4056 or BQ24074

| Charger | TP5100 (Current) | TP4056 | BQ24074 (TI) |
|---------|-------------------|--------|-------------|
| **Type** | Module | Bare IC | Bare IC |
| **Chemistry** | 1S/2S Li-Ion | 1S Li-Ion | 1S Li-Ion |
| **Charge Current** | 2A | 1A (adjustable) | 1.5A |
| **USB Input** | Micro-USB | Micro-USB | **Any (USB-C)** |
| **JLCPCB** | N/A (module) | **Available** | **Available** |
| **Power Path** | No | No | **Yes** (load + charge simultaneously) |
| **Cost** | ~$1 module | ~$0.10 IC | ~$2.50 IC |

**Recommendation for v2.0:**
- Use **BQ24074** for premium power path management (charge + operate simultaneously via USB-C)
- Fall back to **TP4056** if BQ24074 layout complexity is too high for first revision
- USB-C connector (GCT USB4105-GF-A or similar, available on LCSC)

#### Battery Protection

Add a **DW01A + FS8205A** dual-MOSFET protection IC combo (standard Li-Ion protection). Available on LCSC. Protects against:
- Over-discharge (<2.4V)
- Over-charge (>4.3V)
- Over-current
- Short circuit

---

### 6. Display — SSD1306 0.96" OLED (Keep)

**No change recommended for v2.0.** The SSD1306 is:
- Well-supported by Meshtastic
- Low power (~20mA active, 0µA sleep via I2C command)
- Adequate resolution for SAR use case
- Available as through-hole module

> [!NOTE]
> For v3.0/v4.0, consider upgrading to **SH1106 1.3"** or **SSD1309 1.54"** for better readability.

---

### 7. Enclosure — 3D Printed (Local Vietnam)

Per answers.md: 3D print locally in HCMC.

**v2.0 specs:**
- Same form factor ~125×80×32mm
- Material: **PETG** (better heat/UV resistance than PLA)
- Features to design:
  - Button cutouts (4x tactile buttons flush mount)
  - SMA antenna pass-through (right side)
  - GPS antenna window (top — thin PETG ~0.8mm or open)
  - USB-C port opening (right side)
  - Battery compartment access (bottom, screw-on lid)
  - Display window (top face)
  - Mounting holes (M3 standoffs aligning with PCB)

---

## PCB Design Summary for JLCPCB Order

### What Goes on PCB (PCBA by JLCPCB)

| Component | Type | Assembly |
|-----------|------|----------|
| **ESP32-S3-WROOM-1-N8R8** | SMD castellated module | **PCBA** |
| **E22-400M22S LoRa** | SMD castellated module | **PCBA** |
| **NEO-M9N GNSS** | SMD castellated module (LCC-24) | **PCBA** |
| **ICM-20948 IMU** | SMD QFN-24 chip | **PCBA** |
| **TPS63802 buck-boost** | SMD VSON-10 IC + passives | **PCBA** |
| **BQ24074 charger** | SMD QFN-16 IC + passives | **PCBA** |
| **DW01A + FS8205A** | SMD protection IC + Dual MOSFET | **PCBA** |
| **USB-C Connector** | SMD 16-pin (USB4105-GF-A) | **PCBA** |
| **IPEX/U.FL receptacles** | SMD coaxial RF connector (C130006) x2 | **PCBA** |
| **Passives & Transistors** | SMD resistors/caps (0402/0603/0805), NPN, diodes, TVS | **PCBA** |

### What Gets Hand-Soldered / Mounted Separately (v2.0 Assembly)

| Component | Mounting | Notes |
|-----------|----------|-------|
| **SSD1306 OLED** | 4-pin straight female header | Plugs into female header on top PCB side |
| **4× Tactile buttons** | Through-hole (12×12mm waterproof) | Placed at bottom/sides, protrudes through case |
| **Slide Switch** | Through-hole (SS-22F32 slide switch) | Main power switch |
| **Buzzer** | Through-hole (9×4.2mm passive) | Sound notification |
| **Vibration motor** | Wire-to-pad (10mm coin) | Glued to shell, wires soldered to pads |
| **CR1220 battery** | Tabbed coin cell (solder tabs) | GPS backup battery, soldered flat to PCB pads |
| **Battery holder** | Wire-to-pad or JST connector | 2P parallel 21700 holder |
| **SMA connectors** | Panel-mount SMA female with pigtail | Mounts to enclosure, plugs into PCB IPEX receptacles |

---

## Firmware `variant.h` Changes (v1.0 → v2.0)

```diff
- // ESP32-WROOM-32 + Ra-02 SX1278
+ // ESP32-S3-WROOM-1 + SX1262

- #define USE_RF95              // RFM95/SX127x
+ #define USE_SX1262            // SX126x series

- #define LORA_DIO0 26          // SX1278 interrupt
+ #define LORA_DIO1 <pin>       // SX1262 interrupt
+ #define LORA_BUSY <pin>       // SX1262 busy indicator

  // GPS, I2C, buttons, buzzer, vibrator — new GPIO numbers TBD based on S3 pinout
```

---

## Open Questions

> [!IMPORTANT]
> These need your input before I can proceed to create the formal v2.0 requirements and design specs.

### Q1: MCU Upgrade — ESP32-S3 or Keep ESP32?
The ESP32-S3 is strongly recommended for v2.0 based on research:
- Future-proof Meshtastic support
- Native USB (eliminates CP2102 chip)
- More GPIO flexibility for PCB routing
- Better BT 5.0 for app connectivity

**Are you comfortable with ESP32-S3-WROOM-1 (N8R8)?** The v1.0 firmware variant will need a new variant directory but pin mapping is straightforward.

### Q2: LoRa Radio — SX1262 Confirmed?
SX1278 is legacy in Meshtastic. Upgrading to SX1262 is critical for:
- 58% lower RX power consumption
- Active firmware development
- Better RF performance

**Which SX1262 module do you prefer?**
- A) **EBYTE E22-400T22D** — most Meshtastic DIY community experience
- B) **Ai-Thinker Ra-01SC** — direct successor to your current Ra-02
- C) Other preference?

### Q3: GPS — NEO-M9N or NEO-M8N?
- NEO-M9N = premium (4-constellation, anti-jamming, ~$15-20)
- NEO-M8N = mid-tier (3-constellation, ~$8-12)
- ATGM336H = budget (for v3.0/v4.0, ~$3-5)

**Confirm NEO-M9N for v2.0?**

### Q4: IMU — ICM-20948 or Keep MPU6050?
- ICM-20948 = 9-axis (compass for heading), lower power, already in Meshtastic lib
- MPU6050 = proven, simpler, sufficient for fall detection

**Do you want the compass heading feature?** If yes → ICM-20948. If fall detection only → MPU6050 is fine.

### Q5: Battery Charger IC
- **BQ24074 (TI):** Premium, power path, USB-C friendly, ~$2.50
- **TP4056:** Budget, simple, proven, ~$0.10

**BQ24074 for v2.0 (premium tier)?**

### Q6: Display Size
- **0.96" SSD1306** (current, proven)
- **1.3" SH1106** (bigger, same power, better readability)

**Upgrade display for v2.0 or keep 0.96"?**

### Q7: PCB Layers
- **2-layer:** Cheaper, simpler, fine for this complexity
- **4-layer:** Better ground plane for RF, easier routing, ~$5 more per board

**For best quality, 4-layer is recommended for clean RF/power separation.** Agree?

### Q8: Budget Allocation for v2.0
Per answers.md: ~800k VND (~$32 USD) per version, 3-10 units per batch.
- PCB + PCBA (5 boards): ~$50-80 on JLCPCB
- Components (modules × 5): ~$75-100
- 3D printing (5 enclosures): ~$15-30

**Is ~$160-210 total for 5 units acceptable for v2.0?**

### Q9: PCB Design Tool — Flux AI
You mentioned Flux AI for PCB design. Have you:
- A) Created a Flux account?
- B) Tested it with any project?
- C) Want me to prepare a complete schematic specification for Flux input?

### Q10: Form Factor & Mounting
The v2.0 keeps ~125×80×32mm. Questions:
- **Should the OLED be top-mounted (visible through case lid)?**
- **Button placement: front face or side?**
- **SMA connector: right side (as v1.0) or top?**
- **Do you have a 3D printer available, or will you use a local print service?**


1. Please use the currency of VND. Actual funds for the development can be seen in "./artifacts/actual-available-funds.md".
2. PCB design tool: EasyEDA Pro. I am trying Flux.ai now (trial)
I currently have no PCB design experience and would like some.
3. For PCBA, these following components must be:
Small SMD passives, SMD ICs/Chips (i do not have a heatgun or a heatpad)
=> But, still prioritize full PCBA for streamline development
4. Start using castellated modules, and bare ics from this version, do use recommended absolute best tier components out there like ESP32 S3 and so on
5. Keep battery format (21700), case must be as slim as possible (up to 25-28mm, maximum 30mm thick)
6. 3d Print in Vietnam with VAT support.
7. Target smaller platform (because its 3d printed case, as slim as possible while still rugged), use rugged materials, at least have a 21mm inner shell height clearnace + 5mm nudge for tolerances and faults, so 26-28mm in clearance -> 30mm thick (maximum). It is pocket form factor so it must follow length first width after (100-120mm length along with 70-80mm width). In a nutshell, kinda like a phone. Prioritize battery life and ruggedness
8. No formal RF testing. Not in scope
9. the v2.0 is intended for internal development, that we try to produce the best components we can for auditing the next versions.
10. Prosumer-grade, premium like TI chips, esp32 high performance, high quality chips for GPS, and so on
11. Budget is tight, allow only 1 revision with each batch. Proposed batch size is 5 units (including the 1 revision)
12. The service provider (PCBWay,JLCPCB) or Flux.ai (user based, will need the service provider for completion anyways)
the cost of service is no more than 13 million vnd. aprrox 240 USD.

---

## Verification Plan

### Pre-PCB Verification
- Component availability check on LCSC for all SMD parts
- Schematic review against Meshtastic variant requirements
- DRC/ERC pass in PCB design tool

### Post-Assembly Verification
- Power rail voltage verification (TPS63802 → 3.3V ±0.05V)
- SPI communication test (ESP32-S3 ↔ SX1262)
- UART GPS test (NMEA parsing)
- I2C bus scan (OLED 0x3C + IMU 0x68)
- LoRa TX/RX test with existing v1.0 device
- Battery charge/discharge cycle test
- Meshtastic app Bluetooth pairing test
