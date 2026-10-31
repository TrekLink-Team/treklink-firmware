---
id: fall-detection-tasks
type: tasks
status: draft
owner: KhoaDD
summary: Implementation tasks for the fall-detection fixes F4, F5, F6 and F15, one stacked branch per finding on test/fw-rc1.
---

# Fall detection: tasks

Reads with [requirements.md](requirements.md) and [design.md](design.md). Class S (chapter 17
§1). The branches stack on `test/fw-rc1` (`f71f0a312`), which already changes the same module for
the common SOS pattern; the release candidate is the top of the stack.

## Branch 1: `fix/fall-imu-awake` (F5)

- [ ] 1.1 This spec suite, first commit
- [ ] 1.2 ICM-20948 adapter calls `sleep(false)` before each read (design §6.1)
  - _Requirements: REQ-FALL-IMU-01_
- [ ] 1.3 Build treklink-v2

## Branch 2: `feat/fall-imu-datalog`

- [ ] 2.1 `FallDatalog.h` macros and the `treklink-v2-datalog` env (design §7)
- [ ] 2.2 `treklink/tools/imu/` capture, summary and plot tools, with a README
- [ ] 2.3 Build treklink-v2-datalog; capture on the v2: resting on the bench (screen on, then off),
  handled, dropped from a stated height onto a mattress, a simulated trip
  - _Requirements: REQ-FALL-LOG-01, and the evidence for F5 and F15_

## Branch 3: `fix/fall-detection-window` (F4 and the signature)

- [ ] 3.1 `FallDetector.h` core per design §2 and §3
- [ ] 3.2 `test/treklink_fall/`: the old rule misses a fall with bounce (fails before the fix);
  free fall path; impact path; short free fall ignored; a move inside the window restarts the run;
  no run in 20 s gives "user is OK"; a run starting late completes; timestamps with 27 ms jitter
  - _Requirements: REQ-FALL-SIG-01, REQ-FALL-STL-01_
- [ ] 3.3 Module uses the core at 20 ms; ICM and QMI ranges and the QMI rate (design §4, §6)
  - _Requirements: REQ-FALL-RNG-01, REQ-FALL-PRE-01_
- [ ] 3.4 `replay.cpp` over the captures of 2.3; set the provisional constants from them and record
  the reason in the run file

## Branch 4: `fix/fall-calibration` (F6 and F15)

- [ ] 4.1 `FallCalibration.h` core and its native tests: accepted still run, rejected moving run,
  rejected tilted mean, threshold clamp, bias removal
  - _Requirements: REQ-FALL-CAL-01_
- [ ] 4.2 Module: boot calibration, stored version 2 values, erase of the old keys, 60 s retry
- [ ] 4.3 Build matrix at the top of the stack: treklink-v2, treklink-v2-datalog, treklink-v3,
  treklink-v4, tbeam, tbeam-s3-core; `pio test -e native -f treklink_fall`

## Hardware run (top of the stack, new run file)

- [ ] 5.1 v2: F5 (fall detection with the screen timed out), F4 (drop onto a mattress, pre-alarm,
  cancel; drop, then auto-SOS), F6 (calibration log line, old keys erased), F15 (still after bias
  removal), and the chapter 17 §3 regression set
- [ ] 5.2 v4: boot log shows the QMI8658 range and rate, one drop; or "not run" with the reason
