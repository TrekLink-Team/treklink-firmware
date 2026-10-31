#!/usr/bin/env python3
"""Summarise IMU captures recorded with `tk.py imu` (specs/fall-detection REQ-FALL-LOG-01).

    python3 treklink/tools/imu/summary.py docs/test-runs/imu/*-samples.csv

Per file: the real sample period, the acceleration and angular-rate spread, the per-axis gyro mean
(the bias a calibration would remove), the longest run below the free-fall threshold and the impacts
above the impact threshold. Python 3 standard library only.
"""

import csv
import math
import statistics
import sys

FF_G = 0.5  # specs/fall-detection design §3
IMPACT_G = 3.0
G = 9.80665


def pct(values, p):
    if not values:
        return float("nan")
    s = sorted(values)
    k = (len(s) - 1) * p / 100.0
    lo, hi = math.floor(k), math.ceil(k)
    return s[lo] + (s[hi] - s[lo]) * (k - lo)


def summarise(path):
    rows = []
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            try:
                rows.append({k: float(r[k]) for k in ("t_ms", "ax", "ay", "az", "gx", "gy", "gz", "accel_g", "gyro_rads")})
            except (KeyError, ValueError):
                continue
    if len(rows) < 2:
        print(f"{path}: fewer than 2 samples")
        return
    t = [r["t_ms"] for r in rows]
    dt = [b - a for a, b in zip(t, t[1:]) if b > a]
    a = [r["accel_g"] for r in rows]
    w = [r["gyro_rads"] for r in rows]
    dev = [abs(x - 1.0) for x in a]

    longest_ff = 0.0
    start = None
    for r in rows:
        if r["accel_g"] < FF_G:
            start = r["t_ms"] if start is None else start
            longest_ff = max(longest_ff, r["t_ms"] - start)
        else:
            start = None
    impacts = sum(1 for x, y in zip(a, a[1:]) if x > IMPACT_G and y > IMPACT_G)

    print(f"{path}")
    print(f"  samples {len(rows)}, span {(t[-1] - t[0]) / 1000:.1f} s, period median {statistics.median(dt):.1f} ms, p95 {pct(dt, 95):.1f} ms")
    print(f"  |a| g    p1 {pct(a, 1):.3f}  p50 {pct(a, 50):.3f}  p99 {pct(a, 99):.3f}  max {max(a):.3f}")
    print(f"  |a-1| g  p50 {pct(dev, 50):.3f}  p99 {pct(dev, 99):.3f}")
    print(f"  |w| rad/s p50 {pct(w, 50):.3f}  p99 {pct(w, 99):.3f}  max {max(w):.3f}")
    for axis in ("gx", "gy", "gz"):
        v = [r[axis] for r in rows]
        print(f"  {axis} mean {statistics.fmean(v):+.4f} rad/s, std {statistics.pstdev(v):.4f}")
    for axis in ("ax", "ay", "az"):
        v = [r[axis] / G for r in rows]
        print(f"  {axis} mean {statistics.fmean(v):+.4f} g, std {statistics.pstdev(v):.4f}")
    print(f"  longest run below {FF_G} g: {longest_ff:.0f} ms; impacts above {IMPACT_G} g on two samples: {impacts}")


def main(paths):
    if not paths:
        print(__doc__)
        return 2
    for p in paths:
        summarise(p)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
