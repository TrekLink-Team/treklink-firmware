---
id: sos
type: section
status: todo
owner: KhoaDD
summary: Placeholder for a consolidated SOS module spec; today the SOS rules are spread over the variant specs.
---

# SOS: module spec not yet written

There is no `requirements.md`, `design.md` or `tasks.md` for SOS as a module. The behaviour is
specified piecemeal in the variant specs and implemented in shared modules. This page records
where each piece lives until a consolidated spec is written and approved; it adds no requirement
of its own.

## Where the SOS requirements are today

| Source | Covers |
|---|---|
| [v1 requirements](../variants/v1/requirements.md) REQ-SAF-01 | Hold 3 s to trigger, click to ping, transmit cadence, hold 3 s to cancel, maximum power. Base set for all variants. |
| [v2 requirements](../variants/v2/requirements.md) REQ-V2-BTN-01 | SOS button moved to GPIO 4 with a pull-up |
| [v3 requirements](../variants/v3/requirements.md) REQ-V3-SOS-01 | 3 s hold gesture on the single user button, GPIO 38 |
| [v4 requirements](../variants/v4/requirements.md) REQ-V4-SOS-01 | Same gesture on GPIO 0 |
| [shared tasks](../variants/shared/tasks.md) MV-1, MV-5, MV-6 | Shared SOS helper, gesture module, registration in `Modules.cpp` |
| [onboard-queue requirements](../onboard-queue/requirements.md) REQ-EVT-03 | SOS text prefix the queue classifies as P0 |

## Where the SOS code is

| File | Role |
|---|---|
| `src/modules/TrekLinkSOSHelper.{h,cpp}` | Trigger (position then text packet), cancel, beacon cadence, alarms, wire-format prefix |
| `src/modules/TrekLinkButtonModule.{h,cpp}` | v1 and v2 dedicated SOS button |
| `src/modules/TrekLinkSOSGesture.{h,cpp}` | v3 and v4 hold gesture on the user button |
| `src/modules/TrekLinkVariantValidation.h` | Compile-time pairing of version flag and button layout |
| `src/modules/Modules.cpp:199-207` | Chooses the button module or the gesture module |

What the packets look like on the wire, and the known defects, are in
`treklink-docs/_docs/00-project-context/04-firmware-ground-truth.md` §2. Spec-versus-code gaps are
listed in [docs/variants.md](../../docs/variants.md#known-gaps-between-the-specs-and-the-code).
