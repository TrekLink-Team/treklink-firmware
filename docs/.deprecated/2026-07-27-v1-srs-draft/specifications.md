# TrekLink System Requirements Specification (SRS)

**Project Code:** EXE101-G1-TREKLINK  
**Version:** 1.0 (Meshtastic Fork Implementation)  
**Date:** February 4, 2026  
**Status:** APPROVED FOR DEVELOPMENT  
[DEPRECATED]
---

## 1. Introduction

### 1.1 Purpose

TrekLink is an off-grid, decentralized communication device designed for hikers, survivalists, and rescue teams operating in "Dead Zones" (areas with no cellular coverage). It utilizes **Meshtastic firmware v2.6.x** as the base platform with custom TrekLink enhancements for fall detection, power management, and hybrid encryption modes.

### 1.2 Scope

**Version 2.0** represents a radical architectural change from proprietary firmware to a Meshtastic v2.6.x fork. This document supersedes all previous specifications and serves as the **Single Source of Truth (SSOT)** for TrekLink development.

**Key Changes from v1.0:**
- ✅ **Radio Module:** Ebyte E32-433T20D (UART) → **Ra-02 (SX1278) 433MHz SPI**
- ✅ **Firmware Base:** Proprietary ESP-IDF → **Meshtastic v2.6.x fork**
- ✅ **Mesh Protocol:** Custom Managed Flooding → **Meshtastic routing algorithm**
- ✅ **Radio Driver:** Custom UART commands → **RadioLib SPI library**
- ❌ **PRFH:** Pseudo-Random Frequency Hop removed from MVP (deferred)
- ✅ **Encryption:** Meshtastic PKC + optional TrekLink AES-128-GCM layer

**Two-Phase Delivery Strategy:**
- **Phase 1 (PRIORITY):** Working Meshtastic-compatible device with basic features (~16 hours)
- **Phase 2 (OPTIONAL):** Custom TrekLink protocols (fall detection, hybrid encryption) (~20 hours)

If Phase 2 is incomplete, the device remains a functional **generic Meshtastic node**.

---

## 2. System Architecture

### 2.1 Hardware Architecture

```mermaid
graph TB
    subgraph Power
        BATT[2x 21700 Li-Ion<br/>10000mAh Parallel]
        TP5100[TP5100 Charger<br/>USB-C Input]
        MINI360[Mini360 Buck<br/>3.3V/5V Rails]
        BATT --> TP5100
        TP5100 --> MINI360
    end

    subgraph Core
        ESP32[ESP32-WROOM-32<br/>Core 0: LoRa/Mesh<br/>Core 1: UI/Sensors]
        MINI360 --> ESP32
    end

    subgraph Radio
        RA02[Ra-02 SX1278<br/>433MHz SPI]
        ANT433[433MHz Antenna<br/>17.5cm Whip SMA]
        ESP32 -- SPI --> RA02
        RA02 --> ANT433
    end

    subgraph Sensors
        GPS[Neo-6M GPS<br/>UART1]
        MPU[MPU6050 IMU<br/>I2C 0x68]
        ESP32 -- UART1 --> GPS
        ESP32 -- I2C --> MPU
    end

    subgraph UI
        OLED[SSD1306 OLED<br/>0.96" I2C 0x3C]
        BTNS[4x Buttons<br/>GPIO 25,32,34,35]
        BUZZ[Passive Buzzer<br/>GPIO 33]
        VIB[Vibrator Motor<br/>GPIO 4]
        LED[Status LED<br/>GPIO 2]
        ESP32 -- I2C --> OLED
        ESP32 --> BTNS
        ESP32 --> BUZZ
        ESP32 --> VIB
        ESP32 --> LED
    end

    %% Power Gating REMOVED - Meshtastic firmware handles power management
    %% GPIO 13 and GPIO 23 are no longer used for hardware power gating
```

---

## 3. Hardware Specifications

### 3.1 Bill of Materials (BOM)

| Category | Component | Part Number | Specs | Purpose |
|----------|-----------|-------------|-------|---------|
| **MCU** | ESP32-WROOM-32 | ESP32-DevKit-V1 | Dual-core 240MHz, 520KB RAM, Wi-Fi/BT | Main processor |
| **Radio** | **Ra-02 LoRa Module** | **Ra-02 (SX1278)** | **433MHz, SPI, 100mW (20dBm)** | **Mesh networking** |
| **GPS** | Neo-6M GPS Module | NEO-6M | UART, NMEA 9600 baud, internal backup battery | Position tracking |
| **IMU** | MPU6050 6-Axis | MPU-6050 | I2C 0x68, 3-axis gyro + accel | Fall detection |
| **Display** | OLED 0.96" | SSD1306 | 128x64, I2C 0x3C, white | User interface |
| **Power** | 21700 Li-Ion Cells | INR21700-M50T | 2x 5000mAh = 10Ah total, 3.7V nominal | Main battery |
| **Charger** | TP5100 Module | TP5100 | 2S Li-Ion, USB-C input, 1A charge | Battery management |
| **Regulator** | Mini360 Buck | HM-Mini-360 | 3.3V + 5V rails | Voltage regulation |
| **MOSFET** | P-Channel MOSFET | IRF9530N | High-side switch for GPS power | Power gating |
| **Transistor** | NPN Transistor | S8050-D | Gate driver for MOSFET | Level shifting |
| **Audio** | Passive Buzzer | 9056-TS | 3.3V PWM, 2.7kHz resonant | Audible alerts |
| **Haptic** | Vibration Motor | Coin Type 10mm | 3.3V coin vibrator | Haptic feedback |
| **LED** | Status LED | Built-in | Blue LED  (ESP32 GPIO 2) | Visual indicator |
| **Buttons** | Tactile Switches | 6x6mm tactile | 4x buttons (MENU, SOS, UP, DOWN) | User input |
| **Antenna** | 433MHz Whip | Generic SMA | 17.5cm quarter-wave | RF transmission |
| **Enclosure** | IP67 Box | Generic | 125x80x32mm internal | Physical protection |

**Total Estimated Cost:** ~$45 USD (BOM only, excluding enclosure fabrication)

---

### 3.2 GPIO Pinout Allocation

#### Final TrekLink Pin Configuration

| Function | GPIO | Direction | Notes |
|----------|------|-----------|-------|
| **LoRa Ra-02 (SPI)** | | | |
| LORA_SCK | 5 | Output | SPI Clock (Meshtastic DIY default) |
| LORA_MISO | 19 | Input | SPI Master In Slave Out |
| LORA_MOSI | 27 | Output | SPI Master Out Slave In |
| LORA_CS | 18 | Output | SPI Chip Select |
| LORA_DIO0 | 26 | Input | LoRa Interrupt (Packet RX/TX done) |
| LORA_RESET | 14 | Output | LoRa Module Reset |
| **I2C Bus (Shared)** | | | |
| I2C_SDA | 21 | Bidirectional | OLED + MPU6050 |
| I2C_SCL | 22 | Output | I2C Clock |
| **GPS Neo-6M (UART1)** | | | |
| GPS_RX | 16 | Input | ESP32 TX1 → GPS RX |
| GPS_TX | 17 | Output | GPS TX → ESP32 RX1 |
| **User Interface** | | | |
| BUTTON_MENU | 25 | Input | Menu navigation + Silent Mode trigger |
| BUTTON_UP | 32 | Input | Scroll up / increment |
| BUTTON_DOWN | 35 | Input Only | Scroll down / decrement |
| BUTTON_SOS | 34 | Input Only | SOS trigger / Location ping |
| **Notifications** | | | |
| PIN_BUZZER | 33 | Output | Passive buzzer PWM |
| PIN_VIBRATOR | 4 | Output | Vibration motor (via NPN) | | LED_PIN | 2 | Output | Built-in blue LED |
| **Power Management** | | | |
| PIN_GPS_PWR_EN | 13 | Output | GPS P-MOSFET gate (via NPN driver) |
| PIN_OLED_GND_EN | 23 | Output | OLED GND switch (Silent Mode) |
| BATTERY_PIN | 36 | Input Only | ADC1_CH0 for battery voltage sensing |

**Strapping Pin Safety:**
- ✅ GPIO 0, 2, 5: Used safely (no boot conflicts)
- ✅ GPIO 12, 15: **AVOIDED** (boot voltage strapping pins)

**I2C Device Addresses:**
- OLED SSD1306: **0x3C**
- MPU6050: **0x68** (AD0 pin tied to GND)

---

### 3.3 Power Budget Analysis

| Component | Active Current | Sleep Current | Duty Cycle | Average |
|-----------|---------------|---------------|------------|---------|
| ESP32 (dual-core active) | 160mA | 5mA (light sleep) | 20% | 37mA |
| Ra-02 (TX 20dBm) | 120mA | 10mA (standby) | 2% | 12.4mA |
| Ra-02 (RX) | 15mA | - | 10% | 1.5mA |
| GPS Neo-6M (tracking) | 45mA | 20mA (backup mode) | 30% | 27.5mA |
| OLED SSD1306 | 20mA | 0mA (power-gated) | 40% | 8mA |
| MPU6050 | 3.5mA | 10µA (cycle mode) | 100% | 3.5mA |
| Vibrator (alerts) | 80mA | 0mA | 1% | 0.8mA |
| Buzzer (alerts) | 30mA | 0mA | 1% | 0.3mA |
| **TOTAL AVERAGE** | | | | **~91mA** |

**Battery Life Estimate:**
- Capacity: 10,000mAh (2x 5000mAh 21700 cells in parallel)
- Continuous Usage: 10,000mAh / 91mA = **~110 hours (~4.5 days)**
- Deep Sleep Mode (GPS off, OLED off): 10,000mAh / 25mA = **~400 hours (~16 days)**

**Power Saving Strategies (Phase 1):**
- Meshtastic automatic power management
- Light sleep between CAD intervals
- GPS power gating via GPIO 13

**Enhanced Power Saving (Phase 2):**
- OLED Silent Mode (GPIO 23 GND switch)
- Adaptive TX power based on RSSI
- IMU wake-on-motion

---

### 3.4 Physical Design

**Enclosure Dimensions:**
- External: 125mm x 80mm x 32mm
- Internal: 120mm x 74mm x 28mm
- Water Rating: IP67 (dust-tight, 1m submersion for 30 min)

**Component Placement:**
```
┌─────────────────────────────────┐
│    [GPS Antenna - Top]          │  ← GPS module internal antenna
├─────────────────────────────────┤
│                                 │
│  ┌──────────────────────────┐  │
│  │   OLED Display 0.96"     │  │
│  │   128 x 64 pixels        │  │
│  └──────────────────────────┘  │
│                                 │
│  ┌──────────────────────────┐  │
│  │   [SOS BUTTON - RED]     │  │  ← 3s hold = SOS, 1 click = Ping
│  └──────────────────────────┘  │
│                                 │
│    [UP ▲]          [DOWN ▼]    │  ← Menu navigation
│                                 │
└─────────────────────────────────┘

RIGHT SIDE:
┌───┐
│ M │  ← MENU button (1s hold = Silent Mode)
│ E │
│ N │
│ U │
└───┘
[ANT] ← LoRa 433MHz antenna (SMA connector, 17.5cm whip)

BOTTOM:
[USB-C] [DATA]  ← Charging + Serial debug (silicone flap)
```

---

## 4. Software Architecture (Meshtastic Fork)

### 4.1 Firmware Base: Meshtastic v2.6.x

**Architecture Overview:**
```
┌─────────────────────────────────────────────────────────┐
│                   Application Layer                     │
│  - Meshtastic BaseUI (Screen.cpp)                       │
│  - GPS Service (GPSStatus.cpp)                          │
│  - Power Management (PowerStatus.cpp)                   │
│  - [Phase 2] TrekLink Custom Modules                    │
├─────────────────────────────────────────────────────────┤
│                 Meshtastic Core Services                │
│  - Router.cpp (Mesh packet routing)                     │
│  - RadioInterface.cpp (RadioLib abstraction)            │
│  - Channels.cpp (Encryption key management)             │
│  - NodeDB.cpp (Mesh node database)                      │
├─────────────────────────────────────────────────────────┤
│                   Radio Abstraction Layer               │
│  - RadioLib (SX1278 SPI driver for Ra-02)               │
│  - CAD (Channel Activity Detection)                     │
│  - CSMA (Carrier Sense Multiple Access)                 │
├─────────────────────────────────────────────────────────┤
│                    Hardware Abstraction                 │
│  - TrekLink Variant (variants/esp32/treklink/variant.h) │
│  - GPIO definitions, I2C/SPI bus config                 │
│  - Button/LED/Power gate pin mappings                   │
└─────────────────────────────────────────────────────────┘
```

### 4.2 Custom TrekLink Modules (Phase 2 Optional)

If Phase 2 is implemented, add these modules:

**1. TrekLinkButtonModule.cpp**
- Multi-button support (MENU, SOS, UP, DOWN)
- Silent Mode toggle (MENU hold 1s)
- SOS trigger logic (SOS hold 3s)
- Matrix request (SOS double-click)

**2. FallDetectionModule.cpp**
- MPU6050 monitoring (freefall → impact → inactivity)
- Pre-alarm state (30s countdown with haptic/audio warning)
- Auto-SOS trigger if not cancelled
- Integrates with Meshtastic MeshService to send SOS packets

**3. TrekLinkProtocol.cpp (Hybrid Encryption)**
- **TrekLink Private Mode:** AES-128-GCM + Channel ID encryption
- **Meshtastic Public Mode:** Standard Meshtastic PKC only
- **SOS Mode:** Always uses Meshtastic standard packets (universal interoperability)

### 4.3 ESP32 Dual-Core Allocation (Meshtastic Default)

| Core | Tasks | Purpose |
|------|-------|---------|
| **Core 0** | Meshtastic Radio, Mesh routing, Packet processing | Time-critical LoRa operations |
| **Core 1** | UI rendering, GPS parsing, Sensors, User input | Non-critical user-facing tasks |

**FreeRTOS Task Priority:**
- RadioTask: Priority 2 (highest)
- MeshRoutingTask: Priority 1
- UITask: Priority 0 (lowest)

---

## 5. Functional Requirements (EARS Format)

### 5.1 Mesh Networking (REQ-COM)

**REQ-COM-01: Meshtastic Routing Algorithm**

**User Story:** As a user, I want my messages to reach their destination even if I don't have direct line of sight.

**Acceptance Criteria:**
1. **[Phase 1 CRITICAL]** The system **SHALL** utilize **Meshtastic firmware v2.6.x routing algorithm** (improving managed flooding) as the base mesh protocol.
2. **WHEN** a packet is received for another node, **THEN** the system **SHALL** re-broadcast according to Meshtastic's routing logic (hop count decrement, seen buffer deduplication).
3. **IF** a packet has been seen (based on Message ID in RecentPackets), **THEN** the system **SHALL** drop the packet to prevent broadcast storms.
4. **WHILE** the device is ON, **THEN** it **SHALL** listen using RadioLib's Channel Activity Detection (CAD).
5. **[Phase 2 Optional]** The system **MAY** implement custom TrekLink packet types for private communication.

---

**REQ-COM-02: Packet Structure (Meshtastic Protobuf)**

**User Story:** As a developer, I want standardized packet formats for interoperability.

**Acceptance Criteria:**
1. **[Phase 1]** The system **SHALL** use **Meshtastic Protobuf packet structure** (MeshPacket).
2. **WHEN** in **Meshtastic Public mode**, **THEN** use standard MeshPacket format.
3. **[Phase 2 Optional]** WHEN in **TrekLink Private mode**, **THEN** encapsulate custom AES-GCM payloads in PortNum_PRIVATE_APP.
4. **IF** payload exceeds max size, **THEN** the system **SHALL** reject the message (no fragmentation in MVP).

---

**REQ-COM-03: Message Types**

**User Story:** As a user, I want different communication modes.

**Acceptance Criteria:**
1. **[Phase 1]** Support **Meshtastic standard message types**: TEXT, POSITION_APP, NODEINFO_APP.
2. **WHEN** SOS is triggered, **THEN** broadcast **Meshtastic POSITION_APP packet** for universal compatibility.
3. **[Phase 2 Optional]** Support custom TrekLink encrypted messages via PRIVATE_APP.
4. **[DEFERRED]** ~~SOS double-click Matrix Check~~ — removed from MVP per user directive.

---

### 5.2 Safety & Emergency (REQ-SAF)

**REQ-SAF-01: SOS Activation**

**User Story:** As a user, I want a quick way to signal distress.

**Acceptance Criteria:**
1. **WHEN** SOS button is Held for 3 seconds, **THEN** trigger SOS: broadcast Meshtastic POSITION packet + loud buzzer + strobing LED.
2. **WHEN** SOS button is Clicked once, **THEN** broadcast current location (ping).
3. **WHILE** in SOS mode, **THEN** transmit continuously for 1 minute, then beacon every 30 seconds.
4. **WHEN** SOS button is Held for 3 seconds during active SOS, **THEN** cancel and confirm with vibration.
5. **IF** SOS is triggered, **THEN** bypass power control and transmit at maximum power.

---

**REQ-SAF-02: Fall Detection (Phase 2 Optional)**

**User Story:** As a user, I want automatic help if I fall and become unresponsive.

**Acceptance Criteria:**
1. **[Phase 2 Optional]** WHILE device is ON, **THEN** monitor MPU6050 for fall signature (freefall > 0.5s, impact > 3G, inactivity 10s).
2. **WHEN** fall is detected, **THEN** enter "Pre-Alarm" with haptic/visual warning for 30 seconds.
3. **WHILE** in Pre-Alarm, **THEN** sound alarm and vibrate continuously.
4. **IF** user presses any button during Pre-Alarm, **THEN** cancel auto-SOS.
5. **IF** user does NOT cancel within 30s, **THEN** automatically trigger Meshtastic SOS.

---

### 5.3 Security & Encryption (REQ-SEC)

**REQ-SEC-01: Encryption Modes**

**User Story:** As a user, I want secure communications.

**Acceptance Criteria:**
1. **[Phase 1]** Use **Meshtastic PKC (Public Key Cryptography)** for channel encryption.
2. **[Phase 2 Optional]** Add **AES-128-GCM** layer on top of PKC for TrekLink Private mode.
3. **WHEN** in **Meshtastic Public mode** (SOS), **THEN** use only Meshtastic PKC.
4. **WHEN** in **TrekLink Private mode**, **THEN** use Channel ID as AES-GCM seed.
5. **IF** decryption fails, **THEN** discard packet silently.

---

**REQ-SEC-02: Anti-Jamming (DEFERRED)**

**User Story:** As a tactical user, I want resistance to jamming.

**Acceptance Criteria:**
1. **[DEFERRED]** PRFH (Pseudo-Random Frequency Hopping) is **removed from MVP** due to time sync complexity.
2. **[Phase 1]** Operate on static 433MHz frequency (EU_433 region).
3. **[Future]** PRFH may be implemented if GPS time sync and LCG distribution are solved.

---

### 5.4 User Interface (REQ-UI)

**REQ-UI-01: OLED Dashboard**

**User Story:** As a user, I want clear status information.

**Acceptance Criteria:**
1. **[Phase 1]** The system **SHALL** display **Meshtastic BaseUI** on boot.
2. **WHEN** Meshtastic UI is active, **THEN** display: Signal strength, Node count, Battery %, GPS status, Clock.
3. **[Phase 2 Optional]** The system **MAY** switch to custom TrekLink UI (dashboard, map, message presets).
4. **WHEN** Silent Mode is activated (MENU hold 1s), **THEN** the system **SHALL** disable OLED display via Meshtastic screen power management.

---

**REQ-UI-02: Button Functions (Phase 1 Simplified)**

**Acceptance Criteria:**
1. **MENU button:** Navigate Meshtastic menus
2. **UP/DOWN buttons:** Scroll menu items
3. **SOS button (1 click):** Send position broadcast
4. **SOS button (3s hold):** Trigger SOS mode

---

### 5.5 Power Management (REQ-PWR)

**REQ-PWR-01: Power States**

**User Story:** As a user, I want extended battery life.

**Acceptance Criteria:**
1. **[Phase 1]** Use **Meshtastic auto power management** (light sleep between CAD).
2. **WHEN** GPS is not actively needed, **THEN** use Meshtastic GPS power management (firmware-controlled sleep).
3. **[REMOVED]** ~~Hardware power gating (GPIO 13/23) removed~~ — Meshtastic firmware handles power saving via sleep modes.
4. **WHILE** battery voltage drops below 3.2V/cell, **THEN** enter deep sleep and disable non-critical peripherals.

---

**REQ-PWR-02: Battery Monitoring**

**Acceptance Criteria:**
1. **[Phase 1]** Read battery voltage via GPIO 36 (ADC1_CH0) using voltage divider.
2. **WHEN** battery level changes by >5%, **THEN** update Meshtastic PowerStatus telemetry.
3. **IF** battery drops below 10%, **THEN** display low-battery warning and reduce TX power.

---

## 6. Meshtastic Configuration

### 6.1 Region Settings

```yaml
Region: EU_433
Frequency: 433.0 - 434.0 MHz (EU SRD Band)
Bandwidth: 125 kHz
Spreading Factor: SF7 (default, ~5.5km range)
Coding Rate: 4/5
TX Power: 20 dBm (100mW)
Duty Cycle: <10% (EU regulatory compliance)
```

### 6.2 Channel Configuration

**Default Primary Channel:**
```plaintext
Name: "TrekLink-01"
PSK: [32-byte pre-shared key]
Role: PRIMARY
Encryption: AES256 (Meshtastic PKC)
```

**Optional Secondary Channel (Phase 2):**
```plaintext
Name: "TrekLink-Private"
PSK: [Custom Channel ID-derived key]
Role: SECONDARY
Encryption: Meshtastic PKC + TrekLink AES-128-GCM
```

---

## 7. Implementation Phases

### 📦 Phase 1: Meshtastic Base (PRIORITY - 16 hours)

**Goal:** Working Meshtastic-compatible device with basic TrekLink hardware.

**Tasks:**
1. Fork Meshtastic v2.6.x repository
2. Create custom variant `variants/esp32/treklink/variant.h`
3. Configure PlatformIO environment (`env:treklink`)
4. Wire Ra-02 SPI module according to GPIO pinout
5. Flash firmware and verify Ra-02 radio initialization
6. Test GPS Neo-6M UART communication
7. Verify OLED display shows Meshtastic UI
8. Test multi-button input (MENU, SOS, UP, DOWN)
9. Implement battery ADC monitoring
10. Verify mesh packet TX/RX with another Meshtastic device

**Success Criteria:**
- ✅ Device boots with Meshtastic firmware
- ✅ Ra-02 transmits/receives on 433MHz
- ✅ GPS acquires fix and shares position
- ✅ OLED displays Meshtastic dashboard
- ✅ Buttons navigate menus
- ✅ Interoperable with other Meshtastic devices

---

### 🚀 Phase 2: TrekLink Custom Features (OPTIONAL - 20 hours)

**Goal:** Add TrekLink differentiators (fall detection, hybrid encryption, Silent Mode).

**Tasks:**
1. Implement `TrekLinkButtonModule` (multi-button support)
2. Implement Silent Mode (GPIO 23 OLED power gating)
3. Implement `FallDetectionModule` (MPU6050 monitoring)
4. Add fall detection pre-alarm state (30s countdown)
5. Integrate fall detection with Meshtastic SOS trigger
6. Implement `TrekLinkProtocol` hybrid encryption
7. Add TrekLink Private mode (AES-128-GCM + Channel ID)
8. Test custom encrypted messages between TrekLink devices
9. Verify Meshtastic Public mode still works for SOS
10. Implement custom OTA update service

**Success Criteria:**
- ✅ Fall detection triggers auto-SOS after 30s
- ✅ Silent Mode physically cuts OLED power
- ✅ TrekLink Private messages are AES-GCM encrypted
- ✅ Meshtastic Public mode works for universal SOS
- ✅ Custom OTA updates TrekLink firmware

**If Phase 2 is incomplete:** Device remains a fully functional Meshtastic node.

---

## 8. Testing & Verification

### 8.1 Hardware Integration Tests

**Test 1: Ra-02 SPI Communication**
```cpp
// Expected: SX1278 init success, frequency set to 433.0 MHz
RadioLib SX1278 radio = new Module(18, 26, 14, 19); // CS, DIO0, RST, MISO
int state = radio.begin(433.0, 125.0, 9, 7, 20);
assert(state == RADIOLIB_ERR_NONE);
```

**Test 2: GPS NMEA Parsing**
```cpp
// Expected: Valid NMEA sentences, lat/lon populated
HardwareSerial GPS(1);
GPS.begin(9600, SERIAL_8N1, 17, 16); // RX=17, TX=16
TinyGPSPlus gps;
while (GPS.available()) {
    gps.encode(GPS.read());
}
assert(gps.location.isValid());
```

**Test 3: MPU6050 I2C Detection**
```cpp
// Expected: MPU6050 found at address 0x68
Wire.begin(21, 22); // SDA=21, SCL=22
Wire.beginTransmission(0x68);
int error = Wire.endTransmission();
assert(error == 0); // Device acknowledged
```

---

### 8.2 Meshtastic Interoperability Tests

**Test 1: Mesh Packet RX/TX**
- **Setup:** TrekLink device + standard Meshtastic T-Beam (both on EU_433)
- **Procedure:** Send text message from T-Beam → TrekLink
- **Expected:** TrekLink receives packet, displays message on OLED

**Test 2: Position Broadcast**
- **Setup:** TrekLink device with GPS fix
- **Procedure:** Click SOS button once
- **Expected:** Meshtastic POSITION_APP packet broadcast, visible on other mesh nodes

**Test 3: SOS Trigger**
- **Setup:** TrekLink device + monitoring Meshtastic app
- **Procedure:** Hold SOS button for 3 seconds
- **Expected:** High-priority SOS packet sent, buzzer activates, LED strobes

---

### 8.3 Phase 2 Tests (Optional)

**Test 1: Fall Detection**
- **Setup:** TrekLink device with Phase 2 firmware
- **Procedure:** Drop device from 1.5m onto soft surface
- **Expected:** Pre-alarm activates (30s countdown), auto-SOS if not cancelled

**Test 2: Silent Mode**
- **Setup:** TrekLink device with Phase 2 firmware
- **Procedure:** Hold MENU button for 1 second
- **Expected:** OLED turns off (GPIO 23 LOW), other functions continue

**Test 3: TrekLink Private Encryption**
- **Setup:** 2x TrekLink devices (Phase 2), 1x standard Meshtastic device
- **Procedure:** Send TrekLink Private message
- **Expected:** Only TrekLink devices decrypt message, standard device cannot

---

## 9. Regulatory Compliance

### 9.1 Radio Regulations (EU)

- **Frequency Band:** 433.050 – 434.790 MHz (ERC Recommendation 70-03, Annex 1)
- **Max TX Power:** 10 mW ERP (10 dBm) for 433.05-434.79 MHz **OR** 100 mW (20 dBm) for 433.05-434.79 MHz with duty cycle limitations
- **Duty Cycle:** <10% per hour
- **Modulation:** LoRa is classified as narrowband (meets spectrum mask requirements)

**TrekLink Compliance:**
- Operates within 433.0-434.0 MHz (EU_433 region)
- TX power configurable 0-20 dBm (default 20 dBm for range)
- Meshtastic firmware enforces duty cycle limits via airtime tracking

### 9.2 Safety Standards

- **IP67 Rating:** IEC 60529 (dust-tight, immersion to 1m for 30 min)
- **Battery Safety:** IEC 62133 (Li-ion cell safety)
- **EMC Compliance:** EN 301 489 (ElectroMagnetic Compatibility for radio equipment)

---

## 10. Development Environment

### 10.1 Required Tools

- **IDE:** Visual Studio Code + PlatformIO extension
- **Compiler:** ESP-IDF GCC (included with PlatformIO)
- **Debugger:** ESP-PROG JTAG adapter (optional, for hardware debugging)
- **Simulator:** Wokwi (basic logic testing, limited Meshtastic support)

### 10.2 Build Instructions

```bash
# Clone Meshtastic firmware repository
git clone https://github.com/meshtastic/firmware.git
cd firmware
git checkout v2.6.x  # Use latest stable v2.6.x release

# Create TrekLink variant directory
mkdir -p variants/esp32/treklink
cp variants/heltec_v1/variant.h variants/esp32/treklink/

# Edit variant.h with TrekLink GPIO pinout (see Section 3.2)
nano variants/esp32/treklink/variant.h

# Add TrekLink environment to platformio.ini
# (see implementation_plan.md Section 3 for full config)

# Build firmware
pio run -e treklink

# Flash to ESP32
pio run -e treklink -t upload

# Monitor serial output
pio device monitor -b 115200
```

---

## 11. Operational Scenarios

### Scenario A: Basic Mesh Communication (Phase 1)

1. User powers ON TrekLink device (slide switch to ON)
2. Device boots, shows Meshtastic splash screen
3. GPS acquires fix (outdoor), position shared to mesh
4. Another Meshtastic device comes into range
5. User navigates to Messages menu (MENU button)
6. User types simple text message using Meshtastic app (phone via BLE)
7. Message is encrypted (Meshtastic PKC), transmitted via Ra-02
8. Recipient's TrekLink device receives packet, displays message on OLED

**Phase 1 Success:** Universal Meshtastic compatibility achieved.

---

### Scenario B: SOS Emergency (Phase 1)

1. User encounters injury while hiking
2. User holds SOS button for 3 seconds
3. Device vibrates confirmation (3 short pulses)
4. Buzzer sounds loud alarm, LED strobes (blue, built-in GPIO 2)
5. High-priority Meshtastic POSITION_APP packet broadcast to all nodes
6. Packet contains GPS coordinates, battery level, node name
7. Nearby Meshtastic devices receive SOS, display alert on screen
8. Rescue team uses coordinates to navigate to injured hiker
9. After rescue, user holds SOS for 3s to cancel alert

---

### Scenario C: Fall Detection Auto-SOS (Phase 2 Optional)

1. User trips and falls while descending steep trail
2. MPU6050 detects freefall signature (>0.5s freefall, >3G impact, 10s inactivity)
3. Device enters Pre-Alarm state
4. OLED displays countdown: "Fall Detected! 30s to cancel"
5. Buzzer beeps rapidly, vibrator pulses
6. User is unconscious, cannot cancel
7. After 30 seconds, device automatically triggers SOS (same as Scenario B)
8. Nearby nodes receive auto-SOS, rescuers dispatched

**Phase 2 Success:** Automated safety feature for incapacitated users.

---

### Scenario D: Silent Mode Stealth Operation (Phase 2 Optional)

1. User is in tactical scenario requiring stealth
2. User holds MENU button for 1 second
3. Device vibrates once, OLED fades to black (GPIO 23 → LOW)
4. Buzzer disabled, LED disabled, only vibration motor active
5. User receives incoming mesh message
6. Device vibrates in specific pattern (e.g., 2 short pulses = text message)
7. User cannot see OLED but knows message arrived (muscle memory)
8. To read message, user holds MENU again to exit Silent Mode
9. OLED powers back on, message displayed

---

## 12. Known Limitations & Future Enhancements

### Phase 1 Limitations

❌ **PRFH Removed:** No frequency hopping (deferred to future releases)  
❌ **No Preset Messages:** Meshtastic doesn't have built-in preset system (requires custom UI)  
❌ **Limited Button Functions:** Meshtastic default uses single button (multi-button requires custom module)  
❌ **No Dot Matrix Map:** Meshtastic has standard map view (custom matrix map is Phase 2 feature)  

### Phase 2 Enhancements (If Implemented)

✅ **Fall Detection:** Automated SOS for incapacitated users  
✅ **Silent Mode:** Hardware power gating for OLED (stealth operation)  
✅ **Hybrid Encryption:** TrekLink Private mode with AES-128-GCM layer  
✅ **Custom UI:** Preset messages, dot matrix map, advanced dashboard  
✅ **Multi-Button Support:** All 4 buttons functional (MENU, SOS, UP, DOWN)  

### Future Enhancements (Post-MVP)

🔮 **PRFH Anti-Jamming:** Implement frequency hopping with GPS time sync  
🔮 **Bluetooth App:** Custom Android/iOS app for TrekLink-specific features  
🔮 **Wi-Fi AP Mode:** Configure device via web interface (125.0.0.1)  
🔮 **Dead Reckoning:** Estimate position using IMU when GPS is unavailable  
🔮 **RSSI Triangulation:** Fallback positioning using mesh node distances  
🔮 **Custom Map Tiles:** Offline maps stored in SPIFFS/LittleFS  

---

## 13. Conclusion

TrekLink v2.0 represents a strategic pivot from proprietary firmware to a **Meshtastic ecosystem integration**. This approach balances:

✅ **Rapid Development:** Leverage Meshtastic's mature codebase (Phase 1 in ~16 hours)  
✅ **Universal Compatibility:** Interoperate with 100,000+ existing Meshtastic devices  
✅ **Extensibility:** Add custom TrekLink features as time allows (Phase 2)  
✅ **Risk Mitigation:** Device remains functional even if Phase 2 is incomplete  

**This specification supersedes all previous versions and serves as the authoritative reference for TrekLink development.**

---

**Document Control:**
- **Author:** AI Development Team
- **Reviewed By:** [Pending User Approval]
- **Next Review Date:** Upon Phase 1 completion
- **Change Log:**
  - v2.0 (2026-02-04): Complete architectural redesign for Meshtastic fork
  - v1.0 (2026-01-28): Original proprietary firmware specification