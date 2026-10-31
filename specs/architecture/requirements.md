---
id: architecture-requirements
type: requirements
status: draft
owner: KhoaDD
summary: How TrekLink code sits on top of stock Meshtastic so that an upstream release can be taken without breaking TrekLink, and TrekLink cannot break stock code.
verified: {commit: 54e0d8d, date: 2026-10-31, scope: "upstream hooks cited in design.md against v2.7.26.54e0d8d"}
---

# TrekLink firmware architecture: requirements

## 1. Why

Until 2026-10-31 the fork was one snapshot commit of Meshtastic 2.7.19 with TrekLink code mixed
into it. 18 stock source files carried TrekLink edits, several of them unguarded, so stock boards
built TrekLink code and changed behaviour; two upstream submodules had been deleted; about 60 stock
files differed only in line endings or file mode. Upstream moved 522 commits and 866 files between
the fork's base `538a5f0df` and `v2.7.26.54e0d8d`, about 2500 changed lines in the 17 stock files
TrekLink had edited, so no upstream pull could merge cleanly. The leader ruled on 2026-10-31 that the
code base is reorganised into a TrekLink overlay before any firmware fix lands.

## 2. Glossary

| Term | Meaning |
|---|---|
| Upstream tag | The Meshtastic release the TrekLink branch sits on, today `v2.7.26.54e0d8d` |
| Stock path | Any path that exists in the upstream tag |
| Overlay | Every TrekLink path: `treklink/`, `variants/<arch>/treklink_*/`, `test/treklink_*/`, `docs/`, `specs/`, and the TrekLink-owned files of design §2.3 |
| Override | A TrekLink change to stock behaviour made from the overlay, with the stock code left in place (design §4) |
| Capability | A property of a variant's hardware that TrekLink code reads, such as "has a buzzer" (design §3) |

## 3. Requirements

### REQ-ARC-01: Stock paths stay as released

1. The system SHALL keep every stock path byte-identical to the upstream tag, except the
   TrekLink-owned files listed in design §2.3.
2. WHEN a commit changes a stock path that is not listed in design §2.3, THEN the overlay check
   (`treklink/tools/check_overlay.py`) SHALL fail and name the path.

### REQ-ARC-02: TrekLink code lives in the overlay

1. The system SHALL keep TrekLink source code under `treklink/src/`, TrekLink variants under
   `variants/<arch>/treklink_<name>/`, and TrekLink native tests under `test/treklink_*/`.
2. The system SHALL compile `treklink/src/` only in TrekLink environments, through each
   environment's `build_src_filter`.
3. WHEN a stock environment such as `tbeam` or `tbeam-s3-core` is built, THEN no TrekLink source
   SHALL be compiled into it.

### REQ-ARC-03: Overrides are catalogued

1. The system SHALL change stock behaviour only through the override mechanisms of design §4, in
   this order of preference: an upstream hook or define, a configuration default, replacing a stock
   object at start-up, a linker wrap of a stock symbol.
2. Every override SHALL appear in the catalogue of design §4.2 with its mechanism, the stock symbol
   or setting, the upstream file and line it depends on, and the reason.
3. WHEN the branch is moved to a new upstream tag, THEN the overlay check SHALL report every
   override whose upstream dependency changed between the two tags.

### REQ-ARC-04: Variants name hardware, shared code reads capabilities

1. The system SHALL confine pin numbers, chip names and board identity to the variant's own
   directory.
2. Shared TrekLink code SHALL test capabilities (design §3), not variant names. A test of
   `TREKLINK_V2`, `TREKLINK_V3` or `TREKLINK_V4` outside `variants/` SHALL fail the overlay check.
3. WHEN a variant's capability set is incomplete or contradictory, THEN the build SHALL stop with
   a compile-time error naming the variant and the capability.

### REQ-ARC-05: Each variant has a component specification

1. Each supported variant SHALL have a specification in `specs/variants/<vN>/` that lists its
   board, MCU, radio, GNSS, IMU, display, inputs, outputs, power path and capabilities, each
   with its source (variant file line, schematic or vendor document).
2. WHEN the specification and the code disagree, THEN the code SHALL be treated as wrong only after
   a hardware run decides (chapter 17 §4 rule 6).

### REQ-ARC-06: History follows upstream

1. The TrekLink branch SHALL be a linear series of TrekLink commits on top of the upstream tag.
2. WHEN upstream publishes a stable release, THEN a `chore/upstream-sync-<version>` branch SHALL
   rebase the TrekLink series onto the new tag, pass the overlay check, the build matrix and the
   regression set of chapter 17 §3, and record the new tag in design §5.

### REQ-ARC-07: Behaviour is preserved by the move

1. The reorganisation SHALL keep every TrekLink behaviour the 2026-10-29 run observed, except the
   deliberate changes listed in design §6.
2. WHEN a behaviour changes, THEN design §6 SHALL say what changed and why, before the code.

### REQ-ARC-08: The firmware identifies itself

1. WHEN a TrekLink image boots, THEN it SHALL log the TrekLink variant name, the TrekLink overlay
   version and the upstream version it is built on.
2. The client API SHALL report the build environment name (stock `MyNodeInfo.pio_env`), so a tool
   can tell v2, v3 and v4 apart without guessing from USB identifiers.

### REQ-ARC-09: People can build and flash without an assistant

1. The system SHALL provide a terminal tool, `treklink/tools/tk.py`, that a person runs from a
   menu to detect a connected unit, read it, build an image, flash the application only, install
   from scratch, and capture a serial log.
2. The tool SHALL run on the Python that PlatformIO installs, with no package beyond those
   PlatformIO already ships.
3. The tool SHALL print the v2 power-latch warning and wait for a key press before any flash or
   reset of a v2.
4. Every step the tool takes SHALL be a printed command a person can run by hand.
