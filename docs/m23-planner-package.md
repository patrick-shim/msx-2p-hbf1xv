# M23 Planner Package — Exact Cycle Timing (C1/C2/D4): Scope-Resolution + HALT-R Closure + VDP Access-Timing Foundation

- Milestone ID: M23
- Title: Exact Cycle/T-State Timing Parity — Z80 HALT-R Closure (C2) + VDP Access-Slot Timing Foundation (C1/D4, narrowed)
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Request: REQ-M23-001 (planner-first, no production code; logged `agent-protocol/channels/requests.md:1101-1109`)
- Decisions in force: DEC-0004 (HALT-R deferral, `agent-protocol/channels/decisions.md:43-49`), DEC-0005
  (backlog disposition discipline — every planner package restates all 34 rows), DEC-0018/DEC-0019 (M21/M22
  closure — both explicitly built pull-model/atomic devices *because* cycle-accurate VDP timing was
  scheduled for this milestone), the human's 2026-07-07 M21-M23 pre-authorization (separate tags,
  per-milestone QA sign-off, "deliberate cross-check across the entire system" every cycle).
- Backlog effect this cycle (full rationale in §2 below):
  - **C2 (Z80 HALT-R increment) → closes IN FULL.**
  - **C1 (exact cycle/T-state timing parity) → advances to `IN-PROGRESS (M23 partial)`.** The HALT-refetch
    M1-wait sub-item closes via C2; the VDP-access-recovery-wait item is the sole remaining substance and is
    identical to D4's remainder (see §2.4) — no other non-M1, non-VDP wait-state source exists for this
    machine (confirmed by source read, §2.4).
  - **D4 (cycle-accurate VDP access-slot/command timing) → advances to `IN-PROGRESS (M23 partial)`.** A real,
    tested, additive, **non-gating** access-timing foundation ships this cycle (raster-position derivation +
    the `Delta` cost-unit contract + two clearly-separated, cited latency facts). The full per-mode slot
    tables (154/88/31/47 entries), the CPU-priority-steals-command-engine-slot interaction, the command
    engine's real per-pixel/per-line VDP-cycle cost, the exact sub-frame IRQ raster position, and any actual
    CPU-execution gating remain explicitly OPEN, named precisely, and carried forward — mirroring the D7/C5
    "one tracked row, not force-closed, not split into a new letter" precedent (DEC-0011, DEC-0018/0019).
  - No other backlog row is touched. All 34 rows re-affirmed in §5.
- Gate: normal human-release-decision gate applies; per `agent-protocol/state/current-phase.md` the
  coordinator is pre-authorized to proceed through the release-decision/tag step for this specific M21-M23
  run without an additional pause, UNLESS QA does not reach a clean PASS (real blocker → stop and consult
  the human).

> Grounding rule: every behavior-affecting claim below cites a concrete `references/openmsx-21.0/...` or
> `references/fact-sheets/...` path with line numbers, independently re-derived (not transcribed) by the
> planner this cycle. openMSX source is the BEHAVIOR reference only (GPL) — **never copied into `src/`**.
> This package additionally re-derives the actual arithmetic behind every claim (per the task's explicit
> "recompute, don't transcribe" instruction) rather than summarizing prior findings.

---

## 1. Scope and Assumptions

### 1.1 In scope

- **(a) C2 — Z80 HALT-R increment, closed in full.** The R register continues to increment (low 7 bits,
  bit 7 frozen) once per "phantom" M1 refetch cycle while the CPU sits in the HALT loop, matching genuine
  Zilog Z80A behavior (Z80A fact-sheet §5 "R register... You must increment R exactly correctly"). Because
  the HB-F1XV's S1985 asserts its M1-wait on **every** `/M1`-cycle including HALT's internal refetch (not
  just "real" opcode fetches), this closure is inseparable from a small, previously-deferred timing
  correction: each halted "idle" `step()` call costs the MSX-accurate **5 T-states** (bare 4 + 1 M1 wait),
  not the current 4 — independently re-derived from `references/openmsx-21.0/src/cpu/Z80.hh:19-21`
  (`HALT_STATES = 4 + WAIT_CYCLES`) and `CPUCore.cc:2508-2511` (`incR(advanceHalt(HALT_STATES, ...))`), §2.1.
- **(b) A genuine, tested, ADDITIVE, non-gating VDP access-timing foundation** (new
  `src/devices/video/vdp_access_timing.h`, §3.1): a raster-position derivation utility (CPU-T-state-since-
  last-vsync → VDP-cycle-within-current-1368-cycle-line), the `VdpAccessDelta` cost-unit enum (the 15 named
  offsets openMSX's own access-slot calculator is keyed on), and two explicitly-separated, cited latency
  facts (the V9938+/V9958 `Delta::D16` CPU-request-latency floor, and the fact-sheet's independently-quoted
  "~29 Z80-cycle safe access" software convention) — see §2.4/§3 for the full derivation and the reasons
  this stops short of the complete per-mode slot tables.
- **(c) Deterministic unit + integration tests**, zero regression across the FULL M1-M22 suite (117 tests),
  independently re-verified by both the developer and QA, **with the M9/M12 CPU-timing-oracle suites named
  as a non-negotiable, zero-tolerance regression gate** (§4, §6) given this milestone's elevated risk to
  exactly that surface.
- **(d) A concrete, checkable "non-gating" proof**: a regression test that re-runs the exact M21/M22
  system-test CPU-program fixtures (which already contain back-to-back `OUT (#98),A` port accesses with no
  spacer instructions, §2.5) and asserts their cumulative T-state totals are BYTE-IDENTICAL to their current,
  QA-signed values — proving the new access-timing code is inert for every existing call path.
- **(e) Real openMSX A/B evidence** for C2 (a genuine, feasible technique exists, §3.4) and an explicit,
  honest BLOCKED disposition for the VDP-access-wait numeric-cost sub-claim (no feasible live-Tcl-debugger
  cycle-cost comparison technique exists in this project's established methodology, §3.4).
- **(f) Full deferred-backlog review** — all 34 rows re-affirmed (§5).

### 1.2 Out of scope (named explicitly, each with justification)

| Item | Why OUT of M23 | Owning milestone (candidate) |
|---|---|---|
| **Full per-mode VDP access-slot tables** (154/88/31/47 entries, screen-off/sprites-off/sprites-on/text) | Genuine reproduction requires (i) a mid-frame raster-position tracking mechanism this project's clock model does not use during `run_frame()`'s whole-frame-atomic jump, and (ii) transcribing ~340 empirically-measured numeric table entries — a large, exacting, license-sensitive undertaking (§2.4/§2.6) that is its own dedicated-milestone-sized piece of work, not a same-cycle addition alongside C2's CPU-core-level change. | Future VDP-timing-depth milestone (D4 remainder, tracked) |
| **CPU-access-steals-command-engine-slot interaction** (fact-sheet §8: "CPU VRAM access takes priority... a HMMV can be cut ~2× by simultaneous OUT (#98),A") | Depends entirely on the full slot-table model above; `VdpCommandEngine`'s atomic/event-driven execution model (M22) has no notion of a "slot" to steal until that model exists. | Same future milestone |
| **Command engine real per-pixel/per-line VDP-cycle cost** (fact-sheet §8 timing table: 48/56/64/72/88 W/R cycles) | Same dependency; M22's atomic execution deliberately has no elapsed-time model to attach these costs to (M22 planner package §1.2, reaffirmed here). | Same future milestone |
| **Exact sub-frame raster position of the line/VBlank IRQ** | Requires the same raster-position-during-a-live-frame mechanism, extended to interrupt timing; `run_frame()`'s current model asserts VBlank exactly once per whole-frame `tick()`, with no intra-frame line-counter. Building the raster tracker is this milestone's foundation (§3.2); wiring it to interrupt-firing precision is additional, separately-riskable depth. | Same future milestone |
| **Actual CPU-execution gating on VDP access timing** (inserting real wait T-states into `step_cpu_instruction()`/`elapsed_cycles()` for `#98-#9B`/`#9C-#9F` accesses) | Concretely, provably risky (not merely theoretically risky) THIS cycle: the M21/M22 system-test CPU programs already contain multiple back-to-back `OUT (#98),A` port writes with zero intervening spacer instructions (§2.5, cited line numbers) — actually gating on real wait-state/drop behavior would either change those tests' T-state totals (breaking golden assertions) or, per D4's own "too-fast dropped-request behavior," could silently DROP bytes those tests' content assertions depend on. This is a demonstrated, not speculative, regression risk to already-shipped, QA-signed M21/M22 architecture. | Same future milestone, only after the M21/M22 fixtures are deliberately re-validated against real gating |
| **Toshiba-clone-style extra VDP wait cycles** (T9769A/B +2, T9769C +1 per VDP I/O) | Z80A fact-sheet §7 explicitly: "The HB-F1XV's S1985 adds the standard M1 wait but does not add those extra Toshiba VDP waits by default." Not applicable to this machine — a permanent N/A, not a gap. | N/A |
| **V9958 `/WAIT` generator (R#25 WTE)** | Fact-sheet §7/§9: "Not wired on HB-F1XV." Already stored as an inert register bit since M14/M22; no CPU-stall behavior to add. | N/A — this machine never wires `/WAIT` |
| **CPU turbo-mode wait-state switching** | Z80A fact-sheet §6: "The HB-F1XV has no CPU turbo mode... The 'Speed Controller' slider is not a clock change." Belongs to backlog C8 (Sony speed-controller/PAUSE), not C1. | C8 (already a separate, later-indicative backlog row) |
| **ZEXALL/ZEXDOC full sweep** | Backlog C3, a CPU-correctness (not cycle-timing) item; `references/zexall/` is present but a separate milestone's scope per the existing indicative order. | C3 (M24, indicative) |

### 1.3 Assumptions (each grounded, each with a verification action)

- **A-M23-1 (the CPU core's "publish pure datasheet T-states, let the machine layer add the M1 wait"
  architectural split is NOT touched by C2 — it is reused, not altered).** Read
  `src/machine/hbf1xv_machine.cpp:395-401` this cycle: `step_cpu_instruction()` computes
  `tstates = datasheet_tstates + m1_wait` where `m1_wait = s1985_engine_.m1_wait_tstates(cpu_.m1_cycles_last_step())`
  and `s1985_engine_.m1_wait_tstates()` (`src/devices/chipset/s1985_engine.cpp:23-25`) is simply
  `m1_cycles * kM1WaitTStates`. This means the ONLY change C2 needs inside `Z80aCpu::step()`
  (`src/devices/cpu/z80a_cpu.cpp:58-59`, the `else if (state_.halted()) { tstates = 4; }` branch) is to call
  the EXISTING `increment_refresh_register()` helper (`z80a_cpu.cpp:206-215`, already used by every other M1
  fetch) instead of leaving R/`m1_cycle_count_` untouched — the halted branch's own returned `tstates`
  literal stays `4` (bare, datasheet-correct), and the machine-level `+= m1_wait` arithmetic automatically
  produces the correct 5T MSX-accurate result with **zero changes to `hbf1xv_machine.cpp` beyond re-running
  the same, already-shipped formula**. *Verify:* a unit test calls `Z80aCpu::step()` directly (bypassing the
  machine) on an already-halted CPU and asserts the returned value is still the bare `4`, confirming the
  CPU-core/machine-layer split is preserved, not collapsed.
- **A-M23-2 (this is a real hardware fact, not a fabricated timing invention — independently re-derived,
  not merely cited).** `references/openmsx-21.0/src/cpu/CPUClock.hh:53-59` (`advanceHalt`):
  `halts = (ticks + hltStates - 1) / hltStates` (ceiling division) then `clock += halts * hltStates` and
  the caller (`CPUCore.cc:2510`) does `incR(halts)` — i.e., **each unit of `hltStates` (=5 on Z80/MSX,
  `Z80.hh:21`) both advances the clock by 5 T AND increments R by exactly 1.** This is the SAME physical
  M1 cycle doing both things — they cannot be separated on real hardware, and openMSX does not separate
  them either. *Verify:* re-read the two cited files independently and confirm the same conclusion (both
  effects are driven by the identical `halts` computation, not two separate mechanisms).
- **A-M23-3 (the ENTIRE 117-test suite is structurally immune to this change except ONE named test — an
  exhaustive, independently-run grep audit, not an assumption).** Ran `rg "\.halted\(\)" tests/` this cycle:
  22 files match. Read the call-site context of every match: 21 of the 22 use the pattern
  `for/while (... && !machine.cpu().state().halted()) { machine.step_cpu_instruction(); }` — i.e., they STOP
  stepping exactly at the halt boundary and never step again while already halted (confirmed for
  `hbf1xv_m21_vdp_render_system_test.cpp:34`, `hbf1xv_m22_sprites_command_engine_system_test.cpp:33`,
  `hbf1xv_interrupt_ack_integration_test.cpp:58,102`, `hbf1xv_m16_fdc_integration_test.cpp:136`,
  `hbf1xv_m11_parity_checkpoint_integration_test.cpp:66,83`, `hbf1xv_parity_checkpoint_integration_test.cpp:108,126`,
  and every other boot/parity-checkpoint test). `hbf1xv_debug_dump_unit_test.cpp:168-180` captures
  `halt_before`/`r_before`/`cycles_before` only to assert taking a debug dump TWICE is side-effect-free
  (`first == second`) — it never steps while halted either. `z80a_trace_export_unit_test.cpp:152` compares
  two PARALLEL runs' final states for equality (trace-observer-on vs off) — both runs would still match
  each other identically under the new HALT-R behavior, so this is unaffected regardless. **The sole
  exception is `tests/integration/machine/hbf1xv_cpu_step_integration_test.cpp:62-73`**, which explicitly
  steps ONE more time after `HALT` (`t3 = machine.step_cpu_instruction()`, asserting `t3 == 4` and a final
  cumulative `machine.elapsed_cycles() == 22`) — this is the ONE golden this milestone must deliberately,
  visibly update (§4, §6). *Verify:* the developer re-runs the identical `rg "\.halted\(\)" tests/` command
  at implementation time and confirms no NEW site has been added since this planning cycle that would also
  need updating; QA independently re-runs the same command.
- **A-M23-4 (recomputed golden arithmetic for the one affected test, not transcribed).** Current sequence:
  `LD A,0x2A` (7+1=8T), `INC A` (4+1=5T), `HALT` (4+1=5T, unaffected — this is the HALT OPCODE's own
  execution cost, a real M1 fetch, already correctly billed), then one MORE `step_cpu_instruction()` while
  already halted. Under C2 this becomes `4 (bare, unchanged per A-M23-1) + 1 (M1 wait, now correctly
  applied) = 5`, not the current `4`. New cumulative total: `8 + 5 + 5 + 5 = 23` (was `22`). *Verify:* QA
  independently re-derives this exact sum by hand before accepting the updated golden.
- **A-M23-5 (no OTHER non-M1, non-intrinsic-I/O wait-state source exists for this machine — a genuine,
  independently-derived scope-narrowing finding for C1, not an assumption of convenience).** Z80A
  fact-sheet §6 lists exactly three MSX-specific wait categories: (1) the M1 wait (closed M11; the
  HALT-refetch sub-case closes here via C2); (2) VDP-access recovery waits (D4's remainder, §2.4); (3) "the
  HB-F1XV has **no** CPU turbo mode" (N/A, §1.2). The Z80's OWN intrinsic I/O wait state ("the Z80
  automatically inserts one wait state (TW) between T2 and T3 of every I/O cycle... intrinsic to the Z80 and
  independent of the MSX M1 wait," fact-sheet §6) is already correctly present because this project's CPU
  core returns literal Zilog-datasheet T-state counts for every opcode (already QA-verified since M9/M12;
  e.g. `IN A,(n)`/`OUT (n),A` are 11T bare, which already bakes in the 4T I/O M-cycle vs. the 3T plain-memory
  M-cycle). *Verify:* spot-check `execute_unprefixed`'s `IN A,(n)`/`OUT (n),A` opcode handlers in
  `src/devices/cpu/z80a_cpu.cpp` return `11` (unchanged), confirming no hidden double-count risk.
- **A-M23-6 (the reference architecture proves access-slot modeling is genuinely additive — independently
  re-derived by reading the actual call sites, not assumed from the backlog's own wording).** Read
  `references/openmsx-21.0/src/video/VDP.cc:791-847` (the CPU `#98`-port write/read entry point) and
  `:849-888` (`executeCpuVramAccess`, the actual VRAM read/write) this cycle: the port handler does NOT
  execute the access immediately (except in an explicitly-disabled `allowTooFastAccess` escape hatch,
  `:800-813`); instead it computes `delta = isMSX1VDP() ? D28 : D16` (`:842-843`) and schedules a FUTURE
  sync point (`syncCpuVramAccess.setSyncPoint(getAccessSlot(time, delta))`, `:844`) — only when THAT sync
  point fires does `executeCpuVramAccess` run, and that function (`:849-888`) is a completely self-contained,
  atomic VRAM read/write + pointer-increment with **zero** access-slot bookkeeping inside it. This proves
  the reference's OWN architecture separates "compute how long to wait" from "perform the access," which
  means a genuine access-slot feature can be added as a NEW, small, self-contained wait-COST CALCULATOR
  consulted only by the CPU-stepping loop — `VdpFrameRenderer` (M21) and `VdpCommandEngine`/`SpriteEngine`
  (M22) need ZERO internal changes for this. *Verify:* confirm (by diff) that no file under
  `src/devices/video/vdp_frame_renderer.*`, `src/devices/video/vdp_command_engine.*`, or
  `src/devices/video/sprite_engine.*` is touched by this milestone.
- **A-M23-7 (the existing `kFrameCycles` constant already, silently, encodes the exact 1368-VDP-cycles/line
  × 1:6 CPU:VDP ratio fact — an independently-discovered cross-check, not previously documented anywhere in
  this project).** `src/machine/hbf1xv_machine.cpp:14`: `constexpr std::uint64_t kFrameCycles = 228 * 262;`
  (no explanatory comment in the source). Independently recomputed this cycle:
  `references/openmsx-21.0/src/video/VDP.hh:76` gives `TICKS_PER_LINE = 1368` (VDP cycles/line); the Z80A
  fact-sheet §1 and V9958 fact-sheet §1 both independently state the fixed 1:6 CPU:VDP clock ratio
  (21.477270 MHz ÷ 6 = 3.579545 MHz); `1368 / 6 = 228` exactly (no remainder) — matching the `228` factor
  verbatim; `262` is the documented NTSC total lines/frame (V9958 fact-sheet §7, "total 262 lines"). This
  independently PROVES `kFrameCycles` already encodes "228 CPU T-states per scanline × 262 scanlines," even
  though no per-line/per-scanline concept has ever been surfaced anywhere in this codebase before M23. This
  makes the new raster-position derivation (§3.2) a low-risk, well-grounded addition, not an invented ratio.
  *Verify:* a unit test asserts `kCpuTstatesPerLine * 262 == kFrameCycles` (228 × 262 == 59736) via a
  shared, exported constant rather than two independently-hardcoded literals drifting apart.
- **A-M23-8 (the "16-cycle CPU-request latency" and the fact-sheet's own "~29 Z80-cycle safe access" figure
  are two DIFFERENT, non-interchangeable hardware facts — a genuine risk this cycle's own grounding
  surfaced, R-M23-2).** `VDP.cc:842-844`'s `Delta::D16` is a FLOOR fed into `getAccessSlot()`, which then
  searches the mode-dependent slot table for the next ACTUAL slot at least 16 VDP cycles later — the true
  wait can be substantially longer depending on raster position and mode (e.g. the "screen off" table's
  slots are only 8 VDP-cycles apart in the best case but can leave gaps up to 44 cycles wide,
  `VDPAccessSlots.cc:14-35`, independently spot-checked: entries `112 → 164` skip 52 cycles). The V9958
  fact-sheet §2's independently-quoted **"~29 Z80 cycles between two accesses"** is a SEPARATE, coarser,
  SOFTWARE-DISCIPLINE convention MSX programmers use ("an OUT(#99),A instruction takes 12 cycles ... so you
  need 17 extra cycles") — it is NOT the literal mechanism openMSX implements internally, and the two
  numbers do not arithmetically reconcile via simple division (`16/6 ≈ 2.67T` rounds to 3T, nowhere near a
  29T total gap). Conflating them (e.g., presenting a single derived "the" wait-cost number) would
  misrepresent one documented fact as authoritative over the other. This milestone presents BOTH, clearly
  labeled and never combined into a single claimed value (§3.1/§3.3). *Verify:* the implementation's doc
  comments and tests keep these as two separately-named, separately-cited constants, never algebraically
  merged.

---

## 2. The Critical Scope-Resolution Section

### 2.1 Question 1 — is genuine, full access-slot modeling achievable this milestone without rewriting M21/M22?

**Answer: partially yes, partially no — and the "yes" and "no" parts are cleanly separable.**

The **mechanism of scheduling itself** (deciding HOW LONG a CPU-initiated VRAM port access should be delayed
before it takes effect) is genuinely additive, per A-M23-6: the reference's own `executeCpuVramAccess`
(`VDP.cc:849-888`) is a completely self-contained, atomic function with no access-slot state inside it — the
delay/scheduling logic lives entirely OUTSIDE it, in the port-write handler. This means a hypothetical
future milestone COULD add real CPU-wait-state gating to `Hbf1xvMachine::step_cpu_instruction()` (querying a
"how many extra T-states before this VRAM port access takes effect" calculator) **without requiring
`VdpFrameRenderer` or `VdpCommandEngine` themselves to become cycle-scheduled** — those devices would
continue to see a VRAM store that is simply, eventually, correctly written, exactly as today.

However, genuinely computing that delay number requires two things this project's architecture does not yet
have, and building them is real, non-trivial, dedicated-milestone-sized work, not a same-cycle addition:

1. **A raster-position tracking mechanism.** `getAccessSlot()`/`Calculator` (`VDPAccessSlots.hh:41-90`) need
   to know "how many VDP cycles into the CURRENT LINE are we, right now" to index the correct slot-table
   entry. This project's clock model advances in TWO incompatible granularities: fine-grained
   `step_cpu_instruction()` (per-instruction T-states) and whole-frame-atomic `run_frame()`
   (`scheduler_.tick(kFrameCycles)` in one call, `hbf1xv_machine.cpp:346-357`). A genuine per-scanline
   position is only derivable during the fine-grained path; `run_frame()`'s jump has no intermediate
   position to expose. This milestone builds the derivation utility (§3.2) but does not attempt to reconcile
   it with `run_frame()`'s atomic jump — that reconciliation is real, separate depth.
2. **The exact numeric slot tables.** `VDPAccessSlots.cc:14-141` defines five tables totaling roughly 340
   individual `int16_t` entries (154+17 screen-off, 88+16 sprites-off, 31+3 sprites-on, 31+3 char, 47+10
   text — each padded with a cyclic duplicate for wraparound, per the file's own comment at `:8-9`).
   Reproducing these EXACTLY requires careful, independent, line-by-line re-derivation (not transcription)
   to stay clearly on the "hardware fact, not copied code" side of the GPL-isolation guardrail — a large,
   exacting, multi-file undertaking properly scoped as its own milestone, not squeezed alongside C2's small
   CPU-core change and the elevated M9/M12-oracle risk this milestone already carries.

**Conclusion:** the SCHEDULING CONCEPT is additive and does not require touching M21/M22's devices; the
DATA (slot tables) and the FRAME-TIMING RECONCILIATION are large, separable, genuinely deferrable pieces.
M23 builds the additive architecture and the raster-position foundation; it explicitly does NOT build the
slot-table data or wire any actual gating.

### 2.2 Question 2 — what's genuinely, safely achievable this milestone?

- **C2 (HALT-R): closes cleanly, with one caveat surfaced, not hidden.** It is a small, well-scoped,
  CPU-core-only change (§1.3 A-M23-1/A-M23-2) — there is no architectural reason it can't close. The ONE
  thing worth flagging explicitly (this is new information, not previously recorded anywhere) is that
  "cleanly" does NOT mean "with zero visible change to any existing golden test" — it means "with exactly
  ONE deliberate, well-cited, independently-recomputed golden update, and zero incidental collateral damage
  to the other 116 tests" (proven via the exhaustive `.halted()` audit, A-M23-3). This is precisely the
  scenario DEC-0004 anticipated when it deferred C2 in the first place ("implementing it would alter the
  QA-signed hbf1xv_cpu_step timing oracle") — the prediction is now independently confirmed to be accurate
  down to the exact file and exact numbers, not merely plausible.
- **C1's wait-state-parity-beyond-M1 and D4's VDP-cycle-cost modeling: decompose into a safe, informational
  layer vs. risky, gating depth — exactly as the task suggests.** The task's own suggested example — "a
  VDP-side 'how many cycles would a real HMMV of this size cost' calculator that's purely
  informational/queryable, vs. actually gating the CPU's execution on it" — is precisely the right shape for
  what's safe here. §3 builds the informational/queryable half (raster-position derivation + the `Delta`
  cost-unit contract + two clearly-labeled, cited latency facts) and explicitly declines the gating half.

### 2.3 Question 3 — the safest possible thin slice, and why it's the RIGHT call

**The safest thin slice is: (a) close C2 in full — real, hardware-grounded, low-risk, well-isolated; and
(b) ship a genuinely useful, tested, but STRICTLY NON-GATING VDP access-timing foundation for C1/D4, leaving
the full slot-table depth and any actual CPU-timing gating explicitly OPEN and precisely named.**

This is the right call, not merely the easy one, for a reason this milestone's own investigation newly
surfaced (not previously known when M21/M22 were planned): **the M21/M22 system-test CPU-program fixtures
already exercise the exact scenario that would make naive gating dangerous.** Independently confirmed this
cycle by reading the fixture bytes:

- `tests/system/hbf1xv_m21_vdp_render_system_test.cpp:56-62`: three consecutive `LD A,n` / `OUT (#98),A`
  pairs (name/pattern/color table writes) with **zero spacer instructions** between the `OUT` of one pair
  and the `LD` that starts the next.
- `tests/system/hbf1xv_m22_sprites_command_engine_system_test.cpp:58-78`: the same back-to-back
  `LD A,n / OUT (#98),A` pattern, five times in a row, writing sprite Y/X/pattern/color/sentinel bytes.

If M23 were to actually GATE `step_cpu_instruction()` on even an approximate VDP-access-recovery-wait model,
these EXACT, already-shipped, QA-signed test programs would be the first things to break — either their
cumulative T-state totals would shift (breaking any timing assertion), or, more seriously, per D4's own
documented "too-fast access drops requests" behavior (V9958 fact-sheet §2: "Too-fast access drops requests"),
some of those back-to-back `OUT (#98),A` writes could be silently DROPPED, corrupting the sprite/tile
CONTENT those tests assert on — a correctness regression, not merely a timing one. This is a concrete,
demonstrated risk (not a hypothetical one), and it is exactly why real gating is named OUT of scope (§1.2)
rather than attempted "carefully." Mirroring this project's own established discipline (DEC-0005 backlog
disposition; the M14/M17/M18/M19/M20/M21/M22 "contract vs. depth" precedent; the D7/C5 "one tracked row,
carried forward, not force-closed" precedent from DEC-0011/DEC-0018/DEC-0019): M23 ships a real, tested,
honestly-scoped slice now, and names the remainder as backlog rows carried forward — not a fabricated "done"
claim beyond what was actually, safely built.

### 2.4 D4/C1 remainder — precisely named (for the backlog entry, §5)

Carried forward, explicitly, as the still-OPEN substance of both C1 and D4 (identical remainder for this
specific machine, per A-M23-5):

1. The five full per-mode/per-scanline slot tables (screen-off 154, sprites-off 88, sprites-on 31, char 31,
   text 47 — each independently re-derived/re-verified, not transcribed) and the raster-position
   reconciliation with `run_frame()`'s whole-frame-atomic jump (§2.1 item 1).
2. The CPU-access-steals-command-engine-slot interaction (fact-sheet §8).
3. The command engine's real per-pixel/per-line VDP-cycle cost (fact-sheet §8's 48/56/64/72/88-cycle table).
4. The exact sub-frame raster position of the line/VBlank IRQ (currently a whole-frame-boundary hook only,
   `on_vsync()`/`on_line_match()`, `src/devices/video/v9958_vdp.h:65-68`).
5. Any actual CPU-execution gating (gating `step_cpu_instruction()`'s returned/scheduled cycle count on VDP
   access timing) and the "too-fast dropped-request" behavior.

### 2.5 Summary answer for the coordinator

**M23 does NOT claim full closure of D4 or C1.** This is a deliberate, conservative, well-precedented scope
decision, not a failure to deliver: C2 closes cleanly and completely; C1 and D4 each advance to
`IN-PROGRESS (M23 partial)` with a real, tested, additive foundation shipped and a precisely-named remainder
carried forward for a future, dedicated milestone.

---

## 3. Spec Summary

### 3.1 `src/` placement (per `src/CLAUDE.md`)

| File | Responsibility | Grounding |
|---|---|---|
| `src/devices/cpu/z80a_cpu.cpp` (**edit**, surgical) | `Z80aCpu::step()`'s halted branch (`else if (state_.halted()) { tstates = 4; }`) additionally calls the EXISTING `increment_refresh_register()` before returning; `tstates` literal stays `4` (A-M23-1). No signature/API changes. | `references/openmsx-21.0/src/cpu/CPUCore.cc:2508-2511`, `CPUClock.hh:53-59`, `Z80.hh:19-21` |
| `src/devices/video/vdp_access_timing.h` (**new**, header-only, pure functions/constants) | `enum class VdpAccessDelta : int { D0=0, D1=1, D16=16, D24=24, D28=28, D32=32, D40=40, D48=48, D64=64, D72=72, D88=88, D104=104, D120=120, D128=128, D136=136 };` (the 15 named VDP-cycle offsets, independently re-derived from the enum's OWN labels, not the slot tables); `constexpr int kVdpCyclesPerLine = 1368;`; `constexpr int kCpuTstatesPerVdpCycleRatio = 6;`; `constexpr int kCpuTstatesPerLine = kVdpCyclesPerLine / kCpuTstatesPerVdpCycleRatio;` (=228, cross-checked A-M23-7); `constexpr int vdp_cycle_within_line(std::uint64_t cpu_tstates_since_frame_start);`; `constexpr int minimum_request_latency_tstates(VdpAccessDelta delta);` (a lower-bound-only, clearly-labeled `ceil(delta/6)` conversion of the V9938+ `Delta::D16` floor — NOT slot-table-refined); `constexpr int kDocumentedSafeAccessGapTStates = 29;` (the fact-sheet §2 "general consensus" figure, kept textually and numerically SEPARATE from the D16-derived value per A-M23-8, never combined). | `references/openmsx-21.0/src/video/VDPAccessSlots.hh:15-34`; `VDP.hh:76`; `VDP.cc:842-844`; V9958 fact-sheet §2/§7 |
| `src/machine/hbf1xv_machine.h` / `.cpp` (**edit**, additive-only) | New private field `std::uint64_t last_vsync_cycle_ = 0;`, updated with one added line inside the EXISTING `run_frame()` (`last_vsync_cycle_ = elapsed_cycles();` immediately before or after the existing `vdp_.on_vsync()` call). New public const accessor, e.g. `std::uint64_t cycles_since_last_vsync() const;` and `int vdp_cycle_position() const;` (thin wrapper calling `vdp_access_timing::vdp_cycle_within_line(cycles_since_last_vsync())`). **`step_cpu_instruction()`, `run_cycles()`, and the scheduler-tick arithmetic inside `run_frame()` are UNCHANGED** — this is the concrete, checkable "non-gating" boundary (§4). | New design, additive only; grounds in A-M23-7's cross-check |

Boundary compliance: `vdp_access_timing.h` carries no slot/bus/device knowledge (pure functions over
explicit parameters) and is placed under `src/devices/video/` per `src/CLAUDE.md` (device-family
timing/spec constants); the one `Hbf1xvMachine` addition is a thin, additive accessor mirroring the existing
`frame_count()`/`elapsed_cycles()` pattern — no frontend concerns, no new bus wiring.

`CMakeLists.txt`: add `src/devices/video/vdp_access_timing.h` to the header list if the build enumerates
headers (header-only; no new `.cpp` required unless the developer's judgment prefers a translation unit for
the constants, per `src/CLAUDE.md`'s "best judgment" latitude).

### 3.2 Raster-position derivation (the informational half)

```
cycles_since_vsync = elapsed_cycles() - last_vsync_cycle_          // CPU T-states
line_cycle_cpu     = cycles_since_vsync % kCpuTstatesPerLine        // 0..227 (CPU T-state granularity)
line_cycle_vdp     = line_cycle_cpu * kCpuTstatesPerVdpCycleRatio   // 0..1362 (VDP-cycle granularity, coarse)
```

Documented, explicit semantic: this position is **relative to the most recent `on_vsync()` call, or program
start (cycle 0) if `on_vsync()` has never fired** (true for every existing M21/M22 system test, which never
call `run_frame()` at all — they drive the CPU purely via `step_cpu_instruction()` loops). This is an honest,
tested boundary condition (§4), not an oversight.

### 3.3 The two latency facts, kept explicitly separate (A-M23-8)

| Constant | Value | What it actually means | Source |
|---|---|---|---|
| `minimum_request_latency_tstates(Delta::D16)` | `ceil(16/6) = 3` T | The V9938+/V9958 CPU-VRAM-access scheduler's FLOOR delta before it even starts searching the (not-yet-built) mode-dependent slot table — a lower bound, NOT the actual wait, which the full table would refine upward depending on raster position/mode. | `VDP.cc:842-844` |
| `kDocumentedSafeAccessGapTStates` | `29` T | The MSX-programmer "general consensus" software-discipline rule for spacing two `#98`/`#99` accesses apart — a coarser, independently-documented empirical convention, not the literal internal mechanism. | V9958 fact-sheet §2 |

Neither value is wired into any CPU-timing-affecting code path this cycle (§4).

### 3.4 openMSX A/B acceptance plan

**C2 (HALT-R): a genuine, feasible technique exists.** Drive a known program on both sides that executes
`HALT` and idles for N machine cycles, then reads the R register via the live WSL Tcl debugger. *Verify at
implementation time* (do not assume) whether openMSX's Tcl `debug` interface exposes a live R-register
readback comparable to the technique already established in `docs/m12-parity-trace-diff.md`/
`docs/m11-parity-trace-diff.md` (both already read/wrote CPU registers over the Tcl debugger successfully).
If confirmed available: drive an identical `HALT`-then-N-idle-cycles sequence on both engines and confirm R
increments by the identical count on both sides (adversarial self-check: an empty-side comparison →
BLOCKED, a corrupted R value → DIVERGENCE, per the established comparator discipline). If the specific
register readback is NOT confirmed available, report this specific probe honestly BLOCKED rather than
fabricating a workaround.

**D4/C1 VDP-access-wait numeric cost: explicitly BLOCKED, not attempted.** There is no established,
feasible technique in this project's methodology (M11-M22) for comparing "exactly how many extra wait
T-states did openMSX insert for this specific VRAM port access" via the live Tcl debugger — no side effect
of that wait is independently observable without dedicated instrumentation neither engine currently exposes
(unlike register/VRAM-content/PC comparisons, which ARE observable). Since this milestone does not gate any
CPU timing on this value anyway (§2.5), this sub-claim is honestly marked **BLOCKED / not attempted** rather
than fabricated — consistent with the task's explicit permission for this outcome and the M21 planner's own
precedent (`docs/m21-planner-package.md` §2.7, the computed-pixel-color BLOCKED disposition).

Mechanics: `tools/openmsx-m23-halt-r-parity.ps1` (new) + `docs/m23-parity-trace-diff.md` (new), following the
established WSL Tcl-debugger pattern.

---

## 4. Milestone Slice Plan (M23-S1 … S3)

Every slice runs the full evidence-gate set (§6) and leaves `ctest` green across the **entire M1-M22 suite
(117 tests)**, per the human's explicit "deliberate cross-check" directive — not just CPU/VDP-adjacent tests.

### M23-S1 — C2: HALT-R closes in full

- **Goal:** `Z80aCpu::step()`'s halted branch calls `increment_refresh_register()` (§3.1); the ONE affected
  golden test (`hbf1xv_cpu_step_integration_test.cpp`) is updated with the independently-recomputed values
  (§1.3 A-M23-4: `t3` 4→5, cumulative `elapsed_cycles()` 22→23), with an explicit code comment citing the
  `Z80.hh:19-21`/`CPUCore.cc:2508-2511` grounding and DEC-0004 by ID.
- **New unit tests** (`tests/unit/devices/z80a_halt_r_unit_test.cpp`): R increments by exactly 1 per halted
  `step()` call; R wraps `0x7F → 0x00` while bit 7 is preserved across the wrap (mirrors the EXISTING M12
  R-register low-7-bit test pattern); `Z80aCpu::step()` called directly (bypassing the machine) on an
  already-halted CPU still returns the bare `4` (A-M23-1's CPU-core invariant); an interrupt arriving while
  halted still transitions out via the UNCHANGED, already-QA-verified M12 interrupt-accept path (no new
  coupling).
- **Extended integration test**: extend (not replace) `hbf1xv_interrupt_ack_integration_test.cpp` or add a
  new dedicated case confirming several halted idle steps (R visibly incrementing, 5T each at the machine
  level) followed by an interrupt wake, with the SAME IM1-ack timing (14T) as already established in M12 —
  proving zero coupling between the HALT-R fix and the interrupt-accept timing.
- **Regression discipline (zero-tolerance):** `git diff` against the M22 tag (`v1.0.22`) must show changes
  ONLY in `src/devices/cpu/z80a_cpu.cpp` (production) and `tests/integration/machine/hbf1xv_cpu_step_integration_test.cpp`
  (the one deliberate golden update) plus the new test file(s) — no other file under `tests/` is touched.
  Named, explicitly-required-green suites (§6): `z80a_unprefixed_unit_test.cpp`, `z80a_trace_export_unit_test.cpp`,
  `hbf1xv_cpu_parity_integration_test.cpp`, `hbf1xv_m11_parity_checkpoint_integration_test.cpp`,
  `hbf1xv_m13_mem_parity_checkpoint_integration_test.cpp`, `hbf1xv_parity_checkpoint_integration_test.cpp`,
  `hbf1xv_indexed_program_integration_test.cpp`, `hbf1xv_cb_program_integration_test.cpp`,
  `hbf1xv_ldir_program_integration_test.cpp`, `hbf1xv_interrupt_ack_integration_test.cpp`.
- **Gates:** build + ctest green (full suite, 117 prior + new).

### M23-S2 — VDP access-timing foundation (non-gating)

- **Goal:** `vdp_access_timing.h` (Delta enum, raster-position derivation, the two separated latency
  constants); `Hbf1xvMachine::last_vsync_cycle_` + `cycles_since_last_vsync()`/`vdp_cycle_position()`
  accessors (additive-only edit to `run_frame()`).
- **Unit tests** (`tests/unit/devices/video_vdp_access_timing_unit_test.cpp`): `kCpuTstatesPerLine * 262 ==
  kFrameCycles` (A-M23-7's cross-check, via a shared constant, not two independently-hardcoded literals);
  `vdp_cycle_within_line` round-trips correctly across a full-line and full-frame boundary; `Delta` enum
  values match the cited constants exactly; `minimum_request_latency_tstates`/`kDocumentedSafeAccessGapTStates`
  are two DISTINCT, never-combined values (a dedicated test literally asserts they are not equal and are
  never summed/divided into each other anywhere in the implementation).
- **New integration test proving non-gating (the concrete acceptance bar, §2.3):** a test that (a) drives the
  EXACT M21 and M22 system-test CPU-program byte sequences (or calls into a shared fixture) through
  `step_cpu_instruction()` and asserts the cumulative T-state total is BYTE-IDENTICAL to the value produced
  before this slice (a literal before/after regression proof, not merely "still passes"); and (b) separately,
  in the SAME test, calls the new `vdp_cycle_position()`/`minimum_request_latency_tstates()` accessors and
  confirms they return sane, non-crashing values — proving the calculator exists and works AND that nothing
  currently consults it during real CPU stepping.
- **Regression discipline:** `git diff` against the M22 tag confirms `step_cpu_instruction()`, `run_cycles()`,
  and `run_until_cycle()` bodies are byte-for-byte unchanged (only `run_frame()` gains the one new
  bookkeeping line); no file under `src/devices/video/vdp_frame_renderer.*`,
  `src/devices/video/vdp_command_engine.*`, or `src/devices/video/sprite_engine.*` is touched (A-M23-6).
- **Gates:** build + ctest green (full suite).

### M23-S3 — Backlog/documentation closure, A/B evidence, evidence gates

- **Goal:** full 34-row backlog review (§5) written into `agent-protocol/state/deferred-backlog.md` (C2 →
  DONE (M23); C1/D4 → IN-PROGRESS (M23 partial), remainder precisely named per §2.4); C2 A/B evidence
  captured (`docs/m23-parity-trace-diff.md`, §3.4); implementation report (`docs/m23-implementation-report.md`).
- **Evidence gates:** `tools/validate-assets.ps1`; `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`;
  `cmake --build build --config Debug`; `ctest --test-dir build -C Debug --output-on-failure` (full 117 +
  new); `tools/openmsx-m23-halt-r-parity.ps1` → `docs/m23-parity-trace-diff.md` (C2 only; the D4/C1 numeric
  sub-claim stays BLOCKED, §3.4).
- **Gates:** all of the above green; QA sign-off required before closure (normal gate, pre-authorized
  release-decision step per the M21-M23 run).

---

## 5. Full Deferred-Backlog Review (all 34 rows re-affirmed)

Per DEC-0005, every planner package must consult `agent-protocol/state/deferred-backlog.md` in full and
restate every row. All 34 rows, one-line justification each:

**Section A (human-directive-tracked, 2026-07-06):**
- **B1** PSG/YM2149 internals — DONE (M15), re-affirmed unchanged; M23 touches no audio device.
- **B2** RTC/RP5C01 internals — DONE (M15), re-affirmed unchanged; M23 touches no RTC device.
- **B3** FM-PAC/YM2413 device — DONE (M17), re-affirmed unchanged; M23 touches no audio device.
- **B4** MSX-JE 16 KB SRAM — DONE (M20), re-affirmed unchanged; M23 touches no memory-mapper device.
- **B5** Kanji font ROM I/O — DONE (M18), re-affirmed unchanged; M23 touches no Kanji device.
- **B6** Halnote/MSX-JE firmware mapping — DONE (M20), re-affirmed unchanged; M23 touches no cartridge/mapper device.
- **B7** Cartridge loading — DONE (M19), re-affirmed unchanged; M23 touches no cartridge device.
- **B8** FDC drive mechanics — DONE (M16), re-affirmed unchanged; M23 touches no FDC device.
- **B9** VRAM/V9958 VDP contract — DONE (M14), re-affirmed unchanged; M23 adds a NEW, separate, non-gating
  access-timing helper alongside it, touching no VDP contract code.

**Section B (other known deferrals):**
- **C1** Exact cycle/T-state timing parity (wait-state parity beyond M1; VDP-access recovery waits) —
  **advances OPEN → IN-PROGRESS (M23 partial)**: the HALT-refetch M1-wait sub-item closes via C2; the
  VDP-access-recovery-wait remainder is identical to D4's remainder (§2.4), carried forward as ONE item
  (no other independent wait-state source exists for this machine, A-M23-5).
- **C2** Z80 HALT-R increment — **CLOSES: OPEN → DONE (M23)**, per §1.1/§2.2/§4 above.
- **C3** ZEXDOC/ZEXALL full sweep — re-affirmed OPEN; `references/zexall/` remains present, uncommitted;
  unrelated to this milestone's CPU-timing-only scope; still the indicative M24 owner.
- **C4** S1985 backup-RAM `.sram` persistence — DONE (M15), re-affirmed unchanged.
- **C5** Full boot past first device read — re-affirmed IN-PROGRESS (M16 partial); the real auto-disk-boot
  trigger investigation remains untouched by this CPU/VDP-timing-focused milestone.
- **C6** Peripherals (keyboard/joystick) — DONE (M15), re-affirmed unchanged.
- **C7** Printer + cassette — DONE (M18), re-affirmed unchanged.
- **C8** Sony speed-controller/PAUSE + Ren-Sha Turbo — re-affirmed OPEN; explicitly confirmed NOT the same
  thing as C1 (Z80A fact-sheet §6: "not a clock change... implemented as an autofire on the PAUSE button");
  unrelated to this milestone.
- **C9** SDL3 frontend — re-affirmed OPEN; unrelated to this milestone (still indicative M26).
- **C10** FDC flux-level/DMK fidelity — re-affirmed OPEN; unrelated to this milestone.

**Section C (M14 VDP-depth deferrals):**
- **D1** Pixel-accurate raster rendering pipeline — DONE (M21), re-affirmed unchanged; `VdpFrameRenderer`
  untouched this cycle (A-M23-6).
- **D2** Sprite rendering + collision — DONE (M22), re-affirmed unchanged; `SpriteEngine` untouched.
- **D3** VDP command engine — DONE (M22), re-affirmed unchanged; `VdpCommandEngine` untouched.
- **D4** Cycle-accurate VDP access-slot/command timing — **advances OPEN → IN-PROGRESS (M23 partial)**: the
  raster-position derivation + `Delta` cost-unit contract + two separated, cited latency facts close this
  cycle as a real, tested, non-gating foundation; the full slot tables, CPU-priority-steals-command-engine-
  slot interaction, command-engine per-pixel/per-line VDP-cycle cost, exact sub-frame IRQ raster position,
  and any actual CPU-execution gating remain explicitly OPEN and precisely named (§2.4), carried forward as
  ONE tracked row (mirrors the D7/C5 precedent — not force-closed, not split into a new letter).
- **D5** YJK/YJK+YAE color decode — DONE (M21), re-affirmed unchanged.
- **D6** Scroll/interlace/blink/superimpose — DONE (M21), re-affirmed unchanged.
- **D7** G6/G7 planar interleave — DONE (M22, closed in full), re-affirmed unchanged.

**Section D (M17 YM2413 depth deferrals):**
- **E1** YM2413 FM-synthesis DSP/audio depth — re-affirmed OPEN; unrelated to this milestone.
- **E2** YM2413 register-write timing constraint (≥12/84-cycle spacing) — re-affirmed OPEN; this is a
  DIFFERENT device's write-timing constraint (YM2413 `#7C/#7D`, not VDP `#98-#9B`) — NOT touched or
  subsumed by this milestone's VDP-focused C1/D4 work; explicitly noted here so a future planner does not
  assume M23 addressed it.

**Section E (M18 printer/cassette depth deferrals):**
- **F1** Cassette tape image-format/signal fidelity — re-affirmed OPEN; unrelated to this milestone.
- **F2** Printer image/ESC-sequence rendering depth — re-affirmed OPEN; unrelated to this milestone.

**Section F (M19 cartridge-mapper depth deferrals):**
- **G1** KonamiSCC + embedded SCC chip — re-affirmed OPEN; unrelated to this milestone.
- **G2** ROM-database/heuristic mapper auto-detection — re-affirmed OPEN; unrelated to this milestone.
- **G3** Full runtime cartridge hot-plug — re-affirmed OPEN; unrelated to this milestone.
- **G4** Long tail of ~80 other mapper types — re-affirmed OPEN; unrelated to this milestone.

---

## 6. Acceptance Criteria

1. **C2 closes in full**: R increments by exactly 1 (low 7 bits, bit 7 preserved) per halted `step()` call;
   `Z80aCpu::step()` continues to return the bare, unchanged datasheet T-state count (`4`) for the halted
   idle branch (A-M23-1's invariant, unit-tested directly against the CPU core bypassing the machine); the
   machine-level `step_cpu_instruction()` therefore reports `5` T-states for a halted idle step (4 + the
   existing, unmodified M1-wait formula) with **zero changes to `hbf1xv_machine.cpp`'s M1-wait arithmetic**.
2. **Exactly one existing test is deliberately updated**, with an explicit, cited justification in both the
   test file's own comment and the implementation report:
   `tests/integration/machine/hbf1xv_cpu_step_integration_test.cpp` — `t3` 4→5, final `elapsed_cycles()`
   22→23 (A-M23-4's independently-recomputed arithmetic: `8+5+5+5=23`). No other test file among the 22
   files matching `\.halted\(\)` is modified (A-M23-3) unless a fresh re-run of that grep at implementation
   time surfaces a genuinely new site (which must then be reported and justified, not silently patched).
3. **Full regression: the ENTIRE M1-M22 suite (117 tests) plus new M23 tests pass, 100%, zero unexplained
   regressions**, independently reproduced by both the developer and QA from a clean rebuild.
4. **The M9/M12 CPU-timing-oracle suites are a NAMED, zero-tolerance gate** (not merely "part of the full
   suite"): `z80a_unprefixed_unit_test.cpp`, `z80a_trace_export_unit_test.cpp`,
   `hbf1xv_cpu_parity_integration_test.cpp`, `hbf1xv_m11_parity_checkpoint_integration_test.cpp`,
   `hbf1xv_m13_mem_parity_checkpoint_integration_test.cpp`, `hbf1xv_parity_checkpoint_integration_test.cpp`,
   `hbf1xv_indexed_program_integration_test.cpp`, `hbf1xv_cb_program_integration_test.cpp`,
   `hbf1xv_ldir_program_integration_test.cpp`, `hbf1xv_interrupt_ack_integration_test.cpp` must all pass
   **with a `git diff` against the `v1.0.22` tag showing ZERO changes to any of these specific files.** QA
   must independently confirm this via its own diff, not merely trust the developer's report.
5. **The VDP access-timing foundation is non-gating, provably so**: a dedicated regression test re-runs the
   M21/M22 system-test CPU-program fixtures and asserts byte-identical cumulative T-state totals to their
   pre-M23 values; `git diff` against `v1.0.22` confirms `step_cpu_instruction()`, `run_cycles()`, and
   `run_until_cycle()` are unchanged (only `run_frame()` gains one bookkeeping line), and no file under
   `vdp_frame_renderer.*`, `vdp_command_engine.*`, or `sprite_engine.*` is touched.
6. **`kCpuTstatesPerLine * 262 == kFrameCycles`** is asserted by a dedicated unit test via a SHARED constant
   (not two independently-hardcoded literals), proving A-M23-7's cross-check is enforced, not merely stated.
7. **The two latency facts stay separated**: a dedicated unit test asserts
   `minimum_request_latency_tstates(Delta::D16) != kDocumentedSafeAccessGapTStates` and that no
   implementation file combines them arithmetically.
8. **Full 34-row deferred-backlog review** completed and written into `deferred-backlog.md` per §5 (C2 →
   DONE (M23); C1/D4 → IN-PROGRESS (M23 partial) with the exact remainder named; all other 31 rows
   re-affirmed with a one-line justification, no silent drift).
9. **Real openMSX A/B evidence for C2** captured in `docs/m23-parity-trace-diff.md`, using a genuine,
   implementation-time-verified technique (§3.4) — if the specific R-register Tcl readback is confirmed
   unavailable, this is honestly reported BLOCKED, not fabricated. The D4/C1 numeric wait-cost sub-claim is
   explicitly, honestly marked BLOCKED / not attempted (no feasible technique exists; no gating exists to
   even compare).
10. **Evidence gates executed and captured**: `tools/validate-assets.ps1`;
    `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`; `cmake --build build --config Debug`;
    `ctest --test-dir build -C Debug --output-on-failure` (full suite); conditional
    `tools/openmsx-m23-halt-r-parity.ps1` → `docs/m23-parity-trace-diff.md`.
11. **No scope beyond §1.1/§1.2 is implemented** without a new `agent-protocol/channels/decisions.md` entry
    (guardrails Scope Control) — in particular, no actual CPU-execution gating on VDP access timing, no
    slot-table data, and no touch to `VdpFrameRenderer`/`VdpCommandEngine`/`SpriteEngine`.

---

## 7. Risks (each with a verification action)

- **R-M23-1 (the one deliberate golden update is mis-recomputed or silently mis-applied).**
  *Verification:* QA independently re-derives `8+5+5+5=23` and `t3=5` by hand (not by re-running the test
  and trusting green) before accepting the change; confirms the diff touches ONLY the golden literal and an
  explanatory comment, not the test's assertion structure/case names.
- **R-M23-2 (a `.halted()`-using test site not caught by this cycle's audit exists or is added between
  planning and implementation).** *Verification:* developer re-runs `rg "\.halted\(\)" tests/` fresh at
  implementation start and diffs the result against this package's enumerated 22-file list (§1.3 A-M23-3);
  any new match must be individually classified (safe stop-at-boundary pattern vs. risk) and reported. QA
  independently re-runs the same command before sign-off.
- **R-M23-3 (a future contributor mistakes the informational access-timing calculator for a signal that it
  is safe to wire into real CPU gating, given the M21/M22 fixture risk demonstrated in §2.3).**
  *Verification:* the implementation report and the new header's own doc comments explicitly state, in
  writing, that wiring this into `step_cpu_instruction()` requires FIRST re-validating the M21/M22 system-
  test fixtures (cited by file/line) against real gating/drop behavior — a forward-looking warning, not
  merely an omission.
- **R-M23-4 (license-isolation risk if a future or over-eager implementation reproduces the openMSX slot
  tables verbatim to "finish" D4 within this milestone).** *Verification:* this milestone's diff contains no
  large (>20-entry) numeric array under `src/`; the only numeric constants added are the 15 named `Delta`
  values, `kCpuTstatesPerLine=228`, `kVdpCyclesPerLine=1368`, and the two labeled latency facts (16→3T floor,
  29T convention) — each independently re-derivable from the fact-sheet's prose, not the openMSX array
  literals.
- **R-M23-5 (the `last_vsync_cycle_`/raster-position semantic is ambiguous or silently wrong when
  `run_frame()` is never called — true for every existing M21/M22 system test).** *Verification:* a
  dedicated unit test exercises exactly the "no `on_vsync()` yet, position relative to program start"
  case and asserts the documented behavior, not a crash or an undefined value.
- **R-M23-6 (the A/B technique for C2 assumes a live R-register Tcl readback that may not actually be
  exposed by the installed openMSX build).** *Verification:* implementation-time live WSL check performed
  BEFORE claiming the technique works; if unavailable, the probe is marked BLOCKED in
  `docs/m23-parity-trace-diff.md`, not silently skipped or fabricated.
- **R-M23-7 (perception risk: narrowing D4/C1 could be read as under-delivering against the backlog's
  literal wording).** *Verification:* this package's §2 documents the conservative decision with concrete,
  cited, re-derived justification (the demonstrated M21/M22 fixture risk, the license-isolation concern, the
  raster-position/whole-frame-atomic architectural gap) — available for the coordinator/human to review
  and, if they judge otherwise, override via a `decisions.md` entry before implementation proceeds.
- **R-M23-8 (E2, the YM2413 write-timing constraint, could be mistakenly assumed closed alongside C1/D4
  since it is also a "timing" backlog row grouped near C1 in the backlog file).** *Verification:* §5
  explicitly re-affirms E2 as untouched, unrelated OPEN scope — a different device, a different port range,
  not addressed by this milestone.

---

## 8. Developer Handoff

**Build order:** M23-S1 (C2) first — it is small, self-contained, and fully closes one backlog row cleanly.
M23-S2 (VDP access-timing foundation) second — strictly additive, no coupling to S1. M23-S3
(backlog/documentation/evidence) last.

**Hard constraints (do not deviate without a `decisions.md` entry):**
- Do NOT touch `VdpFrameRenderer`, `VdpCommandEngine`, or `SpriteEngine` (M21/M22, already QA-signed).
- Do NOT wire the new `vdp_access_timing.h` calculator into `step_cpu_instruction()`, `run_cycles()`, or
  `run_until_cycle()` — it must remain purely queryable/informational this cycle (§2.3/§4/§6 item 5).
- Do NOT reproduce the openMSX slot-table arrays (`VDPAccessSlots.cc:14-141`) under `src/` this cycle.
- The ONLY existing test file permitted to change is
  `tests/integration/machine/hbf1xv_cpu_step_integration_test.cpp` (golden update, §4/§6 item 2) — verify via
  a fresh `.halted()` grep before touching anything else.
- Run the FULL M1-M22 suite (117 tests), not a subset, before and after each slice; name the M9/M12 oracle
  suites explicitly in the implementation report's evidence section (§6 item 4).

**Files to create:** `src/devices/video/vdp_access_timing.h`,
`tests/unit/devices/z80a_halt_r_unit_test.cpp`, `tests/unit/devices/video_vdp_access_timing_unit_test.cpp`,
a new or extended integration test proving non-gating (§4 M23-S2), `tools/openmsx-m23-halt-r-parity.ps1`,
`docs/m23-implementation-report.md`, `docs/m23-parity-trace-diff.md`.

**Files to edit (surgical, additive only):** `src/devices/cpu/z80a_cpu.cpp` (one branch),
`src/machine/hbf1xv_machine.h`/`.cpp` (one field + one accessor + one line in `run_frame()`),
`tests/integration/machine/hbf1xv_cpu_step_integration_test.cpp` (the one deliberate golden update),
`tests/CMakeLists.txt` (register new test executables), `agent-protocol/state/deferred-backlog.md` (§5
dispositions), `agent-protocol/state/milestones.md`, `agent-protocol/state/definition-of-done.yaml`,
`agent-protocol/state/current-phase.md`.

**Evidence to capture before requesting QA:** full `ctest` output (117 + new, 0 failed); a `git diff
v1.0.22..HEAD -- tests/` review confirming only the named files changed; the C2 A/B trace-diff (or its
honest BLOCKED disposition); the four asset/build/test evidence-gate script outputs.
