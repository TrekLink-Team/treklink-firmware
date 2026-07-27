# TrekLink MVP Requirements Specification

> **Project Code:** EXE101-G1-TREKLINK  
> **Version:** 1.3 (MVP Release - Consolidated SSOT)  
> **Date:** February 2, 2026  
> **Status:** APPROVED FOR DEVELOPMENT

---

## Introduction

TrekLink is an off-grid, decentralized communication device designed for hikers, survivalists, and rescue teams operating in "Dead Zones" (areas with no cellular coverage). It utilizes LoRa technology at 433MHz to create a self-healing mesh network for reliable text messaging, GPS tracking, and automated SOS alerting without reliance on external infrastructure.

The MVP (Minimum Viable Product) focuses on a standalone hardware unit using pre-soldered modules. It prioritizes reliable 433MHz LoRa communication, proprietary managed flooding mesh protocols with Forward Error Correction (FEC), and essential survival UI. The device incorporates advanced features including Pseudo-Random Frequency Hopping (PRFH) for anti-jamming, AES-128-GCM encryption for security, and a Power Gated Architecture for extended battery life (72+ hours under standard use). Bluetooth and Wi-Fi AP features are architecturally prepared but disabled in this version.

This document serves as the Single Source of Truth (SSOT) for development, engineering, and testing. It adheres to the EARS (Easy Approach to Requirements Syntax) format to ensure clarity, testability, and lack of ambiguity.

---

## Actors

| Actor | Description |
|-------|-------------|
| **Tracker (User)** | A hiker, skier, or outdoor enthusiast carrying the device for safety and communication within a group. |
| **Group Leader (Admin)** | A user with authority to define Channel IDs, Hop Limits, and coordinate group activities. |
| **The Mesh (System)** | The autonomous network of TrekLink nodes relaying packets via managed flooding. |

---

## User Stories

| ID | User Story |
|----|------------|
| **US.01** | As a Tracker, I want to send a "Help" message without looking at the screen so that I can signal distress immediately. |
| **US.02** | As a Tracker, I want to see the direction and distance of my friends so I can regroup in dense foliage. |
| **US.03** | As a Group Leader, I want to switch channels to avoid interference from other groups (Frequency Hopping). |
| **US.04** | As a User, I want the device to automatically detect if I fall and become unconscious, sending an SOS on my behalf. |
| **US.05** | As a User, I want to switch to "Silent Mode" to save battery and maintain stealth while still receiving haptic alerts. |
| **US.06** | As a User, I want my communications to be encrypted so that eavesdroppers cannot read my messages or track my location. |
| **US.07** | As a User, I want the device to last at least 3 days on a single charge so that I don't need to charge it during a multi-day trip. |

---

## Requirements

### 1. Communication & Mesh Protocol (REQ-COM)

#### REQ-COM-01: Managed Flooding Mesh
**User Story:** As a user, I want my messages to reach their destination even if I don't have a direct line of sight, so that I can communicate reliably in complex terrain.

**Acceptance Criteria (Updated for Meshtastic Base):**
1. **[Ubiquitous]** The system **SHALL** utilize **Meshtastic firmware v2.6.x routing algorithm** (improving managed flooding) as the base mesh protocol.
2. **WHEN** a packet is received for another node, **THEN** the system **SHALL** re-broadcast according to Meshtastic's routing logic (hop count decrement, seen buffer deduplication).
3. **IF** a packet has already been seen (based on Message ID in Meshtastic's RecentPackets), **THEN** the system **SHALL** drop the packet to prevent broadcast storms.
4. **WHILE** the device is ON, **THEN** it **SHALL** listen for incoming LoRa packets using RadioLib's Channel Activity Detection (CAD).
5. **[Ubiquitous]** The system **SHALL** operate as a decentralized peer-to-peer mesh; every node **SHALL** act as a router with no coordinator node.
6. **[Phase 2 Optional]** The system **MAY** implement custom TrekLink packet types for private group communication with additional AES-128-GCM encryption layer.

#### REQ-COM-02: Packet Structure & Protocol
**User Story:** As a developer, I want an optimized packet structure, so that airtime is minimized and battery life is maximized.

**Acceptance Criteria (Updated for Meshtastic):**
1. **[Phase 1]** The system **SHALL** use **Meshtastic Protobuf packet structure** for interoperability with other Meshtastic devices.
2. **WHEN** transmitting in **Meshtastic Public mode**, ** THEN** the system **SHALL** use standard Meshtastic MeshPacket format (header, from, to, payload, etc.).
3. **[Phase 2 Optional]** WHEN transmitting in **TrekLink Private mode**, **THEN** the system **MAY** encapsulate custom encrypted payloads within Meshtastic PortNum_PRIVATE_APP packets.
4. **IF** a payload exceeds maximum packet size, **THEN** the system **SHALL** reject the message (MVP does not support fragmentation).
5. **WHEN** Link Quality (RSSI/SNR) is below acceptable threshold, **THEN** Meshtastic **SHALL** adjust rebroadcast behavior automatically.

#### REQ-COM-03: Message Types & Priority
**User Story:** As a user, I want different ways to communicate, so that I can send quick updates or urgent alerts.

**Acceptance Criteria (Updated for Meshtastic + TrekLink Hybrid):**
1. **[Phase 1]** The system **SHALL** support **Meshtastic standard message types**: Text (PRIVATE), Position (POSITION_APP), and NodeInfo (NODEINFO_APP).
2. **WHEN** the user triggers SOS, **THEN** the system **SHALL** broadcast a **Meshtastic standard POSITION_APP packet** to ensure interoperability with all Meshtastic devices.
3. **[Phase 2 Optional]** The system **MAY** support custom TrekLink message types (encrypted private messages, preset broadcasts) using PortNum_PRIVATE_APP.
4. **[DEFERRED]** ~~Matrix Check (double-click SOS)~~ — removed from MVP per user directive.
5. **[Ubiquitous]** SOS messages **SHALL** have highest priority and **SHALL** use Meshtastic's maximum priority flag (`meshtastic_MeshPacket_Priority_MAX`).

#### REQ-COM-04: Rebroadcast Modes
**User Story:** As a Group Leader, I want to control how my device relays messages, so that I can prioritize local group communication or extend network range.

**Acceptance Criteria (Updated for Meshtastic + Phase 2):**
1. **[Phase 1]** The system **SHALL** support Meshtastic's channel-based routing (primary channel for local mesh).
2. **[Phase 2 Optional]** WHEN "TrekLink Private Mode" is enabled, **THEN** the system **SHALL** only relay packets with matching Channel ID encryption key.
3. **WHEN** in "Meshtastic Public Mode," **THEN** the system **SHALL** relay all valid Meshtastic packets to extend network range for universal interoperability.

---

### 2. Navigation & Location (REQ-NAV)

#### REQ-NAV-01: GPS Position Tracking
**User Story:** As a user, I want to know where I am and where my friends are, so that we don't get lost.

**Acceptance Criteria:**
1. **[Ubiquitous]** The system **SHALL** attempt to acquire GPS lock via the Neo-6M module at intervals defined in Power Settings.
2. **IF** a GPS fix is acquired, **THEN** the system **SHALL** store the Latitude and Longitude.
3. **WHEN** sending a heartbeat, ping, or message, **THEN** the system **SHALL** include the latest valid GPS coordinates (if Location Broadcast is enabled).
4. **[Ubiquitous]** The GPS **SHALL** utilize a "Hot Start" strategy by maintaining backup power (V_BCKP connected to battery) for <1s fix times after sleep.
5. **WHEN** the system detects a GPS lock, **THEN** it **SHALL** automatically synchronize the ESP32 internal RTC with GPS time to prevent mesh fragmentation due to clock drift.

#### REQ-NAV-02: Position Fallback & Dead Reckoning
**User Story:** As a user, I want to have an estimated position even when GPS is unavailable, so that I can still be located.

**Acceptance Criteria:**
1. **IF** GPS lock is unavailable (HDOP > 5), **THEN** the system **SHALL** fallback to RSSI Triangulation (if >2 nodes are available with known positions).
2. **IF** RSSI Triangulation is not possible, **THEN** the system **SHALL** use Dead Reckoning: `Last_Known_Pos + (Compass_Heading * Est_Speed * Time_Delta)` using MPU6050 data.
3. **WHEN** the user Single Clicks the SOS button (in normal state), **THEN** the system **SHALL** broadcast a position ping (current location) to all nodes.

#### REQ-NAV-03: Distance & Direction Display
**User Story:** As a user, I want to see how far away my group members are, so that I can gauge travel time.

**Acceptance Criteria:**
1. **WHEN** viewing the "Map" screen, **THEN** the system **SHALL** display relative positions of other nodes as dots on a dot-matrix display (self at center 0,0).
2. **IF** a target node is selected (using Up/Down buttons), **THEN** the system **SHALL** calculate and display: Device ID, Distance (in meters/km using Haversine formula), and Direction (compass bearing).
3. **WHEN** a node is selected on the map, **THEN** the system **SHALL** display RSSI/SNR if GPS data is unavailable.

---

### 3. Safety & Emergency (REQ-SAF)

#### REQ-SAF-01: SOS Activation
**User Story:** As a user, I want a quick way to signal distress, so that I can get help immediately.

**Acceptance Criteria:**
1. **WHEN** the SOS button is Held for 3 seconds, **THEN** the system **SHALL** trigger the SOS Routine: Broadcast High Priority SOS Packet at maximum TX power + Loud Buzzer + Strobing LED.
2. **WHEN** the SOS button is Clicked once, **THEN** the system **SHALL** broadcast the current location (Ping) without triggering the audio alarm.
3. **WHILE** in SOS mode, **THEN** the system **SHALL** transmit continuously for 1 minute (Phase 1: Immediate), then switch to pulsed interval transmission (every 30 seconds, Phase 2: Beacon) indefinitely until cancelled or battery depletion.
4. **WHEN** the user Holds the SOS button for 3 seconds during active SOS, **THEN** the system **SHALL** cancel the SOS mode, stop all alarms, and confirm with a long vibration.
5. **IF** the system triggers an SOS, **THEN** it **SHALL** bypass Adaptive Power Control and transmit at maximum power to prioritize reach over stealth.

#### REQ-SAF-02: Fall Detection
**User Story:** As a user, I want the device to call for help if I fall and become unresponsive, so that I can be rescued even if incapacitated.

**Acceptance Criteria:**
1. **WHILE** the device is ON, **THEN** the system **SHALL** continuously monitor MPU6050 data for a specific fall signature: Freefall (>0.5s) followed by High Impact (>3G) followed by Inactivity (10s).
2. **WHEN** a Fall signature is detected, **THEN** the system **SHALL** enter a "Pre-Alarm" state with haptic/visual warning for 30 seconds.
3. **WHILE** in "Pre-Alarm" state, **THEN** the system **SHALL** sound a local alarm and vibrate continuously.
4. **IF** the user presses any button (SOS, MENU, UP, or DOWN) during the Pre-Alarm State, **THEN** the system **SHALL** cancel the auto-SOS and return to normal operation.
5. **IF** the user does NOT cancel the alarm within 30 seconds, **THEN** the system **SHALL** automatically trigger the full SOS Routine.

---

### 4. Security & Electronic Counter-Measures (REQ-SEC)

#### REQ-SEC-01: Encryption
**User Story:** As a user, I want my communications to be private, so that eavesdroppers cannot read my messages or track my location.

**Acceptance Criteria (Updated for Meshtastic Hybrid):**
1. **[Phase 1]** The system **SHALL** use **Meshtastic firmware v2.6.x PKC (Public Key Cryptography)** for channel encryption as the baseline security layer.
2. **[Phase 2 Optional]** The system **MAY** implement an additional **AES-128-GCM** encryption layer on top of Meshtastic PKC for TrekLink Private mode messages.
3. **WHEN** in **Meshtastic Public mode** (SOS, universal broadcasts), **THEN** the system **SHALL** use only Meshtastic's standard PKC encryption.
4. **WHEN** in **TrekLink Private mode**, **THEN** the system **SHALL** use Channel ID as seed for AES-128-GCM encryption before encapsulation in Meshtastic packets.
5. **IF** a received packet fails decryption, **THEN** the system **SHALL** discard it silently.

#### REQ-SEC-02: Anti-Jamming (Deferred from MVP)
**User Story:** As a tactical user, I want communication to persist even if someone tries to jam the frequency.

**Acceptance Criteria:**
1. **[DEFERRED]** Pseudo-Random Frequency Hopping (PRFH) is **removed from MVP scope** due to time synchronization complexity across nodes with different boot times.
2. **[Phase 1]** The system **SHALL** operate on a static 433MHz frequency as configured in Meshtastic region settings (EU_433).
3. **[Future Enhancement]** PRFH may be implemented in future releases if GPS time synchronization and LCG key distribution can be reliably achieved across the mesh.

#### REQ-SEC-03: Anti-Tampering & Replay Prevention
**User Story:** As a user, I want my messages to be protected from being replayed or tampered with.

**Acceptance Criteria:**
1. **[Ubiquitous]** The system **SHALL** track Msg ID (Random Nonce) in the Seen_Buffer.
2. **IF** a packet with a duplicate Msg ID is received (replay attack), **THEN** the system **SHALL** drop the packet immediately.
3. **IF** a packet is injected by a rogue device without the correct Channel Key, **THEN** it **SHALL** fail AES decryption (CRC mismatch) and be discarded silently.

---


### 5. Power Management (REQ-PWR)

#### REQ-PWR-01: Battery Life Target
**User Story:** As a user, I want the battery to last for several days, so that I don't need to charge it during a trip.

**Acceptance Criteria:**
1. **[Ubiquitous]** The system **SHALL** operate for a minimum of 72 hours (3 days) on a full charge under standard mesh traffic (target: 5+ days, theoretical: 30+ days with power gating).
2. **[Ubiquitous]** The system **SHALL** utilize 2x 21700 Li-ion cells in parallel (10,000 mAh effective capacity).

#### REQ-PWR-02: Smart Sleep & Duty Cycle
**User Story:** As a developer, I want to maximize battery life through intelligent power management.

**Acceptance Criteria:**
1. **WHILE** idle (State A: Deep Mesh Sleep), **THEN** the ESP32 **SHALL** enter Deep Sleep mode (~10µA), and the E32 **SHALL** operate in Mode 2 (Power Saving, ~20µA, 2s sleep/50ms wake cycle for preamble sniffing).
2. **WHEN** the LoRa module (in Wake-Up mode) detects a preamble, **THEN** it **SHALL** pull AUX pin LOW to trigger an ESP32 RTC_GPIO interrupt, waking the main MCU (State B: Active Receive).
3. **IF** no activity occurs for a defined screen timeout, **THEN** the system **SHALL** return to sleep.
4. **WHEN** motion is detected (timer-based GPS checks for MVP), **THEN** the system **SHALL** enter State C: Active Tracking (Wake → Turn ON GPS MOSFET → Acquire Fix → Transmit → Turn OFF GPS → Sleep).

#### REQ-PWR-03: Power Gating
**User Story:** As a developer, I want to cut power to unused sensors, so that leakage current is eliminated.

**Acceptance Criteria:**
1. **[REMOVED]** ~~Hardware Power Gating (P-MOSFET on GPIO 13/23)~~ — removed from TrekLink. Meshtastic firmware handles power saving via sleep modes and GPS power management.
2. **WHEN** the GPS is not required (during sleep), **THEN** Meshtastic firmware **SHALL** manage GPS power via software sleep modes (V_BCKP remains connected for hot start).
3. **WHEN** the OLED is off, **THEN** Meshtastic firmware **SHALL** disable the OLED display via I2C command.
4. **WHEN** the device is switched OFF via the physical slide switch, **THEN** the system **SHALL** enter a hard-off state (<10µA consumption) but allow charging circuitry to function.

#### REQ-PWR-04: Low Power Indicators (LPI)
**User Story:** As a user, I want the device to minimize its RF footprint when possible.

**Acceptance Criteria:**
1. **[Ubiquitous]** The system **SHALL** utilize Spread Factor 12 (SF12) to maximize coding gain, allowing signal recovery at -20dB SNR.
2. **[Ubiquitous]** The system **SHALL** implement Adaptive Power Control (APC): Nodes **SHALL** exchange RSSI data and automatically command the E32 to drop power from 100mW (20dBm) to 10mW (10dBm) if link margin exceeds 15dB.
3. **WHEN** a node receives a packet with High RSSI (>-60dBm), **THEN** it **SHALL** piggyback a "Power Down" command in the ACK.
4. **[Ubiquitous]** The system **SHALL** calculate and display Airtime/Channel Utilization % to help users understand the local RF environment.

---

### 6. User Interface (REQ-UI)

#### REQ-UI-01: Physical Controls
**User Story:** As a user, I want simple physical controls, so that I can operate the device with gloves.

**Acceptance Criteria:**

| Component | Action | Function |
|-----------|--------|----------|
| Slide Switch | Slide ON | Active Mode: Screen, Sound, Vibrate enabled. |
| Slide Switch | Slide OFF | System Shutdown. |
| Side Button (MENU) | Click | Menu / Back: Opens main menu or goes back. |
| Side Button (MENU) | Hold (1s) | Toggle Silent Mode. |
| SOS Button | Click (1x) | Ping: Broadcast current location. |
| SOS Button | Double Click | ~~Matrix Request~~ **DEFERRED** — removed from MVP. |
| SOS Button | Hold (3s) | SOS: Trigger Emergency Broadcast. |
| SOS Button | Hold (3s) | Cancel active SOS. |
| Up / Down | Click | Navigation in Menu / Scroll Messages. |
| Up + Down | Combo | Quick Reply: Selects first preset message. |

#### REQ-UI-02: Dashboard Display
**User Story:** As a user, I want to see the system status at a glance, so that I know my connectivity and battery level.

**Acceptance Criteria:**
1. **WHEN** the display is ON, **THEN** the Main Dashboard **SHALL** show:
   - **Top Bar:** Signal Strength (RSSI icon), Number of Active Nodes, Battery Level (% and icon).
   - **Middle Area:** Scrolling log of last 3 messages (Incoming/Outgoing), with SOS messages blinking if urgent.
   - **Bottom Bar:** Channel Utilization (Airtime %), GPS Fix Status (2D/3D/None), Current Time (Clock).
2. **IF** valid GPS data is available, **THEN** the system **SHALL** display a "3D" or "2D" indicator.
3. **IF** GPS is unavailable, **THEN** the system **SHALL** display "---" or a "No GPS" indicator.

#### REQ-UI-03: Menu Navigation
**User Story:** As a user, I want to access device functions through a clear menu structure.

**Acceptance Criteria:**
1. **WHEN** the "Menu" button is clicked, **THEN** the system **SHALL** display the main menu with the following options:
   - **Ping Location:** Force transmit coordinates.
   - **Send Message:** Select from Preset List (e.g., "Safe", "Wait", "Lost", "Help", "Moving", "Stop").
   - **Logs:** View Received (with RSSI/SNR/Hop Count details), View Sent.
   - **Map & Coord:** Dot Matrix Map, Raw GPS Data.
   - **Device Settings:** Set Channel ID, Set Device ID.
   - **System Settings:** LoRa Config (Freq, Baud, Power, Hop Limit), Rebroadcast Mode (All/Local), Power Settings (GPS Interval, Screen Timeout, LED On/Off), Wireless (BT/WiFi - Placeholder), Location Broadcast Settings (Precision: High/Med/Low/Off, Interval).
2. **WHEN** "Up" or "Down" buttons are pressed, **THEN** the system **SHALL** scroll through menu options or lists.
3. **WHEN** navigating within the menu, **THEN** the SOS button single-click **SHALL** continue to broadcast a Ping (safety priority).


---

### 5. Pre-Defined Emergency Messages (REQ-MSG)

#### REQ-MSG-01: Canned Message System
**User Story:** As a SAR operator, I want quick access to pre-defined emergency messages, so that I can communicate status without typing on a small device in stressful situations.

**Acceptance Criteria:**
1. **WHEN** the user single-clicks the MENU button, **THEN** the system **SHALL** display a scrollable list of pre-configured canned messages on the OLED.
2. **WHEN** the user presses UP or DOWN buttons while in the canned message menu, **THEN** the system **SHALL** highlight the previous or next message respectively.
3. **WHEN** the user single-clicks the MENU button while a message is highlighted, **THEN** the system **SHALL** send the highlighted message as a broadcast to the mesh network.
4. **IF** the user navigates away from the canned message menu (e.g., timeout or back action), **THEN** the system **SHALL** close the menu without sending a message.
5. **WHEN** a canned message is sent, **THEN** the system **SHALL** display confirmation on OLED for 2 seconds before returning to home screen.
6. **[Ubiquitous]** The system **SHALL** support minimum 6 pre-defined messages within the 200-byte configuration limit.

#### REQ-MSG-02: Default Message List
**User Story:** As a SAR coordinator, I want emergency-priority messages listed first, so that critical alerts can be sent fastest in life-threatening situations.

**Acceptance Criteria:**
1. **[Ubiquitous]** The system **SHALL** provide the following default canned messages in priority order:
   - Index 1: "LOST - HELP"
   - Index 2: "MEDICAL ISSUE" 
   - Index 3: "I'M SAFE"
   - Index 4: "WAIT FOR ME"
   - Index 5: "COME TO ME"
   - Index 6: "LOW BATTERY"
2. **WHEN** the canned message menu is opened, **THEN** the first message ("LOST - HELP") **SHALL** be highlighted by default for fastest emergency access.
3. **[Ubiquitous]** Emergency messages (Index 1-2) **SHALL** be positioned for access within 2 button presses maximum.

#### REQ-MSG-03: Configuration & Persistence
**User Story:** As a mission planner, I want to customize canned messages before each operation via the Meshtastic app, so that messages are relevant to the specific mission context.

**Acceptance Criteria:**
1. **WHEN** the user configures messages via Meshtastic app (Settings → Module Configuration → Canned Messages), **THEN** the system **SHALL** persist the new message list to ESP32 NVS flash.
2. **WHEN** the device reboots, **THEN** the system **SHALL** load previously configured canned messages from flash.
3. **IF** no custom messages are configured, **THEN** the system **SHALL** use the default message list specified in REQ-MSG-02.
4. **WHEN** factory reset is performed, **THEN** the system **SHALL** restore default canned messages.
5. **[Ubiquitous]** The system **SHALL** support message configuration strings up to 200 bytes total length (pipe-delimited format: "MSG1|MSG2|MSG3").

#### REQ-MSG-04: Fall Detection Auto-Send
**User Story:** As an unconscious hiker, I want the device to automatically send SOS with my coordinates if fall detection triggers and I don't respond, so that rescuers know my location.

**Acceptance Criteria:**
1. **WHEN** MPU6050 detects fall pattern (freefall → impact → stillness) AND 30-second pre-alarm countdown expires without user cancellation, **THEN** the system **SHALL** automatically trigger Meshtastic emergency beacon.
2. **WHEN** fall detection auto-triggers, **THEN** the system **SHALL** send an SOS text message containing GPS coordinates in format: "SOS - [Latitude], [Longitude]".
3. **WHEN** emergency beacon is active, **THEN** the system **SHALL** increase position broadcast frequency to every 30 seconds (overriding normal interval settings).
4. **WHEN** emergency beacon is active, **THEN** the system **SHALL** set TX power to maximum (20 dBm for VN_433 region).
5. **IF** the user presses any button during the 30-second pre-alarm countdown, **THEN** the system **SHALL** cancel auto-trigger and return to normal operation.
6. **WHEN** auto-triggered SOS is sent, **THEN** the user **MAY** manually send "I'M SAFE" canned message to signal recovery and cancel beacon.

---

### 6. TrekLink Device Identity (REQ-ID)

#### REQ-ID-01: Vietnamese Regional Defaults
**User Story:** As a Vietnamese user, I want the device to default to my local region settings (VN_433 frequency, GMT+7 timezone), so that I don't need additional configuration on first boot.

**Acceptance Criteria:**
1. **WHEN** the device boots for the first time (fresh flash), **THEN** the system **SHALL** default to **VN_433 region** (433.0-435.0 MHz, 20 dBm max power, identical to MY_433 settings).
2. **WHEN** the device boots for the first time, **THEN** the system **SHALL** default to **ICT-7 timezone** (GMT+7, Vietnam/Indochina Time).
3. **WHEN** the user accesses LoRa region menu on OLED, **THEN** the system **SHALL** display "VN 433" as the region option.
4. **WHEN** the user views region configuration in Meshtastic app, **THEN** the app **MAY** display "Malaysia 433 MHz" (due to shared protobuf enum for maximum compatibility).
5. **[Ubiquitous]** The user **SHALL** be able to change region and timezone settings via Meshtastic app or OLED menu if traveling outside Vietnam.

#### REQ-ID-02: TrekLink Branding
**User Story:** As a TrekLink user, I want the device to clearly identify as "TrekLink" in Bluetooth discovery and on the splash screen, so that it's distinguishable from generic Meshtastic devices.

**Acceptance Criteria:**
1. **WHEN** the device boots, **THEN** the OLED splash screen **SHALL** display "TrekLink" text at the bottom center (replacing "Meshtastic" branding).
2. **WHEN** the device advertises via Bluetooth, **THEN** the BLE name **SHALL** be formatted as `"TrekLink_abcd"` where `abcd` is the last 4 hexadecimal digits of the ESP32 MAC address.
3. **WHEN** the device broadcasts NodeInfo to the mesh, **THEN** the default node name **SHALL** be "TrekLink ####" where #### is a 4-digit identifier.
4. **[Ubiquitous]** All TrekLink branding **SHALL** maintain compatibility with Meshtastic app and mesh protocols (no protocol-breaking changes).

---

### 7. Hardware Specifications (REQ-HW)

#### REQ-HW-01: Core Components (BOM)
**User Story:** As a hardware engineer, I want a defined BOM, so that the device works as designed.

**Acceptance Criteria:**
1. **The system SHALL** utilize the ESP32-WROOM-32 as the main MCU (Dual Core: Core 0 for LoRa/Mesh, Core 1 for UI/Sensors).
2. **The system SHALL** utilize the **Ra-02 (SX1278) module** for LoRa communication (433MHz, **SPI Interface**, 100mW) integrated via Meshtastic firmware v2.6.x.
3. **The system SHALL** use 2x 21700 Li-ion cells in parallel for power (10,000 mAh).
4. **The system SHALL** include a Neo-6M GPS module (NMEA Protocol) with internal backup battery for Hot Start.
5. **The system SHALL** include an MPU6050 6-Axis Accelerometer/Gyroscope for Fall Detection (I2C interface, shared bus with OLED).
6. **The system SHALL** be based on **Meshtastic firmware v2.6.x** fork with custom TrekLink board variant.
7. **The system SHALL** use IRF9530N P-Channel MOSFETs for high-side power switching of peripherals.
8. **The system SHALL** include S8050-D NPN transistors to drive MOSFET gates from ESP32 3.3V logic.

#### REQ-HW-02: Interface Components
**User Story:** As a user, I want clear feedback from the device.

**Acceptance Criteria:**
1. **The system SHALL** include a 0.96" OLED Display (I2C, 128x64 resolution).
2. **The system SHALL** include a Passive Buzzer (3.3V, 9056-TS) for audible alerts.
3. **The system SHALL** include a Vibration Motor (Coin Type) for haptic feedback.
4. **The system SHALL** include a Status LED (Blue, 5mm, 1.8-2V forward voltage) for visual indicators.
5. **The system SHALL** include 4x Tactile Buttons (MENU, SOS, UP, DOWN) and 1x Slide Switch (2-Position: ON/OFF).

#### REQ-HW-03: Physical Enclosure
**User Story:** As a user, I want a rugged device, so that it survives outdoor conditions.

**Acceptance Criteria:**
1. **The enclosure SHALL** be rated for IP67 (dust tight, immersion up to 1m).
2. **The enclosure SHALL** have external dimensions of 125 x 80 x 32 mm with usable internal space of 120 x 74 x 28 mm.
3. **The enclosure SHALL** be a vertical rectangle plastic enclosure.
4. **The design SHALL** place the GPS antenna on top and the LoRa antenna (SMA Connector, 17.5cm length) on the right side.
5. **All buttons SHALL** be 8-12mm height tactile switches almost touching the top of the enclosure with holes drilled for access.
6. **The right side SHALL** include a USB-C Charging Port covered by a silicone flap.
7. **The left side SHALL** include a USB Serial Interface (Data/Logs, separate from ESP32) for repairability covered by a silicone flap.

#### REQ-HW-04: Pinout & Wiring
**User Story:** As a developer, I want a defined pinout to implement the firmware correctly.

**Acceptance Criteria (Updated for Ra-02 SPI):**
1. **LoRa Ra-02 (SPI):** SCK → GPIO 5, MISO → GPIO 19, MOSI → GPIO 27, CS → GPIO 18, DIO0 → GPIO 26, RESET → GPIO 14.
2. **GPS (Neo-6M):** RX → GPIO 16 (ESP32 TX1), TX → GPIO 17 (ESP32 RX1), uses hardware UART1.
3. **I2C Bus:** SDA → GPIO 21, SCL → GPIO 22 (shared by OLED SSD1306 0x3C, MPU6050 0x68).
4. **MPU6050:** Uses I2C polling via SDA/SCL (GPIO 21/22). INT pin is not connected (fall detection uses software polling).
5. **Battery ADC:** → GPIO 36 (Input Only, ADC1_CH0 for voltage sensing).
6. ~~**GPS P-MOSFET Gate Control:** → GPIO 13~~ — **REMOVED**. Meshtastic manages GPS power via firmware.
7. ~~**OLED GND Switch Control:** → GPIO 23~~ — **REMOVED**. Meshtastic manages OLED via I2C.
8. **Buzzer:** → GPIO 33 (Passive buzzer, PWM output).
9. **Vibrator:** → GPIO 4 (Motor via NPN transistor).
10. **Buttons:** BTN_MENU → GPIO 25, BTN_SOS → GPIO 34, BTN_UP → GPIO 32, BTN_DOWN → GPIO 35.
11. **Status LED:** → GPIO 2 (Built-in blue LED).

---

### 8. Data & Storage (REQ-DAT)

#### REQ-DAT-01: Message Logging
**User Story:** As a user, I want to review past messages and events.

**Acceptance Criteria:**
1. **[Ubiquitous]** The system **SHALL** reserve 75% of EEPROM/NVS memory for Message Logs implemented as a Circular Buffer.
2. **[Ubiquitous]** The system **SHALL** reserve 25% of EEPROM/NVS memory for Settings, Device ID, and Preset Messages.
3. **WHEN** viewing the Logs menu, **THEN** the system **SHALL** display message metadata including RSSI, SNR, Hop Count, and Timestamp.
4. **[Ubiquitous]** Logs **SHALL** be stored in a RAM Ring Buffer and only written to Flash on Buffer Full or Critical Event (SOS) to minimize flash wear.

#### REQ-DAT-02: Preset Messages
**User Story:** As a user, I want quick access to common messages without typing.

**Acceptance Criteria:**
1. **[Ubiquitous]** The system **SHALL** support a minimum of 6 preset messages stored in non-volatile memory.
2. **The default preset messages SHALL be** as defined in REQ-MSG-02: "LOST - HELP", "MEDICAL ISSUE", "I'M SAFE", "WAIT FOR ME", "COME TO ME", "LOW BATTERY".
3. **WHEN** the user selects a preset, **THEN** the system **SHALL** broadcast it with the current GPS coordinates (if enabled).

---

### 9. Development Environment (REQ-ENV)

#### REQ-ENV-01: Framework & Language
**User Story:** As a developer, I want to use standard tools, so that the code is maintainable and compatible with libraries.

**Acceptance Criteria (Updated for Meshtastic Fork):**
1. **The system SHALL** be developed as a **Meshtastic firmware v2.6.x fork** using C++ (C++17 standard).
2. **The system SHALL** utilize the **Meshtastic firmware architecture** with custom TrekLink board variant.
3. **The system SHALL** leverage **RadioLib** for Ra-02 (SX1278) SPI communication.
4. **[Phase 2 Optional]** The system **MAY** integrate custom TrekLink modules for fall detection and hybrid encryption.

#### REQ-ENV-02: Build System
**User Story:** As a developer, I want a reproducible build environment, so that anyone can compile the project.

**Acceptance Criteria (Updated for Meshtastic):**
1. **The system SHALL** use **PlatformIO** as the build system with `esp32_base` platform.
2. **The project configuration SHALL** include a custom `env:treklink` environment in `platformio.ini`.
3. **The custom board variant SHALL** be defined in `variants/esp32/treklink/variant.h` with all GPIO mappings.
4. **The system SHALL** compile with Meshtastic dependencies including `jgromes/RadioLib@^6.0.0`.

#### REQ-ENV-03: Simulation
**User Story:** As a developer, I want to verify logic without hardware, so that I can iterate quickly.

**Acceptance Criteria:**
1. **The system SHALL** be verifiable using the Wokwi simulator for basic UI flows, button debouncing, and I2C protocols.
2. **NOTE:** Meshtastic firmware may require hardware-specific adaptations for full Wokwi compatibility. Core logic can be tested in isolation.

---

## Operational Scenarios

### Scenario A: The Silent SOS
1. User slides power switch to ON.
2. User holds MENU button for 1 second; device enters SILENT Mode (Screen Off, Sound Off).
3. User slips and breaks a leg. User holds SOS Button for 3 seconds.
4. Device Vibrates 3 times (confirming activation).
5. Device transmits High Priority SOS Packet (Broadcast to ALL channels, bypassing PRFH restrictions, at maximum power).
6. Nearby nodes receive SOS; their screens flash "SOS DETECTED".
7. Rescuers navigate to the coordinates provided in the packet using the Dot Matrix Map.

### Scenario B: Group Coordination
1. Leader sets Device to Channel 5.
2. Leader selects "Send Message" → "Come to Me".
3. Device broadcasts packet with Target=0xFF but encrypted/hopped with Key=Channel5.
4. Only devices on Channel 5 decrypt and display "Leader: Come to Me".
5. Devices on Channel 3 receive the RF signal but fail decryption/frequency check and ignore it.

### Scenario C: Adaptive Jamming Evasion
1. A localized jammer begins a broadband sweep of the 433MHz spectrum.
2. TrekLink Node A detects high noise floor and failed ACKs (3 consecutive failures).
3. Node A enters "Search Mode," calculating the next frequencies in the PRFH sequence based on its internal RTC.
4. Node A switches its E32 CHAN register and successfully transmits to Node B, which has moved to the same frequency.
5. The Jammer remains focused on the previous frequency, rendered ineffective.

### Scenario D: Stealth Insertion / Adaptive Power
1. A Search & Rescue team enters a sensitive area.
2. TrekLink nodes detect they are within 100m of each other (High RSSI > -60dBm).
3. The Adaptive Power Control drops TX power to 10mW (10dBm).
4. The RF signature becomes indistinguishable from background electronic noise at a distance of 1km, preventing detection by unauthorized parties while maintaining full group mesh connectivity.

### Scenario E: Automatic Fall Detection
1. User is hiking alone with device in Active Mode.
2. User falls from a ledge (Freefall > 0.5s detected by MPU6050).
3. Impact occurs (> 3G acceleration detected).
4. User becomes unconscious (Inactivity for 10 seconds detected).
5. Device enters Pre-Alarm State: buzzer sounds locally, vibrator pulses for 30 seconds.
6. User does not respond (no Double-Click on SOS button).
7. After 30 seconds, device automatically triggers full SOS Routine, broadcasting coordinates to all nodes.

---

## Conclusion

These requirements encompass the full scope of the TrekLink MVP as defined in the technical specifications. Adherence to these criteria ensures the delivery of a robust, secure, energy-efficient, and resilient off-grid communication device suitable for emergency and decentralized communications in challenging environments.

This document is the Single Source of Truth (SSOT) for all development, engineering, and testing efforts.
