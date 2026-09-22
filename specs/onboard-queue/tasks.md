# Implementation Tasks: onboard-queue (firmware)

> Reads with [`requirements.md`](requirements.md) and [`design.md`](design.md). One branch per phase group, `feat/TK-nn-…` off `dev`, per `01-conventions/07-github-workflow-git-conventions.md`.
>
> **Phases 1–4 are pure and testable natively.** No hardware is needed until Phase 5. That ordering is deliberate: the logic that carries the NFRs gets tested on a laptop, and the device is only needed to prove integration.

---

## Phase 0: Blockers — clear before Phase 1 opens

- [ ] 0.1 Measure the actual LittleFS partition size on v2, v3 and v4 from each variant's partition table; confirm the Phase-4 bounds fit
  - v3 is the binding case — ~4 MB flash shared with two OTA app slots
  - **`design.md` §4 records these bounds as unverified. This task is what makes them facts.**
  - _Requirements: REQ-ERR-06_
- [ ] 0.2 Answer Q1 (health-report interval) and Q2 (retransmit interval N) in `requirements.md` §5
  - Q2 blocks Phase 9's measurement design, not Phase 1
- [ ] 0.3 Answer Q4 — bounded or unbounded shed exemption for an open SOS episode
  - An indefinitely beaconing device with no uplink otherwise fills the queue with its own P1 positions and refuses everything else
  - _Requirements: REQ-STA-04_
- [ ] 0.4 Capture golden fixtures from a live v2: one encoded `ServiceEnvelope` each for an SOS text, an SOS-episode position, a routine position, and a device-metrics telemetry frame
  - These are the inputs for every Phase 2 and 3 unit test; capturing them from hardware once removes hand-written protobuf from the test suite
  - Include a device-metrics sample explicitly — the `TELEMETRY_APP` `oneof` variant is not self-describing in JSON (`04-firmware-ground-truth.md` §4)

---

## Phase 1: Foundation & Record Format

- [ ] 1.1 Add the `TREKLINK_ONBOARD_QUEUE` build flag, defaulted on for v2/v3/v4 and forced off for v1
  - v1 compiles MQTT out (`-D MESHTASTIC_EXCLUDE_MQTT=1`), so the queue must compile out with no dangling references
  - _Requirements: REQ-ERR-05_
- [ ] 1.2 Create `src/mqtt/TrekLinkEventQueue.h` with the record struct, `PriorityTier` enum, `QueueStats`, and the public interface of `design.md` §2
  - _Requirements: REQ-UBI-01_
- [ ] 1.3 Implement length-prefixed record encode/decode with a CRC32 trailer
  - _Requirements: REQ-UBI-01, REQ-ERR-03_
- [ ] 1.4 Add every bound and interval as a `build_flags`-overridable constant with documented defaults, following `MESSAGE_HISTORY_LIMIT` (`MessageStore.h:22–24`)
  - _Requirements: REQ-UBI-05_
- [ ] 1.5 Unit test the record round-trip, including a maximum-size envelope and a deliberately truncated tail
  - _Requirements: REQ-UBI-01, REQ-ERR-03_

---

## Phase 2: Classification — pure, no I/O

- [ ] 2.1 Add `TrekLinkSOSHelper::isEpisodeActive()` as a const accessor over the existing beacon state
  - Additive only — do not alter `tickBeacon()` or any send path in this task
  - _Requirements: REQ-EVT-04_
- [ ] 2.2 Implement `classify()` per `design.md` §2.1
  - _Requirements: REQ-EVT-03, REQ-EVT-04_
- [ ] 2.3 Unit test classification across every PortNum × priority × episode-state combination, using the Phase 0.4 fixtures
  - Must cover: undecoded payload variant → P3; `TEXT_MESSAGE_APP` below `MAX` → P3; `POSITION_APP` with episode active → P1 and inactive → P2
  - _Requirements: REQ-EVT-03, REQ-EVT-04_

---

## Phase 3: RAM Tier & Shedding

- [ ] 3.1 Implement the RAM index — flat array of `{seq, tier, offset, len, packetId, episodeActive, state}`
  - _Requirements: REQ-UBI-01_
- [ ] 3.2 Implement `enqueue()` for the RAM tier with monotonic `seq` assignment
  - _Requirements: REQ-EVT-02_
- [ ] 3.3 Implement `dequeueNext()` — minimum tier, then minimum `seq`
  - _Requirements: REQ-EVT-09_
- [ ] 3.4 Implement `commit(seq)` — mark dead in the index, no flash write
  - _Requirements: REQ-EVT-10_
- [ ] 3.5 Implement `shed()` per the decision tree in `design.md` §2.2 — newest within the lowest occupied tier; never a P0; never an entry of an open episode
  - _Requirements: REQ-STA-02, REQ-STA-04_
- [ ] 3.6 Implement the all-P0-at-capacity refusal with its own counter
  - _Requirements: REQ-ERR-01_
- [ ] 3.7 Implement `QueueStats` counters — enqueued, published, shed by tier, `p0_refused`, `flash_write_failed`
  - _Requirements: REQ-UBI-06_
- [ ] 3.8 Unit test flush ordering over a randomised mixed-tier queue, asserting every P0 precedes every P1
  - _Requirements: REQ-EVT-09_
- [ ] 3.9 Unit test the shed tree: P3 shed before P0; all-P0 refusal sheds nothing; episode entries exempt
  - _Requirements: REQ-STA-02, REQ-STA-04, REQ-ERR-01_

---

## Phase 4: Flash Tier & Durability

- [ ] 4.1 Implement the append-only `queue.bin` writer over `SafeFile` / `FSCom`
  - Follow `MessageStore::saveToFlash()` (`MessageStore.cpp:287`) for the `SafeFile` write-then-rename idiom
  - _Requirements: REQ-EVT-05_
- [ ] 4.2 Implement the immediate fsync on P0 enqueue
  - _Requirements: REQ-EVT-06_
- [ ] 4.3 Implement `persistAll()` and register it on the existing shutdown hook alongside `messageStore.saveToFlash()` (`Power.cpp:810`)
  - _Requirements: REQ-EVT-07_
- [ ] 4.4 Implement `restoreFromFlash()` — one sequential scan rebuilding the RAM index, per `MessageStore::loadFromFlash()` (`MessageStore.cpp:265`, `:287`)
  - Must restore the valid prefix and discard a corrupt tail without failing the boot
  - _Requirements: REQ-EVT-08, REQ-ERR-03_
- [ ] 4.5 Implement `compact()` on a dead-fraction threshold and opportunistically at boot
  - _Requirements: REQ-EVT-05_
- [ ] 4.6 Implement flash-failure tolerance — keep the entry in RAM, count it, never crash the MQTT thread
  - _Requirements: REQ-ERR-02_
- [ ] 4.7 Clamp the configured bound against measured free filesystem space at init, with a logged warning
  - Derive from **LittleFS space, never PSRAM** — `StoreForwardModule.cpp:76–85` is the anti-pattern here
  - _Requirements: REQ-ERR-06_
- [ ] 4.8 Unit test restore from a seeded file, a truncated file, and a file with a corrupt CRC mid-record
  - _Requirements: REQ-EVT-08, REQ-ERR-03_

---

## Phase 5: Integrate the seam into `MQTT.cpp` — non-destructive

- [ ] 5.1 Extract the stock enqueue body (`MQTT.cpp:818–831`) verbatim into `enqueueStock()`
  - Behaviour-preserving extraction only. No logic change in this task.
  - _Requirements: REQ-UBI-02_
- [ ] 5.2 Extract the stock drain body (`MQTT.cpp:699–709`) verbatim into `dequeueStock()`
  - _Requirements: REQ-UBI-02_
- [ ] 5.3 Add the `#if TREKLINK_ONBOARD_QUEUE` branch at both sites, delegating to the queue
  - Do **not** touch the connected fast path at `MQTT.cpp:800` or the `proxy_to_client_enabled` branch at `:607`
  - _Requirements: REQ-EVT-01, REQ-UBI-02, REQ-UBI-04_
- [ ] 5.4 Wire `restoreFromFlash()` to run before the MQTT thread's first publish
  - _Requirements: REQ-EVT-08_
- [ ] 5.5 Enforce one publish per tick on the flush path, preserving the 200 ms cadence
  - _Requirements: REQ-STA-03_
- [ ] 5.6 Build both configurations for v2, v3 and v4 and confirm the queue-disabled build is behaviourally identical to baseline
  - _Requirements: REQ-UBI-02_

---

## Phase 6: Health Reporting

- [ ] 6.1 Define the queue-health payload and emit it on `PRIVATE_APP` (256) at the Q1 interval
  - Suppress entirely while the queue is empty and the uplink is up
  - _Requirements: REQ-EVT-12, REQ-UBI-06_
- [ ] 6.2 Persist the counters across reboot
  - _Requirements: REQ-UBI-06_
- [ ] 6.3 Confirm a stock Meshtastic client ignores the packet without error
  - _Requirements: REQ-UBI-03, REQ-UBI-04_

---

## Phase 7: On-Device Verification

- [ ] 7.1 Boot restore with a pre-seeded non-empty `queue.bin` on v2
  - _Requirements: AC-03_
- [ ] 7.2 Power cut 50 ms after a P0 enqueue; confirm the event survives
  - _Requirements: AC-04_
- [ ] 7.3 Fill with P3 telemetry, enqueue a P0, confirm telemetry sheds and the SOS stays
  - **This is AC-01 — the headline before/after.** Run it on the baseline build first and record the stock eviction.
  - _Requirements: AC-01, AC-05_
- [ ] 7.4 Verify stock mobile-app BLE pairing and `proxy_to_client_enabled` relay with the queue enabled
  - _Requirements: AC-09_
- [ ] 7.5 Demonstrate the capacity bound changing from `build_flags` with no source edit, and visible at runtime
  - _Requirements: AC-11_
- [ ] 7.6 Repeat 7.1–7.3 on **v3** hardware
  - v3 has no PSRAM and the tightest flash budget; a pass on v2 proves nothing about it
  - _Requirements: AC-13, REQ-ERR-06_
- [ ] 7.7 Measure write amplification across a 30-minute outage against the ≤1-erase-per-minute target
  - _Requirements: NFR flash wear_

---

## Phase 8: Comparative Measurement (RQ1 / RQ2)

- [ ] 8.1 Build the harness — generate a scripted mixed-tier event stream at a controlled rate, with the uplink cut programmatically and the mesh left up
  - _Requirements: NFR baseline method_
- [ ] 8.2 Run the baseline (queue disabled, i.e. stock) at outage durations 30 s / 5 min / 30 min
  - _Requirements: RQ1, RQ2_
- [ ] 8.3 Run the same matrix with the queue enabled
  - _Requirements: RQ1, RQ2_
- [ ] 8.4 Record delivery rate, duplicate rate, shed-by-tier, ordering compliance and post-reconnect sync latency per run
  - _Requirements: AC-02_
- [ ] 8.5 Write the results into `treklink-docs` as the RQ1/RQ2 evidence, with the raw data committed
  - Per the golden rule — *có làm mới ghi* — the claim and the data land together

---

## Phase 9: SOS Robustness Fixes — separate, individually measurable

> Sequenced **after** the queue on purpose. Landing these together with the queue would make the measured improvement unattributable between three changes.

- [ ] 9.1 Retransmit the SOS discriminator every Nth beacon in `tickBeacon()`
  - Retires firmware-fix candidate #1 and the Critical single-frame risk at source
  - _Requirements: REQ-OPT-01_
- [ ] 9.2 Set `priority = MAX` explicitly on the beacon send path instead of delegating to `PositionModule::sendOurPosition()`, which assigns `BACKGROUND` for a handheld role (`PositionModule.cpp:377–380`)
  - _Requirements: REQ-OPT-02_
- [ ] 9.3 Measure the SOS-episode detection rate before and after 9.1, independently of the queue result
  - _Requirements: REQ-OPT-01_
- [ ] 9.4 Update `04-firmware-ground-truth.md` §2 and the risk register — the Critical row and the S4 beacon-priority row both change status here
  - _Requirements: AGENTS.md §2.6_

---

## Phase 10: DoD Audit

- [ ] 10.1 Full native unit suite green; zero build warnings on all three variants
- [ ] 10.2 Confirm `requirements.md` and `design.md` still match what was built — `design.md` §4's bounds must now be measured values from task 0.1, not estimates
  - Per tenet #6, this is an **audit** that in-pass sync happened, not the first documentation edit
- [ ] 10.3 Confirm D-019 compliance against the three-row table in `design.md` §5
- [ ] 10.4 Update the Mainflow Coverage Matrix — MF-02 Implementation and Tested columns
