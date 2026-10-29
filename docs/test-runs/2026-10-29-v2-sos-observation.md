---
id: test-run-2026-10-29-v2-sos
type: test-run
status: closed
owner: KhoaDD
summary: Leader's report that SOS works on v2, recorded with what it does and does not establish.
---

# 2026-10-29: v2 SOS field observation

- **Reported by:** KhoaDD, in chat on 2026-10-29. This file was written from that report; no log was
  attached.
- **Variant:** v2 (`treklink-v2`).
- **Firmware commit flashed:** not recorded.

| Behaviour | Result | Evidence |
|---|---|---|
| SOS trigger (3 s hold on the SOS button) | observed, no evidence | leader report |
| Single-click position ping | not tested | |
| SOS cancel (3 s hold while active) | not tested | |
| Unit stuck in SOS (field report of 2026-09-25) | not tested | |
| Beacon cadence (5 s for one minute, then 30 s) | not tested | |
| Fall pre-alarm and auto-SOS | not tested | |

## Why "works" is not yet "verified"

- Neither SOS packet requests an acknowledgement (`want_ack = false`, `src/modules/TrekLinkSOSHelper.cpp:118-121`,
  `:157-160`), so one successful reception shows the path can work, not that it is reliable.
- A cancel transmits nothing (`TrekLinkSOSHelper.cpp:49-53`). The platform therefore cannot see a
  cancel or a unit stuck in SOS; both need the serial log or the device in hand.
- The image's commit was not recorded, so the result cannot be tied to a build.

The next v2 run should capture the serial log (`pio device monitor`) and the flashed commit.
