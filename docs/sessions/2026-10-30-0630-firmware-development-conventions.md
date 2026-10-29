# 2026-10-30: firmware development conventions and the upstream check

- **Goal**: write the gated firmware development conventions after the 2026-10-29 v2 hardware run; commit and open PRs for that run's documentation.
- **Leader rulings**: split location (process in treklink-docs chapter 17, C/C++ practice here); tiered hardware gate (class S safety paths need a dated run on every affected variant); fix the TrekLink-owned defects on the current base, then sync to upstream `v2.7.26`. Recorded as D-077.
- **Upstream check (2026-10-30)**: the fork is one snapshot commit (`91134a2`); its tree matches upstream `develop` at `538a5f0df` (2026-02-03), 1520 files identical and 25 different (inferred, high). Latest stable `v2.7.26` (2026-06-24); `v2.8.1` is an Alpha on pioarduino `55.03.311` (Arduino-ESP32 3.x). Upstream fixed the calibration screen exit (`v2.7.20`) and saves compass calibration (#10166, `v2.7.26`); the IMU still sleeps with the screen off in `v2.8.1`.
- **Done**: PRs #20 here and treklink-docs#128 for the hardware run; on `docs/coding-conventions-v2`, the 2026-02-04 `coding-conventions.md` moved to `docs/.deprecated/2026-10-30-coding-conventions-v1/` (it described FreeRTOS tasks and a `hal/` tree the fork does not use) and a new one written against the code at `cd5329c`; links in `docs/README.md`, `build-and-flash.md`, `.deprecated/README.md`.
- **Repo setup**: `core.hooksPath` here points at a trunk cache directory that had no `commit-msg`; the `tk_checksum` hook was installed there.
- **Found, not fixed**: 21 commits authored by `Claude <noreply@anthropic.com>`, 7 on `origin/dev` (D-049); fixing needs a history rewrite of `dev`.
- **Not done**: firmware fixes, flashing and serial tools, the upstream sync, v3 and v4 runs.
