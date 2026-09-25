# Requirements Specification: onboard-queue (firmware)

**User Story**: As a **Guide carrying a TrekLink node out of Wi-Fi range**, I want the device itself to hold every event it generates, in priority order and across reboots, so that an SOS raised during an outage still reaches the platform when the uplink returns, instead of being silently discarded by routine telemetry.
**Story ID**: US-102 | **Story Points**: 13 | **Priority**: **Highest** | **Main Flow**: MF-02 | **Owner**: KhoaDD

**Approval**: approved by the leader on 2026-09-25 with the Phase A corrections S1 to S15 (`_handoff/phase-a-audit.md`, handoff entry O-003). This revision applies them.

> **Authority**: **D-018** (three-tier buffering; this is Stage B), **D-019** (additive, non-destructive firmware changes), **D-021** (encryption split), **D-008** (firmware is editable and earns credit), **D-015** (business parameters are configuration). Firmware facts are cited to `04-firmware-ground-truth.md` §2 and §4 to §5 as re-audited on 2026-09-25, never assumed.

---

## 1. Domain Context & Scope

`onboard-queue` replaces the *policy* of the node's existing MQTT outbound queue while leaving its *wire behaviour* untouched. It is the on-device half of MF-02's offline-sync claim.

### The defect this exists to fix

The firmware already has an outbound queue. Verified properties (`04-firmware-ground-truth.md` §4.1):

| Property | Today | Required |
|---|---|---|
| Depth | 16 entries (`MQTT.h:27`) | configurable; RAM tier plus flash tier |
| Storage | RAM only (`PointerQueue.h:8`) | RAM, spilling to LittleFS; SOS entries written through |
| Survives an orderly reboot | **No** | **Yes** |
| Survives unclean power loss | **No** | P0 and P1 entries and everything already spilled: **yes**. Unspilled P2/P3 RAM entries: no |
| Overflow | **discards the OLDEST** (`MQTT.cpp:821-823`) | sheds the **newest within the lowest occupied tier**; never P0 |
| Priority | **none, strict FIFO** | P0 to P3, flushed in tier order |
| Drain while connected (direct Wi-Fi) | **one entry per successful reconnect, none while the link stays up** (`MQTT.cpp:604-636`) | one entry per drain interval until empty |
| Contents | own packets and peer packets heard on an uplink-enabled channel (`Router.cpp:369-372`, `:767-769`) | unchanged: both |
| Loss visibility | a serial `LOG_WARN` | per-tier counters, published |

Because eviction takes the oldest entry, **the single `"SOS - …"` text frame that identifies an episode is the first thing dropped** once telemetry, beacons and peer traffic fill the remaining slots. Even surviving entries are published one per reconnect.

### In scope

- A priority-aware, two-tier (RAM and flash) durable outbound queue for MQTT-bound packets, own and peer
- On-device priority classification at enqueue time
- Shedding policy, capacity bounds, and per-tier counters
- Queue-health reporting on the reserved `PRIVATE_APP` port, MQTT-only
- Restoring the queue at boot and draining it while the uplink is up
- SOS robustness fixes, each **separately measurable** (Phase 9 of `tasks.md`)

### Out of scope

- Any change to the published topic shape or to the payload of any existing PortNum (**prohibited by D-019 §3**). The additive `PRIVATE_APP` JSON case of REQ-EVT-12 changes nothing stock emits.
- Aggregating peers that the uplink node cannot hear. This queue holds what reaches this node's `MQTT::onSend()`: its own packets and packets it receives directly or by relay. Multi-node aggregation beyond that is Stage C (D-018).
- Rearchitecting the Meshtastic mesh stack (charter §2, upheld by D-008)
- Mesh-range packet loss. A node out of range of any relay drops at source and this layer cannot see it.
- `mqtt.encryption_enabled`, which stays `false` per D-007 and D-021
- MQTT QoS: stock publishes at QoS 0 and this work does not change it

### Depends on

- `TrekLinkButtonModule`, `TrekLinkSOSGesture`, `FallDetectionModule`: read-only queries of their existing SOS-state accessors (REQ-EVT-04)
- `TrekLinkSOSHelper.h`: the single definition of the SOS text prefix (REQ-EVT-03)
- `FSCom` (LittleFS on ESP32, `FSCommon.h:26-28`) and `SafeFile` for compaction only
- `notifyReboot` and `notifyDeepSleep` observables (`sleep.h:38`, `:41`) for orderly persistence
- Nothing in `treklink-web`, except that `gateway-sync` consumes the health payload defined in `design.md` §2.4

---

## 2. EARS Functional Criteria

### Ubiquitous (always active)

- **REQ-UBI-01**: The queue SHALL store each entry as the existing `{ topic, envBytes }` pair plus a priority tier, a monotonic enqueue sequence, the source `MeshPacket.id`, and an episode flag. It SHALL NOT re-encode, re-frame, or otherwise alter `envBytes`.
- **REQ-UBI-02**: The system SHALL preserve the stock enqueue and drain logic in a reachable form, selected at compile time by `TREKLINK_ONBOARD_QUEUE`, per **D-019 §1**. A build with the flag off SHALL behave identically to the unmodified firmware, including the one-entry-per-reconnect drain, and SHALL pass the upstream `test/test_mqtt` suite unchanged. This build is the RQ1 baseline.
- **REQ-UBI-03**: The system SHALL continue to publish to `<root>/2/e/<channelId>/<nodeId>` and, when `json_enabled`, `<root>/2/json/<channelId>/<nodeId>`, with unchanged payloads for every PortNum stock firmware emits (**D-019 §3**).
- **REQ-UBI-04**: The system SHALL keep the stock Meshtastic mobile-app paths, BLE pairing and the `proxy_to_client_enabled` MQTT relay, fully functional (**D-019 §2**).
- **REQ-UBI-05**: Every capacity bound, drain interval, shed parameter, exemption cap, health interval and compaction threshold SHALL be a `build_flags`-overridable constant with a documented default, following `MESSAGE_HISTORY_LIMIT` (`MessageStore.h:22-24`), and SHALL be registered in the Configuration Matrix (**D-015**).
- **REQ-UBI-06**: The system SHALL maintain monotonic per-tier counters of entries enqueued, published and shed, plus `p0_refused`, `flash_write_failed` and `restore_discarded`, and SHALL persist them at every orderly persistence point (REQ-EVT-07) and at every health report.

### Event-Driven

- **REQ-EVT-01**: WHEN a packet is ready for MQTT publish, the uplink is connected, and the queue is empty, the system SHALL publish immediately and SHALL NOT enqueue, preserving the stock fast path (`MQTT.cpp:800`).
- **REQ-EVT-02**: WHEN a packet is ready for MQTT publish and either the uplink is not connected or the queue is non-empty, the system SHALL classify it into a priority tier and enqueue it.
- **REQ-EVT-03**: WHEN classifying a packet, the system SHALL assign, in this order:
  1. payload not decoded: **P3**
  2. `TEXT_MESSAGE_APP` whose payload begins with the SOS text prefix defined once in `TrekLinkSOSHelper.h`: **P0**, own or peer
  3. `TEXT_MESSAGE_APP` originated by this node with `priority == MAX`: **P0**
  4. `POSITION_APP` originated by this node while an SOS episode is active on this node, or with `priority == MAX`: **P1**
  5. any other `POSITION_APP`: **P2**
  6. everything else, including `TELEMETRY_APP`: **P3**

  *(A peer's SOS text reaches this node with `priority == HIGH`, because priority is not carried over the air and `fixPriority()` assigns `HIGH` to text (`MeshPacketQueue.cpp:53-55`). The prefix is the only signal that survives the hop. Rule 3 keeps a local SOS P0 even if its text is ever reformatted.)*
- **REQ-EVT-04**: WHEN classifying a `POSITION_APP` packet originated by this node, the system SHALL determine SOS-episode state by querying the existing accessors `TrekLinkButtonModule::isSOSActive()`, `TrekLinkSOSGesture::isSOSActive()` and `FallDetectionModule::isInSOSTriggered()`, and SHALL NOT infer it from cadence. *(`TrekLinkSOSHelper` holds no state. The button and gesture flags turn true only after `triggerSOS()` returns, which is why rule 4 of REQ-EVT-03 also accepts `priority == MAX`: the trigger-time position carries `MAX` and would otherwise be P2.)*
- **REQ-EVT-05**: WHEN the RAM tier exceeds its bound, the system SHALL spill the lowest-priority, oldest RAM-resident entries to the flash tier in one batched write rather than discarding them.
- **REQ-EVT-06**: WHEN a **P0** or **P1** entry is enqueued, the system SHALL append it to flash before returning, so a power loss after the enqueue cannot lose it.
- **REQ-EVT-07**: WHEN the device begins an orderly reboot (`notifyReboot`) or shutdown or deep sleep (`notifyDeepSleep`), the system SHALL persist every RAM-resident entry, every pending deletion marker and the counters to flash. This SHALL NOT depend on `HAS_SCREEN`.
- **REQ-EVT-08**: WHEN the device boots, the system SHALL restore the flash tier into the queue before the MQTT layer publishes or enqueues anything.
- **REQ-EVT-09**: WHILE draining, the system SHALL publish in **tier ascending, then enqueue-sequence ascending** order: all P0 before any P1, oldest first within a tier.
- **REQ-EVT-10**: WHEN an entry's `/2/e/` publish returns success and the next MQTT client poll also succeeds, the system SHALL delete that entry. IF the link drops before that poll, THEN the entry SHALL stay queued and be published again. A failure of the companion `/2/json/` publish SHALL NOT prevent deletion. In proxy mode the hand-off to the phone is the success point, as in stock firmware.
- **REQ-EVT-11**: WHEN the queue sheds an entry, the system SHALL increment the shed counter for that entry's tier before releasing it.
- **REQ-EVT-12**: WHEN a queue-health report is due and the uplink is connected, the system SHALL publish depth per tier and the counters of REQ-UBI-06 as a JSON payload on **`PRIVATE_APP` (256)**, on both the `/2/e/` and `/2/json/` topics, **MQTT-only**: the packet SHALL NOT be transmitted over LoRa and SHALL NOT be queued. The JSON serializer SHALL gain an additive `PRIVATE_APP` case that emits that payload; for any `PRIVATE_APP` payload that is not valid JSON it SHALL behave as stock (no `payload` key). The schema is `design.md` §2.4.
- **REQ-EVT-13**: WHEN the uplink is connected and the queue is non-empty, the system SHALL drain one entry per drain interval, on the direct-connected branch of `MQTT::runOnce()` as well as on the proxy and reconnect branches.

### State-Driven

- **REQ-STA-01**: WHILE the uplink is unavailable, the system SHALL continue accepting entries until the total bound is reached, and SHALL NOT discard before that point.
- **REQ-STA-02**: WHILE the queue is at its total bound, the system SHALL treat the incoming entry and every sheddable queued entry as candidates, and SHALL shed the **newest** candidate in the **lowest-priority occupied tier**. It SHALL NEVER shed a P0 entry.
- **REQ-STA-03**: WHILE draining, the system SHALL publish at most one entry per drain interval, default 200 ms, configurable. *(A burst after a long outage would trip broker rate limits and starve the LoRa thread.)*
- **REQ-STA-04**: WHILE an SOS episode is active on this node, the system SHALL NOT shed the first `K` or the latest `M` queued entries flagged as belonging to it; entries between them are sheddable at their own tier. `K` and `M` are configuration, default 3 and 5.
- **REQ-STA-05**: WHILE the flash tier is unavailable (a write failed, or the budget is exhausted), the effective total bound SHALL be the RAM-tier bound, and REQ-STA-02 SHALL apply against it.

### Unwanted Behaviour / Error Cases

- **REQ-ERR-01**: IF the queue is at its bound, the incoming entry is **P0**, and no queued entry is sheddable (every queued entry is P0 or exempt under REQ-STA-04), THEN the system SHALL refuse the enqueue, increment `p0_refused`, and SHALL NOT shed any existing entry. IF the incoming entry is not P0 under the same conditions, THEN it SHALL be refused and counted as shed at its own tier.
- **REQ-ERR-02**: IF a flash write fails or the flash budget is full, THEN the system SHALL keep the entry in the RAM tier, increment `flash_write_failed`, and continue operating. A flash fault SHALL NOT crash the MQTT thread or block the LoRa stack.
- **REQ-ERR-03**: IF the flash log is corrupt or partially written at boot, THEN the system SHALL restore every record before the first invalid one, discard the remainder, count the discarded bytes in `restore_discarded`, rewrite the log without the tail, and boot normally.
- **REQ-ERR-04**: IF power is lost between a successful publish and the durable deletion of that entry, THEN the entry SHALL be re-published after reboot. This is accepted and **not** deduplicated on-device: the backend's unique index on `eventId = sha256(nodeNum:packetId)` (D-006) makes the second delivery a no-op. The same holds for deletion markers not yet flushed.
- **REQ-ERR-05**: IF MQTT is compiled out (`treklink` v1 sets `-D MESHTASTIC_EXCLUDE_MQTT=1`), THEN the queue integration SHALL compile out with no dangling references.
- **REQ-ERR-06**: IF the configured flash budget exceeds the free LittleFS space at init, THEN the budget SHALL be clamped with a logged warning, never silently accepted. The budget is derived from LittleFS space, never from PSRAM.

### Optional Features

- **REQ-OPT-01**: WHERE the SOS-discriminator retransmit fix is enabled, the beacon path SHALL re-send the SOS text every Nth beacon, N configurable, default 4.
- **REQ-OPT-02**: WHERE the beacon-priority fix is enabled, the beacon SHALL be built as its own position packet with `priority = MAX` instead of delegating to `PositionModule::sendOurPosition()`, which assigns `BACKGROUND` (`PositionModule.cpp:374-377`) and sends nothing before the first fix (`:352-355`).
- **REQ-OPT-03**: WHERE the fall-beacon fix is enabled, a fall auto-SOS SHALL enter the same beacon loop as a button or gesture SOS, and its opening position SHALL carry `priority = MAX`.

---

## 3. Non-Functional Requirements

| Property | Target | Verified by |
|---|---|---|
| Offline recovery delivery rate | **≥99%** after a 30 s to 30 min outage | RQ1 bench harness, uplink cut only |
| Priority-ordering compliance | **≥99%**, every P0 before any P2/P3 on reconnect | flush-order assertion over a mixed-tier queue |
| Orderly-reboot durability | 100% of queued entries survive `notifyReboot` | reboot test with a non-empty queue |
| SOS durability | a P0 or P1 entry survives power loss once `enqueue()` has returned | REQ-EVT-06, AC-04 |
| Queue capacity | ≥200 mixed entries on v3; ≥1000 on v2/v4 | per-variant capacity test |
| Flash wear, no SOS | ≤1 flash append per minute at steady state, amortised | write counter over a 30 min outage |
| Flash wear, SOS episode | the bound in `design.md` §2.6, on record | write counter over a scripted episode |
| Stock-path regression | zero behavioural delta with the flag off | REQ-UBI-02 |
| Mesh impact | no increase in airtime; health reports never use LoRa | `airtime` counters, queue on vs off |

**Baseline for every comparative figure** is the flag-off build of **REQ-UBI-02**, which is behaviourally the unmodified firmware on `dev` (D-018). Measurement method: **cut the uplink only**, leaving the mesh intact.

---

## 4. Acceptance Criteria

- **AC-01**: With the uplink severed, 16 telemetry entries followed by one SOS text leaves the SOS in the queue. *(On stock firmware the SOS is evicted. This is the before/after that carries the research claim.)*
- **AC-02**: With the uplink severed for 30 minutes and 200 mixed-tier entries enqueued, restoring the uplink delivers ≥99% and publishes every P0 before any P2 or P3, without another disconnect.
- **AC-03**: An orderly reboot with a non-empty queue loses nothing.
- **AC-04**: An SOS enqueued, then power cut, is present after reboot.
- **AC-05**: Filling the queue with P3 telemetry then enqueuing a P0 sheds telemetry, not the P0, and increments the P3 shed counter.
- **AC-06**: A queue of only P0 entries at capacity refuses a further P0 and increments `p0_refused`, shedding nothing.
- **AC-07**: A truncated log at boot restores the valid prefix, counts the discarded tail, and boots.
- **AC-08**: The flag-off build passes the upstream `test/test_mqtt` suite unchanged (REQ-UBI-02).
- **AC-09**: The stock Meshtastic app pairs over BLE and relays MQTT via `proxy_to_client_enabled` with the queue enabled (REQ-UBI-04).
- **AC-10**: A `PRIVATE_APP` health report appears on both topics, never on LoRa, and is ignored without error by a stock Meshtastic client (REQ-EVT-12).
- **AC-11**: The capacity bound is changed from `build_flags` without a source edit, and the change is visible in the health report (D-015).
- **AC-12**: A `POSITION_APP` packet emitted during an active SOS episode, including the trigger-time position, is classified P1 on-device (REQ-EVT-03, REQ-EVT-04).
- **AC-13**: A peer's `"SOS - …"` text received while the uplink is down is classified P0 (REQ-EVT-03 rule 2).
- **AC-14**: All of the above pass on **v3** hardware, not only v2 (REQ-ERR-06).

---

## 5. Open Questions

- [x] **Q1.** Health-report interval. **Answered 2026-09-25 (O-003)**: 300 s, suppressed while the queue is empty and the uplink is up, plus one report on the first drain after an outage. Configuration.
- [x] **Q2.** Retransmit interval N for REQ-OPT-01. **Answered**: every 4th beacon. Configuration.
- [x] **Q3.** Flash layout. **Answered**: one append-only log with compaction, `design.md` §1.2.
- [x] **Q4.** Episode exemption. **Answered**: bounded, first `K = 3` and latest `M = 5` (REQ-STA-04). Configuration.
- [x] **Q5.** Peer packets. **Answered (O-003, Q-A3)**: peers use the TrekLink queue under the same policy.
- [x] **Q6.** Health transport. **Answered (O-003, Q-A4)**: MQTT-only, JSON payload, additive serializer case.
