---
id: build-and-flash
type: guide
status: approved
owner: KhoaDD
summary: How to build, flash, monitor and test the TrekLink firmware with PlatformIO.
verified: {commit: 09f2a81, date: 2026-10-29, scope: "env names and test env read from platformio files; commands not run in this pass"}
---

# Build, flash and test

Consolidates the former `docs/development-rules.md` and `docs/lifecycle.md` (kept in
[`.deprecated/2026-10-29-repo-docs-v0/`](.deprecated/2026-10-29-repo-docs-v0/)). Git and pull
request rules are not repeated here: they are `treklink-docs/_docs/01-conventions/07-github-workflow-git-conventions.md`.

## Prerequisites

- Python 3.9 or later.
- PlatformIO Core (`pip install platformio`) or the PlatformIO extension for VS Code.

## Environments

The root `platformio.ini` loads every `variants/*/*/platformio.ini` and defaults to the stock
`tbeam` env (`platformio.ini:5`), so always pass `-e`.

| Variant | Env | Stock regression env |
|---|---|---|
| v1 (deprecated) | `treklink` | not applicable |
| v2 | `treklink-v2` | not applicable |
| v3 | `treklink-v3-tbeam` | `tbeam` |
| v4 | `treklink-v4-supreme` | `tbeam-s3-core` |

Board details per variant: [variants.md](variants.md).

## Commands

| Action | Command |
|---|---|
| Build firmware only (avoids the LittleFS image step) | `pio run -e <env> -t buildprog` |
| Full build | `pio run -e <env>` |
| Clean | `pio run -e <env> -t clean` |
| Flash | `pio run -e <env> -t upload` |
| Serial monitor | `pio device monitor` |
| Native unit tests (includes `test/test_onboard_queue/`) | `pio test -e native` |

`native` is defined in `variants/native/portduino/platformio.ini:14`.

## Flashing

Moved here from the repository `README.md` on 2026-10-29. The v2 specs carry a shorter copy of
the v2 sequence in their caution blocks ([requirements](../specs/variants/v2/requirements.md),
[tasks](../specs/variants/v2/tasks.md)); this section is the full procedure.

### v2: power latch and USB reset caution

> [!CAUTION]
> **CRITICAL, must attention!**
> The TrekLink v2.0 Custom PCB has a known hardware limitation affecting the boot power latch and USB CDC reset lines.
> **Note:** Other variants (v1.0, v3.0, v4.0) are NOT affected by this issue.

#### Root Cause Analysis
1. **Linux ModemManager Probe (Major):**
   - Linux `ModemManager` detects `/dev/ttyACM0` (native ESP32-S3 USB CDC).
   - It probes the port by opening it and toggling DTR/RTS lines.
   - On the ESP32-S3, toggling DTR/RTS forces a chip reset, dropping the USB connection and creating a cyclic disconnect/reconnect loop.
2. **Soft-Latch Power Drop:**
   - On a blank or unlatched flash, `IO9_PWR_EN` (GPIO 9) remains low/floating.
   - Power stays ON only while physically holding the `POWER` button (SW3). Releasing it instantly cuts the 3.3V rail.

---

#### Resolution & Flashing Sequence

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
   Connect the v2.0 board via USB-C. Watch for `cdc_acm` or `ttyACM0` device recognition messages. If you see rapid connect/disconnect cycling, ModemManager is still interfering; stop it first (Step 1). Press `Ctrl+C` to stop dmesg once enumeration is confirmed stable.

3. **Choose Flashing Method:**

   ##### Method A: If the board is brand new / unflashed
   - **Step 1:** Press and hold `POWER` and `SELECT` (acts as BOOT / GPIO 0) buttons simultaneously, then immediately click `RESET`.
   - **Step 2:** Release `SELECT`, but continue holding `POWER`.
   - **Step 3:** Place a clamp on the `POWER` button to keep the 3.3V rail active without manual pressure.
   - **Step 4:** Release manual pressure. The clamp now holds the power state.
   - **Step 5:** Run the upload command: `pio run -e treklink-v2 -t upload`.
   - **Step 6:** Keep the clamp on until the upload finishes. Once completed, remove the clamp. The latch on GPIO 9 will now hold the power state automatically.

   ##### Method B: If the board is already flashed (Reflashing)
   - **Step 1:** Place a clamp on the `POWER` button to hold it down physically.
     > [!IMPORTANT]
     > Flashing erases/resets the flash memory. As soon as the upload begins, the firmware-driven GPIO 9 latch drops, which will instantly cut power to the 3.3V rail unless a physical clamp holds the power button down.
   - **Step 2:** Run the upload command: `pio run -e treklink-v2 -t upload`.
   - **Step 3:** Keep the clamp on until the upload finishes. Once completed, remove the clamp. The newly flashed firmware will latch GPIO 9 and keep the board powered.

4. **Post-Flash Verification:**
   - Verify that the device stays powered on automatically after the clamp is removed.
   - Check that the SH1106 or SSD1306 OLED screen turns on and displays the UI.

---

### v3 and v4: LilyGo bootloader entry

For LilyGo T-Beam (v3.0) and LilyGo T-Beam Supreme (v4.0) variants, use one of the bootloader entry sequences below to put the chip into firmware download mode before running `pio run -t upload`.

#### 1. LilyGo T-Beam Supreme (v4.0)

##### Method A: The Button Sequence Method
1. **Identify the buttons:** Orient the board so the antenna is on your right side. Looking at the three buttons, they are ordered from left to right: **Reset**, **Power**, and **Boot** (the Boot button is furthest from the antenna).
2. Press and hold both the **Reset** button and the **Boot** button at the same time.
3. While continuing to hold both, release the **Reset** button first.
4. Immediately after, release the **Boot** button.
5. The device screen will remain completely blank/off, but it is now in Espressif Firmware Download mode and ready to be detected by the flashing software.

##### Method B: Alternative Plug-In Method
If the device is fully powered down, you can use this alternative sequence:
1. Ensure the USB-C cable is completely unplugged from the device.
2. Press and hold down the **Boot** button.
3. While holding the button, plug the USB-C cable into your computer.
4. Keep holding the **Boot** button for 2 to 3 seconds, then release it.

> [!TIP]
> After done flashing, click the **RESET** button once to boot into the new firmware.

#### 2. LilyGo T-Beam v1.2 (v3.0)

##### Method A: Automatic Bootloader Entry (Recommended)
The LilyGo T-Beam v1.2 has an onboard auto-reset circuit driven by the CH9102 USB-to-UART chip. In most cases, you do **not** need to press any physical buttons. Running `pio run -t upload` or using a web-based flasher will automatically negotiate the serial RTS/DTR lines to reset the board into download mode.

##### Method B: Manual Jumper Fallback (If Auto-Flash Fails)
Because the T-Beam v1.2 lacks a physical `BOOT` button connected to GPIO 0, manual bootloader entry requires bridging the boot configuration strapping pin to ground:
1. **Unplug** the USB/power cable from the T-Beam.
2. Locate the header pins along the side of the board and find the pin labeled **0** (or **IO0** / **GPIO0**) and a neighboring **GND** pin.
3. Use a jumper wire, paperclip, or metal tweezers to **bridge/connect** the **0** pin directly to a **GND** pin.
4. While keeping the pins bridged, **plug the USB cable** back into your computer.
5. The device should start up with the screen blank/off, indicating it is now in firmware download mode. You may now remove the jumper wire and run the flashing/upload process.

> [!TIP]
> Done flashing, click the **RESET** button once to boot into the new firmware.

## Before opening a pull request

- `pio run -e <env> -t buildprog` succeeds for every env the change touches, plus the stock
  regression env for v3 or v4 changes.
- `pio test -e native` passes when `src/mqtt/` or `src/serialization/` changes.
- No build artifacts (`.pio/`), credentials or debug prints are staged.
- Hardware-affecting changes get a dated entry in [test-runs/](test-runs/README.md).

`(unverified)` whether the upstream GitHub Actions workflows in `.github/workflows/` build the
TrekLink envs. The repository's own check is `checksum.yml`, which verifies the commit `Checksum`
trailer.
