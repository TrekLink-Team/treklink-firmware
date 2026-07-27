# TrekLink v2.0 — PCB Freelancer Brief & Requirements

> **Revision:** 2.1  
> **Date:** 2026-05-28  
> **Project:** Custom 2-Layer PCB design for TrekLink v2.0 off-grid communicator.  
> **Deliverable Format:** Complete KiCad Project (v8.0+ preferred). Must be directly compatible with JLCPCB PCBA service.
> **Reference Documents:**  
> - `v2.0-component-list.md` — Complete BOM with LCSC part numbers  
> - `v2.0-pcb-design-requirements.md` — Detailed PCB layout and enclosure constraints  
> - `implementation-plan-2.0.md` — Architecture & pin mapping

---

## 1. Functional Requirements

### 1.1 Core Functions
The PCB mainboard acts as the central hub for the TrekLink v2.0 handheld off-grid communicator. It must perform the following core functions:
- **Data Processing:** Run Meshtastic firmware on the ESP32-S3 microcontroller to handle mesh routing, device settings, and user interaction.
- **LoRa Communication:** Transmit and receive packets in the 433 MHz band via the SPI-controlled E22-400M22S module.
- **GNSS Position Tracking:** Capture location coordinates (GPS, GLONASS, BeiDou, Galileo) concurrently via the NEO-M9N module.
- **Motion & Compass Sensing:** Capture 9-axis movement and magnetic heading (accelerometer, gyroscope, magnetometer) via the ICM-20948 IMU.
- **Power Management:** Provide seamless system power transitions between USB input and battery via the BQ24074 charger, regulate a stable 3.3V system rail via the TPS63802 buck-boost converter, and protect the battery cells via the DW01A + FS8205A protector circuit.
- **Soft-Latch Power Control:** Provide a latching power switch using a momentary button and the TPS63802 Enable (EN) pin.
- **Alerts & Haptics:** Drive audio alerts via a passive buzzer (PWM) and haptic feedback via an ERM vibration motor.

### 1.2 Input/Output Interfaces
To minimize physical stress on the board inside the enclosure, all primary user-facing interfaces and ports are mounted off-board and wired back to the PCB:

| Interface | Type | Qty | Location / Pinout / Details |
|-----------|------|-----|----------------------------|
| **USB-C Interface** | 4-Pin Plated Breakout Pads | 1 | Solder pads labeled **VBUS, D+, D−, GND**. Sockets are wired to an external panel-mount USB-C port at the bottom of the enclosure. |
| **Display Interface** | 4-Pin Plated Through-Holes | 1 | Solder pads at top-center labeled **VCC, GND, SDA, SCL**. The SSD1306 OLED module is laid flat and soldered directly to these pads (no headers). |
| **User Input Breakouts** | 2-Pin Plated Breakout Pads | 5 | Breakout terminals labeled **SELECT, SOS, UP, DOWN, POWER** for wiring external IP-rated tactical buttons. |
| **Bench-Test Buttons** | SMD Tactile Switches | 5 | Small SMD tactile buttons (e.g. 3×4mm, `C128477`) placed on-board in parallel with the breakouts for bench testing. |
| **LoRa RF Out** | IPEX Connector on Module | 1 | LoRa module features a built-in IPEX receptacle for direct connection to SMA pigtail. |
| **GPS RF Out** | IPEX/U.FL Receptacle SMT | 1 | RF output on PCB routed via a 50Ω microstrip from GPS module to U.FL receptacle. |
| **Buzzer Output** | 2-Pin Plated Through-Holes | 1 | Plated through-holes for direct-soldering a passive buzzer. |
| **Vibration Motor** | 2-Pin Solder Pads | 1 | Solder pads for direct-wiring a 3V coin vibration motor. |
| **Battery Terminal** | 2-Pin Plated Solder Pads | 1 | Plated solder pads labeled **BAT+, BAT−** for connecting the 1S2P 21700 battery pack. |
| **GPS Backup Battery** | Solder Pads | 1 | Solder pads for tabbed CR1220 backup coin cell. |

---

## 2. Performance Requirements

### 2.1 MCU Clock & Peripherals
- **Microcontroller:** ESP32-S3-WROOM-1-N8R8 module.
- **Clock Frequency:** Dual-core Xtensa LX7 running at 240 MHz (internal PLL).
- **USB Interface:** Native USB 2.0 Full-Speed (12 Mbps) interface routed directly from ESP32-S3 GPIO 19 (D−) and GPIO 20 (D+) to the USB-C breakout pads. No external USB-to-UART bridge IC is used.

### 2.2 Signal Integrity & Impedance Control
- **RF Path (GPS):**
  - The RF trace from NEO-M9N RF_IN pad to the GPS U.FL receptacle must be routed as a **50Ω coplanar waveguide or microstrip** transmission line.
  - Layer 2 must be a solid, unbroken ground plane directly beneath this RF trace.
  - Guard shield stitching vias must be placed along the RF path to prevent noise coupling.
  - Curvy/rounded trace styling is **strictly prohibited** on RF traces.
  - *Note:* LoRa RF routing is entirely contained within the E22-400M22S module; no RF traces for LoRa are needed on the main PCB.
- **USB Data Bus:**
  - Route USB D+/D− traces as a **90Ω differential pair**.
  - Match the length of D+ and D− traces to within 0.15 mm. Keep traces as short as possible.
- **High-Speed SPI Bus (LoRa):**
  - Route SPI lines (SCK, MOSI, MISO, CS) to match length and minimize crosstalk. Max trace length ≤ 30 mm.
- **I2C Bus (OLED & IMU):**
  - Connect 4.7kΩ pull-up resistors to 3.3V on SDA (GPIO 5) and SCL (GPIO 6). Max bus length ~100 mm.
- **Low-Speed Routing (Vintage Curvy Traces):**
  - The designer may optionally use **curvy/rounded trace routing** ("vintage organic styling") for low-speed lines (I2C, UART, button lines, status LED) and power traces.
  - Include teardrops on all low-speed pad-to-trace and via-to-trace connections to match the vintage look.

---

## 3. Dimensions and Physical Specifications

### 3.1 PCB Constraints
- **Layer Stack-up:** 4-Layer (Layer 1: Signal/RF, Layer 2: Solid GND, Layer 3: Solid Power, Layer 4: Signal).
- **Board Dimensions:** Max width **52.5 mm**, max height **70.0 mm**. (Uniform 5.0 mm offset from mounting hole centers with 5.0 mm concentric corner radiuses).
- **Thickness:** 1.6 mm.
- **Min Trace Width / Space:** 0.127 mm / 0.127 mm (5/5 mil).
- **Min Via Drill / Diameter:** 0.3 mm / 0.6 mm.

### 3.2 Mounting Configuration
- **Mounting Method:** Secured via 4× M2.5 screws (3.0 mm drill holes) to the integrated standoffs of the prebuilt enclosure.
- **Horizontal Spacing:** 42.5 mm hole-to-hole.
- **Vertical Spacing:** 60.0 mm hole-to-hole.
- **Edge Offsets:**
  - Left edge to middle mounting holes: **5.0 mm** (uniform boundary).
  - Right edge to right mounting holes: **5.0 mm** (uniform boundary).
  - Top/Bottom edges to mounting holes: **5.0 mm** (uniform boundary).

### 3.3 Physical Layout Topology
The communicator utilizes a vertical "side-by-side" layout inside the 124.6 × 79.6 × 33.7 mm enclosure:
- **PCB Zone (Right Side):** The 52.5 × 70.0 mm PCB is positioned on the right half.
- **Battery Zone (Left Side):** A 44.8 mm wide compartment is reserved on the left half to store the 1S2P 21700 battery cells (approx. 21.5 mm diameter each).
- **Component Placement:**
  - Locate the SSD1306 OLED solder interface at the top-center face of the board.
  - Place the ESP32-S3, E22 LoRa, and NEO-M9N modules on the top/mid sections.
  - Keep the power management ICs, breakouts, and battery terminal pads grouped at the bottom section of the PCB.

---

## 4. Power Requirements

### 4.1 Input Voltages
- **USB Input:** 5V DC (4.75V–5.25V) via external USB-C breakout pads.
- **Battery Input:** 1S2P Li-Ion (3.0V–4.2V nominal, 10Ah).

### 4.2 Power Distribution & Regulation
- **Charger & Power Path (BQ24074):**
  - Battery charge current configured to **1.0A** (via 887Ω ISET resistor).
  - USB input current limit configured to **500mA** (via 1.6kΩ ISET2 resistor).
  - Dynamic Power Path Management (DPPM) must be active to power the system rail even with a depleted or missing battery.
- **System Regulator (TPS63802):**
  - The TPS63802 buck-boost regulator must output a stable **3.3V** rail to power all on-board modules.
  - Feedback resistor values: R1 (high) = 510kΩ, R2 (low) = 91kΩ (1% accuracy).
- **Over-Current Protection:** A **1.5A resettable PTC fuse** (1210 package) must be placed in series between the VBUS breakout pad and the BQ24074 IN pin.

### 4.3 Power Consumption Limits
The PCB layout and component decoupling must support the following power limits:
- **Active Transmit State (LoRa Burst):** Peak current draw ≤ 500mA @ 3.3V.
- **Active Receive / Idle State:** Average current draw ≤ 80mA @ 3.3V.
- **Standby / Sleep State:** Current draw ≤ 2.0mA (GPS in standby, LoRa in sleep, MCU in light sleep).
- **Shutdown State (TPS63802 Disabled):** Total quiescent current draw from battery **≤ 15µA** (TPS63802 EN pulled LOW, BQ24074 in standby, battery protection standby).

---

## 5. Compatibility Requirements

### 5.1 Hardware Compatibility
- **I2C Addresses & Logic Levels:**
  - SSD1306 OLED: 0x3C (7-bit address). Operates at 3.3V logic.
  - ICM-20948 IMU: 0x68 (7-bit address). Operates at 1.8V VDDIO logic.
  - A 1.8V sub-rail (regulated via AMS1117-1.8 from 3.3V) and bidirectional logic level shifters (via 2N7002 MOSFETs) must be implemented on the I2C SDA, SCL, and INT lines to translate between the 1.8V sensor side and the 3.3V MCU side (IMU INT connects to ESP32-S3 GPIO 3).
- **Antenna & RF Switch System:**
  - LoRa RF connects directly to the built-in IPEX receptacle on the E22 module, routing to an external 433 MHz SMA whip antenna via an IPEX-to-SMA pigtail.
  - The E22-400M22S RF switch control lines must be wired as follows: E22 TXEN (Pin 7) connects directly to E22 DIO2 (Pin 8) on the PCB for automatic TX path switching. E22 RXEN (Pin 6) connects to ESP32-S3 GPIO 43 for software-controlled RX path switching.
  - GPS RF U.FL routes to a passive 25 × 25 mm ceramic patch antenna mounted internally inside the battery compartment.
  - A 0Ω resistor footprint must be placed between VCC_RF and the RF_IN microstrip path as a placeholder for a future active antenna bias-T circuit.

### 5.2 Software & Firmware Compatibility
- The board runs the open-source **Meshtastic** firmware.
- The GPIO pinout must map exactly to the custom `variant.h` configuration:
  - **SPI (LoRa):** SCK=21, MOSI=38, MISO=39, CS=14, RESET=40, BUSY=41, DIO1=42, RXEN=43 (TXEN connected to DIO2).
  - **I2C & IMU INT:** SDA=5, SCL=6, IMU_INT=3 (active-LOW).
  - **UART (GPS):** RX=16, TX=17, EN=15.
  - **USB:** D−=19, D+=20.
  - **Notifications:** Buzzer=11 (PWM), Vibrator=12.
  - **Status LED:** GPIO 2.
  - **Battery ADC:** GPIO 1 (via 100kΩ/100kΩ voltage divider).
  - **Buttons:** UP=7, SELECT=0 (active-LOW, 10kΩ pull-up), DOWN=8, SOS=4, POWER Latch=9 (latch output, active HIGH), POWER Sense=13 (sense input, active LOW).

---

## 6. Reliability and Stability Requirements

### 6.1 Service Life & Duty Cycle
- **Expected Service Life:** Minimum **5 years** of reliable operation under typical outdoor handheld use.
- **Duty Cycle:** Designed for continuous 24/7 operation in standby/receive modes, with a 5% transmit duty cycle.

### 6.2 Environmental Specifications
- **Operating Temperature:** -20°C to +60°C (limited by the safe discharge temperature limits of the Li-Ion chemistry).
- **Storage Temperature:** -40°C to +85°C (without battery installed).
- **Humidity:** 5% to 95% RH, non-condensing.

### 6.3 Transient and ESD Protection
To protect internal circuits from ESD discharges and power line surges, the following protective devices must be included in the schematic and placed close to their respective entry points:
- **USB VBUS Surge:** Place a unidirectional TVS diode (SMAJ5.0A or equivalent) at the VBUS breakout pad.
- **USB Data ESD:** Place a low-capacitance ESD protection array (USBLC6-2SC6) on the D+/D− lines close to the breakout pads.
- **GPS RF ESD:** Place an ultra-low capacitance ESD protection diode (PESD0402-140 or equivalent, capacitance ≤ 0.25 pF) on the RF_IN path close to the GPS U.FL receptacle to prevent signal attenuation.
- **Battery Protection:** The DW01A protection IC and FS8205A dual-MOSFET pair must safeguard the cell terminals from over-charge, over-discharge, and short-circuit faults.