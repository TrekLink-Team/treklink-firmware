# Deprecated documents

Read-only. Nothing here is current; live documents must not link into this folder except to
explain where something went. Each folder is `YYYY-MM-DD-<slug>/`, dated by when it was
deprecated. Files were moved with `git mv`, so `git log --follow` shows their full history.

| Folder | Former location | Reason | Superseded by |
|---|---|---|---|
| `2026-07-27-v1-srs-draft/` | `.kiro/docs/.deprecated/specifications.md` | An earlier v1 SRS, already deprecated before the initial commit of 2026-07-27. | `specs/variants/v1/` |
| `2026-07-27-v2-hardware-revisions/` | `.kiro/docs/artifacts/.deprecated/` | Revisions 1.0 and 1.1 of the v2 component list, PCB requirements, freelancer brief and LCSC BOM, already deprecated before the initial commit. | `docs/hardware/v2/` |
| `2026-10-29-hardware-tier-roadmap/` | `.kiro/specs/roadmap.md`, `.kiro/docs/artifacts/roadmap/roadmap-vision.md` (identical) | Defines v3 and v4 as custom pre-production PCBs and v5 as mass production. The leader set v3 and v4 to the LilyGo T-Beam v1.2 and T-Beam Supreme on 2026-10-29. | `docs/variants.md`, `docs/roadmap/proposed-development-roadmap.md` |
| `2026-10-29-kiro-methodology/` | `.kiro/guidelines/01` to `10` | Generic Kiro spec-driven methodology from the Kiro IDE era. Kiro is no longer used; the project's own workflow replaced it. | `treklink-docs/_docs/01-conventions/02-spec-driven-development-workflow.md` |
| `2026-10-29-agent-prompts/` | `.kiro/docs/artifacts/starting-prompt.md`, `.kiro/docs/must-do-while-coding.md`, `.agents/rules/save-your-tokens.md` | Session prompts and an always-on "ultra-concise, telegraphic" reply rule. The reply rule contradicts D-043, which retired the compressed register; moving it here stops assistants that read `.agents/rules/` from applying it. | `AGENTS.md` §2.7 |
| `2026-10-29-prompt-dump/` | `specifications.md` (repository root) | Not a specification: the original v1 drafting prompt followed, from line 916, by a pasted copy of the Kiro methodology. | `specs/variants/v1/` |
| `2026-10-29-answers-scratch/` | `answers.md`, `.kiro/docs/answers.md` (renamed `v2-qa-summary.md`) | Temporary answer files for agent questions during v1 and v2 hardware design. Their content is integrated into the v2 hardware documents. | `docs/hardware/v2/` |
| `2026-10-29-repo-docs-v0/` | `docs/AGENT.md`, `commit-rules.md`, `conventions.md`, `development-rules.md`, `lifecycle.md`, `README.md` | A second set of rules beside the canonical conventions (D-011). Specific errors: `dev/<username>-<feature>` branch names, which chapter 07 does not allow; NRF52 listed as a target, which no TrekLink variant uses. | `AGENTS.md`, chapter 07, `docs/build-and-flash.md`, `docs/coding-conventions.md`, `docs/README.md` |
| `2026-10-30-coding-conventions-v1/` | `docs/coding-conventions.md` (2026-02-04 version) | Written for a greenfield firmware, not this Meshtastic fork: it prescribed FreeRTOS tasks pinned per core, a `hal/ services/ ui/` source tree and a `\`-toggled `debugEnabled` console, none of which the code uses (TrekLink modules are `concurrency::OSThread`s and log through `LOG_*`). It was marked approved and would have taught the wrong model (D-011). | `docs/coding-conventions.md`, chapter 17 (D-077) |
