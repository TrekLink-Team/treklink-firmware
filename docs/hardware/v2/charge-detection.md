# v2 charge detection

## What the board gives the firmware

The v2 charger is a TI BQ24074 (U8). Checked on 2026-11-02 against the KiCad project the board was made from
(`treklink.kicad_pcb`, `treklink-rounded.kicad_pcb`) and the fab netlist (`production/netlist.ipc`, 2026-06-07):

| Signal | Board | Effect |
|---|---|---|
| U8 pad 9 ~CHG (open drain, low while charging) | `unconnected-(U8-~{CHG}-Pad9)` | no charging line |
| U8 pad 7 ~PGOOD (open drain, low with input power) | `unconnected-(U8-~{PGOOD}-Pad7)` | no "external power" line |
| VBUS | C21, C24, F1, U8-6, U8-13, U9-5 only | VBUS reaches no MCU pin |
| GPIO 1 (IO1_BAT_DET) | R29/R30 100k/100k from `/Power/BAT+` | the cell voltage, the only charge information |

Other charger settings on the board: EN2 = GND and EN1 = VBUS, so the BQ24074 runs in USB500 mode with the input
limited to 500 mA, and the 1 A charge current set by ISET (R28, 887 Ω) is not reached. ILIM (R27, 1k6) only applies
in the EN2 = 1, EN1 = 0 mode and has no effect. TS is a fixed 10 kΩ to ground, which turns the battery temperature
check off. The hardware is final; these are recorded, not changed.

## What the firmware does

`src/platform/extra_variants/treklink_v2/charge_detect.cpp`, enabled by `TREKLINK_CHARGE_FROM_VOLTAGE` in the v2
`variant.h`:

- Every 2 s it averages 16 ADC reads of the battery divider and takes the median of the last three samples.
- A median at least 15 mV above the running reference for 3 samples (6 s) means a charger was connected; 15 mV
  below for 3 samples means it was removed. Between steps the reference follows the slow charge or discharge drift.
- A USB host attached to the ESP32-S3 USB port (moving start-of-frame counter) counts as external power at once.
- External power and below 4150 mV is charging; external power at or above 4150 mV is charged.
- `Power.cpp` uses these answers for `isVbusIn()` and `isCharging()` on v2 only (two lines marked `TrekLink`), so
  the screen, the logs and the telemetry show the state. A change triggers an immediate power status update.

Measured steps (bench, 2026-11-02): +32 to +36 mV at 3.6 V and +20 mV at 4.17 V when a charger is connected, the
same size down when it is removed, noise about 2 mV between samples. Test record:
`docs/test-runs/2026-11-02-v2-charge-detection.md`.

## Limits

- A unit that boots already on a wall charger shows no external power until the charger is removed and connected
  again, because there is no step to see. On a PC the USB host makes it immediate.
- A cell that has finished charging may show no step when a charger is connected.
- 4150 mV marks "charged" while the BQ24074 may still be in its constant-voltage phase.
- (unverified) A long transmission or the vibration motor on battery could shift the voltage; the 6 s hold is meant
  to absorb it, and no false change was seen, but a soak with transmissions on battery was not recorded.
