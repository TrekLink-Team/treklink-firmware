---
id: docs
type: section
status: approved
owner: KhoaDD
summary: Index of the treklink-firmware documentation: where each kind of fact lives.
---

# treklink-firmware documentation

Layout set on 2026-10-29 (option A, firmware layout rules in
`treklink-docs/_docs/01-conventions/09-doc-driven-scaffold-and-ssot-conventions.md` §1.1). Every
file that used to live in `.kiro/`, `.agents/`, `_handoff/` or the repository root was moved with
`git mv`; nothing was deleted.

## Where things live

| I need | Path |
|---|---|
| What v1 to v4 are, per pin and per feature | [variants.md](variants.md) |
| Build, flash, monitor, test | [build-and-flash.md](build-and-flash.md) |
| C++ coding conventions for this firmware | [coding-conventions.md](coding-conventions.md) |
| Hardware design records, BOMs, budget | [hardware/](hardware/README.md) |
| Product and variant roadmap | [roadmap/](roadmap/) |
| Dated hardware test runs | [test-runs/](test-runs/README.md) |
| Closed work records (handoff logs, audits) | [work/](work/README.md) |
| Session logs, one file per session | [sessions/](sessions/) |
| Superseded or wrong documents, with reasons | [.deprecated/](.deprecated/README.md) |
| Specs per variant and per module | [../specs/](../specs/README.md) |
| What the firmware puts on the wire | `treklink-docs/_docs/00-project-context/04-firmware-ground-truth.md` |
| Decisions and risks | `treklink-docs/_docs/00-project-context/03-decisions-and-risk-register.md` |
| Git, commits, agent rules, wording | `AGENTS.md` and `treklink-docs/_docs/01-conventions/` |

## Roadmap folder

| File | Contents |
|---|---|
| `roadmap/proposed-development-roadmap.md` | One-page summary of v1 to v5; v3 and v4 are the LilyGo boards |
| `roadmap/roadmap-analysis.md` | Why the project pivoted from the v2 custom PCB to the LilyGo boards |
| `roadmap/proposed-prebuilt-modules-analysis.md` | Comparison of prebuilt LoRa devices considered for v3 |
| `roadmap/firmware-development-roadmap.md` | Per-variant firmware plan (June 2026); its `ruskicoder/treklink` repository name is historical |

## Rules for this folder

- One fact, one home. Link, do not copy. Wire-format facts stay in treklink-docs.
- Live documents start with frontmatter: `id`, `type`, `status`, `owner`, `summary`, and
  `verified` (commit, date, scope) where the document makes claims about the code. Field values
  are listed in [../specs/README.md](../specs/README.md#frontmatter-fields).
- A substantive change to a spec first copies the old version into `.deprecated/YYYY-MM-DD-<slug>/`
  with a row in its README. Typos and link repairs do not.
- `work/`, `test-runs/` entries that are closed, and `.deprecated/` are never edited.
