# screencap

Captures the TrekLink device screens with no device attached, for guides and reviews. A native (Linux) build of
the firmware runs the real device UI and draws into a 128x64 in-memory display instead of the OLED. A script
presses the buttons the way a person does and saves the screens it is told to as PNG files.

The screens come from the firmware's own drawing code, so a guide image changes only when the firmware UI
changes. Rerun the capture after any UI change.

## Build

One native env per variant, defined in `variants/native/treklink_screencap/platformio.ini`:

```bash
pio run -e treklink-screencap-v2 -e treklink-screencap-v3 -e treklink-screencap-v4
```

Each build is `.pio/build/treklink-screencap-<variant>/meshtasticd`. It is a host program, not a device image:
never flash it. The host needs the native build dependencies of Meshtastic (`libyaml-cpp`, `libuv`, `libgpiod`,
`libulfius`, `openssl`). Python 3 with Pillow runs the scripts.

## Capture

```bash
tools/screencap/capture-all.sh            # every variant
tools/screencap/capture-all.sh v3         # one variant
OUT=/some/dir tools/screencap/capture-all.sh
```

Images land in `tools/screencap/shots/<variant>/` (or `$OUT/<variant>/`), with `firmware.log` from the last run.
Each image is the 128x64 panel scaled 4x with nearest neighbour: lit pixels white on black, like the OLED.

`capture-all.sh` runs `scenarios/first-boot-<variant>.txt` on a fresh node, keeps the node state it ends with
(region MY_433), and starts every other scenario of that variant from that state. It gives every run a
made-up GPS fix on a trail near Sa Pa (`GPS=lat,lon,alt` overrides it, `GPS=` turns it off).

## Scenarios

One step per line; blank lines and lines starting with `#` are skipped.

| Step                  | Meaning                                                        |
| --------------------- | -------------------------------------------------------------- |
| `press <button> <ms>` | Hold a button for that many milliseconds, then release it      |
| `wait <ms>`           | Let the firmware run                                           |
| `heading <degrees>`   | Point the simulated compass (v2 only, which has the ICM-20948) |
| `shot <name>`         | Save the current screen as `<name>.png`                        |

Buttons follow the hardware:

| Variant | Buttons                         | Firmware behaviour                                                             |
| ------- | ------------------------------- | ------------------------------------------------------------------------------ |
| v2      | `up`, `down`, `menu`, `sos`     | DOWN next, UP previous, MENU select; MENU held 300 ms shuts down; SOS held 3 s |
| v3, v4  | `user` (the middle user button) | Short press next, 500 ms select or menu, 3 s hold SOS                          |

To run one scenario by hand:

```bash
tools/screencap/screencap.py --variant v3 --boot-wait 900 \
  --scenario tools/screencap/scenarios/sos-v3.txt --out /tmp/shots
```

## How it works

- `TREKLINK_SCREENCAP` builds `src/graphics/CaptureDisplay.cpp`, an `OLEDDisplay` with no panel that keeps the
  last frame the UI pushed. `src/main.cpp` reports a 128x64 SSD1306 so the firmware starts its screen.
- `src/platform/portduino/TrekLinkScreenCap.cpp` binds each TrekLink button to a simulated GPIO, active low like
  the hardware, and reads `press`, `shot` and `quit` commands on stdin. The firmware's own button code reads
  those pins, so menus, long presses and the SOS hold behave as on a device.
- The node runs in simulated-radio mode (`-s`) on a temporary virtual filesystem, with a made-up MAC
  (`02:00:00:00:A1:23`, node `a123`) so no image matches a real unit.
- `--gps` opens a pseudo-terminal, passes its path in `TREKLINK_SCREENCAP_GPS` (bound to `Serial1` in
  `src/platform/portduino/PortduinoGlue.cpp`), answers the firmware's module probe as an L76K and writes a GGA
  and RMC fix every second.
- On v2 the build reports the ICM-20948 and `src/motion/ScreenCapMotionSensor.cpp` stands in for it: it runs
  the same compass calibration steps as `ICM20948Sensor` on a simulated magnetometer that `heading` turns.

## Limits

- No motion sensor on the host: fall detection is not built, so there are no fall screens.
- No other nodes: node lists are empty. Without `--gps`, position frames show "No GPS".
- The first-boot splash shows for about a second, so `01-boot-region-unset` can catch the screen before or
  after it; rerun the first-boot scenario if it does.
- Button timing follows the firmware threads, so a screen can take a frame or two to settle; the scenarios
  wait before each `shot`.
