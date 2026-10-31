---
id: test-run-2026-10-31-v2-overlay-sos-pattern
type: test-run
status: closed
owner: KhoaDD
summary: First v2 run of the TrekLink overlay on upstream v2.7.26 and of the common SOS pattern, unattended, with button presses injected by the diagnostic test hooks. Logs only; nothing heard, felt or seen.
---

# 2026-10-31: v2 overlay on v2.7.26 and the SOS pattern (unattended)

- **Tester:** the agent alone. KhoaDD clamped the power button and stopped ModemManager, then left;
  nobody watched, listened to or touched the unit during the run.
- **Unit:** v2 custom PCB, node `!0aa3a268` ("TrekLink a268"), MAC `e8:f6:0a:a3:a2:68`.
- **Images, both diagnostic** (`-DARDUINO_USB_CDC_ON_BOOT=1 -DTREKLINK_TEST_HOOKS=1`):
  1. `2.7.26.ba1cd1d`, branch `chore/overlay-on-upstream-2-7-26` at `ba1cd1d` plus the uncommitted
     test-hook and MENU changes later committed as `3006130`
     (`logs/2026-10-31-v2/overlay-warm1.log:28`, `overlay-nav3.log`). It is no head of any branch,
     so it counts as a development check only.
  2. `2.7.26.5cd7a71`, the head of `fix/sos-alarm-pattern` when it was flashed
     (`logs/2026-10-31-v2/boot-warm.log:28`), kept as tag `run/2026-10-31-v2-image-5cd7a71`. The
     branch was then rebased onto two documentation commits and a CI file; `git diff 5cd7a71 7140be7`
     touches only `.github/workflows/overlay.yml` and `specs/architecture/tasks.md`, so the code under
     test is the code of `7140be7`.
     The diagnostic flags move the log to USB and add the press hook; neither touches the alarm,
     radio or button code under test.
- **Flash:** `tk.py flash --diagnostic`, application only (bootloader `0x0`, partitions `0x8000`,
  `boot_app0` `0xe000`, app `0x10000`). NVS and LittleFS kept, so the unit's stored fall calibration
  (`ff=0.30, gyro=0.050`) and settings from the 2.7.19 image stayed.
- **Conditions:** indoors, no GPS fix, power button clamped the whole time (so the latch itself was
  not tested), USB cable to the PC, other TrekLink nodes in the node list.
- **Button presses:** `tk.py test "press <gpio> <ms>"` makes the diagnostic image pull that GPIO low
  through open drain, which is what the physical button does. Every button result below is an
  injected press, not a finger.

## Results

| Behaviour | Result | Evidence |
|---|---|---|
| Image identity logged (REQ-ARC-08) | pass | `boot-warm.log:128` |
| Warm reset, IMU detection | **fail, intermittent, stock**: the scan read the ICM-20948 as MPU6050 and stock init failed; the fall adapter fell back to its own driver (no compass) as designed (O-11) | `boot-warm.log:24`, `:72`, `:147` |
| Warm reset, IMU detection, image 1 | pass: ICM20948 found, stock init ok, fall adapter joined the stock driver | `overlay-warm1.log:24`, `:149` |
| Radio init (SX1268, TCXO 1.8 V), region MY_433, time zone ICT-7 | pass | `boot-warm.log:62`, `:77`, `:127` |
| SOS trigger (injected 3.5 s press on GPIO 4) | pass in the log: position and `SOS - [No GPS]` at priority 127, alarms on, SOS pattern on | `sos.log:30`, `:34-50`, `:52` |
| SOS packets on air | sent, but 14 s after the trigger: the radio was busy receiving (`Can not send yet, busyRx`) | `sos.log:55-56`, `:112`, `:131-134` |
| Beacons with no GPS fix | skipped, as before (F11) | `sos.log:63` |
| Stock input during SOS (MENU press) | the event reached the InputBroker; SOS stayed active | `sos.log:94` |
| SOS cancel (second injected 3.5 s press) | pass in the log: alarms off, pattern off | `sos.log:185-188` |
| MENU press, MENU hold, UP, DOWN (image 1) | one event each: select 10, select-long 11, 29, 28; no double event after the MENU fix | `overlay-nav3.log:31`, `:61` |
| Buzzer audible, after a stock beep (F2) | **not run**: nobody listened | |
| Vibration pattern (F3) | **not run**: nobody felt it | |
| LED pattern | **not run**: nobody watched | |
| Cold power-up | not run: no one to cycle power | |
| Fall drop, screen on and off | not run | |
| Long press of each button, no shutdown | MENU 1.5 s and SOS 3.5 s: no shutdown; UP and DOWN long: not run | `overlay-nav3.log:61` |
| Power latch after flash and reboot | not run: the clamp held the button | |

## Findings

- **F7 (2026-10-29) still happens on upstream v2.7.26.** After the warm reset that follows an
  esptool flash, stock `ScanI2CTwoWire` classified the ICM-20948 at 0x68 as MPU6050
  (`boot-warm.log:24`) and stock `AccelerometerThread::init` failed (`:72-73`). Another warm reset of
  the same unit read it correctly (`overlay-warm1.log:24`). This is a stock bug present in the latest
  stable release, so chapter 17 §6 rule 2 applies: an upstream issue once the leader approves.
- **F14. SOS packets wait for a busy channel.** Both SOS packets were queued at once at priority 127
  and left the radio 14 s later, after `Can not send yet, busyRx` (`sos.log:55-56`, `:112`, `:131-134`). Indoors
  with several TrekLink nodes in range. Not investigated.

## Not done

Everything a person has to hear, feel or see, cold power-up, the latch, fall drops, and v3 and v4.
The pattern timing is covered by the native test `test/treklink_alarm` (4 tests, 0 failures, host
build).
