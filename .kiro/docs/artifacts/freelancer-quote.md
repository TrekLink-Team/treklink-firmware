# Request A Quote

## Describe the service you're looking to purchase:

```text
Project: Custom 4-Layer KiCad PCB Design for Off-Grid Handheld Communicator

I am looking for an experienced PCB layout engineer to design a custom 4-layer PCB (Signal-GND-Power-Signal) for a small-batch (7 units) hand-held off-grid communication device running Meshtastic.

Design Requirements:
- Schematic & Layout Tool: KiCad (v8.0+)
- Form Factor: 54.2 mm (W) × 73.2 mm (H) vertical board, designed to fit inside a prebuilt plastic project box (125 × 80 × 32.5 mm). The PCB must mount on 4× M2.5 standoffs with precise hole spacing (41.8 × 59.1 mm) and specific edge offsets to leave a 43.0 mm wide battery compartment on the left for side-by-side 21700 cells.
- Primary ICs & Modules:
  * MCU: ESP32-S3-WROOM-1-N8R8 (castellated)
  * LoRa: CDEBYTE E22-400M22S SX1268 (castellated, SPI, built-in IPEX antenna connector)
  * GPS: u-blox NEO-M9N (LCC-24, UART)
  * IMU: ICM-20948 (QFN-24, I2C, 1.8V VDDIO logic)
  * Power Path Charger: BQ24074 (1.0A charge limit, 500mA USB limit)
  * Regulator: TPS63802 Buck-Boost (3.3V system rail, 2A max)
  * Protection: DW01A + FS8205A battery protector
- Key Features:
  * Soft-latch power control circuit using a dual-MOSFET latch (2N7002 + AO3401A) controlled by ESP32-S3 GPIO 9 (latch output) and sensed via GPIO 13.
  * LoRa RF switch wiring: E22 TXEN (Pin 7) connected to E22 DIO2 (Pin 8) on-board; E22 RXEN (Pin 6) connected to ESP32-S3 GPIO 43 for software receive-enable control.
  * 1.8V sub-rail (regulated via AMS1117-1.8) and bidirectional level shifters (via 2N7002 MOSFETs) for the ICM-20948 IMU (I2C SDA/SCL and INT line routed to ESP32-S3 GPIO 3).
  * Off-board interfaces: 4-pin breakout pads for USB-C (connector is panel-mount, off-board), 4-pin direct solder pads for 0.96" SSD1306 OLED module, and 5× 2-pin breakouts for waterproof external buttons (with parallel SMD test buttons on-board).
  * 50Ω microstrip trace with solid ground plane reference for the GPS (1.57GHz) RF path leading to a U.FL connector. LoRa RF uses the built-in IPEX connector directly on the E22 module (no RF routing on the main PCB).
  * Transient/ESD protection on USB and GPS RF inputs, and a 1.5A PTC fuse on VBUS.
  * Tabbed CR1220 backup battery footprint and 0Ω GPS bias-T placeholder.

Deliverables:
1. Complete KiCad project folder (schematics, layouts, custom footprints).
2. Production files: Gerber, drill files, BOM, and Pick-and-Place CSV formatted specifically for JLCPCB PCBA automation.
3. 3D STEP model of the finished PCB.
4. DRC/ERC reports showing 0 errors.
```

## Attach Files:
The following files from the project workspace are attached to provide full technical details and layout constraints:

* **Specifications & Briefs:**
  1. `Freelancer-requirements.md` (Main layout/routing rules, pinouts, and expected deliverables)
  2. `v2.0-pcb-design-requirements.md` (Detailed PCB design requirements and power architecture notes)
  3. `v2.0-component-list.md` (Comprehensive components and package specifications)
  4. `TrekLink_v2.0_LCSC_BOM.csv` (Complete BOM with LCSC part numbers for SMT assembly)
* **Enclosure Specifications & Diagrams:**
  5. `enclosure/enclosure-specifications.md` (Tolerance limits and structural box details)
  6. `enclosure/v2.0-adjusted-pcb-dimensions.png` (Precise PCB dimensions, mounting hole spacing, and offset layout)
  7. `enclosure/v2.0-usable-enclosure-space.png` (Side-by-side battery placement and PCB boundaries)
  8. `enclosure/v2.0-pcb-footprint.png` (PCB footprint positioning relative to enclosure standoffs)
  9. `enclosure/v2.0-general-enclosure-dimensions.png` (Enclosure outer dimensions)
  10. `enclosure/v2.0-maximum-enclosure-dimensions.png` (Envelope volume of the case)
  11. `enclosure/v2.0-theoretical-pcb-footprint.png` (Baseline design footprint)

## Once you place your order, when would you like your service delivered?
* 5 days

## What is your budget for this service?
* 350$ maximum
