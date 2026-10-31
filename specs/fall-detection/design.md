---
id: fall-detection-design
type: design
status: draft
owner: KhoaDD
summary: How fall detection meets requirements.md on the overlay - a platform-free detector core, the module around it, the adapters, calibration and the datalog build.
---

# Fall detection: design

Reads with [requirements.md](requirements.md) and the overlay architecture
(`specs/architecture/design.md`). Line numbers are at `test/fw-rc1` `f71f0a312` unless stated.

## 1. Layers

| Layer | File | Platform |
|---|---|---|
| Detector core: signature, settle, stillness window | `treklink/src/fall/FallDetector.h` (new, header only) | none; host and device |
| Calibration core: statistics, acceptance, threshold | `treklink/src/fall/FallCalibration.h` (new, header only) | none |
| Module: thread, sensor reads, NVS, pre-alarm, auto-SOS | `treklink/src/fall/FallDetectionModule.{h,cpp}` | firmware |
| Adapters: units, range, wake | `treklink/src/fall/sensors/*FallSensor.cpp` | firmware |
| Datalog tap | `treklink/src/fall/FallDatalog.h` (new) | firmware, compiled out unless `TREKLINK_IMU_STREAM` |
| Host tools | `treklink/tools/imu/` | host |

The cores take one sample at a time, `{t_ms, accel_g[3], gyro_rads[3]}`, and hold no pointers to
Arduino objects, so `test/treklink_fall/` runs them under `pio test -e native` and host `g++`, and
the replay tool feeds a capture through the same code.

## 2. Detector core

`FallDetector::step(sample)` returns `NONE`, `CHECK_STARTED`, `USER_OK` or `RAISE_PREALARM`. It
works on magnitudes: `a = |accel|` in g and `w = |gyro - bias|` in rad/s.

| State | On a sample | Next |
|---|---|---|
| `IDLE` | `a < FF_G`: note the start time | `FREE_FALL` |
| `IDLE` | `a > IMPACT_G` on this and the previous sample | `SETTLE` (`CHECK_STARTED`, cause impact) |
| `FREE_FALL` | `a >= FF_G` and free fall lasted `>= FF_MIN_MS` | `SETTLE` (`CHECK_STARTED`, cause free fall) |
| `FREE_FALL` | `a > IMPACT_G` on two samples | `SETTLE` (`CHECK_STARTED`, cause impact) |
| `FREE_FALL` | `a >= FF_G` sooner | `IDLE` |
| `SETTLE` | `SETTLE_MS` since the check started | `WATCH`; the still count is empty |
| `WATCH` | sample is still: the still run continues; it reached `STILL_MS` | `IDLE` after `RAISE_PREALARM` |
| `WATCH` | sample is not still | `WATCH`; the still run restarts at the next still sample |
| `WATCH` | `WATCH_MS` since `WATCH` began, no completed run | `IDLE` after `USER_OK` |

While a still run is open at the end of `WATCH_MS` the check waits for that run to finish or break,
so a run that starts at 19 s is not cut off (it can end the check at up to 30 s after the settle).
An impact inside `SETTLE` or `WATCH` does not restart the check: the person is being evaluated
already.

## 3. Constants

Provisional values; the datalog capture replaces them, and the run file records why.

| Constant | Value | Source |
|---|---|---|
| Requested sample period | 20 ms | REQ-FALL-SIG-01; an earlier prototype branch measured about 27 ms real on v2 (unverified here) |
| `FF_G` | 0.5 g | ruling |
| `FF_MIN_MS` | 300 ms | ruling |
| `IMPACT_G` | 3.0 g, two consecutive samples | ruling; the prototype recorded idle peaks of 1.4 to 2.6 g (unverified here) |
| `SETTLE_MS` | 2000 ms | ruling |
| `STILL_MS` | 10000 ms | REQ-SAF-02, ruling |
| `WATCH_MS` | 20000 ms | ruling |
| `STILL_ACCEL_BAND_G` | 0.2 g | unchanged from `checkInactivity()` |
| gyro stillness | from calibration, default 1.0 rad/s | REQ-FALL-CAL-01 |
| pre-alarm | 30000 ms | REQ-SAF-02 |

## 4. Module

- `runOnce()` returns 20 ms while monitoring and evaluating, keeps 200 ms in `SOS_TRIGGERED`, and
  passes `millis()` with each sample. The module states become `CALIBRATING`, `DETECTING` (the core
  runs), `PRE_ALARM` and `SOS_TRIGGERED`; `FREEFALL_DETECTED` and `INACTIVITY_DETECTED` move into
  the core.
- Log lines on `CHECK_STARTED` (cause, free-fall ms, peak g), `USER_OK` and `RAISE_PREALARM`, so a
  run file can quote them.
- Cancel paths, the pre-alarm signal and the auto-SOS call are kept as they are on `test/fw-rc1`
  (`FallDetectionModule.cpp:73-140`).

## 5. Calibration

- `FallCalibration` accumulates sums per axis in g and rad/s over 250 samples at the 20 ms period,
  then reports accept or reject with the reason (moving, tilted mean, noisy gyro).
- NVS namespace `fallcal` (unchanged name), keys `ver` (= 2), `bx`, `by`, `bz`, `gstill`. At start
  the module reads `ver`; any other value, or the old keys `ff_thresh` and `gyro_still`, is erased.
- Each boot calibrates afresh, because gyro bias moves with temperature; the stored value is the
  fallback when the boot calibration is rejected. A rejected run retries every 60 s from
  `DETECTING`, sampling at the same time as detection runs.
- Shared resource check (chapter 17 §4 rule 5): the namespace `fallcal` is used only by this module
  (`grep -rn fallcal` over `src/` and `treklink/` at `f71f0a312`).

## 6. Adapters

### 6.1 ICM-20948 (v2), REQ-FALL-IMU-01 and REQ-FALL-RNG-01

- `updateSensorData()` calls `icm->sleep(false)` before `getAGMT()`. Stock sleeps the chip once per
  screen-off and latches `isAsleep` (`src/motion/ICM20948Sensor.cpp:56-60`), so after the adapter
  wakes it stock does not sleep it again until the screen has been on and gone off once more; at
  most one 20 ms sample per screen-off is stale. When the screen comes back, stock's own
  `sleep(false)` (`:63-66`) is a no-op on an awake chip.
- `sleep(false)` is one register read and one write over I2C on the cooperative loop (SparkFun
  ICM-20948 1.3.2 `ICM_20948_sleep`), the same loop and bus the stock driver uses, so the two never
  interleave mid-transfer.
- Full scale becomes `gpm16` and `dps2000` on both the joined singleton and the fallback driver.
  The stock compass feeds raw accelerometer counts into `FusionCompassCalculateHeading`
  (`ICM20948Sensor.cpp:85-87`), which uses their direction only, so the range change does not move
  the heading (INFERRED, high confidence; checked on the v2 compass in the hardware run).
- Power: the IMU stays in its awake mode while the screen is off. This is the cost of fall
  detection; the release notes state it.

### 6.2 QMI8658 (v4), REQ-FALL-RNG-01

- `configAccelerometer(ACC_RANGE_16G, ACC_ODR_62_5Hz)` and
  `configGyroscope(GYR_RANGE_1024DPS, GYR_ODR_56_05Hz)`, the widest gyro range in SensorLib 0.3.4
  (`SensorQMI8658.hpp:53-61`). Stock 2.7.26 has no QMI8658 motion driver, so nothing else configures
  the chip (`specs/variants/v4/components.md`).

## 7. Datalog build, REQ-FALL-LOG-01

- `FallDatalog.h` defines `TREKLINK_IMU_SAMPLE(...)` and `TREKLINK_IMU_MARKER(...)`; without
  `TREKLINK_IMU_STREAM` both expand to nothing. With it, each sample prints
  `IMU,<t_ms>,<ax>,<ay>,<az>,<gx>,<gy>,<gz>,<a>,<w>,<state>` and each event
  `IMU_EV,<t_ms>,<name>,<detail>` through `Serial` directly, not the log system, so the lines stay
  parseable.
- Env `treklink-v2-datalog` in `variants/esp32s3/treklink_v2/platformio.ini` extends
  `treklink-v2` and adds `-DTREKLINK_IMU_STREAM=1 -DARDUINO_USB_CDC_ON_BOOT=1`.
- Tools, Python 3 standard library plus optional matplotlib for plots:
  `capture.py` (port, duration, label, writes CSV), `summary.py` (per file: sample period, a and w
  percentiles, dips and peaks), `plot.py`; and `replay.cpp`, a host program built from the two
  cores that prints every event for a capture, so a threshold change is tested against recorded
  falls before it is flashed.

## 8. Not in scope

- A learned classifier. An earlier prototype trained a 1D CNN (26,864 bytes INT8); its runtime,
  esp-tflite-micro, did not build under PlatformIO, and its recorded test specificity was 56.94 %.
  Parked by the leader on 2026-10-31 unless it is shown to fit the flash with CPU to spare.
- Severity classes on the wire. Any change to the SOS text is a ground-truth and backend change.
