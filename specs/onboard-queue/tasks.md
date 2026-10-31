# Implementation Tasks: onboard-queue (firmware)

> Reads with [`requirements.md`](requirements.md) and [`design.md`](design.md). Branch `feat/onboard-queue` off `dev`, per `01-conventions/07-github-workflow-git-conventions.md`. Revised 2026-09-25 with the Phase A corrections (O-003).
>
> **Phases 1 to 4 are the platform-free policy core** (`design.md` §0.1). They run on a laptop under PlatformIO `native` and under host `g++`. Phase 5 onward touches stock files and needs the firmware toolchain; Phase 7 needs hardware.
>
> **Shared stock symbols** (`MQTT::onSend`, `MQTT::runOnce`, `MQTT::publishQueuedMessages`, `MeshPacketSerializer::JsonSerialize`, `TrekLinkSOSHelper::triggerSOS`, `TrekLinkSOSHelper::sendSOSTextMessage`, `FallDetectionModule::triggerAutoSOS`) are touched only after the orchestrator returns their GitNexus blast radius (O-004).

---

## Phase 0: Blockers

- [x] 0.1 Establish the LittleFS partition size on v2, v3 and v4 from each variant's partition table; confirm the Phase 4 bounds fit
  - v3 1 MiB (`partition-table.csv`), v2 and v4 1.5 MiB (framework `default_8MB.csv`). Recorded in `design.md` §4 and ground truth §5. On-device free-space measurement is task 7.8 (hardware work is on hold, clarification #26).
  - _Requirements: REQ-ERR-06_
- [x] 0.2 Answer Q1 (health-report interval) and Q2 (retransmit interval N): 300 s and every 4th beacon (O-003)
- [x] 0.3 Answer Q4: bounded exemption, K = 3, M = 5 (O-003)
  - _Requirements: REQ-STA-04_
- [ ] 0.4 Capture golden fixtures from a live v2: one encoded `ServiceEnvelope` each for an SOS text, an SOS-episode position, a routine position, and a device-metrics telemetry frame
  - On hold with the other hardware work (clarification #26). Phases 1 to 4 use hand-built envelopes meanwhile; the fixtures replace them when captured.
- [x] 0.5 Apply the Phase A corrections S1 to S15 to `requirements.md`, `design.md` and this file, spec-only, before any code (O-003, Q-A2)

---

## Phase 1: Foundation & Record Format

- [x] 1.1 Add the `TREKLINK_ONBOARD_QUEUE` build flag, on for v2/v3/v4 in their `platformio.ini`, absent for v1, and compile the adapter out when `MESHTASTIC_EXCLUDE_MQTT` is set
  - _Requirements: REQ-ERR-05_
- [x] 1.2 Create `src/mqtt/TrekLinkQueueCore.h` with `Tier`, `ClassifyInput`, `Config`, `Stats`, `LogStorage`, `QueueCore`, per `design.md` §2.5
  - _Requirements: REQ-UBI-01_
- [x] 1.3 Implement the DATA and TOMBSTONE record codec with a CRC-32 trailer (`design.md` §1.1)
  - _Requirements: REQ-UBI-01, REQ-ERR-03_
- [x] 1.4 Add every bound and interval to `src/mqtt/TrekLinkQueueConfig.h` as a `build_flags`-overridable constant with a documented default
  - _Requirements: REQ-UBI-05_
- [x] 1.5 Unit test the record round-trip, a maximum-size envelope, an oversize length, and a truncated tail
  - _Requirements: REQ-UBI-01, REQ-ERR-03_

---

## Phase 2: Classification, pure

- [x] 2.1 Define the SOS text prefix once in `TrekLinkSOSHelper.h` and build the SOS text from it in `sendSOSTextMessage()` callers, keeping the emitted bytes identical
  - Shared symbols: waits for the blast radius (see the note at the top)
  - _Requirements: REQ-EVT-03_
- [x] 2.2 Implement `classify()` per `design.md` §2.1, prefix passed in
  - _Requirements: REQ-EVT-03, REQ-EVT-04_
- [x] 2.3 Unit test classification across PortNum × origin × priority × episode state × prefix
  - Must cover: undecoded → P3; own `MAX` text without prefix → P0; peer `HIGH` text with prefix → P0; peer text without prefix → P3; own position with episode → P1; own `MAX` position without episode → P1; peer position → P2; telemetry → P3
  - _Requirements: REQ-EVT-03, REQ-EVT-04, AC-12, AC-13_

---

## Phase 3: Index, Ordering & Shedding

- [x] 3.1 Implement the RAM index, `{seq, packetId, offset, len, tier, flags, location}`
  - _Requirements: REQ-UBI-01_
- [x] 3.2 Implement `enqueue()` with monotonic `seq`
  - _Requirements: REQ-EVT-02_
- [x] 3.3 Implement `peek()`: minimum tier, then minimum `seq`
  - _Requirements: REQ-EVT-09_
- [x] 3.4 Implement `markInFlight()`, `commit()` and `release()`; an in-flight entry is never shed
  - _Requirements: REQ-EVT-10_
- [x] 3.5 Implement shedding per `design.md` §2.2, incoming entry as a candidate
  - _Requirements: REQ-STA-02, REQ-ERR-01_
- [x] 3.6 Implement the bounded K/M episode exemption
  - _Requirements: REQ-STA-04_
- [x] 3.7 Implement `Stats`: enqueued, published and shed by tier, `p0_refused`, `flash_write_failed`, `restore_discarded`
  - _Requirements: REQ-UBI-06_
- [x] 3.8 Unit test flush ordering over a randomised mixed-tier queue
  - _Requirements: REQ-EVT-09_
- [x] 3.9 Unit test the shed tree: P3 before P0; all-P0 refusal sheds nothing; incoming refused when it is the newest in the lowest tier; K/M exemption; in-flight protection; the counter identity
  - _Requirements: REQ-STA-02, REQ-STA-04, REQ-ERR-01, AC-05, AC-06_

---

## Phase 4: Flash Tier & Durability (core, against `LogStorage`)

- [x] 4.1 Write-through append for P0 and P1
  - _Requirements: REQ-EVT-06_
- [x] 4.2 Batched spill of the lowest-priority, oldest RAM entries when the RAM bound is exceeded
  - _Requirements: REQ-EVT-05_
- [x] 4.3 Batched tombstones: flushed at the batch size, piggybacked on data appends, and at `persistAll()`
  - _Requirements: REQ-EVT-10, REQ-ERR-04_
- [x] 4.4 `persistAll()`: spill every RAM entry, flush tombstones, save counters and `nextSeq`
  - _Requirements: REQ-EVT-07, REQ-UBI-06_
- [x] 4.5 `restore()`: one sequential scan, tombstones applied, valid prefix kept, corrupt tail counted and cut by a rewrite
  - _Requirements: REQ-EVT-08, REQ-ERR-03_
- [x] 4.6 Compaction on the dead-share threshold and before a write that would exceed the budget
  - _Requirements: REQ-EVT-05_
- [x] 4.7 Flash-failure fallback: entry kept in RAM, counted, effective bound drops to the RAM bound
  - _Requirements: REQ-ERR-02, REQ-STA-05_
- [x] 4.8 Unit test restore from a seeded log, a truncated log, a mid-log CRC fault, and tombstones across a restart; compaction; budget exhaustion; flash failure
  - _Requirements: REQ-EVT-08, REQ-ERR-02, REQ-ERR-03, AC-03, AC-04, AC-07_
- [x] 4.9 Health JSON builder per `design.md` §2.4, unit tested against the schema
  - _Requirements: REQ-EVT-12_

---

## Phase 5: Firmware Adapter & Stock Seams, non-destructive

- [x] 5.1 Implement `FsLogStorage` over `FSCom` (append) and `SafeFile(path, true)` (rewrite, meta), and clamp the budget against LittleFS free space at init
  - _Requirements: REQ-ERR-02, REQ-ERR-06_
- [x] 5.2 Implement `TrekLinkEventQueue`: classification input from `mp_decoded`, episode state from the three SOS modules, lazy restore, `notifyReboot` and `notifyDeepSleep` observers
  - _Requirements: REQ-EVT-04, REQ-EVT-07, REQ-EVT-08_
- [x] 5.3 `onSend()`: queue when disconnected **or** when the queue is non-empty; stock branch verbatim under `#else`
  - _Requirements: REQ-EVT-01, REQ-EVT-02, REQ-UBI-02_
- [x] 5.4 `publishQueuedMessages()` delegates to the drain tick; stock body verbatim under `#else`
  - _Requirements: REQ-EVT-09, REQ-UBI-02_
- [x] 5.5 Connected branch of `runOnce()` calls the drain tick at the configured interval
  - _Requirements: REQ-EVT-13, REQ-STA-03_
- [x] 5.6 Commit after the next successful client poll; release on link loss; commit at once in proxy mode
  - _Requirements: REQ-EVT-10_
- [x] 5.7 Build all four envs with the flag on and off; the flag-off build passes `test/test_mqtt` unchanged
  - All four ESP32 envs build (O-007). The native env builds with the flag off and passes `test_mqtt`; its only edit is a GCC 14 narrowing fix to two literals, not a behaviour change
  - _Requirements: REQ-UBI-02, AC-08_

---

## Phase 6: Health Reporting

- [x] 6.1 Publish the health record MQTT-only on `PRIVATE_APP` at the configured interval and after an outage
  - _Requirements: REQ-EVT-12_
- [x] 6.2 Additive `PRIVATE_APP` case in `MeshPacketSerializer`, schema-gated, with a unit test that a non-matching payload serialises as stock
  - Implemented with `test/test_meshpacket_serializer/ports/test_private_app.cpp`, which passes under `pio test -e native` (90 of 90 native cases, 2026-09-26)
  - _Requirements: REQ-EVT-12, REQ-UBI-03_
- [ ] 6.3 Confirm a stock Meshtastic client ignores the packet without error
  - _Requirements: REQ-UBI-04, AC-10_

---

## Phase 7: On-Device Verification

- [ ] 7.1 Boot restore with a pre-seeded non-empty log on v2
  - _Requirements: AC-03_
- [ ] 7.2 Power cut after a P0 enqueue; confirm the entry survives
  - _Requirements: AC-04_
- [ ] 7.3 Fill with P3 telemetry, enqueue a P0, confirm telemetry sheds and the SOS stays. Run the flag-off build first and record the stock eviction.
  - _Requirements: AC-01, AC-05_
- [ ] 7.4 Verify stock app BLE pairing and the `proxy_to_client_enabled` relay with the queue on
  - _Requirements: AC-09_
- [ ] 7.5 Change the capacity bound from `build_flags` and show it in the health report
  - _Requirements: AC-11_
- [ ] 7.6 Repeat 7.1 to 7.3 on **v3**
  - _Requirements: AC-14, REQ-ERR-06_
- [ ] 7.7 Measure flash writes and erase activity over a 30-minute outage with and without an SOS episode, against `design.md` §2.6
  - _Requirements: NFR flash wear_
- [ ] 7.8 Log LittleFS free space at boot on v2, v3 and v4 and confirm the clamped budget
  - _Requirements: REQ-ERR-06_

---

## Phase 8: Comparative Measurement (RQ1 / RQ2)

- [ ] 8.1 Build the harness: a scripted mixed-tier stream at a controlled rate, uplink cut programmatically, mesh left up
- [ ] 8.2 Run the flag-off baseline at 30 s, 5 min and 30 min outages
- [ ] 8.3 Run the same matrix with the queue on
- [ ] 8.4 Record delivery rate, duplicate rate, shed by tier, ordering compliance and post-reconnect sync latency per run
  - _Requirements: AC-02_
- [ ] 8.5 Write the results into `treklink-docs` as RQ1/RQ2 evidence, with the raw data committed

---

## Phase 9: SOS Robustness Fixes, one commit each, after the queue

> Sequenced after the queue so each measured improvement is attributable to one change (O-003, Q-A10).

- [ ] 9.1 Retransmit the SOS text every Nth beacon, N = 4, configurable
  - _Requirements: REQ-OPT-01_
- [ ] 9.2 Beacon builds its own position packet with `priority = MAX`, so it is no longer `BACKGROUND` and still sends before the first fix
  - _Requirements: REQ-OPT-02_
- [ ] 9.3 Fall auto-SOS enters the beacon loop and sends its opening position at `MAX`
  - _Requirements: REQ-OPT-03_
- [ ] 9.4 Measure the SOS-episode detection rate before and after each of 9.1 to 9.3
- [ ] 9.5 Update `04-firmware-ground-truth.md` §2 and the risk register rows that change status
  - _Requirements: AGENTS.md §2.6_

---

## Phase 10: DoD Audit

- [ ] 10.1 Native unit suite green; zero new build warnings on all chosen envs
- [ ] 10.2 `requirements.md` and `design.md` still match what was built
- [ ] 10.3 D-019 compliance against `design.md` §5
- [ ] 10.4 Update the Mainflow Coverage Matrix, MF-02 Implementation and Tested columns
