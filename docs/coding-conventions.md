---
id: coding-conventions
type: guide
status: approved
owner: KhoaDD
summary: Embedded C/C++ practice for TrekLink code in this Meshtastic fork, each safety rule tied to the defect that motivated it.
verified: {commit: cd5329c, date: 2026-10-30, scope: "every source citation in this file"}
---

# TrekLink coding conventions

How TrekLink code is written in this repository. Replaces the 2026-02-04 version, which described a
greenfield firmware (FreeRTOS tasks per core, a `hal/ services/ ui/` tree, a `debugEnabled`
console) rather than the Meshtastic fork this is; it is kept in
[`.deprecated/2026-10-30-coding-conventions-v1/`](.deprecated/2026-10-30-coding-conventions-v1/).

The process around a change (change classes, the fix loop, the hardware-run gate, the upstream
policy) is chapter 17 of the conventions,
`treklink-docs/_docs/01-conventions/17-firmware-development.md`. Git, commit and wording rules are
`AGENTS.md` and chapters 07 and 14. Citations are to `src/` at `cd5329c` unless stated. Findings
`F1` to `F13` are in [test-runs/2026-10-29-v2-hardware-run.md](test-runs/2026-10-29-v2-hardware-run.md).

## 1. Upstream style comes first

TrekLink code follows the Meshtastic style described in `.github/copilot-instructions.md`
("Coding Conventions"):

- Format with `trunk fmt` before committing.
- `PascalCase` classes, `camelCase` functions, methods and members, `UPPER_SNAKE_CASE` constants
  and defines.
- Log with `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN` and `LOG_ERROR`.
- Read configuration through `config.*`, `moduleConfig.*` and `channels.*`, and defaults through
  the helpers in `src/mesh/Default.h`.
- Use `concurrency::Lock` for mutual exclusion and `OSThread` for periodic work.

The rest of this file adds what TrekLink needs on top.

## 2. Where code goes

| Code | Location |
|---|---|
| TrekLink modules | `src/modules/TrekLink*`, `src/modules/FallDetectionModule*`, `src/modules/BuzzerManager.h` |
| IMU adapters behind `FallSensorInterface` | `src/modules/sensors/` |
| Onboard MQTT queue | `src/mqtt/TrekLink*` |
| TrekLink inputs | `src/input/TrekLink*` |
| Pins and capabilities per variant | `variants/*/treklink_*/variant.h`; see [variants.md](variants.md) |
| Platform-free logic cores and their tests | beside the module, tested from `test/test_<module>/` (§9) |

Variant code is selected with the variant flags, `TREKLINK_VARIANT` and `TREKLINK_V1` to
`TREKLINK_V4` (for example `src/modules/Modules.cpp:97-100`), never with board names. Pins come from
`variant.h` macros only. An edit to a stock file is guarded by one of these flags, as at
`src/main.cpp:240` and `src/mesh/NodeDB.cpp:1098`, so stock envs build unchanged and the upstream
sync can find it (chapter 17 §6).

## 3. Execution model

Meshtastic runs modules cooperatively on the Arduino loop. A module derives from
`concurrency::OSThread`; `runOnce()` returns the delay in milliseconds until its next run,
`RUN_SAME` keeps the period, and `disable()` stops it (`src/concurrency/OSThread.h:16`, `:56`,
`:67-69`).

1. **`runOnce()` returns quickly.** Every thread on the loop waits while one runs. TrekLink code
   adds no `delay()` and no wait without a timeout in `runOnce()` or in code it calls. One exists
   today, `delay(100)` after an IMU reset in the fall sensor's self-init
   (`src/modules/sensors/ICM20948FallSensor.cpp:55`); it runs once at boot and is not copied.
2. **Long sequences are state machines across runs.** A timed pattern returns the delay to its next
   step instead of sleeping through it.
3. **Time is a `uint32_t` difference.** Write `millis() - startMs >= periodMs`, never
   `millis() >= deadlineMs`; the difference stays correct across the 49-day wrap. Upstream fixed
   wrap bugs in stock code in #11291, after the fork base.
4. **A window measured in samples is stated in both units.** When a detector polls every 100 ms,
   a 500 ms window is 5 samples and a measured duration is quantised to 100 ms steps (F6; the
   2026-10-29 run logged free falls of 421 and 523 ms). Name the poll period as a constant and
   derive the window from it.

## 4. Shared hardware resources

Every resource below has more than one user, and three of the 2026-10-29 defects (F2, F5, F7) came
from a TrekLink user and a stock user of the same resource. A change that adds, removes or changes a
user adds or edits a row here in the same pull request.

| Resource | Owner | Other users | Known conflict |
|---|---|---|---|
| LEDC channel 0 on v1 and v2 (`BUZZER_LEDC_CHANNEL`, `variants/esp32s3/treklink_v2_0/variant.h:48`) | `BuzzerManager` (`src/modules/BuzzerManager.h:20-45`) | Arduino-ESP32 2.0.17 `tone()`, which uses channel 0 and detaches the pin when a note ends (`cores/esp32/Tone.cpp:9`, `:33-47`), called by stock input beeps and melodies | F2: one stock beep silences the SOS buzzer until reboot |
| Buzzer pin, `PIN_BUZZER` (GPIO 11 on v2) | `BuzzerManager` | stock `buzz/buzz.cpp`, `BuzzerFeedbackThread`, ExternalNotification ringtones (`src/mesh/NodeDB.cpp:812-818`) | F2 |
| Vibrator pin, `PIN_VIBRATOR` (GPIO 12 on v2) | `TrekLinkSOSHelper` alarms | the trigger confirmation pulses (`src/modules/TrekLinkButtonModule.cpp:268`); ExternalNotification through the `PIN_VIBRATION` alias (`src/mesh/NodeDB.cpp:820-822`) | F3: the confirmation pulse turns the SOS vibration off |
| Notification LED, `LED_PIN` (active low on v2 and v3, `LED_STATE_ON 0`) | stock heartbeat (`src/main.cpp:249-262`) | SOS strobe, fall LED (`src/modules/FallDetectionModule.cpp:311`) | the heartbeat rewrites the LED about every second; `deactivateAlarms()` writes a literal `LOW` (`src/modules/TrekLinkSOSHelper.cpp:93-95`), which lights an active-low LED |
| ICM-20948 on v2 | stock `ICM20948Singleton` and `ICM20948Sensor` | fall adapter, which joins the singleton (`src/modules/sensors/ICM20948FallSensor.cpp:18-34`) | F5: stock puts the chip to sleep while the screen is off (`src/motion/ICM20948Sensor.cpp:50-56`); F7: the stock bus scan misreads it after a warm reset |
| QMI8658 on v4 | fall adapter (`src/modules/sensors/QMI8658FallSensor.cpp`) | none in stock | probed on I2C, wired on SPI per LilyGo (ground truth §5; not hardware-verified) |
| NVS namespace `fallcal` | `FallDetectionModule` (`src/modules/FallDetectionModule.cpp:214-224`) | none | no recalibration path once written |
| Screen banners and input routing | stock `Screen` | SOS banners (`src/modules/TrekLinkButtonModule.cpp:204`, `src/modules/TrekLinkSOSGesture.cpp:61`) | a banner owns all input while shown, and SELECT dismisses an SOS banner |

Rules:

1. **Before using a resource, find its other users.** Search for the pin macro, the channel, the
   I2C address and the driver class across `src/`, and read the stock call sites.
2. **One writer at a time.** An output shared by an alarm has one owner, and only the owner writes
   or releases it. Check ownership in code, not in a comment: `BuzzerManager::write()` is documented
   as owner-only (`src/modules/BuzzerManager.h:100`) but checks only that some owner exists
   (`:102-112`).
3. **Logic levels come from the variant.** Write `LED_STATE_ON` and `!LED_STATE_ON`, never a
   literal `HIGH` or `LOW`, for any pin whose polarity differs between variants.

## 5. Talking to hardware

1. **Check every driver status.** A call that returns a status is checked, and a failure is logged
   and returned. The fall adapter ignores the status of `getAGMT()` and always reports success
   (`src/modules/sensors/ICM20948FallSensor.cpp:87-112`), so a bus fault becomes frozen data rather
   than an error.
2. **A sensor read reports failure.** Adapters return `false` on a failed or stale read; callers
   treat repeated failures as a fault with a log line, not as a still device.
3. **Probe the bus the board uses.** The bus and address come from the variant and its schematic
   or vendor document (chapter 17 §4 rule 6).
4. **Never assume the sensor is awake.** If stock code can sleep or reconfigure a shared device
   (F5), the TrekLink user either owns that decision or detects it.

## 6. Units and numeric types

1. **Names carry units.** Use suffixes such as `Ms`, `G`, `Mps2`, `RadS`, `Mv`
   (`impactThresholdG`, `pollPeriodMs`).
2. **Convert once, at the boundary.** `FallSensorInterface` returns m/s² and rad/s
   (`src/modules/FallSensorInterface.h:12-13`); the detector works in one unit, converted with one
   named constant. Self-calibration multiplies noise in m/s² by 3.5 and stores it as a threshold in
   g (`src/modules/FallDetectionModule.cpp:202-206`), which is the error this rule prevents (F6).
3. **A clamp is logged.** A computed value forced to a floor or ceiling logs both the computed and
   the clamped value; the field unit's thresholds sat on their floors unnoticed (F6).
4. **Pick widths for the full range.** A countdown computed from an unsigned difference is signed
   or checked before it is narrowed: `uint8_t timeRemaining = (end - millis()) / 1000` wraps to 255
   after zero (`src/motion/MotionSensor.cpp:46`, F9).

## 7. Safety-path invariants

These hold for every class S change (chapter 17 §1). Each names the defect that broke it.

1. **An active alarm keeps all its outputs until it is cancelled.** No other code path may stop the
   buzzer, vibration or LED pattern of an active SOS (F2, F3).
2. **A detector settles before it judges.** A transition caused by a transient (impact, bounce,
   button release) is followed by a settling window before a stillness or quiet check starts (F4).
3. **Detection never depends on display state.** Fall detection runs with the screen on or off (F5).
4. **A destructive action needs a deliberate gesture.** Shutdown is not reachable from a short hold
   and gives feedback before it acts (F10).
5. **SOS stays visible.** While SOS is active the device shows it, and dismissing a banner does not
   hide that state.
6. **Every alarm and detector transition is logged** with its module tag and the alarm owner, so a
   test run can prove it (§10).

## 8. State machines

- `enum class` states and one `switch` in one function that alone writes the state.
- Every state has an exit, and every waiting state has a timeout.
- Entry actions run once, on the transition, not on every pass.
- The module's design spec carries the same machine as a Mermaid `stateDiagram-v2`, with the same
  state names.

## 9. Testable logic cores

Logic that does not need hardware lives in a platform-free core so `pio test -e native` can run it.
The pattern is the onboard queue:

- The core includes only standard headers and sits in its own namespace
  (`src/mqtt/TrekLinkQueueCore.h:15-22`, `namespace treklink::oq`).
- The adapter maps device types onto the core and `static_assert`s the wire values the core
  relies on (`src/mqtt/TrekLinkEventQueue.cpp:21-23`).
- Tests live in `test/test_<module>/` (`test/test_onboard_queue/test_main.cpp`).

Candidates for the same treatment are the fall state machine and its thresholds, the SOS gesture
timing and the alarm ownership rules. A fix to a core starts with a test that reproduces the
defect.

## 10. Logging

- Prefix TrekLink lines with the module tag and a colon, as the code does today:
  `TrekLinkSOS:`, `FallDetection:`, `BuzzerManager:` (`src/modules/TrekLinkSOSHelper.cpp:37`,
  `src/modules/FallDetectionModule.cpp:32`, `src/modules/BuzzerManager.h:44`).
- Log values with their units, and log every state transition and every alarm on and off.
- Test-run evidence quotes these lines, so a tag or message is renamed only together with the
  specs and runs that cite it.
- No logging inside an interrupt handler.

## 11. Interrupts, memory and persistence

- **Interrupt handlers** are `IRAM_ATTR`, set a flag or notify a thread, and return. No I2C, SPI,
  logging or allocation inside them.
- **Memory.** No heap allocation on a periodic path; use fixed buffers and `snprintf`, never
  `sprintf` into a fixed buffer.
- **Persistence.** Small TrekLink values go to NVS through `Preferences` in a namespace of at most
  15 characters, one namespace per module. Each record carries a version; a value read back is
  range-checked and replaced by the default with a log line if invalid; a write happens only when
  the value changes. A persisted calibration needs a way to redo it, which `fallcal` lacks today.
  Stock configuration in LittleFS is changed only through the stock APIs.

## 12. Comments

Comments are English and explain why. A comment that states what the code guarantees must be true
of the code; a reviewer treats a wrong comment as a defect (chapter 17 §4 rule 4).

## 13. Before a pull request

- The class is stated and chapter 17 §2 is followed.
- `trunk fmt` is clean.
- Every resource touched is in the §4 table with its other users.
- No new `delay()` or unbounded wait on the loop (§3).
- Units are in the names, and conversions happen at one boundary (§6).
- Class S: every §7 invariant still holds, and the test-run file shows it.
