---
id: fall-detection
type: section
status: todo
owner: KhoaDD
summary: Placeholder for a consolidated fall-detection module spec; today the rules are spread over the variant specs.
---

# Fall detection: module spec not yet written

There is no module-level spec suite for fall detection. This page records where the pieces live
until one is written and approved; it adds no requirement of its own.

## Where the requirements are today

| Source | Covers |
|---|---|
| [v1 requirements](../variants/v1/requirements.md) REQ-SAF-02 | Freefall, impact and inactivity signature; 30 s pre-alarm; cancel by any button; auto-SOS |
| [v2 requirements](../variants/v2/requirements.md) REQ-V2-IMU-01 | ICM-20948 adapter |
| [v3 requirements](../variants/v3/requirements.md) REQ-V3-FALL-01 | Excluded on v3, no IMU |
| [v4 requirements](../variants/v4/requirements.md) REQ-V4-FALL-01 | QMI8658 adapter |
| [shared tasks](../variants/shared/tasks.md) MV-2 to MV-4 | `FallSensorInterface`, MPU6050 adapter, module refactor |

## Where the code is

| File | Role |
|---|---|
| `src/modules/FallDetectionModule.{h,cpp}` | State machine, pre-alarm, auto-SOS; stub class on v3 |
| `src/modules/FallSensorInterface.h` | Sensor abstraction the module reads through |
| `src/modules/sensors/` | MPU6050, ICM-20948 and QMI8658 adapters |
| `src/modules/Modules.cpp:209-225` | Picks the adapter per variant, or logs that fall detection is disabled on v3 |

Field reports (v1 never triggers, v2 failed when tested, v4 untested) and the fall-path wire
defects are in `treklink-docs/_docs/00-project-context/04-firmware-ground-truth.md` §2.
