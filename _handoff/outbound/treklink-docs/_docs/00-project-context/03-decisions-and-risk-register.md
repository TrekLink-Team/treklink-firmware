# TrekLink: Decisions Log & Risk Register

Use this file like a lightweight ADR index. Anything marked **OPEN** blocks the dependent work listed under it, resolve before starting that work, not during.

## Decisions

### D-000: Which register document is authoritative
- **Status**: ✅ Resolved
- **Context**: `Phieu_FA26SE159.docx` (individual registration form, detailed RQs/NFRs/TP1–TP6) and `Phieu_dang_ky_de_tai_TrekLink_FA26_FINAL.docx` (team draft, M1–M5 module framing) describe the same project at different fidelity, with minor differences (supervisor email domain `fe.edu.vn` vs `fpt.edu.vn`; module count 5 vs 5 core capabilities; TypeORM vs Prisma, see D-001).
- **Decision**: `Phieu_FA26SE159.docx` is authoritative for anything it specifies explicitly (RQs, NFRs, FSMs, task packages, experiment design). The FINAL draft is used only to fill gaps it doesn't cover.
- **Owner**: Team lead, confirm with supervisor at Review 1 kickoff that FA26SE159 is the version on file.

### D-001: ORM: Prisma vs TypeORM
- **Status**: ✅ **Resolved** (Prisma ORM)
- **Decision**: Prisma ORM is locked as the team mandate for maximum productivity and type safety across backend services. PostgreSQL schema models and migrations are maintained in `backend/prisma/schema.prisma`.

### D-002: Response envelope standard
- **Status**: ✅ Resolved
- **Decision**: All backend responses use the shape already defined in `API_Design_Template.md`: `{ "result": ... "isSuccess": bool, "statusCode": int, "message": string }`. This supersedes the raw-DTO / `errorDetails[]` pattern from the previously-used `dev-flow.zip` (a different, .NET-project convention), see `02-templates/04-api-endpoint-template.md` and `01-conventions/05-backend-conventions.md`.

### D-003: GitLab → GitHub adaptation is cosmetic, not philosophical
- **Status**: ⛔ **SUPERSEDED by D-009** (2026-09-13). Kept verbatim below for history, do not follow it.
- **Decision** *(superseded)*: Branch topology (`main`/`develop`/`features/Implementation_*`/`features/Design_*`/`hotfix/*`/`release/sprint_x`), commit discipline, and the dual Design+Implementation branch pattern from `Git_Lab_Guide.pdf` carry over unchanged. Only the tool-specific mechanics change: GitLab Issues/Labels/Milestones → GitHub Issues/Labels/Milestones+Projects; GitLab MRs → GitHub PRs (multi-template via `.github/PULL_REQUEST_TEMPLATE/`); GitLab child tasks → GitHub sub-issues/tasklists. See `01-conventions/07-github-workflow-git-conventions.md`.

### D-004: Monorepo vs. multi-repo
- **Status**: ✅ **Resolved** (updated Sep 7, Session 2, repos exist and this is what's actually cloned)
- **Context**: TrekLink has 4 codebases: firmware (inherited, read-only), gateway bridge (Node/TS), backend (NestJS), frontend (React). The org is `github.com/TrekLink-Team`. The original sketch (below, kept for history) considered 4 separate app repos.
- **Original options considered**: (a) 4 separate repos under the org (`treklink-firmware`, `treklink-gateway`, `treklink-backend`, `treklink-web`) with independent CI, or (b) one monorepo with workspaces (`apps/gateway`, `apps/backend`, `apps/web`, `firmware/` as a git submodule).
- **Decision actually taken**: a **hybrid**, 3 repos total under `TrekLink-Team`:
  - `treklink-docs`, this repo (SSOT, docs-only).
  - `treklink-firmware`, inherited SU26 firmware, frozen/read-only this term.
  - `treklink-web`, **one** active application repo containing the Gateway Bridge, NestJS backend, and React frontend as workspace packages (`gateway/`, `backend/`, `frontend/`), rather than 3 separate repos.
- **Rationale**: The firmware is a frozen, inherited dependency, keeping it fully separate avoids accidental edits being graded as "new" work, matching the register's note that firmware effort must be excluded from statistics. Splitting the *active* code further into 3 repos (original option a) was reconsidered: for a 5-person/13-week term, one repo with workspace packages keeps shared TypeScript types (e.g. the `eventId`/DTO shapes gateway and backend both touch), a single CI pipeline, and a single `npm install`, at the cost of coarser per-package branch protection, which `module:*` labels and path-scoped PR reviews (see `01-conventions/07-github-workflow-git-conventions.md`) substitute for.
- **Consequence**: `treklink-web/specs/{module}/` is the one spec root for gateway, backend, and frontend modules alike, see `01-conventions/02-spec-driven-development-workflow.md`. Update that doc's example paths if this ever splits back into separate repos.

### D-005: Field-to-cloud bridge topology
- **Status**: ✅ **Resolved and closed** (2026-09-16, Session 6), staged rollout, **both stages in permanent scope**. Supersedes the earlier "Stage B is tradeable" framing.
- **Context**: The register describes the Gateway/Bridge as **dedicated TrekLink hardware with its own Wi-Fi/cellular uplink** (charter §2, FA26SE159 §b). Session 3 established the physical constraint that closes this: **the Gateway Bridge machine is not carried during the trek.** It sits at basecamp. The guide carries a node, not a laptop.
- **Options considered**:
  1. **Basecamp-fixed bridge**, a designated node USB-tethered to the basecamp machine running `gateway/src`; the SQLite P0–P3 queue buffers the machine's intermittent Wi-Fi uplink. Requires no firmware work.
  2. **Node's built-in MQTT uplink, no bridge**, the guide's node publishes to Mosquitto over its own Wi-Fi using the stock Meshtastic MQTT module already compiled into the frozen firmware. Simplest possible path; **has no buffer anywhere.**
  3. **Fork the Meshtastic mobile apps**, rejected outright. iOS requires Xcode on macOS, which the team does not have; an Android-only fork would split Guide/Customer behavior by platform, and mobile apps are explicitly out of charter scope.
- **Decision**: Build **Option 2 first (Stage A, TP1)**, then **add Option 1 on top (Stage B, TP2)** as an additional ingress adapter plus a hard-wired gateway node. Option 3 stays rejected.
- **Rationale**: Option 2 is the shortest path to an end-to-end demo and needs no serial parser, no hardware tether, and no protobuf decoding. It is only non-throwaway because of the ingress-adapter seam defined in **D-007**, Stage B reuses the entire normalizer and ingestion pipeline and contributes only a new adapter in front of it.
- **⚠️ Known gap, deliberately accepted for Stage A**: Option 2 has **zero offline buffering**. Stock Meshtastic MQTT drops packets when the uplink is down (verified Session 2), and Store & Forward is a mesh-internal, PSRAM-only, ~30-packet chat replay, not a cloud-sync buffer. Therefore, **while only Stage A exists**, the following charter §5 NFRs are unmeasurable and RQ1/RQ2 are unanswerable:
  - Offline recovery delivery rate ≥99% after reconnection (30s–30min loss)
  - Priority-ordering compliance ≥99%, all P0 flushed before P2/P3 on reconnect
  - Gateway→cloud sync ≤5s *when uplink returns* (the ≤5s steady-state figure is still measurable)

  These are restored by Stage B, where the SQLite queue sits in the uplink path and loss is induced by killing the bridge's Wi-Fi.
- **Scoping note that makes Stage B sufficient**: every offline NFR in charter §5 is scoped to the **gateway→cloud** leg, not the **node→gateway** mesh leg. Charter §1 gap #1 reads *"no store-and-forward path to the cloud… there is no Gateway between the mesh and a backend."* Mesh-range loss (a node out of LoRa range of any relay) is an RF-coverage problem, already carried as an RF-reliability caveat in the risk table below, and is **not** claimed as in scope. A basecamp-fixed bridge therefore exercises exactly the leg the research questions measure.
- **Hardware scope** (settled Session 3): **Stage A targets v2/v3/v4 only. v1 is out of the demo set.** v1 compiles MQTT out entirely (`-D MESHTASTIC_EXCLUDE_MQTT=1`, `variants/esp32/treklink_v1_0/platformio.ini:10`), the module is removed at build time, so no runtime configuration can enable it. v2/v3/v4 omit that flag and all carry Wi-Fi silicon, so any of them can serve as the uplink node. Were v1 ever needed, deleting the build flag is a one-line change now that D-008 permits firmware edits.
- ~~**Stage B is tradeable scope**~~, **struck (Session 6).** Stage B is permanent scope. Two independent reasons, either of which alone is sufficient:
  1. **The supervisor put it in the mainflow set.** `Documents/course-material/TrekLink-proposed-mainflow-ducndm.png` defines **MF-02, Field Data → Offline Gateway → Cloud Synchronization**, and draws the Gateway Bridge (Node.js) with its SQLite offline queue and priority-ordered reconnection flush as the substance of that flow. Dropping Stage B would delete a supervisor-specified mainflow. See **D-016**.
  2. **The scope constraint that motivated the trade is gone.** At the 2026-09-13 meeting the supervisor granted the team latitude to set its own scope and tech stack, subject to pitching and defending it. There is no longer a scope-reduction pressure to spend Stage B on.
- **Consequence**: charter §5's offline-recovery and priority-ordering NFRs and **RQ1/RQ2 stay binding and must be delivered.** Stage A remains the first increment, not a substitute, never present Stage A as satisfying the offline-recovery NFR.
- **Blocks**: nothing, `specs/gateway-sync/{requirements,design,tasks}.md` are written against this decision. Stage B's isolation in Phase 9 is retained as sequencing, no longer as a removal seam.

### D-006: `eventId` redefinition: the charter's scheme is not constructible
- **Status**: ✅ **Resolved** (Session 3), supersedes charter §2 and `01-conventions/04-architecture-conventions.md` §3
- **Context**: Charter §2 and the architecture conventions both specify `eventId = DeviceID + SessionID + SequenceNumber` as the backend idempotency key. Direct inspection of the frozen firmware shows **neither `SessionID` nor `SequenceNumber` exists anywhere**, not in the TrekLink modules, not in the Meshtastic packet format. The only per-packet identifier transmitted is `MeshPacket.id`, and `Router.cpp:168` (`generatePacketId()`) shows it is a 10-bit rolling counter OR'd with 22 random high bits, re-seeded randomly at every boot. It is a flood-dedup token, **not** a monotonic sequence. The charter's key cannot be built from what the hardware actually sends.
- **Options considered**:
  1. **Split the key** into a packet-level dedup key and an episode-level correlation key.
  2. **Single content-addressed key**, `hash(from : rx_time : portnum : payload)`, rejected: the SOS beacon retransmits every 5s (then 30s) with a fresh `rx_time`, so one SOS episode would mint a new key per tick and create N Incidents from one fall.
  3. **Bridge synthesizes session + sequence**, rejected: the key would then describe the *bridge*, not the device. Restart the bridge, replay the same packets, and every key is new; deduplication becomes a silent no-op, which is precisely the failure the 10× replay NFR exists to catch.
- **Decision** (Option 1):
  - **Packet dedup key**, `GatewayEvent.eventId = sha256(nodeNum : packetId)`. Satisfies charter §5's *"0 duplicate Incidents for repeated eventId delivery (10× replay test)."* Enforced by a unique index; conflict is a no-op insert inside the ingestion transaction.
  - **Episode correlation**, *not* a hash. On SOS ingest, look up an open `Incident` for the same device whose `lastEventAt` falls inside the episode window; append if found, create if not. An earlier draft used `hash(nodeNum : timeBucket)`, which was discarded: a bucket boundary falling mid-episode splits one SOS into two Incidents, the exact failure the key exists to prevent.
- **Consequences**:
  - `backend/prisma/schema.prisma`: `Incident.eventId @unique` currently serves as *both* keys and must be split, `Incident` gains `lastEventAt` and an index on `(deviceId, status, lastEventAt)`, and references its originating `GatewayEvent` by FK rather than carrying a duplicate unique string.
  - **`01-conventions/04-architecture-conventions.md` §3 still states the old formula and is now contradicted by this decision. It must be updated before any `gateway-sync` implementation branch opens.**
  - Charter §2's wording stands as the *intent* (a natural, device-derived idempotency key); this decision records the constructible form of it.
- **Reopened by D-008** (firmware is editable): adding a boot-`sessionId` and a per-packet `sequenceNumber` firmware-side would make the charter's original formula constructible and would give a genuinely monotonic sequence, strictly better than hashing a randomized packet id, and it would let the platform *detect gaps* (a missing sequence number proves loss) rather than merely deduplicating what arrives. **The split key above remains correct and functional with or without it**; treat a firmware sequence as a layered upgrade to evaluate next session, not a prerequisite for TP1.

### D-007: Canonical event envelope + Strategy-based normalizer (the ingress seam)
- **Status**: ✅ **Resolved** (Session 3)
- **Context**: Stage A (node MQTT) and Stage B (serial bridge) do **not** speak the same wire format. The node's MQTT module publishes to a hardcoded topic shape, `<root>/2/e/<channelId>/<nodeId>` carrying a `ServiceEnvelope` protobuf, or `<root>/2/json/<channelId>/<nodeId>` carrying JSON when `json_enabled` (`src/mqtt/MQTT.cpp:423–430`, `:798`). `<root>` is configurable; `/2/e/` and the payload type are not. Meanwhile `gateway/src/mqtt/mqtt-client.ts` publishes `treklink/events/priority/{0..3}` with a TrekLink-shaped payload. If the backend parses Stage A's format directly, Stage B's arrival forces a rewrite of the ingestion layer, and D-005's staged plan quietly becomes throwaway work.
- **Decision**: The backend ingests exactly one internal type, **`TrekLinkEvent`**. Between the wire and the domain sit two seams:
  1. **Ingress adapters** (`MeshIngressAdapter`), one per topology, responsible only for getting bytes off a transport and producing a `RawMeshPacket`. Stage A ships `MqttJsonIngressAdapter`; Stage B adds `SerialBridgeIngressAdapter`. Adapters hold no domain logic.
  2. **Normalizer** (`MeshNormalizerService`), the Strategy-pattern context. It resolves a `PacketNormalizationStrategy` by PortNum from an injected registry and delegates. Strategies are pure (no I/O, no Prisma) and each owns one PortNum's mapping to a domain event kind and priority tier. Adding a PortNum means adding a strategy class and registering it; the normalizer itself never changes (Open/Closed).
- **Rationale**: the normalizer holds ~all the genuinely hard logic (SOS prefix parsing, priority assignment, `eventId` derivation, episode correlation) and is 100% shared between stages. Purity makes it unit-testable without a broker, a device, or a database, which is what makes the charter's dedup/ordering/concurrency NFRs cheap to test rather than integration-only.
- **Stage A transport config, frozen**: `json_enabled = true`, `encryption_enabled = false`, `root = "treklink"`. JSON removes any protobuf dependency from NestJS, and the JSON serializer covers the three PortNums TrekLink actually emits, `TEXT_MESSAGE_APP`, `POSITION_APP`, `TELEMETRY_APP` (`src/serialization/MeshPacketSerializer.cpp:28`, `:208`, `:56`). `encryption_enabled = false` is required so the broker receives decrypted packets; the field's own comment in `module_config.pb.h` notes decrypted output exists for exactly this purpose. The alternative is reimplementing Meshtastic's AES-CTR channel crypto in NestJS.
- **Known limitation of the JSON path**: the JSON envelope (`MeshPacketSerializer.cpp:410–424`) carries `id`, `timestamp`, `to`, `from`, `channel`, `type`, `sender`, `payload`, `rssi`, `snr`, `hops_away`, but **not `MeshPacket.priority`**. The SOS position packet is sent with `priority = MAX` (`TrekLinkSOSHelper.cpp:119`) and is therefore indistinguishable from a routine position report on this path. Handled by backend episode correlation with a backward grace window (see `specs/gateway-sync/design.md` §2.4). Switching to the `/2/e/` protobuf topic would recover `priority` directly and is logged as the deferred precision upgrade.

### D-008: The firmware is editable this term (supersedes the "frozen / read-only" framing)
- **Status**: ✅ **Resolved and closed** (2026-09-16, Session 6). All three sub-questions answered; no part of this decision remains open.
- **Context**: `README.md` §1, charter §2, and D-004's rationale all describe `treklink-firmware` as *"Frozen / read-only this term. Branch-protected. Not a deliverable."* Sessions 1–3 reasoned under that constraint and, on its strength, classified several firmware-rooted defects as unfixable-by-design. **The team lead has confirmed this is wrong**: the team owns the repo, can modify and build it, and can reflash the physical units.
- **Decision**: firmware modification is **technically available** and may be used to fix defects that cannot be worked around from the platform side. All prior "unfixable without touching frozen firmware" conclusions are void and must be re-derived.
- **The distinction that still matters**, three separate claims were bundled under one word:
  1. *Can we edit it?*, **Yes.** Settled.
  2. *Is firmware work in scope?*, **Yes** (answered 2026-09-13, supervisor). Firmware **enhancement and integration** is an accepted development path, not the out-of-scope "firmware redesign." Extending the firmware so the platform integrates with it more deeply, a custom PortNum, an explicit SOS discriminator, a boot `sessionId` + per-packet `sequenceNumber`, beacon priority, is treated as building the product, not rebuilding the inherited component. The charter's exclusion is read narrowly, as prohibiting a rearchitecture of the mesh stack.
  3. *Does firmware effort count toward graded deliverables?*, **Yes** (answered 2026-09-13, supervisor), with the caveat that assessment is not code volume alone. This supersedes D-004's rationale citing "the register's note that firmware effort must be excluded from statistics"; that note is treated as an accounting convention for source statistics, not as a bar on credit.
- **Working rule (Session 6)**: firmware changes are a **first-class option**, evaluated on merit against the platform-side alternative rather than treated as a last resort. Prefer whichever is more robust for the same cost; where the firmware fix is strictly better, as with the SOS discriminator and beacon priority, take it. Keep each change individually justified and logged here. Still excluded: rearchitecting the Meshtastic mesh stack.
- **Reopens**:
  - **D-006**, a firmware-side boot-`sessionId` + per-packet `sequenceNumber` would make the charter's original `eventId` formula constructible. The split key stays valid regardless; this would be a layered upgrade.
  - **The Critical SOS risk below**, the single-unacknowledged-text-frame failure is now firmware-fixable (custom PortNum, `want_ack`, or retransmitting the discriminator with each beacon) rather than mitigable only by heuristic.
  - **D-007's SOS discrimination**, a real custom PortNum would retire the `printf`-format string parser entirely.
  - **v1 MQTT exclusion**, a one-line build-flag removal, though v1 is now out of the demo set anyway (see D-005).
  - **New (Session 4), SOS beacon priority.** `TrekLinkSOSHelper::broadcastPosition()` retransmits via `PositionModule::sendOurPosition()`, which sets `priority = BACKGROUND` for a handheld role (`PositionModule.cpp:377–380`), only the very first position packet at trigger time is `MAX`. Every beacon for the rest of an episode is the *lowest*-priority traffic on the mesh. Candidate fix: have the beacon path set `priority = MAX` (or at least `RELIABLE`) explicitly instead of delegating to the generic broadcast method. See `04-firmware-ground-truth.md` §2.
- **Action** (owner: team lead): none outstanding, both questions answered. Continue to log every firmware change in this register, and carry the firmware work in the same backlog and Progress Log as platform work so the contribution evidence is uniform.

### D-009: Git, tracking & review model v2 (supersedes D-003)
- **Status**: ✅ **Resolved** (2026-09-13, Session 5)
- **Context**: D-003 ratified the school's `Git_Lab_Guide.pdf` model wholesale. Auditing it against reality found it was never actually implemented and could not be: **no repository has ever had a `develop` branch**, yet `develop` appeared in 34 documentation files, making every documented git command broken. Three documents disagreed with each other on the branch model (`07-*.md` §3.2 and D-003 mandated dual Design/Implementation branches; `09-*.md` §1 said they were "not adopted"). The GitHub-Issues-as-tracker assumption conflicted with the team's actual use of Jira.
- **Options considered**:
  1. **Fix reality to match the docs**, create `develop`, adopt dual branches, drop Jira. Rejected: dual branches double PR overhead for a 5-person/13-week term, and the team already runs Jira.
  2. **Fix the docs to match reality, keeping GitLab's philosophy**, chosen.
  3. Leave the contradiction and rely on tribal knowledge. Rejected: agents read the docs literally and will follow the broken commands.
- **Decision**:
  - Integration branch is **`dev`**. Branches: `main`, `dev`, `feat/*`, `fix/*`, `hotfix/*`, `docs/*`, `chore/*`.
  - **`release/*` dropped**, `main` is the release.
  - **Dual Design/Implementation branches killed.** One branch per unit of work; spec commits land before implementation commits on the same branch, which preserves review independence at half the overhead.
  - Branch naming: `{type}/{JIRA-KEY}-{short-kebab-desc}`, e.g. `feat/TK-45-device-registration`. The Jira key is what makes the org-level Jira↔GitHub integration auto-populate each card's Development panel.
  - Commits: **Conventional Commits**, all standard types, Jira key as scope, `feat(TK-45): ...`. Scope omitted when genuinely general. **Not CI-enforced**: commit hygiene must never block a delivery.
  - Merge: **Rebase & merge** default, **Squash & merge** when multi-commit, **merge commits prohibited**. Delete source branch except `dev` → `main`.
  - **No direct pushes to any branch, by anyone, including the leader.**
  - **Jira is the single work tracker.** GitHub Issues hold daily reports and standalone bugs only. GitHub Milestones retired; `points:*` labels retired in favour of Jira story points on the **Fibonacci** scale (1/2/3/5/8/13/21), corrected 2026-09-17; an earlier revision published a non-standard base-5 scale that the backlog was never estimated against.
  - Bugs route three ways: Jira `BUG` **status** (defect found reviewing an active story) / GitHub Issue `type:bug` (standalone, never enters Jira) / Jira `Bug` **work item** (only if >0.5 day or it changes a spec).
  - CI is an **indicator, not a gate**, red CI does not block merge, but the leader must approve any red merge and the reason goes in the PR.
  - PR self-approval permitted **only** for <50-line, non-behavioural housekeeping. The PO is not exempt and must run an independent AI review pass on their own non-trivial PRs.
  - **English only** in every repository artifact. Vietnamese permitted when prompting an AI agent, and by exception in daily reports.
- **Consequence**: `01-conventions/` rewritten as Conventions v2; `07-*.md` fully rewritten; new chapters `10` (Jira), `11` (AI-first), `12` (communication). Two PR templates collapsed into one. `treklink-web/docs/conventions/` deleted (D-011). The generated Developer Handbook PDF is built from these files.
- **Owner**: Team lead.

### D-010: Neon as the shared managed Postgres
- **Status**: ✅ **Resolved** (2026-09-13)
- **Context**: `docker-compose.yml` provisions a local Postgres per developer, which is correct for isolated unit/integration work but gives the team no common database to co-develop against. Integration bugs that only appear with shared data (FK collisions, migration ordering, seed drift) surface late, typically during demo prep.
- **Decision**: Adopt **Neon** as the managed Postgres for shared **dev** and **prod/demo** environments. Local Docker Postgres remains the default for unit tests and CI (fast, hermetic, no network).
- **Consequence**: Each member installs the **Neon MCP connector** from the Claude skills store and authorises it under their own account. Connection strings live in each developer's `ignore/envs/` and are **never committed**. `DATABASE_URL` in CI continues to point at the ephemeral service container, not Neon.
- **Open**: branch-per-developer Neon databases vs. one shared dev database, decide before the first cross-module integration sprint.
- **Owner**: Team lead + LongLP (data lane).

### D-011: Single canonical convention set; vendored copies deleted
- **Status**: ✅ **Resolved** (2026-09-13)
- **Context**: `treklink-web/docs/conventions/` held a verbatim, unreconciled copy of the author's generic `dev-flow` template, 12 files with zero TrekLink-specific content, still teaching `develop`, bracket-tag commits, and the dual-branch model. `treklink-web/AGENTS.md` explicitly directed agents to **fall back to it**, so an agent opened on `treklink-web` alone would have been actively taught the wrong rules.
- **Decision**: **Delete it.** `treklink-docs/_docs/01-conventions/` is the single canonical convention set for all three repositories. No repo vendors a second copy. A single `AGENTS.md`, identical in content, is placed in `capstone/`, `treklink-docs/`, `treklink-web/`, and `treklink-firmware/`, and points at the canonical folder.
- **Rationale**: A duplicated convention set does not merely drift, it drifts *silently*, and the copy is what gets read when the sibling layout isn't present. The previous fallback was worse than having no fallback, because a wrong answer delivered confidently costs more than a missing one.
- **Consequence**: Agents must be opened on the `capstone/` parent folder. An agent opened on a single repo will find `AGENTS.md` pointing at a path it cannot resolve, and must say so rather than guessing, which is the intended failure mode.
- **Owner**: Team lead.

### D-012: Map provider: Goong Maps over MapLibre GL; OpenStreetMap tiles prohibited

- **Status**: ✅ **Resolved** (2026-09-16, Session 6)
- **Context**: `frontend/src/widgets/LiveMapWidget/LiveMapWidget.tsx:18-19` renders the standard
  OpenStreetMap tile server (`https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png`). The team lead
  inspected every OSM base layer and confirmed each renders the Hoàng Sa and Trường Sa archipelagos
  under Chinese toponyms. Under Vietnamese law that makes the rendered product unlawful to publish.
- **Legal basis**: **Nghị định 174/2026/NĐ-CP, Điều 93 khoản 3 điểm a**, in force **1 July 2026**,
  fine **30–40 million VND** for *"Đăng, phát, sử dụng hình ảnh bản đồ Việt Nam không thể hiện đầy
  đủ hoặc thể hiện sai chủ quyền quốc gia."* It raises the 20–30M VND of Decree 15/2020/NĐ-CP Art.
  99(3)(b). Ancillary sanctions include confiscation of the equipment used, forced takedown of the
  content and its links, and **forced removal of the application**. Note the wording penalises
  *failing to represent fully* as well as *misrepresenting*, omission is sufficient; a nine-dash
  line is not required for liability. Enforcement against software and devices is established
  practice (imported-vehicle GPS confiscations, Hải Phòng 2019; 25M VND fine to the Oceanman 2023
  organiser under the predecessor article).
- **Options considered**:
  1. **Goong Maps via MapLibre GL**, chosen. Vietnamese provider (IMAP JSC), servers in VN and SG,
     free tier of $100 standing credit with 30,000 requests/month, 180 req/min and 1,000 map loads.
  2. **VietMap**, rejected on cost. Technically the closest fit (it publishes a documented
     `@vietmap/vietmap-gl-leaflet` wrapper that would have preserved the Leaflet API and a raster
     XYZ satellite endpoint), but it is not free.
  3. **Viettel Maps**, rejected on capability. Its public documentation exposes only a Static Map
     image endpoint, an `<iframe>` embed, Place and Routing. No XYZ tile layer and no GL style are
     documented, so it cannot back an interactive operations map.
  4. **Self-hosted sovereignty-corrected tiles**, rejected. Tile pipeline and storage cost far
     exceed a 13-week budget for no graded benefit.
- **Decision**: **Goong Maps rendered through MapLibre GL JS.** This replaces Leaflet as the map
  rendering library, Goong publishes GL styles, not an XYZ raster layer, so Leaflet is no longer
  the right host. Style URL form:
  `https://tiles.goong.io/assets/goong_map_web.json?api_key={MAP_KEY}`; variants
  `goong_map_web` (full icons), `goong_map_highlight` (minimal icons), `goong_satellite`.
  Integration is via the `maplibre-gl` CDN bundle, Goong documents no npm package of its own.
- **Two distinct credentials**: a **Map Key** authenticates style/tile URLs; a separate **API Key**
  authenticates Autocomplete, Direction, Geocoding, Distance Matrix and Place Detail. Do not
  conflate them.
- **Provider-agnostic by construction** (team lead, Session 6): the map layer must not hardcode
  Goong. Provider identity, style URL, key and default viewport are configuration, so a future
  swap to another GL-style provider is a config change and not a code change. See **D-015**.
- **Consequences**:
  - Leaflet is removed from the stack. Ten documents name it, `01-project-charter.md` (×2),
    `01-conventions/00-index.md`, `04-architecture-conventions.md`, `06-frontend-conventions.md`,
    `02-roadmap-and-milestones.md`, `AGENTS.md` (×4 copies), backlog story **US-055** via
    `build_backlog.py`, and `Documents/reports/Report1_Project_Introduction_DRAFT.md` §6.1, all must be
    updated in the same pass.
  - The API key is visible in the browser by design. Mitigate with an HTTP-referer allowlist and a
    per-IP rate limit on the key, and record that mitigation, it is the direct answer to the
    "API key exposed in source" failure mode in the faculty fault handbook §10.
  - **Acceptance test, mandatory before Review 1**: load the operations map over Hoàng Sa
    (~16.5°N, 112.0°E) and Trường Sa (~9.7°N, 114.0°E), confirm Vietnamese toponyms and correct
    sovereignty depiction, and file the screenshots as evidence. This is a team-performed check;
    it has **not** been carried out yet.
- **Owner**: Team lead (schema/UI layout lane) + LongNN (frontend lane).

### D-013: Execution model: SEP490 2026 mainflow-incremental, retaining SWP490 practice

- **Status**: ✅ **Resolved** (2026-09-16, Session 6)
- **Context**: `02-roadmap-and-milestones.md` was derived from `SWP490_Lo-trinh-Capstone_v1.0.pdf`.
  The Fall 2026 course issues `SEP490_Student_Project_Execution_Schedule.pdf` (50pp) and
  `SEP490_Huong_dan_nhanh_cho_sinh_vien.pdf`, which describe a materially different model: gates at
  W4/W8/W13 plus final submission at W15, four Common Meetings (W1, W3, W7, W12), a weekly Group
  Meeting, and **incremental delivery per Mainflow** rather than by TP package or Iteration.
- **Decision**: the **SEP490 schedule is the spine**. TP1–TP6 and the Iteration 1/2/3 framing are
  demoted to a historical appendix and mapped onto mainflow ownership; they no longer drive planning.
  Practices from SWP490 that the SEP490 documents do not contradict are **retained deliberately**
  because they are good practice, the phase discipline, the deliverable ledger shape, and the
  Week-6 scope lock (which SEP490 does not mention but which remains real).
- **The dual council is real but informal** (team lead, Session 6): "Hội đồng 1.1 / 1.2" is the
  *Hội đồng kín* naming, never published as an official document and conveyed only through the
  supervisor's outline. It maps cleanly onto the SEP490 calendar and is **not** a separate pair of
  events: **Hội đồng 1.1 = Faculty Council, W13** (Gate 3, defence eligibility);
  **Hội đồng 1.2 = Final Submission & Defense, W15**. Record it in internal planning; cite only the
  SEP490 names in graded documents.
- **Retake path**: failure at either council moves the group to a council in the following term,
  ~March 2027. This is why the registration form's stated duration reads 09/2026 – 03/2027 while
  the working plan ends in December 2026. **Plan for December.**
- **Known contradictions inside the SEP490 source**, resolved as follows and flagged wherever cited:
  - Review 1 at **W4** (Roadmap table, Master Schedule 5.2, quick guide) vs W3 (§4 prose item 2)
    → **W4** adopted, two sources to one.
  - "Two mainflows complete at W7" (§4, Master Schedule 5.1) vs "Sprint 1 (W8) → MF-01, MF-02"
    (Sprint Plan) → the **stricter W7** reading adopted; W8 is the confirmation demo.
- **Owner**: Team lead.

### D-014: Tracking artifacts: the SEP490 workbook and Progress Log

- **Status**: ✅ **Resolved** (2026-09-16, Session 6)
- **Context**: SEP490 §§27–37 name ten tracking artifacts and recommend consolidating them into one
  8-sheet workbook. `Documents/tracking/Report3_Project Tracking.xlsx` (WBS / Issues / Defects / Q&A) is the
  2021 artifact and covers a strict subset. The course also ships
  `templates/Mã nhóm_Progress_Log.xlsx`, named as **the** primary evidence for Individual
  Contribution, a dimension on which an individual member can fail even when the group passes.
- **Decision**: adopt both.
  1. **`GFA26SE55_Progress_Log.xlsx`**, one file for the whole term, from the course template.
     Updated **before every Group Meeting**. Per-member, per-week, tied to a named deliverable;
     "continued working on the project" is not an entry.
  2. **An 8-sheet tracking workbook**, `01_MASTER_SCHEDULE`, `02_WEEKLY_TASK_TRACKER`,
     `03_DELIVERABLE_TRACKER`, `04_TRACEABILITY_MATRIX`, `05_REVIEW_ACTION_TRACKER`,
     `06_RISK_ISSUE_CONFIG`, `07_FINAL_READINESS_CHECKLIST`, `08_INDIVIDUAL_CONTRIBUTION_LOG`.
  3. The **Mainflow Coverage Matrix** is the single sprint-tracking instrument the supervisor uses
     to confirm progress in the weekly Group Meeting. It is authoritative over any other view.
- **Risk register interface**: the Risk & Issue Log sheet adopts **the school's columns**
  (`ID · Risk/Issue · Impact · Probability · Mitigation/Action · Owner · Deadline · Status`) rather
  than a composite exposure score. The register below stays the engineering source of truth; the
  workbook sheet is its reporting projection and must be regenerated from it, never edited apart.
- **Consequence**: `Project Weekly Report_GroupName.xlsx` is retired. The old Project Tracking
  sheets fold into `02` and `06`.
- **Owner**: Team lead.

### D-015: Business parameters are configuration, never constants

- **Status**: ✅ **Resolved** (2026-09-16, Session 6)
- **Context**: the faculty fault handbook ranks hardcoded business parameters the **#2 cause of
  failure**, and its list of most-asked council questions opens with *"can this number be changed?
  Demo it for me now."* SEP490's W11 Definition of Done requires "tham số nghiệp vụ cấu hình được"
  and a maintained **Configuration Matrix**.
- **Values already hardcoded** (found by inspection, Session 6):
  `gateway/src/queue/priority-queue.ts:50` peek limit `10` · `gateway/src/mqtt/mqtt-client.ts:20`
  `reconnectPeriod: 3000` · `:41` flush batch `20` · `:49` topic prefix
  `treklink/events/priority/` · `frontend/src/widgets/LiveMapWidget/LiveMapWidget.tsx:6`
  `DEFAULT_CENTER`.
- **Values that must be born configurable** (not yet written): SOS beacon cadence (5 s for the first
  minute, then 30 s), episode-correlation window and backward grace, cadence-anomaly threshold
  (N positions in window W), P0–P3 tier mapping, gateway→cloud sync target (≤5 s), battery warning
  thresholds, rental rate, deposit, late fee, damage fee, and the map provider/style/viewport
  (D-012).
- **Decision**: no business parameter appears as a literal in source. Each is registered in the
  Configuration Matrix with parameter, current value, location, configurable (Y/N), tested, and a
  demo path. A parameter that cannot be changed and shown changing during a demo is treated as a
  defect, not as a style preference.
- **Owner**: Team lead (schema) + each module owner for their own parameters.

### D-016: The five Main Flows are supervisor-specified and binding

- **Status**: ✅ **Resolved** (2026-09-16, Session 6)
- **Context**: SEP490 organises all delivery from W3 to W12 around Mainflows, and the supervisor
  supplied the set for this project directly as `Documents/course-material/TrekLink-proposed-mainflow-ducndm.png`.
  They are not a format suggestion; they are the units the Mainflow Coverage Matrix tracks, the
  units demoed at each Iteration review, and the units the council evaluates.
- **Decision**: adopt exactly these five, with these identifiers.

  | MF | Name | Substance |
  |---|---|---|
  | **MF-01** | Booking → Rental → Trip Preparation | Customer browses package, submits booking, reserves device → Staff reviews, confirms, allocates device, assigns guide, generates rental agreement, checks out device → Guide receives device and prepares |
  | **MF-02** | Field Data → Offline Gateway → Cloud Sync | Device (SOS/GPS/telemetry) over LoRa mesh → Gateway Bridge with SQLite priority queue while offline → MQTT → NestJS; P0–P3 processing with idempotency; priority-ordered flush on reconnection |
  | **MF-03** | SOS → Incident → Emergency Response | SOS broadcast → gateway → idempotency check on `eventId` → Incident created if absent → FSM Detected → Acknowledged → In Progress → Resolved → Closed, each transition recording actor, timestamp and note → WebSocket notification to Staff and Guide |
  | **MF-04** | Real-Time Trip Monitoring | Device → gateway → MQTT → backend → WebSocket → dashboard showing active trips, device positions, battery status, incident alerts, last-seen time, for Admin / Staff / Guide |
  | **MF-05** | Return → Inspection → Billing → Maintenance | Check-in, return inspection, charge calculation, payment, rental close; device FSM Available → Reserved → Rented → In-Field → Returned → Maintenance → Retired |

- **Backlog mapping**: MF-01 ← E3 + E2 + E1 · MF-02 ← E4 · MF-03 ← E5 · MF-04 ← E5 (frontend) ·
  MF-05 ← E6 + E2. **E7 (DevOps) and E8 (Research & Evaluation) are cross-cutting and are
  deliberately not mainflows**, state this explicitly wherever the Coverage Matrix is presented,
  so the omission does not read as an oversight.
- **Consequences**:
  - MF-02 makes the offline buffer non-negotiable, see D-005.
  - MF-03 prints "idempotency check (using eventId)" on the supervisor's own diagram, which turns
    D-006's split key from an internal design note into a **demo obligation**: a replay must be
    shown producing exactly one Incident.
  - MF-05 and MF-01 together make the 7-state device lifecycle and the rental lifecycle
    demonstrable end-to-end, which is what the W13 council grades.
- **Owner**: Team lead.

### D-017: Diagram and figure conventions; Mermaid pinned at 12.0.0

- **Status**: ✅ **Resolved** (2026-09-17, Session 6)
- **Context**: the Main Flow diagrams were authored as `flowchart` blocks with `subgraph` groupings.
  That *looks* like a swimlane and is not, the groups do not constrain layout, so the actor
  partition is decorative. The course supplies actor-partitioned swimlanes as its worked example
  (`Documents/templates/Main flows_ex02.jpg`) and the supervisor's own TrekLink diagram uses that
  form. Separately, nothing in the pipeline checked whether a diagram was *legible once printed*:
  the handbook carried figures that render at 1.58 pt on A4.
- **Decision**:
  1. **Multi-actor process flows are Mermaid `swimlane-beta`**, one `subgraph` per actor lane,
     orientation `TB`. Plain `flowchart` is reserved for structures with no actors, context
     diagrams, feature trees, architecture diagrams.
  2. **Mermaid is pinned at 12.0.0.** `swimlane-beta` requires ≥ 11.16.0 and did not exist in the
     previous pin of 10.9.1. All 12 pre-existing diagrams were re-rendered at 12.0.0 and none
     regressed.
  3. **Figure placement is computed, not authored.** `plan_figure` in `build_handbook.py` renders
     each diagram headless, measures its viewBox and smallest rendered label, and chooses inline or
     a counter-clockwise rotated plate, whichever affords the larger scale. Both axes are always
     constrained, so nothing clips.
  4. **A 7 pt legibility floor**, with the escape hatch that a figure below it is a note rather than
     a build failure; the fix is to re-source the diagram, and any carried exception is recorded in
     `01-conventions/13-diagram-and-figure-conventions.md`.
  5. **Monochrome.** No figure may depend on hue, these documents are printed and photocopied.
  6. Every figure carries a number and a caption and is referenced before it appears. Numbering is
     per source document; the build renumbers continuously across the handbook and rewrites the
     in-text references.
- **Consequences**:
  - Diagrams are **pre-rendered to inline SVG** before pagination. Previously mermaid rendered in
    the browser during `--print-to-pdf`, which meant layout could not know a figure's size until
    after pagination had been decided, computed placement is impossible that way.
  - Rotation happens **inside the SVG coordinate system**, never as a CSS transform: a transformed
    box paints in one place and paginates in another, which prints content on the wrong page.
  - Three pre-existing figures were below the floor and were re-sourced by splitting: the session
    lifecycle sequence diagram, the Jira delivery loop, and the AI session workflow. The corpus is
    now **27 figures, all above the floor**, lowest 7.40 pt.
  - The CI "raw mermaid source" text check was removed. Its markers (`commit id:`, `[*] -->`,
    `-->|`) now all appear in the conventions prose that documents them, so they false-positive.
    CI validates figure numbering instead; an unrenderable diagram is already a hard build error.
- **Owner**: Team lead.

### D-018: Three-tier buffering topology; the on-device durable queue is inserted as Stage B

- **Status**: ✅ **Resolved** (2026-09-17, Session 7), **refines D-005, does not supersede it.** D-005's staged-rollout principle and its permanent-scope conclusion both stand.
- **Context**: D-005 described two stages, node MQTT with no buffer (A), then a basecamp bridge holding the SQLite queue (B). Session 7 inspection of `src/mqtt/MQTT.{h,cpp}` found that **the node already has a queue**, and that it is actively harmful: 16 entries, RAM-only, strict FIFO, **discard-oldest** on overflow (`MQTT.cpp:821–823`). Because eviction takes the oldest entry, the single `"SOS - …"` text frame that identifies an episode is **the first thing dropped** during an outage, the exact inverse of MF-02's binding priority rule. Full evidence: `04-firmware-ground-truth.md` §4.1.
- **Decision**, three tiers, delivered in this order:

  | Stage | Where | Buffer | Status |
  |---|---|---|---|
  | **A** | Guide's node publishes direct to MQTT over its own Wi-Fi | the stock 16-entry RAM FIFO, i.e. effectively none | **already exists, pure configuration, nothing to build** |
  | **B** | **On the device (firmware)**, RAM tier, then a flash tier | priority-ordered, reboot-proof, bounded, shed-aware | **built now; this term's focus** |
  | **C** | Basecamp gateway, node tethered to a laptop or phone | the large central queue, aggregating the whole local mesh | built after B; form settled by **D-020** |

- **Naming note, to stop a rename from creating drift**: what documents written before Session 7 call *"Stage B, the basecamp bridge with the SQLite queue"* is **now Stage C**. Stage B is the new on-device work. Every `[B]`-tagged requirement in `gateway-sync/requirements.md` predating this decision refers to Stage C and is retagged in the same pass.
- **Rationale**: Stage B is chosen ahead of Stage C on three independent grounds.
  1. **It fixes a defect rather than adding a layer.** Stage C buffers the gateway→cloud leg; it does nothing about a node that evicts its own SOS 80 seconds into an outage. Those are different failures and only Stage B addresses the second.
  2. **It needs no hardware the team does not already have.** Stage C needs a basecamp machine plus a tether; Stage B needs a node and a broker.
  3. **It is the strongest available answer to the council's individual-contribution question.** A cited before/after on inherited firmware is a materially better claim than new application code.
- **What Stage B does not do, stated so it is never overclaimed**: an on-device queue buffers only *that node's own* events. It cannot buffer for peer nodes that never reach Wi-Fi. Multi-node aggregation is Stage C's job and Stage B is not a substitute for it, the same relationship D-005 established between A and B.
- **Consequences**:
  - RQ1/RQ2 become answerable **this term, against a real baseline**, the unmodified firmware at its current commit (see D-019's preservation requirement, which is what makes the baseline reproducible).
  - The risk row *"Stage A has no offline buffer, so RQ1/RQ2 cannot be evaluated until Stage B lands"* is downgraded: Stage B lands first and is the smaller of the two.
  - `specs/gateway-sync/*` gains Stage-B requirements; `treklink-firmware/specs/onboard-queue/*` is created as the firmware-side spec suite, following the convention that specs live in `specs/{module}/` inside the repo that holds the code (`01-conventions/02-spec-driven-development-workflow.md` §2).
- **Owner**: KhoaDD (MF-02 owner).

### D-019: Firmware changes are additive and non-destructive; stock behaviour is preserved and must keep working

- **Status**: ✅ **Resolved** (2026-09-17, Session 7)
- **Context**: D-008 established that the firmware is editable. It did not say *how*. The queue work in D-018 touches `src/mqtt/MQTT.{h,cpp}`, stock Meshtastic files on the critical path of the app-pairing and MQTT-uplink features the product depends on. Overwriting them in place would destroy the measurement baseline RQ1/RQ2 need, and would risk the two integrations the demo cannot lose.
- **Decision**, three binding constraints on every firmware change this term:
  1. **Never delete or overwrite stock logic. Preserve it and branch around it.** The replaced stock path is kept intact and reachable, either behind a compile-time flag defaulting to the TrekLink path or extracted verbatim into a clearly-named function that the new code can call. A reviewer must be able to read what upstream did without consulting git history.
  2. **Stock Meshtastic mobile-app compatibility is a hard requirement.** The phone app pairs over BLE and, with `proxy_to_client_enabled`, carries the MQTT uplink itself (`MQTT.cpp:800`, `:607`). That path must keep working unmodified, it is how a guide with a phone and no laptop reaches the backend at all, and it is Stage C's mobile ingress.
  3. **The stock MQTT endpoint stays the publish target.** TrekLink does not invent a wire format here. Events still leave as `ServiceEnvelope` on `<root>/2/e/<channelId>/<nodeId>` and JSON on `<root>/2/json/...` (`MQTT.cpp:423–430`). The queue changes *which event leaves next and whether it survives a reboot*, never what it looks like on the wire. This keeps `gateway-sync`'s normalizer (D-007) unchanged and keeps the units legible to any stock Meshtastic tooling.
- **Rationale**: constraint 1 is what makes the D-018 baseline reproducible, "unmodified firmware" must be a build the team can still produce on demand, not a git tag nobody can flash. Constraints 2 and 3 protect the two integrations whose loss would cost the demo, for no benefit: nothing about a durable queue requires a new wire format or a forked app protocol.
- **Consequence**: a firmware PR that deletes stock code, changes a published topic shape, or breaks app pairing is rejected on this decision regardless of how well the queue works.
- **Owner**: KhoaDD.

### D-020: Basecamp gateway (Stage C) delivery form: browser-first, native desktop as fallback, no iOS

- **Status**: 🟡 **Decided in principle, build deferred** (2026-09-17, Session 7). Stage C is scheduled after Stage B; this records the form so Stage B's interfaces do not foreclose it.
- **Context**: Stage C needs a machine at basecamp with more storage than a node and an intermittent internet uplink. Three delivery forms were considered, against a guide who is not a technician.
- **Options considered**:
  1. **Purely web-based**, the guide opens a tab. The page reaches the node over **Web Serial** (tethered laptop) or **Web Bluetooth** (phone or laptop), holds the queue in browser-local storage (IndexedDB), and forwards upstream when the network returns. No install, cross-platform by construction, one artifact to maintain.
  2. **Native cross-platform desktop app** (Windows/Linux/macOS), or the `server.sh` / `server.bat` one-click equivalent over the existing `gateway/src`. Full serial access, no browser-permission friction, real filesystem storage. Three build targets and a signing/distribution problem.
  3. **A second Android app running alongside Meshtastic**, rejected. iOS is unviable without macOS/Xcode (already established in D-005), so this splits guide behaviour by platform, and it duplicates the pairing the stock app already does.
- **Decision**: **Option 1 is the target form**, with Option 2 retained as the documented fallback for any venue where browser device APIs are unavailable. Option 3 stays rejected, consistent with D-005.
- **Rationale**: Option 1's install cost is zero, which is the only property a non-technical guide actually experiences. `gateway/src` is not wasted under it, the queue policy, priority tiers and flush ordering are the substance and port directly; only the transport and storage adapters differ, which is the same ingress-adapter seam D-007 already defines.
- **⚠️ Known risks, accepted and carried** (see the risk table):
  - **Web Serial and Web Bluetooth are Chromium-only**, no Safari, no Firefox. On iOS this means *no browser at all* can reach the node, so an iPhone guide is served only by the stock Meshtastic app path (D-019 constraint 2), not by Stage C.
  - **A tab is a fragile host for a durable queue.** Backgrounded tabs get throttled, and a closed tab stops forwarding. IndexedDB survives the close, so nothing is lost, but delivery stalls until the tab is reopened. This must be surfaced in the UI and stated honestly at the council, not presented as an always-on service.
  - **WebRTC is not the mechanism.** It was raised as a candidate; it addresses peer-to-peer transport, not device access or storage, and is not required by Option 1. Recorded here so it is not reintroduced as an assumption.
- **Owner**: KhoaDD; revisit at Stage C kick-off.

### D-021: Encryption: provision a custom channel PSK; keep the MQTT hop plaintext behind TLS

- **Status**: ✅ **Resolved** (2026-09-17, Session 7)
- **Context**: "turn on end-to-end encryption" was raised as a requirement. Inspection shows the premise needs splitting, because two independent switches were being treated as one.
  - **LoRa channel PSK.** `Channels::initDefaultChannel()` (`Channels.cpp:128–134`) already sets `psk.bytes[0] = 1`, which `:238` documents as *"no change vs defaultPSK"*. So RF traffic **is** AES-encrypted out of the box, with **Meshtastic's published default key**. Any stock client in range decrypts it, SOS positions included. Encryption is present; confidentiality is not.
  - **`moduleConfig.mqtt.encryption_enabled`.** Controls only whether the packet handed to the broker stays encrypted. D-007 froze it `false` so the backend receives decoded payloads.
- **Decision**:
  1. **Provision a custom per-fleet PSK on the primary channel**, replacing the default key, as part of device provisioning in MF-01. This is the change that delivers actual confidentiality on the RF hop, and it is configuration, not code.
  2. **`encryption_enabled` stays `false`** (D-007 unchanged). The gateway→cloud hop is secured by **TLS plus a per-gateway API key** instead.
- **Rationale**: setting `encryption_enabled = true` would force reimplementing Meshtastic's AES-CTR channel crypto inside NestJS *and* would eliminate the JSON topic entirely (`MeshPacketSerializer` needs a decoded payload), taking out the whole Stage A ingress path, to protect a hop that TLS already protects. The real exposure was never the broker hop; it was the well-known RF key, which item 1 closes for the cost of a provisioning step.
- **Consequences**:
  - Device provisioning acquires a new mandatory step, and the PSK becomes a managed secret, never committed, never printed in a graded document, distributed per D-015 as configuration.
  - A custom PSK means a **stock Meshtastic app must be given the channel** to pair usefully. This does not violate D-019 constraint 2, the app path still works, it just needs the channel QR like any Meshtastic channel.
  - Losing the PSK bricks the fleet's interoperability until every unit is re-provisioned. Treat it as a recovery-critical secret with an escrowed copy.
- **Owner**: KhoaDD; provisioning step to be written into `specs/devices/`.

### D-022: `capstone` is a live auto-sync repository; the no-direct-push rule is scoped to the code repos

- **Status**: ✅ **Resolved** (2026-09-17, Session 7)
- **Context**: `capstone/` was already a local git repository with no remote, tracking `AGENTS.md`, `CLAUDE.md` and `Documents/`, the three sibling repos are gitignored. The team needs graded paperwork (progress log, reports, diagrams, meeting notes) to move between members continuously, edited from Obsidian. That is incompatible with `01-conventions/07-github-workflow-git-conventions.md` §3, which requires every change to arrive by PR and forbids direct pushes to any branch.
- **Options considered**:
  1. Fold `Documents/` into `treklink-docs`, one repo, but every progress-log edit becomes a PR, which defeats the purpose and would make the workbook a merge-conflict generator.
  2. `capstone` with the three siblings as submodules, rejected: detached HEADs and stale checkouts are a known failure mode for a team under deadline, and it solves nothing here.
  3. Give the existing `capstone` repo a remote and let it auto-sync.
- **Decision** (Option 3): `capstone` is published as a **private** repository and runs **auto-commit / auto-push to `main`**. The no-direct-push rule is explicitly **scoped to `treklink-docs`, `treklink-web` and `treklink-firmware`**, which keep full PR discipline.
- **Rationale**: PR review exists to gate code that can break a build or a contract. Meeting notes and a progress log cannot. Applying the same ceremony to both makes the ceremony get skipped for the thing that actually needs it. Separating them keeps the gate credible where it matters.
- **Scope limits, all three are load-bearing**:
  1. **`capstone` only.** A change to a sibling repo is still a PR, always.
  2. **`_docs/` stays read-only in the vault.** `treklink-docs` is a nested repository that `capstone/.gitignore` excludes, so Obsidian can read and link conventions while edits to them still go through a PR. This is a feature, not a limitation.
  3. **History stays linear.** `pull.rebase = true` is set in the repo config; merge commits remain prohibited everywhere (§3).
- **Consequences**: the repo is private, because `Documents/` holds supervisor meeting notes and course-issued PDFs. Auto-commit intervals must exceed a typical agent session, or in-progress edits get committed mid-work.
- **Owner**: KhoaDD.

### D-023: A Main Flow owner is not the assignee of every story in that flow

- **Status**: ✅ **Resolved** (2026-09-17, Session 7)
- **Context**: raised by LongLP, the charter (`01-project-charter.md:14`) and roadmap (`02-roadmap-and-milestones.md:164`, `:249`) name **LongNN** as the frontend/FSD, map-and-monitoring-UI member and **MF-04 owner**, while the generated backlog assigned every `module:monitoring` story to TanNB. Verified by inspection, and the report was correct on MF-04 and over-generalised beyond it.
- **What the audit actually found**:
  - LongNN held **zero** monitoring and **zero** frontend stories, his nine were devices ×4, trips ×3, auth ×1, billing ×1. He did not own `TK-63`, the MapLibre/Goong live map, despite being the designated map member.
  - Load was skewed: TanNB 28 stories across nine modules, LongNN 9.
  - Cross-tabulating all 75 MF-tagged stories showed 66 "mismatches" *if* one assumes the flow owner owns every story in it. **That assumption is wrong**, MF-01's stories are spread across all five members by design.
  - **MF-04 was the only flow whose named owner held none of its stories.** That is what made it a real defect rather than a naming artifact.
  - It was **not a generator bug**: `build_backlog.py` documented a deliberate Session-2 rearrangement that was never reflected back into the charter.
- **Decision**, two parts:
  1. **The roles are distinct and both are kept.** A **Main Flow owner** (D-016) is accountable for that flow reaching Demo Ready on the Mainflow Coverage Matrix. A **story owner** implements one story. A flow legitimately spans several implementers. Do not reassign a whole flow to its owner.
  2. **MF-04 is corrected by layer, not by flow.** UI stories → **LongNN** (`TK-63` live map, `TK-64` telemetry display, `TK-73` gateway connectivity indicator, exactly the "map and monitoring UI" the charter names). Backend infrastructure stays with **TanNB** (`TK-61` Socket.io gateway, `TK-46` monitoring read API). Session 2's reasoning for placing infrastructure there is unchanged.
- **Rationale**: this satisfies the charter's *skill* basis and the Session-2 risk weighting simultaneously, the frontend member gets the frontend work, the riskiest backend infrastructure stays where it was deliberately put. No charter or roadmap edit is needed, because the charter was already right.
- **Consequence, individual contribution**: the register already carries the risk that a member with thin visible contribution fails individually even when the group passes. LongNN listed as MF-04 owner while another member demos MF-04 is exactly the *"which part did you do?"* failure the faculty handbook describes. This correction is as much about defensible individual evidence as about tidy assignment. New balance: TanNB 25, LongLP 20, Khoa 18, HoangTK 13, LongNN 12.
- **Also corrected in the same pass**: `build_backlog.py` carried an assignment rationale naming a teammate as unreliable. That file is tracked and forms part of the graded documentation set. The rationale is rewritten in terms of layer and track record; person-level judgements belong in `ignore/`, not in a graded repository.
- **Follow-up**: Jira must be re-synced for `TK-63`, `TK-64`, `TK-73` before implementation starts.
- **Owner**: KhoaDD.

### D-024: Prose conventions are binding, and Jira keys are scoped to Epics and User Stories

- **Status**: ✅ **Resolved** (2026-09-22, Session 8). Two rulings recorded together because both were issued in the same instruction and both touch `01-conventions/07`.
- **Context**: two separate wording and tracking defects were raised by the leader.
  1. Every tracked document, and every AI chat reply, was using the em dash as a general-purpose clause separator. At volume it reads as machine-generated, it is inconsistent with the graded reports' register, and it hides the sentence boundary a reader under time pressure needs.
  2. `07-github-workflow-git-conventions.md` §2.2 said a branch without a Jira key was *"rare, prefer creating the card."* Followed literally, that mints a Jira card for every CI fix, tooling change and conventions edit, which pollutes the backlog, the Mainflow Coverage Matrix and the burndown. Those three are exactly what the supervisor reads at the weekly Group Meeting.
- **Decision**, two parts:
  1. **Prose conventions are binding**, published as [`01-conventions/14-prose-and-wording.md`](../01-conventions/14-prose-and-wording.md). No em dash in prose anywhere, no en dash as a clause separator, no banned openers, no aphorism formulas, no two-beat antithesis, no unverified claims, no fabricated precision. Code fences, inline code spans, "not applicable" table cells and verbatim quotations are exempt as data. Every AI assistant loads the `prose-and-wording` skill and `caveman` level `full` at session start, without being asked; chat is compressed, anything persisted to a repository stays normal prose.
  2. **A Jira key belongs to an Epic or a User Story and to nothing else.** A branch and a commit scope carry a key only when the unit of work is one of those cards. Housekeeping, CI, automation, tooling, conventions and non-deliverable documentation carry no key and use a plain descriptive branch. Task and Subtask breakdown is tracked inside the parent story card, not as its own keyed branch. Do not create a card so that a branch can have a key.
- **Rationale**: part 1 is a register decision, and the register of a graded artifact is not cosmetic, because the Faculty Council reads the reports. Part 2 restores the backlog's meaning: a card should represent delivered product scope, so that the Coverage Matrix stays an honest instrument.
- **Consequences**:
  - `07-github-workflow-git-conventions.md` §2.2 and §3.1 are amended in the same pass.
  - `00-index.md` gains row 14.
  - The four canonical `AGENTS.md` copies gain §2.7 and a line in §6.
  - A **mechanical sweep ran on 2026-09-22** across `treklink-docs`, replacing 1230 em dashes in 54 files. It was verified by diff sampling, `git diff --check` for the trailing whitespace a blind join leaves behind, and the new checker, per chapter 14 §6.2. `treklink-web` and `treklink-firmware` are **not** swept; chapter 14 §6 corrects a file when it is next edited for another reason.
  - **`.github/scripts/check_prose.py` and `.github/workflows/prose-check.yml` are built and green.** They enforce the two regex-detectable rules, the em dash in prose and a banned opener at the start of a sentence, on every pull request and push to `dev` and `main`. The rest of chapter 14 stays review-enforced, because a checker that guesses at aphorism formulas or unverified claims produces false positives.
  - **Never swept, and skipped by the checker**: `topics/`, `ignore/`, and the two generated backlog files. Chapter 14 §6.1 carries the list.
- **Incident recorded in the same pass**: the first run of the sweep had an exclusion filter for `topics/` that silently failed. It reworded the **registered capstone project title**, in English and in Vietnamese, in both `topics/Phieu_FA26SE159.md` and `topics/Phieu_dang_ky_de_tai_TrekLink_FA26_FINAL.md`. Caught by sampling the diff, reverted with `git checkout` before any commit, net change zero. Those forms are the version the school holds and the title is frozen until the Week 6 scope lock. The rule that came out of it is `08-ai-agent-steering-and-discipline.md` Stage 2.7: print the file list a sweep will touch and confirm the exclusion held, do not assume the filter worked.
- **Two further standing rules were issued in the same instruction** and are recorded as `08-ai-agent-steering-and-discipline.md` Stages 2.6 and 2.7:
  - **Stage 2.6**: a standing instruction from the leader is written into the conventions in the same pass, routed by kind. An instruction that lives only in a chat transcript is lost at the next session boundary.
  - **Stage 2.7**: correct before polished. A document that is factually right, cites its evidence and contradicts nothing is done, even if its wording is plain. This does not license an unverified claim; Evidence Completeness still outranks Resource Efficiency.
- **Owner**: KhoaDD.

## Risk register (carried from FA26SE159, kept live)

| Risk | Likelihood | Impact | Mitigation | Status |
|---|---|---|---|---|
| Insufficient physical TrekLink devices | Medium | High | Simulate Gateway→Cloud load via MQTT scripts; simulation never substitutes for LoRa RF reliability claims | Open |
| Gateway sync latency exceeds 5s NFR | Medium | Medium | Tune MQTT QoS, reduce payload size, backpressure on queue flush | Open |
| RQ3 baseline not objectively measurable | High | Medium | Randomized simulation drills; independent observer triggers SOS, records timestamp; participants unaware of exact trigger time | Open |
| NestJS + MQTT + WebSocket integration underestimated | Medium | High | PoC gateway→backend integration in TP1 Week 2, not deferred to TP3 | Open |
| Firmware message schema incompatible with new gateway | Low | High | Schema frozen & documented in TP1 before any gateway implementation | Open, tracked as TP1 exit gate |
| Scope creep | High | Medium | Feature freeze after TP5 Week 10; anything else goes to post-capstone backlog | Open |
| ~~ORM indecision stalls TP3 start~~ | Medium | Medium | D-001 resolved (Prisma) ahead of the Week 4 target | **Mitigated** |
| **[Added]** Field connectivity may need phone-based bridging instead of a dedicated Gateway node (D-005) | Medium | High | Validate dedicated Gateway (Wi-Fi/cellular node, native Meshtastic MQTT module) in TP1 PoC before committing to it; mobile-app forking treated as out of scope given no macOS/Xcode access | **Mitigated**, D-005 resolved as a staged rollout (Stage A node-MQTT, Stage B basecamp bridge); app forking formally rejected |
| **[Added]** ~~NestJS is a skill only for Khoa~~, **corrected Session 6.** The enrolment framework codes show **LongLP and HoangTK are both `BIT_SE_NJS_18D` (Node.js)**; NestJS is a framework over Node and is treated as covered by that training. Backend capability is therefore **3 of 5**, not 1 of 5. Residual exposure is the two IC-track members (LongNN, TanNB) picking up backend stories solo | Low | Medium | Ramp LongNN and TanNB on `01-conventions/05-backend-conventions.md` module conventions during W2–W5, mentored by Khoa/LongLP. Lane assignment already routes core backend to the two NJS-track members. This is the substance of the Report 2 §2.3 Training Plan | **Downgraded** (Session 6) |
| **[Added S3] SOS is announced by exactly one unacknowledged text packet.** `TrekLinkSOSHelper::triggerSOS()` sends the `"SOS - …"` text once with `want_ack = false` (`TrekLinkSOSHelper.cpp:157–160`); `tickBeacon` then retransmits **position only**. If that single text frame is lost over RF, the episode is invisible to the backend, the beacons that follow look like routine position reports. | Medium | **Critical** | **Two layers, per D-008.** *Platform side (built now)*: cadence-anomaly detector, the beacon runs 5s for the first minute then 30s (`TrekLinkSOSHelper.cpp:181`), far denser than routine reporting, so ≥N positions in window W raises a *suspected* episode at lower confidence (REQ-EVT-06, `design.md` §2.4). *Firmware side (now available, preferred)*: retransmit the SOS discriminator with each beacon, and/or set `want_ack`, and/or introduce a real custom PortNum so SOS stops depending on a `printf` string. The platform mitigation is defence-in-depth and should be kept even after a firmware fix. | Open, **firmware fix candidate #1** |
| **[Added S3]** Stage A (node MQTT, D-005) has no offline buffer, so charter §5's offline-recovery and priority-ordering NFRs and RQ1/RQ2 cannot be evaluated until Stage B lands | High | Medium | **Stage B is permanent scope** (D-005 closed, Session 6), it is the substance of supervisor-specified **MF-02** (D-016), so it cannot be traded away. Residual risk is now schedule, not scope: Stage B must land early enough for RQ1/RQ2 evaluation in W9–W12. Do not present Stage A as satisfying the offline NFR at any point. **Further reduced (Session 7, D-018)**: the buffer work is now split, and the *smaller* half, the on-device queue, is sequenced first and needs no basecamp hardware, so a usable offline buffer exists well before the basecamp gateway (now Stage C) is built. | Open, **schedule risk only**, scope question closed, exposure reduced by D-018 |
| ~~**[Added S3]** `treklink_v1_0` compiles MQTT out, so v1 units cannot act as the Stage A uplink node~~ | Medium | High | v1 is **out of the demo set** (team lead, Session 3), Stage A targets v2/v3/v4, all of which leave MQTT compiled in and carry Wi-Fi silicon. Were v1 ever needed, removing `-D MESHTASTIC_EXCLUDE_MQTT=1` is a one-line build-flag change now that D-008 permits it. | **Closed** |
| **[Added S3]** Stage A's JSON MQTT path omits `MeshPacket.priority`, making the SOS position packet indistinguishable from a routine one (D-007) | High | Medium | Backend episode correlation with a backward grace window (`design.md` §2.4). Two upgrades now available: switch to the `/2/e/` protobuf topic to recover `priority`, or emit an explicit SOS marker firmware-side (D-008). | Open, accepted for Stage A |
| ~~**[Added S3]** `04-architecture-conventions.md` §3 still publishes the old `deviceId:sessionId:sequenceNumber` formula~~ | High | Medium | §3 rewritten to the D-006 split key, with the D-008 firmware-sequence option noted as a layered upgrade. | **Closed** (Session 3) |
| **[Added S3]** Sessions 1–3 reasoned under a false "firmware is frozen" premise (D-008); several conclusions were shaped by a constraint that does not exist | High | Medium | D-008 logs the correction and lists every decision it reopens. Re-derive the affected conclusions next session before they harden into implementation. | Open, **re-derivation pending** |
| ~~**[Added S3]** Firmware changes may earn no graded credit (D-008 question 3)~~ | Medium | Medium | Supervisor confirmed 2026-09-13 that **firmware commits do earn credit**, and that firmware enhancement is in scope rather than the excluded "redesign". D-008 closed. Firmware work is planned, assigned and evidenced on the same footing as platform work. | **Closed** (Session 6) |
| **[Added S4]** SOS beacon retransmits (all but the very first position packet of an episode) are sent at `BACKGROUND` mesh priority, not `MAX`, `PositionModule::sendOurPosition()` sets priority by device role, and a handheld unit isn't `TRACKER`/`TAK_TRACKER`. Under mesh congestion, an ongoing emergency's beacon trail is currently the least-protected traffic class on the network after its opening packet. | Medium | High | Firmware-fix candidate under D-008: set `priority = MAX` (or `RELIABLE`) explicitly on the beacon send path rather than delegating to the generic broadcast method. Platform-side, the episode-correlation logic in `gateway-sync` already doesn't rely on `MeshPacket.priority` at all, so this doesn't block TP1/TP2, it's a firmware-side robustness gap, not a backend defect. | Open, **firmware fix candidate** |
| **[Added S6] Map tiles rendering foreign toponyms over Hoàng Sa / Trường Sa are unlawful to publish in Vietnam.** `LiveMapWidget.tsx:18-19` ships the default OpenStreetMap tile server; the team lead verified every OSM base layer labels the archipelagos with Chinese names. Nghị định 174/2026/NĐ-CP Art. 93(3)(a), in force 1 Jul 2026, fines 30–40M VND and permits forced removal of the application | High | **Critical** | Migrate to **Goong Maps over MapLibre GL** (D-012), provider and style held in configuration so the layer is swappable. Referer-allowlist and rate-limit the browser-visible key. **Acceptance test before Review 1**: load the map over ~16.5°N 112.0°E and ~9.7°N 114.0°E, confirm Vietnamese toponyms, file screenshots as evidence | Open, **migration not started; sovereignty check not yet performed** |
| **[Added S6] The registered English title contains "Smart Device Rental Management" but the system has no AI/ML component.** The faculty fault handbook devotes a whole section to projects whose name promises "AI / smart / thông minh" and whose demo reduces to CRUD, and states plainly that rule-based systems must be called rule-based | High | Medium | Prepare and rehearse the honest answer: the "smart" is **device-level autonomy**, IMU fall detection, autonomous SOS broadcast, multi-hop mesh routing, priority-ordered store-and-forward, not machine learning. Never use "AI" for the platform in any graded document or slide. Every member must be able to give this answer; it is a near-certain council question | Open, **answer to be drafted and rehearsed before W12 Mock Council** |
| **[Added S6] Business parameters hardcoded as literals.** Ranked #2 cause of failure in the faculty fault handbook, and the first entry in its most-asked-questions list is "can this number be changed? demo it now." Five instances already exist in a codebase with no business logic written yet | High | High | D-015: every business parameter in configuration, registered in the Configuration Matrix with a demo path. Fix the five existing instances now, before the pattern is copied into the modules that carry the real parameters (fees, deposits, thresholds, episode windows) | Open, **task queued this session** |
| **[Added S6] Individual-contribution evidence.** A member with few commits and thin meeting presence fails individually even when the group passes; Git organisation and the Progress Log are the named evidence. Implementation currently stands at zero across all modules, so no member has contribution evidence yet | Medium | High | D-014: Progress Log updated before every Group Meeting, per-member and tied to a named deliverable; `08_INDIVIDUAL_CONTRIBUTION_LOG` maintained from W1. Lane assignment gives every member an owned mainflow so commits distribute by construction | Open |
| **[Added S7] The node's own MQTT queue discards an SOS before it discards telemetry.** `mqttQueue` is 16 entries, RAM-only, strict FIFO, and evicts the **oldest** entry on overflow (`MQTT.h:27`, `MQTT.cpp:821–823`). An SOS beacon trail plus routine telemetry fills it in ~80 s of outage, and the single `"SOS - …"` text frame that identifies the episode is the oldest entry, so it goes first. A reboot loses the queue entirely. The eviction is a serial-console `LOG_WARN`, invisible to the platform, so the loss is also unmeasurable. | High | **Critical** | **D-018 Stage B**, an on-device priority-ordered, flash-backed, shed-aware queue, built additively per D-019 and following the `MessageStore` precedent (`04-firmware-ground-truth.md` §4.1). Publish per-tier drop counters so the loss becomes observable and RQ1 gets a device-side denominator. | Open, **this is the term's primary firmware deliverable** |
| **[Added S7] v3 has no PSRAM, and a flash-sized queue bound must not be derived from PSRAM.** `treklink-v3-tbeam` is ESP32 LX6 with ~4 MB flash shared with two OTA app slots; v2 (S3 N8R8) and v4 (Supreme S3) both carry 8 MB PSRAM. The existing `StoreForwardModule` already branches `ps_calloc`/`calloc` on PSRAM presence (`StoreForwardModule.cpp:80–85`), so a queue sized the same way would silently collapse to near-zero capacity on v3 while tests pass on v2. | Medium | High | Size the durable tier off **remaining LittleFS space**, per variant, with the bound as a `build_flags` override exactly as `MESSAGE_HISTORY_LIMIT` already is (`MessageStore.h:22–24`). Capacity is a Configuration Matrix entry per D-015. Test the bound on v3 hardware specifically, not only on v2. | Open, **v3 is in the demo set** (D-018) |
| **[Added 2026-09-25] Correction to the S7 row "v3 has no PSRAM".** The build contradicts it: `treklink-v3-tbeam` inherits `-DBOARD_HAS_PSRAM -mfix-esp32-psram-cache-issue` from `env:tbeam` (`variants/esp32/tbeam/platformio.ini:19-20`), while `treklink-v2` enables no PSRAM (`treklink_v2_0/platformio.ini:19-24`, board `esp32-s3-devkitc-1` declares none). (unverified) physical PSRAM population on the v2 and v3 units. LittleFS is 1 MiB on v3 and 1.5 MiB on v2/v4 (`04-firmware-ground-truth.md` §5). | Medium | High | The S7 mitigation stands unchanged: size the durable tier off remaining LittleFS space, never PSRAM. The per-variant PSRAM column in `onboard-queue/design.md` §4 is corrected in the firmware repo. | Open |
| **[Added 2026-09-25] Correction to the S7 row "The node's own MQTT queue discards an SOS".** Two facts in it were incomplete. (1) In direct Wi-Fi mode stock firmware drains **one** queued entry per successful reconnect and nothing while the link stays up (`MQTT.cpp:604-636`, `:699-709`), so the baseline is worse than drop-oldest alone. (2) The queue also holds **peer** packets the node hears (`Router.cpp:767-769`), so it fills faster than the "~80 s" estimate, which was never measured. | High | **Critical** | Same mitigation, D-018 Stage B. `onboard-queue` must add a drain on the connected branch of `MQTT::runOnce()` (`MQTT.cpp:624-632`), a third stock call site the design did not list. | Open |
| **[Added 2026-09-25] A fall auto-SOS never beacons.** `FallDetectionModule::triggerAutoSOS()` sends one `BACKGROUND` position and one text frame (`FallDetectionModule.cpp:144-145`) and never calls `tickBeacon()`, which only the button and gesture modules call (`TrekLinkButtonModule.cpp:312`, `TrekLinkSOSGesture.cpp:116`). A fall episode is two packets and then silence, so the cadence-anomaly detector (REQ-EVT-06) cannot fire for it, and losing the single text frame makes a fall invisible. | Medium | **Critical** | Firmware fix under D-008: have the fall path enter the same beacon loop as a button SOS and send its opening position at `MAX`. Sequence with `onboard-queue` Phase 9 so each change stays separately measurable. | Open, **firmware fix candidate** |
| **[Added 2026-09-25] An SOS raised before the first GPS fix produces no beacons.** `PositionModule::sendOurPosition()` returns without sending when there is no fixed position and no local position since boot (`PositionModule.cpp:352-355`). Only the trigger-time packets leave the node. | Medium | High | Firmware fix under D-008: the beacon path builds its own position packet (as `sendPositionPacket()` already does) instead of delegating to `sendOurPosition()`. The same change can carry `priority = MAX` (REQ-OPT-02). | Open, **firmware fix candidate** |
| **[Added 2026-09-25] The on-device queue-health report cannot be read on the JSON topic.** `PRIVATE_APP` (256) has no case in `MeshPacketSerializer`, so it falls to `default` (`MeshPacketSerializer.cpp:403-404`) and the JSON message has no `payload`. `gateway-sync` Stage A ingests only `<root>/2/json/...` (REQ-EVT-01), so its REQ-EVT-12/13 are unreachable as specified. | High | Medium | Decide the transport before `onboard-queue` Phase 6: an additive `PRIVATE_APP` case in the serializer, or a `/2/e/` subscription for port 256 in the backend. Define the payload schema in both specs. | Open, **question raised by the firmware session** |
| **[Added S7] The default LoRa channel PSK is Meshtastic's published key**, so RF traffic is encrypted but not confidential, any stock client in radio range decrypts SOS positions (`Channels.cpp:128–134`, `:238`). | High | High | **D-021**, provision a custom per-fleet PSK during device provisioning in MF-01; treat it as a recovery-critical escrowed secret. Distinct from `mqtt.encryption_enabled`, which stays `false` per D-007 with the broker hop secured by TLS + per-gateway API key. | Open, **provisioning step not yet written into `specs/devices/`** |
| **[Added S7]** Stage C's browser form (D-020) relies on **Web Serial / Web Bluetooth, which are Chromium-only**, and on iOS no browser can reach the node at all, so an iPhone guide is served only by the stock Meshtastic app path. A backgrounded or closed tab also stalls forwarding, though IndexedDB means nothing is lost. | Medium | Medium | Document the supported-host matrix before Stage C kick-off; surface forwarding state in the UI; retain the native-desktop fallback (D-020 Option 2) for unsupported venues. State the tab dependency honestly at the council rather than presenting Stage C as an always-on service. | Open, **Stage C deferred; decision recorded so Stage B does not foreclose it** |
| **[Added S4]** No backlog user story exists for the cadence-inferred `SUSPECTED` SOS episode path (`gateway-sync/design.md` §1.3/§2.4, REQ-EVT-06), the primary mitigation for the Critical single-unacknowledged-text-frame risk above has no corresponding Staff-facing workflow story yet | Medium | Medium | US-088 added (`03-backlog/02-user-stories.md`), assigned Khoa, Staff sees a Suspected episode visually distinct from Confirmed and can dismiss it with a lighter action than the full Resolved→Closed flow. | **Mitigated** (Session 4) |
