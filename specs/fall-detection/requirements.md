---
id: fall-detection-requirements
type: requirements
status: draft
owner: KhoaDD
summary: Fall detection requirements for v2 and v4 after the 2026-10-29 v2 run (findings F4, F5, F6, F15) and the leader rulings of 2026-10-31.
hw_verified: none
---

# Fall detection: requirements

Applies to every variant with an IMU: v2 (ICM-20948) and v4 (QMI8658). v3 has no IMU and stays
excluded (REQ-V3-FALL-01). The trigger outcome is unchanged from REQ-SAF-02 of the v1 spec: a
30 s pre-alarm the person can cancel with any button, then the fall auto-SOS. This file replaces
the detection rule of REQ-SAF-02 criterion 1 for v2 and v4.

Every threshold below is provisional until a v2 capture with the datalog build (REQ-FALL-LOG-01)
confirms it; the capture and the chosen values are recorded in a test-run file and in
[design.md](design.md) §3.

## REQ-FALL-SIG-01: Fall signature (leader ruling, 2026-10-31)

1. WHILE fall detection runs and no fall is being evaluated, the system SHALL sample the IMU at a
   requested 20 ms period and SHALL measure every duration from sample timestamps, never from the
   requested period.
2. The system SHALL treat a sample as part of free fall WHEN the acceleration magnitude is below
   0.5 g.
3. WHEN free fall lasts at least 300 ms, THEN the system SHALL start the post-fall check of
   REQ-FALL-STL-01 at the first sample at or above 0.5 g.
4. WHEN the acceleration magnitude exceeds 3.0 g on at least two consecutive samples, THEN the
   system SHALL start the post-fall check at the last sample above 3.0 g, whether or not free fall
   came first.
5. A free fall shorter than 300 ms that ends without an impact SHALL return detection to its idle
   state with no alarm.
6. Known risk, accepted by the ruling: an impact above 3.0 g followed by stillness raises the
   pre-alarm even when only the unit fell, for example a unit dropped on a table and left there.
   The 30 s pre-alarm is the cancel path for that case.

## REQ-FALL-STL-01: Stillness after a fall (finding F4, leader ruling 2026-10-31)

1. WHEN the post-fall check starts, THEN the system SHALL ignore the next 2 s of samples (impact
   and bounce).
2. After those 2 s, WHEN the unit stays still for 10 s without a break, THEN the system SHALL raise
   the fall pre-alarm.
3. A sample that is not still SHALL restart the 10 s count and SHALL NOT end the check.
4. IF no 10 s still stretch completes within 20 s after the 2 s settle, THEN the system SHALL end
   the check as "user is OK", log it, and return to idle with no alarm.
5. A sample is still WHEN the acceleration magnitude is within 0.2 g of 1 g AND the
   bias-corrected angular rate magnitude (REQ-FALL-CAL-01) is below the gyro stillness threshold.
6. Root cause of F4, KNOWN at `80ccbe08a`: the stillness count started on the sample that ended
   free fall, which is the impact (`treklink/src/fall/FallDetectionModule.cpp:337-345`), and the
   first sample that was not still cancelled the check (`:353-356`).

## REQ-FALL-PRE-01: Pre-alarm and auto-SOS (unchanged behaviour)

1. WHILE the pre-alarm runs, the system SHALL play the pre-alarm signal of REQ-SOS-ALM-03 for 30 s.
2. IF any button is pressed during the pre-alarm, THEN the system SHALL cancel it and return to
   idle.
3. WHEN 30 s pass without a cancel, THEN the system SHALL send the fall auto-SOS and play the
   common SOS pattern (REQ-SOS-ALM-01). The wire format stays as recorded in ground truth §2.

## REQ-FALL-IMU-01: The shared ICM-20948 stays awake (finding F5, v2)

1. WHILE fall detection runs on v2, the ICM-20948 SHALL deliver fresh samples whether the screen is
   on or off.
2. Root cause of F5, KNOWN at `v2.7.26.54e0d8d`: stock `ICM20948Sensor::runOnce()` calls
   `sleep(true)` once when the screen is off and neither `config.display.wake_on_tap_or_motion` nor
   `config.device.double_tap_as_button_press` is set, then latches `isAsleep`
   (`src/motion/ICM20948Sensor.cpp:54-61`). The registers then keep the last sample, so the fall
   module reads a constant 1 g.
3. The fix SHALL NOT edit a stock file and SHALL NOT change the stock screen-wake settings (leader
   ruling, 2026-10-31).

## REQ-FALL-RNG-01: Sensor range and rate

1. The accelerometer SHALL be configured to at least ±16 g and the gyroscope to the widest range the
   driver offers up to ±2000 dps, so an impact peak is not clipped at ±8 g.
2. On v4 the QMI8658 output data rate SHALL be at least the 50 Hz sampling rate (62.5 Hz
   accelerometer; the SensorLib 0.3.4 default used before was 31.25 Hz).

## REQ-FALL-CAL-01: Gyro calibration (findings F6 and F15, leader ruling 2026-10-31)

1. WHEN fall detection starts, THEN the system SHALL collect 5 s of samples and SHALL accept them as
   a calibration only IF the mean acceleration magnitude is within 0.1 g of 1 g AND every
   accelerometer axis has a standard deviation below 0.05 g AND every gyroscope axis has a standard
   deviation below 0.1 rad/s.
2. An accepted calibration SHALL set the gyro bias (the per-axis mean, in rad/s) and the gyro
   stillness threshold (three times the mean per-axis standard deviation, clamped to 0.05 to
   0.3 rad/s). It SHALL NOT change the free-fall or impact thresholds, which are fixed.
3. IF the samples are rejected, THEN the system SHALL keep the last accepted calibration stored in
   flash, or, with none stored, a zero bias and a stillness threshold of 1.0 rad/s, and SHALL try
   again every 60 s until one is accepted in this boot.
4. Stored calibration SHALL carry a format version; values without the current version SHALL be
   ignored and erased, which discards the values written by the old self-calibration.
5. Root cause of F6, KNOWN at `80ccbe08a`: the old self-calibration took the noise in m/s² and
   used it as a threshold in g (`treklink/src/fall/FallDetectionModule.cpp:202-206`; adapters return
   m/s², `FallSensorInterface.h:12`), so a run with the unit moving could raise the free-fall
   threshold to its 1.0 g ceiling.
6. Finding F15, INFERRED (medium confidence, unverified on the current build): an earlier TrekLink
   v2 prototype branch recorded a resting gyro magnitude with a median of 0.75 rad/s (uncorrected
   bias) and a 99th percentile of 2.65 rad/s. Against a stillness threshold of 0.05 to 0.3 rad/s
   such a unit is never still, and the fall auto-SOS cannot fire. The capture of REQ-FALL-LOG-01
   on the current build confirms or refutes it.

## REQ-FALL-LOG-01: IMU datalog build (diagnostic only)

1. The firmware SHALL offer a v2 diagnostic env that streams every sample the fall detector acts on
   (timestamp, three accelerometer axes, three gyroscope axes, the two magnitudes, the detector
   state) as CSV lines over the USB console, plus a marker line at every state change.
2. The datalog env SHALL never be the release image: its name ends in `-datalog` and it is not in
   the release workflow.
3. Host tools in `treklink/tools/imu/` SHALL capture the stream to a file, summarise it per
   recording, plot it, and replay a capture through the same detector logic the firmware runs.
