---
id: architecture-tasks
type: tasks
status: draft
owner: KhoaDD
summary: Steps that move TrekLink onto upstream v2.7.26 as an overlay.
---

# TrekLink firmware architecture: tasks

Branch `chore/overlay-on-upstream-2-7-26`, based on upstream `v2.7.26.54e0d8d`.

- [x] 1. Base the branch on the upstream tag and carry the TrekLink repository files (AGENTS.md,
      CLAUDE.md, README.md, docs/, specs/, Checksum workflow) over from `dev` `f062a35`.
- [x] 2. This spec, and the per-variant component specs in `specs/variants/v2` to `v4`.
- [x] 3. Move the TrekLink sources into `treklink/src/` by area, with capabilities in place of
      version flags (design §3.2). Keep the logic as it is except design §6.
- [x] 4. Variants `treklink_v2`, `treklink_v3`, `treklink_v4` with `lateInitVariant()` and the
      catalogue defines (design §4.2 O-03, O-05, O-06, O-10).
- [x] 5. `VariantDefaults.cpp` (O-01, O-02, O-04) and `InputCancelObserver` (O-07).
- [x] 6. MQTT queue override by linker wrap (O-08) and the health report (O-09); move the native
      test to `test/treklink_queue/`.
- [x] 7. `check_overlay.py` (design §4.3), run in CI.
- [x] 8. Build matrix of design §7.2 and the native test: treklink-v2, -v3, -v4, tbeam, tbeam-s3-core build (2026-10-31; v3 uses 98.0 % of its app partition against 89.6 % for stock tbeam); queue test 31 of 31 on a host build.
- [x] 9. `tk.py`, the menu tool (REQ-ARC-09), and `docs/build-and-flash.md` rewritten around it.
- [x] 10. Diagnostic `TREKLINK_TEST_HOOKS` image for unattended button presses.
- [ ] 11. Hardware run on v2 (unattended logs done 2026-10-31, `docs/test-runs/2026-10-31-v2-overlay-sos-pattern.md` on `fix/sos-alarm-pattern`; audible, felt and cold-boot checks open); v3 and v4 when the leader has them on the bench.
- [ ] 12. Docs in the same pass: `docs/variants.md`, `docs/coding-conventions.md` paths, specs
      index, ground truth §5 paths and the D-077 amendment on treklink-docs.
- [ ] 13. Leader: force-push the new history to `dev` and `main` after review, keeping the old `dev`
      as tag `archive/pre-overlay`.
