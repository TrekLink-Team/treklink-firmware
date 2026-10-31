---
id: architecture-design
type: design
status: draft
owner: KhoaDD
summary: The TrekLink overlay on stock Meshtastic v2.7.26, its directory layout, start-up path, capability flags, override catalogue, sync procedure and the deliberate behaviour changes of the move.
verified: {commit: 54e0d8d, date: 2026-10-31, scope: "every upstream file:line in §2 to §5 at v2.7.26.54e0d8d"}
---

# TrekLink firmware architecture: design

Requirements: [requirements.md](requirements.md). Line citations are to upstream
`v2.7.26.54e0d8d` unless a TrekLink path is named.

## 1. Principles

1. **Stock code is a dependency, not a draft.** The upstream tree is used as released. TrekLink
   adds files and changes behaviour from the outside (REQ-ARC-01, REQ-ARC-03).
2. **One direction of knowledge.** TrekLink code may call stock code; stock code never names
   TrekLink. Stock environments compile no TrekLink file (REQ-ARC-02).
3. **Hardware facts live with the variant.** A variant directory says what the board has; shared
   TrekLink code asks for capabilities (REQ-ARC-04).
4. **Every override is visible.** If TrekLink changes what a stock function does, the catalogue in
   §4.2 says where and how, and the overlay check watches the stock code it depends on.

## 2. Layout

### 2.1 Tree

```text
treklink-firmware/
├── src/ … variants/<stock>/ …        upstream v2.7.26, unchanged
├── treklink/
│   ├── src/                          compiled only by TrekLink envs
│   │   ├── TrekLink.h, TrekLink.cpp  start-up entry, identity log, module creation
│   │   ├── Capabilities.h            capability defaults and compile-time checks (§3)
│   │   ├── VariantDefaults.cpp       variantDefaultConfig() and first-boot defaults (§4.2 O-01 to O-04)
│   │   ├── alarm/                    BuzzerManager (buzzer LEDC ownership)
│   │   ├── sos/                      SOSHelper, SOSButtonModule, SOSGestureModule
│   │   ├── fall/                     FallDetectionModule, FallSensorInterface, sensors/
│   │   ├── input/                    InputCancelObserver (any button cancels a fall pre-alarm)
│   │   └── queue/                    QueueCore, QueueConfig, EventQueue, MqttOverride (§4.2 O-08)
│   └── tools/
│       ├── tk.py                     menu tool for people (REQ-ARC-09)
│       └── check_overlay.py          overlay check (REQ-ARC-01, REQ-ARC-03, REQ-ARC-04)
├── variants/esp32s3/treklink_v2/     v2 custom board: variant.h, variant.cpp, platformio.ini, pins_arduino.h
├── variants/esp32/treklink_v3/       v3 T-Beam v1.2 overlay on stock tbeam
├── variants/esp32s3/treklink_v4/     v4 T-Beam Supreme overlay on stock tbeam-s3-core
├── test/treklink_queue/              native test of the queue core (moved from test/test_onboard_queue)
├── docs/  specs/                     TrekLink documentation and specifications
└── AGENTS.md  CLAUDE.md  README.md   TrekLink-owned (§2.3)
```

Upstream discovers every `variants/*/*/platformio.ini` by itself (`platformio.ini` `extra_configs`),
so a TrekLink variant directory needs no stock edit.

### 2.2 How a TrekLink env compiles the overlay

Each TrekLink `platformio.ini` extends the stock base and adds the overlay sources, the pattern
upstream uses for t-deck-pro (`variants/esp32s3/t-deck-pro/platformio.ini:19-21`):

```ini
build_src_filter =
  ${esp32s3_base.build_src_filter}
  +<../treklink/src/>
  +<../variants/esp32s3/treklink_v2>
build_flags =
  ${esp32s3_base.build_flags}
  -I treklink/src
  -I variants/esp32s3/treklink_v2
  -D TREKLINK_VARIANT
```

### 2.3 TrekLink-owned files at stock paths

| Path | Why it cannot live elsewhere | Sync rule |
|---|---|---|
| `AGENTS.md` | The canonical TrekLink agent file must sit at the root of all five repositories (AGENTS.md header). Upstream's pointer file is replaced; its content stays reachable at upstream `.github/copilot-instructions.md`, which is untouched. | keep ours |
| `README.md` | The repository landing page names TrekLink and links upstream | keep ours |

Nothing else at a stock path may differ. `.gitignore` is not edited; local tool output such as
`.gitnexus/` goes to `.git/info/exclude`.

## 3. Start-up and capabilities

### 3.1 Start-up path

| Step | Stock call site | TrekLink action |
|---|---|---|
| Power-up | `main.cpp:330` calls `earlyInitVariant()` (weak, `main.cpp:271-272`) | v2 only: `variants/esp32s3/treklink_v2/variant.cpp` turns the status LED on and the GPS power on, and parks the LoRa CS line high. The power latch itself is stock `PIN_POWER_EN` (§4.2 O-05). |
| Factory defaults | `NodeDB.cpp:800-801` calls `variantDefaultConfig()` and `variantDefaultModuleConfig()` (weak, `NodeDB.cpp:86-90`) | `treklink/src/VariantDefaults.cpp` sets the time zone and MQTT defaults (§4.2 O-01, O-03) |
| Modules, input, screen | `setupModules()` `main.cpp:934`, `inputBroker->Init()` `:955-956`, `screen->setup()` | nothing: stock runs as released |
| Radio up | `initLoRa()` `main.cpp:989` | nothing |
| Late init | `main.cpp:991` calls `lateInitVariant()` (weak, `main.cpp:268-269`) | each TrekLink `variant.cpp` defines `lateInitVariant()` and calls `treklink::start()`, which logs the identity (REQ-ARC-08), applies first-boot defaults (§4.2 O-02, O-04) and creates the TrekLink modules the capabilities enable |

`lateInitVariant()` is the only hook that runs after modules, input and the screen exist, so the
TrekLink modules are created there. They are ordinary `MeshModule` and `OSThread` objects; stock
module creation in `Modules.cpp` is not touched. No stock T-Beam variant defines `lateInitVariant()`
for these boards (`tbeam_displayshield/variant.cpp:29` is compiled only with `HAS_CST226SE`).

### 3.2 Capabilities

Defined in each `variant.h`; `treklink/src/Capabilities.h` supplies defaults and stops the build on a
contradiction (REQ-ARC-04.3). Shared code tests these, never `TREKLINK_Vn`.

| Capability | Meaning | v2 | v3 | v4 |
|---|---|---|---|---|
| `TREKLINK_VARIANT_NAME` | boot log name | "TrekLink v2 (custom S3)" | "TrekLink v3 (T-Beam v1.2)" | "TrekLink v4 (T-Beam Supreme)" |
| `TREKLINK_SOS_BUTTON_PIN` | dedicated SOS button; absent means the user-button trigger | 4 | absent | absent |
| `PIN_BUZZER` | SOS and alarm buzzer (stock name) | 11 | absent | absent |
| `PIN_VIBRATOR` | vibration motor | 12 | absent | absent |
| `TREKLINK_SOS_LED_PIN` | LED strobed during SOS | 2 | 4 (stock `LED_PIN`) | absent |
| `TREKLINK_FALL_SENSOR` | IMU adapter for fall detection: `TREKLINK_FALL_ICM20948`, `TREKLINK_FALL_QMI8658` or `TREKLINK_FALL_NONE` | ICM20948 | NONE | QMI8658 |
| `TREKLINK_ONBOARD_QUEUE` | durable MQTT queue (`specs/onboard-queue`) | 1 | 1 | 1 |

## 4. Overrides

### 4.1 Mechanisms, in order of preference

1. **Upstream hook or define.** A weak function (`earlyInitVariant`, `lateInitVariant`,
   `variantDefaultConfig`, `variantDefaultModuleConfig`, `variant_shutdown`) or a define stock code
   already reads (`PIN_POWER_EN`, `REGULATORY_LORA_REGIONCODE`, `TCXO_OPTIONAL`,
   `INPUTDRIVER_TWO_WAY_ROCKER_*`).
2. **Configuration default.** A value written into `config` or `moduleConfig` at factory reset or
   first boot, which the stock code then honours. The owner can still change it.
3. **Object replacement or observation at start-up.** From `treklink::start()`: observe the
   `InputBroker`, set a stock object's state through its public API.
4. **Linker wrap.** `-Wl,--wrap=<mangled symbol>` routes calls to a stock function that come from
   other translation units through a TrekLink function, which may call the original as
   `__real_<symbol>`. Calls inside the stock function's own file are not redirected, so a wrap is used
   only where every caller that matters is in another file. Envs that wrap build without LTO
   (`build_unflags = -flto`), as `treklink-v2` already does, because LTO can resolve a call before the
   wrap applies (inferred, high; checked by the overlay check reading the linked map).
5. **No stock file copies.** Copying a stock file into the overlay to change it hides upstream fixes;
   it is not used.

### 4.2 Catalogue

| ID | Behaviour | Mechanism | Stock dependency | Replaces the old stock edit in |
|---|---|---|---|---|
| O-01 | Time zone ICT-7 by default | 2: `config.device.tzdef` in `variantDefaultConfig()` | `main.cpp:866-875` uses `tzdef` when set | `main.cpp` `DEFAULT_TIMEZONE` |
| O-02 | TrekLink canned messages by default | 3: at first boot, if the stored list equals stock `"Hi|Bye|Yes|No|Ok"`, `cannedMessageModule->handleSetCannedMessageModuleMessages()` (public, `CannedMessageModule.h:86`) | stock default string in `installDefaultCannedMessageModuleConfig()` | `CannedMessageModule.cpp` |
| O-03 | Region VN 433, which is the regulatory MY_433 band | 1: `REGULATORY_LORA_REGIONCODE` in each `variant.h` | `RadioInterface.cpp:496-497` | the `VN_433` menu entries in `MenuHandler.cpp`, `MenuAction.h`, `MenuApplet.cpp`, dropped |
| O-04 | Owner name "TrekLink xxxx" | 3: at start, if the long name is stock "Meshtastic xxxx", rename to "TrekLink xxxx" and save | `NodeDB.cpp:1128-1134` default owner | `NodeDB.cpp`; `main.cpp` device name follows from the short name |
| O-05 | v2 power latch held at boot, released at shutdown | 1: `PIN_POWER_EN 9` in the v2 `variant.h` | `main.cpp:332-334` drives it high after `earlyInitVariant()`; `sleep.cpp:246-248` drives it low and floats it on shutdown | `sleep.cpp` `POWER_LATCH_PIN` |
| O-06 | v2 UP and DOWN navigation buttons | 1: `INPUTDRIVER_ENCODER_TYPE 2`, `INPUTDRIVER_TWO_WAY_ROCKER`, `_LEFT 7`, `_RIGHT 8`; MENU stays the stock user button on `BUTTON_PIN 0` | `InputBroker.cpp:373-378`, `UpDownInterruptImpl1.cpp:11-16` | `InputBroker.cpp` and `TrekLinkButtonInput` |
| O-07 | Any input cancels a fall pre-alarm | 3: `InputCancelObserver` observes `inputBroker` (an `Observable<const InputEvent *>`, `InputBroker.h:67`) | stock input events | `TrekLinkButtonInput::runOnce()` |
| O-08 | Durable, priority-ordered MQTT queue replaces the stock 16-entry FIFO | 4: wrap `MQTT::onSend`, called from `Router.cpp:382` and `:826`. While the link is up and the TrekLink queue is empty, `__real_` runs unchanged; otherwise the packet goes to the TrekLink queue, drained by a TrekLink thread through the public `MQTT::publish()` (`MQTT.h:51-53`) | `MQTT::onSend` envelope rules (`MQTT.cpp`, encryption choice, uplink and `OK_TO_MQTT` checks), mirrored in `MqttOverride.cpp` and watched by the overlay check | `MQTT.cpp`, `MQTT.h` |
| O-09 | Queue health report on the JSON topic | 3: TrekLink builds the JSON object itself and publishes it through `MQTT::publish()` | none | `MeshPacketSerializer.cpp` `PRIVATE_APP` case |
| O-10 | v2 SX1268 TCXO at 1.8 V | 1: `SX126X_DIO3_TCXO_VOLTAGE 1.8` and `TCXO_OPTIONAL` | `RadioInterface.cpp:379-391` | `RadioInterface.cpp` |
| O-11 | Fall detection shares the v2 ICM-20948 with the stock motion driver | decided in the fall-detection fix (F5, F7); until then the adapter starts its own driver only when stock created no `accelerometerThread` (`main.cpp:770-773`) and otherwise joins `ICM20948Singleton` | `ICM20948Sensor.cpp` singleton | `ICM20948Sensor.cpp/.h` `isInitialized()` |

Dropped with no replacement: the "SSD1306 (Fallback)" OLED probe in `ScanI2CTwoWire.cpp` (the v2
OLED is found as SH1106 at 0x3c, `docs/test-runs/logs/2026-10-29-v2/cold1.log:22`), the "Vietnam"
time-zone menu entries (O-01 sets the zone; the owner can type any zone), and the "TrekLink" boot
title in `UIRenderer.cpp` (a TrekLink screen is later UI work).

### 4.3 The overlay check

`treklink/tools/check_overlay.py`, run in CI and before every sync:

1. Every stock path is byte-identical to the upstream tag recorded in §5, except §2.3.
2. No `TREKLINK_V2`, `TREKLINK_V3` or `TREKLINK_V4` test outside `variants/`.
3. For each catalogue row with a stock dependency, a stored hash of the dependent stock function or
   line range; a sync lists every row whose hash changed, for review before the rebase lands.

## 5. Upstream base and sync

| Field | Value |
|---|---|
| Upstream tag | `v2.7.26.54e0d8d` (stable, 2026-06-24) |
| Platform | `platformio/espressif32@6.13.0` (`variants/esp32/esp32-common.ini:8`) |
| Previous base | `538a5f0df` (2.7.19), snapshot fork `91134a2`; the old `dev` is kept as tag `archive/pre-overlay` |

Sync: fetch the new stable tag, branch `chore/upstream-sync-<version>`, `git rebase --onto <new tag>
<old tag>`, run the overlay check, the build matrix and the chapter 17 §3 regression set, then update
this table. 2.8.x stays out while it is Alpha (chapter 17 §6 rule 5).

## 6. Deliberate behaviour changes of the move

| Change | Why |
|---|---|
| v2 MENU button now follows the stock user button: short press next frame, 500 ms long press select, shutdown only on release after a 3.9 s hold with the lead-up tone (`ButtonThread.h:24`, `ButtonThread.cpp:276-279`) | Replaces the 300 ms instant shutdown of finding F10 (`TrekLinkButtonInput.cpp:31`) with stock handling |
| No `VN_433` menu entry; the region is locked to the MY_433 band by `REGULATORY_LORA_REGIONCODE` | The entry mapped to the same enum value and only added a label |
| No TrekLink boot title | Stock UI file edit; returns with TrekLink UI work |
| v1 is not ported | Deprecated since D-076; it compiles MQTT out and has no unit in service. Its specs stay as the historical base requirement set |
| Upstream fixes from 2.7.20 to 2.7.26 arrive, including the compass calibration screen exit (F9) and saved compass calibration (F8, upstream #10166) | Taken with the base, not fixed in the fork (chapter 17 §6 rule 3) |

## 7. Testing the move

1. Overlay check passes.
2. Builds: `treklink-v2`, `treklink-v3`, `treklink-v4`, and stock `tbeam` and `tbeam-s3-core` as
   the regression proof that stock envs are unaffected.
3. `pio test -e native -f treklink_queue`.
4. Hardware, on every variant with a unit: boot log identity line, IMU detection, SOS trigger and
   cancel, MENU long press, beacon packets, compared against the 2026-10-29 run. Runs that inject
   button presses through the diagnostic `TREKLINK_TEST_HOOKS` image say so per behaviour.
