# 2026-11-02: v2 charge detection from battery voltage steps

- **Date**: 2026-11-02, 00:20 to 01:10 (UTC+7)
- **Tester**: KhoaDD at the bench (plugging, screen checks); flashing and logs by the agent
- **Variant**: v2, units a294 (`e8:f6:0a:a3:a2:94`, about 3.6 V) and a278 (`e8:f6:0a:a3:a2:78`, about 4.17 V),
  both with 0.96" SSD1306 screens, power buttons clamped
- **Firmware**: branch `feat/v2-charge-detect` on `dev` `db34bca1a`, env `treklink-v2`, app only at 0x10000
  (NVS kept). Three images: a bench logger (2 s battery samples, not in the PR), the detector with the USB console
  (`-DARDUINO_USB_CDC_ON_BOOT=1`), and the detector as a release image.
- **Change class**: N (charge status on screen, logs and telemetry; no SOS, fall or power-latch path)

## Why

The BQ24074 CHG and PGOOD outputs are unconnected on the v2 board and VBUS reaches no MCU pin
(`docs/hardware/v2/charge-detection.md`). The only signal is the cell voltage on GPIO 1.

## Results

| Behaviour | Evidence | Result |
|---|---|---|
| Step size, charger connected and removed | logger samples: a294 3.600 to 3.608 V on a charger, 3.570 to 3.574 V off it (-36 mV at removal, +32 mV at reconnection); a278 4.168 to 4.172 V on, 4.148 to 4.150 V off (-20, +22 mV); noise about 2 mV; one single-sample spike of +44 mV on a294 (`logs/2026-11-02-v2-charge/*-chg-samples.log`) | pass |
| PC and wall charger give the same step | a278 on a wall charger 4.168 to 4.172 V with no USB host, the same as on the PC | pass |
| PC detected at boot (console image) | `TrekLink charge: external power connected (3618 mV, USB host)` on a294, `(4166 mV, USB host)` on a278 (`*-det-events.log`) | pass |
| Charging versus charged | a294 `usbPower=1, isCharging=1, batMv=3614`; a278 `usbPower=1, isCharging=0, batMv=4164` | pass |
| Wall charger connected and removed by voltage step (console image) | a294 log after replug: `external power connected (3618 mV)` with no USB host, `external power removed (3578 mV)`, then `connected (3590 mV, USB host)` | pass |
| Screen states, release image | leader: a294 shows charging on the PC and on the wall charger and stops when unplugged; a278 shows plugged (not charging); switches took 2 to 10 s | observed, no evidence |
| No false "connected" on battery while transmitting | five direct messages from a278 to a294 during 4 min on battery were not acknowledged, so no reply transmission was confirmed; the USB log is empty while unplugged | not tested |

## Notes

- Detection time is 2 to 10 s: a step must hold 3 samples of 2 s, and the samples run on their own clock.
- Known limits are listed in `docs/hardware/v2/charge-detection.md` (boot on a wall charger, a finished cell,
  4150 mV threshold).
