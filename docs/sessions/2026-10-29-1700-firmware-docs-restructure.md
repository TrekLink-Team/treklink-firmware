# 2026-10-29 17:00: firmware documentation restructure (D-076)

- **Goal**: audit the v1 to v4 documentation and restructure the repository's specs and docs per the conventions (option A, leader-approved).
- **Baseline**: `origin/dev` at `09f2a81`.
- **Leader facts (2026-10-29)**: v1 deprecated, mesh only, not catered for by treklink-web; v2 SOS trigger works, not yet verified (cancel transmits nothing, no acks); v3 and v4 SOS untested; v3 and v4 are the LilyGo T-Beam v1.2 and T-Beam Supreme; Kiro IDE no longer used.
- **Done**: every document under `.kiro/`, `.agents/`, `_handoff/`, the root `specifications.md` and `answers.md`, and the old `docs/*.md` moved with `git mv` into `specs/` and `docs/` (layout in `docs/README.md`); nothing deleted. Spec frontmatter and corrected status lines; `docs/variants.md`; `docs/build-and-flash.md` (absorbs the README flashing sections); `docs/test-runs/`; `docs/.deprecated/` with reasons; link repairs; `.trunk/trunk.yaml` exemption moved from `_handoff/**` to `docs/**`.
- **Findings**: v3 and v4 specs promise alarms the boards cannot drive (`TrekLinkSOSHelper.cpp:66-78`); cancel sends nothing (`:49-53`). Both recorded in the treklink-docs risk register with D-076.
- **Not done**: em dash sweep of the moved pre-2026-09-22 specs; consolidated `sos` and `fall-detection` module specs (index pages with `status: todo`); code fixes.
