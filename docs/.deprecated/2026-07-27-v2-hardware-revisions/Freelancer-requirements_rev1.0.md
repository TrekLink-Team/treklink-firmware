Your purchase was confirmed! Next, fill in some info get your order started
The freelancer will need this info before they can start working on your order:

1. If you’re ordering for a business, what’s your industry?(Optional)
Select the project that best fits your needs

Save industry to my profile.
2. Is this order part of a bigger project you're working on?(Optional)
Yes
Don’t see what you’re looking for? Start typing to find your project

3. What type of Project you want that I design for you?
Off-grid LoRa mesh communication device (TrekLink v2.0) running Meshtastic. Integrates ESP32-S3-WROOM-1-N8R8, SX1262 LoRa, GNSS GPS, IMU, and OLED. Fits a 100x70mm 4-layer PCB inside a 30mm thick enclosure.
Deliverables: Complete KiCad project folder (v8.0+ preferred) containing schematic (.kicad_sch), board (.kicad_pcb), library symbols, footprints, and 3D models. Output must be 100% compatible with direct JLCPCB fabrication and SMT assembly (BOM and CPL centroid files must be exportable via standard KiCad JLCPCB plugins).
Physical Layout: Vertical side-by-side split. Upper half hosts RF modules (E22 LoRa, NEO-M9N GNSS), OLED display header, and ICM-20948 IMU. Lower half hosts USB-C port, SS-22F32 slide power switch, battery protection (DW01A+FS8205A), charging circuitry (BQ24074), and 3.3V buck-boost (TPS63802). Battery is a 1S2P 21700 (10Ah) battery pack mounted in a bracket underneath the PCB, connecting via JST-PH 2-pin or direct wire pads on the bottom layer.
Aesthetic constraints: Curvy, rounded, organic trace routing ("80s vintage RAM card style") for all digital buses (I2C, UART, button inputs) and power/GND rails. No standard 45-degree or 90-degree trace angles allowed on low-speed lines. Add vintage teardrops to all pad-to-trace and via-to-trace connections to give it an old-school analog/digital hybrid look.
Silkscreen: Meshtastic logo (vector graphics) and "TrekLink" project name printed on the top silkscreen layer. Include a blank serial number frame on the silkscreen: "v2.0 - [______]".
Electrical Specs & Pin Mappings:
- LoRa SPI: SCK=21, MOSI=38, MISO=39, CS=14, RESET=40, BUSY=41, DIO1=42.
- I2C (OLED & IMU): SDA=5, SCL=6.
- GNSS UART: RX=16 (ESP RX), TX=17 (ESP TX), EN=15 (power enable).
- Buttons: MENU (BOOT)=0, SOS=4, UP=7, DOWN=8.
- Notifications: Buzzer=11, Vibrator=12 (NPN driver).
- Battery ADC: GPIO1 (ADC1_CH0) via 100k/100k divider.
- Status LED: GPIO10.

4. What type of circuit or pcb design you want ?
4-layer PCB (Layer 1: Signal/RF, Layer 2: Solid GND, Layer 3: PWR/Aux, Layer 4: Signal). Must use 100% in-stock LCSC parts for JLCPCB PCBA.
Key Components:
- U1: ESP32-S3-WROOM-1-N8R8 (C2913201)
- U2: E22-400M22S SX1262 LoRa (C411291)
- U3: u-blox NEO-M9N GNSS (C5119087)
- U4: ICM-20948 IMU (C726001)
- U5: TPS63802DLAT Buck-Boost 3.3V (C1849531)
- U6: BQ24074RGTR Charger (C54313)
- U7: DW01A Protection (C351410) + Q1: FS8205A Dual N-MOSFET (C32254)
- J1: USB4105-GF-A USB-C (C2688138)
- L1: cjiang FTC252012SR47MBCA 2520 0.47uH (C5832368)
Layout Rules & Checklist (Designers must verify):
1. RF Routing: 50-ohm microstrip for 433MHz LoRa path (E22 antenna pad to U.FL J2) and GPS RF path (NEO-M9N RF_IN to U.FL J3). Route straight on Layer 1. Do not use curved traces for RF. Provide solid, unbroken Layer 2 GND plane directly under RF lines. Surround RF traces with ground shield stitching vias.
2. GPS Antenna: NEO-M9N does NOT have an internal antenna. Route RF_IN to dedicated U.FL (J3) near board edge. Provide CR1220 tabbed backup battery footprint near GPS module, protected by BAT54S dual Schottky diode to prevent back-charging.
3. Power Path: Connect TPS63802 input directly to BQ24074 OUT pin (not BAT pin) for DPPM. TPS63802 feedback resistors (510k/91k) and bulk caps must be placed extremely close to IC pins. Place L1 (0.47uH 2520) close to SW pin with a wide, short track to minimize EMI. BQ24074 thermal pad must connect to GND plane via at least 4 thermal vias.
4. USB-C CC Lines: CC1 and CC2 must each have an independent 5.1k-ohm pull-down resistor to GND. Do NOT tie them together. ESD protection (USBLC6-2SC6) and SMAJ5.0A TVS diode must be placed immediately at the USB-C connector entrance before VBUS/D+/D- connect to other parts. USB D+/D- must route as a 90-ohm differential pair to ESP32-S3.
5. Track Widths: 3.3V, VBAT, and VBUS rails must be at least 24mil wide. Low-speed digital traces 8-10mil.
6. Decoupling: Place 10uF and 100nF decoupling capacitors directly at the VCC pins of U1, U2, U3, and U4. GND pads of decoupling caps must connect directly to Layer 2 GND plane via vias.
7. Test Pads: 1.5mm test pads for TP_3V3, VBAT, VUSB, GND, SDA, SCL, GPS_TX, LORA_CS, CHG.


The information I provided is accurate and complete. Any changes will require the seller's approval, and may be subject to additional costs.