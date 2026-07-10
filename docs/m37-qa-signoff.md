# M37 Final QA Sign-off (Slices A-E, tag target v1.0.38)

- Milestone ID: M37 (hardening + two frontend features), DEC-0055 (A/B/C) + DEC-0056 (D/E)
- Commit range verified: `e845b1e..HEAD` (HEAD = `85827e5`), 4 commits / 5 slices
  - A `861eec1` — openMSX VDP-IRQ A/B probe (tooling/docs only)
  - B `85b48aa` — inserted FM-PAC OPLL mixed into machine audio (additive)
  - C `85827e5` — WD2793 read-sector rotational first-DRQ latency
  - D+E `cd459a3` — `--speed` CLI + SDL3 window scaling/fullscreen (one commit, two slices)
- QA performed from the ONE canonical `build/` (clean wipe -> `tools/bootstrap-build.ps1`, SDL3=ON superset)
- Date: 2026-07-10

## 1. Regression Scope

Behavior-affecting surface this cycle: the machine audio mixer (Slice B), the WD2793 Type-II
Read-Sector timing path (Slice C, highest risk), and the SDL3 frontend CLI/window/renderer
(Slices D/E). Slice A is tooling/docs only (no `src/` change). The regression matrix therefore
covers: audio mixing + the pre-existing audio-fidelity oracles; FDC read/boot/disk-save/multi-disk;
CPU/core untouched-proof (ZEXALL withhold justification); frontend CLI parsing + window/renderer
wiring; asset + build evidence gates.

## 2. Regression Matrix Status

| Area | Evidence | Result |
| --- | --- | --- |
| cpu/core untouched (ZEXALL withhold) | `git diff --stat e845b1e..HEAD -- src/devices/cpu src/core` = **EMPTY** | PASS |
| Full deterministic suite (excl. #128 ZEXALL) | `ctest -E hbf1xv_m24_zexall_system_test` | **210/210 passed, 0 failed** |
| Total registry / exclusion accounting | `ctest -N` = 211 total; `-E` set = 210; excluded = exactly #128 | PASS (1 excluded) |
| Slice B — zero-FM-PAC byte-identity oracle | `frontend_machine_audio_mixer_fmpac_unit_test` (#166), non-vacuity guard present | PASS |
| Slice B — pre-existing audio-oracle sweep | #150, #165, #185 (loudness), #186 (ultrasonic-silence), #187 (midband) | PASS (built-in path unchanged) |
| Slice C — wd2793 type2 (re-derived + new rotational oracle) | `devices_fdc_wd2793_type2_unit_test` (#70) | PASS |
| Slice C — FDC/boot/disk-save/disk-read/multi-disk | #69-74, #172, #179, #155, #154, #208, #153 | PASS |
| Slice C — adversarial mutation (oracle liveness) | drop rotational term -> **exactly 5 targeted failures**, restored byte-identical | PASS (oracle non-vacuous) |
| Slices D/E — CLI additive discipline | `frontend_sdl3_cli_unit_test` (#139) cases 1-9 unchanged, case 14 additive guard | PASS |
| Slices D/E — window/renderer wiring | `frontend_sdl3_app_scaling_integration_test` (#211), pixel test (#199) | PASS |
| Evidence gate — assets | `tools/validate-assets.ps1` = True (7 BIOS incl. f1xvmus.rom, 5 ROMs) | PASS |
| Evidence gate — executables | `build/Debug/sony_msx_headless.exe` + `sony_msx_sdl3.exe` built | PASS |

### Per-slice verdicts

- **A (VDP-IRQ A/B probe) — PASS.** Tooling/docs only, no `src/` change. `docs/openmsx-vdp-irq-parity.md`
  records PASS `0->1->0->1`, grounded in openMSX `VDP.cc:1186-1198`. No regression surface.
- **B (FM-PAC OPLL mix) — PASS.** Purely additive: the 2/3/4-arg mixer overloads now delegate to
  the new 5-arg (`FmSources`) overload with an all-null array, so `fm_pac_sum == 0 -> +0 ->`
  byte-identical for any input. The hard oracle (case 1) compares the pre-M37 4-arg path against the
  new path over a rich PSG + keyed built-in-OPLL + SCC input and includes an explicit
  `ZeroFmPac_OracleInputIsNonSilent_NonVacuous` guard (not two zero buffers). Wired into production
  at `sdl3_app.cpp:443-447` (fmpac(1)/fmpac(2) queried fresh each frame). The pre-existing
  loudness/ultrasonic/midband audio oracles remain green -> the built-in audio path is provably
  unchanged.
- **C (WD2793 read rotational latency) — PASS (documented approximation).** Scoped to READ SECTOR
  only (`kReadStartCycles` preserved for write/read-address/read-track/write-track). Oracle
  discipline holds: the ONLY FDC test file touched is `wd2793_type2_unit_test.cpp`; the missed-DRQ
  oracle was **strengthened** (added exact `first_drq == 5358` assertion), a new rotational oracle
  was added, and **nothing was weakened** (no loosened tolerance, no deleted assert, no `#if 0`).
  All five re-derived deadlines were independently recomputed and are internally consistent with the
  model `L = issue + ((k*P/9 + P - phase) mod P) + 5358`, P=715909:
  `5358 / 84903 / 323539 / 641721 / 721267` — all confirmed. The openMSX A/B artifact
  (`docs/openmsx-fdc-read-timing-ab.md`) corroborates the sawtooth/spread: openMSX spread
  675404-23704 = **651700**; model spread 631779-5358 = **626421** — both vary across a ~1-rotation
  band (a fixed-latency FDC would show 0). Source grounding verified against
  `references/openmsx-21.0/src/fdc/WD2793.cc:544/557/624` (getNextSector rotational time -> `startReadSector`
  first DRQ at `gapLength + 1 + 1`). **Second-reference note:** `references/fmsx-60/source/EMULib/WD1793.c:251`
  asserts DRQ immediately (zero rotational latency); the two references DISAGREE, and this emulator
  correctly follows the more-accurate openMSX/real-hardware model (a sector must physically rotate
  under the head before data appears). Honestly disclosed as a sector-vs-track APPROXIMATION
  (evenly-spaced sequential sectors, not openMSX's real addrIdx interleave) — bounded-consistent,
  not byte-identical.
- **D (`--speed <0..7>` CLI) — PASS.** Fully automated-covered. Additive `speed_level` field; shared
  `parse_speed_level` validator used by both SDL3 and headless (`main.cpp`); range policy mirrors
  `--max-frames`. `--speed` absent -> `nullopt` -> controller untouched (level 0, byte-identical).
  Applied AFTER `cold_boot()` (verified by scaling integration test: level 5 survives, default = 0).
  F6/F7 runtime stepping unchanged.
- **E (SDL3 scaling/fullscreen) — PASS on wiring; visual/interactive residual for human.** Additive:
  window `SDL_WINDOW_RESIZABLE`, `SDL_SetRenderLogicalPresentation(320,240,LETTERBOX)`,
  `--filter` default `SDL_SCALEMODE_LINEAR` (= renderer's own default = byte-identical to before),
  `--scale` default keeps 640x480 (= scale 2). Alt+Enter toggle intercepted BEFORE input_mapper
  dispatch with `!event.key.repeat` guard and `SDL_KMOD_ALT` mask (same discipline as F10/F11/F12) so
  RETURN never leaks into the MSX matrix. Integration test reads back real SDL3 state
  (`SDL_GetWindowFlags`, `SDL_GetRenderLogicalPresentation`, presenter `scale_mode()`) and honestly
  does NOT assert the runtime fullscreen transition or the visual look.

## 3. Failures and Risk Ranking

No test failures. No blocker-, critical-, or high-severity findings. Residual items are inherently
un-automatable (visual / interactive / real-game-timing) and are ranked below.

- **Medium — Slice C real-game disk-timing (YS II).** Slice C materially changes the Read-Sector
  first-DRQ latency (from a flat 228 cycles to a rotational 0..~721k-cycle band). All automated
  disk paths (boot, disk-save persist, multi-disk hot-swap, m16 FDC integration) pass and the model
  faithfully follows openMSX (which itself runs YS II), but no automated YS-II regression exists and
  this emulator's sector model is a documented approximation of openMSX's interleave. Both DEC-0055
  and DEC-0056 explicitly require a human YS-II re-verify after Slice C.
- **Low — Slice E visual/fullscreen.** The scaled picture, `--filter` look, drag-resize, and the
  Alt+Enter fullscreen transition cannot be asserted under the dummy video driver. These are SDL
  presentation only — they cannot affect emulation determinism or any test — but the human requested
  these features (DEC-0056) and should confirm they deliver before the tag.

## 4. Required Fixes

None. No code changes required for sign-off. The items in Section 3 are human live-verify actions,
not defects.

## 5. Sign-off Decision: **Conditional Pass**

All mechanical/automated regression criteria PASS: cpu/core provably untouched (ZEXALL withhold
justified), 210/210 deterministic tests green, every M37 oracle verified non-vacuous (including an
adversarial mutation of the delicate Slice C rotational oracle), additive discipline confirmed on
B/D/E, and both evidence gates (assets + both executables) green. No blocker remains.

The v1.0.38 tag is **gated on the two human live-verify actions below** — driven by the two residual
risks that cannot be asserted headlessly (and explicitly called for by DEC-0055/DEC-0056):

1. **REQUIRED (Slice C, Medium risk) — YS II disk re-check.** Boot the two-disk YS II set on
   `sony_msx_sdl3.exe`, confirm both the initial boot load and an in-game interior/disk load (the
   M36 flagship path) complete correctly with the new rotational read-latency model. This is the one
   residual that could mask a real disk-I/O regression no automated test covers.
2. **REQUIRED (Slice E, Low risk) — SDL3 visual verify.** Launch with e.g.
   `--scale 4 --filter linear` (then `--filter nearest`), confirm the picture is aspect-correct
   letterboxed and undistorted, drag-resize scales live, and `Alt+Enter` toggles fullscreen in BOTH
   directions with no stray RETURN reaching the emulated keyboard. Also spot-check `--speed 7` visibly
   slows the CPU duty cycle (Slice D — automated for the initial-level wiring, but the felt effect is
   worth a glance).

On successful completion of both, this Conditional Pass converts to a full PASS and v1.0.38 may be
tagged. If either surfaces a defect, it returns to the developer (Slice C timing regression would be
blocker-level; Slice E visual issue is a non-blocking follow-up fix).

### Sign-off Conditions (checklist for the coordinator/human)

- [ ] Human YS II two-disk live re-check passes (boot + in-game disk load).
- [ ] Human SDL3 `--scale`/`--filter`/resize/Alt+Enter visual verify passes.
- [ ] (Optional) `--speed 7` felt slow-down spot-check.

---

### Assumptions / verification notes

- Assumption: the A/B artifacts were captured against openMSX 19.1 on WSL (the binary flavour stated
  in both docs), while the repo reference tree is 21.0. Both docs disclose this and assert the
  read-sector rotational model (`getNextSector` / `startReadSector`) is unchanged 19.1->21.0.
  Verification: confirmed the cited semantics in `references/openmsx-21.0/src/fdc/WD2793.cc:544-644`;
  I did not re-run openMSX myself (no WSL/openMSX invocation in this QA pass), so the numeric openMSX
  columns are taken as the developers' captured measurements, not re-measured. Internal consistency
  and source grounding were independently verified.
- The `docs/asset-checksums.txt` change in this range is a benign report regen that picked up a new
  untracked playtest ROM (`king_s_valley_2.rom`) in `roms/`; the `[bios]` section is byte-unchanged.
- The adversarial Slice C mutation was performed non-destructively (DEC-0049): `cp` backup ->
  edit -> build -> test -> restore from backup; `git status`/`git diff` confirmed the tree
  byte-identical afterward, and the affected target was rebuilt from the restored source.
