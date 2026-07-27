# TrekLink

> **Meshtastic-Powered Off-Grid Search & Rescue (SAR) Firmware**  
> Unified PlatformIO pipeline for multi-generational TrekLink devices (v1.0, v2.0, v3.0, v4.0)

TrekLink is a custom branded firmware fork of [Meshtastic](https://meshtastic.org/) optimized for Search & Rescue (SAR) operations and backcountry expeditions. It adds custom branding, a unified SOS emergency trigger/cancel system, IMU-based fall detection, regional defaults, and optimized canned messages.

[![License: GPL-3.0](https://img.shields.io/badge/License-GPL%203.0-blue.svg)](LICENSE)
[![Platform: ESP32/ESP32-S3](https://img.shields.io/badge/Platform-ESP32%20%2F%20ESP32--S3-blue.svg)](https://www.espressif.com/)
[![Meshtastic: 2.7.19](https://img.shields.io/badge/Meshtastic-2.7.19-green.svg)](https://meshtastic.org/)
[![Radio: 433MHz](https://img.shields.io/badge/Radio-433MHz%20LoRa-orange.svg)]()

---

## 📱 Hardware Variants

TrekLink supports four device generations deployed across custom PCBs and commercial off-the-shelf (COTS) hardware:

| Variant | Hardware Platform | MCU | IMU Sensor | SOS Trigger Method | Buzzer |
|---------|-------------------|-----|------------|--------------------|--------|
| **v1.0** | Custom PCB | ESP32 | MPU6050 | Dedicated Button (GPIO 34) | Yes |
| **v2.0** | Custom PCB | ESP32-S3 | ICM-20948 | Dedicated Button (GPIO 4) | Yes |
| **v3.0** | LilyGo T-Beam v1.2 | ESP32 | *None* | 3s User Button Hold (GPIO 38) | No |
| **v4.0** | LilyGo T-Beam Supreme | ESP32-S3 | QMI8658 | 3s User Button Hold (GPIO 0) | No |

---

## 🚀 Quick Start & Build Targets

Firmware builds are managed via PlatformIO. To avoid LittleFS compilation issues on standard toolchains, use the `-t buildprog` target to build the firmware binary alone.

```bash
# Clone the repository
git clone https://github.com/ruskicoder/treklink.git
cd treklink

# Build specific TrekLink variants
pio run -e treklink -t buildprog            # v1.0 Custom ESP32 PCB
pio run -e treklink-v2 -t buildprog         # v2.0 Custom ESP32-S3 PCB
pio run -e treklink-v3-tbeam -t buildprog   # v3.0 LilyGo T-Beam v1.2
pio run -e treklink-v4-supreme -t buildprog # v4.0 LilyGo T-Beam Supreme

# Flash to device (e.g. v4.0)
pio run -e treklink-v4-supreme -t upload

# Verify stock Meshtastic regression targets compile cleanly
pio run -e tbeam -t buildprog
pio run -e tbeam-s3-core -t buildprog
```

---

## ✨ Features

- **Branding Integration:** Auto-applies "TrekLink" node naming prefix, BLE name, and splash screen logo to distinguish SAR devices.
- **Centralized SOS System (`TrekLinkSOSHelper`):** Central state machine handles triggering/canceling emergency status, broadcasting GPS/text packets, and driving buzzers/vibrators.
- **IMU-Agnostic Fall Detection:** Unified `FallDetectionModule` utilizing a common `FallSensorInterface`. Supports `MPU6050`, `ICM20948`, and `QMI8658` (via `SensorLib`).
- **Gesture SOS (`TrekLinkSOSGesture`):** Polls single-button COTS hardware (v3.0/v4.0) for a 3-second hold to trigger/cancel emergency mode, leaving short presses unhindered for screen navigation.
- **Strict Compile-Time Safety (`TrekLinkVariantValidation`):** Static assert system prevents flashing mismatching configurations or running multiple variant flags simultaneously.
- **Local Defaults:** Auto-configured to Malaysia/Vietnam 433MHz region (`MY_433`), timezone `ICT-7`, and pre-loaded with Search & Rescue brevity canned messages.

---

## 🏗️ Project Structure

```
treklink/
├── variants/
│   ├── esp32/
│   │   ├── treklink_v1_0/          # v1.0 variant overlay & build flags
│   │   └── treklink_v3_tbeam/      # v3.0 T-Beam v1.2 overlay
│   └── esp32s3/
│       ├── treklink_v2_0/          # v2.0 Custom PCB S3 overlay
│       └── treklink_v4_supreme/    # v4.0 T-Beam Supreme S3 overlay
├── src/
│   ├── modules/
│   │   ├── sensors/
│   │   │   ├── MPU6050FallSensor   # v1.0 IMU adapter
│   │   │   ├── ICM20948FallSensor  # v2.0 IMU adapter
│   │   │   └── QMI8658FallSensor   # v4.0 IMU adapter
│   │   ├── FallSensorInterface.h   # Sensor abstraction layer
│   │   ├── FallDetectionModule     # Thread for fall logic & alarm
│   │   ├── TrekLinkSOSHelper       # Centralized SOS state & actions
│   │   ├── TrekLinkButtonModule    # Dedicated button handler (v1.0/v2.0)
│   │   ├── TrekLinkSOSGesture      # Single button gesture handler (v3.0/v4.0)
│   │   ├── BuzzerManager.h         # Shared thread-safe buzzer singleton
│   │   └── TrekLinkVariantValidation.h # Compile-time configuration check
│   └── main.cpp                    # BLE name and early init
├── platformio.ini                  # Core build environment settings
└── README.md                       # This file
```

---

## 🧪 Verification & Boot Logs

When a device boots, it checks the configured hardware profile and prints the active variant to the serial monitor:

```
[INFO] Variant: TrekLink v4.0 (T-Beam Supreme)
[INFO] TrekLink SOS Gesture Module initialized (BUTTON_PIN=0)
[INFO] TrekLink Fall Detection Module initialized (QMI8658)
```

Pairing via the mobile app will advertise the BLE name as `TrekLink_XXXX` (based on MAC address).

---

## ⚠️ Hardware Caution: Flashing & Recovery (v2.0 Only)

> [!CAUTION]
> **CRITICAL, must attention!**
> The TrekLink v2.0 Custom PCB has a known hardware limitation affecting the boot power latch and USB CDC reset lines.
> **Note:** Other variants (v1.0, v3.0, v4.0) are NOT affected by this issue.

### Root Cause Analysis
1. **Linux ModemManager Probe (Major):**
   - Linux `ModemManager` detects `/dev/ttyACM0` (native ESP32-S3 USB CDC).
   - It probes the port by opening it and toggling DTR/RTS lines.
   - On the ESP32-S3, toggling DTR/RTS forces a chip reset, dropping the USB connection and creating a cyclic disconnect/reconnect loop.
2. **Soft-Latch Power Drop:**
   - On a blank or unlatched flash, `IO9_PWR_EN` (GPIO 9) remains low/floating.
   - Power stays ON only while physically holding the `POWER` button (SW3). Releasing it instantly cuts the 3.3V rail.

---

### Resolution & Flashing Sequence

To flash or recover the v2.0 board, select the appropriate method below:

1. **Stop ModemManager (on Host PC):**
   ```bash
   sudo systemctl stop ModemManager
   ```

2. **Observe USB Enumeration (Diagnostic):**
   Before connecting the board, start a kernel log watcher to identify USB device nodes and detect ModemManager interference:
   ```bash
   sudo dmesg -wH
   ```
   Connect the v2.0 board via USB-C. Watch for `cdc_acm` or `ttyACM0` device recognition messages. If you see rapid connect/disconnect cycling, ModemManager is still interfering — stop it first (Step 1). Press `Ctrl+C` to stop dmesg once enumeration is confirmed stable.

3. **Choose Flashing Method:**

   #### Method A: If the board is brand new / unflashed
   - **Step 1:** Press and hold `POWER` and `SELECT` (acts as BOOT / GPIO 0) buttons simultaneously, then immediately click `RESET`.
   - **Step 2:** Release `SELECT`, but continue holding `POWER`.
   - **Step 3:** Place a clamp on the `POWER` button to keep the 3.3V rail active without manual pressure.
   - **Step 4:** Release manual pressure. The clamp now holds the power state.
   - **Step 5:** Run the upload command: `pio run -e treklink-v2 -t upload`.
   - **Step 6:** Keep the clamp on until the upload finishes. Once completed, remove the clamp. The latch on GPIO 9 will now hold the power state automatically.

   #### Method B: If the board is already flashed (Reflashing)
   - **Step 1:** Place a clamp on the `POWER` button to hold it down physically.
     > [!IMPORTANT]
     > Flashing erases/resets the flash memory. As soon as the upload begins, the firmware-driven GPIO 9 latch drops, which will instantly cut power to the 3.3V rail unless a physical clamp holds the power button down.
   - **Step 2:** Run the upload command: `pio run -e treklink-v2 -t upload`.
   - **Step 3:** Keep the clamp on until the upload finishes. Once completed, remove the clamp. The newly flashed firmware will latch GPIO 9 and keep the board powered.

4. **Post-Flash Verification:**
   - Verify that the device stays powered on automatically after the clamp is removed.
   - Check that the SH1106 or SSD1306 OLED screen turns on and displays the UI.

---

## 🔌 Flashing Instructions (COTS Boards: v3.0 & v4.0)

For LilyGo T-Beam (v3.0) and LilyGo T-Beam Supreme (v4.0) variants, use one of the bootloader entry sequences below to put the chip into firmware download mode before running `pio run -t upload`.

### 1. LilyGo T-Beam Supreme (v4.0)

#### Method A: The Button Sequence Method
1. **Identify the buttons:** Orient the board so the antenna is on your right side. Looking at the three buttons, they are ordered from left to right: **Reset**, **Power**, and **Boot** (the Boot button is furthest from the antenna).
2. Press and hold both the **Reset** button and the **Boot** button at the same time.
3. While continuing to hold both, release the **Reset** button first.
4. Immediately after, release the **Boot** button.
5. The device screen will remain completely blank/off, but it is now in Espressif Firmware Download mode and ready to be detected by the flashing software.

#### Method B: Alternative Plug-In Method
If the device is fully powered down, you can use this alternative sequence:
1. Ensure the USB-C cable is completely unplugged from the device.
2. Press and hold down the **Boot** button.
3. While holding the button, plug the USB-C cable into your computer.
4. Keep holding the **Boot** button for 2 to 3 seconds, then release it.

> [!TIP]
> After done flashing, click the **RESET** button once to boot into the new firmware.

### 2. LilyGo T-Beam v1.2 (v3.0)

#### Method A: Automatic Bootloader Entry (Recommended)
The LilyGo T-Beam v1.2 has an onboard auto-reset circuit driven by the CH9102 USB-to-UART chip. In most cases, you do **not** need to press any physical buttons. Running `pio run -t upload` or using a web-based flasher will automatically negotiate the serial RTS/DTR lines to reset the board into download mode.

#### Method B: Manual Jumper Fallback (If Auto-Flash Fails)
Because the T-Beam v1.2 lacks a physical `BOOT` button connected to GPIO 0, manual bootloader entry requires bridging the boot configuration strapping pin to ground:
1. **Unplug** the USB/power cable from the T-Beam.
2. Locate the header pins along the side of the board and find the pin labeled **0** (or **IO0** / **GPIO0**) and a neighboring **GND** pin.
3. Use a jumper wire, paperclip, or metal tweezers to **bridge/connect** the **0** pin directly to a **GND** pin.
4. While keeping the pins bridged, **plug the USB cable** back into your computer.
5. The device should start up with the screen blank/off, indicating it is now in firmware download mode. You may now remove the jumper wire and run the flashing/upload process.

> [!TIP]
> Done flashing, click the **RESET** button once to boot into the new firmware.

---

## 📜 License

- **TrekLink Custom Additions:** MIT License
- **Meshtastic Core Codebase:** GPL-3.0 License

---

## 📞 Support & Resources

- **Issues:** [GitHub Issues](https://github.com/ruskicoder/treklink/issues)
- **Meshtastic Discord:** [Join Community](https://discord.gg/meshtastic)
- **Documentation:** [Meshtastic Docs](https://meshtastic.org/docs/)

---

**Happy Meshing!** 📡🏔️
