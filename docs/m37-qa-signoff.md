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

---

# Slice F delta / final v1.0.38 sign-off (A-F)

- Delta added: **DEC-0057** live-verify PASS + new **Slice F** (`83a0888`, SDL3-frontend-only).
- Range now verified: `e845b1e..HEAD` (HEAD = `83a0888`), 7 commits / 6 slices. Slice F touches
  ONLY `src/frontend/{sdl3_app,sdl3_cli,sdl3_main}.{cpp,h}` (+ two test files + a checksum regen) —
  zero `src/devices/cpu` / `src/core` / VDP / PSG / memory / slot edits.
- QA re-run performed from the ONE canonical `build/` after a **fully pristine from-scratch
  bootstrap** (full wipe incl. the vendored SDL3 install -> `tools/bootstrap-build.ps1`, SDL3=ON
  superset). Date: 2026-07-10.

## Precondition: A-E converted to full PASS (DEC-0057)

The prior Conditional Pass gated on two human live-verify actions. Per **DEC-0057** the human
LIVE-VERIFIED the candidate ("YS II loads and plays perfectly, SRAM works perfectly, screen looks
good"), discharging **both** Gate 1 (Slice C YS II two-disk boot + interior/disk load + save under
the new WD2793 rotational read-latency — no regression) and Gate 2 (Slice E SDL3 scaling/visual).
Slices **A-E are therefore full PASS**. No re-litigation below; this section is the Slice F delta +
whole-release no-regression confirmation only.

## F. Regression Scope (delta)

Behavior-affecting surface added by Slice F: (F1) an off-by-default `--capture <on|off>` gate on the
F10 stream-capture host hotkey, and (F2) a default-window-size change 640x480 (scale 2) -> 960x720
(scale 3). Both are SDL3-frontend-only, additive, and off/inert by default. The regression matrix
extends the A-E matrix with the F10-gate differential and the re-derived default-window oracle; it
also re-confirms the A-E automated oracles are still green after the Slice F rebuild.

## F. Regression Matrix Status (delta + whole-release re-run)

| Area | Evidence | Result |
| --- | --- | --- |
| cpu/core untouched — WHOLE range | `git diff --stat e845b1e..HEAD -- src/devices/cpu src/core` = **EMPTY** | PASS |
| Full deterministic suite (excl. #128 ZEXALL) | `ctest -E hbf1xv_m24_zexall_system_test` from pristine build/ | **210/210 passed, 0 failed** (67.87 s) |
| Registry / exclusion accounting | `ctest -N` = **211** total; `-E` set = **210**; excluded = exactly #128 ZEXALL | PASS (1 excluded) |
| ZEXALL withhold justification | durable RC all-pass log `docs/m31-rc-zexall-log.txt` (`ok_markers=67 error_markers=0` both ZEXALL+ZEXDOC); cpu/core LOGIC byte-identical to v1.0.37 | PASS |
| F1 — `--capture` parse oracle | `sdl3_cli_unit_test` Case 15: on->true / off->false / absent->false / bad->error+false / missing->error+false / order-independent w/ `--stream-light` — all HARD asserts | PASS |
| F1 — F10-gate end-to-end (non-vacuous) | scaling integ Cases 5+6: identical injected `SDL_EVENT_KEY_DOWN`/F10 via real `run_one_frame()`->`poll_and_dispatch_events()`; **off -> stream inactive**, **on -> stream active** | PASS (two-sided differential) |
| F2 — default window 960x720 (HARD) | scaling integ Case 4: config field == 960x720 **AND** live `SDL_GetWindowSize` read-back == 960x720 | PASS |
| F2 — scale-absent parser assert preserved | `sdl3_cli_unit_test` Case 11: `!pabsent.scale.has_value()` predicate UNCHANGED (only label re-derived); `--scale 3`==3, bounds/non-numeric/missing error asserts intact | PASS (not weakened) |
| Additive discipline — sdl3_cli | pre-existing Cases 1-14 unchanged; only additive Case 15 + one label rename | PASS |
| No-regression — Slice B audio | `frontend_machine_audio_mixer_fmpac_unit_test` (#165) | PASS |
| No-regression — Slice C FDC/boot/disk | wd2793_type2 (#70), m16_fdc (#74), boot suite (#13/#49/#67/#75/#178/#171), disk-save (#154), multi-disk (#207) | PASS |
| No-regression — Slice D/E scaling/pixel | `sdl3_app_scaling_integration_test` (#210), video-presenter pixel (#198), `sdl3_cli_unit_test` (#138) | PASS |
| Evidence gate — assets | `tools/validate-assets.ps1` = **True** (7 BIOS incl. `f1xvmus.rom`, 5 ROMs) | PASS |
| Evidence gate — executables | pristine `build/Debug/sony_msx_headless.exe` (2.08 MB) + `sony_msx_sdl3.exe` (2.04 MB) built | PASS |

> Test-number note: the `-E` run renumbers 1..210 (ZEXALL #128 removed), so tests after #128 read
> `N-1` vs the A-E section's full-registry numbering (audio 166->165, cli 139->138, scaling 211->210,
> pixel 199->198, disk-save 155->154, multi-disk 208->207). No test was lost; #70 (pre-#128) is
> unchanged. Total registry count is unchanged (Slice F added test *cases*, not new test binaries).

### Slice F per-item verdicts

- **F1 (`--capture <on|off>` F10 gate) — PASS.** Source: `sdl3_app.cpp:352` guards the F10 branch on
  `config_.capture_enabled && ... SDL_SCANCODE_F10 && !repeat`; when OFF the event is NOT consumed and
  falls through to `input_mapper_.dispatch_event()` where F10 is unmapped -> zero matrix effect, no
  toggle, no log (byte-identical default gameplay). F11 disk-swap (`:329`), F12 snapshot (`:338`),
  Alt+Enter fullscreen (`:364`) are untouched. Parse: `sdl3_cli.cpp:171-183` mirrors the `--filter`
  value policy (on/off else `.errors`). The F10-gate oracle is **non-vacuous by construction**: Cases
  5 and 6 push the IDENTICAL fresh-F10 key-down event through the REAL event loop and differ ONLY in
  `config.capture_enabled`, yielding OPPOSITE observable outcomes via `stream_capture_active()`
  (false vs true). This is mutation-equivalent (Case 5 = gate-closed reality, Case 6 = gate-open
  reality) — a stronger non-tautology proof than a one-sided mutation, so no separate adversarial
  mutation was required for Slice F.
- **F2 (default `--scale 3 --filter linear`) — PASS.** `Sdl3AppConfig` default `window_width/height`
  moved 640x480 -> 960x720 (`sdl3_app.h:69-70`); `sdl3_main.cpp:121-124` still overrides ONLY when
  `--scale` is present (320N x 240N, N in [1,8]); logical presentation stays 320x240 letterbox
  (Slice E, asserted Case 3). The change is a legitimate re-derivation, NOT a weakening: the
  scale-absent parser contract (`!scale.has_value()`) is preserved verbatim (only the test label was
  re-derived), and the new default is a HARD double assert (config field AND live `SDL_GetWindowSize`
  read-back). `--filter` default was already linear (no behavior change).

## F. Failures and Risk Ranking

No test failures. No blocker/critical/high finding. Two residual items, both low-risk:

- **Low (informational, non-blocking) — trivial stale code comment.** `sdl3_main.cpp:120` still reads
  `// ... absent keeps the default 640x480 (= scale 2).` The CODE is correct (the 960x720 default now
  lives in `sdl3_app.h` and `main.cpp` only overrides when `--scale` is present); only the inline
  comment is stale. The user-facing `--help` text was correctly updated to `default 3 = 960x720`.
  Purely cosmetic; does not affect behavior or any assertion. Optional cleanup, does NOT gate the tag.
- **Low (live-visual, non-blocking) — two un-headless-assertable items.** (a) the 960x720 window
  actually opening on a real display, and (b) a real-session F10-inert confirmation. Both are
  **structurally covered**: (a) by Case 4's live `SDL_GetWindowSize` read-back on a real SDL3 window
  (dummy driver), (b) by the Cases 5/6 injected-event differential through the real event loop. The
  earlier A-E live-verify (DEC-0057) already exercised a real interactive SDL3 window, and Slice F
  only changes a default size + adds an off-by-default gate. **These do NOT gate the tag.**

## F. Required Fixes

None. The stale comment is an optional trivial follow-up, not a defect blocking release.

## F. Sign-off Decision (A-F): **PASS**

All mechanical/automated regression criteria PASS from a fully pristine from-scratch build:
cpu/core provably untouched across the WHOLE `e845b1e..HEAD` range (ZEXALL withhold justified against
the durable RC all-pass log + byte-identical CPU logic vs v1.0.37), **210/210** deterministic tests
green (exactly ZEXALL #128 withheld), Slice F oracles verified hard + non-vacuous (the F10 gate by a
two-sided injected-event differential; the 960x720 default by a config + live read-back double
assert), additive discipline confirmed, no A-E regression, and both evidence gates (assets + both
executables) green. Slices A-E are full PASS via the DEC-0057 human live-verify. **No blocker remains
and no tag gate remains** — the two live-visual items are structurally covered and explicitly
non-gating. **v1.0.38 (A-F) is cleared for tagging** (owner-run push per DEC-0047; coordinator owns
the tag/checkpoint — QA does not commit).

### Assumptions / verification notes (Slice F pass)

- Assumption: my first rebuild attempt's project tree vanished from `build/` after an exit-0 build (a
  transient filesystem anomaly, cause undetermined). Verification action taken: I discarded it and ran
  a **fully pristine** from-scratch bootstrap (wiped `build/` entirely incl. the vendored SDL3 install,
  rebuilt SDL3 + project), confirmed both exes + `build/CTestTestfile.cmake` persisted, and ran the
  suite from that tree — 210/210. All Slice F results above are from the pristine tree.
- The `git status` working tree was clean before and after this QA pass; only the gitignored `build/`
  was wiped/rebuilt — no tracked source was perturbed (DEC-0049 non-destructive discipline).
- I did not re-run openMSX in this pass; Slice F is SDL3-frontend-only (window default + host-hotkey
  gate) with no device-timing/parity surface, so no openMSX A/B is applicable to the delta. The
  Slice C parity evidence is unchanged from the A-E section and was live-verified by the human.
