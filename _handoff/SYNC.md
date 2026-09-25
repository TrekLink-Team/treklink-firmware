# Handoff log: treklink-firmware cloud session

Append-only. Cloud entries are `C-nnn`, orchestrator entries are `O-nnn`. Never edit an earlier entry.

---

### C-001 | cloud to orchestrator | 2026-09-25T03:40Z | STATUS

Phase A complete. Stopped for approval. No firmware source is modified.

- Branch: `feat/onboard-queue`, cut from `dev@733dd40`. The cloud harness named `dev` as its default branch; the prompt's `feat/onboard-queue` was used instead, because `AGENTS.md` §3 prohibits direct pushes to `dev`.
- Context pack: attached to the session, unzipped to `/tmp/firmware-pack/` (not committed). `PACK.txt`: `treklink-docs@089374b`, `treklink-web@b299218`, `capstone@cecdae7`.
- Full report: `_handoff/phase-a-audit.md` (A1 audit table, A2 spec defects, A3 build results).

Headline findings:

1. Stock direct-mode MQTT drains **one** queued entry per reconnect, not one per 200 ms (`MQTT.cpp:604-636`). The spec's drain design rests on the wrong premise (defect S1).
2. The MQTT queue also holds **peer** packets (`Router.cpp:767-769`), so "own events only" is false (S2), and a peer SOS arrives as `HIGH`, not `MAX`, so the spec would class it P3 (S3).
3. `isEpisodeActive()` has no state to read; the SOS flag lives in three modules and flips after the trigger packets are already queued (S4).
4. The `PRIVATE_APP` health report has no payload on the JSON topic that `gateway-sync` Stage A consumes (S5).
5. A fall auto-SOS never beacons, and an SOS before the first GPS fix sends no beacons (two new firmware-fix candidates).
6. v1's env is `treklink`, not `treklink-v1`. v3's build enables PSRAM and v2's does not, the reverse of the register row. LittleFS: v3 1 MiB, v2/v4 1.5 MiB.
7. No build ran: the PlatformIO registry is blocked by this environment's egress policy (see C-004).

---

### C-002 | cloud to orchestrator | 2026-09-25T03:40Z | REQUEST

Please apply to `treklink-docs` (docs root `_docs/`), through a PR there:

1. `_docs/00-project-context/04-firmware-ground-truth.md`: replace with `_handoff/outbound/treklink-docs/_docs/00-project-context/04-firmware-ground-truth.md`. Its §8 lists all 18 changes. The file is now free of em dashes (it was edited, so §6 of chapter 14 applied).
2. `_docs/00-project-context/03-decisions-and-risk-register.md`: replace with the outbound copy. The only change is five rows appended after the S7 PSRAM row (two correction rows, three new risks). No existing entry was altered, per the ledger rule. The rest of the file still carries pre-2026-09-22 em dashes; no sweep was done.
3. For the `treklink-web` session, not applied here: `gateway-sync` REQ-EVT-06 (cadence detector) cannot fire for a fall SOS or for an SOS raised before the first GPS fix, and REQ-EVT-12/13 are unreachable on the JSON path until Q-A4 below is decided.

---

### C-003 | cloud to orchestrator | 2026-09-25T03:40Z | QUESTION

One batch. My recommendation comes first in each.

- **Q-A1 Build path.** The environment cannot reach the PlatformIO registry, so Phase B's "build every chosen environment before each push" cannot be met here. Options: (a) the leader adds the PlatformIO hosts to the environment's allowed domains (C-004) and I build normally; (b) I write the queue policy as a platform-free core whose Unity tests also compile with host `g++`, run those here, and every firmware env build is done on a local machine before merge; (c) both, (b) now and (a) when available. Recommend (c).
- **Q-A2 Spec approval.** The suite carries no recorded approval. Approve it with the corrections S1 to S14 of `_handoff/phase-a-audit.md`, landed as spec-only commits at the start of Phase B, before any code? Recommend yes.
- **Q-A3 Peer packets (S2).** (a) The TrekLink queue takes peer packets too, under the same policy, and the docs say Stage B also buffers peers heard directly by the uplink node; (b) only `isFromUs` packets use the TrekLink queue, peers stay on the stock 16-entry path. Recommend (a): a peer's SOS is worth saving, and (b) would keep the drop-oldest defect for it.
- **Q-A4 Health report transport (S5, S6).** (a) Publish MQTT-only (never through LoRa), payload a small UTF-8 JSON object on `PRIVATE_APP`, plus an additive `PRIVATE_APP` case in `MeshPacketSerializer` that embeds it as `payload`; stock never emits port 256, so no existing JSON output changes; (b) keep the serializer stock and have the backend subscribe `/2/e/+/+` for port 256 and decode protobuf; (c) defer Phase 6. Recommend (a).
- **Q-A5 Spec open questions.** Accept the proposed defaults: Q1 300 s, suppressed when empty and connected; Q2 every 4th beacon; Q3 append log; Q4 bounded exemption, first 3 and latest 5 positions of an open episode. Recommend yes.
- **Q-A6 Durability model (S7).** Write through to flash immediately for P0 and P1 (the SOS episode); P2/P3 live in RAM and spill when the RAM tier fills; persist all RAM entries on `notifyReboot` and on `Power::shutdown()`. Accept that unclean power loss drops unspilled P2/P3. Recommend yes.
- **Q-A7 Commit point (S12).** Mark an entry published only after the next successful `pubSub.loop()` following its `publish()`, rather than on socket write. Recommend yes. Proxy mode commits on handoff to the phone, as stock does.
- **Q-A8 Ordering while draining (S11).** While the queue is non-empty, new packets go through the queue instead of the connected fast path. Recommend yes.
- **Q-A9 Baseline.** The flag-off build keeps stock behaviour exactly, including the one-per-reconnect drain, as the RQ1 baseline. Recommend yes.
- **Q-A10 New firmware fixes.** Add "fall auto-SOS enters the beacon loop" and "beacon builds its own position packet so no-fix SOS still beacons" as Phase 9 tasks, after the queue. Recommend yes.

---

### C-004 | cloud to orchestrator | 2026-09-25T03:40Z | BLOCKED

Firmware builds and native tests are blocked by the cloud environment's network policy.

- Denied by egress policy: `api.registry.platformio.org`, `api.registry.nm1.platformio.org`, `dl.registry.platformio.org`, `collector.platformio.org`.
- 403 on `https://github.com/meshtastic/platform-native/archive/f566d364204416cdbf298e349213f7d551f793d9.zip`.
- Remedy, leader side: in the cloud environment settings, edit Network access to add those hosts (or `*.platformio.org` and `github.com` archives), or pick a broader access level.

Phase A did not need a build to complete. Phase B does; see Q-A1.
