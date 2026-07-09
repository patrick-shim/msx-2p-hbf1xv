# M32 Implementation Report — Raster-Accurate Per-Line Rendering + Line-Interrupt Delivery (Defect A) + FM Mix Calibration k=21 (Defect B)

- Milestone: M32 (DEC-0039 RC-playtest defect pair; charter ratifications D-1/D-2 in RESP-M32-001)
- Package: `docs/m32-planner-package.md` (slices S1-S5 implemented in full)
- Baseline: v1.0.32 production candidate (must not regress). Tag target: v1.0.33.
- Author: MSX Developer Agent, 2026-07-09
- Working tree left UNCOMMITTED for QA/coordinator (standing rule).
- ZEXALL/ZEXDOC: correctly NOT run — `git diff v1.0.32 -- src/devices/cpu src/core` is EMPTY
  (verified below, AC-1); fast-subset cadence per `feedback_slow_test_cadence.md`.

---

## 1. What shipped (by slice)

### S1 — Device layer: `VdpScanlineAccumulator` + the render-sync seam

- NEW `src/devices/video/vdp_scanline_accumulator.{h,cpp}`: the line-granular accumulation
  store. `sync_to_line(end)` commits display lines `[watermark, min(end, height))` via the
  EXISTING, untouched `VdpFrameRenderer::render_line()` against LIVE registers; `finalize()`
  seals the frame under the end-of-frame geometry policy (§2.4: width/height/border from live
  state; 256→512 pixel-double / 512→256 even-sample / non-2x copy-and-pad adaptation — D10);
  `compose()` returns "accumulated past + projected future" without sealing. Wrap safety: a
  POSITIVE backwards sync (the DEC-0034 step-only loop-shape class, or a mid-frame LN/geometry
  origin jump) seals deterministically and restarts; `end == 0` (border positions) is always a
  plain no-op. Row buffers are reused across frames (R-M32-3, no per-line allocation). A
  staleness guard (`mark_completed_frame_stale`) keeps the boundary fast path honest: ANY VDP
  write after finalize (hooked, or a machine `debug_io_write` to ports #98-#9F) drops the sealed
  frame back to live projection — preserving exact legacy snapshot semantics for scenes rebuilt
  after frames ran (a real pattern in the existing suites).
- NEW `VdpRenderSyncListener` seam on `V9958Vdp` (`v9958_vdp.h/.cpp`): nullable
  (default-nullptr = byte-identical no-op, unit-proven), invoked at the TOP of `io_write()` for
  all four ports #98-#9B BEFORE any state mutates — the openMSX sync-before-change protocol
  (references/openmsx-21.0/src/video/PixelRenderer.cc:253-394 register/palette updates,
  :510-517 VRAM writes; the LINE-accuracy rounding at :549-571 is the granularity model; fMSX's
  per-scanline pipeline corroborates, references/fmsx-60/source/fMSX/MSX.c:209-224, 2149-2155).
  `raster_display_line()` promoted private→public (needed by the machine adapter and the
  line-int leg; behavior unchanged).
- §1.2 zero-call-site finding RE-CONFIRMED this cycle: before M32,
  `V9958Vdp::on_line_match()` had production call sites ONLY in
  `tests/unit/devices/video_v9958_status_irq_unit_test.cpp` — grep over `src/` showed the sole
  definition and zero callers. Line interrupts never fired in any real execution pre-M32.
- Legacy `VdpFrameRenderer::render_frame()` NOT modified, NOT removed (the in-test oracle,
  §2.2) — `git diff v1.0.32 -- src/devices/video/vdp_frame_renderer.*` is empty.

### S2 — Machine wiring + line-interrupt delivery (D-1 ratified)

- `Hbf1xvMachine` (`hbf1xv_machine.{h,cpp}`):
  - `VdpRenderSyncAdapter` (attached in `wire_bus()`): a hooked write while the beam is on
    display line L commits `[watermark, L+1)` — **the L+1 latch rule** (§2.3). Border/vblank
    positions (L < 0) commit nothing. `debug_io_write()` SUSPENDS the adapter (the §2.3
    documented exclusion — the non-perturbing debug seam never drives the hook; proven by a
    dedicated integration case).
  - `on_vsync_boundary()` gains ONE additive call: `scanline_accumulator_.finalize(...)`.
    **Documented, justified ordering deviation from the package's letter**: finalize runs
    AFTER `vdp_.on_vsync()`, not before. Reason (recorded in-code and here): `on_vsync()`
    mutates no register/VRAM/palette state the background renderer consumes, but it DOES
    (a) recompute the sprite visibility tables from the frame's own END-of-frame VRAM and
    (b) advance the R#13 blink countdown — and the legacy snapshot renderer that produced
    every committed evidence frame ran AFTER `on_vsync_boundary()` (the
    `Sdl3App::run_one_frame()` shape), i.e. against the post-recompute table and
    post-decrement blink state. Finalizing BEFORE `on_vsync()` would give every projected line
    a one-frame-stale sprite table versus BOTH the legacy pipeline and real hardware (whose
    per-line attribute fetch first sees a frame's vblank-written attributes during that same
    frame), violating the package's own AC-4/AC-5 hard oracles for any moving-sprite scene.
    The package's stated intent ("the frame closes on the frame's own state") is honored:
    the recompute at boundary F samples the frame-F end-of-frame VRAM. Per-line LIVE sprite
    fetch remains the D9 remainder.
  - `render_frame(field)` re-routed, signature EXACTLY unchanged. Progressive: boundary call →
    the sealed frame (fresh only — see staleness guard); mid-display call → memoized commit to
    L+1 + live projection below; fresh/debug-built scenes → pure live projection (== legacy by
    construction). Even/Odd (test/debug-only fields, no production caller — all four consumers
    audited per §2.4) keep legacy frozen-snapshot semantics via `vdp_frame_renderer_` —
    a documented design choice. Logical constness: the accumulator member is `mutable`;
    committing lines the beam has passed is memoization (documented in the header).
  - **Line-interrupt delivery** (`poll_line_interrupt()`, called once per
    `step_cpu_instruction()` immediately after the scheduler tick — the M25 pause-gate
    precedent; NOT in `Z80aCpu`, NOT in `core::Scheduler`): O(1) per step — an input
    fingerprint compare (R#19/R#23/R#9/last-vsync BY VALUE, so the cache is write-path
    independent) plus one compare against the cached next-match cycle. Fires
    `vdp_.on_line_match()` exactly once when the raster crosses into screen line
    M = (R#19 − R#23) & 0xFF (openMSX VDP.cc:518-576 `scheduleHScan`, incl. the :554-559
    "never occurs" clamp for M ≥ active lines and the :571-574 passed-moment→next-window rule;
    fMSX MSX.c:2091-2104 — algebraically the identical relation; both references agree, no
    arbitration needed). IE1-off software: `on_line_match()` is a proven no-op on the IRQ line
    and S#1 FH (existing M14 gating, v9958_vdp.cpp) — zero CPU-visible change, proven by the
    integration test's twin-trajectory case AND by the full existing suites. `run_frame()`/
    `run_cycles()` (no-CPU paths) deliver no line interrupts, exactly as they deliver VBlank
    only via `on_vsync_boundary()`.
  - `cold_boot()` additions: accumulator reset, line-int cache invalidation, and
    `last_vsync_cycle_ = 0` (the scheduler resets to 0, so every vsync-relative raster
    derivation must restart from the same origin — a machine that ran frames before a second
    `cold_boot()` would otherwise underflow `cpu_tstates_since_vsync()`; latent since M23,
    first load-bearing for the M32 cache; no existing caller pattern is affected, all fast
    suites green).
- `CMakeLists.txt`: one new source file registered.

### S3 — Split-screen proof

- NEW `tests/system/hbf1xv_m32_split_screen_system_test.cpp` (§2.6): synthetic in-RAM Z80
  program (zero game bytes/knowledge) — GRAPHIC4, per-row-unique signature VRAM, IE0+IE1, IM1
  handler dispatching on S#1 FH (line path: count + R#23=128; VBlank path: S#0 read + the
  MANDATORY R#23=0 AND R#19=80 re-arm), driven through the real frame-loop shape for 5 frames.
  Asserts: two regions (rows above the boundary == the untouched-legacy scroll-0 reference,
  rows at/below == the scroll-128 reference, every row pair discriminating), boundary within
  the §2.5 precision band [80, 84) (measured: 82), S#1 FH observed by the handler (counter ≥
  3), two-machine byte-identical frames AND counters, plus the ADVERSARIAL IE1-off arm: FH
  counter == 0 and the frame byte-equals the no-split reference — the interrupt leg is
  load-bearing. Additive `--dump-frame` argv for the A/B harness (ctest invocation unchanged).
- NEW permanent A/B tooling: `tools/openmsx-m32-split-ab.ps1` + `tools/gen-m32-split-probe.py`
  + `tools/m32-split-ab-compare.py`. **Disposition: PARITY, split-boundary delta 0** — full
  results, adversarial arms (comparator self-check; IE1-off on real openMSX shows fh=0 and no
  split), and two honestly-recorded bring-up findings (the R#8 VR bit; openMSX throttle-off
  screenshot staleness) in `docs/m32-parity-trace-diff.md`. A-M32-3 verified (Tcl injection
  practical — the fallback was NOT needed). A-M32-4 verified better than required (boundary
  delta 0, not merely ±2).
- **Aleste 2 live smoke (AC-9 — the human's exact repro): RESOLVED.** Scripted deterministic
  run (M31-shape SPACE holds at frames 600/1500/2100/2700; auto-identified KonamiSCC image
  loaded explicitly), real frame loop, 3600 frames:
  - f2600 = WEAPON SELECT screen; f2900-f3600 = REAL GAMEPLAY — "AREA 1" HUD row intact at the
    top, NON-GARBAGE, VISIBLY SCROLLING forest playfield with the player ship and enemy
    sprites; f3600 shows clearly advanced terrain + score 1280 accumulating.
    Evidence committed: `debug/frames/m32-aleste-play-f{2600,2900,3000,3200,3600}.{frame,png}`.
  - **A-M32-1 VERIFIED (the observed protocol)**: Aleste 2 first enables IE1 MID-FRAME at
    frame 2844 (gameplay start; boundary-granular sampling reads IE1 OFF — the game enables it
    after vblank and clears it in the line handler), with R#19 = 220 written once at frame
    2451 and R#23 toggling per frame (vblank sets the HUD-region scroll ≈ 204 so
    (220 − 204) & 0xFF ≈ 16 = the HUD/playfield boundary; the line handler switches to the
    playfield scroll; boundary samples read the playfield value). Exactly the DEC-0039 triage
    shape, now measured.
  - R-M32-5 watch item (sprite/background consistency): no visible sprite/playfield
    inconsistency observed in the gameplay frames; enemy sprites overlapping the HUD text row
    at f3600 is ordinary MSX sprite-over-HUD behavior (sprites are not split-clipped on real
    hardware either). Recorded, nothing D9-worse seen live.

### S4 — FM mix calibration (D-2 ratified)

- `src/frontend/machine_audio_mixer.h`: `kFmAmplitudeScale` 5 → **21**, with the full §2.8
  derivation in the header: openMSX Sony_HB-F1XV.xml:63/:191 21000:9000 is a PER-CHANNEL ratio
  (source-verified normalization: AY8910.cc:64-93/:977-980 per-channel peak 1.0;
  YM2413Okazaki.cc:48/:154-165/:1051-1054 per-channel 255 × 1/256), so
  k = round(400 × 31 × (3/7) / 256) = round(20.76) = **21**; the worst-case-sum reading is
  explicitly rejected in the header (it reproduces M31's defective k≈4.6). Clamp math REDONE
  honestly: melody 9×256×21 = 48,384 (FM alone exceeds int16); rhythm 4,096 raw × 21 = 86,016;
  + PSG 24,800 + two SCCs 14,400 = 125,216 ≫ 32,767 → clamp REQUIRED.
- NOTHING else in the audio chain changed — `git diff v1.0.32` for
  `src/frontend/audio_pacer.*`, `src/frontend/psg_audio_pump.*`,
  `src/devices/audio/ym2413_synth.*`, `src/devices/audio/ym2413_opll.*`,
  `src/frontend/machine_audio_mixer.cpp`: ALL EMPTY (verified, §4 below).
- Tests: `machine_audio_mixer_fm_unit_test.cpp` saturation case rebuilt to the package's
  constructed worst case (9 aligned carriers + max PSG + two max SCCs → exact +32,767 clamp on
  BOTH stereo sides) plus a NEW FM-alone case proving BOTH rails clamp (±32,767/−32,768) at
  k=21; the zero-YM2413 byte-identity HARD oracle cases are byte-for-byte UNMODIFIED and green.
  NEW `tests/unit/frontend/machine_audio_mixer_loudness_ratio_unit_test.cpp`: single
  full-volume FM carrier vs single full-volume PSG channel, each mixed alone —
  **measured: fm_peak = 5,376 (= 256×21), psg_peak = 12,400 (= 31×400 exactly), ratio
  0.433548 vs reference 0.428571 → +1.16%, well inside the ±15% gate** (the bound derives from
  the reference ratio, not the implementation).
- Evidence re-capture at k=21 (M31 procedure: two IDENTICAL deterministic Aleste sessions, FM
  mixed vs fm=nullptr; captured on the FINAL tree):
  `debug/sounds/m32-fm-aleste-fmON.{dump,wav}` / `-fmOFF.{dump,wav}` (148,470 stereo pairs =
  3.37 s from the first divergence; WAV SHA-256 differ: 2be6991c… vs 921a4b80…). Decoded
  figures: first_diff at interleaved index 1,025,998 (≈ frame 695-698 — the real title's first
  key-on, matching M31's 1,026,062 within a methodology-rounding hair), 105,366 differing
  values in the window, **FM contribution peak 3,780 = EXACTLY the package-predicted
  900 × 21/5; mean-nonzero 1,094 vs M31's 261 (×4.19 ≈ 21/5)**. The audible-divergence
  property is preserved at the new, 4.2× (+12.5 dB) level. The M31 pair stays committed as the
  historical baseline. (Per R-M32-7, the human's ear remains the final acceptance signal.)

### S5 — Gates, ledger, docs

Everything below in §3-§6; ledger updates applied
(`agent-protocol/state/deferred-backlog.md`: NEW rows D8/D9/D10; C10→M33-era / F1→M34-era /
F2→M35-era per DEC-0039; open rows C1/D4, E3, G3/G4/G5 re-affirmed UNCHANGED;
`agent-protocol/state/current-phase.md` at implementation-complete; RESP-M32-002 appended to
`agent-protocol/channels/responses.md`).

---

## 2. New/updated tests (package §5 matrix rows 1-4, 7, 10-12 — all delivered)

| Test | Layer | Status |
|---|---|---|
| `tests/unit/devices/video_vdp_scanline_accumulator_unit_test.cpp` (NEW) | unit | PASS — AC-4 static-frame equivalence across 15 mode configs (TEXT1/TEXT2/G1/G2/G3/MC/G4±scroll±sprites/G5/G6/G7/YJK/YJK+YAE/blank), seeded randomized chunked sync, compose() partial equivalence, idempotence, wrap-safety seal, deterministic 256→512 geometry adaptation, two-run identity |
| `tests/unit/devices/video_v9958_render_sync_seam_unit_test.cpp` (NEW) | unit | PASS — null-listener byte-identity (regs/VRAM/status/palette/rendered frame vs a plain twin), old-state visibility at callback time (register commit AND VRAM write), one call per io_write on all four ports, io_read never fires, nullptr detaches |
| `tests/integration/machine/hbf1xv_m32_per_line_latch_integration_test.cpp` (NEW) | integration | PASS — exact L+1 latch (hooked R#23 write at line 100 → rows ≤100 old / ≥101 new, against legacy-renderer references), vblank write = whole frame, boundary render == finalized frame (repeat-call identical), **debug_io_write hook-exclusion (no split through the debug seam)**, two-run identity |
| `tests/integration/machine/hbf1xv_m32_line_interrupt_integration_test.cpp` (NEW) | integration | PASS — fire within one line of match entry ((70+100)·228 window), S#1 FH set/read-clear/IRQ release, once-per-crossing (quiet remainder of the window; next window re-fires), the (R#19−R#23)&0xFF re-arm relation (R#23=60 → line 40), IE1-off ⇒ no IRQ ever + FH never set + byte-identical CPU trajectories for machines with DIFFERENT R#19 values, M≥active never fires (the openMSX "never occurs" clamp), two-run identical fire cycle |
| `tests/system/hbf1xv_m32_split_screen_system_test.cpp` (NEW) | system | PASS — §1 above (incl. the adversarial IE1-off arm and two-machine determinism) |
| `tests/unit/frontend/machine_audio_mixer_fm_unit_test.cpp` (UPDATED) | unit | PASS — k=21 arithmetic; worst-case saturation with two SCCs (both sides +32,767); FM-alone both-rails clamp; zero-YM2413 hard-oracle cases UNMODIFIED |
| `tests/unit/frontend/machine_audio_mixer_loudness_ratio_unit_test.cpp` (NEW) | unit | PASS — ratio 0.4335 ∈ [0.3643, 0.4929]; exact-scale cross-checks; two-run identity |

One test-fixture bug found and fixed during S3 bring-up (recorded honestly): the split test's
debug VRAM fill initially ran in the reset (legacy G1) mode, where the 14-bit VRAM pointer
wraps at 16 KB WITHOUT the R#14 carry (v9958_vdp.cpp `advance_vram_pointer()`, a real modeled
hardware behavior) — the second 16 KB overwrote the first. Fixed by setting GRAPHIC4 before
the auto-increment fill (V9938 mode → R#14 carry); the openMSX-side probe fills via a Z80 loop
in G4 and never had the issue.

## 3. Test-suite results (final tree, after the temporary evidence probe was reverted)

- Headless (`cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF`; `cmake --build build --config
  Debug` → zero errors): `ctest --test-dir build -C Debug -LE m24_slow_full_sweep` →
  **177/177 passed** (baseline 171 + 6 new), 38.43 s.
- SDL3-ON (`build-m32-sdl`, `-DCMAKE_PREFIX_PATH=<repo>/build/_sdl3_install`, dummy drivers,
  SDL3.dll on PATH): build → zero errors; `ctest -LE m24_slow_full_sweep` → **186/186 passed**
  (baseline 180 + 6 new), 43.65 s.
- Determinism guards: `hbf1xv_m27_replay_determinism_system_test` and the M10 event-log golden
  are inside both counts and green; the boot-logo integration test passes UNMODIFIED
  (`entry=75 exit=259 f4_end=0x80` — its own stderr checkpoint line).
- ZEXALL/ZEXDOC: NOT run (AC-1 holds — see §4).

## 4. Isolation / audit gates (AC-1..AC-3, all verified on the final tree)

- `git diff v1.0.32 --stat -- src/devices/cpu src/core` → **empty** (ZEXALL tripwire never
  fired; R-M32-1 not triggered).
- `git diff v1.0.32 --stat -- src/frontend/audio_pacer.h src/frontend/audio_pacer.cpp
  src/frontend/psg_audio_pump.h src/frontend/psg_audio_pump.cpp
  src/devices/audio/ym2413_synth.h src/devices/audio/ym2413_synth.cpp
  src/devices/audio/ym2413_opll.h src/devices/audio/ym2413_opll.cpp
  src/frontend/machine_audio_mixer.cpp src/devices/video/vdp_frame_renderer.h
  src/devices/video/vdp_frame_renderer.cpp` → **empty** (Defect B is ONE constant + comments;
  the legacy renderer is untouched).
- `src/main.cpp`: the temporary `--m32-evidence` probe was fully reverted before the §3 gate
  runs (diff empty at that point). At QA-CLOSURE time (§9) main.cpp gained a PERMANENT,
  additive `--debug-session` extension (`--frames <N>` frame-loop drive + `--dump-frame
  <name>`), required by QA's recorded-recipe condition; absent flags leave every pre-M32
  invocation byte-identical (the frame-loop branch is entered only when `--frames` is
  present). Details in §9; post-change gate re-run in §10.
- Universal-fix audit: no game-keyed logic anywhere in the diff (nothing consults a title,
  SHA, or cartridge identity in any new/changed src/ path; the smoke tooling identifies ROMs
  only to LOAD them).
- License isolation: zero code/table transcription from `references/openmsx-21.0/` or
  `references/fmsx-60/`; every behavior claim in the new headers/comments cites concrete
  `references/...` paths (PixelRenderer.cc:253-394/510-517/549-571; VDP.cc:518-576/913-923;
  MSX.c:209-224/2091-2104/2149-2155; Sony_HB-F1XV.xml:63/:191; AY8910.cc:64-93/977-980;
  YM2413Okazaki.cc:48/154-165/1051-1054; Yamaha YM2413 FM Chip.md:184/:186).

## 5. AC-5 committed-evidence byte-identity sweep (the HARD oracle) + AC-6 escalation

Methodology (recorded so QA can reproduce): the local `debug/frames/*.frame` dumps were first
proven to be the exact sources of the committed PNGs (frame-to-png.py is deterministic;
8/8 spot conversions byte-matched the committed PNGs). A v1.0.32 **baseline git worktree** was
built with the SAME temporary `--m32-evidence` probe (M31's own probe precedent; reverted from
the final tree) and used to (a) RECOVER the M31 capture recipes by byte-matching regenerated
dumps against the committed `.frame` files, then (b) A/B the M32 tree against both the
committed artifacts and the same-recipe baseline outputs.

**Recipes recovered and verified (baseline reproduces the committed bytes exactly):**
unattended BIOS (9/9 frames byte-identical to `m31-rc-bios-*`); unattended DOS boot with
`disks/msxdos22.dsk` (5/5 = `m31-rc-dos-*`); Aleste with SPACE@300-314/600-614 (2/2 =
`m31-rc-aleste-*`); unattended MG2 (3/3 = `m31-rc-mg2-*`). The Metal Gear recipe could NOT be
recovered (four candidate scripts tried; the M31 report did not record the exact input frames
— a recipe-recording gap to note for future evidence capture); Metal Gear was therefore gated
by the rigorous same-script baseline-vs-M32 A/B instead.

**RESULT — byte-identical on the M32 tree (AC-5 GREEN for):**

| Artifact | Result |
|---|---|
| `m31-rc-bios-f{210,320,400,520,700,1100,2000,3000}` | byte-identical (8/9) |
| `m31-rc-dos-f{200,400,600,800,1000}` | byte-identical (5/5) |
| `c5-verify-settled` (== dos f1000, proven byte-equal) | byte-identical |
| `m31-rc-aleste-f{500,900}` (intro; IE1 not yet enabled) | byte-identical (2/2) |
| `m31-rc-mg2-f{1260,2200}` | byte-identical (2/3) |
| `m26-example-boot` (regenerated via the standing `--frame-dump-demo`) | byte-identical |
| `konami-splash-after-f0750-white-background.png` (unattended Metal Gear, f750) | **PNG byte-identical on the M32 tree** |
| `boot-logo-post-colorfix-f0210` (pixels == m31-rc-bios-f210) | byte-identical (transitively) |
| Metal Gear f750 same-script baseline-vs-M32 | byte-identical |
| DEC-0031 boot-logo integration timeline | unchanged (`entry=75 exit=259 f4_end=0x80`) |

**DIVERGENCES — root-caused, NOT rebaselined, ESCALATED to the coordinator per AC-6
(committed artifacts untouched; side-by-side evidence at `debug/frames/m32-divergence-*`):**

1. **`m31-rc-bios-f150` (+ its DEC-0031-era border twin `boot-logo-after-f0150`, proven the
   same underlying frame)** — 80/192 rows differ (rows 32-111, the logo-letter band).
   Root cause: the DEC-0031 boot-logo WOBBLE writes the R#26/R#27 horizontal-scroll pair
   mid-frame, paced per S#0 collision-poll exit (docs/border-and-boot-logo-investigation.md:128
   "R#26/R#27 scroll pair, DJNZ ×84"). The M32 renderer now renders those writes raster-true —
   per-line shift bands ("tearing") during the slide, exactly what openMSX's LINE-accuracy
   sync-before-change produces for mid-frame horizontal-scroll writes
   (PixelRenderer.cc:253-258) and what real hardware displays on a single mid-slide frame.
   The legacy snapshot flattened the whole frame to the end-of-frame scroll value. NOT an IE1
   trajectory shift (IE1 confirmed never enabled during boot — see the survey below); the CPU
   trajectory is bit-identical (all mode/timing checkpoints unchanged); only the honest
   raster-true pixels differ. Evidence: `m32-divergence-bios-f150-m32.{frame,png}` vs the
   committed `m31-rc-bios-f150.png`.
2. **`m31-rc-metalgear-f{1100,1400}`** (same-script baseline A/B) — 17 and 14 rows differ
   respectively, in single sprite-band regions (rows 89-105 / 95-108, ~10-20 px wide).
   Root cause: **D9-class** (the package's own §1.4-item-2 predicted remainder). Metal Gear
   writes VRAM mid-display during gameplay; any hooked mid-display write commits the rows the
   beam has passed, and those committed rows composite sprites from the PREVIOUS boundary's
   visibility table (per-frame snapshot) — up to one frame of sprite lag inside the committed
   region for a moving sprite (Snake). Projected rows are exact. Evidence:
   `m32-divergence-metalgear-f{1100,1400}-{v1032,m32}.png`.
3. **`m31-rc-mg2-f1700`** — 12 rows differ (rows 76-95, one sprite band) during the MG2 title
   animation; identical D9-class cause. Evidence: `m32-divergence-mg2-f1700-m32.png` vs the
   committed `m31-rc-mg2-f1700.png`.

**Committed-vs-local precision (verified via `git ls-files debug/`):** of the four divergent
scenarios, exactly TWO touch COMMITTED artifacts — `boot-logo-after-f0150-letters-sliding.png`
(the wobble frame; the committed PNG is the bordered render of the same underlying frame as
the local `m31-rc-bios-f150.frame`, proven pixel-equal via `--with-border` reconversion) and
`m31-rc-metalgear-f1100.png`. The `metalgear-f1400` / `mg2-f1700` divergences affect only
LOCAL, uncommitted `.frame` artifacts (the committed mg2 PNG is f1260, which reproduces
byte-identically) — same root causes, recorded for completeness. Additionally, two committed
static-phase artifacts were not directly regenerated (no exact frame index in the recovered
recipes): `border-after-f0300`/`border-before-f0300` (BASIC static phase — bracketed by the
byte-identical f320/f400 regenerations) and `boot-logo-before-f0135` (pre-logo blank phase);
and the M29-era `m29-aleste-f*` PNGs (v1.0.30-binary captures) are outside the package's AC-5
list and were not re-verified. All noted for QA.

**A-M32-2 outcome (IE1 survey, instrumented across every evidence scenario incl. mid-frame
sampling):** NO evidence-set scenario enables IE1 — bios (3000 frames), dos (1000), aleste
(900; enables IE1 only at gameplay, frame 2844, far beyond the committed f500/f900), metalgear
(1400), mg2 (2200; writes R#19=180 once at f571 but never IE1). **A-M32-2 HOLDS: none of the
divergences is an IE1/CPU-trajectory shift.** All four are pixel-only consequences of
raster-true rendering itself (1 = mid-frame scroll writes now honestly rendered; 2-3 = the
declared D9 sprite-snapshot remainder interacting with mid-display VRAM writes). The developer
recommendation to the coordinator: accept the four as the intended Defect-A behavior change
(the openMSX-corroborated direction) and regenerate those four artifacts at closure —
**not performed by the developer; AC-6/the dispatch instruction reserves that decision.**

## 6. Evidence gates (all run on the final tree; real outputs)

- `tools/validate-assets.ps1` → "Asset validation result: True" (7 BIOS files, 3 ROMs).
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` → refreshed (checksums
  identical; timestamp-only diff).
- Headless + SDL3-ON builds/ctest → §3 counts.
- `tools/openmsx-ab-smoke.ps1` → `docs/openmsx-ab-smoke.md` refreshed (behavior-affecting
  gate); the historical R5-resolution note was restored after regeneration (the DEC-0032/F1
  lesson — the script overwrites it).
- `tools/openmsx-m32-split-ab.ps1` → **PARITY** (`docs/m32-parity-trace-diff.md`), exit 0
  end-to-end, including the comparator self-check and the reference-side IE1-off arm.

## 7. Deviations from the package (each with reasoning; no scope change)

1. **Finalize ordering** (finalize AFTER `vdp_.on_vsync()`, package said before) — required by
   the package's own AC-4/AC-5 hard oracles; full analysis in §1-S2 and in-code. Without it,
   every moving-sprite frame (Metal Gear gameplay etc.) would render one frame of sprite lag
   versus the committed evidence AND versus real hardware.
2. **Even/Odd fields keep snapshot semantics** — documented design choice (§1-S2); production
   only ever renders Progressive; AC-4 holds trivially for those fields.
3. **Split-test assertion band [S, S+4) instead of the package's ±2 prose** — the two-write
   register protocol + measured IM1-accept/handler latency lands the commit at S+2 with ≤1
   line of slack; the boundary is asserted structurally (measured: exactly 82 on BOTH our side
   and openMSX). The A/B gate (≤2 lines between sides) was met with delta 0.
4. **The `debug_io_write` hook-exclusion unit case lives in the per-line-latch INTEGRATION
   test**, not the S1 unit test — `debug_io_write` is a machine seam, not a `V9958Vdp` seam
   (noted in the unit test header).
5. **Temporary `--m32-evidence` probe in `src/main.cpp`** for the evidence sweep/captures
   (M31's own precedent), REVERTED via `git checkout -- src/main.cpp` before the final gate
   runs; both configs rebuilt clean after the revert.

## 8. Known issues / open items for QA

1. The four AC-6 divergences (§5) await coordinator arbitration; the committed evidence set is
   untouched. Suggested QA check: reproduce any of them via a v1.0.32 worktree + the §5
   recipes (the report's methodology section), or simply verify the still-green portions.
2. The M31 Metal Gear capture recipe remains unrecovered (gap noted; the same-script A/B
   covers the regression question rigorously). Future evidence captures should record exact
   input scripts in the report.
3. D9 remainder now has a MEASURED characterization (sprite-band-sized, §5 items 2-3) recorded
   on the backlog row.
4. Per R-M32-7, k=21 is reference-honest but the human's ear on
   `debug/sounds/m32-fm-aleste-fmON.wav` (vs `-fmOFF.wav`) is the final acceptance signal.
5. R-M32-3 (per-step overhead): no measurable fast-suite slowdown (177 tests in ~38 s vs the
   M31 baseline ~32-40 s range); no interactive frame-rate measurement was taken this cycle
   (headless-only evidence); flag for the human's next live session if desired.
6. The pre-existing diskless-boot banner delay is untouched (diff-proven; known issue since
   M16-era).

## 9. Evidence supersession (M32 closure)

Per QA's Conditional Pass (`docs/m32-qa-signoff.md`: all four AC-6 divergences CONFIRMED as
intended raster-truth changes, zero code fixes) and the coordinator's closure directive, the
TWO affected COMMITTED artifacts were regenerated IN PLACE under the M32 per-line renderer,
each with a recorded, re-runnable recipe. The original M31/DEC-0031-era bytes remain
retrievable at git tag **v1.0.32**. Local-only divergent frames (metalgear-f1400, mg2-f1700,
`.dump` companions) required no action (not committed artifacts).

1. **`debug/frames/boot-logo-after-f0150-letters-sliding.png`** — regenerated in place
   (old PNG SHA-256 4e54dd5426721bd9…, new e72f4d4c2c207257…; the 640×240 bordered geometry
   preserved via `--with-border`). Why: the DEC-0031 boot-logo wobble writes the R#26/R#27
   horizontal-scroll pair mid-frame, and the M32 renderer now shows the slide's per-line comb
   tearing (QA-confirmed consistent with `boot-logo-openmsx-reference-t2500ms.png`; the
   openMSX sync-before-change direction, PixelRenderer.cc:253-258). Recorded recipe
   (permanent — uses the new `--debug-session` frame-loop flags; unattended cold boot,
   frame 150):
   ```
   build/Debug/sony_msx_headless.exe --debug-session bios 0 --frames 150 \
       --dump-frame m32-closure-bios-f150.frame
   python tools/frame-to-png.py --with-border debug/frames/m32-closure-bios-f150.frame \
       -o debug/frames/boot-logo-after-f0150-letters-sliding.png
   ```
   Mechanism validated: this capture is BYTE-IDENTICAL to the AC-5 sweep's probe capture of
   the same frame (`debug/frames/m32-divergence-bios-f150-m32.frame`).
2. **`debug/frames/m31-rc-metalgear-f1100.png`** — regenerated in place (new PNG SHA-256
   38d2f1d6538f8b02…, 256×212; Snake among the trucks with the LIFE/CLASS HUD, visually
   re-verified). Why: the D9-class one-frame sprite-band snapshot lag in mid-frame-committed
   rows (~17/212 rows; mechanism pixel-proven by QA on mg2-f1700). **Recorded recipe — QA's
   condition (the M31 original's input script was never recorded and is unrecoverable):**
   `tools/capture-metalgear-evidence.ps1`, consuming
   `tools/metalgear-evidence-input.script` (HBF1XV-INPUT-SCRIPT v1: SPACE held during frames
   [300,315) and [600,615), cycle stamps 17,861,064 / 18,757,104 / 35,781,864 / 36,677,904 =
   299/314/599/614 × 59,736) — real BIOS boot, `roms/metalgear.rom` loaded explicitly as
   `Konami`, exactly 1,100 frames through the real frame loop, frame dump + deterministic
   PNG conversion in place. The tool runs the capture TWICE and asserts two-run byte identity
   before converting (verified this run: frame SHA-256 61E8DEC72E4FAB41… both runs); its
   header records that the original M31-era bytes live at tag v1.0.32. The tool's output is
   byte-identical to the §5 same-script A/B M32 capture
   (`m32-divergence-metalgear-f1100-m32.png`) — the script-driven input reproduces the probe
   run exactly (titles sample the keyboard once per frame, so instruction-granular
   script-application skew is invisible).

Enabler (permanent, additive): `--debug-session` gained `--frames <N>` (drive N frames via
the REAL frame loop — VBlank delivered at every boundary, no halt-stop; the pre-existing
step-only loop cannot boot titles, the DEC-0034 loop-shape finding) and `--dump-frame <name>`
(write_frame_dump after the run). Absent flags → byte-identical pre-existing behavior.

## 10. Post-supersession gate re-run (after the §9 changes)

- `cmake --build build --config Debug` → zero errors (headless; `src/main.cpp` links only
  into `sony_msx_headless` — the SDL3 executable uses `sdl3_main.cpp` and is unaffected; the
  §3 SDL3-ON 186/186 result stands).
- `ctest --test-dir build -C Debug -LE m24_slow_full_sweep` → **177/177** (re-run after the
  main.cpp change).
- `git diff v1.0.32 -- src/devices/cpu src/core` → still **empty**.

## 11. QA handoff

All §4 package acceptance criteria addressed: AC-1..AC-4 §4/§2; AC-5/AC-6 §5 (green matrix +
four escalated, root-caused, never-rebaselined divergences); AC-7 §1-S3/§2; AC-8
`docs/m32-parity-trace-diff.md` (PARITY, self-checked); AC-9 §1-S3 (gameplay reached,
non-garbage split, PNGs committed, FM BGM audible per §1-S4 capture); AC-10..13 §1-S4;
AC-14 §3/§6; AC-15 §3; AC-16 this report + ledger. Working tree uncommitted; coordinator owns
the commit and the AC-6 arbitration.
