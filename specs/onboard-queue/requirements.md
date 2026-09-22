# Requirements Specification: onboard-queue (firmware)

**User Story**: As a **Guide carrying a TrekLink node out of Wi-Fi range**, I want the device itself to hold every event it generates — in priority order, across reboots — so that an SOS raised during an outage still reaches the platform when the uplink returns, instead of being silently discarded by routine telemetry.
**Story ID**: US-102 | **Story Points**: 13 | **Priority**: **Highest** | **Main Flow**: MF-02 | **Owner**: KhoaDD

> **Authority**: **D-018** (three-tier buffering; this is Stage B), **D-019** (additive, non-destructive firmware changes), **D-021** (encryption split), **D-008** (firmware is editable and earns credit), **D-015** (business parameters are configuration). Firmware facts are cited to `04-firmware-ground-truth.md` §4.1–4.2, never assumed.

---

## 1. Domain Context & Scope

`onboard-queue` replaces the *policy* of the node's existing MQTT outbound queue while leaving its *wire behaviour* untouched. It is the on-device half of MF-02's offline-sync claim.

### The defect this exists to fix

The firmware already has an outbound queue. Verified properties (`04-firmware-ground-truth.md` §4.1):

| Property | Today | Required |
|---|---|---|
| Depth | 16 entries (`MQTT.h:27`) | configurable; RAM tier + flash tier |
| Storage | RAM only (`PointerQueue.h:8`) | RAM, spilling to LittleFS |
| Survives reboot | **No** | **Yes** |
| Overflow | **discards the OLDEST** (`MQTT.cpp:821–823`) | sheds **newest within the lowest occupied tier**; never P0 |
| Priority | **none — strict FIFO** | P0–P3, flushed in tier order |
| Loss visibility | a serial `LOG_WARN` nobody reads | per-tier counters, published |

Because eviction takes the oldest entry, **the single `"SOS - …"` text frame that identifies an episode is the first thing dropped** — it is the oldest thing in the queue by the time telemetry fills the remaining 15 slots, roughly 80 seconds into an outage.

### In scope

- A priority-aware, two-tier (RAM → flash) durable outbound queue for MQTT-bound events
- On-device priority classification at enqueue time
- Shedding policy, capacity bounds, and per-tier drop counters
- Queue-health reporting on the reserved `PRIVATE_APP` port
- Restoring the queue at boot and flushing it on uplink recovery
- The SOS-discriminator retransmit fix (firmware-fix candidate #1, D-008) — **as a separately measurable change**

### Out of scope

- Any change to the published topic shape or payload encoding (**prohibited by D-019 §3**)
- Mesh-internal store-and-forward for *peer* nodes — that is Stage C's job (D-018); this queue holds only this node's own events
- Rearchitecting the Meshtastic mesh stack (charter §2, upheld by D-008)
- Mesh-range packet loss — a node out of range of any relay drops at source and this layer cannot see it
- `mqtt.encryption_enabled` — stays `false` per D-007/D-021

### Depends on

- `TrekLinkSOSHelper` — queried for live SOS-episode state (see REQ-EVT-04)
- `FSCom` / `SafeFile` — the flash substrate, following the `MessageStore` precedent (`MessageStore.cpp:244`, `:287`)
- Nothing in `treklink-web`. The backend contract is unchanged by construction.

---

## 2. EARS Functional Criteria

### Ubiquitous (always active)

- **REQ-UBI-01**: The queue SHALL store each entry as the existing `{ topic, envBytes }` pair plus a priority tier, a monotonic enqueue sequence, and the source `MeshPacket.id`. It SHALL NOT re-encode, re-frame, or otherwise alter `envBytes`.
- **REQ-UBI-02**: The system SHALL preserve the stock enqueue and drain logic in a reachable form, selectable at compile time, per **D-019 §1**. A build with the TrekLink queue disabled SHALL behave byte-identically to the current firmware.
- **REQ-UBI-03**: The system SHALL continue to publish to `<root>/2/e/<channelId>/<nodeId>` and, when `json_enabled`, `<root>/2/json/<channelId>/<nodeId>`, with unchanged payloads (**D-019 §3**).
- **REQ-UBI-04**: The system SHALL keep the stock Meshtastic mobile-app paths — BLE pairing and `proxy_to_client_enabled` MQTT relay — fully functional (**D-019 §2**).
- **REQ-UBI-05**: Every capacity bound, flush interval, shed policy and classification threshold SHALL be a `build_flags`-overridable constant with a documented default, following `MESSAGE_HISTORY_LIMIT` (`MessageStore.h:22–24`), and SHALL be registered in the Configuration Matrix (**D-015**).
- **REQ-UBI-06**: The system SHALL maintain a monotonic per-tier counter of events enqueued, published, and shed, and SHALL persist these counters across reboot.

### Event-Driven

- **REQ-EVT-01**: WHEN a packet is ready for MQTT publish and the uplink is connected, the system SHALL publish immediately and SHALL NOT enqueue — preserving the current fast path (`MQTT.cpp:800`).
- **REQ-EVT-02**: WHEN a packet is ready for MQTT publish and the uplink is not connected, the system SHALL classify it into a priority tier and enqueue it.
- **REQ-EVT-03**: WHEN classifying a packet, the system SHALL assign: `TEXT_MESSAGE_APP` with `priority == MAX` → **P0**; `POSITION_APP` while an SOS episode is active on this node → **P1**; `POSITION_APP` otherwise → **P2**; `TELEMETRY_APP` → **P3**; any other PortNum → **P3**.
- **REQ-EVT-04**: WHEN classifying a `POSITION_APP` packet, the system SHALL determine SOS-episode state by querying `TrekLinkSOSHelper` directly, and SHALL NOT infer it from cadence. *(The device holds ground truth about its own SOS state; the backend can only infer P1 through a grace-window heuristic — D-007. On-device classification is therefore strictly more accurate, and this requirement is the reason.)*
- **REQ-EVT-05**: WHEN the RAM tier reaches its bound, the system SHALL spill the lowest-priority, oldest entries to the flash tier rather than discarding them.
- **REQ-EVT-06**: WHEN a **P0** entry is enqueued, the system SHALL flush it to flash before returning, so a power loss within milliseconds of an SOS cannot lose it.
- **REQ-EVT-07**: WHEN the device begins an orderly shutdown, the system SHALL persist the entire RAM tier to flash, on the existing shutdown hook that already serves `MessageStore` (`Power.cpp:810`).
- **REQ-EVT-08**: WHEN the device boots, the system SHALL restore the flash tier into the queue before the MQTT thread begins publishing.
- **REQ-EVT-09**: WHEN the uplink transitions to connected and the queue is non-empty, the system SHALL flush in **priority ascending, then enqueue-sequence ascending** order — all P0 before any P1, oldest first within a tier.
- **REQ-EVT-10**: WHEN an entry's publish to the `/2/e/` topic succeeds, the system SHALL delete that entry. A failure of the companion `/2/json/` publish SHALL NOT prevent deletion. *(JSON is a convenience topic; treating it as a delivery condition doubles the failure surface for no gain.)*
- **REQ-EVT-11**: WHEN the queue sheds an entry, the system SHALL increment the shed counter for that entry's tier before releasing it.
- **REQ-EVT-12**: WHEN a queue-health report is due, the system SHALL publish depth-per-tier and the counters of REQ-UBI-06 as a packet on **`PRIVATE_APP` (256)**. *(The reserved application range — `04-firmware-ground-truth.md` §6 — so the report is additive: same envelope, same topic, and stock clients ignore an unknown PortNum. This is what keeps REQ-UBI-03 satisfiable while still making loss observable.)*

### State-Driven

- **REQ-STA-01**: WHILE the uplink is unavailable, the system SHALL continue accepting events until the *total* bound across both tiers is reached, and SHALL NOT discard before that point.
- **REQ-STA-02**: WHILE the queue is at its total bound, the system SHALL shed the **newest** entry within the **lowest-priority occupied tier**, and SHALL NEVER shed a P0 entry.
- **REQ-STA-03**: WHILE flushing, the system SHALL publish at most one entry per MQTT thread tick, preserving the existing 200 ms cadence (`MQTT.cpp:609`, `:619`). *(A burst dump after a long outage would trip broker rate limits and starve the LoRa thread.)*
- **REQ-STA-04**: WHILE an SOS episode is active, the system SHALL NOT shed any entry belonging to that episode regardless of tier.

### Unwanted Behaviour / Error Cases

- **REQ-ERR-01**: IF the queue is at its total bound and the entry being enqueued is **P0** and every queued entry is also **P0**, THEN the system SHALL refuse the enqueue, increment a distinct `p0_refused` counter, and SHALL NOT shed any existing P0. *(Losing a known-queued SOS to make room for a newer one is not an improvement. Refusal is recorded rather than silent.)*
- **REQ-ERR-02**: IF a flash write fails or the filesystem is full, THEN the system SHALL keep the entry in the RAM tier, increment a `flash_write_failed` counter, and continue operating. A flash fault SHALL NOT crash the MQTT thread or block the LoRa stack.
- **REQ-ERR-03**: IF the flash tier is corrupt or partially written at boot, THEN the system SHALL restore every record that validates, discard the remainder, count the discards, and boot normally. A corrupt queue file SHALL NOT prevent boot.
- **REQ-ERR-04**: IF power is lost between a successful publish and the deletion of that entry, THEN the entry SHALL be re-published after reboot. This is accepted and **not** deduplicated on-device — the backend's unique index on `eventId = sha256(nodeNum:packetId)` (D-006) already makes the second delivery a no-op. *(A second dedup index on a microcontroller would duplicate a guarantee the backend provides for free. The `MeshPacket.id` is stored per REQ-UBI-01 so the backend can key on it.)*
- **REQ-ERR-05**: IF the queue is enabled on a variant with MQTT compiled out — `treklink-v1_0` sets `-D MESHTASTIC_EXCLUDE_MQTT=1` (`04-firmware-ground-truth.md` §5) — THEN the queue SHALL also compile out, with no dangling references.
- **REQ-ERR-06**: IF the configured flash bound exceeds the space actually available on the variant, THEN the build SHALL fail or the bound SHALL be clamped at init with a logged warning — never silently accepted. *(v3 is ESP32 LX6 with no PSRAM and ~4 MB flash shared with two OTA slots; a bound that passes on v2's 8 MB would otherwise fail only in the field.)*

### Optional Features

- **REQ-OPT-01**: WHERE the SOS-discriminator retransmit fix is enabled, `tickBeacon()` SHALL re-send the SOS text discriminator every Nth beacon, N configurable. *(Retires the Critical single-frame risk at source. Built and measured **separately** from the queue so each change's effect is attributable.)*
- **REQ-OPT-02**: WHERE the beacon-priority fix is enabled, the beacon send path SHALL set `priority = MAX` explicitly instead of delegating to `PositionModule::sendOurPosition()`, which assigns `BACKGROUND` for a handheld role (`PositionModule.cpp:377–380`).

---

## 3. Non-Functional Requirements

| Property | Target | Verified by |
|---|---|---|
| Offline recovery delivery rate | **≥99%** after a 30 s – 30 min outage | RQ1 bench harness, uplink cut only |
| Priority-ordering compliance | **≥99%**, every P0 before any P2/P3 on reconnect | flush-order assertion over a mixed-tier queue |
| Reboot durability | 100% of flash-tier entries survive a power cycle | power-cycle test with a non-empty queue |
| P0 durability window | a P0 event survives power loss ≥50 ms after enqueue | REQ-EVT-06 |
| Queue capacity | ≥200 mixed events on v3; ≥1000 on v2/v4 | per-variant capacity test |
| Flash wear | ≤1 erase cycle per minute at steady state | write-amplification measurement over a 30 min outage |
| Stock-path regression | zero behavioural delta with the queue disabled | REQ-UBI-02 build compared against baseline |
| Mesh impact | no measurable increase in airtime utilisation | `airtime` counters, queue on vs off |

**Baseline for every comparative figure** is the **unmodified firmware at its current commit on `dev`** (D-018), built from the preserved stock path of REQ-UBI-02. Measurement method: **cut the uplink only**, leaving the mesh intact, so the variable under test is isolated and the experiment is reproducible indoors.

---

## 4. Acceptance Criteria

- **AC-01**: With the uplink severed, 16 telemetry events followed by one SOS text frame leaves the SOS present in the queue. *(On stock firmware the SOS is evicted — this is the before/after that carries the research claim.)*
- **AC-02**: With the uplink severed for 30 minutes and 200 mixed-tier events enqueued, restoring the uplink delivers ≥99% and publishes every P0 before any P2 or P3.
- **AC-03**: A power cycle with a non-empty queue loses nothing from the flash tier.
- **AC-04**: An SOS enqueued and the power cut 50 ms later is present after reboot.
- **AC-05**: Filling the queue with P3 telemetry then enqueuing a P0 sheds telemetry, not the P0, and increments the P3 shed counter.
- **AC-06**: A queue of only P0 entries at capacity refuses a further P0 and increments `p0_refused`, shedding nothing.
- **AC-07**: A truncated queue file at boot restores the valid prefix, counts the discarded tail, and boots.
- **AC-08**: A build with the queue disabled produces behaviour identical to the baseline firmware on the same inputs (REQ-UBI-02).
- **AC-09**: The stock Meshtastic mobile app pairs over BLE and relays MQTT via `proxy_to_client_enabled` with the queue enabled (REQ-UBI-04).
- **AC-10**: A `PRIVATE_APP` health report is published and is ignored without error by a stock Meshtastic client (REQ-EVT-12).
- **AC-11**: The capacity bound is demonstrated changeable from `build_flags` without a source edit, and the change is visible at runtime (D-015 — "can this number be changed? demo it now").
- **AC-12**: A `POSITION_APP` packet emitted during an active SOS episode is classified P1 on-device (REQ-EVT-04), with no dependence on backend cadence inference.
- **AC-13**: All of the above pass on **v3** hardware, not only v2 (REQ-ERR-06).

---

## 5. Open Questions

- [ ] **Q1.** Health-report interval for REQ-EVT-12. Too frequent wastes airtime on a congested mesh; too rare and a dashboard staleness indicator lags. Proposed default 300 s, and suppressed entirely while the queue is empty and the uplink is up.
- [ ] **Q2.** Retransmit interval N for REQ-OPT-01. Every beacon is the most robust and the most airtime; every 4th beacon bounds the worst-case discriminator loss to ~20 s during the dense phase. Needs a number before AC-01's companion SOS-loss experiment can be designed.
- [ ] **Q3.** Whether the flash tier is one append-only log with compaction, or fixed-size slots indexed by tier. Log is simpler and wears more evenly; slots make priority flush O(1). Resolved in `design.md` §2 — flagged here because it is the one genuinely reversible-at-cost decision.
- [ ] **Q4.** Does an SOS episode's own entries get an unbounded exemption under REQ-STA-04, or a bounded one? An indefinitely beaconing device with no uplink will otherwise fill the queue with its own P1 positions and refuse everything else.
