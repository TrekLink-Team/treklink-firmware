# Phase A audit: onboard-queue firmware session, 2026-09-25

Audited commit: `treklink-firmware@733dd40` (`dev`, 2026-09-22). Docs root: `treklink-docs@089374b` from `firmware-pack.zip`. Branch for this work: `feat/onboard-queue`.

Epistemic tags: **KNOWN** read from source in this session; **INFERRED** from control flow or upstream sources, confidence stated; **UNKNOWN** not established.

---

## A1. Ground-truth re-verification

Corrected copy: `_handoff/outbound/treklink-docs/_docs/00-project-context/04-firmware-ground-truth.md` (its §8 is the change log). Register rows proposed in `_handoff/outbound/treklink-docs/_docs/00-project-context/03-decisions-and-risk-register.md` (five appended rows, no existing row altered).

| # | Claim (section) | Verdict | Citation at `733dd40` |
|---|---|---|---|
| 1 | No custom PortNum; SOS on `TEXT_MESSAGE_APP` and `POSITION_APP` (§1, §6) | still true | `portnums.pb.h:153` (`PRIVATE_APP = 256`, unused) |
| 2 | No `sessionId` or `sequenceNumber` anywhere (§1) | still true | grep over `src/modules/TrekLink*`, `Fall*`, `src/mesh/` returns nothing |
| 3 | One SOS = two packets, position then text (§1) | still true | `TrekLinkSOSHelper.cpp:40`, `:43`; fall path `FallDetectionModule.cpp:144-145` |
| 4 | `triggerSOS()` at `TrekLinkSOSHelper.cpp:41-48` (§2) | moved | `:35-47` |
| 5 | Position flags `MAX`, no ack, channel 0 at `:118-121` (§2) | still true | `:118-121` |
| 6 | Text flags at `:157-160` (§2) | still true | `:157-160` |
| 7 | `activateAlarms()` is local only (§2) | still true | `:66-78` |
| 8 | `FallDetectionModule` is a `triggerSOS()` caller (§2) | **false** | it calls `broadcastPosition()` then `sendSOSTextMessage()` directly, `FallDetectionModule.cpp:140-152`; opening position is `BACKGROUND` |
| 9 | Fall prefix `"SOS - FALL DETECTED"` at `FallDetectionModule.cpp:145` (§2) | still true | `:145` |
| 10 | `snprintf` templates `:146`, `:148`, 100-byte buffer (§2) | still true | `:146`, `:148`, buffer `:140` |
| 11 | Beacon 5 s then 30 s, `:181` (§2) | still true | `:181`, function `:176-190` |
| 12 | Beacons carry position only (§2) | still true | `:185` |
| 13 | Beacons go out at `BACKGROUND`, `PositionModule.cpp:377-380` (§2) | moved | `:374-377` |
| 14 | `cancelSending(prevPacketId)` at `PositionModule.cpp:359-360` (§2) | moved | `:358-359` |
| 15 | `cancelSOS()` at `:57-61`, sends nothing (§2) | moved | `:49-53`, still sends nothing |
| 16 | `PREALARM_TIMEOUT = 30000` (§2) | still true | `FallDetectionModule.h:73` |
| 17 | Dead constants in three headers (§2) | still true | `TrekLinkSOSHelper.h:23-28`, `TrekLinkButtonModule.h:43-44`, `FallDetectionModule.h:13-14`; plus `TREKLINK_MSG_PRIV` at `TrekLinkButtonModule.h:45` |
| 18 | `generatePacketId()` at `Router.cpp:168-187` (§3) | still true | `:168-187` |
| 19 | id assigned in `allocForSending()` `:198`; `rx_time` may be 0 (§3) | still true | `:198`, `:199-200` |
| 20 | `MeshPacket.id` comment at `mesh.pb.h:913-921` (§3) | still true | `:913-920` |
| 21 | `pickNewNodeNum()` at `NodeDB.cpp:1123-1142`, MAC at `:1127` (§3) | moved | `:1121-1142`, `:1127` |
| 22 | Topic shapes `MQTT.cpp:423-430`, `:798` (§4) | still true | same |
| 23 | `ServiceEnvelope` `mqtt.pb.h:16-25` (§4) | still true | same |
| 24 | MQTT config `module_config.pb.h:126-160` (§4) | still true | same |
| 25 | JSON envelope fields `MeshPacketSerializer.cpp:410-424`, no priority (§4) | still true | `:410-425` |
| 26 | JSON PortNum case lines (§4) | still true | `:28`, `:56`, `:190`, `:208`, `:253`, `:273`, `:299`, `:353`, `:363`, `:380` |
| 27 | Telemetry `oneof`, `telemetry.pb.h:408-428`, JSON `:56-120` (§4) | moved | header same; JSON case spans `:56-189` |
| 28 | `MAX_MQTT_QUEUE 16` (§4.1) | still true | `MQTT.h:27` |
| 29 | `QueueEntry` `MQTT.h:68-72`, `PointerQueue.h:8` (§4.1) | still true | same |
| 30 | RAM only, no flash path (§4.1) | still true | no `FSCom` or `SafeFile` in `src/mqtt/` |
| 31 | Drop-oldest at `MQTT.cpp:821-823` (§4.1) | still true | same |
| 32 | No priority awareness, `:818-831` (§4.1) | still true | `:818-833` |
| 33 | Drain one per `runOnce()` at 200 ms, `:699-709`, `:607`, `:619` (§4.1) | **false for direct Wi-Fi** | `publishQueuedMessages()` is called only at `:607` (proxy, where nothing is ever enqueued) and `:619` (the single tick that re-establishes the link). The connected branch `:624-632` never drains. One entry per reconnect. **INFERRED, high confidence**; no test covers a multi-entry drain (`test/test_mqtt/MQTT.cpp:480-499` uses one entry) |
| 34 | Enqueue only when not proxy and not connected, `:800`, `:818` (§4.1) | still true | same |
| 35 | Reconnect 30 s / 5 s at `:621`, `:613` (§4.1) | moved | `:622`, `:613` |
| 36 | "~80 s to fill 16 slots" (§4.1) | **unmeasured estimate** | now labelled as such; peer traffic (row 41) makes it faster |
| 37 | `MessageStore` precedent: `MessageStore.h:22-24`, `.cpp:244`, `SafeFile`, `FSCommon.h:28` (§4.1) | still true, with caveats | `MessageStore.cpp` builds only `#if HAS_SCREEN` (`:2`); `SafeFile` rewrites whole files and, with `fullAtomic = false` as `MessageStore` uses it (`:295`), deletes the old file before writing (`SafeFile.cpp:11-12`), so it is neither append nor atomic |
| 38 | Shutdown hook `Power.cpp:810`; `MenuHandler.cpp:2142`; `Screen.cpp:701` (§4.1) | still true, with caveat | `:810` sits inside `#if HAS_SCREEN` (`:809-811`) in `Power::shutdown()` (`:788`). `Power::reboot()` (`:757`) saves nothing and only notifies `notifyReboot` (`:759`) |
| 39 | Default PSK: `Channels.cpp:128-134`, `:234-238` (§4.2) | still true | `:128-135`, `:234-238` |
| 40 | v1 env is `treklink-v1` (§5) | **false** | env is `treklink` (`variants/esp32/treklink_v1_0/platformio.ini:1`) |
| 41 | (new) MQTT uplink also forwards and queues heard **peer** packets | new, KNOWN | `Router.cpp:767-769`; filter `MQTT.cpp:754-763` only bites on default keys |
| 42 | (new) received packets carry no sender priority; a peer SOS text is `HIGH` | new, KNOWN | `fixPriority()` `MeshPacketQueue.cpp:41-65` |
| 43 | (new) `PRIVATE_APP` has no JSON case; payload dropped on `/2/json/` | new, KNOWN | `MeshPacketSerializer.cpp:403-404` |
| 44 | (new) fall auto-SOS never beacons | new, KNOWN | `tickBeacon` callers only `TrekLinkButtonModule.cpp:312`, `TrekLinkSOSGesture.cpp:116` |
| 45 | (new) no GPS fix means no beacons | new, KNOWN | `PositionModule.cpp:352-355` |
| 46 | (new) episode flag lives in three callers and is set after the trigger packets hit MQTT | new, KNOWN | `TrekLinkButtonModule.cpp:195-196`, `TrekLinkSOSGesture.cpp:56-57`, `Router.cpp:369-372` |
| 47 | (new) LittleFS: v3 1 MiB, v2/v4 1.5 MiB | new, INFERRED high | `partition-table.csv:7`; framework `default_8MB.csv` of Arduino-ESP32 2.0.17, read from upstream, not from a local build |
| 48 | (new) v3 build enables PSRAM, v2 build does not | new, KNOWN for the build; UNKNOWN for the physical units | `variants/esp32/tbeam/platformio.ini:19-20`; `treklink_v2_0/platformio.ini:19-24` |

---

## A2. Spec review: `specs/onboard-queue/`

### Approval status the spec carries

- None of `requirements.md`, `design.md`, `tasks.md` has a status or approval field.
- The suite landed on `dev` in `c715797` (2026-09-22, Do Dang Khoa, the story owner).
- `US-102` does not appear in the backlog snapshot (`_docs/03-backlog/02-user-stories.md`).
- `07-clarification-answers.md` records #26 "`onboard-queue` Phase 0 hardware measurement: **Hold**" and #27 "Phase 0 defaults proposed instead of blocking: **Yes**".

Reading: merged, owner-authored, **no recorded approval gate** per `02-spec-driven-development-workflow.md` §4 ("Granular checklist approved by lead"). All 58 tasks open.

### Defects

Severity is impact on the spec's own acceptance criteria if built as written.

| # | Sev | Where | Defect | Proposed fix |
|---|---|---|---|---|
| S1 | **High** | REQ-STA-03; design §0 seam table, §2.3; tasks 5.5 | "Preserve the existing 200 ms drain" rests on a false premise (audit row 33). Stock direct mode drains one entry per reconnect. Built as specified, the queue never flushes while the link is up, and AC-02 fails. The design lists two stock call sites; a third is required: the connected branch of `MQTT::runOnce()` (`MQTT.cpp:624-632`). | Add a drain step on the connected branch behind `TREKLINK_ONBOARD_QUEUE`, one entry per tick, tick interval a D-015 parameter. Update REQ-STA-03, design §0/§2.3/§5, task 5.3. |
| S2 | **High** | requirements "Out of scope"; design §7; D-018 "What Stage B does not do" | "This queue holds only this node's own events" is false (audit row 41). The MQTT path queues every decodable peer packet the uplink node hears. Capacity, classification and the Stage B vs Stage C boundary are all affected. | Either (a) accept peers and say so (Stage B then partially buffers peers within one hop of the uplink node), or (b) queue only `isFromUs` packets and leave peer packets on the stock path. Needs a decision (Q-A3). |
| S3 | **High** | REQ-EVT-03; design §2.1 | `TEXT_MESSAGE_APP && priority == MAX` identifies only locally sent SOS text. A peer SOS arrives as `HIGH` (audit row 42) and would be classed **P3**, the first tier shed. | Classify P0 by payload prefix `"SOS - "`, the same contract `gateway-sync` REQ-EVT-04 uses, with `MAX` as an additional local signal. |
| S4 | **High** | REQ-EVT-04; design §2.1; task 2.1 | `isEpisodeActive()` "over the beacon state `tickBeacon()` already maintains" has nothing to read: the helper is stateless (audit row 46). State is split across three modules, and it flips to true only after the trigger position is already enqueued, so the first SOS position would be **P2**. | Add episode state to the helper, set inside `triggerSOS()` and `sendSOSTextMessage()` before sending, cleared by `cancelSOS()` and the fall cancel. Also class `POSITION_APP` with `priority == MAX` as P1. Still additive per D-019. |
| S5 | Medium | REQ-EVT-12; design §2.4; `gateway-sync` REQ-EVT-12/13 | The health report on `PRIVATE_APP` has no payload on the JSON topic (audit row 43), and Stage A ingests JSON only. No spec defines the payload schema or encoding. | Decide transport and schema (Q-A4). |
| S6 | Medium | REQ-EVT-12; NFR "Mesh impact" | "Publish ... as a packet" is ambiguous. Sent through the mesh it costs airtime on every node and contradicts the zero-airtime NFR. | Publish MQTT-only by building the `ServiceEnvelope` locally, never via `sendToMesh()`. |
| S7 | Medium | REQ-EVT-07; task 4.3; requirements table "Survives reboot: Yes" | The only hook named is `Power.cpp:810`, which is `HAS_SCREEN`-only and runs on shutdown, not reboot. Config and admin reboots go through `Power::reboot()` and `notifyReboot`. Brown-out and battery death have no hook, so non-P0 RAM entries are lost on unclean power loss; "survives reboot: yes" overclaims. | Persist on `notifyReboot` and on shutdown, unconditionally. State the durability model: flash tier and P0 survive any power loss; RAM tier survives orderly restarts only. |
| S8 | Medium | design §1.2, §2 "Substrate" | "Append-only log through `SafeFile`, which gives write-then-rename atomicity" is wrong on both counts (audit row 37). `SafeFile` cannot append, and in non-atomic mode deletes the file first. | Append with `FSCom.open(path, "a")` plus per-record CRC (already in the design for torn tails). Use `SafeFile(path, true)` only for compaction. |
| S9 | Medium | design §4 table | PSRAM column contradicts the build (audit row 48). Flash-tier bounds were unverified. | Replace with measured partition sizes (audit row 47). Task 0.1 can close statically; on-device free-space measurement stays on hold per clarification #26. |
| S10 | Medium | REQ-STA-04 with REQ-ERR-01 (open Q4) | Unbounded episode exemption plus never-shed P0 can make the whole queue unsheddable; then any further P0, including a second person's SOS, is refused. | Bounded exemption: keep the first K and the most recent M positions of an open episode; the middle of the trail is sheddable. K and M are D-015 parameters. |
| S11 | Medium | REQ-EVT-01 | "Publish immediately when connected" lets live P2/P3 traffic overtake a still-draining backlog that holds P0, which breaks the ordering NFR measured on reconnect. | WHILE the queue is non-empty, route new packets through the queue. |
| S12 | Low | REQ-EVT-10; design §2 `commit()` | `PubSubClient` publishes at QoS 0; `publish()` returning true means bytes written to the socket, not broker receipt. Proxy mode always returns true. | Accept at-most-once after socket write and state it, or commit only after the next successful `pubSub.loop()`. QoS change is out (D-019 §3 spirit). |
| S13 | Low | REQ-UBI-02; AC-08 | "Byte-identically" is untestable once stock bodies move into functions. | "Behaviourally identical": the upstream `test/test_mqtt` suite passes unchanged with the flag off. |
| S14 | Low | REQ-ERR-01 text vs Figure 3 | The text covers "every queued entry is P0"; the figure also covers "non-P0 exists but all exempt". | Align the text with the figure. |
| S15 | Low | whole suite | Written before 2026-09-22; carries em dashes. | Fix when the files are next edited, per `14-prose-and-wording.md` §6. |

### Open questions carried by the spec, with proposed defaults (clarification #27)

| Spec Q | Proposed default |
|---|---|
| Q1 health-report interval | 300 s; suppressed while the queue is empty and the link is up; one immediate report on reconnect after a non-empty outage |
| Q2 discriminator retransmit N | every 4th beacon; Phase 9, not blocking |
| Q3 log vs slots | log, as the design says, with the S8 correction |
| Q4 episode exemption | bounded, K = 3 first, M = 5 latest (S10) |

### Consumer contract (`gateway-sync`) cross-check

| Item | Status |
|---|---|
| Wire format unchanged, `/2/e/` and `/2/json/` (REQ-UBI-03 vs gateway-sync §1) | consistent |
| Device tier not on the wire (gateway-sync REQ-UBI-08) | consistent, the design keeps tier internal |
| `eventId = sha256(nodeNum:packetId)`; republish after crash is a backend no-op (REQ-ERR-04 vs gateway-sync REQ-ERR-01) | consistent; `envBytes` are stored verbatim so `packetId` survives |
| Health report (REQ-EVT-12 both sides) | **broken**, S5 |
| gateway-sync REQ-EVT-06 cadence detector | does not cover fall SOS or no-fix SOS (audit rows 44, 45); flag to the web session |

---

## A3. Build confirmation

Target environments (ground truth §5, D-005 demo set): `treklink-v2`, `treklink-v3-tbeam`, `treklink-v4-supreme`. `treklink` (v1) excluded, MQTT compiled out. Native tests: `native`.

| Command | Result |
|---|---|
| `pip install platformio` | PlatformIO Core 6.2.0 installed (PyPI reachable) |
| `pio run -e treklink-v3-tbeam` | **failed**, `Platform Manager: Installing platformio/espressif32 @ 6.12.0` then `HTTPClientError` |
| `pio run -e treklink-v2` | **failed**, same |
| `pio run -e treklink-v4-supreme` | **failed**, same |
| `pio test -e native -f test_mqtt` | **failed**, `PackageException: Got the unrecognized status code '403' when downloaded https://github.com/meshtastic/platform-native/archive/f566d364....zip` |

Cause, from the session proxy's failure log: the egress policy rejects CONNECT to `api.registry.platformio.org`, `api.registry.nm1.platformio.org`, `dl.registry.platformio.org` and `collector.platformio.org`; `github.com/meshtastic/platform-native/archive/...` returns 403. Toolchains, the ESP32 platform, the native platform and every registry library are unreachable. `raw.githubusercontent.com` and PyPI are reachable. Host `g++ 13.3`, `clang++` and `cmake` are installed.

No firmware binary was built. HEAD is unmodified; no build result exists for it from this session.
