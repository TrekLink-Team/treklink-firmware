# TrekLink — AI Agent Rules & Steering Directives

> **Canonical file.** This exact content lives in four places: `capstone/`, `treklink-docs/`,
> `treklink-web/`, and `treklink-firmware/`. They are identical by design — do not let them drift.
> If you edit one, edit all four in the same pass.
>
> **Applies to every AI coding assistant** — Claude Code, Cursor, Copilot, Antigravity, Codex,
> Windsurf, Gemini CLI, OpenCode. Conventions v2, 2026-09-13.

---

## 0. Read This Before Doing Anything

You are working on **TrekLink** (FPT capstone FA26SE159), a LoRa-mesh trekking safety and
operations platform. The workspace is a `capstone/` parent folder containing three sibling
repositories plus the graded academic reports.

**At the start of every session, before any other action:**

1. **Resolve the docs root** (§1) and read
   `_docs/01-conventions/00-index.md`. At minimum then read **07** (git), **10** (Jira) and
   **11** (AI-first) in full.
2. **Check open decisions**: `_docs/00-project-context/03-decisions-and-risk-register.md`.
   Anything marked `OPEN` may block the task you are about to start. Say so before starting it.
3. **Check for recent sessions**: glob `docs/sessions/` and `ignore/*/docs/sessions/` for anything
   from the last ~24h touching the same area. **Do not re-derive what a recent session already
   found** — reading a 20-line session file is orders of magnitude cheaper than repeating the
   investigation.
4. **Locate today** on `_docs/00-project-context/02-roadmap-and-milestones.md` — which sprint,
   which roadmap week, what is due.

Do this **autonomously, without being asked.** A developer should never have to tell you to follow
the conventions. If you find yourself about to ask "should I follow the project conventions?", the
answer is yes and the question wastes a turn.

---

## 1. Repository Layout & Path Resolution

```text
capstone/                      ← OPEN THIS as your workspace root — and now a git repo of its own
├── AGENTS.md                  ← this file
├── scripts/sync.sh            ← auto-commits and pushes this repo (D-022)
├── Documents/                 ← graded academic deliverables. Not engineering docs.
│   ├── reports/               ← Reports 1–7 (.md source + .docx) and slide decks; figures in reports/assets/
│   ├── tracking/              ← Progress Log and tracking workbooks — .xlsx, one editor at a time
│   ├── course-material/       ← issued by the school/supervisor. READ-ONLY.
│   ├── templates/             ← blank forms. READ-ONLY — copy out, never fill in place.
│   └── meetings/              ← supervisor briefs and minutes
├── treklink-docs/             ← SSOT: conventions, decisions, backlog, templates
│   └── _docs/                 ← the canonical documentation root
├── treklink-web/              ← the active build
│   ├── gateway/  backend/  frontend/
│   └── specs/{module}/        ← requirements.md → design.md → tasks.md
└── treklink-firmware/         ← inherited SU26 LoRa firmware (editable per D-008, surgically)
```

**Resolving the docs root**, in order:

| You are opened on | Docs root |
|---|---|
| `capstone/` (**recommended**) | `treklink-docs/_docs/` |
| `treklink-docs/` | `_docs/` |
| `treklink-web/` or `treklink-firmware/` | `../treklink-docs/_docs/` |

> [!IMPORTANT]
> **If you cannot resolve the docs root, stop and say so.** Do not guess at the conventions, and do
> not fall back to generic best practice. A previous vendored copy of the conventions inside
> `treklink-web/docs/conventions/` was deleted (Decision **D-011**) precisely because it drifted
> and then confidently taught the wrong rules. A missing answer is cheaper than a wrong one.

**Prefer being opened on `capstone/`.** Cross-repo work is the normal case here — a spec in
`treklink-docs`, a wire format in `treklink-firmware`, the code in `treklink-web`.

---

## 2. The Non-Negotiables

### 2.1 Docs first, code later

No production code, database migration, or UI view before the module's spec suite exists and is
approved: `specs/{module}/requirements.md` (EARS) → `design.md` → `tasks.md` → `api-design/*.md`.
The gate is real. See `01-conventions/02-spec-driven-development-workflow.md`.

### 2.2 Ask questions before implementing

Run the clarification interview and **hard stop**. Batch your questions — domain edge cases,
authorization rules, error conditions, anything ambiguous or about to be assumed — and wait for
answers. An agent that starts implementing without asking is guessing at the domain, and those
guesses surface expensively in review.

### 2.3 Never self-advance through an approval gate

State the plan, the blast radius, and what you are deliberately not doing. Then **wait for explicit
approval.** Silence is not approval.

### 2.4 English only, in everything written to the repository

Code, identifiers, comments, commit messages, branch names, PR text, review comments, specs,
documentation, Jira cards, diagrams. **All English.**

The developer may prompt you in Vietnamese — that is fine, and English is merely preferred.
**Regardless of prompt language, everything you write into the repository is English.** The only
exception anywhere in the project is a daily-report entry.

### 2.5 Read before write

Never edit a file you have not opened and read in this session.

### 2.6 Document in the same pass — deferred documentation is banned

The instant you confirm a fact, resolve a risk, find a contradiction, or make a change a document
governs, **edit that document in the same tool-call sequence.** Not at session end. Not on a
to-do list.

Per finding, ask which of these it touches, and fix all that apply immediately:
- `_docs/00-project-context/04-firmware-ground-truth.md` — any verified firmware/wire-format fact
- `_docs/00-project-context/03-decisions-and-risk-register.md` — any new/resolved risk or decision
- `specs/{module}/{requirements,design,tasks}.md`, `api-design/*.md`
- Schema or code comments asserting something as fact — fix a wrong one now
- The backlog (`_docs/03-backlog/`) — leader only, via `build_backlog.py`

Rationale: Evidence Completeness outranks Resource Efficiency in the decision hierarchy. Long
sessions compact and paraphrase; a finding written from memory at hour three is measurably less
precise than one written the moment it was confirmed.

---

## 3. Git & Delivery

Full detail: `01-conventions/07-github-workflow-git-conventions.md`.

| Rule | |
|---|---|
| Integration branch | **`dev`** (there is no `develop`) |
| Branches | `main`, `dev`, `feat/*`, `fix/*`, `hotfix/*`, `docs/*`, `chore/*` |
| Branch naming | `feat/TK-45-device-registration` — **Jira key included** |
| Commits | Conventional Commits, Jira key as scope: `feat(TK-45): add device FSM guard` |
| Merge | Rebase & merge; Squash if multi-commit; **merge commits prohibited** |
| Direct pushes | **Never** — in `treklink-docs`, `treklink-web`, `treklink-firmware`. See the `capstone` exception below. |
| After any merge to `dev` | Everyone rebases; the merge is announced in Zalo |

> [!IMPORTANT]
> **Never commit or push unless explicitly asked.** Stage nothing, commit nothing, push nothing on
> your own initiative. Report what changed and let the developer decide.

### The `capstone` repository is the one exception (D-022)

`capstone/` is itself a private git repo tracking `Documents/` and this file. It **auto-commits and
auto-pushes to `main`** on a timer, because graded paperwork has to move between the team
continuously and PR-gating a progress log only teaches people to skip the gate.

- The exception is **scoped to `capstone` alone.** A change to any of the three code repos is still
  a PR, always.
- `treklink-docs/` is a *nested* repo excluded by `capstone/.gitignore`, so conventions are
  readable and linkable from the Obsidian vault while edits to them still go through a PR. That is
  deliberate, not an oversight.
- History stays linear there too: `pull.rebase = true`. Merge commits remain prohibited everywhere.
- **Before a long edit in `capstone`, take the lock** — `echo "reason" > .sync-lock` — so the timer
  does not publish half-finished work. Delete it when you are done.

---

## 4. Architecture Contracts (treklink-web)

- **Backend**: NestJS modular monolith, strict module isolation — no cross-module repository or
  entity access. (`01-conventions/04-architecture-conventions.md`)
- **Response envelope, non-negotiable**: every endpoint returns
  `{ "result": ..., "isSuccess": bool, "statusCode": int, "message": string }` (D-002).
- **ORM**: Prisma (D-001). Schema at `backend/prisma/schema.prisma`.
- **Frontend**: Feature-Sliced Design, Tailwind, **MapLibre GL JS over Goong Maps** (D-012 — never
  OpenStreetMap; map provider is configuration, not a literal), TanStack Query.
- **Auth**: JWT + bcrypt, RBAC via CASL. Every mutating endpoint needs **both** a JWT guard and a
  policy check.
- **Database**: local Docker Postgres for tests/CI; **Neon** for shared dev and prod (D-010).
- **Diagrams**: Mermaid only. Never PlantUML in new work.

### Firmware reality overrides the charter

Before designing anything that touches a device, read
`_docs/00-project-context/04-firmware-ground-truth.md`. It records what the firmware **actually
puts on the wire**, cited to `file:line`. Several charter assumptions have already been disproven
by it — notably the `eventId` scheme (D-006). Do not design against the charter's description of
the firmware without checking this file first.

---

## 5. Thinking Discipline

Full detail: `01-conventions/08-ai-agent-steering-and-discipline.md`.

**Banned**: stalling interjections ("Wait, actually…"), rhetorical loops with no tool call to
resolve them, narrating an action immediately before doing it, theatrical self-reaction, absolutist
claims before empirical verification, simulating a tool run in prose.

**Required loop**: state one falsifiable hypothesis → name the exact check → run it → state the
finding once → proceed. If refuted, move to the next hypothesis without re-litigating the failed
one.

**Epistemic anchor**: categorize as **KNOWN** (verified this session) / **INFERRED** (state
confidence) / **UNKNOWN** (flag before proceeding).

**Circuit breakers** — stop and ask, do not keep going:
- Same command or tool run >2× with no change in result
- Same file edited >2× without passing tests
- About to touch files outside the approved scope

**Blast radius** before any migration, deletion, dependency bump, or cross-module refactor: list
every affected caller, argue the alternative with equal rigour, check rollback feasibility. If
rollback is hard, get explicit confirmation first.

---

## 6. Model & Token Policy

Full detail: `01-conventions/11-ai-first-doctrine-and-toolchain.md` §2 and §6.

- **Critical coding, planning, architecture, design** → Claude (Sonnet 5 / Opus 5 / Fable 5.1) or
  GPT (5.6 Sol / 6 Astra). Prefer Claude for planning and design.
- **Google Gemini, and any model outside that list** → ingestion, chores, subagents, codebase
  understanding, explanation **only**. Never critical modules. No exceptions.
- Every PR declares `Model used:`.

**Context budget: 80% maximum.** Cross it and run `/summarization`, then start a fresh session from
the handoff prompt. Session size is not quota'd — 20K to 500K are all legitimate — but
proportionality is: heavy architecture work should cost a lot, a chore should cost almost nothing.
Cheapness never justifies an unverified answer.

**End sessions early and cleanly.** Do not extend a session for any reason except finishing the
current task.

---

## 7. Session Workflow

Run `/treklink-session`, or follow it manually:

```
1. Introduction        state the goal, name the repo/module
2. Context ingestion   grep the codebase, read conventions, check recent session files
3. Questions           batch them, HARD STOP, wait
4. Answers             the developer answers; debate, don't just accept
5. Pre-check           plan + blast radius + explicit approval gate
6. Implement
7. Report              what changed, test results, decisions, what was NOT done
8. Doc update          same pass — specs, decisions, session file
9. Wrap up             /summarization, end the session
```

Loop back to step 3 whenever something new becomes ambiguous.

**Session files**: `docs/sessions/YYYY-MM-DD-HHMM-topic.md` (tracked) and
`ignore/{name}/docs/sessions/YYYY-MM-DD-HHMM-topic.md` (personal). One file **per session
instance** — never a shared rolling file, which is how concurrent sessions lose findings.

---

## 8. Where Things Live

| I need… | Path |
|---|---|
| Conventions (the handbook source) | `_docs/01-conventions/` |
| Decisions & risks | `_docs/00-project-context/03-decisions-and-risk-register.md` |
| What the firmware actually sends | `_docs/00-project-context/04-firmware-ground-truth.md` |
| Roadmap & deadlines | `_docs/00-project-context/02-roadmap-and-milestones.md` |
| Backlog (**read-only to members**) | `_docs/03-backlog/` |
| Templates | `_docs/02-templates/` |
| Module specs | `treklink-web/specs/{module}/` |
| Personal scratch | `ignore/{name}/` — gitignored, safe to write |
| Generated handbook PDF | `_docs/TrekLink_Developer_Handbook_v1.0.pdf` — **never hand-edit** |

> [!WARNING]
> **`_docs/03-backlog/01-epics.md` and `02-user-stories.md` are generated.** Edit
> `build_backlog.py` and regenerate. Hand-edits are destroyed on the next run. Leader only.
>
> **The handbook PDF is generated** from `_docs/01-conventions/`. Edit the markdown, then rebuild.

---

## 9. Tool Discipline

Prefer structured tools over raw shell where both exist: dedicated read/search/edit tools over
`cat`/`grep`/`sed`. Reserve raw terminal commands for compilers, test runners, and git.

**Never rename a shared symbol via blind find-and-replace** — use the language server or IDE
refactor. TrekLink's cross-module DI wiring breaks easily under naive text replacement.

**Large files**: grep for what you need and read targeted line ranges. Do not read a 3000-line file
in full to answer a narrow question — it crowds out reasoning for no benefit.

---

## 10. Per-Repo Appendix

Everything above this line is **canonical and identical in all four copies**. Below the marker,
a repository may carry tool-generated or repo-specific content — for example the GitNexus code-
intelligence block in `treklink-firmware`, which the GitNexus CLI regenerates in place between its
own HTML-comment markers.

> [!WARNING]
> Do not write those marker strings anywhere above this section. A generator that replaces
> everything between the first start marker and the first end marker would swallow the canonical
> content along with its own block. Keep tool markers confined to the appendix.

Rules for the appendix:
- **Never** put a project convention there. Conventions belong in `_docs/01-conventions/`.
- Only tool-generated blocks with their own regeneration markers, or genuinely repo-local build
  facts that exist nowhere else.
- If you find yourself writing a rule in the appendix, you are creating the second SSOT that
  D-011 deleted. Put it in the conventions instead.

<!-- TREKLINK-CANONICAL-END -->

