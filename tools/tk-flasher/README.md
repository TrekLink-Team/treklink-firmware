# tk-flasher

A keyboard-only terminal tool that installs, updates, verifies and erases TrekLink firmware over
USB, with a serial monitor and a Meshtastic node info screen. One native binary per operating
system. Python, esptool and PlatformIO are not needed at run time.

It is written in Rust with [ratatui](https://ratatui.rs) and crossterm for the screens, the
[espflash](https://github.com/esp-rs/espflash) crate as a library for flashing, erasing, MD5
checks and serial access, and the [meshtastic](https://github.com/meshtastic/rust) crate for node
info.

Status: built and unit tested on Linux, cross-built for Windows. It has not yet been run against
real hardware; see "What has been tested" at the end.

## Install

Download the archive for your system from the `tk-flasher` workflow run in the GitHub Actions tab
of `TrekLink-Team/treklink-firmware` (artifacts `tk-flasher-x86_64-linux`,
`tk-flasher-x86_64-windows`, `tk-flasher-x86_64-macos`, `tk-flasher-aarch64-macos`). Each holds
the binary and this README.

- Linux: `tar xzf tk-flasher-x86_64-linux.tar.gz`. The binary is static (musl) and runs on any
  x86_64 distribution.
- Windows: unzip `tk-flasher-x86_64-windows.zip` and run `tk-flasher.exe` from Windows Terminal or
  PowerShell.
- macOS: `tar xzf tk-flasher-aarch64-macos.tar.gz` (Apple silicon) or the `x86_64` one (Intel).
  The binary is not signed. Remove the download quarantine once with
  `xattr -d com.apple.quarantine ./tk-flasher`, or allow it under System Settings, Privacy and
  Security.

To build from source, install Rust 1.95 or newer, then:

```bash
cd tools/tk-flasher
cargo build --release
```

The binary is `target/release/tk-flasher` (`tk-flasher.exe` on Windows).

## Run

```bash
./tk-flasher
```

Other options: `--list-ports` prints the serial ports and exits, `--version`, `--help`.

Environment variables, all optional:

- `GITHUB_TOKEN`: sent to `api.github.com` when set. Needed only if the repository is private, or
  to avoid the hourly rate limit for requests without a token.
- `TK_FLASHER_REPO`: `owner/name` of a fork to read releases from. The default is
  `TrekLink-Team/treklink-firmware`.

Every screen has a help line at the bottom. Arrow keys (or `j` and `k`) move, `Enter` chooses,
`Esc` goes back, `q` quits from the menus. Every action, chip detection, region plan, MD5 and
result is appended to `tk-flasher.log` next to the binary (or in the current folder when the
binary's folder is read-only).

## Variants

Checked against the PlatformIO envs in this repository.

| Code | Board                    | Chip     | PlatformIO env        | USB link                    | Flash |
| ---- | ------------------------ | -------- | --------------------- | --------------------------- | ----- |
| v1   | TrekLink v1.0 custom PCB | ESP32    | `treklink`            | USB-UART (CP210x or CH34x)  | 4 MB  |
| v2   | TrekLink v2.0 custom PCB | ESP32-S3 | `treklink-v2`         | native USB (303a:1001)      | 8 MB  |
| v3   | LilyGo T-Beam            | ESP32    | `treklink-v3-tbeam`   | USB-UART (CP210x or CH9102) | 4 MB  |
| v4   | LilyGo T-Beam Supreme    | ESP32-S3 | `treklink-v4-supreme` | native USB (303a:1001)      | 8 MB  |

v1 is not built by the release workflow (`.github/workflows/treklink_release.yml` builds v2, v3
and v4), so it always needs a local image. v2 keeps itself powered with a latch that the firmware
holds; see screen 5.

## Screens

### 1. Port picker

Lists serial ports with the USB VID:PID and a friendly name:

| VID:PID                         | Shown as                          | Usually  |
| ------------------------------- | --------------------------------- | -------- |
| 303a:1001                       | ESP32-S3 native USB (Serial/JTAG) | v2 or v4 |
| 10c4:ea60                       | Silicon Labs CP210x USB-UART      | v1 or v3 |
| 1a86:55d4, 1a86:55d3, 1a86:7523 | WCH CH9102, CH343, CH340 (CH34x)  | v1 or v3 |

Press `r` to refresh after plugging a unit in. On macOS only the `/dev/cu.*` node of each device
is listed.

### 2. Variant picker

Choose v1 to v4 (keys `1` to `4` work too). The details pane shows the chip, the env, the flash
size and whether the port you picked looks like the right kind of USB link. A mismatch is a
warning only; the real gate is the chip check: before any write the tool reads the chip type from
the ROM bootloader (the same detection esptool does) and refuses to flash when it does not match
the variant, with the reason on the result line. It also refuses when the reported flash is
smaller than the variant needs.

### 3. Image picker

- Latest GitHub release: reads `treklink-manifest.json` from the latest release, picks the entry
  whose `variantCode` matches, checks its chip and env against the table above, downloads the
  factory and application images and checks their size and SHA-256 against the manifest. A copy
  is kept in `tk-flasher-downloads/<tag>/` next to the binary. The screen says whether
  `GITHUB_TOKEN` is set. Without it only a public repository can be read; for a private one, quit,
  set `GITHUB_TOKEN` and start again.
- Local factory image: a `firmware-*.factory.bin`, the merged image written at 0x0.
- Local parts: pick the application `.bin`. `bootloader.bin`, `partitions.bin` and
  `boot_app0.bin` are taken from the same folder when present (a PlatformIO
  `.pio/build/<env>/` folder has the first two); otherwise the tool asks for the bootloader and
  the partition table. Without a `boot_app0.bin` the standard arduino-esp32 one is used, which
  selects the first app slot.

Every image is checked before it is accepted: ESP image headers, the chip ID inside them (an
ESP32 image is refused for an ESP32-S3 variant and the other way round), the partition table
magic, an application partition at 0x10000 that the application fits in, and, for a release, that
the application image is the same build as the one inside the factory image.

### 4. Actions

| Action     | What it writes                                                                | Settings |
| ---------- | ----------------------------------------------------------------------------- | -------- |
| Install    | erases the whole flash, then writes the factory image at 0x0                  | erased   |
| Update     | bootloader, partitions at 0x8000, boot_app0 at 0xe000, application at 0x10000 | kept     |
| Verify     | nothing; compares the MD5 of each Update region on the chip with the file     | kept     |
| Erase only | erases the whole flash                                                        | erased   |

The bootloader goes to the chip's boot address: 0x1000 on ESP32 (v1, v3) and 0x0 on ESP32-S3
(v2, v4). For a factory image, Update and Verify cut the four regions out of it at those offsets.
NVS (0x9000) and the LittleFS partition are never touched by Update, which is why settings
survive.

Install, Update and Erase show the plan (each region, its offset and size) and wait for `y`. Any
other key cancels without writing. Verify is read-only and needs no confirm.

During a job each region has a progress bar, and the result line says what happened. Writes are
verified by MD5 as they go. The port runs at 115200 baud throughout; a higher baud switch failed
once on a v2. Native USB ports use the USB Serial/JTAG reset sequence, USB-UART bridges the
classic DTR and RTS sequence. After the job the unit is reset into the firmware.

Release images carry no Bluetooth OTA loader and no LittleFS image. After Install those
partitions are blank; the firmware formats LittleFS on first boot (`src/FSCommon.h`).

### 5. v2 power latch

Before any action that resets a v2 (all four above, because connecting to the bootloader is a
reset), the tool shows "Hold the power button now and keep holding until done" and waits for
`Enter`. Keep holding until the result line appears. Without the button held the unit loses
power on reset.

### 6. Serial monitor

Opens the port at 115200 baud without resetting the unit. An ESP32-S3 on native USB resets when
RTS is high while DTR is low, so RTS is set low first and DTR is left high; neither step passes
through a reset state. Colour codes are stripped.

Keys: `Space` or `p` pause, arrows and `PgUp` `PgDn` scroll (scrolling also pauses), `End`
follows the output again, `s` saves the scrollback to `monitor-<time>.log` next to the binary,
`c` clears, `q` returns to the menu.

### 7. Node info

Opens the port the same way as the monitor, runs the Meshtastic config handshake and shows the
node number, long and short name, firmware version, hardware model, battery, LoRa region and the
TrekLink variant. The firmware does not send the variant over the API: the tool looks for the
`Variant: TrekLink ...` boot log line, and otherwise infers it from the hardware model (TBEAM is
v3, LILYGO_TBEAM_S3_CORE is v4, a `TrekLink` default name on another model means v1 or v2) and
says that it is inferred. If no Meshtastic reply arrives within 6 seconds the screen says the
unit is not running Meshtastic and suggests the serial monitor. `r` retries.

## Troubleshooting

- No ports listed: use a data cable, not a charge-only one, and press `r`.
- Linux, "Permission denied": add yourself to the serial group and log in again.
  On Debian, Ubuntu and Fedora the group is `dialout`, on Arch it is `uucp`:

  ```bash
  sudo usermod -aG dialout "$USER"
  ```

  Also close anything else holding the port (a serial monitor, ModemManager probing a new
  device).
- Windows, a v1 or v3 does not appear as a COM port: install the USB-UART driver. CP210x: the
  Silicon Labs "CP210x USB to UART Bridge VCP" driver. CH340: the WCH CH341SER driver. CH343 and
  CH9102: the WCH CH343SER driver. Check Device Manager under "Ports (COM and LPT)". The v2 and v4 native USB
  port needs no driver on Windows 10 and 11.
- macOS port names: native USB shows up as `/dev/cu.usbmodem*`, CP210x as
  `/dev/cu.SLAB_USBtoUART` or `/dev/cu.usbserial-*`, CH34x as `/dev/cu.wchusbserial*` or
  `/dev/cu.usbserial-*`. Use the `cu` node; the tool hides the `tty` twins.
- "Could not talk to the bootloader": check the port and cable, then put the unit in download
  mode by hand: hold BOOT, tap RESET, release BOOT, and retry. A v2 also needs the power button
  held.
- "Refused: variant ... needs an ESP32-S3, but the connected chip is an ESP32": the wrong variant
  is selected, or the wrong unit is plugged in. Nothing was written.
- "Verify: everything matches except boot_app0": the unit has switched app slots since the last
  write (for example after a Bluetooth OTA). The firmware itself matches.
- GitHub 404: there is no release yet, or the repository is private and `GITHUB_TOKEN` is not
  set. GitHub 403: the rate limit for requests without a token; set `GITHUB_TOKEN` or wait.

## Development

```bash
cargo fmt --check
cargo clippy --all-targets -- -D warnings
cargo test
```

The unit tests cover the offset tables, the region plans for each action, image and partition
table checks, manifest parsing, the chip check, port classification, node info parsing and the
rendering of every screen. One test talks to the real GitHub release and is skipped by default:

```bash
cargo test -- --ignored live_release
```

The workflow `.github/workflows/tk-flasher.yml` runs on `workflow_dispatch` and on tags matching
`tk-flasher-v*`. It runs `cargo fmt --check`, `cargo clippy -D warnings` and `cargo test`, then
builds release binaries for x86_64 Linux (musl, static), x86_64 Windows, and x86_64 and aarch64
macOS, and uploads them as artifacts. The macOS x86_64 binary is cross-built on the Apple silicon
runner.

## Licences

This tool lives in a GPL-3.0 firmware repository and is distributed under GPL-3.0
(`license = "GPL-3.0-only"` in `Cargo.toml`, matching the repository `LICENSE`). Its main
dependencies:

- espflash: MIT or Apache-2.0, compatible with GPL-3.0.
- meshtastic (Rust crate): GPL-3.0, the same licence as this repository, which is what allows
  linking it into the binary.
- ratatui, crossterm, serialport, tokio, ureq, serde: MIT or Apache-2.0.

## What has been tested

Tested in a Linux sandbox without hardware: `cargo build --release` (glibc and static musl),
`cargo test`, `cargo clippy -D warnings`, `cargo fmt --check`, a Windows cross-build, the live
release test (downloads v2, v3 and v4 from `v2.7.26-treklink.1`, checks SHA-256, and builds valid
Install and Update plans from the real images), and the screens driven in a pseudo-terminal
(release download, confirm, cancel, the v2 latch prompt, the node info timeout).

Not tested: anything that talks to a real unit (connect, chip detection, erase, write, MD5
verify, reset, the v2 power latch timing, the serial monitor output, the node info handshake),
the macOS builds, and the Windows binary at run time.
