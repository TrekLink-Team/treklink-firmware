---
id: specs
type: section
status: approved
owner: KhoaDD
summary: Index of firmware specs, by hardware variant and by module, with status.
---

# Firmware specs

One spec root for the repository (chapter 02 §2 of the conventions). Variant specs moved here from
`.kiro/specs/` on 2026-10-29 with `git mv`; their requirement text is unchanged. Each file now
starts with frontmatter (`status`, `support`, `hw_verified`, `verified`), and the header status
line was corrected where it still read "DRAFT" after every task had been ticked.

## Architecture

[architecture/](architecture/): how TrekLink sits on upstream Meshtastic as an overlay (draft, 2026-10-31). Read it before
changing any build file or adding code.

## By variant

Each active variant has a `components.md`, the authoritative list of its parts, pins and TrekLink
capabilities. The older `requirements.md`, `design.md` and `tasks.md` beside it predate the
overlay and give way to `components.md` where they disagree.

| Folder | Hardware | Status | Support | Hardware-verified |
|---|---|---|---|---|
| [variants/v1/](variants/v1/) | Perfboard ESP32, Ra-02 | implemented | deprecated, mesh only | none this term |
| [variants/v2/](variants/v2/) | Custom PCB ESP32-S3 | implemented | active | partial: SOS trigger observed |
| [variants/v3/](variants/v3/) | LilyGo T-Beam v1.2 | implemented | active | none |
| [variants/v4/](variants/v4/) | LilyGo T-Beam Supreme | implemented | active | none |
| [variants/shared/tasks.md](variants/shared/tasks.md) | Cross-variant tasks (SOS helper, fall sensor interface, gesture module) | implemented | active | partial |

v1 holds the base requirement set; the v2 to v4 specs list only what differs. The variant matrix is
[docs/variants.md](../docs/variants.md).

## By module

| Folder | Status |
|---|---|
| [onboard-queue/](onboard-queue/) | approved; 39 of 63 tasks ticked. Open: golden fixtures (0.4), stock-client check (6.3), hardware tests (7), measurement (8), SOS fixes (9), closeout (10) |
| [sos/](sos/) | todo: index of where SOS is specified today; no module spec yet |
| [fall-detection/](fall-detection/) | todo: same, for fall detection |

## Frontmatter fields

| Field | Values |
|---|---|
| `status` | `draft`, `approved`, `implemented`, `deprecated`, `todo` |
| `support` | `active`, `deprecated` (variant still builds but the platform does not cater for it) |
| `hw_verified` | `none`, `partial`, `yes`: whether a dated run in [docs/test-runs/](../docs/test-runs/README.md) backs the behaviour |
| `verified` | the commit and date the document was last checked against the code, and what was checked |
