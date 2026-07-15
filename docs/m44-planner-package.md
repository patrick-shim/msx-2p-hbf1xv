# M44 Planner Package — DEF-M44-CMDSYNC Phase 1 (Laydock 2 HUD command-engine render-sync)

- Decision of record: **DEC-0065** (`agent-protocol/channels/decisions.md:778-785`) — M44 opened, PHASED.
- Milestone: **M44 (Phase 1)**, release target **v1.1.1**, owner-run tag+push per DEC-0047.
- Spec owner: MSX Planner. Developer owner: MSX Developer. QA owner: MSX QA.
- Scope class: **VDP render-timing accuracy**, `src/devices/video` only. **NO** `src/core`, **NO**
  `src/devices/cpu` (mechanical ZEXALL per-edit trigger does not fire; a FULL screen-mode +
  command-engine openMSX A/B is required because blast radius touches every command-drawn screen).
- Discipline: planner-first + **system-wide-review-first** (`feedback_system_wide_review_first`) +
  **universal-fixes-only** (`feedback_universal_fixes_only`). This is a generic V9958
  render-observation-timing fix, not a Laydock-specific patch.

This package is **spec + mechanism + acceptance only**. No production code is written or modified here.

---

## 1. Scope and Assumptions

### 1.1 In scope (Phase 1 — the contained flicker fix)

Give the V9958 command engine's **atomic** bitmap-VRAM writes correct render-**observation** timing
so each display row is committed by the M32 scanline accumulator against the correct
partial-command state — WITHOUT the full per-mode access-slot cycle model. Concretely:

- A default-inert wiring seam letting the command engine's per-destination-row writes drive the
  M32 render-sync (accumulator) exactly like openMSX routes command writes through
  `VDPVRAM::cmdWrite` → `writeCommon` → `VRAMWindow::notify` before committing the byte
  (`references/openmsx-21.0/src/video/VDPVRAM.hh:428, 575-593, 318-322`).
- The address→display-line mapping needed to place each command destination row on its correct
  scanline (feasible via the exact inverse of the renderer's vertical-scroll geometry — §3.3).
- Fold the SECONDARY latent gap (command writes to sprite attribute/pattern tables) into the same
  seam **only to the extent it is cheap** (§3.6); otherwise it degrades to today's behavior with no
  regression and is documented as a Phase-1 residual.

### 1.2 Out of scope (Phase 2 — tracked follow-on, separate milestone; DEC-0065)

- Incremental `execute*(EmuTime limit)` command execution over emulated time.
- Per-mode **access-slot** cycle calculator and exact **CE / S#2 bit0 (busy) duration** so
  busy-wait-on-CE games are cycle-exact (the COUPLED gap; DEC-0065 Phase 2).
- Any Z80 T-state / core / scheduler change. Any change under `src/core` or `src/devices/cpu`.

### 1.3 Non-goals

- No new game-specific code paths. No Laydock detection. The fix keys only on generic VDP state
  (mode / page / R#23 vertical scroll / raster position).
- No weakening of any existing command-engine result oracle (M22) or M32 byte-identity oracle.
- No SDL3/frontend behavior change beyond the frame content the accumulator already feeds it.

### 1.4 Assumptions (each carries a verification action)

- **A-1 (central):** Phase-1 observation timing alone removes the human-visible Laydock 2 HUD
  tear/flicker to openMSX-crisp parity. **Verify:** the reused `debug/laydock2-flicker/` `.omr` A/B
  (developer captures OURS vs `omsx/`; QA re-runs). **Honest residual pre-authorized (§2.5, R-1):**
  if a *bounded* frame-to-frame jitter remains, it is the Phase-2 CE-pacing coupling (the flicker's
  secondary driver), documented as the Phase-1/2 boundary — NOT a Phase-1 failure.
- **A-2:** Laydock 2's tearing writes are **atomic** commands (LMMV/LMMM/HMMV/HMMM/YMMM), not
  event-driven transfers. Grounded: the event-driven LMMC/HMMC/LMCM transfers write one unit per
  `R#44`/`S#7` port interaction, and each such port write is a separate `io_write` that already
  fires `on_before_state_change()` at the beam (v9958_vdp.cpp:125) — so transfers are already
  per-unit synced; only the atomic burst (whole `NX*NY` inside one `R#46` write) bypasses the seam.
  **Verify:** developer confirms via a command-trace of the Laydock repro that the HUD is rebuilt
  with atomic fills/copies. (Snapshot `debug/laydock2-flicker/.../vdp.txt` shows GRAPHIC4,
  R#23=0xA4, plus an armed transfer — both paths present; the atomic path is the tear source.)
- **A-3:** The destination page-row → display-line inverse `L = ((dy & 0xFF) − R#23) & 0xFF` is
  exact for GRAPHIC4/5 and the same page-row inverse holds for GRAPHIC6/7. **Verify:** it is the
  literal inverse of `VdpFrameRenderer::vertical_scroll_wrap` (vdp_frame_renderer.cpp:170-171); the
  new deterministic test (§4.4) pins it under a non-tautology mutation.
- **A-4:** Default-inert wiring (no row-sink installed / null) is byte-identical to v1.1.0 for every
  current oracle and A/B. **Verify:** the M32 zero-listener byte-identity unit oracle stays green
  unmodified; headless + SDL3 fast ctest unchanged counts; M41 screen-mode A/Bs 0.000%.

---

## 2. Spec Summary

### 2.1 Confirmed root cause (recap — NOT re-investigated; from DEC-0065 + `debug/laydock2-flicker/`)

Our 10 VDP commands execute as a **zero-cycle burst**: for the atomic commands the whole `NX*NY`
sequence of `vram_.write()` calls happens inside the single `R#46` (CMD) `io_write`
(`vdp_command_engine.cpp:293-312` dispatch; the `for(;;)` loops at `:362/391/425/519/548`, plus
`run_pset`/`run_line`). `VdpVram::write()` (`vdp_vram.h:59`) is a plain store — no timestamp, no
notify; the command engine holds only a `VdpVram&`, never wired into the M32 render-sync. The
`io_write` seam fires exactly **one** `on_before_state_change()` before the `R#46` write
(v9958_vdp.cpp:117-127), committing accumulator lines up to the current beam line from the
**pre-command** state; then the entire rectangle applies at once. Across a frame's MANY HUD blits,
the accumulator freezes HUD scanlines at a **patchwork of intermediate blit states**, and because
the CE clears instantly the game's blit loop is **unpaced** — its beam phase drifts frame-to-frame,
so the patchwork boundary jitters → **flicker**.

Evidence (`debug/laydock2-flicker/`): `VERDICT_triptych.png` — FINAL VRAM page-3 HUD is **CLEAN**
(the command engine computes the correct result), openMSX DISPLAY is **CLEAN and stable**
(`omsx/oms_005..011` byte-identical), OURS DISPLAY is **TORN** (red bars, blue-noise score digits,
`GAME OVER` bleed) and **changes every few frames** (`hud_montage.png` /
`COMPARE_hud_ours_vs_openmsx.png`, frames 10297/10300/10304/10308 all differ). Snapshot registers
(`.../vdp.txt`): `MODE=GRAPHIC4`, `R#23=0xA4` (vertical scroll 164), `R#25=0x03` (SP2 + MSK
multi-page fine H-scroll), `R#26=0x19`, `R#27=0x06`. This is a pure render-**observation**-timing
defect: the bytes are right; **when each display row observes them is wrong.**

openMSX ground truth: every command write goes through `VDPVRAM::writeCommon(addr, val, time)`,
which — **before** committing the byte — notifies `bitmapVisibleWindow` / `spriteAttribTable` /
`spritePatternTable` ("Subsystem synchronisation should happen before the commit, to be able to
draw backlog using old state", `VDPVRAM.hh:587-591`); `VRAMWindow::notify(addr,time)` fires only if
`isInside(addr)` and calls `observer->updateVRAM(addr − baseAddr, time)` → `renderUntil(time)`
(`VDPVRAM.hh:309-322`). openMSX executes the command **incrementally over emulated time**, so each
write carries a distinct `time`; the renderer draws each display row from the VRAM state at that
row's beam-time → crisp.

### 2.2 The central design question (resolved)

> Can the flicker be eliminated with a render-**observation-timing** fix that does NOT require the
> per-mode access-slot CYCLE model (Phase 2)? Because our command is an instantaneous burst, a naive
> "notify at the current beam line for every write" notifies ALL writes at the same line and does
> not distribute them across the HUD scanlines.

Answer: **YES for the dominant, human-visible artifact (the intra-frame patchwork tear), via
Mechanism A below; with an honest, pre-authorized Phase-2-coupled residual for the last sliver of
frame-to-frame stability (§2.5).** We evaluated three candidates.

### 2.3 Mechanism evaluation

**(A) Address → display-line mapping + per-destination-row render-sync (openMSX-window-notify
analog, adapted to a burst). — CHOSEN (primary).**
For each destination ROW a command writes, map its destination page-row to a display line and fire
the render-sync to commit the accumulator **up to that line** from the pre-write state BEFORE the
row's bytes are written. This is the exact structural analog of openMSX's
`writeCommon`→`window.notify(addr,time)`, with the **destination display line standing in for
openMSX's `EmuTime`** (which we lack without the Phase-2 cycle model).

- Feasibility of address→display-line: **HIGH.** The vertical mapping is the *literal inverse* of
  the renderer: `VdpFrameRenderer::vertical_scroll_wrap(line) = (line + R#23) & 0xFF`
  (vdp_frame_renderer.cpp:170-171) ⇒ display line `L = ((dy & 0xFF) − R#23) & 0xFF` for a
  destination page-row `dy`. The command engine already holds `dy` in every atomic loop. **The hard
  part — the horizontal SP2 / R#26 / R#27 / MSK multi-page geometry — needs NO inversion**: the
  accumulator's `render_line(L, …)` already reads it live from VRAM + registers
  (vdp_frame_renderer.cpp:174-370, `scrolled_name_col`/`compose_bitmap_scroll`/
  `bitmap_scroll_pages`). We only need the vertical mapping to decide *which display line to commit
  to*; `render_line` handles everything horizontal. Laydock 2's GRAPHIC4 + R#23 split + SP2 is
  therefore fully covered.
- The visible-page gate is openMSX's own: only fire when the destination address is inside the
  currently-displayed bitmap page (`isInside(bitmapVisibleWindow)` analog, `VDPVRAM.hh:309-311`) —
  this correctly skips Laydock's off-screen source/template page (snapshot `SY=752`, a page-2+
  source) so those writes commit nothing spuriously.

**(B) Distribute the burst's writes across the raster in beam order without cycle counts.**
This is subsumed by (A): (A) *is* raster-order distribution, but keyed on the destination line
(deterministic, geometry-derived, stable frame-to-frame) rather than an invented beam order. A pure
(B) that assigns each row a synthetic raster line would have to invent a per-command duration to
choose those lines — that invented duration is precisely the CE-timing that DEC-0065 defers to
Phase 2. (A)'s destination-line key avoids inventing timing while still distributing. **Rejected as
a standalone; its raster-order-commit idea is retained inside (A).**

**(C) Hybrid / honest residual.** We adopt (A) as the mechanism AND state honestly (§2.5) that the
last component of frame-to-frame stability is Phase-2-coupled. This is the intellectually honest
disposition the task requires: **(A) removes the intra-frame tear; a bounded residual jitter, if
any survives the A/B, is the deferred CE-pacing.** The chosen Phase-1 boundary is exactly
"commit-per-destination-region-in-raster-order, gated to active display" (§2.4).

### 2.4 Chosen mechanism — precise semantics (the "why it works" contract)

For each **atomic** command (LMMV, LMMM, HMMV, HMMM, YMMM, PSET, LINE), at each
**destination-row boundary** (the point where `dy` advances / the single row for PSET / each `dy`
change for LINE), BEFORE writing that row's bytes:

1. Compute `L = ((dy & 0xFF) − R#23) & 0xFF` (the renderer's exact vertical inverse).
2. **Visible-page gate:** if the destination row's VRAM address is NOT in the currently-displayed
   bitmap page, OR the mode is non-bitmap/text, → **fall back** to today's behavior (the single
   beam-line sync already fired at command issue); do nothing extra. (Covers off-screen pages,
   sprite tables, text modes — no regression.)
3. **Active-display gate:** if the command is executing during **vblank/border**
   (`raster_display_line() < 0`), → **suppress** the per-row commit; today's byte-identical
   whole-next-frame semantics are preserved (this keeps the common "rebuild HUD in the vblank ISR"
   path bit-exact and is the key anti-regression guard).
4. **Wrap guard:** if `L < watermark` (the destination row is above the accumulation point, i.e.
   already committed, or a `&0xFF` wrap made it non-monotonic), → **skip** (the accumulator's
   `sync_to_line` would otherwise trigger its wrap-safety `finalize()` mid-command; we must never
   seal a partial frame mid-command). Already-swept rows appearing next frame is hardware-correct.
5. Otherwise call the accumulator to **commit `[watermark, L)` from the current (pre-this-row) live
   VRAM**, advancing the watermark to `L`. Then the command writes row `dy`.

**Why this produces openMSX's clean per-row result (proof against the evidence):**

- The FINAL VRAM is already clean (triptych top) — so the ONLY thing to fix is *when* rows observe
  the writes.
- Trace a top-to-bottom command writing destination page-rows `P0<P1<…` (display lines
  `L0<L1<…`), executing while the beam is at line `B` in active display:
  - Before writing `P0`: commit `[watermark, L0)` from pre-command VRAM → rows above the command's
    top are frozen with old content (correct — they are not the command's rows).
  - Before writing `P1`: commit `[L0, L1)` — row `L0` now reads live VRAM that INCLUDES `P0`'s
    write → row `L0` shows the command's content. And so on.
  - Result: each destination display row is committed **with the command's own content, in raster
    order**, instead of the whole rectangle frozen at the single beam line `B`. This is precisely
    openMSX's `renderUntil(time)` per write, with the destination line as the `time` proxy.
- The committed content now depends on the **destination geometry** (fixed every frame: the HUD is
  at the same display lines) rather than the **jittery beam phase** — so the frame-to-frame
  patchwork boundary collapses toward a stable, crisp image, matching `omsx/oms_005..011`.
- The `GAME OVER` bleed / half-blitted score digits are exactly "rows committed before the blit
  that fills them"; under the per-row commit those rows are committed *after* their fill in raster
  order, so the bleed disappears.

### 2.5 Honest containment verdict + the Phase-1/2 boundary (R-1, pre-authorized)

Phase 1 removes the **intra-frame patchwork tear** — the dominant, human-reported artifact (the
scrambled band, red bars, `GAME OVER` bleed). The residual, if any survives the `.omr` A/B:

- **Already-swept rows** cannot be un-frozen (step 4) — those command writes appear next frame,
  exactly as on real hardware (you cannot repaint a drawn scanline). Not a defect.
- **Full frame-to-frame determinism** of a blit loop the game paces by busy-waiting on **S#2 CE**
  additionally depends on the **Phase-2 CE-busy-wait duration** (our CE clears instantly, so the
  unpaced loop's beam phase can still drift). If the A/B shows a bounded residual flicker after the
  observation fix, it is this Phase-2 coupling — **explicitly deferred by DEC-0065**, documented,
  and NOT re-opened in M44.

**Boundary statement (authoritative for M44):** *Phase 1 = no intra-frame tear (observation
timing). Phase 2 = CE/S#2 busy-wait duration + exact access-slot cycles (deferred).* The `.omr` A/B
is the acceptance gate for "no tear"; a residual pure-timing jitter is dispositioned to Phase 2, not
treated as an M44 miss.

### 2.6 Wiring contract (chosen: command-engine per-row sink; VdpVram stays a plain store)

Two candidate wirings were considered (DEC-0065 lists both):

- **(ii) VdpVram write-notify observer** (openMSX's literal location: `VDPVRAM::writeCommon`).
  Rejected as primary: `VdpVram`'s header contract is deliberately "a plain store, no notify"
  (vdp_vram.h:38-42, 57-59); `VdpVram::write` is a hot path also driven by CPU `#98` writes (already
  synced by the `io_write` seam) and by event-driven transfers (already per-unit synced), so a
  global observer there would need a scoped gate flag AND add a per-write branch to the CPU path —
  larger blast radius, contract violation.
- **(i) Give the command engine the render-sync listener — CHOSEN.** Add to `VdpCommandEngine` a
  **default-null** per-row sink pointer; the atomic loops invoke it once per destination row (§2.4).
  `V9958Vdp` (which owns the registers and the command engine) implements the sink privately,
  performs the address→display-line mapping + gates, and forwards to the accumulator. This touches
  ONLY `src/devices/video`, leaves the CPU VRAM path and `VdpVram` byte-for-byte untouched, needs no
  hot-path flag, and matches the accumulator's natural per-line commit granularity. It mirrors
  openMSX's *semantics* (notify-before-write, visible-window-gated) while placing the hook where our
  architecture keeps write intent (the command engine), a justified deviation from openMSX's file
  layout.

**Contract shape (interface only — developer finalizes names):**

- `VdpCommandEngine`: new default-null `CommandRowSink* row_sink_` + `set_command_row_sink(...)`.
  The atomic run_* functions call `row_sink_->on_dest_row(dy, row_base_addr)` (or equivalent
  carrying the destination page-row and its base VRAM address) at each destination-row boundary,
  BEFORE that row's writes. **Null sink ⇒ zero behavior change (strict superset).**
- `V9958Vdp`: a private adapter implementing `CommandRowSink`, installed into `cmd_engine_` during
  construction/reset. It computes `L` (§2.4 steps 1-4) from live registers (`control_register(23)`,
  displayed-page base, mode, `raster_display_line()`) and forwards a **commit-to-line** request to
  the render-sync listener.
- `VdpRenderSyncListener` (v9958_vdp.h:55-59): extend with a line-parameterized commit, e.g.
  `virtual void on_commit_up_to(int display_line) {}` (default empty ⇒ existing listeners
  unaffected). The machine's `VdpRenderSyncAdapter` (hbf1xv_machine.cpp:53-102) implements it by
  calling `scanline_accumulator_.sync_to_line(display_line)` — **no `+2` beam margin** (that margin
  is specific to the beam→renderUntil rounding, hbf1xv_machine.cpp:71-101; the destination-line
  mapping is exact). It must honor the existing `suspended_` flag (debug_io_write exclusion) and
  `mark_completed_frame_stale()`.

Default inertness is guaranteed at two layers: the command sink is null unless V9958Vdp installs it,
and `on_commit_up_to` has an empty default — so unit tests constructing a bare `VdpCommandEngine`
or a bare `V9958Vdp` are byte-identical.

### 2.7 Secondary latent gap — sprite attribute / pattern table writes (§ folds in cheaply-or-notes)

openMSX also notifies `spriteAttribTable` / `spritePatternTable`. Our sprite path is per-line live
(`sprite_engine_`), and the accumulator commits rows before each command row-write, so bitmap
coverage is handled. A command byte written to the sprite tables, however, has **no cheap
address→display-line inverse** (a sprite attribute row maps to an arbitrary on-screen Y via its
Y-coordinate byte). Per DEC-0065's "fold in if cheap, else note": **the visible-page gate (§2.4
step 2) sends sprite-table / non-bitmap destinations to the fall-back path (today's single beam-line
sync)** — no regression, and command-driven sprite-table writes are rare (games write sprite tables
via CPU `#98`, already synced). **Recorded as a Phase-1 residual**, not fixed in M44.

---

## 3. Milestone (M44 — Phase 1)

- **Milestone ID:** M44
- **Title:** DEF-M44-CMDSYNC Phase 1 — V9958 command-engine render-observation timing (Laydock 2
  HUD flicker, contained fix)
- **Objective:** Route atomic command-engine bitmap-VRAM writes through the M32 render-sync at
  per-destination-row / display-line granularity so each display row observes the correct
  partial-command state, eliminating the Laydock 2 HUD intra-frame tear to openMSX-crisp parity —
  without the Phase-2 access-slot/CE-duration cycle model.
- **Included Scope:** §1.1; the wiring contract §2.6; the mechanism §2.4; the secondary sprite gap
  handled by fall-back §2.7. Files in §5.
- **Excluded Scope:** §1.2 (all Phase-2 cycle/CE-duration work); any `src/core` or `src/devices/cpu`
  change; any frontend behavior change.
- **Dependencies:** M32 scanline accumulator + `VdpRenderSyncListener` seam (present); M22 command
  engine (present); M38/M41 V9958 scroll/multi-page + screen-mode A/B harness (`tools/m41-run.ps1`,
  `tools/m41-video-ab.py`, `tools/m41-scenarios.json`); `tools/omr-to-input-script.py` +
  `debug/laydock2-flicker/` reuse; WSL `/usr/bin/openmsx`.
- **Interfaces Affected:** `VdpCommandEngine` (new default-null row sink; atomic loops call it);
  `V9958Vdp` (private `CommandRowSink` adapter + display-line mapping; installs sink);
  `VdpRenderSyncListener` (new default-empty `on_commit_up_to(int)`); `Hbf1xvMachine::
  VdpRenderSyncAdapter` (implements it → `scanline_accumulator_.sync_to_line`). No public
  machine/frontend API change.
- **Acceptance Criteria:** §4.
- **Unit Test Plan:** §4.4 (new per-row observation-ordering test + M22/M32 oracles unchanged).
- **System Integration Test Plan:** §4.4 (accumulator/machine integration; Laydock `.omr` A/B).
- **Regression Impact:** §6 matrix. High blast radius (every command-drawn screen) ⇒ full
  screen-mode + command A/B mandatory.
- **Exit Criteria:** all §4 gates green; QA sign-off; owner release decision + tag v1.1.1.
- **Status:** Planned.

### 3.1 Recommended slice plan (developer)

- **S1 — Wiring seam (default-inert).** Add `VdpCommandEngine` null row sink + setter;
  `VdpRenderSyncListener::on_commit_up_to(int)` default-empty; the machine adapter implementation
  (calls `sync_to_line`). No atomic-loop calls yet. Gate: byte-identical everywhere (A-4), M32
  zero-listener oracle green. **This slice alone must change nothing.**
- **S2 — Mapping + gates in V9958Vdp.** Private `CommandRowSink` adapter: display-line inverse
  (§2.4 step 1), visible-page gate (step 2), active-display gate (step 3), wrap guard (step 4).
  Install the sink in construction/reset. Unit-test the mapping in isolation (§4.4).
- **S3 — Atomic-loop hooks.** Insert the per-destination-row `row_sink_->on_dest_row(...)` call in
  `run_lmmv`/`run_lmmm`/`run_hmmv`/`run_hmmm`/`run_ymmm`/`run_pset`/`run_line` at the row boundary,
  BEFORE the row's writes. Confirm `run_point`/`run_srch` (no bitmap writes) and the event-driven
  transfers (already synced) are untouched.
- **S4 — Determinism test + evidence.** New per-row observation-ordering integration test with
  proven non-tautology (§4.4). Run all evidence gates (§4.1). Capture the Laydock `.omr` A/B and the
  M41 screen-mode A/B sweep. Write `docs/m44-implementation-report.md` + A/B artifacts.

---

## 4. Acceptance Criteria

### 4.1 Evidence gates (run and capture; never fabricate)

- `tools/validate-assets.ps1` — BIOS present + ≥1 ROM. **PASS.**
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` — refreshed.
- `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=ON && cmake --build build --config Debug` — clean
  rebuild, both executables (`sony_msx_headless.exe`, `sony_msx_sdl3.exe`).
- `ctest --test-dir build -C Debug --output-on-failure` — deterministic suite GREEN (fast subset;
  new count = prior + new test(s), no drop).
- **openMSX A/B REQUIRED (behavior-affecting)** — §4.2 + §4.3.
- **ZEXALL/ZEXDOC:** WITHHELD-substituted — confirm `git diff v1.1.0..HEAD -- src/core
  src/devices/cpu` is EMPTY (mechanical per-edit trigger does not fire). Record the empty diff.

### 4.2 Behavior parity — Laydock 2 HUD (the headline fix)

- **AC-1:** The Laydock 2 HUD renders **crisp and frame-to-frame stable**, matching openMSX, via the
  reused `debug/laydock2-flicker/` `.omr` A/B: OURS (post-fix) vs `omsx/oms_0xx` show no red bars,
  no blue-noise score digits, no `GAME OVER` bleed, and no per-few-frame boundary jitter (contrast
  the pre-fix `hud_montage.png`). Capture the post-fix montage + a triptych update.
- **AC-1-residual (R-1):** If a *bounded* residual jitter remains, it is dispositioned in the report
  as the **Phase-2 CE-pacing coupling** (§2.5) with evidence, NOT rebaselined away and NOT treated
  as an M44 failure. QA judges whether the intra-frame tear is gone (the M44 bar) independently of
  any pure-timing residual.

### 4.3 No-regression parity — every other command-drawn screen (high blast radius)

- **AC-2:** ALL M41 screen-mode A/Bs remain **0.000%** vs openMSX
  (`tools/m41-run.ps1` / `tools/m41-video-ab.py` over `tools/m41-scenarios.json`; the 13 modes).
- **AC-3:** The M22 **sprite + command-engine SYSTEM** parity oracle
  (`tests/system/hbf1xv_m22_sprites_command_engine_system_test.cpp`) and the M22 command-engine
  integration + unit oracles (`tests/integration/machine/hbf1xv_m22_command_engine_integration_test.cpp`;
  `tests/unit/devices/video_vdp_command_engine_{atomic,logic_ops,nonbitmap,pending_col,registers,
  transfer}_unit_test.cpp`) stay **GREEN unmodified** — the fix changes observation timing only, not
  VRAM results, so command *content* oracles are untouched. If any must be re-derived, it requires a
  written justification proving no weakening (no coverage removed, assertions equal-or-stronger).
- **AC-4:** The M32 oracles stay green: the **zero-listener byte-identity** unit oracle
  (default-inert proof), `tests/system/hbf1xv_m32_split_screen_system_test.cpp`,
  `tests/integration/machine/hbf1xv_m32_{line_interrupt,per_line_latch}_integration_test.cpp`.
- **AC-5:** Aleste 2 mid-frame split (M32/M38 live scene) unchanged or improved — spot A/B
  (`debug/frames/`), no new tear.

### 4.4 New deterministic test — per-row observation ordering (non-tautology REQUIRED)

- **AC-6:** A new deterministic unit/integration test constructs GRAPHIC4 with a known `R#23`,
  positions the raster mid-active-display, and issues an atomic command (e.g. LMMV/HMMV) whose
  destination page-rows straddle the beam. It asserts the accumulator's committed rows reflect the
  **per-row raster-order distribution**: rows committed before their destination-line show
  pre-command content; each destination display row shows the command's content; ordering matches
  `L = ((dy&0xFF) − R#23) & 0xFF`.
- **AC-6 non-tautology:** an adversarial arm mutates the mapping to the OLD single-beam-line commit
  (all rows keyed to the beam) and asserts the test **FAILS** (reproduces the patchwork). A second
  arm proves the vblank/active-display gate (a command during `raster < 0` commits nothing → whole
  frame post-command, byte-identical to today). The test must FAIL if the mapping, either gate, or
  the wrap guard is removed.
- **AC-7:** Default-inertness proof — with no row sink installed, the exact pre-fix committed frame
  is reproduced byte-for-byte (guards A-4).

---

## 5. File and Blast-Radius Map

All edits confined to `src/devices/video` (+ the machine adapter method). **NO `src/core`, NO
`src/devices/cpu`, NO `src/peripherals`, NO frontend behavior change.**

| File | Change | Risk |
| --- | --- | --- |
| `src/devices/video/vdp_command_engine.h` | Add default-null `CommandRowSink* row_sink_` + `set_command_row_sink()`; declare the small `CommandRowSink` interface (or reuse a lightweight callback). | Low — additive; null default inert. |
| `src/devices/video/vdp_command_engine.cpp` | Insert one per-destination-row `row_sink_` call in the 7 atomic writers (`run_lmmv/lmmm/hmmv/hmmm/ymmm/pset/line`) at the existing row-advance point, BEFORE the row's writes. `run_point/run_srch` + event-driven transfers untouched. | **Medium — the behavioral core.** Hot loops; must not change VRAM results or write order. |
| `src/devices/video/v9958_vdp.h` | Extend `VdpRenderSyncListener` with default-empty `on_commit_up_to(int)`; declare the private `CommandRowSink` adapter + display-line mapping helper. | Low — additive; default-empty method keeps existing listeners inert. |
| `src/devices/video/v9958_vdp.cpp` | Implement the adapter: display-line inverse + visible-page gate + active-display gate (`raster_display_line()`) + wrap guard; install the sink in ctor/reset; forward to `render_sync_->on_commit_up_to(L)`. | Medium — the mapping/gates; unit-tested in isolation (AC-6). |
| `src/machine/hbf1xv_machine.cpp` (+ `.h` decl) | `VdpRenderSyncAdapter::on_commit_up_to(int)` → `scanline_accumulator_.sync_to_line(line)` (no `+2` margin), honoring `suspended_` + `mark_completed_frame_stale()`. | Low — mirrors the existing `on_before_state_change` adapter. |
| `src/devices/video/vdp_vram.{h,cpp}` | **UNCHANGED** (stays a plain store — wiring chosen to avoid touching it, §2.6). | None. |
| `src/devices/video/vdp_scanline_accumulator.{h,cpp}` | **UNCHANGED** — reuses `sync_to_line` as-is (its idempotency, wrap-safety, live `render_line` are exactly what the mechanism needs). | None. |
| `src/devices/video/vdp_frame_renderer.*` | **UNCHANGED** — `vertical_scroll_wrap` is only *read from* (its inverse is computed in the adapter). | None. |
| `tests/...` (new) | Per-row observation-ordering test (AC-6/6-nt/7); optionally extend an M22 integration test. | Additive. |

Confirm at every gate: `git diff v1.1.0..HEAD --stat` shows only `src/devices/video`, the one
`src/machine` adapter method, and `tests/` — nothing under `src/core` or `src/devices/cpu`.

---

## 6. Regression Matrix

| Area | Oracle / evidence | Expected | Why at risk |
| --- | --- | --- | --- |
| Default inertness | M32 zero-listener byte-identity unit oracle; AC-7 | Byte-identical | New seam must be no-op when uninstalled |
| Command VRAM results | M22 atomic/logic/nonbitmap/pending_col/registers/transfer unit tests | Green unmodified | Loops edited; results must not change |
| Command + sprite system parity | `hbf1xv_m22_sprites_command_engine_system_test` | Green unmodified | Shared command path |
| Split-screen / line-int | `hbf1xv_m32_split_screen_system_test`; `m32_line_interrupt`, `m32_per_line_latch` integration | Green | Shares render-sync seam |
| Screen modes (13) | M41 A/B (`tools/m41-run.ps1` + `m41-video-ab.py`) | **0.000%** | Any command-mid-active-display screen |
| Laydock 2 HUD | `.omr` A/B, `debug/laydock2-flicker/` | Crisp/stable = openMSX (AC-1); residual→Phase 2 (AC-1-r) | The target defect |
| Aleste 2 split | `debug/frames/` spot A/B (AC-5) | Unchanged/improved | Mid-frame split scene |
| CPU/core | `git diff -- src/core src/devices/cpu` | EMPTY → ZEXALL withheld | Confirms no CPU touch |
| Both executables | headless + SDL3 fast ctest | Green, count = prior + new | Build/link + SDL3 path |

---

## 7. Risks

- **R-1 (Central, pre-authorized — §2.5):** Observation timing may not fully stabilize
  frame-to-frame flicker because the flicker's secondary driver (unpaced blit loop → beam-phase
  drift) is the Phase-2 CE-busy-wait coupling. **Mitigation:** the `.omr` A/B is the acceptance gate
  for "no intra-frame tear"; any bounded residual jitter is dispositioned to Phase 2 (deferred by
  DEC-0065), documented, not rebaselined. QA judges the tear (M44 bar) separately from pure timing.
- **R-2 (Regression, high blast radius):** the atomic-loop hooks run for EVERY command-drawn screen.
  **Mitigation:** default-inert layering (AC-7), the four gates (§2.4 steps 2-4) that fall back to
  today's behavior for non-bitmap/off-page/vblank/wrap, and the mandatory M41 0.000% + M22/M32
  oracle sweep (AC-2/3/4).
- **R-3 (Mid-command finalize):** a naive `sync_to_line(L)` with `L < watermark` would trip the
  accumulator's wrap-safety `finalize()` and seal a partial frame mid-command. **Mitigation:** the
  explicit wrap guard (§2.4 step 4) — skip when `L < watermark`.
- **R-4 (Vblank premature commit):** committing at destination lines during vblank would freeze
  active rows before later vblank commands finish → a NEW regression on the common vblank-rebuild
  path. **Mitigation:** the active-display gate (§2.4 step 3) suppresses per-row commit when
  `raster_display_line() < 0`, preserving today's byte-identical whole-next-frame semantics.
- **R-5 (LINE per-pixel `dy`):** `run_line` changes `dy` per pixel; a per-`dy`-change hook could
  over-notify. **Mitigation:** hook only when `dy` actually changes (`wrap_step(dy, ty)` sites);
  `sync_to_line` is idempotent so redundant equal-line calls are cheap no-ops. Optional (matches
  openMSX `VDPVRAM.hh:585`): skip the notify when the write does not change the byte.
- **R-6 (Multi-page vertical / interlace / GRAPHIC6-7 edge geometry):** the `&0xFF` inverse assumes
  a 256-row page. **Mitigation:** cover GRAPHIC4/5 precisely (Laydock is G4); G6/G7 use the same
  page-row inverse; anything the gate cannot map cheaply falls back (§2.4 step 2) — no regression.
- **R-7 (License isolation):** openMSX `VDPVRAM`/`VDPCmdEngine` are behavior references only.
  **Mitigation:** cite `references/openmsx-21.0/...` for *semantics*; never copy code or its data
  tables (`feedback_license_sensitive_scope`). This fix is our own render-sync plumbing, not an
  openMSX transcription.

---

## 8. Developer Handoff

- **Do:** implement S1→S4 (§3.1) in `src/devices/video` (+ the one machine adapter method), keeping
  the seam **default-inert** (null sink + empty `on_commit_up_to`). Mechanism = §2.4; wiring = §2.6.
- **Central invariant:** the fix changes only *when display rows observe* command writes — never
  *what* bytes are written, nor the write order. All M22 content oracles must pass unmodified.
- **Grounding to cite (read, never copy):** `references/openmsx-21.0/src/video/VDPVRAM.hh:309-322,
  428, 575-593` (notify-before-commit, visible-window gate); `VDPCmdEngine.cc` (command structure).
  Our inverse geometry: `src/devices/video/vdp_frame_renderer.cpp:170-171`.
- **Gates:** §4.1 evidence gates; the Laydock `.omr` A/B (AC-1, reuse `debug/laydock2-flicker/`);
  the M41 screen-mode A/B sweep (AC-2); M22/M32 oracles (AC-3/4); the new per-row test with
  non-tautology + inertness (AC-6/6-nt/7). Confirm the empty `src/core|cpu` diff.
- **Report:** `docs/m44-implementation-report.md` + A/B artifacts under `docs/` (and refreshed
  montage/triptych under `debug/laydock2-flicker/`). Honestly disposition any R-1 residual to
  Phase 2; do NOT expand into Phase-2 CE-duration work (DEC-0065 boundary).
- **Open questions for a decision entry (if hit):** (Q1) if the `.omr` A/B shows the intra-frame
  tear is NOT fully removed by observation timing alone — i.e., Phase 1 cannot meet AC-1 without
  some CE-duration — escalate to the coordinator for a DEC to either accept the residual as the
  documented Phase-1/2 boundary or pull the minimal CE-duration slice forward. (Q2) if any M22/M32
  oracle genuinely must change, it needs a decisions.md entry proving no weakening before edit.
