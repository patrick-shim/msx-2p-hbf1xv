# M23 QA Sign-off — Exact Cycle Timing: Z80 HALT-R Closure (C2) + VDP Access-Timing Foundation (C1/D4, narrowed)

- Milestone ID: M23
- QA Owner: MSX QA Agent
- Developer package reviewed: `docs/m23-planner-package.md`, `docs/m23-implementation-report.md`,
  `docs/m23-parity-trace-diff.md`
- Baseline: `v1.0.22` (M22 closed, 117/117 tests)
- This is the final milestone of the pre-authorized M21-M23 run
  (`agent-protocol/state/current-phase.md`); this sign-off is the gate the coordinator's
  release-decision/tag step is conditioned on.

All findings below are from artifacts I read directly and commands I ran myself this cycle (clean
rebuild, `ctest`, `git diff` against the `v1.0.22` tag, a fresh `rg` audit, hand-recomputed
arithmetic, an independent WSL/openMSX 19.1 re-run, and two additional exploratory openMSX Tcl
probes of my own). No developer claim was accepted without independent reproduction.

---

## 1. Regression Scope

- **CPU core** (`src/devices/cpu/z80a_cpu.cpp`, `Z80aCpu::step()`'s halted branch) — the single
  highest-risk surface touched this cycle, directly adjacent to the M9/M12 CPU-timing-oracle
  zero-tolerance gate.
- **Machine layer** (`src/machine/hbf1xv_machine.{h,cpp}`) — one bookkeeping line added to
  `run_frame()`, two new accessors; `step_cpu_instruction()`/`run_cycles()`/`run_until_cycle()`
  claimed byte-for-byte unchanged.
- **New VDP access-timing header** (`src/devices/video/vdp_access_timing.h`) — pure,
  parameter-only functions/constants, claimed non-gating (consulted by nothing in the CPU-stepping
  path).
- **CLI tool** (`src/main.cpp`) — an additive, default-`0`, backward-compatible extension of the
  existing `--parity-trace` mode, not itemized in the planner's own file list (disclosed by the
  developer).
- **One deliberate golden update**
  (`tests/integration/machine/hbf1xv_cpu_step_integration_test.cpp`).
- **4 new test executables** (2 unit, 2 integration) plus `tests/CMakeLists.txt` registration.
- **Full regression suite**: the entire M1-M22 corpus (117 tests) plus the 4 new M23 tests (121
  total).
- **openMSX 19.1/WSL A/B evidence** for backlog C2, independently re-run by QA (not merely read).

---

## 2. Regression Matrix Status

| Area | Verification performed by QA | Result |
|---|---|---|
| Clean rebuild | `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` then `cmake --build build --config Debug` | Build succeeded, zero errors (only pre-existing C4819 code-page warnings) |
| Full test suite | `ctest --test-dir build -C Debug --output-on-failure` | **121/121 passed, 0 failed**, 2.87s — matches the developer's claimed 117-prior + 4-new, zero regression |
| Zero-tolerance oracle files (10 named test files + 6 named VDP-device files = 16 paths total; the task brief's own count label said "13" but enumerated 16 concrete paths — I checked every one of the 16 listed) | `git diff v1.0.22 -- <each path>` run individually for all 16 | **Every single one produced literally zero diff output** (confirmed by direct `wc -l` == 0 on each, not just "no error") |
| `.halted()` site audit | `rg -l "\.halted\(\)" tests/` (fresh, independent run) | 25 files matched. 22 are the planner's original pre-M23 set; the 3 new files are the M23 test files that legitimately step-while-halted to exercise the HALT-R fix itself (`z80a_halt_r_unit_test.cpp`, `hbf1xv_m23_halt_r_integration_test.cpp`, `hbf1xv_m23_vdp_access_timing_non_gating_integration_test.cpp`). 25 − 3 = 22, exactly matching the planner's audit; combined with the `git diff v1.0.22 --stat -- tests/` result (only `CMakeLists.txt` + the one golden file touched under `tests/`), this independently proves no OTHER pre-existing `.halted()`-using test file was modified. |
| Golden arithmetic re-derivation | Hand-recomputed independently (not trusted from the report) | `LD A,0x2A`=8T, `INC A`=5T, `HALT`=5T (all unaffected, datasheet+1 M1 wait), halted-idle step = 4(bare)+1(M1 wait, newly applied)=**5T** (was 4T). Cumulative: `8+5+5+5=23` (was 22). Matches `t3==5`/`elapsed_cycles()==23` in the updated test exactly. |
| Non-gating fixture goldens (6 captured totals in the new integration test) | Independently re-derived by hand from the verbatim-copied M21/M22 system-test byte sequences (cross-read against `tests/system/hbf1xv_m21_vdp_render_system_test.cpp` and `tests/system/hbf1xv_m22_sprites_command_engine_system_test.cpp` to confirm the bytes are genuinely identical, not just claimed) | GRAPHIC1: 9×`LD`(8T)+9×`OUT`(12T)+`HALT`(5T)=**185** ✓; GRAPHIC4: 5×8+5×12+5=**105** ✓; GRAPHIC6: 6×8+6×12+5=**125** ✓; YJK: 10×8+10×12+5=**205** ✓; M22 sprites: 22×8+22×12+5=**445** ✓; M22 command engine: 26×8+26×12+5=**525** ✓. All six captured constants independently reproduced by hand, byte sequences confirmed verbatim against the source system tests. |
| CPU-core grounding citations | Read directly: `references/openmsx-21.0/src/cpu/Z80.hh:19-21`, `CPUClock.hh:53-59`, `CPUCore.cc:2508-2511` | `WAIT_CYCLES=1`, `HALT_STATES = 4 + WAIT_CYCLES` (=5) confirmed at `Z80.hh:20-21`. `advanceHalt(hltStates, time)`: `halts = ceil(ticks/hltStates)`, `clock += halts*hltStates`, returns `halts` — confirmed at `CPUClock.hh:56-58`. Caller `incR(narrow_cast<uint8_t>(T::advanceHalt(T::HALT_STATES, scheduler.getNext())))` confirmed at `CPUCore.cc:2510` — the SAME `halts` value drives both the R increment and the clock advance, exactly as claimed. |
| CPU-core fix itself | Read `src/devices/cpu/z80a_cpu.cpp:58-75` directly | The halted branch calls the pre-existing `increment_refresh_register()` (line 74) then still returns bare `tstates=4` (line 75) — matches A-M23-1's invariant exactly; confirmed via the unit test's direct-CPU-bypass-machine case (`HaltedStep_BypassMachine_ReturnsBareFourAndOneM1`, asserting `t==4 && m1_cycles_last_step()==1`). |
| `increment_refresh_register()` correctness | Read `z80a_cpu.cpp:222-231` | Preserves bit 7, increments low 7 bits with wraparound (`(r+1)&0x7F`) — matches the existing M12 pattern; unit-tested for the `0x7F→0x00` wrap case. |
| VDP access-timing header purity/determinism | Read `src/devices/video/vdp_access_timing.h` in full (148 lines) | All functions are `constexpr`, operate only on explicit parameters (no clock/global state); the 15 `VdpAccessDelta` values and the two latency constants are the only numeric additions — no large array (R-M23-4 risk avoided, confirmed: file is 148 lines total, only named enum + 2 scalar constants). |
| Non-gating boundary | Read `src/machine/hbf1xv_machine.cpp:346-362` (`run_frame()`) and diffed `step_cpu_instruction()`/`run_cycles()`/`run_until_cycle()` | `run_frame()` gains exactly the one claimed line (`last_vsync_cycle_ = elapsed_cycles();`) plus the pre-existing `on_vsync()`/`++frame_count_` lines; `step_cpu_instruction()` (`:384-422`), `run_cycles()` (`:370-372`), `run_until_cycle()` (`:374-382`) contain **zero** reference to `vdp_access_timing`/`cycles_since_last_vsync`/`vdp_cycle_position` — confirmed by direct read, not just trusting the stat diff. |
| `main.cpp` CLI extension | `git diff v1.0.22 -- src/main.cpp` (full diff read, not just stat) | Confirmed: `halt_idle_extra_steps` is an optional trailing parameter defaulting to `0`; the pre-existing stepping loop (`while (steps < max_steps && !machine.cpu().state().halted())`) is untouched; the new code only executes when the caller explicitly passes a non-zero value AND the CPU is already halted. Byte-identical behavior for every pre-M23 invocation. |
| Deferred-backlog / 34-row review | Read `agent-protocol/state/deferred-backlog.md` in full | C2 disposed "ready to close DONE (M23)"; C1/D4 disposed "ready to advance OPEN → IN-PROGRESS (M23 partial)" with the exact 5-item remainder named verbatim (full slot tables + raster/`run_frame()` reconciliation; CPU-steals-command-engine-slot interaction; command-engine per-pixel/per-line VDP-cycle cost; exact sub-frame IRQ raster position; actual CPU-execution gating) — matches planner package §2.4 exactly, word-for-word. All other 31 rows re-affirmed, no silent drift observed. Status-column flip explicitly and correctly left un-flipped, pending this sign-off. |
| Evidence-gate scripts | Ran myself: `tools/validate-assets.ps1` | `Asset validation result: True`, 7 BIOS files + 2 ROM files present — matches the developer's claim. |

---

## 3. Failures and Risk Ranking

**No test failures. No blocking defects found.** Two non-blocking, documentation-precision items
were independently surfaced by QA (in the spirit of the M22/DEC-0019 precedent of QA going further
than the developer's own narrative):

### Risk 1 (Low, procedural) — `main.cpp` scope note has no `decisions.md` entry yet

The developer's own report (§6, Known Issues) explicitly defers the `decisions.md` ratification to
the coordinator. I independently confirmed the change itself is safe (see Scrutiny Item 2 below),
but per CLAUDE.md's "scope changes require an entry in `agent-protocol/channels/decisions.md`" and
the guardrails' Scope Control section, this should be explicitly recorded — most naturally folded
into the same closure decision entry the coordinator writes for M23 (mirroring how DEC-0018/DEC-0019
absorbed similar in-cycle findings). This is a paperwork completeness item, not a code defect, and
does not block sign-off.

### Risk 2 (Low, non-blocking) — the A/B divergence's specific causal framing is more precise than "wall-clock time between Tcl round-trips" alone

See Scrutiny Item 1 below for the full independent analysis. The bottom-line conclusion (divergence
is a live-openMSX-session scheduling artifact, not a defect in this project's own R-register logic)
is sound and independently corroborated. The specific mechanism I found is more precisely
"emulated-time state accumulated during the unthrottled pre-measurement boot window," not literally
"real/wall-clock delay between individual `step()` round-trips" as the developer's report frames it.
Recommend recording this refinement as a forward-looking watch-item (same treatment M22's
divergence-narrative correction received under DEC-0019), not a blocker.

---

## 4. Required Fixes

**None required before sign-off.** Recommended, non-blocking follow-ups for the coordinator at
closure:

1. Add a `decisions.md` entry (or fold into the M23 closure decision) explicitly ratifying the
   `src/main.cpp` `--parity-trace` `halt_idle_extra_steps` extension as an accepted, disclosed,
   in-scope tooling addition (Risk 1 above).
2. Record the refined causal framing for the C2 A/B divergence (Risk 2 above / Scrutiny Item 1) as
   a forward-looking watch-item in `agent-protocol/state/current-phase.md`'s standing watch-items
   list, alongside the existing M22-originated ones.

Neither action requires new code, a new test, or a re-run of any gate.

---

## 5. Sign-off Decision: **Pass**

---

## Independent Verification Detail

### A. Full-suite rebuild and test run (directly observed, not trusted from the report)

```
cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF
cmake --build build --config Debug          -> build succeeded, zero errors
ctest --test-dir build -C Debug --output-on-failure
  ...
  100% tests passed, 0 tests failed out of 121
  Total Test time (real) = 2.87 sec
```

This matches the developer's claimed 117 (M1-M22) + 4 (M23 new) = 121, zero regression.

### B. Zero-diff check on all 16 named zero-tolerance files (ran myself, not delegated)

`git diff v1.0.22 -- <path>` for each of the 10 named CPU-timing-oracle test files
(`z80a_unprefixed_unit_test.cpp`, `z80a_trace_export_unit_test.cpp`,
`hbf1xv_cpu_parity_integration_test.cpp`, `hbf1xv_m11_parity_checkpoint_integration_test.cpp`,
`hbf1xv_m13_mem_parity_checkpoint_integration_test.cpp`,
`hbf1xv_parity_checkpoint_integration_test.cpp`, `hbf1xv_indexed_program_integration_test.cpp`,
`hbf1xv_cb_program_integration_test.cpp`, `hbf1xv_ldir_program_integration_test.cpp`,
`hbf1xv_interrupt_ack_integration_test.cpp`) plus the 6 named VDP-device files
(`vdp_frame_renderer.h`, `vdp_frame_renderer.cpp`, `vdp_command_engine.h`, `vdp_command_engine.cpp`,
`sprite_engine.h`, `sprite_engine.cpp`) — **every single one produced zero lines of diff output**.
This is the single most important regression-safety confirmation for this milestone, and I did not
accept the developer's or coordinator's report of it — I ran it myself against the `v1.0.22` tag.

### C. `.halted()` audit (ran myself)

`rg -l "\.halted\(\)" tests/` returned 25 files. Three are brand-new M23 test files that
legitimately step past the halt boundary to exercise the HALT-R fix itself
(`z80a_halt_r_unit_test.cpp`, `hbf1xv_m23_halt_r_integration_test.cpp`,
`hbf1xv_m23_vdp_access_timing_non_gating_integration_test.cpp` — this last one steps only in its
"non-gating" section which never crosses the halt boundary in the fixture-replay portion; it is
included here only because the file matched the grep pattern in an unrelated helper). The remaining
22 exactly match the planner's own enumerated pre-M23 list. Cross-checked against
`git diff v1.0.22 --stat -- tests/`, which shows only `tests/CMakeLists.txt` (additive test
registration) and `hbf1xv_cpu_step_integration_test.cpp` (the one deliberate golden) touched under
`tests/` — proving none of the other 21 pre-existing `.halted()`-using files was modified.

### D. Golden arithmetic (hand re-derived, not re-run-and-trusted)

`LD A,0x2A` = 7 (datasheet) + 1 (M1 wait) = 8T. `INC A` = 4+1 = 5T. `HALT` = 4+1 = 5T (its own real
M1 fetch, unaffected by this milestone). The halted-idle step: CPU core still returns bare `4`
(confirmed by reading `z80a_cpu.cpp:74-75` directly — `increment_refresh_register(); tstates = 4;`),
and the machine-level `step_cpu_instruction()` formula (`hbf1xv_machine.cpp:404-407`,
UNCHANGED this cycle) adds `m1_wait = s1985_engine_.m1_wait_tstates(cpu_.m1_cycles_last_step())`
= 1 (since `increment_refresh_register()` now increments `m1_cycle_count_` even in the halted
branch) → `4 + 1 = 5`. Cumulative: `8 + 5 + 5 + 5 = 23`. This matches
`tests/integration/machine/hbf1xv_cpu_step_integration_test.cpp`'s updated `t3==5` and
`elapsed_cycles()==23` assertions exactly.

I additionally hand-verified all six "non-gating" fixture golden totals
(`hbf1xv_m23_vdp_access_timing_non_gating_integration_test.cpp`) by (1) confirming the copied byte
arrays are genuinely byte-for-byte identical to the live system-test source files
(`tests/system/hbf1xv_m21_vdp_render_system_test.cpp`, `tests/system/hbf1xv_m22_sprites_command_engine_system_test.cpp`)
and (2) independently tallying each program's `LD A,n`/`OUT (n),A`/`HALT` instruction counts and
multiplying by 8T/12T/5T respectively: GRAPHIC1 (9 LD + 9 OUT + 1 HALT) = 72+108+5=**185**; GRAPHIC4
(5+5+1) = 40+60+5=**105**; GRAPHIC6 (6+6+1) = 48+72+5=**125**; YJK (10+10+1) = 80+120+5=**205**;
M22 sprites (22+22+1) = 176+264+5=**445**; M22 command engine (26+26+1) = 208+312+5=**525**. All six
match the captured constants in the test file exactly — strong, independently-derived confirmation
that the new access-timing calculator is genuinely inert for every one of these fixtures.

### E. Reference-source grounding (read directly, not cited from memory)

- `references/openmsx-21.0/src/cpu/Z80.hh:19-21`: `WAIT_CYCLES = 1`; `HALT_STATES = 4 + WAIT_CYCLES`
  (=5). Confirmed.
- `references/openmsx-21.0/src/cpu/CPUClock.hh:53-59` (`advanceHalt`): `ticks = clock.getTicksTillUp(time)`;
  `halts = (ticks + hltStates - 1) / hltStates` (ceiling division); `clock += halts * hltStates`;
  returns `halts`. Confirmed.
- `references/openmsx-21.0/src/cpu/CPUCore.cc:2508-2511`: `} else if (getHALT()) { incR(narrow_cast<uint8_t>(T::advanceHalt(T::HALT_STATES, scheduler.getNext()))); setSlowInstructions(); }`.
  Confirmed: the SAME `halts` return value both increments R and advances the clock — this is the
  exact grounding basis for both the developer's production fix and their A/B-divergence
  root-cause hypothesis.

---

## Scrutiny Item 1 — Independent assessment of the A/B divergence hypothesis

I read `docs/m23-parity-trace-diff.md` directly, then **independently re-ran the actual harness
myself** (`tools/openmsx-m23-halt-r-parity.ps1`, live WSL openMSX 19.1) rather than trusting the
captured artifact, and additionally ran two of my own exploratory Tcl probes against openMSX to
test the hypothesis further.

**What I confirmed by direct re-run:** my independent execution reproduced the reported result
**byte-for-byte identically** — R diverges at seq=3/4/5 to exactly `0x2B`/`0x21`/`0x23`, matching the
committed artifact exactly, with seq 0-2 matching this project's engine exactly (R=00/01/02 on both
sides).

**(a) Is the hypothesis internally consistent with `CPUClock.hh`'s actual `advanceHalt`
mechanism?** Yes, in its core mechanism claim: `advanceHalt` genuinely computes a ceiling-division
bulk jump toward `scheduler.getNext()` (the next scheduled sync point), not a fixed one-quantum
step, so a single Tcl `step()` call CAN legitimately advance R by far more than 1 in one call. This
part of the grounding is accurate and I confirm it independently from the source.

**(b) Does "divergence onset tracks real/wall-clock time, not step count" support the developer's
specific causal framing over a defect?** The onset-independent-of-total-step-count observation
(confirmed at HaltIdleSteps=2/3/5 per the report) does support "not a step-count-indexed defect" —
agreed. However, I went further and ran two additional experiments the report did not attempt:

1. **Exact re-run reproducibility**: an independent process re-run, on a different invocation, at a
   possibly different moment and host load, produced the **identical** R values (`0x2B`/`0x21`/`0x23`)
   as the original capture. If the mechanism were genuinely sensitive to unpredictable real/wall-clock
   latency between individual PowerShell/WSL/openMSX round-trips (which should vary somewhat between
   independent invocations on a general-purpose host), I would expect at least minor variation in the
   resulting values run-to-run. Getting an *exact* match is evidence the phenomenon is actually
   **deterministic** for a fixed script (same `BootSeconds`, same step count), not noise-driven.
2. **Boot-window sensitivity probe**: I built a variant Tcl script that shortens the pre-measurement
   "boot" wait (`after time N` before the debug-break/HALT-loop begins) from the harness's default 6
   seconds down to 1 second. This produced a **categorically different** divergence pattern — R
   jumped immediately at seq=1 (to `0x1B`) rather than holding at seq=0-2 before jumping at seq=3.
   This shows the divergence onset is governed by how much *emulated* time has already elapsed via
   the unthrottled pre-measurement window (itself a function of `BootSeconds` real time under
   `throttle off`), which determines where the CPU's clock lands relative to openMSX's own internal,
   periodic device-scheduling queue (VDP/PSG/etc. sync points) — **not** by the real-time latency of
   each individual `step()` Tcl round-trip during the measurement loop itself, which is the developer
   report's literal framing ("real/wall-clock elapsed time between Tcl round-trips").

**(c) My own conclusion:** the hypothesis is **honest and not papering over a real bug** — the
bottom-line disposition (this is a property of the live openMSX reference session's own
`advanceHalt`/scheduler behavior while halted, not a defect in either engine's R-register
correctness) is well-grounded and independently corroborated by my own re-run and probes. The
project's own closure claim for C2 correctly rests on the deterministic unit/integration tests and
direct source citation, not on this live-session probe, which is the right evidentiary posture.
However, the **specific causal mechanism named** ("wall-clock elapsed time between round-trips") is
imprecise — my evidence points to it being the amount of **emulated time accumulated during the
unthrottled boot window before measurement begins** (a deterministic function of scheduler state at
the moment of the debug-break), not literal per-`step()`-call round-trip jitter. This is a
documentation-precision refinement, not a defect, and I recommend it be captured as a follow-up
watch-item (mirroring the M22/DEC-0019 precedent where QA refined rather than rejected the
developer's divergence narrative). It does not change the sign-off outcome.

## Scrutiny Item 2 — Independent assessment of the `main.cpp` CLI extension

I read the full `git diff v1.0.22 -- src/main.cpp` directly (not the implementation report's
summary). Findings:

- `run_parity_trace()` gains one new trailing parameter, `halt_idle_extra_steps`, defaulting to `0`.
- The pre-existing stepping loop (`while (steps < max_steps && !machine.cpu().state().halted())`)
  is **completely unchanged** — confirmed by diff, no lines touched inside that loop.
- The new behavior is gated by `if (halt_idle_extra_steps > 0 && machine.cpu().state().halted())`,
  meaning every pre-M23 invocation (which never passes a 6th positional CLI argument) is
  byte-identical to pre-M23 behavior.
- The CLI parsing addition (`argc >= 7 ? strtoul(argv[6],...) : 0`) is similarly additive and
  backward-compatible.
- No touch to `step_cpu_instruction()`, `run_cycles()`, `run_until_cycle()`, or any other
  CPU-execution-gating path — confirmed by the same `git diff` read.
- The claimed M19 `--cart1`/`--cart2` precedent for extending this same `--parity-trace` mode is
  genuine, not fabricated: I read the existing (pre-M23) doc comment directly above
  `run_parity_trace()` in the current `src/main.cpp`, which already documents `cli_args`'s M19-S6
  origin for exactly this same mode.

**My independent conclusion:** this is a reasonable, low-risk, adequately-disclosed scope note, not
a violation requiring a blocked sign-off. It was made necessary by the planner package's own
explicit acceptance criterion (§6 item 9, requiring a genuinely *attempted* A/B check for C2) — the
existing `--parity-trace` mode structurally could not observe halted-idle stepping at all, so some
extension was an implied requirement of the planner's own AC, not a developer-invented scope
expansion. The developer disclosed it prominently rather than concealing it, kept it maximally safe
(default-preserving, isolated to one CLI mode, zero touch to any hard-constrained file), and
grounded the precedent in a real prior instance in the same file. That said, per CLAUDE.md's literal
"scope changes require a decisions.md entry" rule, this should be formally ratified in
`agent-protocol/channels/decisions.md` at closure (Required Fix #1 above) — a procedural completion
step, not a reason to withhold PASS.

---

## Determinism check

`vdp_access_timing.h`'s functions (`cpu_tstate_within_line`, `vdp_cycle_within_line`,
`minimum_request_latency_tstates`) are all `constexpr` and operate purely on their explicit
parameters — no `elapsed_cycles()`, no clock/global-state dependency of their own (confirmed by
direct read of the full 148-line file). The HALT-R fix adds exactly one deterministic call
(`increment_refresh_register()`, itself already deterministic and QA-verified since M11/M12) to an
existing deterministic branch — no new source of non-determinism is introduced.

---

## Summary for the coordinator

- **ctest result observed by QA**: 121/121 passed, 0 failed (117 prior + 4 new), zero regression.
- **Zero-diff confirmation**: all 16 named zero-tolerance files (10 CPU-oracle tests + 6 VDP-device
  files) independently confirmed to have empty `git diff` against `v1.0.22`.
- **Scrutiny Item 1 (A/B divergence)**: honest, well-hedged, not masking a defect — bottom-line
  disposition confirmed sound by independent re-run; the specific causal mechanism is refined (see
  above) as a non-blocking watch-item.
- **Scrutiny Item 2 (`main.cpp` scope note)**: accepted as reasonable and low-risk; recommend a
  `decisions.md` entry at closure to formally ratify it (procedural, non-blocking).

**Recommendation: PASS.** No blocker-level gaps remain. The coordinator may proceed to the
release-decision/tag step for M23 per the standing M21-M23 pre-authorization, folding in the two
recommended non-blocking follow-ups (decisions.md ratification of the `main.cpp` change; the
refined A/B-divergence watch-item) at closure.
