# M25 Implementation Report — Sony Speed-Controller + Hardware PAUSE + Ren-Sha Turbo (closes C8)

- Milestone ID: M25
- Developer: MSX Developer Agent
- Planner package: `docs/m25-planner-package.md`
- Status recorded here: developer implementation COMPLETE, evidence captured. NOT yet closed —
  QA sign-off + coordinator release decision are the remaining gates (per the standing M24-M25
  continuation directive: proceed through the release-decision/tag step without an extra pause on
  a clean QA PASS; STOP and consult the human otherwise). The `deferred-backlog.md` status-column
  ledger flip to DONE and the `v1.0.25` tag are left to the coordinator at closure time.

---

## 1. Milestone Target

Implement deferred-backlog row **C8** — three genuinely new, never-previously-scoped HB-F1XV-specific
hardware mechanisms grounded in `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` §9 and
`references/fact-sheets/Zilog Z80A CPU.md` §6:

1. **Hardware PAUSE** — the Sony **MB670836** companion chip's documented behavior: "physically halts
   the CPU and cannot be bypassed in software." Genuinely distinct from the Z80's own `HALT`
   instruction (CPU-internal, already modeled M9/M23/C2).
2. **Speed Controller** (slow-motion slider) — NOT a clock-speed change; "implemented as an autofire
   on the PAUSE button synced to BLANK," slowing games by pausing them intermittently.
3. **Ren-Sha Turbo** — a separate, simpler autofire feature (rapid button-press synthesis) on the
   space bar and joystick trigger-A of both ports, grounded in real, usable openMSX reference
   behavior + the real per-machine `Sony_HB-F1XV.xml` calibration.

Slice plan followed exactly as scoped (`docs/m25-planner-package.md` §3):

- **S1** — `Mb670836PauseController` isolated unit tests (zero machine wiring).
- **S2** — `RenshaTurbo` isolated unit tests (zero peripheral wiring).
- **S3** — `KeyboardMatrix`/`JoystickPorts` wiring + regression guards.
- **S4** — machine wiring (the highest-risk slice: `step_cpu_instruction()`/`run_frame()`/
  `cold_boot()`), followed IMMEDIATELY by the `git diff v1.0.24` confirmation of the 12 named
  zero-tolerance CPU-timing-oracle test files (§4 below), before proceeding to S5.
- **S5** — dedicated system integration test + openMSX A/B evidence + backlog/documentation closure.

## 2. Code Changes

### New (production, `src/`)

- **`src/devices/chipset/mb670836_pause.h` / `.cpp`** — `Mb670836PauseController` (92 + 49 lines). A
  machine-level CPU-execution gate, NOT part of `Z80aCpu` (zero edit to `src/devices/cpu/*`). Two
  independent trigger sources OR into one combined gate (A-M25-4):
  - `press_pause_button()` toggles an internal `engaged_` bool (A-M25-1, a documented first-principles
    design choice — no datasheet documents the real button's electrical behavior; the closest openMSX
    precedent, `MSXTurboRPause` (a DIFFERENT chipset, S1990, cited for design-inspiration only), uses
    the same toggle/flip-flop semantics).
  - `set_speed_level(0..7)` + `on_vsync()` drive a deterministic VBlank-synced duty cycle:
    `kPeriodFrames = 8`; frame `f` (evaluated at the START of the frame, i.e. BEFORE `on_vsync()`'s own
    increment) is paused iff `(f % 8) < speed_level_` (A-M25-3, a documented design default — no
    datasheet/community measurement of the real slider's numeric ratio exists).
  - `cpu_should_pause() = engaged_ || speed_paused_`.
- **`src/peripherals/rensha_turbo.h` / `.cpp`** — `RenshaTurbo` (107 + 55 lines) + `RenshaTurboClockSource`
  (X-pattern of `RtcClockSource`/`FdcClockSource`/`CassetteClockSource`). Grounded in real openMSX
  behavior (behavior reference, never copied): `RenShaTurbo.{hh,cc}`/`Autofire.{hh,cc}` (signal
  generator shape), `MSXPPI.cc:90-93`/`sound/MSXPSG.cc:90-93` (OR-combine wiring points, independently
  re-read this cycle and confirmed byte-for-byte identical to the planner's own citations), and the
  real per-machine `Sony_HB-F1XV.xml:16-19` calibration (`min_ints=47`/`max_ints=221`, independently
  re-read this cycle). The toggle-frequency formula is independently re-derived from the config data's
  own documented MEANING (`Autofire.hh:66-76`'s doc comment), NOT transcribed from openMSX's own
  `setClock()` code shape (A-M25-6): `ints(speed) = kDefaultMaxInts - (speed * (kDefaultMaxInts -
  kDefaultMinInts)) / 100`; `half_period_cycles = (kSystemClockHz * ints) / 6000`, all-integer
  arithmetic. `signal() = ((clock_source_->cpu_cycles() / half_period_cycles()) & 1) == 1` when
  `speed_ != 0` and a clock source is attached, else `false`. `keyboard_row8_or_mask()`/
  `joystick_trigger_a_or_mask()` return `0x01`/`0x10` when `signal()` is true, `0x00` otherwise —
  OR-only, can never force a 0 bit (R-M25-6).

### Edited (additive only)

- **`src/peripherals/keyboard_matrix.h`/`.cpp`** — new `attach_rensha_turbo(const RenshaTurbo*)`
  (default `nullptr`); `keyboard_row(8)` ORs in `rensha_->keyboard_row8_or_mask()` only when `row==8`
  and attached.
- **`src/peripherals/joystick.h`/`.cpp`** — new `attach_rensha_turbo(const RenshaTurbo*)` (default
  `nullptr`); `read_port_a()` ORs in `rensha_->joystick_trigger_a_or_mask()` after the existing
  cassette-input bit logic, applied to the already-selected port's value (matches openMSX's
  `ports[selectedPort]`-after-mux wiring).
- **`src/machine/hbf1xv_machine.h`/`.cpp`** — new owned members: `pause_controller_`
  (`Mb670836PauseController`), `rensha_turbo_` (`RenshaTurbo`), `rensha_clock_` (new
  `RenshaTurboClock` nested class, X-pattern of `RtcClock`/`FdcClock`/`CassetteClock`); `wire_bus()`
  attaches `rensha_turbo_` into `keyboard_`/`joystick_` and its own clock source; `cold_boot()` adds
  `pause_controller_.reset()`/`rensha_turbo_.reset()`; `run_frame()` adds ONE line,
  `pause_controller_.on_vsync();`, immediately alongside the existing `vdp_.on_vsync()` call;
  `step_cpu_instruction()` adds an early-return gate at the very top (`if
  (pause_controller_.cpu_should_pause()) { scheduler_.tick(1); return 1; }`) BEFORE any opcode decode
  — everything below that gate is byte-for-byte unmodified from pre-M25; new accessors
  `pause_controller()`/`rensha_turbo()` mirroring the existing `s1985()`/`ppi()` style.
- **`CMakeLists.txt`** — two additive lines registering `mb670836_pause.cpp`/`rensha_turbo.cpp` in the
  `sony_msx_core` static-library source list.
- **`tests/CMakeLists.txt`** — 6 new `add_executable`/`add_test` blocks under a new `--- M25 ---`
  section (40 lines total), no asset-root compile-definition needed this milestone (nothing new reads
  a ROM/BIOS asset).

`git diff v1.0.24 --stat -- src/ CMakeLists.txt tests/CMakeLists.txt` (full, verified):

```
 CMakeLists.txt                      |  2 ++
 src/machine/hbf1xv_machine.cpp      | 70 +++++++++++++++++++++++++++++++++++++
 src/machine/hbf1xv_machine.h        | 47 +++++++++++++++++++++++++
 src/peripherals/joystick.cpp        | 14 ++++++++
 src/peripherals/joystick.h          |  8 +++++
 src/peripherals/keyboard_matrix.cpp | 15 +++++++-
 src/peripherals/keyboard_matrix.h   |  8 +++++
 tests/CMakeLists.txt                | 40 +++++++++++++++++++++
 8 files changed, 203 insertions(+), 1 deletion(-)
```

(the two new `src/devices/chipset/mb670836_pause.*` and `src/peripherals/rensha_turbo.*` files are
untracked/new, not shown by `git diff --stat` against a tag — confirmed present via direct listing).

### Unmodified (verified, not merely asserted)

`git diff v1.0.24 --name-only -- src/devices/cpu/ src/devices/video/ src/devices/audio/
src/devices/rtc/ src/devices/fdc/ src/devices/cartridge/ src/devices/memory/ src/devices/halnote/
src/devices/kanji/ src/core/` returns **empty** — zero changes to the CPU core or any other device/
core-scheduling file. `src/devices/chipset/` gained exactly one new file pair
(`mb670836_pause.{h,cpp}`); `s1985_engine.*`, `ppi_8255.*`, `slot_bus.*`, `io_bus.*`, `mapper_io.*`,
`switched_io.*`, `system_bus.*`, `system_control.*` are all unchanged.

### New (tests)

- `tests/unit/devices/chipset/mb670836_pause_unit_test.cpp` (229 lines) — `reset()` idle defaults;
  `press_pause_button()` toggle true→false→true; `set_speed_level()` clamping; `on_vsync()`+duty-cycle
  formula exhaustively hand-verified for TWO distinct speed levels (3 and 6 of 8) across a full
  `kPeriodFrames`-length window, plus a repeat-across-a-second-window determinism check, plus
  `speed_level=0` (never paused) and `speed_level=kMaxSpeedLevel` (paused every frame but the last)
  edge cases; `cpu_should_pause()` truth table (all 4 combinations of button/speed-controller state);
  a two-instance determinism check.
- `tests/unit/peripherals/rensha_turbo_unit_test.cpp` (209 lines) — `reset()`/unattached-clock idle
  default (`signal()` always false); `speed()==0` always false regardless of cycles; the
  `half_period_cycles()` derivation independently re-verified by the TEST ITSELF (a separate
  `hand_computed_half_period()` helper, not calling into the class under test) at THREE concrete speed
  values (100, 1, 50); `keyboard_row8_or_mask()`/`joystick_trigger_a_or_mask()` correctness; two-run
  determinism.
- `tests/integration/peripherals/rensha_turbo_integration_test.cpp` (199 lines) — (a) REGRESSION
  GUARD: default/explicit-nullptr `attach_rensha_turbo()` reproduces byte-for-byte pre-M25 behavior
  for `KeyboardMatrix`/`JoystickPorts`; (b) with the key/trigger NOT held, a live, engaged RenshaTurbo
  produces ZERO observable difference at every sampled cycle across an exhaustive sweep spanning
  multiple toggle periods (R-M25-6); (c) with the key/trigger genuinely held, the read alternates
  between pressed/released at the expected computed cadence, verified for BOTH the keyboard path and
  (through a real `PsgYm2149`) the joystick path.
- `tests/integration/machine/hbf1xv_m25_pause_integration_test.cpp` (203 lines) — (a) REGRESSION
  GUARD: the default (post-`cold_boot()`) pause-controller state reproduces
  `hbf1xv_cpu_step_integration_test.cpp`'s own T-state/register/PC/`elapsed_cycles()` oracle
  byte-for-byte (the exact program from that file, read but not modified — it remains a named
  zero-tolerance oracle); (b) a deterministic counter-increment loop proves PC/every
  register(`af`/`bc`/`de`/`hl`/`ix`/`iy`/`sp`/`i`/`r`/shadow registers)/memory stay byte-for-byte
  FROZEN across 20 paused `step_cpu_instruction()` calls, with `elapsed_cycles()` advancing by exactly
  20 (1 T-state/call); (c) releasing the button resumes execution from the exact frozen PC and the
  counter continues incrementing correctly; (d) Acceptance Criterion 3 — a dedicated I/O-port +
  arithmetic probe program run for 10 steps while paused never advances PC nor disengages the gate;
  only `press_pause_button()` releases it.
- `tests/integration/machine/hbf1xv_m25_speed_controller_integration_test.cpp` (150 lines) — drives
  the same counter-loop program purely via `step_cpu_instruction()` while calling
  `machine.pause_controller().on_vsync()` DIRECTLY at simulated-VBlank boundaries (NEVER calling
  `run_frame()` in the same test, R-M25-5). Uses a documented testing simplification — a
  whole-loop-iteration-aligned simulated-VBlank quantum (40 steps = 10 iterations/window) instead of
  the literal 59736 T-state `frame_cycles_per_frame()` constant, specifically to keep the growth oracle
  an EXACT, hand-computable integer with zero partial-instruction-overshoot ambiguity. At
  `speed_level=3` over 16 windows (2 full periods): every paused window shows EXACTLY zero counter
  growth AND every one of its 40 steps returns exactly 1 T-state; every unpaused window shows EXACTLY
  10 iterations of growth AND a T-state sum of exactly 440 (10×44); total growth over 2 periods = 100,
  matching the hand-computed oracle exactly.
- `tests/system/hbf1xv_m25_speed_pause_rensha_system_test.cpp` (210 lines) — the milestone's required
  dedicated system integration test, combining all three mechanisms against real stepped CPU programs
  on one machine instance across three phases: Phase 1 (PAUSE engage/release mid-counter-loop-program);
  Phase 2 (Speed-Controller duty-cycle slow-motion across a full 8-window period, hand-computed total
  growth = 12); Phase 3 (Ren-Sha Turbo, driven by a real infinite spin-loop CPU program advancing
  `elapsed_cycles()`, observed toggling the SPACE-key row AND a joystick trigger over thousands of real
  CPU-driven cycles — both a negative-control sweep with neither held, and a positive alternation
  check with both held).

### New (tools, non-shipped)

- **`tools/openmsx-m25-rensha-parity.ps1`** (292 lines) — the openMSX A/B evidence harness (§5 below).

## 3. Unit Test Results

```
$ ctest --test-dir build -C Debug -R "mb670836|rensha_turbo" --output-on-failure
    Start 125: devices_chipset_mb670836_pause_unit_test
1/6 Test #125: devices_chipset_mb670836_pause_unit_test ...............   Passed    0.02 sec
    Start 126: peripherals_rensha_turbo_unit_test
2/6 Test #126: peripherals_rensha_turbo_unit_test .....................   Passed    0.02 sec
    Start 127: peripherals_rensha_turbo_integration_test
3/6 Test #127: peripherals_rensha_turbo_integration_test ..............   Passed    0.02 sec
    Start 128: machine_hbf1xv_m25_pause_integration_test
4/6 Test #128: machine_hbf1xv_m25_pause_integration_test ..............   Passed    0.03 sec
    Start 129: machine_hbf1xv_m25_speed_controller_integration_test
5/6 Test #129: machine_hbf1xv_m25_speed_controller_integration_test ...   Passed    0.02 sec
    Start 130: hbf1xv_m25_speed_pause_rensha_system_test
6/6 Test #130: hbf1xv_m25_speed_pause_rensha_system_test ..............   Passed    0.03 sec

100% tests passed, 0 tests failed out of 6
```

(An earlier run of `machine_hbf1xv_m25_pause_integration_test` failed with two case failures —
`Setup_AfterOneFullLoop_PcBackAtOrigin`/`Resume_AfterOneMoreFullLoop_PcBackAtOrigin` — a genuine bug
in the TEST's own loop-length constant, `kN = 5` instead of the correct `kN = 4` for the 4-instruction
counter loop; fixed and reconfirmed before proceeding, an honest disclosure per this project's
established practice of not hiding self-caught defects.)

## 4. Integration Test Results

### 4.1 Immediate post-S4 CPU-timing-oracle confirmation (before proceeding to S5)

Per the dispatch's explicit instruction, run IMMEDIATELY after the S4 machine-wiring slice:

```
$ git diff v1.0.24 --stat -- \
    tests/unit/devices/z80a_unprefixed_unit_test.cpp \
    tests/unit/devices/z80a_trace_export_unit_test.cpp \
    tests/integration/machine/hbf1xv_cpu_parity_integration_test.cpp \
    tests/integration/machine/hbf1xv_m11_parity_checkpoint_integration_test.cpp \
    tests/integration/machine/hbf1xv_m13_mem_parity_checkpoint_integration_test.cpp \
    tests/integration/machine/hbf1xv_parity_checkpoint_integration_test.cpp \
    tests/integration/machine/hbf1xv_indexed_program_integration_test.cpp \
    tests/integration/machine/hbf1xv_cb_program_integration_test.cpp \
    tests/integration/machine/hbf1xv_ldir_program_integration_test.cpp \
    tests/integration/machine/hbf1xv_interrupt_ack_integration_test.cpp \
    tests/integration/machine/hbf1xv_cpu_step_integration_test.cpp \
    tests/integration/machine/hbf1xv_m23_halt_r_integration_test.cpp
(no output — empty diff, exit code 0)
```

All 12 named zero-tolerance CPU-timing-oracle test files (the 10 from the M23 precedent list, plus
M9's own `hbf1xv_cpu_step_integration_test.cpp` and M23's own added
`hbf1xv_m23_halt_r_integration_test.cpp`) are confirmed **byte-for-byte unchanged** against `v1.0.24`.
This gate was satisfied BEFORE proceeding to S5, as instructed.

### 4.2 Full regression suite (all M1-M25, including the slow M24 sweep)

```
$ ctest --test-dir build -C Debug -LE m24_slow_full_sweep --output-on-failure
...
127/129 Test #128: machine_hbf1xv_m25_pause_integration_test ..........................   Passed    0.02 sec
128/129 Test #129: machine_hbf1xv_m25_speed_controller_integration_test ...............   Passed    0.01 sec
129/129 Test #130: hbf1xv_m25_speed_pause_rensha_system_test ..........................   Passed    0.02 sec

100% tests passed, 0 tests failed out of 129
Total Test time (real) =   5.83 sec
```

Full suite, including the slow `hbf1xv_m24_zexall_system_test`, run directly/synchronously (not
nested behind a background-wait pattern) per the dispatch's explicit instruction:

```
$ ctest --test-dir build -C Debug --output-on-failure
...
124/130 Test #124: hbf1xv_m24_zexall_system_test ......................................   Passed  1365.03 sec
125/130 Test #125: devices_chipset_mb670836_pause_unit_test ...........................   Passed    0.01 sec
126/130 Test #126: peripherals_rensha_turbo_unit_test .................................   Passed    0.01 sec
127/130 Test #127: peripherals_rensha_turbo_integration_test ..........................   Passed    0.01 sec
128/130 Test #128: machine_hbf1xv_m25_pause_integration_test ..........................   Passed    0.02 sec
129/130 Test #129: machine_hbf1xv_m25_speed_controller_integration_test ...............   Passed    0.02 sec
130/130 Test #130: hbf1xv_m25_speed_pause_rensha_system_test ..........................   Passed    0.02 sec

100% tests passed, 0 tests failed out of 130

Label Time Summary:
m24_slow_full_sweep    = 1365.03 sec*proc (1 test)

Total Test time (real) = 1368.54 sec
```

**130/130 (124 prior M1-M24 + 6 new M25), 0 failed, zero regression.** The slow
`hbf1xv_m24_zexall_system_test` genuinely re-ran to completion (1365.03s ≈ 22.75 minutes, consistent
with M24's own historically-measured range of ~24-27 minutes) and PASSED — its own hard
`error_markers == 0` gates for both ZEXALL and ZEXDOC (added at M24 closure, DEC-0022's Required
Fix #1) genuinely fired and held, confirming the CPU core's own already-QA-verified correctness is
completely unaffected by this milestone's additive `step_cpu_instruction()` gate (expected, since
`git diff v1.0.24` already independently confirmed zero changes to `src/devices/cpu/*`, and this test
provides the strongest possible empirical confirmation of that same fact: 5,764,169,474 real
instructions per suite executed through the gated `step_cpu_instruction()` path with the gate
genuinely idle throughout, producing byte-identical results to M24's own closure figures).

### 4.3 Evidence gates

```
$ tools/validate-assets.ps1
Asset validation result: True
BIOS directory: bios/ — 7 files present (f1xvbios.rom, f1xvdisk.rom, f1xvext.rom, f1xvfirm.rom,
  f1xvkdr.rom, f1xvkfn.rom, f1xvmus.rom)
ROM directory: roms/ — 2 files present (aleste.rom, metalgear.rom)

$ tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt
Checksum report written to: docs/asset-checksums.txt (unchanged asset set/hashes from prior milestones)

$ cmake --build build --config Debug
(clean full build succeeds — sony_msx_headless.exe + sony_msx_core + all 130 test executables)
```

## 5. openMSX A/B Evidence (`docs/m25-parity-trace-diff.md`)

Two separate, honestly-dispositioned sub-claims, exactly per the planner's Acceptance Criterion 9
(this split is the point, not a shortcut):

### 5.1 Ren-Sha Turbo: genuinely attempted, PARITY

`tools/openmsx-m25-rensha-parity.ps1` drives the REAL `Sony_HB-F1XV` openMSX machine (openMSX 19.1,
WSL — the same installed reference runtime used throughout M11-M24; the source-tree grounding
citations are against `references/openmsx-21.0`, an established, disclosed distinction) via its own
live Tcl interface: `set renshaturbo 100` (the real openMSX Autofire speed setting, a first-class
Tcl-linked `IntegerSetting`) + `debug write/read ioports` sampling of keyboard row 8 (#A9, after
selecting row 8 via #AA) and PSG R14 (#A2, after latching register 14 via #A0), scheduled via `after
time <seconds> { ... }`-chained callbacks — deliberately NOT per-instruction Tcl single-stepping,
because M23/M24's own prior findings independently established that live per-instruction Tcl
single-stepping becomes slow/inconsistent past a small step count; this technique instead lets the
real engine run freely (native, continuous emulation) between each scheduled sample, sidestepping that
limitation entirely.

Result, reproduced twice this cycle (once during initial development, once after fixing a PowerShell
string-escaping bug in the report-generation code — see Known Issues):

- **(a) Not held, RenSha engaged: keyboard row-8 bit0 (SPACE) — PARITY.** 10/10 samples idle (zero
  observable effect), matching R-M25-6's invariant, confirmed live on the real reference engine.
- **(c) Not held, RenSha engaged: PSG R14 bit4 (joystick trigger A) — PARITY.** 10/10 samples idle,
  same invariant confirmed for the joystick path.
- **(b) Held (`keymatrixdown 8 1`, a real, first-class openMSX Tcl scripting primitive for driving the
  keyboard matrix directly), RenSha engaged: keyboard row-8 bit0 — PARITY.** The real reference
  engine's live read alternates: 20/40 samples "pressed" (bit0=0), 20/40 samples "released" (bit0=1,
  forced by the autofire OR-combine) — matching this project's own OR-only-releases implementation
  (A-M25-7).
- **Explicitly NOT attempted (honest disposition):** a live "held" alternation demonstration for the
  PSG R14/joystick-trigger-A path — no equivalent "hold a joystick button" Tcl scripting primitive was
  found in openMSX 21.0 (`keymatrixdown`/`keymatrixup`, `Keyboard.cc:1565-1611`, apply ONLY to the
  keyboard matrix, confirmed by direct source read this cycle). The not-held invariant IS confirmed
  live for the joystick path; the held-alternation behavior for that path relies on this project's own
  already-exhaustively-tested implementation (the identical OR-combine code path as the keyboard).

### 5.2 Hardware PAUSE / Speed Controller: honestly BLOCKED (the right disposition, not a shortfall)

Re-confirmed this cycle by direct source read (not merely trusted from the planner package):

- `references/openmsx-21.0/src/SG1000Pause.hh` — Sega SG-1000/SC-3000 "hold"/"pause"/"reset" button,
  triggers an NMI. Different machine family entirely (not MSX), different mechanism (NMI, not a
  CPU-halting WAIT-line gate).
- `references/openmsx-21.0/src/MSXTurboRPause.{hh,cc}` — MSX turboR (S1990 chipset) pause key, a
  flip-flop status bit at I/O port `0xA7`, implemented by calling `getMotherBoard().pause()`/
  `unpause()` — openMSX's own whole-session engine pause, architecturally incompatible with this
  project's atomic, deterministic per-instruction `step_cpu_instruction()` stepping model. Different
  chipset (S1990, not S1985/MB670836) and different machine family (turboR, not the HB-F1XV's plain
  MSX2+).
- None of the six real Sony MSX machine XML definitions wire a Pause or SpeedController device; four
  of the six explicitly say so in their own `<description>` text: `Sony_HB-F1.xml:9`,
  `Sony_HB-F1II.xml:9`, `Sony_HB-F1XD.xml:9`, `Sony_HB-F1XDJ.xml:9` all read "speed controller (not
  emulated)" verbatim. `Sony_HB-F1XDmk2.xml:9` (found via QA's fresh `find` sweep, RESP-M25-003) says
  it is "identical to its predecessor" (`Sony_HB-F1XD.xml`, which explicitly says "not emulated") and
  independently confirmed to wire no Pause/SpeedController device either. `Sony_HB-F1XV.xml:9` (this
  project's actual target machine) does not even mention the speed controller in its own (shorter)
  description.

**Conclusion: openMSX 21.0 genuinely has ZERO Sony-specific PAUSE/speed-controller modeling anywhere
to diff against.** No parity is asserted for this sub-claim. Per Acceptance Criterion 9, this BLOCKED
disposition does NOT gate C8's closure — it mirrors the M21 computed-pixel-color and C3/M24
disk-boot-A/B precedents of an honestly-disclosed BLOCKED sub-claim not blocking an otherwise-complete
milestone.

## 6. Full Deferred-Backlog Review (all 34 rows, per DEC-0005)

**Section A (human-directive-tracked):** B1-B9 all re-affirmed **DONE**, unchanged; M25 touches no
PSG/RTC/YM2413/SRAM/Kanji/Halnote/cartridge/FDC/VDP device file (`git diff v1.0.24 -- src/devices/`
confirms only the one new `chipset/mb670836_pause.*` pair). B9's own VDP `run_frame()` note: the M25
edit is a single additive line calling the NEW `pause_controller_.on_vsync()`, immediately alongside
the pre-existing, unchanged `vdp_.on_vsync()` call.

**Section B (other known deferrals):**

- **C1** Exact cycle/T-state timing parity — re-affirmed **IN-PROGRESS (M23 partial)**, unchanged; M25
  adds no CPU-timing-affecting code inside `Z80aCpu` itself.
- **C2** Z80 HALT-R increment — re-affirmed **DONE (M23)**, unchanged; M25 touches no CPU-core file,
  and the PAUSE mechanism deliberately does NOT increment `R` (the opposite of HALT-R) — a genuinely
  different mechanism, no regression risk to C2's own closed scope.
- **C3** ZEXDOC/ZEXALL full parity sweep — re-affirmed **DONE (M24)**, unchanged; the system test
  re-ran to completion this cycle (§4.2) with the identical result, confirming M25 introduces zero
  CPU-core regression.
- **C4** S1985 backup-RAM `.sram` persistence — re-affirmed **DONE (M15)**, unchanged.
- **C5** Full boot past first device read — re-affirmed **IN-PROGRESS (M16 partial)**, unchanged;
  unrelated to M25.
- **C6** Peripherals (keyboard/joystick) — re-affirmed **DONE (M15)**; M25 ADDS to
  `KeyboardMatrix`/`JoystickPorts` (RenshaTurbo attach points), a regression-guarded, additive
  extension — C6's own already-closed contract (matrix/joystick semantics with no RenSha attached) is
  explicitly re-verified byte-for-byte unchanged (M25-S3's dedicated regression-guard test).
- **C7** Printer + cassette — re-affirmed **DONE (M18)**, unchanged.
- **C8** Sony speed-controller/PAUSE + Ren-Sha Turbo — **THIS MILESTONE.** Implementation complete
  this cycle for all three sub-items; the PAUSE/Speed-Controller A/B sub-claim is honestly BLOCKED for
  lack of a comparable reference engine, which does not gate closure per Acceptance Criterion 9.
  Ledger status transition to DONE (M25) reserved for the coordinator at closure, pending QA sign-off.
- **C9** SDL3 frontend — re-affirmed **OPEN**; unrelated to this milestone (still indicative M26).
  Forward note for the future C9 milestone: PAUSE/Speed-Controller/Ren-Sha Turbo now have a complete
  machine/peripheral-level API surface (`press_pause_button()`, `set_speed_level()`,
  `RenshaTurbo::set_speed()`) ready for a future keyboard/input binding — this milestone deliberately
  adds no CLI/SDL3 wiring itself.
- **C10** FDC flux-level/DMK disk fidelity — re-affirmed **OPEN**; unrelated to this milestone.

**Section C (M14 VDP-depth deferrals):** D1-D3, D5-D7 re-affirmed **DONE**, unchanged; **D4**
re-affirmed **IN-PROGRESS (M23 partial)**, unchanged; M25 touches no VDP-timing file.

**Section D (M17 YM2413 depth deferrals):** E1, E2 re-affirmed **OPEN**; unrelated to this milestone.

**Section E (M18 printer/cassette depth deferrals):** F1, F2 re-affirmed **OPEN**; unrelated.

**Section F (M19 cartridge-mapper depth deferrals):** G1-G4 re-affirmed **OPEN**; unrelated.

All 34 rows accounted for; only C8 changes disposition this cycle (implementation complete, pending
QA — not yet flipped to DONE in the ledger, per the coordinator-reserved closure transition).

## 7. Known Issues / Residual Risks

- **A self-caught test bug, fixed before requesting QA**: an early draft of
  `hbf1xv_m25_pause_integration_test.cpp` used `kN = 5` for the counter loop's per-iteration step
  count; the actual loop is 4 instructions/iteration, not 5. Caught immediately by the test's own
  assertion failing (`Setup_AfterOneFullLoop_PcBackAtOrigin`), fixed to `kN = 4`, reconfirmed passing.
  Zero production-code risk — this was a test-file-only defect.
- **A genuine PowerShell string-escaping bug, fixed before finalizing A/B evidence**: the first draft
  of `tools/openmsx-m25-rensha-parity.ps1` used stray extra backticks around embedded double-quotes
  (e.g. `` `"`released`"` `` instead of `` `"released`" ``), which PowerShell interpreted as escape
  sequences (`` `r `` = carriage return, `` `n `` = newline) rather than literal backtick characters,
  corrupting several words in the first-generated `docs/m25-parity-trace-diff.md` (e.g. "released"
  rendered as "\reased" — a dropped 'r' — and "not" as a spurious line break). Also used a non-ASCII
  `§` character with `-Encoding ASCII`, corrupting to `?`. Both fixed (removed the stray backticks;
  replaced `§N.N` with the word "section N.N"); the script was re-run and the regenerated
  `docs/m25-parity-trace-diff.md` confirmed clean. This was caught by directly reading the generated
  artifact rather than trusting the script's own success exit code — a reminder that tooling output
  must always be independently inspected, not just trusted from an exit code.
- **PAUSE's idle T-state charge (1 per paused `step_cpu_instruction()` call) is a documented modeling
  choice, not a hardware-quantized fact** (R-M25-7) — real hardware's WAIT-line hold is not naturally
  discretized; this is the finest-grained unit this whole-instruction-atomic engine can charge, with
  no overshoot risk, and does not affect any other already-closed milestone's timing oracle (confirmed
  via the §4.1 `git diff` check).
- **The Speed-Controller's `kPeriodFrames=8`/level-range and the PAUSE-button toggle semantics are
  first-principles design defaults, not measured hardware facts** (A-M25-1/A-M25-3) — no Sony
  MB670836 datasheet or community measurement exists. Documented as such in code comments; if a real
  service-manual/measurement ever surfaces, re-verify/re-calibrate.
- **No CLI flag or SDL3/frontend binding was added this cycle** (deliberate, per §1.2 of the planner
  package) — `press_pause_button()`/`set_speed_level()`/`RenshaTurbo::set_speed()` are machine/
  peripheral-level API surfaces only, exercised by tests, exactly like every other human-input
  peripheral in this project today. Ready for C9 (SDL3 frontend, still OPEN) to bind.
- **Debug-tooling extension** (`DebugEventType::Pause`/`Resume`, a PAUSE/RenSha section in
  `serialize_state_dump()`) was named as developer's-judgment-optional in the planner's §6, not a hard
  acceptance criterion — NOT implemented this cycle, to keep the change set surgically confined to
  what the acceptance criteria actually require. Flagged here for QA/coordinator visibility, not
  silently dropped.

## 8. QA Handoff

- Full regression: **130/130** (124 prior M1-M24 + 6 new M25), 0 failed, independently reproducible
  from a clean rebuild (`cmake --build build --config Debug` then `ctest --test-dir build -C Debug
  --output-on-failure`, budget ~23-27 minutes for the slow zexall system test).
- `git diff v1.0.24` confirms zero changes to `src/devices/cpu/`, `src/devices/video/`,
  `src/devices/audio/`, `src/devices/rtc/`, `src/devices/fdc/`, `src/devices/cartridge/`,
  `src/devices/memory/`, `src/devices/halnote/`, `src/devices/kanji/`, `src/core/`; only new
  `src/devices/chipset/mb670836_pause.*`, new `src/peripherals/rensha_turbo.*`, and additive edits to
  `src/peripherals/keyboard_matrix.*`, `src/peripherals/joystick.*`, `src/machine/hbf1xv_machine.*`.
- All 12 named zero-tolerance CPU-timing-oracle test files confirmed byte-for-byte unchanged (§4.1) —
  QA should independently re-run this exact `git diff` command.
- Hardware PAUSE genuinely freezes PC/every register/R/memory across multiple paused steps
  (`elapsed_cycles()` still advancing exactly 1 T-state/call) and cannot be bypassed via any
  CPU-visible API (a dedicated I/O-port+arithmetic probe test) — both independently, exhaustively
  tested (§4, `hbf1xv_m25_pause_integration_test.cpp`).
  The Speed Controller's duty cycle is deterministic and hand-computable (three independent
  hand-computed oracles: the S1 unit test, the S4 integration test, the S5 system test).
- Ren-Sha Turbo never forces a spurious press (R-M25-6) — an exhaustive negative-control sweep, both
  in isolation (`rensha_turbo_integration_test.cpp`) and through a real `Hbf1xvMachine` driven by real
  CPU-executed cycles (the S5 system test), plus a genuine LIVE confirmation against real openMSX
  (§5.1).
- Real openMSX A/B evidence (`docs/m25-parity-trace-diff.md`): Ren-Sha Turbo PARITY (live, both the
  not-held invariant and the held keyboard alternation); Hardware PAUSE/Speed Controller honestly
  BLOCKED with the exact citations reproduced in the artifact itself, per Acceptance Criterion 9.
- QA should independently: (1) re-run the full suite fresh from a clean rebuild (budget ~25 minutes);
  (2) independently re-run the 12-file `git diff v1.0.24` CPU-timing-oracle confirmation; (3)
  independently re-read `src/devices/chipset/mb670836_pause.{h,cpp}` and
  `src/peripherals/rensha_turbo.{h,cpp}` and assess the design-default vs. hardware-fact distinctions
  (A-M25-1/A-M25-3 vs. A-M25-5/A-M25-6/A-M25-7); (4) independently re-run
  `tools/openmsx-m25-rensha-parity.ps1` and confirm the live PARITY result and the honest BLOCKED
  disposition for PAUSE/Speed-Controller; (5) independently assess whether the two self-caught/fixed
  bugs disclosed in §7 (the test loop-length constant, the PowerShell escaping bug) indicate any
  broader process concern; (6) full 34-row deferred-backlog review (§6) independently spot-checked.
