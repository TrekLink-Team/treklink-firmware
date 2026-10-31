---
id: test-runs
type: section
status: approved
owner: KhoaDD
summary: Dated hardware test records, one file per run, never edited after the run is closed.
---

# Hardware test runs

One file per run: `YYYY-MM-DD-<variant>-<topic>.md`. A closed run is history; correct it with a new
file, never by editing the old one.

Each file records:

- **Date, tester, variant, firmware commit** (the `git rev-parse --short HEAD` of the flashed image).
- **What was exercised**, one line per behaviour (for SOS: trigger, ping, cancel, beacon cadence,
  fall pre-alarm, fall auto-SOS).
- **Evidence** for each line: serial log excerpt, received packet on a second node or the
  platform, photo or video. A behaviour without evidence is recorded as `observed, no evidence`.
- **Result** per line: `pass`, `fail`, `observed, no evidence`, or `not tested`.

## Index

| Date | Variant | Topic | Result |
|---|---|---|---|
| 2026-10-29 | v2 | [SOS field observation](2026-10-29-v2-sos-observation.md) | trigger observed; rest not tested |
| 2026-10-29 | v2 | [Hardware run: SOS, fall, compass](2026-10-29-v2-hardware-run.md) | SOS trigger partial, buzzer fails after a beep, fall detection fails, compass fails uncalibrated |
