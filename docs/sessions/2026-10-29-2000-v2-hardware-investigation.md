# 2026-10-29 20:00: v2 hardware investigation (SOS, fall detection, compass)

- **Goal**: read the TrekLink firmware in depth, then investigate the v2 compass, v2 and v4 fall detection, and SOS on v2 to v4. Investigation only; fixes belong to a later gated fixing session and an upstream PR.
- **Baseline**: `dev` at `ea37a06`.
- **Done**: code reading of every TrekLink module, the stock motion, button, buzzer and screen paths they touch, and the v2 IMU schematic; a logged hardware run on one v2 unit with a diagnostic image (`-DARDUINO_USB_CDC_ON_BOOT=1`, no source change). Record: `docs/test-runs/2026-10-29-v2-hardware-run.md`, logs in `docs/test-runs/logs/2026-10-29-v2/`. Variant matrix updated (`docs/variants.md`).
- **Main results**: the SOS buzzer is silenced by any stock beep; SOS has no continuous vibration; fall detection cannot reach the pre-alarm (stillness check starts at impact) and the shared IMU sleeps while the screen is off; IMU detection fails after a warm reset; the compass is unusable uncalibrated and its calibration screen cannot be left; MENU held 300 ms shuts v1/v2 down; v2 has no console on USB; v4 probes its IMU on the wrong bus (code only).
- **treklink-docs**: ground truth §2, §5, §7 and seven risk rows, on branch `docs/v2-hardware-findings`.
- **Not done**: fixes, v3 and v4 hardware runs, the fall pre-alarm and auto-SOS paths, reception on a second node, commits.
