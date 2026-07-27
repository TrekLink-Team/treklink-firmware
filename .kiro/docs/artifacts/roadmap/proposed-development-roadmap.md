# TrekLink Development Roadmap — Version Summary

## v1.0: Perfboard Prototype (✅ DONE)
- ESP32-WROOM-32 + Ra-02 SX1278 + Neo-6M + MPU6050 + SSD1306
- Perfboard build, jumper wiring, motherboard/daughterboard stacked design
- Firmware: Meshtastic fork with custom variant (`treklink_v1_0`)
- Status: Complete. Proof of concept validated.

## v2.0: Custom PCB Prototype (🔧 IN DEVELOPMENT)
- ESP32-S3-WROOM-1 + E22-400M22S (SX1268) + NEO-M9N + ICM-20948 + SSD1306
- Single custom 4-layer PCB (52.5mm × 70.0mm), prebuilt ABS enclosure
- 5 buttons (SELECT, SOS, UP, DOWN, POWER), soft-latch power management
- Firmware: Custom variant (`treklink_v2_0`), full SOS + fall detection
- Batch: 7 units, JLCPCB PCBA assembly
- Status: PCB design with freelancer, firmware variant to be created

## v3.0: T-Beam V1.2 COTS (📋 PLANNED)
- LILYGO T-Beam V1.2 (ESP32 + SX1262 433MHz + NEO-M8N)
- Custom 3D-printed enclosure (open source), 21700 battery mod
- Firmware: Fork upstream `tbeam` variant, add TrekLink branding + SOS (long-press GPIO 38)
- No fall detection (no IMU onboard)
- Status: Planned, hardware sourcing pending

## v4.0: T-Beam Supreme COTS (📋 PLANNED)
- LILYGO T-Beam Supreme (ESP32-S3 + SX1262 + MAX-M10S + QMI8658)
- Custom 3D-printed enclosure (open source), 21700 battery mod
- Firmware: Fork upstream `tbeam-s3-core` variant, add TrekLink branding + SOS (long-press GPIO 0) + fall detection (QMI8658, sensor-agnostic)
- Status: Planned, hardware sourcing pending

## v5.0: Mass Production (🔮 FUTURE)
- Proprietary custom PCB, castellated proprietary modules
- Injection-molded waterproof enclosure, commercial-grade finish
- Direct B2B factory sourcing, smallest footprint, maximum cost optimization
- Status: Future vision