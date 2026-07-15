# M49 Planner Package — Per-Line LIVE Sprite Attribute/Register Fetch (backlog D9)

- Milestone ID: M49
- Title: Per-line LIVE sprite attribute/register fetch (progressive `check_until` sprite engine) — closes backlog D9
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent (handoff below)
- QA Owner: MSX QA Agent
- Opened by: DEC-0076 (2026-07-14), PLANNING phase, planner-first
- Reference precedence (DEC-0073): tier-1 real-hardware spec > tier-2 openMSX + spec-disciplined reasoning > tier-3 fMSX. On a confirmed conflict the hardware spec wins and the openMSX divergence is documented.

---

## 1. Scope and Assumptions

### 1.1 Objective

Convert the V9958 sprite subsystem from its current **frame-atomic** model (one end-of-frame
snapshot of the whole register file + all VRAM) to a **per-line LIVE** model that computes each
output line's sprite visibility / 5th-(9th-)sprite / position by reading the control registers and
VRAM **at that line's raster point**, exactly as the background renderer already does. This closes
the ONE architectural defect catalogued as backlog **D9** and removes the top-region sprite culling,
mis-positioning, and 1-frame flicker that mid-frame **cycle-timed** VDP reprogramming (fixed HUD over
a scrolling playfield) currently causes on Space Manbow and Laydock 2.

### 1.2 Confirmed root cause (ground truth — from the completed cross-validated investigation)

- `SpriteEngine::recompute_frame()` (`src/devices/video/sprite_engine.cpp:74-301`) is called ONLY
  from `V9958Vdp::on_vsync()` (`src/devices/video/v9958_vdp.cpp:685-705`). It snapshots the whole
  `control_regs` file + all VRAM at end-of-frame and computes every output line's visibility /
  5th-sprite / position in one pass.
- Registers baked frame-final in that pass: **R#23** vertical scroll (`sprite_engine.cpp:138`,
  applied in the Y formula `:183`); **R#8** SPD-enable + TP (`:103-105`, `:247`); **R#1** BL / SI /
  MAG (`:102`, `:108-110`); **R#5/R#11** attribute-table base (`:116-117`, mask `:132`); **R#6**
  pattern base (`:136`).
- The BACKGROUND is already per-line-LIVE: `VdpFrameRenderer::render_line()` reads registers live,
  and the M32 render-sync seam `Hbf1xvMachine::VdpRenderSyncAdapter::on_before_state_change()`
  (`src/machine/hbf1xv_machine.cpp:53-124`) commits background lines up to the beam with a **+2-line
  margin** (`:101`) BEFORE each mid-frame VDP write applies.
- Space Manbow + Laydock 2 do CYCLE-TIMED mid-frame VDP reprogramming (**NOT** line interrupts:
  R#0 IE1=0, R#19=0xDB) to split a fixed HUD over a scrolling playfield. The end-of-frame sprite
  snapshot captures the bottom/playfield state, so top-region sprites are culled / mis-positioned and
  flicker (the snapshot racing the cycle-timed writes + a 1-frame lag).

### 1.3 Tier-1 grounding for the fix direction

- `references/fact-sheets/Yamaha V9958 VDP.md` §6 line 120: **"sprite data for line N is fetched
  during line N−1"** — the sprite check is PER-SCANLINE, not per-frame. (Also §6 line 120: the
  1-pixel vertical shift, sprite at Y=0 appears on the 2nd displayed line.)
- §7 line 126 + §10 line 178: R#23 vertical scroll is a live per-line display offset; games (Space
  Manbow, F1 Spirit 3D) rely on exact per-line scroll behavior.
- §6 line 117-118: mode-2 max-8-per-line, per-line colour/CC/IC/EC via the colour table, the bit-9
  attribute-table AND-mask, and the 5S / C semantics — all PRESERVED byte-for-byte.
- Register map §5 lines 71-77: R#1 BL/IE0/M1/M2/SI/MAG, R#8 TP/SPD, R#5+R#11 sprite attribute table
  base, R#6 sprite pattern generator, R#23 vertical scroll (display offset).

### 1.4 Tier-2 corroboration (openMSX — read for EFFECT only, NEVER copy code or tables)

openMSX `SpriteChecker::sync()/checkUntil()` (`references/openmsx-21.0/src/video/SpriteChecker.{hh,cc}`)
checks sprites incrementally up to the current raster; each sprite-register write in `VDP.cc`
(`updateVerticalScroll` / `updateSpritesEnabled` / `updateSpriteSizeMag` / `updateTransparency` +
the attribute/pattern base updaters) syncs sprites to the current raster with the OLD value THEN
applies the new value. This is the SAME sync-before-change protocol our BACKGROUND already has
(M32) which our SPRITES lack. **This is an ALGORITHM change (WHEN/with-WHAT-state), not a data
table** — the D4/C1 license-sensitive-table ban does NOT apply here, but the standing "never copy
openMSX code" rule (guardrails, `feedback_license_sensitive_scope.md`) DOES: read the incremental
`checkUntil` model for understanding only; do not transcribe `SpriteChecker`'s code.

### 1.5 Assumptions (each carries a verification action)

- **A-M49-1 (verify in dev):** With NO mid-active-display sprite-relevant write in a frame (the
  common case — all sprite setup happens in vblank), the progressive per-line pass MUST produce
  byte-identical per-line visible-sprite buffers, S#0 (5S/C/number), S#3-S#6 collision X/Y, and the
  collision-event queue vs today's frame-atomic `recompute_frame()`. Verify: the entire existing
  M22/M32/sprite unit+integration+system suite stays green unmodified (§4 EG-2), plus a dedicated
  no-mid-write byte-identity oracle.
- **A-M49-2 (verify in dev):** The buffer HEIGHT (192 vs 212, from R#9 bit7) remains a
  frame-boundary decision taken at `on_vsync()` as today (`v9958_vdp.cpp:691-703`); only the
  per-line register/VRAM READS become progressive. Mid-frame R#9 LN changes are out of scope
  (exotic; no known title). Verify: height-selection oracle unchanged.
- **A-M49-3 (verify via A/B):** R#23 stays in the sprite Y formula (`sprite_line =
  ((line-1) + r23 - y) & 0xFF`), now read per-line-LIVE, NOT removed. Confirm scroll DIRECTION and
  the split line against openMSX A/B on Space Manbow (§4 EG-4). The fix is per-line-live R#23, not
  the removal of R#23.
- **A-M49-4 (assumption, documented):** The line-granular quantization of a mid-frame split matches
  the BACKGROUND's existing +2-line rounding (M39/M44); intra-line cycle-timed writes remain
  line-quantized (not cycle-exact). This is the human-accepted M44 Phase 2b boundary (residual §5).
- **A-M49-5 (open sub-question — MUST be resolved in dev):** Whether Laydock 2's scoreboard "noise"
  is PURELY the sprite defect or ALSO has a BACKGROUND facet. Requires a Laydock 2 headless repro
  during development (§6 Task 0 / §4 EG-4). Do not assume sprites are the whole story until the
  before/after frame captures isolate the scoreboard region.

### 1.6 Non-goals (out of scope — no scope expansion without a new DEC)

- NO cycle-exact intra-line sprite timing (that needs the banned VDP access-slot position table —
  C1/D4 UNBUILDABLE-WITHOUT-FABRICATION; unchanged).
- NO change to the DEC-0031 collision / 5th-sprite / status machinery semantics (byte-identical; §3 S3).
- NO change to sprite PIXEL compositing (`VdpFrameRenderer::composite_sprites()` still consumes the
  same `visible_sprites(line)` read-only API).
- NO `src/core/` or `src/devices/cpu/` edits expected. NO command-engine TIMING change (that is M48).
- NO removal of R#23 from the sprite Y offset.

---

## 2. Spec Summary

### 2.1 Behavior contract (target)

For each active-display output line L in `[0, height)`, the sprite subsystem computes that line's
visible-sprite list, position, 5th/9th-sprite detection, and collision events using the values of
R#1 (BL/SI/MAG), R#5/R#11 (attribute base), R#6 (pattern base), R#8 (SPD/TP), R#23 (vertical
scroll), and the sprite attribute/colour/pattern VRAM **as they stand at the raster point of line L**
— NOT as they stand at the end of the frame. A mid-frame write to any of those registers (or to
sprite-attribute-table VRAM) takes effect from the SAME commit boundary the background uses (L+2),
so the sprite split and the background split land on the identical line.

### 2.2 Dependency map (src boundaries touched)

| Layer | File(s) | Role in M49 |
|---|---|---|
| `src/devices/video` | `sprite_engine.{h,cpp}` | **PRIMARY.** New stateful `check_until(line)` watermark pass replacing the single `recompute_frame()` sweep; preserves the per-line formulas + mode-2 base-mask addressing verbatim. |
| `src/devices/video` | `v9958_vdp.{h,cpp}` | Drive `check_until(beam)` from the register/VRAM write path (funnel already exists — `change_register()` + the `#98/#99` VRAM path); finalize + roll the watermark at `on_vsync()`; wire the sprite sync into the existing render-sync seam signalling. |
| `src/machine` | `hbf1xv_machine.cpp` | Extend the M32 seam `on_before_state_change()` (`:53-124`) so a sprite-relevant write ALSO drives the sprite `check_until(beam)` with the SAME +2 boundary (`:101`) as the background. Reuse the M44 watermark-direction logic (`:118-123`). |
| `src/devices/video` | `vdp_scanline_accumulator.h` | REFERENCE ONLY — the sprite watermark mirrors its `watermark()` / `sync_to_line()` / wrap-safety model (`:61-145`); no edit expected. |
| `src/frontend` | `vdp_frame_renderer.*` | UNCHANGED — still consumes `visible_sprites(line)` read-only. |

No `src/core`, no `src/devices/cpu`, no `src/peripherals` edits expected.

### 2.3 Interaction seams to gate (HIGH blast radius — all sprite rendering, all 13 screen modes)

1. **M32 render-sync +2 margin** (`hbf1xv_machine.cpp:101`) — the background AND sprite splits MUST
   coincide on the identical line. The sprite `check_until` boundary MUST use the same `target =
   line + 2`.
2. **M44 command-engine row sink** (`hbf1xv_machine.cpp:126-146`, `command_row_sync()` in
   `v9958_vdp.cpp`) — this sink can advance the accumulator watermark PAST the beam within a frame.
   The sprite watermark MUST use the SAME advance-only / wrap-direction discipline
   (`hbf1xv_machine.cpp:118-123`) so a command-row commit never drives a sprite `check_until` below
   its own watermark and seals a partial-frame sprite pass mid-command.
3. **Determinism** — identical inputs → identical per-line sprite buffers + status across runs; the
   progressive pass must be watermark-monotonic and reset cleanly on `reset()`/frame boundary.
4. **DEC-0031 collision byte-identity** — the line-granular collision / 5th-sprite / status machinery
   (`sprite_engine.cpp:238-325`) stays byte-identical (D9 requires it); it is fed from the
   progressive pass instead of the single sweep.
5. **M48 coexistence (note):** M48's command-engine TIMING overlay (`effective_slots_per_line()`,
   `arm_command_busy_window()`, `steal_command_slot_for_cpu_vram_access()` — already referenced in
   `v9958_vdp.h:284-315`) reads live R#1 BL + raster + R#8 SPD but is a STATUS/timing overlay; it
   does NOT compute sprite visibility. M49 changes WHEN sprite visibility is computed, not command
   timing. They are orthogonal but co-resident in `v9958_vdp.*`; the developer must keep both paths
   independent and re-run the M48 command-timing oracles to confirm no cross-perturbation.

### 2.4 A documented tier-1 vs current-behavior divergence to PRESERVE (do not "fix")

Tier-1 §6 line 117 says collision is "checked once/frame". Our code (DEC-0031, the boot-logo fix)
uses openMSX's progressive line-granular collision re-latch (`sync_collision_to_raster()`,
`sprite_engine.cpp:309-325`) because the HB-F1XV BIOS boot-logo wobble depends on it. This
divergence is ALREADY resolved in favour of the boot-logo fix and is OUT OF SCOPE for M49 — M49 MUST
keep the DEC-0031 collision behavior byte-identical (§3 S3, §4 EG-6). Do not "correct" it toward the
once/frame reading.

---

## 3. Milestones (slice breakdown)

M49 is delivered as four dependency-ordered slices. Each slice is independently buildable and leaves
the suite green; the fix is not "live" until S2 wires the seam.

### Slice S1 — stateful `check_until(line)` watermark pass (the engine)

- **What:** Give `SpriteEngine` a stateful, watermark-driven `check_until(line)` (mirroring
  `VdpScanlineAccumulator`'s `watermark()`/`sync_to_line()` model, `vdp_scanline_accumulator.h:61-145`)
  that populates the per-line visible-sprite buffer + 5th/9th-sprite reading `control_regs` + VRAM AT
  that raster point, incrementally advancing a watermark — REPLACING the single all-lines
  `recompute_frame()` sweep. The per-line register/VRAM reads become progressive; the per-line
  display-enable (R#1 BL) / sprite-enable (R#8 SPD) / sprite-mode-0 gates are evaluated PER-LINE-LIVE
  (this is the actual fix — a mid-frame R#1/R#8 toggle now affects only the lines after its commit
  boundary, not the whole frame).
- **Preserve verbatim (only WHEN/with-WHAT-state changes):** the per-line Y/X/pattern formulas
  (`sprite_engine.cpp:179-221`), the 1-pixel vertical shift `(line-1)` (`:183`), MAG/size16 handling,
  the mode-2 base-mask addressing `mode2_attr_addr` + the +512 sub-table + per-line colour offset
  (`:116-135`, `:160-215`), the planar (G6/G7) interleave (`:32-39`), the EC/CC/IC bit semantics, and
  the `frameStart()` unconditional per-line clear (`:78`).
- **Tier-1 grounding:** §6 line 120 (line-N fetched during line N−1); §6 line 118 (mode-2 attr mask);
  §5 register map (R#1/R#5/R#6/R#8/R#11/R#23 roles).
- **State:** a `watermark_` (first line not yet checked), plus the frame-boundary
  height/mode/reset lifecycle; `reset()` clears it.

### Slice S2 — drive `check_until` from the EXISTING M32 seam (the wiring — the fix goes live here)

- **What:** In `Hbf1xvMachine::VdpRenderSyncAdapter::on_before_state_change()`
  (`hbf1xv_machine.cpp:53-124`), when the write targets a **sprite-relevant register**
  (R#1, R#5, R#6, R#8, R#11, R#23) OR **sprite-attribute-table VRAM**, ALSO call the sprite engine's
  `check_until(target)` with the SAME `target = line + 2` boundary (`:101`) the background uses, so
  the sprite split lands on the identical line as the background split. `on_vsync()` finalizes the
  sprite pass to `height` and rolls the watermark for the next frame.
- **Watermark direction discipline:** reuse the M44 advance-only / wrap logic
  (`hbf1xv_machine.cpp:118-123`) for the sprite watermark — a genuine frame wrap (`target <
  last_beam_commit_target_`) finalizes; otherwise advance-only. This keeps a command-row commit
  (M44 sink) from sealing a partial sprite pass mid-command.
- **Sprite-relevant-write classification:** the seam already knows the register index via the
  `change_register()` funnel (`v9958_vdp.h:84-89` observer) and the VRAM write path. Sprite-attr VRAM
  detection must use the LIVE attribute-table base (R#5/R#11) at the write's raster point. If precise
  address classification is over-costly, the conservative fallback is "any VRAM write in active
  display drives the sprite `check_until(beam)`" — correct (never stale), at most a small extra
  incremental sweep; the developer picks the cheapest correct option and documents it.
- **Tier-1 / tier-2 grounding:** §6 line 120 + openMSX sync-before-change (`VDP.cc`
  update*/base-updater → `SpriteChecker::sync`), the same protocol our background M32 seam already
  implements.

### Slice S3 — feed DEC-0031 collision / 5th-sprite / status from the progressive pass (byte-identical)

- **What:** The line-granular collision-event collection, 5th/9th-sprite S#0 latch, and S#3-S#6
  coordinate machinery (`sprite_engine.cpp:238-325`) MUST stay byte-identical (D9 requires it). Feed
  it from the progressive per-line pass instead of the single post-sweep loop. The S#0-read re-latch
  (`sync_collision_to_raster` / `consume_collision_events_up_to`) and the frame-boundary latch
  contract are unchanged.
- **Invariant:** for a frame with no mid-active-display sprite-relevant write, the collision-event
  queue, the S#0 5S/C/number, and S#3-S#6 X/Y are byte-identical to today.
- **Tier-1 grounding:** §6 line 117-118 (5S/C semantics, mode-2 collision exclusion CC/IC).

### Slice S4 — R#23 stays in the Y formula, now per-line-LIVE; confirm direction via openMSX A/B

- **What:** Keep R#23 in `sprite_line = ((line-1) + r23 - y) & 0xFF`, reading r23 per-line-LIVE
  from the progressive segment's register state (NOT the frame-final `control_regs[23]`). Confirm the
  scroll DIRECTION and that the sprite split coincides with the background split, via a Space Manbow
  A/B against openMSX.
- **Tier-1 grounding:** §7 line 126 / §10 line 178 (R#23 live per-line offset; scroll-reliant titles).

**Dependency sequencing:** S1 (engine, no behavior change until wired) → S2 (wiring, fix goes live)
→ S3 (collision byte-identity, verified against DEC-0031 tests) → S4 (R#23 direction A/B). S3 and S4
verification can run concurrently once S2 lands. Task 0 (Laydock 2 repro, §6) runs FIRST to fix the
before/after capture baseline and answer A-M49-5.

---

## 4. Acceptance Criteria

### Per-slice acceptance

- **AC-S1:** `SpriteEngine::check_until(line)` exists and, driven line-by-line to `height`, produces
  byte-identical per-line visible-sprite buffers + status to today's `recompute_frame()` for a frame
  with NO mid-active-display sprite-relevant write (new no-mid-write byte-identity unit oracle). The
  per-line R#1 BL / R#8 SPD / sprite-mode-0 gates are evaluated per-line-live (new unit oracle: a
  mid-frame BL/SPD toggle changes only lines after the commit boundary). Formulas + mode-2
  base-mask + planar addressing unchanged (existing `video_sprite_engine_mode1/mode2/mode2_attribute_masking`
  unit tests green UNMODIFIED).
- **AC-S2:** With the seam wired, a synthetic mid-frame R#23 (and R#1 BL) rewrite at display line L
  moves the sprite visibility split to line L+2 — the SAME line as the background split (new
  split-coincidence system oracle; the sprite split boundary equals the background split boundary
  from `hbf1xv_machine.cpp:101`). The M44 watermark-direction discipline holds (a command-row commit
  does not seal a partial sprite pass — existing M44 oracles green).
- **AC-S3:** DEC-0031 collision / 5th-sprite / S#0 / S#3-S#6 machinery byte-identical: the boot-logo
  collision re-latch behavior and `video_sprite_engine_collision_relatch_unit_test.cpp` green
  UNMODIFIED; a new adversarial oracle proves the progressive pass feeds the SAME collision events as
  the frame-atomic pass for a no-mid-write frame.
- **AC-S4:** R#23 remains in the Y formula, per-line-live; the Space Manbow A/B (EG-4) confirms
  scroll direction and split coincidence.

### Evidence gates (run and capture — never fabricate)

- **EG-1 (build):** `cmake --build build --config Debug` succeeds for BOTH executables (SDL3=ON
  superset config per CLAUDE.md).
- **EG-2 (full ctest — MANDATORY):** `ctest --test-dir build -C Debug --output-on-failure` fully
  green, including EVERY existing sprite/M22/M32 unit+integration+system test UNMODIFIED
  (`video_sprite_engine_mode1/mode2/mode2_attribute_masking/collision_relatch`,
  `video_vdp_frame_renderer_sprites`, `video_vdp_frame_renderer_sprite_edge_clip`,
  `hbf1xv_m22_sprites_integration`, `hbf1xv_m22_sprites_command_engine_system`,
  `hbf1xv_m32_split_screen_system`, `hbf1xv_m32_line_interrupt_integration`,
  `hbf1xv_m32_per_line_latch_integration`) PLUS the new M49 oracles. No existing oracle weakened
  (non-tautology: adversarial mutation of the progressive pass must FAIL at least one oracle).
- **EG-3 (FULL 13-mode screen-mode A/B vs openMSX — MANDATORY, given the HIGH blast radius):** all 13
  M41 screen modes must remain **0.000%** vs openMSX (the standing `tools/`-driven screen A/B, the
  M41 baseline). Sprites touch all 13 modes; this gate is non-negotiable for M49 closure.
- **EG-4 (title before/after frame-capture verification — MANDATORY):** headless deterministic
  before/after frame captures for **Space Manbow** AND **Laydock 2** proving the top-region sprite
  culling / mis-position / flicker is resolved after the fix (and regressed by an adversarial revert).
  For Laydock 2 the capture MUST isolate the scoreboard region to answer A-M49-5 (sprite-only vs also
  background). Captures land under `debug/` (new `debug/m49/` subfolder as needed). Grounded openMSX
  A/B for Space Manbow (R#23 direction + split line, AC-S4).
- **EG-5 (DEC-0031 collision-byte-identity gate — MANDATORY):** an explicit gate proving the
  collision / 5th-sprite / S#0 / S#3-S#6 output is byte-identical to the pre-M49 build for the
  boot-logo path and the collision-relatch oracle (the D9 hard requirement).
- **EG-6 (determinism):** two identical headless runs of the Space Manbow + Laydock 2 replay produce
  byte-identical per-frame sprite output (frame-hash equality).
- **EG-7 (asset + checksum health):** `tools/validate-assets.ps1` PASS; `tools/checksum-assets.ps1
  -OutFile docs/asset-checksums.txt` refreshed.
- **EG-8 (ZEXALL cadence — NOT required):** per `feedback_slow_test_cadence.md`, M49 touches NO
  `src/devices/cpu/` or `src/core/` code (git-diff expected EMPTY for those trees); the full ZEXALL
  sweep is NOT triggered. QA confirms the empty cpu/core diff as the substitution rationale.
- **EG-9 (ban attestation):** QA greps `src/` to confirm no `SpriteChecker` code/table transcription
  (algorithm-only change; the standing "never copy openMSX code" rule — the D4/C1 DATA-TABLE ban does
  not apply since this is not a data table, but the code-copy ban does).

**Closure gate:** NORMAL human-release-decision gate (HIGH blast radius — all sprite rendering, all
13 screen modes). NO auto-close. QA sign-off required; owner live spot-check of Space Manbow +
Laydock 2 (and a sprite-heavy non-split title, e.g. a shmup, to catch regressions in the common path)
recommended before release.

---

## 5. Risks and Mitigations

- **R1 — HIGH blast radius (all sprite rendering, all 13 modes).** Mitigation: the byte-identity
  invariant (A-M49-1) makes the common no-mid-write path provably unchanged; EG-2 keeps every
  existing sprite oracle unmodified; EG-3 the full 13-mode A/B; EG-4 title captures; normal
  human-release gate.
- **R2 — Split-coincidence drift (sprite split off by ±1 line from background).** Mitigation: S2
  reuses the IDENTICAL `target = line + 2` boundary (`hbf1xv_machine.cpp:101`) and the M44
  watermark-direction logic; AC-S2 asserts split-line equality against the background accumulator.
- **R3 — M44 command-row-sink / watermark interaction (partial sprite pass sealed mid-command).**
  Mitigation: the sprite watermark uses the SAME advance-only / wrap discipline
  (`hbf1xv_machine.cpp:118-123`); existing M44 oracles must stay green (EG-2).
- **R4 — DEC-0031 collision regression.** Mitigation: S3 feeds the SAME machinery unchanged; EG-5 is
  a dedicated byte-identity gate; the boot-logo path is a hard oracle.
- **R5 — Determinism (progressive pass introduces order-dependence).** Mitigation: watermark-monotonic
  advance; clean frame-boundary reset; EG-6 frame-hash equality across two runs.
- **R6 — Performance (incremental per-write sprite sweeps replace one per-frame sweep).** Mitigation:
  `check_until` is advance-only (each line checked at most once per frame); the conservative
  "any-active-VRAM-write drives check_until(beam)" fallback is at most a small extra incremental
  sweep, never a full re-sweep. Watch for pathological per-write sprite thrash in command-heavy
  frames; profile if a slowdown appears.
- **R7 — M48 co-residency in `v9958_vdp.*`.** Mitigation: keep the sprite-fetch path and the M48
  command-TIMING overlay independent; re-run the M48 command-timing oracles (EG-2) to confirm no
  cross-perturbation. Flag to coordinator: `v9958_vdp.h:284-315` already references M48 Slice 1/2
  seams while `current-phase.md` still labels M48 PLANNING — a state/code drift the coordinator
  should reconcile independently of M49.
- **R8 — Laydock 2 residual may be partly BACKGROUND (A-M49-5).** Mitigation: Task 0 repro FIRST;
  if the scoreboard "noise" is also a background facet, that is a SEPARATE defect — record it and
  scope it to a follow-on milestone rather than expanding M49 (no scope creep without a new DEC).

### Residual risk (honest note — what M49 does NOT fully eliminate)

- **1-frame-lag elimination:** M49 removes the end-of-frame SNAPSHOT lag for sprites, bringing them
  in line with the background's per-line-live behavior. The residual is the SAME line-granular +2
  rounding the background already has — a split is committed at a line boundary, not at the exact VDP
  cycle of the write.
- **Cycle-timed intra-line split cases (e.g. Aleste 2):** where a title reprograms the VDP at a
  specific in-line T-state (mid-scanline), the split remains LINE-quantized, exactly as the
  background is today. This is the human-ACCEPTED M44 Phase 2b boundary (DEC-0069-OUTCOME: the
  residual thin-line/label jitter is "i'm good with what we have"). M49 makes sprites match the
  background's line-granularity; it does NOT exceed it, and a cycle-exact intra-line split would
  require the banned VDP access-slot position table (C1/D4 UNBUILDABLE-WITHOUT-FABRICATION). This
  ceiling is stated up-front so QA does not treat a residual sub-line jitter on Aleste 2 as an M49
  failure.

### Open sub-question (MUST be answered in dev — flagged per the task)

**Is Laydock 2's scoreboard "noise" purely the sprite defect, or ALSO a background facet?** This is
NOT settled by the investigation. It requires a Laydock 2 headless repro during development
(§6 Task 0, EG-4): capture the scoreboard region before/after the sprite fix. If the scoreboard is
clean after the sprite fix → purely sprite (D9). If residual background corruption remains in the
scoreboard region → a SEPARATE background facet exists; record it as a new backlog row and do NOT
fold it into M49 (scope control).

---

## 6. Developer Handoff

**Milestone:** M49 — per-line LIVE sprite attribute/register fetch (backlog D9). Opened by DEC-0076.
PLANNING → implement. Universal spec-modeling fix only (never game-specific). Reference precedence
DEC-0073 (tier-1 fact-sheet > openMSX-for-effect > fMSX). Expected edits confined to
`src/devices/video/{sprite_engine,v9958_vdp}.*` + `src/machine/hbf1xv_machine.cpp`. ZERO `src/core`,
ZERO `src/devices/cpu` (EG-8 relies on that diff being empty).

**Do NOT copy** openMSX `SpriteChecker.{hh,cc}` code (guardrails / `feedback_license_sensitive_scope.md`).
Read the incremental `checkUntil` model for EFFECT only.

**Task 0 (FIRST — fixes the capture baseline + answers A-M49-5):** Build a deterministic headless
Laydock 2 (and Space Manbow) repro that reaches the fixed-HUD-over-scrolling-playfield state, capture
the pre-fix frames under `debug/m49/`, and isolate the Laydock 2 scoreboard region. This answers the
open sub-question and gives the before/after diff for EG-4.

**Slice order (each leaves the suite green):**
1. **S1 — engine:** add `SpriteEngine::check_until(line)` (watermark model, mirror
   `vdp_scanline_accumulator.h:61-145`) replacing the single `recompute_frame()` sweep; preserve the
   per-line formulas + mode-2 base-mask + planar addressing verbatim; make the R#1 BL / R#8 SPD /
   sprite-mode-0 gates per-line-live. Add the no-mid-write byte-identity + per-line-gate unit oracles
   (AC-S1). No behavior change until wired.
2. **S2 — wiring (fix goes live):** in `hbf1xv_machine.cpp:53-124`, on a sprite-relevant write
   (R#1/R#5/R#6/R#8/R#11/R#23 or sprite-attr VRAM) call sprite `check_until(line + 2)` with the SAME
   +2 boundary + M44 watermark-direction discipline; finalize + roll at `on_vsync()`. Add the
   split-coincidence system oracle (AC-S2).
3. **S3 — collision byte-identity:** feed the DEC-0031 machinery (`sprite_engine.cpp:238-325`) from
   the progressive pass; boot-logo + collision-relatch oracles green UNMODIFIED; add the adversarial
   byte-identity oracle (AC-S3, EG-5).
4. **S4 — R#23 A/B:** confirm R#23 stays in the Y formula per-line-live; Space Manbow openMSX A/B for
   direction + split line (AC-S4, EG-4).

**Test obligations (deterministic):** new unit oracles — no-mid-write byte-identity; per-line BL/SPD
gate; split-coincidence (sprite split == background split at L+2); progressive-vs-atomic
collision-event equality. All existing sprite/M22/M32 oracles stay UNMODIFIED and green. Reuse
existing `tests/parity/m22_sprite_*` probes where applicable.

**Evidence gates to run + capture (EG-1..EG-9, §4):** build both exes; FULL ctest (all existing sprite
oracles unmodified + new); FULL 13-mode 0.000% screen A/B vs openMSX (MANDATORY); Space Manbow +
Laydock 2 before/after headless captures (MANDATORY, `debug/m49/`) with Space Manbow openMSX A/B;
DEC-0031 collision-byte-identity gate (MANDATORY); determinism frame-hash equality; asset+checksum;
NO ZEXALL (confirm empty cpu/core diff); ban-attestation grep of `src/`.

**Closure:** NORMAL human-release gate (no auto-close). QA sign-off + recommended owner live
spot-check of Space Manbow, Laydock 2, and a sprite-heavy non-split shmup. Record backlog D9 →
DONE (M49) on closure; if A-M49-5 surfaces a background facet, open a NEW backlog row (do not expand
M49).

**Grounding index (cite these paths in the implementation report):**
- Root cause: `src/devices/video/sprite_engine.cpp:74-301`, `src/devices/video/v9958_vdp.cpp:685-705`.
- Seam: `src/machine/hbf1xv_machine.cpp:53-124` (+2 at `:101`), `:126-146` + `:118-123` (M44 dir).
- Watermark model: `src/devices/video/vdp_scanline_accumulator.h:61-145`.
- DEC-0031 collision machinery: `src/devices/video/sprite_engine.cpp:238-325`.
- Tier-1: `references/fact-sheets/Yamaha V9958 VDP.md` §6 line 117-120, §7 line 126, §10 line 178,
  §5 register map lines 71-77.
- Tier-2 (effect only, never copy): `references/openmsx-21.0/src/video/SpriteChecker.{hh,cc}`,
  `references/openmsx-21.0/src/video/VDP.cc` (update*/base-updater → `SpriteChecker::sync`).
