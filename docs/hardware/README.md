---
id: hardware
type: section
status: approved
owner: KhoaDD
summary: Hardware design records per variant, the project budget and BOMs, and product concept art.
---

# Hardware documents

Moved from `.kiro/docs/` and `.kiro/docs/artifacts/` on 2026-10-29 with `git mv`; content is
unchanged. Variant definitions are in [../variants.md](../variants.md).

| Path | Contents |
|---|---|
| `concept/` | Product concept render and front-panel layout (`treklink-concept.png`, `concept-layout.png`) |
| `budget/` | Project funds (`actual-available-funds.md`), the v1 purchase ledger (`Bills of Materials - BOM.csv`), the revised development BOM, three build versions `v0.0.2` to `v0.0.4` at 17 units each (`Development BOM.xlsx`, `development-BOM.csv`) |
| `v1/assembly-guide.md` | Perfboard assembly guide for the two v1 units |
| `v2/` | Custom PCB design: hardware analysis, finalized specification (`implementation-plan-2.0.md`), PCB design requirements, component list, freelancer brief and quote, PCB service template, schematic review |
| `v2/bom/` | LCSC and Thegioiic BOMs plus their blank templates |
| `v2/schematic/` | The freelancer's 7-page KiCad schematic (PDF and per-page PNG) and the power schematic |
| `v2/enclosure/` | Enclosure specification, PCB dimensions and layout drawings |
| `v2/requirements.zip` | Archive of the v2 requirements pack dated 2026-06-03: brief, specification, PCB requirements, component list, LCSC BOM and five enclosure drawings not stored elsewhere |

v3 and v4 are off-the-shelf LilyGo boards and have no hardware design records. Earlier revisions of
the v2 documents are in [`../.deprecated/2026-07-27-v2-hardware-revisions/`](../.deprecated/2026-07-27-v2-hardware-revisions/).
