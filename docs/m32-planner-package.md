# M32 Planner Package — RC-Playtest Defect Pair: Raster-Accurate Per-Line Rendering + FM Mix Calibration

- Milestone ID: M32
- Title: Raster-accurate per-line rendering (Defect A) + honest FM mix calibration (Defect B)
- Charter: DEC-0039 (human-authorized after first-hand v1.0.32 play of Aleste 2: "M32: both fixes")
- Baseline that must not regress: v1.0.32 production candidate (`docs/m31-qa-signoff.md` — QA
  unconditional PASS with explicit RC verdict; headless fast 171/171, SDL3-ON fast 180/180,
  6-item smoke matrix, committed evidence PNGs/WAVs)
- Tag target: v1.0.33
- Author: MSX Planner Agent, 2026-07-09
- ZEXALL/ZEXDOC posture: **NOT triggered.** No `src/devices/cpu/` or `src/core/` change is
  planned or permitted by this package (fast-subset cadence per
  `feedback_slow_test_cadence.md`). §7 R-M32-1 states the loud-flag rule if implementation
  pressure ever points at those trees.

---

## 1. Scope and Assumptions

### 1.1 Objective

Fix the two human-confirmed v1.0.32 RC-playtest defects universally (never game-keyed, per
`feedback_universal_fixes_only.md`):

- **Defect A (headline, renderer architecture):** `Hbf1xvMachine::render_frame(field)`
  (`src/machine/hbf1xv_machine.cpp:617-619`) delegates to
  `VdpFrameRenderer::render_frame()`, a documented "deterministic, pull-model,
  frozen-register-snapshot" renderer (`src/devices/video/vdp_frame_renderer.h:29-37`,
  `vdp_frame_renderer.cpp:415-424` — a loop over `render_line()` against the register state at
  the moment of the call). Real V9958 output is produced as the beam advances; a register
  write between lines takes effect on the following line. A mid-frame R#23 rewrite from a
  line-interrupt handler — the standard MSX2/2+ HUD/playfield "screen split", Aleste 2
  gameplay's exact shape — is unrepresentable by construction. Fix: scanline-accumulation
  rendering driven by the machine's raster position, reading LIVE VDP registers at each line
  boundary (the openMSX PixelRenderer model, §2.1).
- **Defect B (mixing policy):** `MachineAudioMixer::kFmAmplitudeScale = 5`
  (`src/frontend/machine_audio_mixer.h:90`) leaves the fully-functional YM2413 ~29 dB under
  the PSG (coordinator-measured from the committed `debug/sounds/m31-fm-aleste-fmON/fmOFF.wav`
  pair: FM contribution peak 900 / mean-nonzero 261 on the ±32,767 scale, vs PSG effects at
  24,800 = 62×400). openMSX's own `Sony_HB-F1XV.xml` balances PSG:MSX-MUSIC at 21000:9000
  (~7.4 dB). Fix: an honestly-derived replacement constant (§2.8: **kFmAmplitudeScale = 21**),
  with the clamp math redone and new deterministic loudness oracles.

### 1.2 The load-bearing survey finding (S1's anchor — read before anything else)

**`V9958Vdp::on_line_match()` has ZERO production call sites.** The hook exists since M14
(`src/devices/video/v9958_vdp.h:86-88`, implementation `v9958_vdp.cpp:385-390`: sets
`irq_horizontal_` when R#0 IE1 is enabled, cites VDP.cc:409-415) and is exercised only by
`tests/unit/devices/video_v9958_status_irq_unit_test.cpp:104/119/133`. Nothing in
`src/machine/` or `src/frontend/` ever calls it — the machine delivers only the VBlank
interrupt (`on_vsync_boundary()` → `vdp_.on_vsync()`, `hbf1xv_machine.cpp:389-418`).
**Line interrupts therefore never fire during any real execution today.**

Consequence: a per-line renderer ALONE cannot fix Aleste 2 — the game's split handler never
runs mid-frame, so R#23 is never rewritten mid-frame, so even a perfect per-line renderer
would render a static R#23 all frame. Defect A's fix necessarily has TWO legs:

1. **Per-line accumulation rendering** (the renderer-architecture change DEC-0039 names), and
2. **Line-interrupt delivery** — wiring the existing, already-tested `on_line_match()` hook to
   the machine's raster position (machine-level, pull-style, the DEC-0026/DEC-0031 raster
   pattern; NOT a `src/core/` or `src/devices/cpu/` change).

DEC-0039's phrasing "this is a renderer change, not a timing change" is honored in the narrow
sense (the RENDERER itself acquires no interrupt/status side effects — reading/accumulating
pixels changes nothing CPU-visible), but leg 2 IS a behavior-affecting timing addition (a new
interrupt source becomes live). §7 D-1 flags this loudly for coordinator ratification before
developer dispatch. Without leg 2 the milestone's own acceptance criteria 7-9 (split test,
A/B, Aleste smoke) are unachievable; the package is written with leg 2 IN scope.

Note the current symptom is exactly consistent with this two-gap analysis: Aleste 2's VBlank
handler sets the HUD-region R#23, the line-interrupt handler that would switch to the
playfield scroll never runs, so the whole frame renders at the HUD offset — HUD correct,
un-rotated scroll ring buffer (garbage) below, all non-split screens pixel-perfect (DEC-0039
triage; the human's screenshot).

### 1.3 In scope

- New scanline-accumulation render path (device-layer accumulator + a nullable render-sync
  seam on `V9958Vdp` + machine wiring), preserving `render_frame()`'s public signature.
- Machine-level line-interrupt delivery (R#19/R#23/IE1, both-references-agreed semantics §2.5).
- Hard regression oracles: static-frame byte-identity vs the legacy snapshot renderer;
  committed frame evidence byte-identity; determinism.
- New deterministic synthetic split-screen integration/system test (no game assets).
- openMSX A/B plan for the split scenario; Aleste 2 live-software smoke.
- `kFmAmplitudeScale` 5 → 21 with full derivation, redone clamp math, updated constructed
  saturation unit test, new loudness-ratio oracle, zero-YM2413 byte-identity oracle re-proof,
  re-captured fmON/fmOFF evidence pair.
- Ledger maintenance: DEC-0039 backlog-label shifts + new named remainder rows (§6).

### 1.4 Out of scope (explicit non-goals, each with a disposition)

1. **Sub-line (mid-line / pixel-granular) rendering accuracy.** openMSX models it as its
   `Accuracy::PIXEL` setting (`references/openmsx-21.0/src/video/PixelRenderer.cc:549-571`
   translates time to `{limitTicks % TICKS_PER_LINE, limitTicks / TICKS_PER_LINE}` under
   PIXEL, but rounds to whole lines under `Accuracy::LINE`; mid-line horizontal-scroll-low
   changes sync at `PixelRenderer.cc:253-258`). Line-boundary granularity is openMSX's own
   first-class `Accuracy::LINE` mode and suffices for line-interrupt-driven splits. Sub-line
   effects are a named remainder (§6, new row D8) — NOT built this cycle.
2. **Per-line LIVE sprite attribute fetching.** Sprite visibility tables remain computed once
   per frame (`V9958Vdp::on_vsync()` → `sprite_engine_.recompute_frame(...)`,
   `v9958_vdp.cpp:360-380`), which consumes R#23 once per frame (`sprite_engine.cpp:126`).
   openMSX checks sprites progressively with live registers; our frame-snapshot model samples
   the END-of-previous-frame R#23 — in the common split arrangement (VBlank handler sets the
   HUD value, line-int handler sets the playfield value, so end-of-frame state ≈ the playfield
   scroll) sprite positioning in the sprite-bearing playfield region is off by at most one
   frame's scroll delta (a few pixels). Named remainder (§6, new row D9); the Aleste smoke
   (§4 AC-9) watches for visible sprite/background inconsistency and records it honestly if
   seen. DEC-0031's collision-event machinery (`sprite_engine.h:77-115`) is NOT touched —
   collision/5th-sprite STATUS behavior stays byte-identical.
3. **Mid-frame display-geometry-change fidelity** (mode switches that change active width
   512↔256 or height 212↔192 mid-frame). Handled deterministically and crash-free by the
   §2.4 adaptation policy; per-line-perfect fidelity for geometry-mixed frames is a named
   remainder (§6, new row D10). No title in the current evidence set does this.
4. **C1/D4 access-slot timing depth** — untouched; the UNBUILDABLE-WITHOUT-FABRICATION ruling
   stands (no license-sensitive table work of any kind this cycle).
5. **Any change to `AudioPacer`, `PsgAudioPump`, `psg_ym2149.*`, `scc_wavetable.*`,
   `ym2413_synth.*`, `ym2413_opll.*`** — Defect B is ONE constant in
   `machine_audio_mixer.h` plus its comment/doc/tests. The DEC-0033 pacing arithmetic and the
   M31 synthesis engine are byte-for-byte off-limits.
6. **Game-keyed anything** — standing memory; both fixes are machine-policy/architecture
   changes applied identically to all software.

### 1.5 Assumptions (each labeled, each with a verification action)

- **A-M32-1**: Aleste 2's gameplay split is driven by the IE1/R#19 line interrupt (DEC-0039
  triage). *Verification (S3)*: instrument the Aleste smoke run to log R#0 IE1 enables and
  R#19 writes during gameplay; record the observed protocol in the implementation report. The
  synthetic split test does not depend on this assumption either way.
- **A-M32-2**: no title/scenario in the committed evidence set (BIOS boot logo, Metal Gear,
  Konami splash, MSX-DOS/BASIC, Aleste non-gameplay screens, metalgear2_scc title) enables
  IE1, so newly-live line interrupts do not alter their CPU trajectories. *Verification (S2)*:
  a temporary instrumented run (or debug-event inspection) over each evidence scenario
  asserting IE1 was never enabled; if any scenario DOES use IE1, §4 AC-6's escalation path
  applies (root-cause, openMSX A/B on that scene, coordinator arbitration — never a silent
  re-baseline).
- **A-M32-3**: openMSX on WSL can drive and screenshot the synthetic split scene via Tcl
  (`screenshot` command lineage of the existing `tools/openmsx-*.ps1` scripts).
  *Verification (S3)*: source-verify the Tcl surface before scripting (the R-M30-6
  discipline); if genuinely impossible, an honest BLOCKED disposition with cited reasoning is
  acceptable per the M25/M26 precedent — but must be attempted first.
- **A-M32-4**: `raster_display_line()`'s existing NTSC line arithmetic
  (`v9958_vdp.cpp:423-437`: `tstates / kCpuTstatesPerLine % 262`, active window derived from
  R#9 LN — grounded in fact-sheet §7 by DEC-0026) is accurate enough to place the split
  boundary within ±2 lines of openMSX's. *Verification (S3)*: the A/B comparison itself.

### 1.6 Dependencies

- M21 `VdpFrameRenderer` (`render_line()` already exists per line —
  `vdp_frame_renderer.h:46-49`) — reused as-is; it is the per-line workhorse.
- M14 `V9958Vdp` contract + `on_line_match()` hook (`v9958_vdp.h:81-91`) — wired, not changed.
- DEC-0026 `VdpClockSource`/`VdpRasterClock` (`v9958_vdp.h:23-31`,
  `hbf1xv_machine.h:470-491`, `628`) and DEC-0031's `raster_display_line()`
  (`v9958_vdp.h:149-157`) — the raster-position foundation both legs stand on.
- M22 sprite compositing inside `render_line()` (`vdp_frame_renderer.h:62-72`) — unchanged.
- M26 `Sdl3App::run_one_frame()` (`sdl3_app.cpp:220-245`) and M25's machine-level
  `step_cpu_instruction()` gate precedent — the wiring points.
- M29/M31 `MachineAudioMixer` + oracles (`machine_audio_mixer.h`,
  `tests/unit/frontend/machine_audio_mixer_unit_test.cpp`,
  `machine_audio_mixer_fm_unit_test.cpp`) — Defect B's surface.
- M27 `--input-script` + frame-dump tooling — the Aleste smoke driver.

---

## 2. Spec Summary

### 2.1 Reference grounding for the per-line model (all files read this cycle)

- **openMSX (primary)** — `references/openmsx-21.0/src/video/PixelRenderer.cc`:
  - Every VDP state-change notification syncs rendering up to the current beam position
    BEFORE the change applies: `updateHorizontalScrollLow/High`, `updateBorderMask`,
    `updateMultiPage`, `updatePalette`, `updateVerticalScroll`, `updateDisplayMode`,
    `updateNameBase/PatternBase/ColorBase` all call `sync(time)` first
    (PixelRenderer.cc:253-394); VRAM writes likewise (`updateVRAM` → `renderUntil(time)`,
    :510-517); `sync()` = "update VRAM, then renderUntil" (:527-547); `frameEnd()` renders
    the remaining lines (:219-228).
  - `renderUntil()` translates emulated time into a beam limit; under `Accuracy::LINE` the
    limit is a whole line index (`(limitTicks + TICKS_PER_LINE - 400) / TICKS_PER_LINE`,
    :549-571) — line-boundary granularity is a first-class openMSX accuracy mode. PIXEL
    accuracy (mid-line effects) exists and is explicitly out of M32 scope (§1.4 item 1).
  - The background converter consumes vertical scroll LIVE at draw time
    (`displayY += vdp.getVerticalScroll()`, :44-60).
- **fMSX (independent cross-reference)** — `references/fmsx-60/source/fMSX/MSX.c`: the
  entire display pipeline is per-scanline by construction — the per-line loop calls
  `(RefreshLine[ScrMode])(ScanLine)` as the scan advances (MSX.c:209-224, 2149-2155).
  **Both references agree** that per-line, beam-driven rendering is the architecture; our
  frozen-snapshot renderer is the outlier. No disagreement to arbitrate on the model itself.
- **Line-interrupt semantics — both references AGREE** (recorded per the DEC-0030
  discipline):
  - openMSX `references/openmsx-21.0/src/video/VDP.cc:518-576` (`scheduleHScan`): the match
    moment is `((controlRegs[19] - controlRegs[23]) & 0xFF)` display lines after display
    start — i.e. R#19 is compared against a line counter that starts at the vertical-scroll
    value, so the SCREEN-space match line is `(R#19 − R#23) & 0xFF`. FH rises at the start
    of the right border of the matched line (VDP.cc:913-923).
  - fMSX `references/fmsx-60/source/fMSX/MSX.c:2091-2104`: fires when
    `(((ScanLine+VScroll) & 0xFF) - VDP[19]) & 0xFF == 0` — algebraically the identical
    relation; `SetIRQ(INT_IE1)` gated on `VDP[0] & 0x10`; S#1 read clears FH
    (MSX.c:1148), matching our existing `v9958_vdp.cpp:227-234`.

### 2.2 Architecture decision 1 — accumulation buffer owner and lifecycle

**New device-layer class `VdpScanlineAccumulator`**
(`src/devices/video/vdp_scanline_accumulator.{h,cpp}`), owned by `Hbf1xvMachine` as a data
member declared after `vdp_frame_renderer_` (it binds `const V9958Vdp&` and
`const VdpFrameRenderer&`). Contract:

- State: a per-frame line store (per-line pixel rows at their rendered width + per-line
  metadata), a `watermark_` (first line not yet accumulated, 0-based display line), and the
  finalized `FrameBuffer` of the most recently completed frame.
- `sync_to_line(int exclusive_end_line)`: renders display lines
  `[watermark_, min(end, current_height))` via the EXISTING
  `VdpFrameRenderer::render_line()` (live registers — `render_line()` already reads the
  live `V9958Vdp` it references), advances `watermark_`. Idempotent; no-op for
  `end <= watermark_`. Negative/border raster positions clamp to 0.
- `finalize(Field)`: `sync_to_line(height)`, then materializes the frame per the §2.4
  geometry policy, stores it as "last completed frame", resets `watermark_ = 0` and the line
  store for the next frame.
- Wrap safety: if the raster position observed at a sync is BEHIND `watermark_` within the
  same frame (a step-only caller that never calls `on_vsync_boundary()` let the 262-line
  arithmetic wrap — the DEC-0034 loop-shape class), the accumulator finalizes-to-bottom and
  resets deterministically. No crash, no host-state dependence.
- Determinism: every output is a pure function of the machine's cycle history and the
  sequence of sync points (which are themselves cycle-derived). No wall clock, no host state.

**The legacy `VdpFrameRenderer::render_frame()` is NOT modified and NOT removed** — it stays
the in-test reference oracle for static-frame equivalence (§4 AC-4) and remains the honest
"snapshot of now" primitive. Only `Hbf1xvMachine::render_frame()`'s delegation changes.

### 2.3 Architecture decision 2 — the register-latch contract (sync hook)

**New nullable seam on the VDP** (X-pattern of `VdpClockSource`/`IrqLine`, the project's
established attach idiom):

```
class VdpRenderSyncListener { virtual void on_before_state_change() = 0; };
void V9958Vdp::attach_render_sync(VdpRenderSyncListener*);   // default nullptr = no-op
```

- Called at the TOP of `V9958Vdp::io_write()` for all four ports (#98 VRAM data, #99
  register/address, #9A palette, #9B indirect) BEFORE the write mutates any state — the
  direct analogue of openMSX's sync-before-change protocol (PixelRenderer.cc:253-394,
  510-517). One call site, uniform; covers control registers, palette, VRAM, and every
  command-engine mutation (all 13 commands execute inside register/data-port writes per the
  M22 hybrid model, so their VRAM effects are bracketed by hooked writes).
- `debug_io_write()` (the non-perturbing debug seam) does NOT invoke the hook — its
  documented contract stays "non-perturbing"; scenes built through it are covered by the
  lazy finalize path. Recorded as a documented property, with a dedicated unit test.
- The machine's listener adapter: reads `vdp_.raster_display_line()` = L and calls
  `accumulator_.sync_to_line(L + 1)` — i.e. **a write while the beam is on display line L
  takes effect from line L+1; lines ≤ L keep the pre-write state**. This is the line-granular
  simplification of openMSX's LINE-accuracy rounding (PixelRenderer.cc:560-567) and errs by
  at most one line against the PIXEL-accurate model; combined with interrupt-entry latency it
  places a split boundary within ~2 lines of real hardware (§2.6 test margins account for
  this).
- **Interrupt/status side effects of rendering: none.** The accumulator and hook read
  registers/VRAM and write only into their own pixel store. S#0/S#1/S#2 semantics, the
  DEC-0031 collision machinery, IRQ lines, and the command engine are untouched by the
  render path. (The separate line-interrupt DELIVERY leg is §2.5 — a distinct, explicitly
  flagged scope item, not a side effect of rendering.)

### 2.4 Architecture decision 3 — `render_frame()` semantics and every consumer

- **Signature unchanged**: `FrameBuffer Hbf1xvMachine::render_frame(Field) const`
  (`hbf1xv_machine.h:196`). Implementation re-routes to the accumulator:
  sync to the current raster line, then render the REMAINING lines below the beam from live
  registers into the returned frame ("accumulated past + projected future"). Properties:
  - Called at a frame boundary (all existing production call sites), this equals the
    finalized accumulated frame.
  - For any frame with NO mid-frame VDP write, it is byte-identical to the legacy snapshot
    renderer for ANY call position (every line renders from the same register state) — the
    §4 AC-4/AC-5 hard oracle.
  - Mid-frame calls return raster-true partial accumulation — a semantics upgrade, still
    deterministic (pure function of cycle history + call position).
  - Const-ness: the accumulator member is `mutable` with a documented logical-constness
    argument (committing lines the beam has already passed is memoization: a subsequent
    hooked write would commit the identical bytes, since no VDP state can change between the
    call and that write except through hooked writes themselves). No signature evolution
    needed.
- **Consumers audited (all four)**:
  1. `Sdl3App::run_one_frame()` (`sdl3_app.cpp:236-245`) — calls `render_frame()` immediately
     after `on_vsync_boundary()`: receives the finalized per-line frame. The SDL3 present
     path (`Sdl3VideoPresenter::blit_frame`, XRGB1555 blit) consumes `FrameBuffer`
     unchanged — zero presenter edits. `--border` composition unchanged.
  2. Headless dump path `Hbf1xvMachine::write_frame_dump()` (`hbf1xv_machine.cpp:941`) —
     unchanged call, same `FrameBuffer` serialization, `tools/frame-to-png.py` unaffected.
  3. `src/main.cpp:357` (vdp-render-parity evidence mode) — unchanged call; §4 AC-5's
     byte-identity oracle covers it.
  4. Tests calling `machine.render_frame()` / `VdpFrameRenderer::render_frame()` directly —
     the latter is untouched by design; the former is covered by AC-4/AC-5.
- **`on_vsync_boundary()`** gains exactly one additive call — `accumulator_.finalize(...)` —
  alongside the existing `vdp_.on_vsync()` / `pause_controller_.on_vsync()` /
  `last_vsync_cycle_` lines (`hbf1xv_machine.cpp:389-418`), preserving the M23/M25/M26
  one-added-line precedent. Ordering: finalize BEFORE `vdp_.on_vsync()` so the frame closes
  on the frame's own state (sprite tables for the NEXT frame are recomputed by `on_vsync()`
  after the old frame is sealed).
- **`run_frame()`/`run_cycles()` (no-CPU atomic paths)**: no hooked writes occur, so
  finalize renders the whole frame from current registers — automatically byte-identical to
  the legacy snapshot. No special-casing.
- **Geometry policy (mode change mid-frame)**: the finalized `FrameBuffer` takes the
  END-of-frame register state's width/height (exactly what the legacy snapshot renderer
  does today — static-frame identity is automatic). Lines accumulated under a different
  width are adapted deterministically (256→512 pixel-double; 512→256 even-sample); lines
  above the final height are dropped; lines never accumulated because height grew render at
  finalize from live state. Documented simplification; remainder row D10 (§6).

### 2.5 Architecture decision 4 — line-interrupt delivery (the flagged leg)

- **Placement**: `Hbf1xvMachine::step_cpu_instruction()`, immediately after the CPU step —
  the M25 pause-gate precedent for machine-level per-step logic (`hbf1xv_machine.cpp:445+`).
  NOT in `Z80aCpu`, NOT in `core::Scheduler` (which stays the pure cycle counter it is,
  `src/core/scheduler.h`). `run_cycles()`/`run_frame()` (no-CPU paths) deliver no line
  interrupts — exactly as they deliver VBlank only via explicit `on_vsync_boundary()` today.
- **Trigger rule** (both-references-agreed, §2.1): when the raster's display line crosses
  into line `M = (R#19 − R#23) & 0xFF` (evaluated against live registers), call
  `vdp_.on_line_match()` once for that crossing. The existing hook applies the IE1 gate and
  IRQ plumbing (`v9958_vdp.cpp:385-390`); S#1-read clear semantics already exist
  (`v9958_vdp.cpp:227-234`). Matches with `M >= frame line count` never fire (openMSX
  VDP.cc:554-559's "never occurs" clamp, line-granular analogue).
- **Cost bound**: O(1) compare per instruction (cache the next boundary cycle; recompute on
  crossing and on R#19/R#23/R#9 writes — openMSX's own reschedule semantic, VDP.cc:518-524).
  No per-step division requirement.
- **Precision disclosure**: openMSX raises FH at the matched line's right border
  (VDP.cc:913-923); our poll fires at the first instruction boundary after the raster enters
  the matched line — up to one instruction (≤ ~23 T-states) plus intra-line position early
  relative to openMSX. At line granularity, combined with the §2.3 L+1 write rule, handler
  writes land within ~2 lines of the reference — the §2.6 assertion margins encode this.
- **Regression posture**: with IE1 disabled (every current evidence scenario per A-M32-2),
  `on_line_match()` is a no-op on the IRQ line — `irq_horizontal_` is only set under
  `R#0 & 0x10`. S#1 FH visibility is likewise IE1-gated in our implementation
  (`v9958_vdp.cpp:445-452`). Zero behavior change for IE1-off software. (Note: fMSX models a
  set-then-immediately-clear FH for IE1-off — MSX.c:2104's comment path; openMSX sets FH
  regardless and gates only the IRQ. Our existing M14 implementation gates BOTH on IE1; this
  pre-existing disclosed narrowing is unchanged this cycle and recorded here for honesty.)

### 2.6 The synthetic split-screen test (universal oracle, no game assets)

New `tests/system/hbf1xv_m32_split_screen_system_test.cpp` (+ a narrower integration test,
§3 S3): a synthetic in-RAM Z80 program (M24 harness / flat-RAM-program precedent — zero
Aleste knowledge, zero game bytes):

1. Program the VDP to GRAPHIC4 (SCREEN 5); fill VRAM so that every display row's content
   encodes its own SOURCE row index (e.g. a per-row 8-pixel binary row-number signature) —
   making the applied scroll offset of every output row machine-checkable.
2. Set R#23 = 0; write R#19 = S (split line, e.g. 80, chosen so
   `(R#19 − R#23) & 0xFF = S` at fire time); enable IE0 + IE1 (R#1 bit5, R#0 bit4).
3. Interrupt handler: read S#0 (dispatch VBlank vs line via S#1 FH); on the line interrupt:
   read S#1 (clears FH), write R#23 = 128, `EI`/`RETI`. On VBlank: rewrite R#23 = 0 and
   R#19 = S (re-arming; NB the match line depends on live R#23 — the both-references
   `(R#19 − R#23) & 0xFF` relation makes re-arming after the R#23 rewrite mandatory, and the
   test deliberately exercises exactly that).
4. Drive via the real frame-loop shape (`step_cpu_instruction()` + `on_vsync_boundary()`,
   the C5-closure/`run_one_frame()` shape) for ≥3 frames; take `render_frame()`.
5. Assert: rows `[0, S-2]` carry source-row signatures at offset 0; rows `[S+2, height)`
   carry signatures at offset 128 (mod-256 wrap per `vertical_scroll_wrap`,
   `vdp_frame_renderer.h:114-121`); the ±2-line boundary band is excluded from assertions
   (§2.5 precision disclosure). Assert S#1 FH was observed set by the handler (via a
   program-memory flag). Assert two independent machines produce byte-identical frames and
   event logs (M27 determinism pattern).
6. Adversarial arm: the same program run with IE1 masked off must produce a NO-split frame
   (all rows offset 0) — proving the test discriminates and the interrupt leg is load-bearing.

### 2.7 openMSX A/B plan for the split scenario (Defect A evidence)

New `tools/openmsx-m32-split-ab.ps1` (lineage: `tools/openmsx-ab-smoke.ps1` +
`tools/openmsx-m29-scc-parity.ps1` conventions; WSL `/usr/bin/openmsx`, `Sony_HB-F1XV`
machine, power-cycle methodology per the DEC-0028 standing rule):

- Side A (ours): the §2.6 synthetic program run headlessly, frame dumped to PNG.
- Side B (openMSX): the same program bytes injected/driven via Tcl (source-verify the
  mechanism BEFORE scripting — R-M30-6 discipline; A-M32-3), `screenshot` captured after the
  same settle time.
- Comparison gate: REGION-STRUCTURAL, not byte-level (different scalers/palette pipelines):
  for each side independently, recover each output row's source-row signature and assert both
  sides show the same two-region offset structure {0 above ≈S, 128 below ≈S} with the split
  boundary within ±2 lines of each other. Adversarial comparator self-check (corrupt one
  region on a copy → must flag) per the M28/M29 precedent.
- Disposition rules: PARITY / DIVERGENCE(with analysis) / BLOCKED(with cited reasoning) —
  never silently skipped. Artifact: `docs/m32-parity-trace-diff.md`.

### 2.8 Defect B — the derived constant, shown end to end

**Reference ratio.** `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:63` sets PSG
`<volume>21000</volume>`; `:191` sets MSX-MUSIC `<volume>9000</volume>` → FM:PSG = 3:7
(≈ −7.36 dB).

**What the ratio applies to (source-verified this cycle, the derivation's crux).** openMSX
normalizes BOTH chips to the same per-channel unit before the XML volumes apply:

- AY8910: per-channel volume table peaks at exactly 1.0
  (`references/openmsx-21.0/src/sound/AY8910.cc:64-93` — `volumeTab[15] =
  YM2149EnvelopeTab[31] = 1.0`), and its device amplification factor is 1.0
  (`AY8910.cc:977-980`).
- YM2413 (Okazaki core): per-slot linear peak is `(1 << DB2LIN_AMP_BITS) − 1 = 255`
  (`references/openmsx-21.0/src/sound/YM2413Okazaki.cc:48` `DB2LIN_AMP_BITS = 8`;
  `:154-165` table construction), and the device amplification factor is
  `1 / (1 << DB2LIN_AMP_BITS) = 1/256` (`:1051-1054`) → per-channel peak ≈ 255/256 ≈ 1.0.

So 21000:9000 is a **per-channel-unit loudness ratio**. (The alternative reading — normalize
FM by its 9-channel+rhythm×2 theoretical worst-case SUM — is rejected with reasoning: it
yields k = 10,628/2,304 ≈ 4.6 (melody) or 10,628/4,096 ≈ 2.6 (rhythm), i.e. "no change or
quieter", contradicting both the measured 29 dB defect and openMSX's source-verified
per-channel normalization above. That worst-case-sum framing is precisely the arithmetic that
produced kFmAmplitudeScale = 5 in M31 — the header's own `9*256*5` clamp example,
`machine_audio_mixer.h:82-90`. The worst-case sum belongs in the CLAMP analysis, below, not
in the loudness target.)

**Our units.**

- PSG per-channel resolved amplitude: 0..31 (`src/devices/audio/psg_ym2149.h:88-96`);
  per-PCM-side full scale 62 = two chip channels (center A + the side's own B or C, the B1
  stereo law), giving the familiar 62 × 400 = 24,800 (`machine_audio_mixer.h:39-41`; the
  M31-measured PSG effects peak). One full-volume PSG channel = 31 × 400 = **12,400** PCM.
- FM per-channel peak: ±256 — the synth's ±2048 operator width through the documented `>>3`
  per-channel presentation scale (`src/devices/audio/ym2413_synth.h:128-131`), anchored to
  the fact-sheet §7 measured per-volume peak series "255, 180, 127, ..."
  (`references/fact-sheets/Yamaha YM2413 FM Chip.md:184`; §7's "blend ratio varies by
  machine" note at :186 is why an external reference ratio is the right calibrator).

**The constant.**

```
kFmAmplitudeScale = round( kPsgAmplitudeScale × PSG_per_channel_raw × (9000/21000) / FM_per_channel_raw )
                  = round( 400 × 31 × 3/7 / 256 )
                  = round( 12,400 × 0.42857 / 256 )
                  = round( 5,314.3 / 256 )
                  = round( 20.76 )
                  = 21
```

**Cross-check against DEC-0039's own full-scale form**: the charter's "24,800 × 9000/21000 ≈
10,600 for a full-scale FM mix" — 24,800 is two PSG channel-units per side, so the FM
equivalent is two FM channel-units: 2 × 256 × 21 = **10,752**, within 1.2% of 10,600 (the
residue is integer rounding of k). Resulting single-channel ratio: (256×21)/12,400 = 0.4335
vs reference 0.42857 → −7.26 dB vs −7.36 dB. FM loudness rises 21/5 = 4.2× (+12.5 dB) over
v1.0.32; the committed Aleste FM peak 900 would re-measure ≈ 3,780.

**Clamp math redone (goes verbatim-class into the header comment).** Worst theoretical
alignments at k = 21: melody 9 × 256 × 21 = 48,384 (FM alone exceeds int16); rhythm-mode
6 × 256 + 5 × (256 × 2) = 4,096 raw (§6 rhythm ×2 double-output quirk, fact-sheet :179,
:183) → 86,016; plus PSG 24,800 and two SCCs 14,400 → 125,216 ≫ 32,767. **The int16 clamp is
therefore REQUIRED and is unit-tested at a constructed worst case** (updated
`machine_audio_mixer_fm_unit_test.cpp` saturation case: all-9-channels-aligned FM + max PSG +
two max SCCs → exact clamp to ±32,767 asserted on both stereo sides). Realistic music sits
far below (measured raw FM peak 180 → 3,780).

**Defect B oracles.**

1. **Zero-YM2413 byte-identity HARD oracle stays green untouched** — k multiplies a zero
   term for `fm == nullptr` and for the silent-attached OPLL; the existing M31 oracle tests
   (whose reference side is the literal v1.0.31 arithmetic, independent of k) are re-run and
   must pass unmodified.
2. **New deterministic loudness-ratio test**: single full-volume FM carrier (the M31 S2
   full-volume test-patch setup whose peak ≈ 255) vs single full-volume PSG channel
   (amplitude 31 square), each mixed alone through `MachineAudioMixer`; assert
   `peak_FM / peak_PSG` ∈ [0.42857 × 0.85, 0.42857 × 1.15] (±15% bound absorbs waveform
   shape, EG settle, ZOH — the bound is derived from the reference ratio, not tuned to the
   implementation).
3. **Constructed-worst-case saturation test** (above) — clamping behavior at the new scale is
   explicitly unit-tested.
4. **Evidence re-capture**: the fmON/fmOFF deterministic pair re-captured at k = 21
   (`debug/sounds/m32-fm-aleste-fmON/fmOFF.wav`), showing the new audible level; the M31 pair
   stays committed as the historical baseline. Python-decoded peak/divergence figures recorded
   in the implementation report.

---

## 3. Milestones (slices)

### S1 — Accumulator + render-sync seam (device layer only; zero machine behavior change)

Scope: `VdpScanlineAccumulator` (§2.2); `VdpRenderSyncListener` seam on `V9958Vdp` (§2.3,
default nullptr = byte-identical no-op, proven); `debug_io_write` hook-exclusion property
test. The developer re-confirms the §1.2 zero-call-site finding and records it in the
implementation report.
Deliverables: `src/devices/video/vdp_scanline_accumulator.{h,cpp}`; minimal additive edits to
`v9958_vdp.{h,cpp}` (attach + one hook call site); unit tests.
Acceptance (slice): static-frame equivalence oracle green across the §5 mode matrix — for
each mode/state, accumulator output (synced in randomized line-chunk patterns with no
interleaved writes) byte-identical to legacy `VdpFrameRenderer::render_frame()`. Null-listener
V9958Vdp byte-identical (existing VDP unit suites untouched and green).

### S2 — Machine wiring: write-hook, finalize, `render_frame()` re-route, line-int delivery

Scope: machine listener adapter (sync-to-L+1 rule); `on_vsync_boundary()` finalize (one
additive call, ordering per §2.4); `Hbf1xvMachine::render_frame()` re-route (signature
unchanged, mutable-accumulator logical constness documented); §2.5 line-interrupt delivery in
`step_cpu_instruction()` with the O(1) cached-threshold design; A-M32-2 IE1 survey across the
evidence scenarios.
Acceptance (slice): full existing fast suites green (headless + SDL3-ON); §4 AC-5 committed
frame-evidence byte-identity sweep green; boot-logo integration timeline unchanged
(`entry=75 exit=259 f4_end=0x80`); new unit/integration tests for the L+1 latch rule and the
line-int trigger rule (incl. the `(R#19 − R#23) & 0xFF` re-arm subtlety and IE1-off no-op).

### S3 — Split-screen proof: synthetic system test + openMSX A/B + Aleste 2 live smoke

Scope: §2.6 synthetic split system test (+ integration-level variant asserting the latch/int
mechanics without full-frame assertions); §2.7 A/B script + artifact; Aleste 2 scripted smoke
(M27 `--input-script` + frame dumps) reaching GAMEPLAY past weapon select with a non-garbage,
correctly-split playfield — the human's exact repro — with PNG evidence committed; A-M32-1
protocol logging.
Acceptance (slice): §4 AC-7/8/9.

### S4 — FM mix calibration

Scope: `kFmAmplitudeScale` 5 → 21 + full header-comment rewrite (derivation + redone clamp
math per §2.8); saturation test update; new loudness-ratio test; zero-YM2413 oracle re-run;
fmON/fmOFF re-capture. NOTHING else in the audio chain changes (§1.4 item 5; git-diff-proven).
Acceptance (slice): §4 AC-10..13.

### S5 — Regression + evidence gate, ledger, docs

Scope: full evidence-gate run (§4 AC-14); `docs/m32-implementation-report.md`;
`docs/m32-parity-trace-diff.md`; ledger updates (§6: C10/F1/F2 label shifts re-recorded, new
rows D8/D9/D10, open rows re-affirmed); state files; handoff to QA.

Suggested order S1→S2→S3→S4→S5; S4 is independent of S1-S3 and may be interleaved, but its
evidence re-capture must run on the final tree.

---

## 4. Acceptance Criteria (milestone gate — all must hold)

1. **No `src/devices/cpu/` or `src/core/` edits** (`git diff v1.0.32` empty for both trees);
   ZEXALL/ZEXDOC therefore NOT run (fast-subset cadence). If this criterion is ever at risk,
   STOP and escalate (R-M32-1).
2. **Universal-fix audit**: no game-keyed logic anywhere in the diff (grep + review; the
   standing memory's bar).
3. **License isolation**: no code/table transcription from openMSX/fMSX; every behavior claim
   in code comments cites concrete `references/...` paths; the §2.1/§2.8 citations appear in
   the shipped headers.
4. **Static-frame equivalence HARD oracle**: for any frame during which no VDP register/
   palette/VRAM write occurs, the per-line accumulated output is BYTE-IDENTICAL to the legacy
   snapshot renderer — proven by the S1 matrix suite (all §5 modes, sprites on/off, scroll
   values, border cases) AND by AC-5.
5. **Committed evidence byte-identity**: regenerating the committed frame evidence on the M32
   tree reproduces byte-identical images (SHA-256/PIL diff bbox=None): boot-logo PNGs
   (DEC-0031 set), Metal Gear gameplay, Konami splash, DOS/BASIC prompt, m31-rc-* matrix
   PNGs, `debug/frames/m26-example-boot.png`, `debug/frames/c5-verify-settled.png`. The
   DEC-0031 boot-logo integration test passes unmodified (`entry=75 exit=259 f4_end=0x80`).
6. **Trajectory-shift escalation rule**: if any AC-5 scenario diverges, the developer must
   root-cause it; if caused by that title legitimately using IE1 (A-M32-2 falsified), the
   finding goes to the coordinator with an openMSX A/B of the affected scene BEFORE any
   evidence regeneration — never a silent re-baseline.
7. **Synthetic split test green** (§2.6), including the adversarial IE1-off no-split arm and
   the two-machine byte-identical determinism check.
8. **openMSX A/B for the split scenario** executed per §2.7 with a recorded
   PARITY/DIVERGENCE/BLOCKED disposition and comparator self-check
   (`docs/m32-parity-trace-diff.md`). BLOCKED requires cited, checked reasoning (M25/M26
   standard).
9. **Aleste 2 live smoke**: scripted, deterministic run reaches gameplay past weapon select;
   committed frame dump(s) show HUD + playfield with DIFFERENT effective scroll regions and a
   non-garbage playfield (the human's repro resolved). Sprite/background consistency observed
   and honestly recorded (§1.4 item 2 watch-item). FM BGM audibility noted from the same run
   (ties to S4).
10. **kFmAmplitudeScale = 21** lands with the §2.8 derivation and redone clamp math in the
    header; the constructed-worst-case saturation unit test passes at the new scale.
11. **Zero-YM2413 byte-identity oracle** passes unmodified (nullptr AND silent-attached).
12. **Loudness-ratio oracle** green: single-carrier FM vs single-channel PSG peak ratio within
    ±15% of 9000/21000.
13. **fmON/fmOFF evidence pair re-captured** at the new scale, python-decoded figures in the
    report; audible-divergence property re-confirmed.
14. **Evidence gates**: `tools/validate-assets.ps1` green; `tools/checksum-assets.ps1
    -OutFile docs/asset-checksums.txt` refreshed; `cmake --build build --config Debug` clean;
    `ctest --test-dir build -C Debug --output-on-failure` fast subsets green in BOTH configs
    (headless and SDL3-ON, dummy drivers) with counts recorded; behavior-affecting change ⇒
    the A/B obligations here are AC-8 plus `tools/openmsx-ab-smoke.ps1` →
    `docs/openmsx-ab-smoke.md` refresh.
15. **Determinism**: the M27 replay-determinism system test and the M10 event-log golden stay
    green; the split system test's own two-run identity holds.
16. **Ledger/docs**: §6 ledger notes applied; implementation report includes the §1.2
    zero-call-site finding, A-M32-1..4 verification outcomes, and the §2.5 precision
    disclosures.

## 5. Test Matrix (additions; naming follows existing conventions)

| # | Test (new/updated) | Layer | Proves | Slice |
|---|---|---|---|---|
| 1 | `tests/unit/devices/video_vdp_scanline_accumulator_unit_test.cpp` (NEW) | unit | Static-frame equivalence vs legacy snapshot across modes TEXT1/TEXT2/G1/G2-3/MC/G4/G5/G6/G7/YJK/YJK+YAE, sprites on/off, R#23≠0, blank modes; randomized chunked sync; wrap-safety reset; geometry-adaptation determinism (256↔512 mid-frame) | S1 |
| 2 | `tests/unit/devices/video_v9958_render_sync_seam_unit_test.cpp` (NEW) | unit | Null listener = byte-identical VDP behavior; listener called before state mutates (old-state visibility); `debug_io_write` never calls the hook | S1 |
| 3 | `tests/integration/machine/hbf1xv_m32_per_line_latch_integration_test.cpp` (NEW) | integration | Machine-level L+1 latch rule: an R#23 write at raster line L changes rendered lines only from L+1; write during vblank affects whole frame; `render_frame()` at boundary == finalized frame | S2 |
| 4 | `tests/integration/machine/hbf1xv_m32_line_interrupt_integration_test.cpp` (NEW) | integration | `(R#19 − R#23) & 0xFF` match (both-references relation); once-per-crossing; IE1-off ⇒ no IRQ, no CPU-visible change; S#1 read clears FH; re-arm after R#23 rewrite | S2 |
| 5 | Existing full fast suites (headless 171 + SDL3-ON 180 baselines) | all | No regression; boot-logo timeline unchanged; M27 determinism + M10 golden green | S2/S5 |
| 6 | AC-5 evidence byte-identity sweep (scripted via existing tools, results in report) | system/evidence | Committed PNG set reproduces byte-identically | S2/S5 |
| 7 | `tests/system/hbf1xv_m32_split_screen_system_test.cpp` (NEW) | system | The Aleste-shape universal oracle: synthetic in-RAM program, IE1 handler rewrites R#23 mid-frame ⇒ two regions with different scroll offsets (±2-line boundary band); adversarial IE1-off arm ⇒ no split; two-run byte identity | S3 |
| 8 | `tools/openmsx-m32-split-ab.ps1` → `docs/m32-parity-trace-diff.md` (NEW) | A/B evidence | Region-structural split parity vs real openMSX (power-cycle rule; comparator self-check) | S3 |
| 9 | Aleste 2 scripted smoke (M27 input-script; PNGs to `debug/frames/`) | live smoke | Human repro resolved: gameplay reached, split playfield non-garbage | S3 |
| 10 | `tests/unit/frontend/machine_audio_mixer_fm_unit_test.cpp` (UPDATED) | unit | k=21 arithmetic; constructed worst-case saturation clamps exactly at ±32,767; zero-YM2413 hard oracle unmodified and green | S4 |
| 11 | `tests/unit/frontend/machine_audio_mixer_loudness_ratio_unit_test.cpp` (NEW) | unit | Single-carrier FM vs single-channel PSG peak ratio within ±15% of 9000/21000 | S4 |
| 12 | fmON/fmOFF re-capture (`debug/sounds/m32-fm-aleste-*.wav`) + python decode | evidence | New audible FM level; divergence at first key-on preserved | S4 |

## 6. Ledger Notes (deferred-backlog obligations, per DEC-0005)

- **Backlog rows this milestone closes: none.** M32 is a defect-pair milestone; it closes the
  two DEC-0039 defects, not backlog rows.
- **DEC-0039 numeric-label shifts to re-record in `deferred-backlog.md` at M32 closure**:
  C10 (FDC flux/DMK) → **M33-era**; F1 (cassette tape-image/signal fidelity) → **M34-era**;
  F2 (printer rendering depth) → **M35-era** (informal-era labels only; final numbers at
  their own kickoffs, per the DEC-0035/DEC-0039 convention).
- **Open rows re-affirmed UNCHANGED**: C1/D4 (sourcing-blocked, UNBUILDABLE-WITHOUT-
  FABRICATION ruling stands, no owner); E3 (YM2413 cycle-exactness remainder, sourcing-
  blocked, no owner); G3 (runtime hot-plug), G4 (~80-type mapper long tail), G5 (SCC-I/SCC+)
  — on-demand, unscheduled.
- **NEW remainder rows to add at closure** (M32-origin, committed scope, unscheduled):
  - **D8 — sub-line (pixel-granular) rendering accuracy**: mid-line register effects
    (openMSX `Accuracy::PIXEL`, PixelRenderer.cc:549-571, :253-258). M32 ships openMSX's
    LINE-accuracy equivalent by design.
  - **D9 — per-line live sprite attribute fetch**: sprite checking against live per-line
    registers (openMSX SpriteChecker model) vs our per-frame `recompute_frame()` snapshot
    (`v9958_vdp.cpp:360-380`, `sprite_engine.cpp:126`); includes the split-frame sprite-
    offset analysis of §1.4 item 2.
  - **D10 — mid-frame display-geometry-change fidelity**: per-line-perfect handling of
    512↔256 / 212↔192 mid-frame switches beyond the §2.4 deterministic adaptation policy.

## 7. Risks and Open Questions (coordinator arbitration items first)

- **D-1 (ARBITRATE BEFORE DISPATCH — the loud flag): line-interrupt delivery is new
  machine-level behavior**, not a pure renderer change. §1.2's finding (zero
  `on_line_match()` production call sites) makes it a hard prerequisite for every split
  oracle in this package. It lives entirely in `src/machine/` + one existing
  `src/devices/video/` hook — `src/core/` and `src/devices/cpu/` untouched, so **ZEXALL is
  still NOT triggered** — but DEC-0039's "interrupt/status side effects UNCHANGED" sentence
  should be consciously re-read as "unchanged FOR THE RENDER PATH; line-interrupt delivery is
  an explicit, separate M32 scope item". Requested: coordinator ratification (a decisions.md
  note at dispatch suffices).
- **D-2 (ARBITRATE): kFmAmplitudeScale = 21 vs the charter's parenthetical.** DEC-0039/the
  dispatch sketch reads "…≈10,600 for a full-scale FM mix … 9 channels + rhythm ×2 define the
  full-scale sum". Taken literally (divide 10,600 by the 9-channel or rhythm worst-case SUM),
  that arithmetic yields k ≈ 4.6 or ≈ 2.6 — no change or quieter, reproducing M31's own
  flawed normalization and contradicting the measured defect. §2.8's source-verified
  finding (openMSX normalizes both chips per channel: AY8910.cc:64-93/977-980,
  YM2413Okazaki.cc:48/154-165/1051-1054) resolves the ambiguity: the ratio is per-channel,
  k = 21, and the charter's own 10,600 figure is recovered as two channel-units (10,752).
  The worst-case sums are used where they belong — the clamp analysis. Requested:
  ratification of this refinement.
- **R-M32-1 (ZEXALL tripwire)**: if implementation genuinely requires touching
  `src/core/scheduler.*` or anything under `src/devices/cpu/`, the developer STOPS and
  escalates; if the coordinator approves such a touch, the standing carve-out fires one
  immediate full ZEXALL/ZEXDOC sweep. The package's design needs neither (O(1) per-step
  compare in `step_cpu_instruction()`, M25 precedent).
- **R-M32-2 (evidence-trajectory shifts)**: a title in the evidence set might enable IE1
  (A-M32-2 false) and legitimately change behavior — handled by AC-6's
  escalate-don't-rebaseline rule.
- **R-M32-3 (per-step overhead)**: the line-int check and write-hook add per-instruction/
  per-OUT work. Mitigation: cached-threshold compare (§2.5); watermark early-out (§2.3);
  no allocation per line (reused staging rows). The DEC-0033 pacing evidence (59.9227 fps
  real-time headroom) gives ample margin; if interactive frame rate measurably drops, record
  measurements in the report.
- **R-M32-4 (split-boundary precision)**: ±2-line placement vs openMSX (poll granularity +
  L+1 latch rule). Encoded in test margins and the A/B gate; refinement belongs to D8.
- **R-M32-5 (sprite/background consistency in split frames)**: §1.4 item 2's analysis says
  playfield sprites should be near-correct in the common arrangement; if the Aleste smoke
  shows visible sprite displacement, record it against new row D9 (not a silent fix, not a
  scope creep).
- **R-M32-6 (A/B injectability)**: driving the synthetic program inside openMSX may prove
  Tcl-awkward (A-M32-3); fallback: a minimal .cas/.rom-shaped carrier built by the test
  tooling (still no game assets), else honest BLOCKED.
- **R-M32-7 (loudness end-judgment)**: k = 21 is the reference-honest constant, but perceived
  balance is content-dependent (§7 fact-sheet note). The human's ear on the re-captured
  evidence/live play is the final acceptance signal; any further adjustment must repeat the
  §2.8 derivation discipline with a cited basis — never to-taste.

## 8. Exit Criteria

All §4 acceptance criteria hold with recorded evidence; `docs/m32-implementation-report.md`
and `docs/m32-parity-trace-diff.md` written; QA sign-off obtained (planner→developer→QA→
coordinator closure per protocol); ledger/state files current; tag v1.0.33 on coordinator
release decision.

## 9. Developer Handoff (dispatch summary)

Build S1→S5 per §3. Read FIRST: this package §1.2/§2.1-§2.5 (Defect A) and §2.8 (Defect B);
then `references/openmsx-21.0/src/video/PixelRenderer.cc` (sync-before-change protocol),
`references/openmsx-21.0/src/video/VDP.cc:518-576` + `references/fmsx-60/source/fMSX/
MSX.c:2091-2104` (agreed line-match relation), and the cited local files. Hard rules: no
`src/devices/cpu/` or `src/core/` edits (R-M32-1 tripwire); no reference-code/table copying;
no game-keyed logic; `AudioPacer`/`PsgAudioPump`/synth files byte-untouched; legacy
`VdpFrameRenderer::render_frame()` untouched (it is the oracle). Do not start S2's line-int
wiring or S4's constant before the coordinator resolves §7 D-1/D-2. Evidence gates per §4
AC-14; every claim in the implementation report maps to a command output or file read.
