---
id: test-run-2026-10-29-v2-hardware
type: test-run
status: closed
owner: KhoaDD
summary: First logged v2 hardware run. SOS, fall detection and compass exercised with serial logs; fall detection cannot trigger and the SOS buzzer is silenced by any stock beep.
verified: {commit: ea37a06, date: 2026-10-29, scope: "every source citation in this file"}
---

# 2026-10-29: v2 hardware run (SOS, fall detection, compass)

- **Tester:** KhoaDD handled the board; the agent flashed it and read the serial log.
- **Unit:** v2 custom PCB, node `!0aa3a268` ("TrekLink a268"), USB-JTAG MAC `E8:F6:0A:A3:A2:68`.
- **Firmware:** `dev` at `ea37a06`, env `treklink-v2`, built with
  `PLATFORMIO_BUILD_FLAGS=-DARDUINO_USB_CDC_ON_BOOT=1` and no source change. The flag only moves
  the serial console to USB (see finding F1). The image reports `2.7.19.ea37a06`
  (`logs/2026-10-29-v2/boot2.log:29`).
- **Flash:** esptool, bootloader `0x0`, partitions `0x8000`, `boot_app0` `0xe000`, app `0x10000`.
  NVS and LittleFS were kept, so the unit's stored fall calibration is the field one.
- **Conditions:** indoors, no GPS fix, battery 16 to 19 %, USB cable to a PC. No second node and no
  platform listening, so reception was not checked.
- **Logs:** `logs/2026-10-29-v2/`. `boot2` is a warm reset with the power button held, `cold1` a
  cold power-up, `run2` the SOS, fall and compass tests after `cold1`, `run3` the calibration
  retest and the screen-off checks after a later power-up whose boot lines were not captured.

## Results

| Behaviour | Result | Evidence |
|---|---|---|
| SOS trigger, before any button beep | pass, partly: buzzer on, LED strobe, 3 vibration pulses, then no vibration | leader; `run2.log:4`, `:26-27` |
| SOS trigger after one UP press | **fail**: buzzer silent | leader; `run2.log:69` (`tone()` on pin 11), `:72`, `:95` |
| SOS packets per episode, no GPS fix | 2: an empty position and `SOS - [No GPS]`, priority 127, `WantAck=0` | `run2.log:4-40` |
| Beacon cadence, no GPS fix | **fail**: every beacon skipped | `run2.log:44`, `:48`, `:52` and onward |
| SOS cancel (3 s hold) | local only: buzzer off, LED back to heartbeat, 2 long pulses; nothing transmitted | leader; `run2.log:65-67` |
| Fall, 80 cm drop onto a bed | not detected (free fall 421 ms, below 500 ms) | leader; `run2.log:148` |
| Fall, 1.5 m drop onto a bed, screen on | **fail**: fall detected, then dismissed 100 ms later | leader; `run2.log:155-159` |
| Fall, 1.5 m drop, screen timed out | **fail**: no free-fall line at all | leader; `run3.log:9` and no `Freefall` line in `run3.log` |
| Fall pre-alarm, auto-SOS | not tested: unreachable through a drop | |
| Compass, uncalibrated, flat | **fail**: needle between 9:00 and about 10:30 over a full turn | leader; `run2.log:198` (offsets all zero) |
| Compass, uncalibrated, upright | **fail**: needle between about 7:30 and 9:30 | leader |
| Compass during calibration | pass: needle tracks | leader |
| Leaving the calibration screen | **fail**: stuck until reboot | leader |
| IMU detection, warm reset | **fail**: ICM-20948 read as MPU6050 | `boot2.log:25-26`, `:86`, `:158` |
| IMU detection, cold power-up | pass | `cold1.log:25-26`, `:87`, `:157` |
| Unit stuck in SOS (2026-09-25 report) | not reproduced | |

## Findings

Line citations are to `src/` at `ea37a06` unless stated.

- **F1. v2 has no serial console or client API on USB.** The env's board, `esp32-s3-devkitc-1`, sets
  `ARDUINO_USB_MODE=1` and not `ARDUINO_USB_CDC_ON_BOOT`, so `Serial` is UART0 on GPIO 43/44
  (Arduino-ESP32 2.0.17 `cores/esp32/HardwareSerial.h:326-333`). Two 20 s reads of the stock image
  returned 0 bytes. No phone or computer client can talk to a v2 over USB. `tbeam-s3-core` sets the
  flag, so v4 is not affected.
- **F2. A stock beep disables the SOS buzzer.** Arduino-ESP32 `tone()` uses LEDC channel 0 and
  detaches the pin when the note ends (`cores/esp32/Tone.cpp:9`, `:33-47`). `BuzzerManager` attaches
  `PIN_BUZZER` to channel 0 once (`modules/BuzzerManager.h:35-45`, `boot2.log:103`) and never again.
  Stock beeps on every input event (`buzz/BuzzerFeedbackThread.cpp:31-46`, `buzz/buzz.cpp:67`) and
  ExternalNotification rings on the same pin for incoming messages (`mesh/NodeDB.cpp:812-818`).
- **F3. No continuous vibration during SOS.** `activateAlarms()` sets the vibrator on
  (`modules/TrekLinkSOSHelper.cpp:76-78`); the 3-pulse confirmation then ends with
  `vibratorPulse(false)` (`modules/TrekLinkButtonModule.cpp:268`).
- **F4. Fall detection cannot reach the pre-alarm.** The 10 s stillness check starts at the end of
  free fall, which is the impact (`modules/FallDetectionModule.cpp:337-345`), and cancels on the
  first sample that is not still (`:353-356`): total acceleration within 0.2 g of 1 g and gyro below
  the calibrated threshold (`:62-71`). Impact and bounce are never still.
- **F5. The shared IMU sleeps while the screen is off.** After a cold boot the fall adapter joins the
  stock `ICM20948Singleton` (`modules/sensors/ICM20948FallSensor.cpp:18-34`, `cold1.log:157`). Stock
  `ICM20948Sensor::runOnce()` calls `sleep(true)` when the screen is off unless
  `wake_on_tap_or_motion` or `double_tap_as_button_press` is set (`motion/ICM20948Sensor.cpp:50-56`,
  `run3.log:9`). v2 sets neither, and the screen times out after 600 s (`mesh/Default.h:24`).
- **F6. Stored fall thresholds are the clamp floors**, 0.30 g and 0.050 rad/s (`boot2.log:160`,
  `modules/FallDetectionModule.cpp:205-206`). Self-calibration multiplies noise in m/s² by 3.5 and
  uses it as a threshold in g (`:202-206`; adapters return m/s², `modules/FallSensorInterface.h:12`).
  Free fall must stay below 0.3 g for 500 ms, sampled every 100 ms (`modules/FallDetectionModule.h:71`).
- **F7. IMU detection depends on the boot type.** After a warm reset the stock scan read register
  0x00 at 0x68 as `0x0` and classified the chip as MPU6050 (`detect/ScanI2CTwoWire.cpp:576-596`); the
  stock compass driver failed and the fall adapter started a second driver instance with no compass.
  A cold power-up read `0xea`. (inferred) The ICM-20948 keeps a non-zero register bank across an
  ESP32 reset because it stays powered; register 0x00 of bank 2 reads 0x00.
- **F8. The uncalibrated compass is dominated by a field from the board.** Calibration offsets start
  at zero on every boot (`run2.log:198`, `motion/ICM20948Sensor.h:94`) and are never saved. With
  30 s of tumbling the needle tracked (leader). (inferred) A fixed field about 2.5 times Earth's, from
  parts near the IMU such as the steel 21700 cells, the vibration motor, the buzzer or the E22 shield.
- **F9. The calibration screen cannot be left without a reboot.** `endAlert()` queues
  `STOP_ALERT_FRAME`, which only clears `pauseBanner` (`graphics/Screen.cpp:876-878`);
  `START_ALERT_FRAME` cleared `showingNormalScreen` (`:862`), and every runtime `setFrames()` and the
  frame navigation are gated on it (`:1438`, `:1459-1472`, `:1699`). The countdown is `uint8_t` of
  an unsigned difference and wraps after zero (`motion/MotionSensor.cpp:46`). Stock behaviour.
- **F10. Holding MENU 300 ms shuts a v1 or v2 down at once.** `input/TrekLinkButtonInput.cpp:31`
  maps the long press to `INPUT_BROKER_SHUTDOWN`; `input/UpDownInterruptBase.h:7` and `.cpp:86-89`
  fire it at 300 ms while held; `modules/SystemCommandsModule.cpp:115-116` acts with no confirmation.
- **F11. With no GPS fix an SOS is two packets and then silence.** Beacons log `Skip position send;
  no fresh position since boot` (`run2.log:44`), already recorded in ground truth §2.
- **F12. Battery telemetry reports `usbPower=0`** with the USB cable connected (`run2.log:3`); the
  pack read 3.37 to 3.42 V. Not investigated.
- **F13. `E (2435) ledc: ledc_get_duty(745): LEDC is not initialized`** at every boot
  (`boot2.log:76`). Source not identified.

## Not done

Fall pre-alarm, button cancel of a pre-alarm, fall auto-SOS, reception on a second node or the
platform, and any v3 or v4 run. Fixes belong to a later fixing session, on `fix/*` branches with a
hardware run each.
