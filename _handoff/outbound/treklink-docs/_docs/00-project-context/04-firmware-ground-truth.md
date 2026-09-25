# TrekLink: Firmware Ground Truth

> **What this is**: verified facts about what `treklink-firmware` *actually does on the wire*, established by direct source inspection in Session 3 (extended in Sessions 4 and 7, re-audited on 2026-09-25 against `treklink-firmware@733dd40`). Every row cites `file:line`. Nothing here is inferred from the charter, the register, or Meshtastic upstream documentation, except where a row is explicitly marked `(unverified)` or `(inferred)`.
>
> **Why it exists**: three separate specification assumptions turned out to contradict the firmware. Each cost a session to discover. This file is the reference that stops the fourth.
>
> **Status of the firmware repo**: **editable**, see **D-008**. Earlier docs describe it as frozen or read-only; that framing is superseded. Firmware *redesign* remains out of scope per charter §2.
>
> **Line numbers** were re-verified on 2026-09-25 against `dev@733dd40`. Where a citation moved, the new line is given. Where a claim was wrong, it is corrected in place and listed in §8.

---

## 1. The three assumptions that were wrong

| Assumed | Actual | Consequence |
|---|---|---|
| SOS has a distinct custom PortNum | **No custom PortNum exists.** SOS rides stock `TEXT_MESSAGE_APP` (1) and `POSITION_APP` (3), discriminated by an ASCII prefix on the text body | Gateway parser is coupled to a `printf` format string, see **D-007** |
| `eventId = deviceId:sessionId:sequenceNumber` | **Neither `sessionId` nor `sequenceNumber` exists.** `MeshPacket.id` is a rolling-counter and random hybrid, re-seeded at boot | Idempotency key redefined, see **D-006** |
| One SOS = one event | **One SOS = two independent packets** (position, then text), each with its own packet id | Backend must correlate; no single-frame dispatch, see `gateway-sync/design.md` §2.4 |

---

## 2. SOS and fall detection

**Trigger path (button and gesture).** `TrekLinkSOSHelper::triggerSOS()` (`src/modules/TrekLinkSOSHelper.cpp:35-47`) executes in order:

1. `sendPositionPacket()` (`:40`), `POSITION_APP`, `priority = MAX`, `want_ack = false`, `channel = 0` (`:118-121`)
2. `sendSOSTextMessage("SOS")` (`:43`), `TEXT_MESSAGE_APP`, same flags (`:157-160`)
3. `activateAlarms()` (`:46`), local buzzer, vibrator and LED only (`:66-78`), no transmission

Callers of `triggerSOS()`: `TrekLinkButtonModule` (v1/v2 dedicated SOS button, `TrekLinkButtonModule.cpp:227-229`) and `TrekLinkSOSGesture` (v3/v4, 3-second hold, `TrekLinkSOSGesture.cpp:56`).

**Trigger path (fall auto-SOS), corrected 2026-09-25.** `FallDetectionModule` does **not** call `triggerSOS()`. `FallDetectionModule::triggerAutoSOS()` (`FallDetectionModule.cpp:140-152`), reached after the pre-alarm timeout (`:376-378`), calls:

1. `broadcastPosition()` (`:144`), which is `PositionModule::sendOurPosition()`, so the fall episode's opening position goes out at **`BACKGROUND`** priority for a handheld role, never `MAX` (see the beacon note below)
2. `sendSOSTextMessage("SOS - FALL DETECTED")` (`:145`), `TEXT_MESSAGE_APP`, `priority = MAX`, `want_ack = false`

> ⚠️ **(2026-09-25) A fall auto-SOS has no beacon at all.** `tickBeacon()` is called only from `TrekLinkButtonModule.cpp:312` and `TrekLinkSOSGesture.cpp:116`. `FallDetectionModule` never calls it and keeps no beacon timer. A fall-triggered episode is therefore exactly two packets, one `BACKGROUND` position and one text frame, and then silence until a human acts. The backend's cadence-anomaly detector (REQ-EVT-06) cannot fire for a fall, because there is no cadence.

**Wire formats.** `snprintf` templates at `TrekLinkSOSHelper.cpp:146` and `:148`, 100-byte buffer declared at `:140`, truncated on overflow (`:151-153`):

| Form | Emitted when |
|---|---|
| `SOS - [11.123456], [107.654321]` | button or gesture trigger, valid GPS fix |
| `SOS - [No GPS]` | button or gesture trigger, no valid fix |
| `SOS - FALL DETECTED - [11.123456], [107.654321]` | fall auto-SOS, valid fix |
| `SOS - FALL DETECTED - [No GPS]` | fall auto-SOS, no fix |

Parsers must test the fall prefix **first**, since `"SOS - FALL DETECTED - …"` also satisfies `"SOS - "`.

**Beacon cadence.** `tickBeacon()` (`:176-190`, interval at `:181`): 5 s intervals for the first 60 s, then 30 s, indefinitely until cancelled or the battery dies. Button and gesture paths only (see above).

> ⚠️ **Beacons carry position only.** `tickBeacon` calls `broadcastPosition()` (`:185`), never `sendSOSTextMessage()`. The text frame that *identifies* the episode as an SOS is transmitted **exactly once**, with `want_ack = false`. Lose it to RF and every subsequent packet looks like routine position reporting. Tracked as the **Critical** row in the risk register; firmware fix candidate #1 under D-008.

> ⚠️ **(Session 4) Beacons are also *not* high-priority.** `broadcastPosition()` calls `positionModule->sendOurPosition()` (`TrekLinkSOSHelper.cpp:61`), the module's ordinary periodic-broadcast path, **not** the manually built packet `sendPositionPacket()` uses at trigger time. `PositionModule::sendOurPosition(NodeNum, bool, uint8_t)` (`PositionModule.cpp:350`) sets `p->priority` to `RELIABLE` only for `TRACKER` and `TAK_TRACKER` device roles, and **`BACKGROUND`**, the lowest tier, for every other role (`:374-377`), which is what a handheld TrekLink unit is. So only the *first* position packet sent by `triggerSOS()` (built by `sendPositionPacket()`, `priority = MAX`, `:118-119`) is high-priority. Every beacon retransmit for the rest of the episode goes out at `BACKGROUND`, the same tier as routine chatter. Under mesh congestion, an ongoing emergency's beacon trail is the *least* protected traffic on the network after its opening packet. D-007's "deferred precision upgrade" (switch to the `/2/e/` protobuf topic to recover `MeshPacket.priority`) would recover `BACKGROUND` for beacons, not `MAX`. Firmware-fix candidate, see D-008.
>
> `sendOurPosition()` also cancels any not-yet-transmitted prior position packet from the same node (`service->cancelSending(prevPacketId)`, `PositionModule.cpp:358-359`), so each new beacon supersedes the last one still queued for RF.

> ⚠️ **(2026-09-25) With no GPS fix, beacons transmit nothing.** `sendOurPosition()` returns before allocating a packet when `config.position.fixed_position` is false and the node has had no local position since boot (`PositionModule.cpp:352-355`). An SOS raised before the first fix produces the `"SOS - [No GPS]"` text and the trigger-time position (built independently by `sendPositionPacket()`), and then **no beacon traffic at all**, even though `tickBeacon()` keeps logging "Beacon retransmit".

**Episode state lives in the callers, not in the helper (2026-09-25).** `TrekLinkSOSHelper` is stateless: `tickBeacon()` takes `sosStartTime` and `lastTxTime` as arguments (`TrekLinkSOSHelper.h:81`). The "is an SOS active" flag is held in three places:

| Holder | Field / accessor | Evidence |
|---|---|---|
| `TrekLinkButtonModule` (v1/v2) | `sosActive`, `isSOSActive()` | `TrekLinkButtonModule.h:65`, `:103` |
| `TrekLinkSOSGesture` (v3/v4) | `sosActive`, `isSOSActive()` | `TrekLinkSOSGesture.h:50`, `:31` |
| `FallDetectionModule` | `currentState == SOS_TRIGGERED`, `isInSOSTriggered()` | `FallDetectionModule.h:38` |

Both button and gesture set `sosActive = true` **after** `triggerSOS()` returns (`TrekLinkButtonModule.cpp:195-196`, `TrekLinkSOSGesture.cpp:56-57`). MQTT `onSend()` runs synchronously inside `Router::send()` (`Router.cpp:369-372`), so the trigger-time position and text reach the MQTT layer while the caller's flag is still `false`.

**Cancellation.** `cancelSOS()` (`TrekLinkSOSHelper.cpp:49-53`) calls only `deactivateAlarms()`. **No packet is transmitted on cancel**; the mesh and the backend are never told the episode ended. Open question: whether an episode can therefore only ever be closed by an operator.

> **(Session 4) Two visually identical gestures, two very different effects.** Both `TrekLinkButtonModule` (v1/v2, dedicated SOS button) and `TrekLinkSOSGesture` (v3/v4, 3 s hold on the general button, enforced at compile time by `TrekLinkVariantValidation.h`) support a hold-to-cancel action. It has real effect only during `FallDetectionModule`'s 30-second `PRE_ALARM` countdown (`FallDetectionModule.h:73`, `PREALARM_TIMEOUT = 30000`), before any SOS packet has been sent, when cancelling suppresses a false-positive fall and nothing is ever transmitted. Once an SOS has fired, the identical gesture calls `cancelSOS()` (or `cancelAutoSOS()` for a fall, `TrekLinkSOSGesture.cpp:47-50`, `TrekLinkButtonModule.cpp:186-189`), which only silences local alarms. The backend has already received the broadcast; nothing retracts it. Guide-facing training material and UI copy should make this distinction explicit.

**Dead constants.** `TREKLINK_MSG_SOS 0x01` and `TREKLINK_MSG_FALL 0x02` are `#define`d in three headers (`TrekLinkSOSHelper.h:23-28`, `TrekLinkButtonModule.h:43-44`, `FallDetectionModule.h:13-14`), and `TrekLinkButtonModule.h:45` adds `TREKLINK_MSG_PRIV 0x03`. **None is referenced anywhere in the codebase.** They are not wire discriminators. Do not build a parser expecting them.

---

## 3. Packet identity

`generatePacketId()` (`src/mesh/Router.cpp:168-187`):

```c
rollingPacketId = random(...)          // re-seeded at every boot
rollingPacketId++
rollingPacketId &= ID_COUNTER_MASK     // keeps low 10 bits
id = rollingPacketId | random(...) << 10   // 22 random high bits
```

- **Not monotonic** across reboots, and not monotonic in the high bits at all.
- The 10-bit counter portion wraps every 1024 packets.
- Meshtastic's own purpose for this field is flood dedup inside the mesh, not application sequencing.
- Assigned in `Router::allocForSending()` (`:190-202`, id at `:198`), so every locally originated packet gets one.
- **(Session 4, checked and ruled out)** The protobuf field comment on `MeshPacket.id` claims IDs are "always 0 for no-ack or non-broadcast packets" (`mesh.pb.h:913-920`). `Router::allocForSending()` (`Router.cpp:198`) unconditionally calls `generatePacketId()` regardless of `want_ack` or broadcast status. Every TrekLink SOS, position and telemetry packet gets a real, non-zero `packetId`, so D-006's `sha256(nodeNum:packetId)` key is safe from this angle.

Also set there: `from = nodeDB->getNodeNum()`, `to = NODENUM_BROADCAST`, `rx_time = getValidTime(...)` (`:199-200`), **which yields `0` when the device RTC holds no valid time.** Treat `rx_time == 0` as null, never as an epoch timestamp.

**(Session 4) `nodeNum` stability, answers `specs/gateway-sync/requirements.md` §5 Q8.** `pickNewNodeNum()` (`NodeDB.cpp:1121-1142`) derives a candidate from the device's MAC address (`ourMacAddr[2..5]`, `:1127`) and only picks a different candidate if that value collides with a *different* MAC already known in the local NodeDB (`:1130-1139`). So `nodeNum` is MAC-derived and stable for a given physical unit across ordinary reboots. It is "MAC-derived, uniqueness-adjusted against locally known peers", not a pure function of the MAC, so treat it as reliable in practice rather than mathematically invariant.

---

## 4. MQTT uplink

**Topic structure** (`src/mqtt/MQTT.cpp:423-430`, `:798`). `<root>` is configurable, the rest is hardcoded:

```
<root>/2/e/<channelId>/<nodeId>      → ServiceEnvelope protobuf
<root>/2/json/<channelId>/<nodeId>   → JSON, when json_enabled
<root>/2/map/                        → MapReport, when map_reporting_enabled
```

Default `<root>` is `msh`. `ServiceEnvelope` carries `{ packet, channel_id, gateway_id }` (`mqtt.pb.h:16-25`).

**The uplink carries peer traffic, not only the node's own (2026-09-25).** `MQTT::onSend()` has two callers: `Router::send()` for packets this node originates (`Router.cpp:369-372`, guarded by `isFromUs(p)`), and the receive path for packets **heard from other nodes** (`Router.cpp:767-769`, guarded by `!isFromUs(p)`). Peer packets are filtered only by the `OK_TO_MQTT` bitfield check, which applies when the channel uses a default key and the broker is not on a private address (`MQTT.cpp:754-763`). Under D-021's custom PSK that filter does not apply, so a TrekLink uplink node forwards, and while disconnected **queues**, every decodable packet it hears on an uplink-enabled channel.

**Received packets carry no sender priority.** `MeshPacket.priority` is local transmit-queue state and is not part of the LoRa frame. `fixPriority()` (`MeshPacketQueue.cpp:41-65`) assigns a default to any packet whose priority is `UNSET`: `HIGH` for text and admin (`:53-55`), `ACK` for routing, `RELIABLE` or `DEFAULT` otherwise. So a peer's SOS text is indistinguishable by priority from ordinary chat; only its `"SOS - "` prefix identifies it. A locally sent SOS text keeps `MAX` because `fixPriority()` only fills an unset value (`:45`).

**Config surface** (`module_config.pb.h:126-160`): `enabled`, `address[64]`, `username[64]`, `password[32]`, `encryption_enabled`, `json_enabled`, `root`, `proxy_to_client_enabled`, `map_reporting_enabled`.

**JSON envelope fields** (`src/serialization/MeshPacketSerializer.cpp:410-425`):
`id`, `timestamp`, `to`, `from`, `channel`, `type`, `sender`, `payload`, plus `rssi`, `snr`, `hops_away`, `hop_start` when non-zero.

> ⚠️ **`MeshPacket.priority` is absent from the JSON envelope.** The SOS position packet is sent at `priority = MAX` but arrives on the JSON topic indistinguishable from a routine position report. The `/2/e/` protobuf topic preserves it. See D-007.

**JSON PortNum coverage** (`MeshPacketSerializer.cpp`): `TEXT_MESSAGE_APP` (`:28`), `TELEMETRY_APP` (`:56`), `NODEINFO_APP` (`:190`), `POSITION_APP` (`:208`), `WAYPOINT_APP` (`:253`), `NEIGHBORINFO_APP` (`:273`), `TRACEROUTE_APP` (`:299`), `DETECTION_SENSOR_APP` (`:353`), `PAXCOUNTER_APP` (`:363`), `REMOTE_HARDWARE_APP` (`:380`). The three TrekLink actually emits are all covered.

> ⚠️ **(2026-09-25) Any other PortNum, including `PRIVATE_APP` (256), loses its payload on the JSON topic.** An unlisted PortNum falls to `default: break;` (`MeshPacketSerializer.cpp:403-404`), so the JSON message carries the envelope fields of `:410-425` with an **empty `type` and no `payload` key**. A consumer that reads only `<root>/2/json/...` cannot read any payload the firmware puts on `PRIVATE_APP` unless the serializer gains a case for it. The `/2/e/` protobuf topic carries it intact.

> **(Session 4) `TELEMETRY_APP` is a protobuf `oneof`, not one payload shape.** `meshtastic_Telemetry` (`telemetry.pb.h:408-428`) carries a `which_variant` discriminator over `device_metrics` (`battery_level`, `voltage`, `channel_utilization`, `air_util_tx`, `uptime_seconds`), `environment_metrics` (`temperature`, `humidity`, ...), and others, but the JSON serialization (`MeshPacketSerializer.cpp:56-189`) does **not** emit an explicit variant key; a consumer must infer the variant from which fields are present. Both variants define a `voltage` field, so "has a `voltage` key" alone does not disambiguate them. `TelemetryStrategy` (gateway-sync design §2.2) should guard on `battery_level` or `uptime_seconds` presence, and the Phase 0.4 golden-fixture capture should include a device-metrics sample explicitly.

`encryption_enabled = false` makes the broker receive decrypted packets; the field's own comment notes this exists for external consumers. The alternative is reimplementing Meshtastic's AES-CTR channel crypto in NestJS.

### 4.1 (Session 7, corrected 2026-09-25) The node-side MQTT queue already exists, and discards SOS first

**A queue is already in the firmware.** It is stock Meshtastic, not TrekLink code, and it is *not* an offline store-and-forward buffer. Every property below is read from source.

| Property | Verified value | Evidence |
|---|---|---|
| Depth | **16 entries, fixed at compile time** | `MQTT.h:27`, `#define MAX_MQTT_QUEUE 16` |
| Element | `{ std::string topic; std::basic_string<uint8_t> envBytes; }`, a pre-encoded `ServiceEnvelope` | `MQTT.h:68-72` |
| Storage | `PointerQueue<QueueEntry>` over `TypedQueue`, a FreeRTOS queue of heap pointers. **RAM.** | `MQTT.h:72`, `PointerQueue.h:8` |
| Survives reboot | **No.** No flash path exists on this code path. | no `FSCom` or `SafeFile` reference anywhere in `src/mqtt/` |
| Overflow policy | **Discards the OLDEST entry** and reuses its slot | `MQTT.cpp:821-823`, `LOG_WARN("MQTT queue is full, discard oldest"); entry = mqttQueue.dequeuePtr(0);` |
| Priority awareness | **None.** Strict FIFO; `MeshPacket.priority` is never consulted on enqueue. | `MQTT.cpp:818-833` |
| Enqueue condition | only when `!proxy_to_client_enabled && !isConnectedDirectly()` | `MQTT.cpp:800`, else branch at `:818` |
| Contents | this node's own packets **and** peer packets heard on an uplink-enabled channel | `Router.cpp:369-372`, `:767-769` (see §4) |
| Drain, direct Wi-Fi mode, **corrected** | **One entry per successful reconnect, then no further draining while the link stays up.** `publishQueuedMessages()` has exactly two call sites (`MQTT.cpp:607`, `:619`). `:619` runs only on the tick where `reconnect()` has just succeeded, and pops one entry (`:708`). On every later tick `pubSub.loop()` succeeds and the connected branch (`:624-632`) returns 20 ms without touching the queue. | `MQTT.cpp:604-636`, `:699-709` |
| Drain, proxy mode | one entry per 200 ms tick (`:606-608`), but proxy mode never enqueues, because `onSend()` publishes immediately when `proxy_to_client_enabled` (`:800`) | `MQTT.cpp:606-608`, `:800` |
| Reconnect period | 30 s while the link is wanted and down (`:622`); 5 s when not wanted (`:613`) | `MQTT.cpp:613`, `:622` |

> **What the drain correction changes.** Session 7 recorded "one message per `runOnce()` at 200 ms". Control flow shows that holds only in proxy mode, where the queue is never used. In direct Wi-Fi mode, after an outage, stock firmware publishes **one** queued entry when the link returns and leaves the other fifteen in RAM until the next disconnect and reconnect cycle, or until newer traffic evicts them. `(inferred from control flow)`: the upstream test `test_sendQueued` (`test/test_mqtt/MQTT.cpp:480-499`) enqueues and drains a single entry only, so no test covers a multi-entry drain. A native test that would prove it is proposed but was not written or run in the 2026-09-25 session (PlatformIO registry unreachable). Add to the risk register as a baseline fact for RQ1.

**Consequences, in order of severity.**

1. ⚠️ **Drop-oldest + FIFO means an SOS is evicted by routine telemetry.** During an uplink outage the SOS beacon trail (5 s for the first 60 s, then 30 s, §2), ordinary telemetry, and any peer traffic the node hears fill all 16 slots. Because eviction takes the *oldest* entry, the single `"SOS - …"` text frame that identifies the episode, transmitted exactly once with `want_ack = false` (§2), is **the first thing discarded**. This is the inverse of MF-02's binding rule that all `P0` events flush before any `P2` or `P3`. (Session 7 estimated "roughly 80 seconds" to fill; that figure assumed own-node traffic only and was not measured.)
2. **Stock direct mode does not drain a backlog.** Even entries that survive eviction are published one per reconnect (see above). The unmodified-firmware baseline for RQ1 is therefore worse than "16 entries, drop oldest".
3. **RAM-only storage makes exception scenario E02-3 unsatisfiable by construction.** A node reboot with a non-empty queue loses every buffered event.
4. **The discard is silent.** Nothing is published, counted, or surfaced; the eviction exists only as a `LOG_WARN` on a serial console. RQ1's device-side loss denominator is unobservable today.

**There is a flash-persistence precedent in this repo to build on.** `MessageStore` (`src/MessageStore.cpp`, `src/MessageStore.h`) is a bounded, record-oriented, flash-backed store: `MESSAGE_HISTORY_LIMIT 20` overridable from `build_flags` (`MessageStore.h:22-24`), fixed-size records serialized by `writeMessageRecord()` (`MessageStore.cpp:244`) and read by `readMessageRecord()` (`:265`), written through `SafeFile` onto `FSCom` (`saveToFlash()`, `:287-314`; LittleFS on ESP32, `FSCommon.h:26-28`).

Caveats, added 2026-09-25:

- `MessageStore.cpp` compiles only `#if HAS_SCREEN` (`MessageStore.cpp:2`). It is a pattern to copy, not a component the queue can depend on.
- Its shutdown save in `Power::shutdown()` (`Power.cpp:788`) is itself inside `#if HAS_SCREEN` (`:809-811`). Other callers: `MenuHandler.cpp:2142`, `SystemCommandsModule.cpp:81`. `loadFromFlash()` runs from `Screen.cpp:701`.
- `Power::reboot()` (`Power.cpp:757-786`) does **not** save `MessageStore`. It only notifies `notifyReboot` observers (`:759`) before `ESP.restart()` (`:761`). Any RAM-held state that must survive a config-change reboot has to observe `notifyReboot`, not only the shutdown path.

### 4.2 (Session 7) Mesh channel encryption is on by default, with a publicly known key

`Channels::initDefaultChannel()` (`src/mesh/Channels.cpp:128-135`) sets `channelSettings.psk.bytes[0] = 1` with `defaultpskIndex = 1` (`:133-134`), and the key-expansion comment at `:238` states *"index of 1 means no change vs defaultPSK"*; higher indices increment the last byte of the same base key (`:234-238`).

So on a factory-default TrekLink unit the primary channel **is** AES-encrypted over RF, using **Meshtastic's published default PSK**. Any stock Meshtastic client in radio range decrypts the traffic, including SOS positions.

> **Two different switches, routinely conflated.** The LoRa **channel PSK** protects the RF hop between devices. `moduleConfig.mqtt.encryption_enabled` (§4) decides only whether the packet handed to the *broker* stays encrypted. They are independent, and the correct settings differ, see **D-021**.

---

## 5. Hardware variants

| Variant | PlatformIO env | Board | Config file | MQTT |
|---|---|---|---|---|
| v1 | **`treklink`** (corrected 2026-09-25, was listed as `treklink-v1`) | `treklink-esp32` (ESP32) | `variants/esp32/treklink_v1_0/platformio.ini:1` | ❌ **compiled out**, `-D MESHTASTIC_EXCLUDE_MQTT=1` (`:10`) |
| v2 | `treklink-v2` | `esp32-s3-devkitc-1` | `variants/esp32s3/treklink_v2_0/platformio.ini:1` | ✅ compiled in |
| v3 | `treklink-v3-tbeam` | extends `env:tbeam` (`ttgo-t-beam`) | `variants/esp32/treklink_v3_tbeam/platformio.ini:1-2` | ✅ compiled in |
| v4 | `treklink-v4-supreme` | extends `env:tbeam-s3-core` | `variants/esp32s3/treklink_v4_supreme/platformio.ini:1-2` | ✅ compiled in |

All four gate TrekLink code behind `-D TREKLINK_VARIANT`. v2, v3 and v4 add `TREKLINK_V2`, `TREKLINK_V3`, `TREKLINK_V4`; v1 is the "no version flag" case in `TrekLinkVariantValidation.h`. All carry Wi-Fi silicon. v1 also sets `-D MESHTASTIC_EXCLUDE_POWER_TELEMETRY=1` (`:9`).

**Per D-005, Stage A targets v2/v3/v4. v1 is out of the demo set.** v1's exclusion is a build-time flag; no configuration can enable MQTT on a v1 image.

**Flash partitions and LittleFS size (2026-09-25).** The filesystem is LittleFS on every ESP32 target (`variants/esp32/esp32-common.ini:22`).

| Variant | Partition table | LittleFS (`spiffs` data partition) | Evidence |
|---|---|---|---|
| v3 | repo `partition-table.csv`, 4 MB layout | `0x100000` = **1 MiB** at `0x300000` | `esp32-common.ini:86`; `partition-table.csv:7` |
| v2 | framework `default_8MB.csv` | `0x180000` = **1.5 MiB** at `0x670000` | `treklink_v2_0/platformio.ini:4` |
| v4 | framework `default_8MB.csv` | `0x180000` = **1.5 MiB** at `0x670000` | `tbeam-s3-core/platformio.ini:52` |

`default_8MB.csv` is not in this repository. It resolves to the Arduino-ESP32 framework's table: the repo pins `platformio/espressif32@6.12.0` (`esp32-common.ini:6-8`), whose `platform.json` requires `framework-arduinoespressif32 ~3.20017.0`, that is Arduino-ESP32 2.0.17, and that tag's `tools/partitions/default_8MB.csv` gives the row above. `(inferred)`: read from upstream sources over the network, not from a local build, because the PlatformIO registry was unreachable on 2026-09-25. The repo's own `partition-table-8MB.csv` is **not** referenced by v2 or v4. LittleFS is shared with NodeDB, preferences and any `MessageStore` file, so usable space for a queue file is less than the partition size.

**PSRAM in the build configuration (2026-09-25).** This corrects the Session 7 risk row that says v3 has no PSRAM.

| Variant | PSRAM enabled in the build | Evidence |
|---|---|---|
| v3 | **Yes**, `-DBOARD_HAS_PSRAM -mfix-esp32-psram-cache-issue`, inherited from `env:tbeam` | `variants/esp32/tbeam/platformio.ini:19-20` |
| v4 | Yes, `-DBOARD_HAS_PSRAM` | `boards/tbeam-s3-core.json:8` |
| v2 | **No flag.** `esp32-s3-devkitc-1` declares none and `treklink_v2_0/platformio.ini` adds none | `treklink_v2_0/platformio.ini:19-24` |

`(unverified)` whether the physical v3 units carry PSRAM (T-Beam v1.x modules are sold with 8 MB PSRAM) and whether the v2 module is N8R8. Either way, the build is what decides whether firmware can use PSRAM, and on v2 today it cannot.

---

## 6. Portnums

The enum (`src/mesh/generated/meshtastic/portnums.pb.h`) is **stock Meshtastic, unmodified**, zero TrekLink entries. `PRIVATE_APP = 256` (`:153`) is the reserved range for application-specific ports and is currently unused by TrekLink; it is the natural home for a custom SOS PortNum should D-008's firmware option be taken. See §4 for what the JSON topic does with it.

`protobufs/` and `meshtestic/` are git submodules (`.gitmodules`) and are **not checked out** in the working copy. Generated headers under `src/mesh/generated/meshtastic/` are committed and are the readable source of truth for wire format.

---

## 7. Maintenance

Update this file whenever firmware behaviour relevant to the platform changes, especially under D-008, where the firmware is no longer static. Every claim must cite `file:line`. If a fact cannot be cited, it does not belong here.

**Downstream consumers**: `treklink-web/specs/gateway-sync/{requirements,design,tasks}.md`, `treklink-firmware/specs/onboard-queue/{requirements,design,tasks}.md`, decisions **D-006**, **D-007**, **D-008**, **D-018**, **D-019**, **D-021**.

**2026-09-25 re-audit** against `treklink-firmware@733dd40`: every citation re-read; corrections and additions are listed in §8.

**Session 7 additions**: the node-side `mqttQueue`, 16 entries, RAM-only, FIFO, drop-oldest (§4.1); the `MessageStore` flash precedent (§4.1); default-channel PSK is the published Meshtastic default (§4.2). Found by inspection of `src/mqtt/MQTT.{h,cpp}`, `src/mesh/PointerQueue.h`, `src/MessageStore.{h,cpp}` and `src/mesh/Channels.cpp`.

**Session 4 additions**: SOS-beacon priority downgrade after the first packet (§2), `nodeNum` MAC derivation (§3), `TELEMETRY_APP`'s `oneof` ambiguity (§4), and the two-tier cancel-gesture distinction (§2).

---

## 8. Change log of the 2026-09-25 re-audit

| # | Section | Change | Kind |
|---|---|---|---|
| 1 | §2 | `triggerSOS()` range `:41-48` is now `:35-47`; `cancelSOS()` `:57-61` is now `:49-53` | moved |
| 2 | §2 | `PositionModule.cpp` priority lines `:377-380` are now `:374-377`; `cancelSending` `:359-360` is now `:358-359` | moved |
| 3 | §2 | `FallDetectionModule` does not call `triggerSOS()`; its opening position is `BACKGROUND` | corrected |
| 4 | §2 | A fall auto-SOS never beacons | new |
| 5 | §2 | With no GPS fix, beacons transmit nothing | new |
| 6 | §2 | SOS episode state is held by three callers, not the helper; flag is set after the trigger packets reach MQTT | new |
| 7 | §2 | `TREKLINK_MSG_PRIV 0x03` is a fourth dead constant | new |
| 8 | §3 | `pickNewNodeNum()` starts at `:1121`; `mesh.pb.h` comment ends at `:920` | moved |
| 9 | §4 | MQTT uplink forwards and queues peer packets, not only own events | new |
| 10 | §4 | Received packets carry no sender priority; a peer SOS arrives as `HIGH` text | new |
| 11 | §4 | `PRIVATE_APP` loses its payload on the JSON topic | new |
| 12 | §4 | Telemetry JSON case spans `:56-189`, not `:56-120` | moved |
| 13 | §4.1 | Direct-mode drain is one entry per reconnect, not one per 200 ms | corrected |
| 14 | §4.1 | "~80 seconds to fill" was an estimate, now marked as such | corrected |
| 15 | §4.1 | `MessageStore` compiles only with a screen; `Power::reboot()` saves nothing and only notifies `notifyReboot` | new |
| 16 | §5 | v1 env is `treklink`, not `treklink-v1` | corrected |
| 17 | §5 | LittleFS partition sizes per variant | new |
| 18 | §5 | v3 build enables PSRAM; v2 build does not | new, contradicts a Session 7 risk row |
