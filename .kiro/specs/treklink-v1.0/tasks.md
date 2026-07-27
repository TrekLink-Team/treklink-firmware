# TrekLink Implementation Tasks (Meshtastic Fork)

## Implementation Strategy

This document outlines the two-phase implementation strategy for migrating TrekLink from proprietary firmware to a Meshtastic v2.6.x fork with Ra-02 433MHz SPI radio.

**Phase Priority:**
- **Phase 1 (CRITICAL):** Working Meshtastic-compatible device
- **Phase 2 (OPTIONAL):** Custom TrekLink features

If Phase 2 is incomplete, the device remains a fully functional generic Meshtastic node.

---

## Phase 1: Meshtastic Base Implementation

### 1. Meshtastic Repository Setup

- [x] 1.1 Clone and configure Meshtastic firmware repository
  - Clone official Meshtastic repository v2.6.x branch
  - Explore repository structure (src/, variants/, platformio.ini)
  - _Requirements: REQ-ENV-01, REQ-ENV-02_

- [x] 1.2 Create TrekLink custom variant directory
  - Create variants/esp32/treklink/ directory
  - Copy template from variants/heltec_v1/variant.h as starting point
  - Study variant.h structure for pin definitions and configuration
  - _Requirements: REQ-HW-04_

---

### 2. GPIO Configuration & Variant Definition

- [x] 2.1 Define Ra-02 SPI pin mappings in variant.h
  - Define LoRa SPI pins (SCK, MISO, MOSI, CS, DIO0, RESET)
  - Configure SPI bus settings (VSPI, 4 MHz clock)
  - _Requirements: REQ-HW-04.1_

- [x] 2.2 Define I2C peripheral pins for OLED and MPU6050
  - Add I2C pin definitions (SDA pin 21, SCL pin 22)
  - Configure I2C speed (100 kHz standard mode)
  - Add I2C device addresses: OLED (0x3C), MPU6050 (0x68)
  - _Requirements: REQ-HW-04.2, REQ-HW-04.3_

- [x] 2.3 Define GPS UART pins
  - Add GPS Neo-6M UART1 pin mappings (RX/TX pins 16/17)
  - Configure UART baud rate (9600 for NMEA)
  - _Requirements: REQ-HW-04.2_

- [x] 2.4 Define button and notification pins
  - Add button GPIO definitions (MENU button pin 25 for Phase 1)
  - Add notification pins (buzzer, vibrator, LED)
  - Note Phase 2 will add additional buttons (SOS, UP, DOWN)
  - _Requirements: REQ-HW-04.10_

- [x] 2.5 Define power management pins
  - Add power gating control pins (GPS power enable, OLED GND, battery ADC)
  - Configure ADC attenuation for battery voltage sensing
  - _Requirements: REQ-HW-04.6, REQ-HW-04.7, REQ-HW-04.5_

- [x] 2.6 Set Meshtastic region configuration
  - Define frequency region (EU_433 for 433MHz band)
  - Verify EU 433MHz band compliance (433.05-434.79 MHz)
  - _Requirements: REQ-COM-01.1, Regulatory Compliance_

---

### 3. PlatformIO Environment Configuration

- [x] 3.1 Add TrekLink environment to platformio.ini
  - Create custom environment extending esp32_base
  - Configure build flags for TrekLink variant
  - Set LoRa region and variant-specific includes
  - _Requirements: REQ-ENV-02_

- [x] 3.2 Verify dependency resolution
  - Check RadioLib library inclusion
  - Verify Adafruit libraries for OLED and IMU
  - Run dependency check command
  - _Requirements: REQ-ENV-01.3_

---

### 4. Compilation & Firmware Build

- [x] 4.1 Build firmware for TrekLink variant
  - Run PlatformIO build command for treklink environment
  - Resolve any compilation errors (pin conflicts, missing defines)
  - Verify successful build output (firmware.bin generated)
  - _Requirements: REQ-ENV-02.1_

- [x] 4.2 Flash firmware to ESP32
  - Connect ESP32 via USB
  - Flash compiled firmware using PlatformIO
  - Monitor serial output for boot messages
  - Verify Meshtastic version, region, and radio initialization logs
  - _Requirements: REQ-ENV-02.1_

---

### 5. Hardware Integration Testing

- [x] 5.1 Verify Ra-02 SPI initialization
  - Connect Ra-02 module to ESP32 according to GPIO pinout
  - Monitor serial output for SX1278 init success message
  - Test packet transmission via Meshtastic CLI
  - _Requirements: REQ-HW-01.2, REQ-COM-01.1_

- [x] 5.2 Test GPS Neo-6M UART communication
  - Connect GPS module to UART1 pins
  - Monitor NMEA sentences in serial log
  - Verify GPS fix acquisition outdoors (under 60 seconds)
  - _Requirements: REQ-HW-01.4, REQ-NAV-01_

- [x] 5.3 Verify OLED display initialization
  - Connect SSD1306 OLED to I2C bus
  - Confirm Meshtastic splash screen displays
  - Test menu navigation using MENU button
  - _Requirements: REQ-HW-02.1, REQ-UI-01_

- [x] 5.4 Test MPU6050 IMU I2C detection
  - Connect MPU6050 to I2C bus (shared with OLED)
  - Verify I2C address 0x68 detected in serial log
  - Read accelerometer/gyro test values
  - _Requirements: REQ-HW-01.5_

- [x] 5.5 Verify button input detection
  - Connect MENU button to designated GPIO
  - Test button press triggers Meshtastic UI response
  - Monitor debouncing behavior
  - _Requirements: REQ-UI-02_

- [x] 5.6 Test battery ADC voltage sensing
  - Connect battery voltage divider to ADC pin
  - Verify Meshtastic PowerStatus displays battery percentage
  - Confirm voltage reading accuracy (6.4V-8.4V range for 2S)
  - _Requirements: REQ-PWR-02_

---

### 6. Meshtastic Interoperability Testing

- [x] 6.1 Two-device mesh packet exchange
  - Test message exchange between TrekLink and standard Meshtastic device
  - Verify packet reception within 5 seconds
  - Confirm RSSI/SNR values displayed
  - _Requirements: REQ-COM-01, REQ-COM-02, REQ-COM-03_

- [x] 6.2 Position broadcast functionality
  - Test GPS position broadcast from TrekLink
  - Verify position appears on Meshtastic app map
  - Confirm POSITION_APP packet format
  - _Requirements: REQ-NAV-01, REQ-SAF-01_

- [x] 6.3 Multi-hop routing test
  - Set up 3 devices in line configuration for relay testing
  - Verify device B rebroadcasts packets from A to C
  - Confirm hop count decrements correctly
  - _Requirements: REQ-COM-01.2_

---

## Phase 2: TrekLink Custom Features

### 7. Multi-Button Support Module

- [x] 7.1 Create TrekLinkButtonModule class
  - Create module extending SinglePortModule and OSThread
  - Add member variables for 4 button states (MENU, SOS, UP, DOWN)
  - Implement interrupt-based handling with attachInterrupt()
  - _Requirements: REQ-UI-01_

- [x] 7.2 Implement click/double-click/hold event detection
  - Define button state machine (IDLE, PRESS_DETECTED, HOLD_DETECTED, WAIT_DOUBLE_CLICK)
  - Track press/release timestamps with millis()
  - Add ISR handlers for all buttons with IRAM_ATTR
  - Use interrupt-driven architecture for instant response
  - _Requirements: REQ-UI-01_

- [x] 7.3 Map button actions to Meshtastic functions
  - Map MENU button: click for navigation, hold for Silent Mode
  - Map SOS button: click for position, hold for emergency, double-click for Matrix request
  - Map UP/DOWN buttons: menu scroll and message navigation
  - _Requirements: REQ-UI-01, REQ-COM-03_

- [x] 7.4 Register module with Meshtastic module system
  - Add TrekLinkButtonModule to registered modules list
  - Define TREKLINK_VARIANT in variant.h
  - Verify module setup() and runOnce() are called during boot
  - _Requirements: REQ-UI-01_

---


### 9. Fall Detection Module

- [x] 9.1 Create FallDetectionModule class
  - Create new module extending ProtobufModule
  - Implement state machine (MONITORING → FREEFALL → IMPACT → INACTIVITY → PRE_ALARM → SOS_TRIGGERED)
  - Add MPU6050 polling logic in runOnce()
  - _Requirements: REQ-SAF-02_

- [x] 9.2 Implement freefall and impact detection algorithms
  - Read accelerometer data from MPU6050
  - Detect freefall condition (total acceleration < 0.5G for >0.5s)
  - Detect impact (total acceleration > 3G spike)
  - _Requirements: REQ-SAF-02.1, REQ-SAF-02.2_

- [x] 9.3 Implement inactivity detection
  - Monitor gyroscope for movement cessation
  - Detect stillness (gyro < threshold for 10 seconds)
  - Transition to PRE_ALARM state
  - _Requirements: REQ-SAF-02.3_

- [x] 9.4 Implement pre-alarm countdown and auto-SOS
  - Trigger 30-second countdown with buzzer/vibration alerts
  - Allow user cancellation via any button press
  - Auto-trigger Meshtastic SOS if no response
  - _Requirements: REQ-SAF-02.4, REQ-SAF-02.5_

- [ ] 9.5 Test fall detection with simulated drop
  - Simulate fall by dropping device from height
  - Verify pre-alarm triggers correctly
  - Test cancellation via button press
  - Confirm auto-SOS broadcast after timeout
  - _Requirements: REQ-SAF-02_

- [x] 9.6 Implement SOS Morse code buzzer pattern
  - Replace continuous tone with Morse code SOS pattern (... --- ...)
  - Pattern: 3 short beeps, 3 long beeps, 3 short beeps, repeat
  - Timing: Short=200ms, Long=600ms, Gap=200ms, Repeat interval=2s
  - _Requirements: REQ-SAF-02.6_

- [x] 9.7 Integrate fall detection auto-send with emergency messaging
  - When fall detection auto-triggers after 30s timeout, send SOS text message
  - Format message as: "SOS - FALL DETECTED" with GPS coordinates
  - Broadcast as high-priority emergency message to mesh network
  - Test simulated fall → 30s countdown → SOS message sent with coordinates
  - _Requirements: REQ-MSG-04.1, REQ-MSG-04.2, REQ-MSG-04.3_

<!-- ---

### 10. Hybrid Encryption (TrekLink Private Mode)

- [ ] 10.1 Create TrekLink custom protocol layer
  - Create TrekLinkProtocol module files
  - Add AES-128-GCM encryption functions using ESP32 mbedTLS
  - Define custom packet payload structure
  - _Requirements: REQ-SEC-01.2, REQ-SEC-01.4_

- [ ] 10.2 Implement Channel ID-based key derivation
  - Create deriveKey() function for channel-specific encryption
  - Generate unique nonce per packet (timestamp + random)
  - _Requirements: REQ-SEC-01.4_

- [ ] 10.3 Encapsulate encrypted payloads in Meshtastic packets
  - Use PortNum_PRIVATE_APP for TrekLink private messages
  - Maintain compatibility with Meshtastic packet format
  - _Requirements: REQ-SEC-01.1_

- [ ] 10.4 Implement decryption and authentication
  - Verify GCM auth tag on received private messages
  - Drop packets with invalid Channel ID or corrupted auth tag
  - _Requirements: REQ-SEC-01.3_

- [ ] 10.5 Test TrekLink Private Mode isolation
  - Test encrypted message exchange between two TrekLink devices
  - Verify standard Meshtastic device cannot decrypt private messages
  - Confirm SOS still uses Meshtastic public mode for universal interoperability
  - _Requirements: REQ-SEC-01_

--- -->


### 11. Global Button Navigation Implementation

- [x] 11.1 Create TrekLinkButtonInput for 3-button navigation only
  - Create src/input/TrekLinkButtonInput.h/cpp extending UpDownInterruptBase
  - Hardcode GPIO: UP=32, DOWN=35, MENU=25 (NOT SOS)
  - Map to InputBroker events (ALT_PRESS for UP, USER_PRESS for DOWN, SELECT for MENU)
  - Use #ifdef TREKLINK_VARIANT guard for conditional compilation
  - _Requirements: REQ-UI-01_

- [x] 11.2 Integrate TrekLinkButtonInput with InputBroker
  - Add initialization in InputBroker::Init() after trackball setup
  - Register as observable input source
  - Test UP/DOWN/MENU work globally in Meshtastic UI
  - _Requirements: REQ-UI-01_

- [x] 11.3 Keep SOS button (GPIO 34) in TrekLinkButtonModule unchanged
  - SOS button STAYS in TrekLinkButtonModule for emergency-only functions
  - GPIO 34, ISR (sosButtonISR), custom polling (NOT InputBroker)
  - Remove UP/DOWN/MENU from TrekLinkButtonModule (now in TrekLinkButtonInput)
  - Add cancelSOS() function (hold 3s during SOS cancels)
  - Update fall alarm cancel (ONLY SOS hold 3s, not any button)
  - _Requirements: REQ-SAF-01_

- [x] 11.4 Remove obsolete code from TrekLinkButtonModule
  - Remove menuButton, upButton, downButton structures and ISRs
  - Remove toggleSilentMode() function and silent mode variables
  - Remove SOS double-click handler (matrix request)
  - Update runOnce() to poll SOS button only
  - _Requirements: Code cleanup_

- [ ] 11.5 Test both button systems independently
  - Navigation: UP/DOWN in menus, MENU selects items (all contexts)
  - SOS: single-click ping, hold 3s triggers, hold 3s cancels
  - Verify no cross-interference between systems
  - Verify MENU hold does nothing (silent mode removed)
  - Verify SOS double-click does nothing (removed)
  - _Requirements: REQ-UI-01, REQ-SAF-01_

---

### 13. Canned Message Presets Configuration

- [x] 13.1 Enable CannedMessageModule in TrekLink variant
  - Add USE_CANNED_MESSAGE_MODULE definition to variant.h
  - Verify module appears in compiled module list
  - _Requirements: REQ-MSG-01.1, REQ-MSG-01.6_

- [x] 13.2 Configure default emergency message list
  - Define default canned messages string in variant.h: "LOST - HELP|MEDICAL ISSUE|I'M SAFE|WAIT FOR ME|COME TO ME|LOW BATTERY"
  - Initialize moduleConfig.canned_message.messages with defaults on first boot
  - Test factory reset loads correct default messages
  - _Requirements: REQ-MSG-02.1, REQ-MSG-03.3_

- [x] 13.3 Test canned message transmission
  - Verify message menu opens on MENU button click
  - Test UP/DOWN navigation highlights correct message (uses global buttons from Task 11)
  - Test MENU click sends highlighted message to mesh
  - Verify message appears on receiving device within 5s
  - _Requirements: REQ-MSG-01.5, REQ-COM-02.2_

- [ ] 13.4 Test message persistence via Meshtastic NVS
  - Test message configuration via Meshtastic app (Settings → Canned Messages)
  - Verify custom messages persist across device reboots
  - Test factory reset restores default messages
  - _Requirements: REQ-MSG-03.1, REQ-MSG-03.2_


---


### 14. VN_433 Region Configuration

- [x] 14.1 Add VN_433 region option to OLED menu system
  - Edit MenuApplet.cpp to add "VN 433" menu item after MY_433
  - Edit MenuHandler.cpp to add VN_433 option mapped to MY_433 protobuf enum
  - Test OLED region menu displays "VN 433" correctly
  - _Requirements: REQ-ID-01.3_

- [x] 14.2 Implement VN_433 menu action handler
  - Add SET_REGION_VN_433 enum to MenuAction.h
  - Create case handler in MenuApplet.cpp that applies MY_433 region code
  - Test selecting VN_433 from OLED menu changes region setting
  - _Requirements: REQ-ID-01.1, REQ-ID-01.5_

- [x] 14.3 Set VN_433 as default region for TrekLink variant
  - Define REGULATORY_LORA_REGIONCODE as MY_433 in TrekLink variant.h
  - Test fresh firmware flash boots with VN_433 pre-selected
  - _Requirements: REQ-ID-01.1_

- [ ] 14.4 Validate VN_433 frequency and power compliance
  - Verify transmission frequency range: 433.0-435.0 MHz
  - Verify maximum TX power: 20 dBm (100 mW EIRP)
  - Test region changeable via app for international travel
  - _Requirements: REQ-ID-01.1, REQ-ID-01.5_

---

### 15. Vietnam Timezone (GMT+7) Configuration

- [x] 15.1 Add Vietnam (GMT+7) to timezone menu options
  - Edit MenuHandler.cpp to add "Vietnam (GMT+7)" option with POSIX string "ICT-7"
  - Add SET_TZ_VIETNAM enum to MenuAction.h
  - Test timezone appears in OLED menu list
  - _Requirements: REQ-ID-01.2_

- [x] 15.2 Implement Vietnam timezone action handler
  - Create SET_TZ_VIETNAM case handler that applies "ICT-7" timezone
  - Add "Vietnam (GMT+7)" menu item to MenuApplet.cpp
  - Test selecting timezone from OLED menu
  - _Requirements: REQ-ID-01.2_

- [x] 15.3 Add timezone label mapping for display
  - Extend getTimezoneLabelFromValue() function to recognize "ICT-7"
  - Return "Vietnam (GMT+7)" label for display
  - Test timezone label displays correctly in device info
  - _Requirements: REQ-ID-01.2_

- [x] 15.4 Set GMT+7 as default timezone for TrekLink variant
  - Define DEFAULT_TIMEZONE as "ICT-7" in variant.h
  - Modify main.cpp timezone initialization to apply variant default on first boot
  - Test fresh firmware shows correct GMT+7 time on OLED clock
  - _Requirements: REQ-ID-01.2_

- [ ] 15.5 Validate GPS time synchronization with GMT+7 offset
  - Test GPS provides UTC time and ESP32 RTC applies +7 hour offset
  - Verify OLED clock displays Vietnam local time correctly
  - Verify message timestamps show GMT+7 time
  - _Requirements: REQ-NAV-01.5_

---

### 16. TrekLink Branding Implementation

- [x] 16.1 Replace Meshtastic splash screen text with TrekLink
  - Identify splash screen rendering code (Screen.cpp or TFTDisplay.cpp)
  - Replace "Meshtastic" string with "TrekLink" in bottom center text
  - Test device reboot displays "TrekLink" on splash screen
  - _Requirements: REQ-ID-02.1_

- [x] 16.2 Update Bluetooth advertising name to TrekLink
  - Modify BluetoothUtil.cpp BLE name assignment from "Meshtastic_" to "TrekLink_"
  - Maintain 4-digit hex suffix from device ID
  - Test Bluetooth scan shows "TrekLink_abcd" name
  - _Requirements: REQ-ID-02.2_

- [x] 16.3 Update default node name to TrekLink
  - Modify NodeDB.cpp default node name from "Meshtastic ####" to "TrekLink ####"
  - Maintain 4-digit identifier suffix
  - Test node info in Meshtastic app shows "TrekLink ####"
  - _Requirements: REQ-ID-02.3_

- [ ] 16.4 Verify Meshtastic protocol compatibility maintained
  - Test TrekLink can send messages to generic Meshtastic devices
  - Test TrekLink can receive messages from generic Meshtastic devices
  - Confirm branding changes are UI-only, no protobuf modifications
  - _Requirements: REQ-ID-02.4, REQ-COM-01.2_


