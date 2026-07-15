# M48 Planner Package — V9958 Command-Engine ↔ CPU VRAM Access-Slot CONTENTION Model (ban-respecting, formula-based)

- Milestone ID: M48
- Title: V9958 command-engine ↔ CPU VRAM access-slot CONTENTION model (deferred backlog **D4** remainder), COMPACT FORMULA / parametric — NOT the openMSX per-slot POSITION table
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent (TBD at kickoff)
- QA Owner: MSX QA Agent (TBD at kickoff)
- Decision: opened by **DEC-0075** (2026-07-14)
- Status: **Planned (planning phase)**
- Grounding tier order (STRICT, DEC-0073): real-hardware spec (tier 1) > openMSX effect-understanding + own spec-disciplined reasoning (tier 2) > fMSX (tier 3). Every constant below is cited to tier 1.

---

## 0. Reference precedence & the license-sensitive-scope ban (read first)

This milestone is the long-deferred backlog **D4 / C1** VDP-timing-depth work. It exists under a
**non-negotiable license-sensitive-scope ban** (project memory `feedback_license_sensitive_scope.md`;
`agent-protocol/guardrails.md` license-isolation rule; CLAUDE.md C1/D4 note; re-affirmed in
DEC-0074 and in `agent-protocol/state/deferred-backlog.md` rows C1/D4):

> **NEVER reproduce openMSX's own large data tables, even "independently re-derived."**

The canonical banned artifact is `references/openmsx-21.0/src/video/VDPAccessSlots.cc:14-141` — the
~340-entry per-slot VDP-cycle POSITION tables (per display mode). Those tables are read **only to
understand the *effect*** and are **NEVER transcribed, re-derived, curve-fit, or otherwise smuggled**
into `src/` under any name. This milestone therefore uses a **COMPACT FORMULA / parametric model**
keyed on the three small tier-1 per-line slot **COUNTS** the fact-sheet publishes plus the
CPU-priority rule — not per-slot positions.

**Honest ceiling, stated up front (per the task's explicit instruction):** a *faithful* contention
model as openMSX implements it *does* require the per-slot POSITION table (to know at which VDP cycle
each free slot lands within the 1368-cycle line). That table is banned here. **We therefore CANNOT
build the cycle-exact per-slot model.** M48 builds the best **ban-respecting average-throughput
approximation** instead, and §7 states precisely what that means for "parity." This is NOT a "too
large" deferral — it is a genuine, permanent sourcing/license blocker for the *exact* model, and M48
does not pretend otherwise.

---

## 1. Scope and Assumptions

### 1.1 What already shipped (do NOT rebuild)

v1.1.4 / DEC-0074 (M47 device-fidelity pass) already landed the **CONTAINED, per-command** cost
corrections in `src/devices/video/vdp_command_engine.{h,cpp}` + `v9958_vdp.{h,cpp}`:

- Per-unit execution cost per command (fact-sheet §8 "per pixel/byte" column): HMMV 48, YMMM 64,
  HMMM 88, LMMV 96, LMMM 120, **LINE 112 (88R+24W, was 88)**, SRCH 88 (VDP cycles/unit).
- Per-line end-of-line overhead (§8 "per line" column): HMMV +56, HMMM +64, LMMV +64, LMMM +64,
  YMMM +0, LINE +32/minor-Bresenham-step (`vdp_command_engine.cpp:59-79`).
- LMCM/LMMC/HMMC transfer-ready (TR) **per-unit pacing** (`arm_transfer_ready_window()`,
  `transfer_unit_cost_tstates()`), which never drops transfer data.
- A pure, clock-free engine duration `VdpCommandEngine::last_cmd_duration_tstates()`, turned into an
  absolute S#2-bit0 **CE busy-window** expiry `cmd_busy_until_cycles_` by
  `V9958Vdp::arm_command_busy_window()` (`v9958_vdp.cpp:152-175`).
- A **coarse, empirical, per-mode active-display multiplier** `kActiveDisplaySlotFactorPercent[5]`
  (200% GRAPHIC4/5/NonBitmap, 300% GRAPHIC6/7/YJK; `v9958_vdp.cpp:61-67`), applied only while
  `raster_display_line() >= 0` at command-issue time. This was DEC-0069 live-human-validated against
  Laydock 2 / F1 visuals — it is a placeholder for the very slot-availability contention M48 now
  makes principled and **sprite-aware**.
- The **`--vdp-cmd-bias` diagnostic knob** (`cmd_busy_bias_tstates_`, `set_cmd_busy_bias()`,
  `v9958_vdp.h:171`) added under DEC-0069/0072, applied AFTER the display factor. Default 0 →
  byte-identical. **M48 must preserve this knob and its default-0 determinism.**

### 1.2 What is STILL missing (the M48 gap)

Our command engine mutates VRAM **atomically** inside the `OUT (#98)/(#99)` that writes R#46 (or
per-unit for the 3 event-driven transfers), and does **not** model CPU ↔ command-engine access-slot
**CONTENTION**:

1. A running command's throughput is **not capped by the per-line access-slot COUNT** — the fact-sheet's
   154 (display off) / 88 (sprites off) / **31 (sprites on)** — so commands are not further slowed when
   sprites are enabled during active display. (The current `kActiveDisplaySlotFactorPercent` is a fixed
   per-mode factor, **not** sprite-aware and **not** grounded in the slot counts.)
2. A running command and a concurrent CPU `OUT (#98)` / `IN (#98)` **do not slow each other** — real
   hardware gives the CPU priority; each CPU VRAM access **steals a slot** from the command engine,
   extending its completion. Fact-sheet §8: "with sprites on, a HMMV can be cut ~2× by simultaneous
   `OUT (#98),A`."

### 1.3 In scope (M48)

- A **ban-respecting, formula-based** contention model layered onto the EXISTING S#2 CE/TR
  busy-window timing overlay — a pure **timing/STATUS** effect, **not** a change to when VRAM writes
  commit (they stay atomic — see §5).
- **Slice 1**: per-line command-throughput cap derived from the three tier-1 slot COUNTS
  (154/88/31), sprite-aware and display-state-aware; supersedes the empirical
  `kActiveDisplaySlotFactorPercent` placeholder.
- **Slice 2**: CPU-concurrent-access command slowdown (CPU-priority rule) — each CPU `#98` VRAM access
  during a busy command extends that command's CE window by one slot's worth of time. **One-directional
  only** (CPU → command); the command NEVER stalls or drops a CPU access (§4.2, §6).
- Add the three slot-count constants (154/88/31) as named tier-1 constants in the existing
  `src/devices/video/vdp_access_timing.h` M23 foundation.
- Deterministic unit + integration tests; screen-mode A/B (no rendering regression); a command-engine
  timing A/B (the contention parity evidence); the full ZEXALL/ZEXDOC sweep at QA.

### 1.4 Out of scope (M48) — explicitly, with justification

- **The ~340-entry per-slot POSITION table** (`VDPAccessSlots.cc`) — BANNED (§0). Hence no cycle-exact
  per-slot phase, and no per-raster-position write tearing beyond M44's existing per-destination-row
  granularity.
- **Command → CPU stalling (the /WAIT direction).** The HB-F1XV does **not** wire the V9958 /WAIT
  generator (tier-1 fact-sheet §1: "the HB-F1XV does NOT wire the V9958 /WAIT feature, so software must
  respect VRAM access timing itself"; §7 WTE). The CPU is never held by the VDP on this machine.
  Modeling a CPU stall would also require gating CPU T-states — the exact hazard the M23 header warns
  against (`vdp_access_timing.h:41-55`) and would break the M21/M22 back-to-back-`OUT` fixtures and
  shift ZEXALL. **Excluded on tier-1 hardware grounds + the CPU-timing-invariance requirement.**
- **Cycle-exact "too-fast access drops requests"** (fact-sheet §2/§10). Reproducing it needs slot
  positions (banned) and would risk corrupting game VRAM by dropping a real CPU write. **Excluded.**
- **Making the busy-window cost track display-state CHANGES mid-command.** Commands execute atomically
  at issue; the contention factor is computed from the display/sprite state **at issue time** (§5.3).
  A documented approximation consistent with the atomic-execution philosophy.
- Any change to the M44 per-destination-row render-sync path, the pixel output, or the VRAM-mutation
  order (§5).

### 1.5 Assumptions (each with a verification action)

- **A-M48-1** — The S#2 CE/TR busy windows are the ONLY CPU-visible surface of command timing (no game
  reads command progress except by polling S#2). *Verify:* re-read `v9958_vdp.cpp` S#2 read path
  (lines 758-782) + `references/fact-sheets/Yamaha V9958 VDP.md` §8 handshake (line 152). Basis for
  "timing/STATUS-only, VRAM output byte-identical."
- **A-M48-2** — The effective slot count is derivable from live registers: display-enable = R#1 bit6
  (BL) AND raster in the active display region (`raster_display_line() >= 0`); sprites-enabled = R#8
  bit1 (SPD=0) AND the mode has sprites (not TEXT1/TEXT2). *Verify:* `v9958_vdp.*` register accessors +
  `vdp_mode.*` + fact-sheet §4 (R#1/R#8 bit maps, lines 71-72) and §6 (sprite modes). The developer
  must confirm the exact SPD polarity and text-mode exclusion in-source before wiring.
- **A-M48-3** — The free-running per-command duration already shipped
  (`last_cmd_duration_tstates()`) corresponds to the **maximum-slot-availability** regime (the
  fact-sheet's 154-slot "screen off" case is the least-contended). The slot-availability factor
  inflates UP from there. *Verify:* fact-sheet §8 line 163 ("Actual speed depends on access-slot
  availability: 154 ... 88 ... 31"); the §8 measurement caveat (line 192) that the numbers were taken
  on a V9938 (Philips NMS8250). Recorded as an assumption because the fact-sheet does not state which
  slot regime the raw per-pixel/per-line figures were measured under; the developer must confirm the
  baseline choice against the command-engine A/B and document any adjustment (within the ban — only the
  three counts as inputs).
- **A-M48-4** — Default-off determinism: with no concurrent CPU access and the display off/blanked, the
  M48 model reduces to the current v1.1.4 CE window (factor 1.0). *Verify:* a byte-identity regression
  test vs the pre-M48 CE-window values for display-off commands.

---

## 2. Spec Summary

### 2.1 Tier-1 constants (the entire numeric surface of M48)

All from `references/fact-sheets/Yamaha V9958 VDP.md`:

| Constant | Value | Source |
|---|---|---|
| VDP cycles per scanline | **1368** | §7 line 124; already `kVdpCyclesPerLine` |
| CPU T-states per VDP-cycle ratio | **6** (÷6) | §1 line 30/34; already `kCpuTstatesPerVdpCycleRatio` |
| Access slots/line — display OFF | **154** | §8 line 163 |
| Access slots/line — display ON, sprites OFF | **88** | §8 line 163 |
| Access slots/line — display ON, sprites ON | **31** | §8 line 163 |
| CPU-priority rule | CPU VRAM access steals a command slot | §8 line 163 |
| /WAIT not wired on HB-F1XV | CPU never stalled by VDP | §1 line 34, §7 line 129 |

That is **three new constants** (154/88/31) plus the two already present. There is **no per-slot
position array** anywhere in this design.

### 2.2 The model (contract-level)

**Per-slot average cost.** With `S` slots distributed across a 1368-cycle line, the average VDP-cycle
spacing between slots is `1368 / S`. In CPU T-states: `ceil(1368 / S / 6)`. This is an **average
rate**, derived purely from `S` — it carries no positional information (that is the ban boundary).

- display off: `1368/154 ≈ 8.88` VDP cyc/slot → ~2 T-states/slot
- sprites off: `1368/88 ≈ 15.5` VDP cyc/slot → ~3 T-states/slot
- sprites on: `1368/31 ≈ 44.1` VDP cyc/slot → ~8 T-states/slot

**Slice 1 — per-line throughput cap (slot-availability factor).** Replace the fixed
`kActiveDisplaySlotFactorPercent[mode]` with a factor that is a function ONLY of *which of the three
counts applies*:

```
S_baseline    = 154                       (max-availability regime, A-M48-3)
S_effective   = 154 (display off/border/vblank) | 88 (display on, sprites off) | 31 (display on, sprites on)
slot_factor   = S_baseline / S_effective  ≈ { 1.00 , 1.75 , 4.97 }
CE_window_ts  = ceil( free_running_ts * slot_factor )   [then + --vdp-cmd-bias, as today]
```

The factor is thus a 3-valued lookup, not a table. `slot_factor = 1.00` when the display is off →
**A-M48-4 byte-identity** with v1.1.4 for the display-off path. The developer MAY, within the ban,
calibrate the exact ratio against the command-engine A/B (see §2.3) but the **only permitted inputs
are 154/88/31, 1368, 6** — no VDPAccessSlots-derived per-slot tuning.

Recommended implementation locus: `V9958Vdp::arm_command_busy_window()` (the display/sprite state is
known there; the engine stays state-agnostic so `last_cmd_duration_tstates()` and its M44 oracle stay
UNCHANGED). This is the minimal, lowest-blast-radius change.

**Slice 2 — CPU-concurrent-access slowdown (CPU priority).** On each CPU VRAM access through port
`#98` (`V9958Vdp::io_read`/`io_write`, `port & 3 == 0`), if a command is currently busy
(`cmd_busy_until_cycles_ > clock_source_->cpu_total_cycles()`), extend the command's expiry by one
slot's worth of time at the current `S_effective`:

```
if (command busy at this CPU #98 access)
    cmd_busy_until_cycles_ += ceil( 1368 / S_effective / 6 )   // one stolen slot, CPU-priority
// the CPU access itself is serviced normally — no stall, no dropped write, no CPU T-state change
```

This reproduces the §8 "HMMV cut ~2× by simultaneous `OUT (#98),A`" effect *on average*: a tight CPU
loop hammering `#98` during a sprites-on command steals ~8 T-states of command progress per access,
which for a comparable access cadence roughly halves the command's effective throughput. The reverse
direction (command → CPU) is **not** modeled (§1.4, tier-1 /WAIT justification).

### 2.3 Behavior boundaries / acceptance grounding

- **Sprites-on slows commands identically regardless of sprite count** — fact-sheet §6(a) line 120:
  "the whole sprite subsystem always performs the same VRAM fetch pattern ... even if fewer/no sprites
  are visible — this is why command execution slows identically whenever sprites are enabled." So the
  Slice 1 sprites-on branch keys on *sprites enabled* (R#8 SPD + mode), NOT on how many sprites are
  active on the line.
- **Commands execute line-by-line** — §8 line 163: "a 20×4 rect is faster than 4×20." The existing
  per-line overhead term already encodes this; Slice 1's factor multiplies the whole (per-unit +
  per-line) duration, preserving that ordering.

---

## 3. Milestones (slice breakdown — small, independently verifiable)

> Sequencing: **Slice 1 → Slice 2** (Slice 2 consumes Slice 1's `S_effective` helper). Each slice is
> independently buildable, unit-testable, and A/B-checkable, and each closes with the full evidence
> gate set (§5 of "Acceptance Criteria").

### Slice 1 — Per-line throughput cap from 154/88/31

- **Deliverables:**
  - `src/devices/video/vdp_access_timing.h`: add `kSlotsPerLineDisplayOff = 154`,
    `kSlotsPerLineSpritesOff = 88`, `kSlotsPerLineSpritesOn = 31` (tier-1, cited §8 line 163) + a
    `constexpr` per-slot-cost helper `slot_cost_tstates(int slots_per_line)` = `ceil(1368/slots/6)`.
  - `src/devices/video/v9958_vdp.{h,cpp}`: a private helper `effective_slots_per_line()` computing the
    3-way count from R#1 BL + `raster_display_line()` + R#8 SPD + mode (A-M48-2); rework
    `arm_command_busy_window()` to apply `slot_factor = 154 / effective_slots_per_line()` (replacing
    `kActiveDisplaySlotFactorPercent`), preserving the `--vdp-cmd-bias` add and the
    `command_timing_suspended_` gate.
- **Tier-1 grounding:** fact-sheet §8 line 163 (three counts + line-by-line), §7 line 124 (1368), §4
  lines 71-72 (R#1 BL / R#8 SPD), §6 lines 116-120 (sprite modes / text has no sprites).
- **Independently verifiable via:**
  - Unit oracle (new): for a fixed command (e.g. HMMV NX×NY), assert the armed CE window equals the
    hand-derived `ceil(free_running * 154/S)` for S ∈ {154, 88, 31}, driven by controlling BL/SPD/mode
    and raster position. Oracle values re-derived from the tier-1 constants (NOT read back from the
    implementation → non-tautology).
  - Byte-identity regression: display-off commands produce the SAME CE window as v1.1.4 (A-M48-4).
  - Command-engine timing A/B (sprites-off vs sprites-on, no concurrent CPU access) vs openMSX.
- **DoD:** build + full ctest green; Slice-1 unit oracle + byte-identity regression pass; the DEC-0069
  live-validated titles (Laydock 2 / F1) do not visibly regress (headless frame capture where possible;
  owner spot-check flagged if the factor shift is visible — see Risk R3).

### Slice 2 — CPU-concurrent-access command slowdown (CPU-priority rule)

- **Deliverables:**
  - `src/devices/video/v9958_vdp.cpp`: in `io_read`/`io_write` for `port & 3 == 0` (`#98` VRAM data),
    when a command is busy, add `slot_cost_tstates(effective_slots_per_line())` to
    `cmd_busy_until_cycles_`. CPU access path otherwise unchanged (no early return added to the CPU
    step; no VRAM write dropped/delayed).
  - Respect `command_timing_suspended_` and the null-clock (headless-without-clock) path exactly as the
    existing CE window does (inert when no clock).
- **Tier-1 grounding:** fact-sheet §8 line 163 ("CPU VRAM access takes priority over the command
  engine and steals slots — with sprites on, a HMMV can be cut ~2× by simultaneous `OUT (#98),A`"); §1
  line 34 / §7 line 129 (/WAIT not wired → CPU never stalled, so the effect is one-directional).
- **Independently verifiable via:**
  - Unit/integration oracle (new): issue a command, then perform N CPU `#98` accesses while busy;
    assert `cmd_busy_until_cycles_` advanced by exactly `N * slot_cost_tstates(S)` beyond the Slice-1
    baseline; assert the CPU access returned correct data and CPU T-states are unchanged.
  - Adversarial: assert a CPU `#98` access AFTER the window has expired does NOT extend it (no phantom
    contention).
  - Command-engine timing A/B: HMMV + a concurrent `OUT (#98)` stream, sprites on, vs openMSX — the
    §8 "~2× cut" scenario (matched on the aggregate-rate approximation, §7).
- **DoD:** build + full ctest green; Slice-2 oracles pass; **CPU-timing invariance proven** — the M21
  (`hbf1xv_m21_vdp_render_system_test`) and M22 (`hbf1xv_m22_sprites_command_engine_system_test`)
  back-to-back-`OUT` fixtures are byte-identical in T-states pre/post-M48 (the M23-warned
  re-validation, `vdp_access_timing.h:41-55`); `git diff` shows `src/devices/cpu/*` and `src/core/*`
  untouched.

---

## 4. Acceptance Criteria

### 4.1 Slice-level (must all hold at each slice DoD)

- **AC-1 (Slice 1)** — The active-display command CE busy window is computed as
  `ceil(free_running_ts * 154 / S_effective)` with `S_effective ∈ {154, 88, 31}` selected from live
  R#1 BL + raster + R#8 SPD + mode; the empirical `kActiveDisplaySlotFactorPercent` placeholder is
  removed or reduced to the sprites-off case, with a header comment citing fact-sheet §8 line 163.
  A new unit oracle proves the three factors against tier-1-re-derived expected values.
- **AC-2 (Slice 1)** — Display-off / border / vblank commands (`S_effective = 154`, factor 1.00)
  produce a CE window **byte-identical** to v1.1.4 (A-M48-4 regression test).
- **AC-3 (Slice 2)** — Each CPU `#98` VRAM access during a busy command extends the CE window by
  exactly `slot_cost_tstates(S_effective)` T-states; a `#98` access outside the window has no effect;
  proven by a dedicated unit/integration oracle with tier-1-re-derived expected values.
- **AC-4 (Slice 2, HARD)** — **CPU timing is byte-identical.** The M21/M22 CPU-program fixtures report
  identical T-state totals pre/post-M48; `git diff v1.1.4 -- src/devices/cpu src/core` is EMPTY. The
  CPU `#98` access is never stalled, delayed, or dropped by the contention model.
- **AC-5 (both)** — Determinism preserved: identical inputs → identical CE/TR windows across runs; the
  `--vdp-cmd-bias` knob still applies (after the slot factor) with default 0 unchanged.
- **AC-6 (both)** — VRAM mutation stays atomic and byte-identical: the model touches only S#2 CE/TR
  timing, never the written pixels, the write order, or the M44 per-destination-row render-sync path.

### 4.2 Milestone-level evidence gates (run and capture; never fabricate)

- **EG-1** — `tools/validate-assets.ps1` PASS (BIOS present + ≥1 ROM).
- **EG-2** — `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` refreshed.
- **EG-3** — `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=ON` + `cmake --build build --config Debug`
  succeeds for BOTH executables from the single canonical `build/` (DEC-0041).
- **EG-4** — `ctest --test-dir build -C Debug --output-on-failure` — the FULL deterministic suite is
  green (current baseline 228/228 + the new M48 oracles), zero regression.
- **EG-5 (MANDATORY for M48)** — The **full ZEXALL/ZEXDOC sweep** runs at QA and PASSES (0 error
  markers, both suites), captured to a durable log under `debug/m48/zexall/`. Rationale: M48 touches
  the CPU↔VRAM access-gating path; even though the design does NOT edit `src/devices/cpu`/`src/core`
  (proven by AC-4's empty diff), the task mandates the sweep as the belt-and-suspenders safety net for
  any accidental CPU-timing shift. The `feedback_slow_test_cadence.md` "withhold-substitute on empty
  cpu/core diff" shortcut is **NOT** taken this milestone — the sweep is required regardless.
- **EG-6 (screen-mode A/B, HARD)** — All **13 M41 screen modes still MATCH openMSX at 0.000%**
  (bit-exact), proving the atomic VRAM output is unchanged by the timing overlay. Reuse/extend the
  M41 screen-mode A/B harness; capture the parity matrix in `docs/m48-openmsx-ab.md`.
- **EG-7 (command-engine timing A/B)** — A command-engine timing A/B vs openMSX for representative
  commands (at minimum HMMV, HMMM, LMMM, LINE) under the three slot conditions (display-off,
  sprites-off, sprites-on) AND the §8 "HMMV + concurrent `OUT (#98)`" contention scenario. The
  disposition is measured and honestly recorded per §7 (aggregate-rate parity, NOT cycle-exact); the
  DIRECTION and rough magnitude of the sprites-on and CPU-concurrent slowdown must match. Captured in
  `docs/m48-openmsx-ab.md`.
- **EG-8 (non-tautology, HARD)** — Every new timing oracle is **re-derived from the tier-1 constants**
  (154/88/31, 1368, 6), never read back from the implementation, and an adversarial mutation (e.g.
  swap 88↔31, or drop the CPU-steal add) makes the oracle FAIL — proving the tests discriminate. **No
  existing oracle is weakened** (the M22/M32/M44 command-timing oracles and the render byte-identity
  oracles stay intact; any oracle whose asserted CE window legitimately changes under the new factor is
  re-derived from tier-1, with the change explained, not loosened).
- **EG-9 (ban attestation)** — The implementation report attests that `VDPAccessSlots.cc` was read for
  effect only and NO per-slot position value was transcribed/re-derived into `src/`; the sole numeric
  inputs are 154/88/31/1368/6. QA independently greps `src/` to confirm no ~340-entry-style array was
  introduced.

### 4.3 Closure

- QA sign-off (`docs/m48-qa-signoff.md`) with all gates independently reproduced.
- DEC-0069 live-validated titles (Laydock 2 / F1 / YS II boot) either shown non-regressed headlessly
  OR flagged for an owner live spot-check (Risk R3). Normal human-release-decision gate applies (no
  auto-close) given the HIGH blast radius.
- Same-cycle ledger update: `deferred-backlog.md` C1/D4 rows updated to reflect the CONTENTION model's
  ban-respecting partial delivery (the *average-throughput* contention now modeled; the *cycle-exact
  per-slot* remainder stays UNBUILDABLE-WITHOUT-FABRICATION), with the DEC-0075 note.

---

## 5. Coexistence with atomic VRAM mutation + M44 render-sync (design invariant)

This is a **timing/STATUS-only** overlay. Concretely:

- **VRAM writes stay atomic.** The command's pixels are still committed inside the `OUT (#98)/(#99)`
  that writes R#46 (atomic commands) or per-unit for LMCM/LMMC/HMMC — **unchanged**. M48 changes only
  `cmd_busy_until_cycles_` / `tr_busy_until_cycles_` (the S#2 CE/TR expiry timestamps that a poll
  reads), never the written bytes, the write order, or which pixels are drawn.
- **M44 render-sync is untouched.** The `VdpCommandEngine::CommandRowSink` per-destination-row sink
  (`vdp_command_engine.h:80-92`) still fires once per destination row BEFORE that row's writes, so the
  renderer still captures mid-frame command output at the correct raster line. M48 adds nothing to that
  path.
- **State snapshot at issue time (documented approximation).** Because execution is atomic, the
  `S_effective` used for the slot factor is sampled from the display/sprite state **at command issue**;
  it does not track state changes that occur *while the CE window elapses over subsequent frames*.
  Consistent with the existing atomic model; recorded as an honest §1.4 approximation.
- **`command_timing_suspended_` + null-clock** paths behave exactly as today: when timing is suspended
  or there is no `clock_source_`, the model is inert (no busy window, no contention) — preserving the
  deterministic headless/unit-test paths.

Result: EG-6 (13 screen modes 0.000%) and the M44/M32 render oracles are structurally guaranteed to
hold, because no rendering input changed.

---

## 6. Risks

| # | Risk | Severity | Mitigation |
|---|---|---|---|
| **R1** | Touching the CPU↔VRAM access-gating path shifts CPU timing → breaks M21/M22 CPU-program fixtures and/or ZEXALL. | **HIGH** | Design forbids ANY CPU-side stall/delay/drop (Slice 2 only extends the *command's* expiry; the CPU `#98` access is serviced unchanged). AC-4 proves the M21/M22 fixtures byte-identical; EG-5 runs the FULL ZEXALL/ZEXDOC sweep at QA; `git diff` proves `src/devices/cpu`+`src/core` empty. The M23 header re-validation warning (`vdp_access_timing.h:41-55`) is discharged explicitly. |
| **R2** | The model interferes with the atomic-write + M44 render-sync assumption (writes commit inside the OUT). | **HIGH** | §5: timing/STATUS-only overlay; VRAM mutation + write order + M44 sink strictly unchanged. EG-6 (13 modes 0.000%) + the M44/M32 render oracles are the guard. |
| **R3** | HIGH all-games blast radius (rendering + CPU timing). The new sprite-aware factor changes the CE window for real titles; the sprites-on branch (~4.97×) is far more aggressive than the DEC-0069-tuned 200/300%, risking regressing the live-human-validated Laydock 2 / F1 timing/visuals. | **HIGH** | EG-6 full 13-mode A/B (0.000%) + EG-7 command-timing A/B (all 3 slot states) + no oracle weakened (EG-8). Where headless capture cannot prove a live title unregressed, flag an OWNER live spot-check before release (normal human-release gate; no auto-close). If the tier-1 factor visibly regresses a DEC-0069 title, reconcile per DEC-0073 (document the tier-1-spec-vs-live-tuning tension; prefer the spec, record the divergence) — do NOT silently re-tune with table-derived values. |
| **R4** | Determinism / `--vdp-cmd-bias` regression. | Medium | AC-5: bias applied after the slot factor, default 0 unchanged; a determinism regression test (identical inputs → identical CE/TR windows) + the byte-identity display-off test (AC-2). |
| **R5** | Ban leakage — the temptation to reach for `VDPAccessSlots.cc` per-slot spacing to "improve" the sprites-on average. | **HIGH (governance)** | §0 + EG-9 ban attestation + QA grep of `src/` for any ~340-entry-style array. Only 154/88/31/1368/6 are legal inputs. If faithfulness *requires* the table, the honest disposition is §7 (approximation), not smuggling. |
| **R6** | Average-throughput model under-inflates the sprites-on case (the real 31 slots are unevenly clustered with large gaps during active fetch; the average understates worst-case gaps). | Medium (accuracy) | Accepted + documented as the ban ceiling (§7). The A/B (EG-7) measures and records the residual; the model is directionally correct and matches on average — the exact gap structure is the banned-table remainder, left OPEN in C1/D4. |
| **R7** | Oracle tautology (tests curve-fit to our own output). | Medium | EG-8: oracles re-derived from tier-1 constants + adversarial mutation must fail; QA independently reproduces the derivation. |

---

## 7. Honest statement — what CANNOT be done within the ban, and what "parity" means here

**What the faithful (openMSX) model does that we cannot.** openMSX's contention is driven by the
`VDPAccessSlots.cc` per-slot POSITION tables: for each display mode it knows the exact VDP cycle at
which every free slot lands within the 1368-cycle line, and the command engine + CPU compete for those
*specific* slots via `getAccessSlot(delta)`. That per-slot positional knowledge is the ~340-entry
banned table. **Without it we cannot:**

1. Reproduce the **cycle-exact** VDP-cycle at which any single access is granted → no exact
   CPU/command interleave phase.
2. Model the **exact worst-case gap** structure of the 31 sprites-on slots (they cluster with large
   gaps during active fetch); our `1368/S` average understates those gaps (Risk R6).
3. Reproduce **cycle-exact "too-fast dropped request"** behavior (also excluded on the safety ground
   that dropping a real CPU write corrupts game VRAM).
4. Reproduce any **raster-position-dependent** command/CPU tearing beyond M44's per-destination-row
   granularity.

**What "parity" realistically means for M48 (the ban-respecting bar):**

- **Aggregate command-completion timing** — the S#2 CE/TR busy window a game polls — matches the
  tier-1 fact-sheet §8 slot model **on average**, i.e. within the average-rate (`1368/S`)
  approximation, and matches openMSX's aggregate command duration in *direction and rough magnitude*,
  **not cycle-for-cycle**.
- **The sprites-on and CPU-concurrent slowdowns are present, sprite-aware, and roughly quantitatively
  correct** (the §8 "~2× with a concurrent `OUT (#98)`" effect emerges from the model), which the
  current v1.1.4 build does NOT have at all — a genuine, real improvement.
- **Pixel output stays bit-exact** (EG-6: all 13 modes 0.000% vs openMSX) — the atomic writes are
  untouched.
- **CPU timing stays bit-exact** (AC-4 + EG-5) — no CPU stall/drop is introduced.

**Authority note (DEC-0073).** The tier-1 authority here is the fact-sheet §8 slot COUNTS, not
openMSX. openMSX's own command timing is a V9938 logic-analyzer *approximation* (fact-sheet §8 caveat
line 192) and openMSX is documented "too fast on VDP commands" (fact-sheet §10 line 176, openMSX issue
#2057). So exact openMSX parity is neither the target nor fully achievable; matching the tier-1 §8
counts on average, with the cycle-exact per-slot remainder honestly left OPEN in C1/D4, is the correct
and complete disposition of M48 under the ban.

---

## 8. Developer Handoff

- **Primary edit targets (all under `src/devices/video/` — zero `src/core`, zero `src/devices/cpu`):**
  - `vdp_access_timing.h` — add `kSlotsPerLineDisplayOff=154` / `kSlotsPerLineSpritesOff=88` /
    `kSlotsPerLineSpritesOn=31` (cite §8 line 163) + `constexpr slot_cost_tstates(int slots)` =
    `ceil(1368/slots/6)`.
  - `v9958_vdp.{h,cpp}` — Slice 1: `effective_slots_per_line()` helper + reworked
    `arm_command_busy_window()` (replace `kActiveDisplaySlotFactorPercent` with `154/S_effective`);
    Slice 2: the `#98` CPU-access `cmd_busy_until_cycles_` extension in `io_read`/`io_write`.
  - `vdp_command_engine.{h,cpp}` — expected UNCHANGED (the engine stays state-agnostic; its
    `last_cmd_duration_tstates()` and the M44 duration oracle must not move). Confirm via diff.
- **Test targets:** new unit oracles under `tests/unit/devices/video/` (Slice-1 slot-factor,
  Slice-2 CPU-steal, adversarial); an integration test under `tests/integration/machine/` proving CPU
  T-state invariance + the M21/M22 fixture byte-identity (extend the M23 non-gating pattern
  `hbf1xv_m23_vdp_access_timing_non_gating_integration_test`); the byte-identity display-off
  regression (AC-2).
- **A/B harness:** reuse the M41 screen-mode A/B for EG-6; add/extend a command-engine timing A/B under
  `tools/` for EG-7 (the 3 slot states + the concurrent-`OUT` scenario), output to `docs/m48-openmsx-ab.md`.
- **Evidence dirs:** ZEXALL log under `debug/m48/zexall/`; any frame captures under `debug/m48/frames/`.
- **Order:** Slice 1 (build+ctest+A/B) → Slice 2 (build+ctest+CPU-invariance+A/B) → QA (full ZEXALL
  sweep + all gates reproduced). Keep changes incremental; do not fold both slices into one commit.
- **Standing rules:** single `build/` only (DEC-0041); no game-specific logic (universal fixes only);
  system-wide review before code; cite tier-1 for every constant; NEVER open/transcribe
  `VDPAccessSlots.cc` values into `src/`.

---

## 9. Backlog / deferred-scope consultation (DEC-0005)

- **Closes (partially, ban-respecting):** C1 / D4 — the *average-throughput* CPU↔command CONTENTION
  model (per-line slot cap from 154/88/31 + CPU-priority slot-steal) now delivered. Mark the rows to
  reflect this partial, ban-respecting close in the same cycle.
- **Re-affirmed still-OPEN (the ban remainder):** the *cycle-exact per-slot* model (the ~340-entry
  `VDPAccessSlots.cc` position tables; exact sub-frame IRQ raster position; cycle-exact dropped-request;
  raster-position command/CPU tearing) stays **UNBUILDABLE-WITHOUT-FABRICATION** — unchanged by M48.
- **Untouched other backlog:** D8/D9/D10 (renderer sub-line/live-sprite/geometry remainders), E3/E4
  (audio depth), C10/F1/F2, G-rows — none closed or affected by M48.
