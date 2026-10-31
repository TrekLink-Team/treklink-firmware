---
id: fall-detection
type: section
status: draft
owner: KhoaDD
summary: Fall-detection module spec for v2 and v4; the variant specs keep only the adapter rules.
---

# Fall detection

| File | Content |
|---|---|
| [requirements.md](requirements.md) | Signature, stillness window, pre-alarm, IMU wake, range, calibration, datalog build |
| [design.md](design.md) | Detector and calibration cores, module, adapters, datalog build |
| [tasks.md](tasks.md) | The stacked fix branches and the hardware run |

## Other sources

| Source | Covers |
|---|---|
| [v1 requirements](../variants/v1/requirements.md) REQ-SAF-02 | Pre-alarm, cancel and auto-SOS; its detection rule (criterion 1) is replaced for v2 and v4 by REQ-FALL-SIG-01 and REQ-FALL-STL-01 |
| [v2 requirements](../variants/v2/requirements.md) REQ-V2-IMU-01 | ICM-20948 adapter |
| [v3 requirements](../variants/v3/requirements.md) REQ-V3-FALL-01 | Excluded on v3, no IMU |
| [v4 requirements](../variants/v4/requirements.md) REQ-V4-FALL-01 | QMI8658 adapter |
| [SOS requirements](../sos/requirements.md) REQ-SOS-ALM-01, -03 | The SOS pattern and the pre-alarm signal |

## Where the code is

| File | Role |
|---|---|
| `treklink/src/fall/FallDetectionModule.{h,cpp}` | Module: thread, calibration storage, pre-alarm, auto-SOS; stub class on v3 |
| `treklink/src/fall/FallSensorInterface.h` | Sensor abstraction (m/s² and rad/s) |
| `treklink/src/fall/sensors/` | ICM-20948 and QMI8658 adapters |

Field reports and the fall-path wire defects are in
`treklink-docs/_docs/00-project-context/04-firmware-ground-truth.md` §2; the v2 findings F4 to F7 in
`docs/test-runs/2026-10-29-v2-hardware-run.md`.
