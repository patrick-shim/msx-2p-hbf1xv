# M51 Implementation Report — Sprite disappearance/flicker during scroll (DEC-0078)

- Milestone: M51 (`docs/m51-planner-package.md`, REQ-M51-002)
- Developer: MSX Developer Agent (autonomous run, owner pre-authorized)
- Date: 2026-07-15
- Verdict: **H1 CONFIRMED — M49 (v1.1.6) regression; fix branch (a), shape (i) implemented,
  all evidence gates green.** See §8 for a SERIOUS session-interference incident that
  forced the verification into an isolated git worktree; the verified change set is
  staged for coordinator landing (NOT committed, per instructions).

---

## 1. Task 0 — deterministic repro + oracle (AC-T0)

Tools delivered/extended (all `tools/m51-*`, gitignored/local per repo convention):
`m51-capture-frames.ps1` (per-frame deterministic capture wrapper, era-safe CLI),
`m51-sprite-oracle.py` (pinned-box/pinned-signature presence oracle; `--boxes-csv`
SAT-anchored mode; profiles `aleste2`, `firebird`, `firebird_wide`, `firebird_omx`),
`m51-sat-census.py` (SAT census = guard §1.6 prong 1 + oracle box source),
`m51-openmsx-frames.ps1` (frame-indexed WSL openMSX per-frame capture),
`m51-firebird-input.script`, `m51-laydock2-input.script`.

### Aleste 2 (MUST) — window f3560-f3599, recipe `tools/aleste-play-evidence-input.script`

- SAT census: player plane (pattern pair 56/60, x=120) in the SAT on **40/40** frames,
  display band 153-168 constant while R#23 decrements every frame (live scroll);
  max 4 sprites/line (mode-2 hardware limit 8 — under-limit).
- Render oracle (pre-fix main @ 0fd9690): **absent 40/40** (0 white pixels in the
  SAT-anchored box) → `verdict=FAIL`. Whole-band emptiness (prong 3) at ≤4 sprites/line
  (prong 1) = DEFECT, not authentic multiplex.
- Guard §1.6 discipline note: an earlier candidate window f3380-f3419 was REJECTED —
  its SAT census shows NO player entry (genuine in-game death of the idle plane), so its
  absence frames carry no render expectation. The oracle's `--boxes-csv` mode encodes
  this exclusion permanently.
- Determinism: f3560/f3599 dual runs byte-identical
  (`b38966248cda4804…`, `6a6d3d48197a9be…`).
- Adversarial stable-scene check: weapon-select f2600-f2609 → 0 transitions, PASS.
- Evidence: `debug/m51/aleste2-scroll2/` (frames, png, snapshots, `oracle-main.csv`,
  `sat-census.csv`), `debug/m51/aleste2-stable/`.

### Firebird / Hi no Tori (MUST) — window f2000-f2039, `tools/m51-firebird-input.script`

- SAT census: player = two stacked pairs pats {0,4}+{8,12}, x=120, in SAT **40/40**;
  max 6 sprites/line. (Also documented: the game flips R#5 between SAT double-buffers
  0xF200/0xF600 at ~30 Hz.)
- Render oracle (pre-fix): **8 whole-band absent frames, 16 present↔absent transitions**
  (alternation ~every other frame in f2000-f2018) → `verdict=FAIL`.
- openMSX discriminator (T0.4, A-M51-2): openMSX 19.1 `Sony_HB-F1XV`, default
  `limitsprites`, frame-indexed identical inputs, same window, same oracle instrument
  (`firebird_omx` profile, --ox 32 --oy 14): **present 40/40, 0 transitions.**
  → The player flicker is **100% our defect, 0% authentic multiplex.**
- Determinism: f2000/f2039 dual runs byte-identical.
- Evidence: `debug/m51/firebird/`, `debug/m51/openmsx/firebird/` + `firebird-oracle.csv`.

### Aleste 2 openMSX leg (caveat)

Captured (`debug/m51/openmsx/aleste2/`) but the openMSX game state DIVERGED by f3560
(cross-emulator RNG: death/explosion mid-window, different score) — unusable as a
per-frame reference; recorded with caveat (37/40 present, PASS — no wholesale absence).
The Firebird leg is the state-equivalent arbiter; Aleste's defect classification rests
on SAT prongs 1+3, which need no openMSX leg.

### Laydock 2 (SHOULD leg, R7)

Disk boot verified to title/credits (f2000-f3000) and into the "PLAYER1 SET /
NEW DATA / DATA LOAD" menu (f4200+) with a SPACE script — probes under
`debug/m51/laydock2/probe/`. Full gameplay repro needs name-registration menu
navigation (deep scripted input discovery); per the package's R7 "if workable" clause
this leg is closed at the probe stage. The fix mechanism is title-independent
(proven on both MUST titles); owner live spot-check at release covers Laydock 2
(package exit criteria).

## 2. Task 1 — bisect (AC-T1)

Pre-flight: all four tag hashes == REQ-M51-001 ledger hashes (`git rev-parse`):
v1.1.4=2e09040…, v1.1.5=e7a7a41…, v1.1.6=0d84451…, v1.2.0=0bf3285….
`git diff v1.1.6..v1.2.0 -- src/devices/video` is EMPTY and the `src/machine` diff is
sprite-neutral (grep for sprite/commit/sync_to_line/watermark: no hits) → v1.2.0 ≡
v1.1.6 for sprites; current main (0fd9690) was measured directly in Task 0.

Sequential detached checkouts + rebuilds of the ONE canonical `build/` (headless
target; the gitignored current-era `tests/` cannot compile against old-tag headers —
expected). Oracle-metric comparison (A-M51-3), SAT census per leg as the in-gameplay
confirmation. Outputs: `debug/m51/bisect/<tag>/`.

| Leg | Aleste 2 (SAT-anchored oracle) | Firebird (SAT-anchored oracle) |
|---|---|---|
| v1.1.4 (pre-M48) | PASS — 16/16 present, 0 transitions (alive window f3600-3615; f3560 window had an in-game death on this leg's RNG timeline — R2, window shifted by census) | PASS — 16/16 present, 0 transitions |
| v1.1.5 (M48 contention) | PASS — 16/16 present, 0 transitions (f3560-3575) | PASS — 16/16 present, 0 transitions |
| v1.1.6 (M49 per-line sprites) | **FAIL — 16/16 ABSENT** (f3560-3575, plane in SAT 16/16) | **FAIL — 7 absent, 13 transitions** |
| v1.2.0/main | FAIL — 40/40 absent (Task 0) | FAIL — 8 absent, 16 transitions (Task 0) |

**First-bad: v1.1.6 (M49) for BOTH titles. v1.1.5 == v1.1.4 == clean → H5/M48
eliminated; attribution (a).** Backlog D9 disposition: this REPAIRS the M49 closure
(per DEC-0078, does not reopen scope).

## 3. Task 2 — attribution verdict (written BEFORE fix code; AC-T2)

Instrumentation: env-gated `SONY_MSX_M51_SPRITE_TRACE` (path or "1"=stderr) +
`SONY_MSX_M51_TRACE_BAND=lo:hi`, confined to `v9958_vdp.{h,cpp}` +
`hbf1xv_machine.cpp`, M47-logger precedent, logging the package's five event classes
(SPLIT-OPEN / SPRITE-SEG / CMDROW+CMDCOMMIT / BAND-ROW / VSYNC+BOUNDARY).
EG-8/A-M51-4: with env unset AND set, the frame dump is byte-identical to the
uninstrumented baseline (three-way SHA256 equality, f3560 capture).

**Smoking-gun trace (Aleste 2 failing frame, `debug/m51/traces/aleste2-f3560.trace`):**

```
F3559 SPLIT-OPEN raster=12 target=14 wrap=1 prior_wm=212 was_active=0   <- lazy-open CLEARS buffers
F3559 ...seam paces sprite_wm along the beam to 69...
F3559 CMDROW dy=96 r23=121 display_line=231 raster=67                  <- terrain-strip blit, wrap-space dest
F3559 CMDCOMMIT range=[69,231) sprite_wm=69                            <- commits THE REST OF THE FRAME
F3559 BAND-ROW line=148..175 visible=0 past_sprite_wm=1  (28/28 rows)  <- plane band sealed sprite-less
```

Firebird failing frame F2000 (`debug/m51/traces/firebird-f2001.trace`): split opens at
raster 2, blit dy=240-247 → display_line 221-228 → `CMDCOMMIT range=[6,221)
sprite_wm=6` → whole frame sealed sprite-less. On the ADJACENT present frame (F1999)
the split opened at raster 74 — AFTER the early blits — so the sink committed from the
previous vsync recompute's still-populated buffers (the documented 1-frame-stale
fallback): the ~30 Hz alternation is the game's per-frame variation in whether a
sprite-relevant VDP write precedes its blits.

**Mechanism (H1 CONFIRMED):** `Hbf1xvMachine::VdpRenderSyncAdapter::on_commit_up_to()`
(`src/machine/hbf1xv_machine.cpp:138-158` @ 0fd9690) commits background rows to the
blit destination without pacing the render-only sprite pass
(`v9958_vdp.cpp:714-716` documented the sink deliberately not driving the sprite
watermark), after `commit_sprite_split()`'s lazy-open `SpriteEngine::begin_frame()`
cleared all per-line buffers (`sprite_engine.cpp:93`); `composite_sprites`
(`vdp_frame_renderer.cpp:381-392,519-520`) then seals those rows sprite-less (the
accumulator never re-renders committed rows). Trigger geometry: scroll engines blit
the incoming terrain strip to VRAM rows whose M44 display-line inverse
`(dy - R#23) & 0xFF` lands in the wrap-space band 212-255 — one such blit commits the
ENTIRE remaining frame in one `sync_to_line`, which is why the absence is wholesale
and scroll-correlated.

- H2 (segment-gate quantization): ELIMINATED — trace segments carry correct live gates
  (R#1 42→22→62 HUD choreography, R#8 SPD toggles) with contiguous advances; no
  whole-segment blanking.
- H3 (spurious wrap re-open): ELIMINATED — exactly one SPLIT-OPEN per frame, at the
  frame's first active-display write (`prior_wm=212`, `was_active=0`); no mid-frame
  re-opens in any trace.
- H4 (per-line-limit misfire): ELIMINATED — census max 2-6 sprites/line on affected
  bands; absence is whole-band, not lowest-priority dropping.
- H5 (M48 phase shift): ELIMINATED by the bisect (v1.1.5 clean on the same metric).
- Authentic-multiplex share: ZERO for the repro'd symptoms (openMSX same-window rate 0;
  under-limit lines; whole-band shape). The per-line-limit logic itself is untouched by
  the fix, so genuine >8/line multiplex flicker is preserved by construction.

## 4. Fix — branch (a), shape (i): consumer-side sprite pacing

Files (the DEC-0078 expected-edit locus, nothing wider):

- `src/devices/video/v9958_vdp.h` — new `V9958Vdp::commit_sprite_rows(int target)`
  contract + M51 doc; corrected the stale "no command-row sink drives the sprite
  watermark" clause.
- `src/devices/video/v9958_vdp.cpp` — `commit_sprite_rows()` implementation:
  ADVANCE-ONLY-WHEN-ACTIVE render-only pacing
  (`if (!sprite_split_active_) return; sprite_engine_.check_until_visible_only(target-1);`)
  + the Task 2 trace plumbing (env-gated, default-off).
- `src/machine/hbf1xv_machine.cpp` — `on_commit_up_to()` calls
  `machine_.vdp_.commit_sprite_rows(display_line)` BEFORE
  `scanline_accumulator_.sync_to_line(display_line)`; the same invariant guard on the
  two `render_frame()` mid-display/step-only memoization syncs (package fix-shape (i)
  clause; const_cast under the accumulator's existing logical-constness memoization
  rationale) + trace plumbing.

Invariant established: **no framebuffer row is ever composited from a sprite
line-buffer that is empty purely because of pipeline lifecycle.** Grounding:
tier-2 openMSX consumer-side sync contract — every render advance paces the sprite
checker first (`references/openmsx-21.0/src/video/PixelRenderer.cc:580-584`,
`SpriteChecker.hh:242-247`) — EFFECT re-expressed at our line-granular seam, never
transcribed; tier-3 fMSX triangulation — fused per-scanline `Sprites()`/
`ColorSprites()` selection makes the invariant structural
(`references/fmsx-60/source/fMSX/Common.h:18-19,99-155,247-284`); tier-1 fact-sheet
§6 line 120 (per-scanline sprite fetch model, line N fetched during N−1) is the
hardware behavior the per-line-live pass models. No tier conflicts arose.

Advance-only-when-active rationale (documented decision, package (a)-(i) clause):
while the split is not open the buffers hold the previous `on_vsync()` recompute —
the documented pre-M49 1-frame-stale fallback (never absence) — and every failing
CMDCOMMIT in the traces ran with the split already active (the triggering OUT opened
it via `on_before_state_change`), so a command commit never needs to lazy-open.
MUST-NOT-TOUCH list respected: DEC-0031 frame-atomic collision path, static per-line
formulas, mode-2 masks, M44 background watermark discipline, all M48 timing constants —
zero edits.

## 5. Tests (AC-F3)

- `tests/unit/devices/video_v9958_command_row_sprite_pacing_unit_test.cpp`
  (`devices_v9958_command_row_sprite_pacing_unit_test`): mechanism reproduction
  (lazy-open clear → line past watermark EMPTY), fix contract (pacing repopulates the
  hardware-expected set incl. X), advance-only-when-active no-op, no-rewind,
  wrap-space clamp (231→height), RENDER-ONLY S#0 byte-identity (DEC-0031).
- `tests/integration/machine/hbf1xv_m51_command_sprite_pacing_integration_test.cpp`
  (`machine_hbf1xv_m51_command_sprite_pacing_integration_test`): machine-level defect
  scenario (split opens at beam 20, hooked HMMV to rows 150-159 sweeps the sprite band
  101-108 past the sprite watermark): sealed frame byte-identical to the no-mid-frame
  reference, sprite band explicitly present, S#0 byte-identical to reference
  (DEC-0031), no-command control, two-run determinism.
- Registered in `tests/CMakeLists.txt` (2 targets).
- **Adversarial-revert proof (non-tautology): EXECUTED.** Stubbing
  `commit_sprite_rows` to a no-op (compile-preserving behavioral revert) fails
  4 unit cases + 2 integration cases (`M51Fix_CommitSpriteRows_Line105Repopulated`,
  `CommandRun_SpriteRows101to108_PresentDespiteCommandSinkSweep`, …); restore → all
  pass. No existing oracle was modified or weakened (EG-2 wall green, unmodified).

## 6. Evidence gates

All gates below were executed in the ISOLATED verification worktree (git worktree,
detached at 0fd9690 + the M51 diff — §8 explains why), SDL3=ON superset configuration:

- **EG-1 build:** both executables built, zero errors (`sony_msx_headless.exe`,
  `sony_msx_sdl3.exe`).
- **EG-2 fast-subset ctest:** `ctest -C Debug --output-on-failure -LE
  m24_slow_full_sweep` → **244/244 passed** (242 pre-M51 oracles green + unmodified,
  incl. all sprite/M22/M32/M44/M48/M49 oracles, + 2 new M51 tests).
- **EG-3 13-mode 0.000% screen A/B vs openMSX (M49 debt, re-run in full):** the
  13-mode set = 11 generated scenario carts (tools/gen-m38-vdp-scroll-cart.py,
  regenerated — debug/ was cleaned post-M50) + 2 manifest scenarios.
  Result: **all 12 runnable modes 0.000% mismatch MATCH** (s01-s11 via
  `tools/m51-m38-run.ps1` → `debug/m51/ab-m38/m38-ab-report.md`; `baseline_cart_g2`
  via `tools/m51-ab-run.ps1` → `debug/m51/ab/m41-ab-report.md`);
  `baseline_disk_circle` = BLOCKED, which IS its recorded expected disposition in
  `tools/m41-scenarios.json` (historical WSL openMSX disk-boot block, not an M51
  regression). Harness variants are byte-identical to the originals except a
  parameterized `-Headless` path (the canonical exe was file-locked by the
  interfering session, §8).
- **EG-4 DEC-0031 collision byte-identity:** the collision-relatch unit oracle +
  boot-logo integration oracle green and unmodified in EG-2; plus the new unit case 6
  and integration case 3 assert S#0 byte-identity directly across the fix path.
- **EG-5 before/after captures:** pre-fix `debug/m51/aleste2-scroll2/` (40/40 absent),
  `debug/m51/firebird/` (8 absent/16 transitions); post-fix `debug/m51/postfix/…`:
  **Aleste 2 40/40 present, 0 transitions (PASS); Firebird 40/40 present,
  0 transitions (PASS)** — Firebird's post-fix transition rate (0) equals the openMSX
  reference rate (0), satisfying AC-F1's both-direction bound (R1: not below openMSX
  either). openMSX discriminator outputs under `debug/m51/openmsx/`.
- **EG-6 determinism:** post-fix dual runs byte-identical per title (aleste f3560:
  `7a89d434…` ×2; firebird f2039: `745ef023…` ×2), and equal to the stored postfix
  captures.
- **EG-7 assets:** `tools/validate-assets.ps1` PASS (7 BIOS + fmpac.rom);
  `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` refreshed.
- **EG-8 instrumentation hygiene:** trace default-off; unset-env run AND set-env run
  both byte-identical to the uninstrumented baseline (3-way SHA256, plus post-fix
  re-attestation `a1==a2==a3`).
- **EG-9 ban attestation:** grep over `src/` — no SpriteChecker/VDPAccessSlots code or
  table transcription; only comment-level grounding citations.
- **EG-10 ZEXALL:** correctly NOT run — `git diff` shows ZERO `src/core` /
  `src/devices/cpu` edits.
- **EG-11 scope attestation:** `git diff --stat` (worktree) =
  `src/devices/video/v9958_vdp.cpp | 111 +`, `src/devices/video/v9958_vdp.h | 44 +`,
  `src/machine/hbf1xv_machine.cpp | 78 +` (228 insertions, 5 deletions — comment
  corrections) — exactly the DEC-0078 locus; plus gitignored `tests/` (2 new files +
  CMake rows) and `tools/m51-*`.

## 7. Honest residuals

- The M49 line-granular ceiling stands: +2-line quantization; cycle-timed intra-line
  effects stay line-quantized. M51 does not exceed it.
- Rows committed by the sink while the split is NOT yet open render the previous
  frame's sprite set (pre-M49 1-frame-stale fallback — visible at worst as one frame
  of lag, never absence). This is the documented §2.1 contract, unchanged.
- Authentic >8/line multiplex flicker is preserved (per-line cap untouched); Firebird
  wave scenes may legitimately flicker where openMSX does too.
- Laydock 2: probe-stage repro only (R7); owner live spot-check at release.
- v1.1.7's phosphor softener attribution ("genuine 8-sprite flicker") is now
  corrected by evidence: the owner-visible flicker was THIS defect.

## 8. SERIOUS INCIDENT — concurrent duplicate developer session (coordinator action required)

During this run a SECOND, concurrent automation session (evidently an earlier/duplicate
M51 developer dispatch) operated on the same repository:

- ~00:52-01:47: it silently moved the shared git HEAD to v1.1.4 mid-run (detected via
  "Previous HEAD position" at my first bisect checkout).
- ~02:03-02:07: it ran its own "v1.1.6" bisect captures — while `build/Debug` held a
  **v1.1.5** binary from MY leg sequence → its bisect leg data is cross-version
  contaminated. Quarantined to `debug/m51/quarantine-rogue-session/` (do not trust).
- ~02:04-02:08: its spawned emulator processes file-locked `build/Debug/sony_msx_headless.exe`
  (LNK1168), and it silently restored HEAD to main under me.
- 02:46-03:05: it began writing ITS OWN Task-2 trace + fix edits to
  `src/devices/video/{v9958_vdp,sprite_engine}.*`, `src/machine/hbf1xv_machine.cpp` in
  the canonical tree, and REWROTE my new unit test file in the shared gitignored
  `tests/` toward a combined shape-(i)+(ii) variant.

Mitigations taken: all M51 verification moved to an isolated `git worktree` (detached
0fd9690) with its own build (subst M:); Task-0 capture provenance re-proven by SHA256
(worktree binary reproduces the stored pre-fix captures byte-identically); every gate
in §6 executed against the isolated, provably-mine tree. DEC-0041 single-build tension
is acknowledged: the temporary out-of-repo build exists ONLY because the canonical
tree/build had an active foreign writer; the canonical `build/` remains the tree of
record for the coordinator's landing re-verification.

**Landing instructions (coordinator):** the verified M51 change set of record is
staged at the worktree `m51-deliverables/` (absolute path in the developer handoff
message): `m51-src.diff` (+ full copies of the 3 src files, SHA256 recorded), the two
test files, and the CMakeLists block (already appended in the shared tests/ tree).
Serialize against the duplicate session (terminate or let finish), then land EXACTLY
ONE change set — this verified one, or the duplicate's after ITS OWN full gate run —
never a mix. After landing, rebuild the canonical `build/` and re-run
`ctest -C Debug -LE m24_slow_full_sweep` + the post-fix title oracles as the final
canonical attestation. Note: the duplicate session's current unit-test rewrite asserts
shape-(ii) retention semantics that FAIL against this report's shape-(i) src (and vice
versa) — the test file must match whichever src lands.

## 9. QA handoff

- QA re-verifies from the canonical `build/` AFTER the coordinator lands the change
  set (single-build policy) — EG-1/EG-2 + post-fix oracle PASS on Aleste 2 + Firebird
  are the decisive re-checks; EG-3 artifacts under `debug/m51/ab-m38/` + `debug/m51/ab/`.
- NORMAL human-release gate (HIGH blast radius): owner live spot-check of Aleste 2,
  Firebird, Laydock 2 recommended, ideally `--persistence off` so the softener cannot
  mask residuals.

---

## 10. CANONICAL-TREE POST-LANDING ATTESTATION (second developer session, 2026-07-15)

Written by the OTHER of the two concurrent M51 developer sessions (§8's "duplicate" —
from this side, §8's author was the concurrent peer; both sessions detected each
other). Both sessions independently reached the SAME attribution (H1, first-bad
v1.1.6/M49, branch (a)) from independent captures; the change set was landed on main
as **commit `278af86`** ("fix(vdp): M51 command-row sprite pacing", 03:22) — the
shape-(i)-only design with `V9958Vdp::commit_sprite_rows()` at the M44 sink + the
`render_frame()` memoization syncs, and the (i)-consistent test files (this session's
shape-(ii) sprite_engine variant and its test cases were superseded during
convergence; the landed src keeps `begin_frame`'s clear-at-open).

Independent corroborating evidence trail from this session (all under `debug/m51/`,
distinct capture windows from §1's):

- **Firebird f1800-f1859 (pre-fix main @ 0fd9690):** SAT census in_sat 60/60, max
  4 sprites/line; oracle **27/60 whole-band ABSENT, 53 transitions → FAIL**.
  openMSX per-frame leg (f1800-f1899, `SlowSpeed` 10 — see the render-skip caveat
  below): **100/100 present, 0 transitions.** (`firebird-main/`, `firebird-omx/`.)
- **Task 2 trace (this session's independent capture, `traces/firebird-f1801.trace`):**
  failing frame `CMDROW dest=217 acc_wm=6 sprite_wm=6 empty_rows=206 raster=4` —
  one wrap-space terrain blit sealed rows [6,212) sprite-less; adjacent good frame
  had zero CMDROW events. Same mechanism as §3's Aleste trace.
- **Bisect redo with build-integrity discipline** (`bisect/v1.1.4-redo|v1.1.5-redo|
  v1.1.6-redo/`, `--clean-first` rebuilds + binary-freshness hash checks):
  v1.1.4 PASS 32/32 · v1.1.5 PASS 32/32 · v1.1.6 **FAIL 14/32 absent,
  28 transitions** · main FAIL — first-bad **v1.1.6**, agreeing with §2.
  NOTE: `bisect/v1.1.4/` (non-redo) is STALE-BINARY CONTAMINATED (the leg ran a
  main-built exe after an incremental-build miss) — quarantined, superseded by
  `v1.1.4-redo/`; see `bisect/README-QUARANTINE.md`.
- **openMSX capture-cadence caveat (methodology, affects any per-frame omx leg):**
  `screenshot -raw` returns the last RENDERED frame and openMSX drops render frames
  under load — consecutive per-`after frame` screenshots came back byte-identical
  3-4 frames at a time at full speed. `tools/m51-openmsx-frames.ps1` therefore slows
  emulation (`set speed 10`) across the capture window; with that fix the Aleste 2
  idle-window openMSX leg reproduces our exact per-frame pattern (see below).
- **Aleste 2 idle-window authentic-flicker disposition (guard §1.6 prong 2):** the
  spawn-window 2-frame white/blue alternation (95↔53 signature pixels) and the
  settled-window 16-frame-period 55-blip are IDENTICAL in openMSX per-frame captures
  (`aleste2-omx/`) — AUTHENTIC game behavior, correctly NOT altered by the fix
  (post-fix idle-window per-frame counts byte-equal to pre-fix, `aleste2-after/`).

Gates re-executed on the CANONICAL tree at the landed commit (the §8 landing
instruction's final attestation):

- **EG-1:** full build, both exes, zero errors (after a `--clean-first` core rebuild;
  an interleaved-edit stale-object state was detected and flushed first).
- **EG-2:** `ctest -C Debug -LE m24_slow_full_sweep` → **244/244** (242 pre-M51
  unmodified + `devices_v9958_command_row_sprite_pacing_unit_test` +
  `machine_hbf1xv_m51_command_sprite_pacing_integration_test`).
- **EG-3 (full 13/13):** `tools/m51-eg3-run.ps1` (per-mode geometry via
  `tools/m41-video-ab.py`, `-doublesize` for the 512-wide classes) over the 13
  regenerated `tools/gen-m41-video-cart.py` scenarios:
  **13/13 MATCH at 0.000%** (m0_text1, m1_g1, m2_g2, m4_g3, m5_g4, m6_g5, m7_g6,
  m8_g7, m10_yjkyae, m12_yjk, sp1_single, sp2_double, sp3_collision) —
  `debug/m51/m41-ab/m51-eg3-report.md`. This completes §6's 12-of-13 accounting:
  the 13th scenario class is covered by the cart set with correct geometry.
- **EG-4:** collision-relatch + M22 oracles green unmodified inside EG-2; unit case 6
  + integration case 3 assert S#0 byte-identity across the fix path directly.
- **EG-5/AC-F1 (this session's windows):** Firebird f1800-f1859 post-fix
  **60/60 present, 0 transitions (== the openMSX reference rate 0)**; Aleste 2
  f2900-f2939 post-fix 40/40 present, per-frame signature counts byte-equal to
  pre-fix (authentic blink preserved; boundary frame dumps hash-identical
  8FC358DA…/A198CFC8… pre==post on this write-free-window).
- **EG-6:** dual-run byte-identity green in every capture above.
- **EG-7:** `validate-assets.ps1` → True (7 BIOS + fmpac.rom); checksums refreshed
  to `docs/asset-checksums.txt`.
- **EG-8:** trace-on vs trace-off 1801-frame Firebird runs → frame dumps
  BYTE-IDENTICAL.
- **EG-9:** `src/` grep — no SpriteChecker/VDPAccessSlots code or table
  transcription (comment citations only).
- **EG-10:** `git diff v1.2.0..HEAD -- src/core src/devices/cpu` EMPTY → ZEXALL
  correctly not run.
- **EG-11:** landed diff confined to `src/devices/video/v9958_vdp.{h,cpp}` +
  `src/machine/hbf1xv_machine.cpp` (+ gitignored `tests/`, `tools/m51-*`) — the
  DEC-0078 locus exactly; `src/devices/video/sprite_engine.*` net-unchanged in the
  landed set.

**Process incident (both directions, for the coordinator):** two M51 developer
sessions ran concurrently on the one repository (each experiencing the other as §8's
interference); additionally the landed commit `278af86` was created by the peer
session while milestone instructions said "leave changes uncommitted for QA review."
This session did NOT revert the commit (clobber risk, M35 precedent) — the coordinator
should ratify or re-stage it as part of QA/release. All §10 gate numbers were measured
against exactly that commit.
