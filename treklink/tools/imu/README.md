# IMU capture for fall-detection tuning

Bench procedure for [specs/fall-detection](../../../specs/fall-detection/requirements.md)
REQ-FALL-LOG-01. Every command runs from the repository root with PlatformIO's Python, which has
pyserial: `PY=~/.platformio/penv/bin/python`.

1. Build and flash a datalog image (a diagnostic image that also streams the IMU). Never ship it.

   ```bash
   $PY treklink/tools/tk.py build --env treklink-v2 --datalog --jobs 2
   $PY treklink/tools/tk.py flash --env treklink-v2 --datalog
   ```

2. Record one file per action. The label says what the unit did; the files go to
   `docs/test-runs/imu/` unless `--out` says otherwise.

   ```bash
   $PY treklink/tools/tk.py imu --label rest-screen-on --seconds 60
   $PY treklink/tools/tk.py imu --label rest-screen-off --seconds 700
   $PY treklink/tools/tk.py imu --label drop-1m-mattress --seconds 40
   ```

   The screen of a v2 times out after 600 s, so a screen-off rest needs a longer capture (finding
   F5).

3. Summarise:

   ```bash
   python3 treklink/tools/imu/summary.py docs/test-runs/imu/*-samples.csv
   ```

Each capture writes `<time>-<label>-samples.csv` (`label,seq,t_ms,ax,ay,az,gx,gy,gz,accel_g,gyro_rads,state`;
acceleration and angular rate per axis are raw, in m/s2 and rad/s; `gyro_rads` is the magnitude after the gyro bias of the last accepted calibration is removed; state 255 during the boot calibration), `-events.csv` (state
changes and the calibration result) and `-console.log` (every other console line). Check the console
log for keys before committing it: a client that asks for the configuration makes the unit print it.
