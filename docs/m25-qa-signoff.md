# M25 QA Sign-off — Sony Speed-Controller + Hardware PAUSE (MB670836) + Ren-Sha Turbo (backlog C8)

- Milestone ID: M25
- QA Owner: MSX QA Agent
- Developer package reviewed: `docs/m25-planner-package.md`, `docs/m25-implementation-report.md`,
  `docs/m25-parity-trace-diff.md`
- Coordinator's own independent verification reviewed: RESP-M25-001, RESP-M25-002
  (`agent-protocol/channels/responses.md`)
- Baseline: `v1.0.24` (M24 closed, 124/124 tests)
- This is the first milestone to implement genuinely new, never-previously-scoped HB-F1XV-specific
  hardware: a machine-level CPU-execution gate (hardware PAUSE) inserted at the very top of
  `step_cpu_instruction()`, an autofire-on-PAUSE Speed Controller reusing that same gate, and a
  separate Ren-Sha Turbo autofire peripheral. The single highest-sensitivity surface is the
  `step_cpu_instruction()`/`run_frame()`/`cold_boot()` edit — this sign-off treats the 12-file
  zero-tolerance CPU-timing-oracle diff and the full slow ZEXALL/ZEXDOC re-run as non-negotiable,
  not optional, gates.

Every finding below is from artifacts I read directly and commands I ran myself this cycle,
starting from a **fully clean rebuild** (`build/` deleted, reconfigured, rebuilt from scratch) —
not the coordinator's or developer's build output, not their `git diff`, not their openMSX A/B
run. The one piece of evidence the coordinator explicitly left for QA — the live openMSX A/B
script — was independently re-run by me end-to-end against the real WSL `Sony_HB-F1XV` machine,
not merely re-read from the committed artifact.

---

## 1. Regression Scope

- **New machine-level device** (`src/devices/chipset/mb670836_pause.{h,cpp}`) —
  `Mb670836PauseController`, a CPU-execution gate consulted at the very top of
  `step_cpu_instruction()`, before any opcode decode. Two trigger sources (manual toggle button,
  VBlank-synced Speed-Controller duty cycle) OR into one `cpu_should_pause()` gate. NOT part of
  `Z80aCpu` — zero edit to `src/devices/cpu/*`.
- **New peripheral** (`src/peripherals/rensha_turbo.{h,cpp}`) — `RenshaTurbo`, an autofire signal
  generator wired pull-style into `KeyboardMatrix`/`JoystickPorts` via additive, default-nullptr
  OR-mask attach points.
- **Edited (claimed additive-only)**: `src/machine/hbf1xv_machine.{h,cpp}` (new owned members, the
  `step_cpu_instruction()` early-return gate, one new line in `run_frame()`, two new `.reset()`
  calls in `cold_boot()`, four new accessors), `src/peripherals/keyboard_matrix.{h,cpp}`,
  `src/peripherals/joystick.{h,cpp}` (attach points + OR-mask consumption), `CMakeLists.txt` /
  `tests/CMakeLists.txt` (source/test registration only).
- **Explicit non-changes claimed and independently checked**: zero changes to `src/devices/cpu/`,
  `src/devices/video/`, `src/devices/audio/`, `src/devices/rtc/`, `src/devices/fdc/`,
  `src/devices/cartridge/`, `src/devices/memory/`, `src/devices/halnote/`, `src/devices/kanji/`,
  `src/core/`; zero changes to `src/main.cpp` (no CLI/frontend binding this cycle).
- **New tests**: 2 unit (`mb670836_pause_unit_test.cpp`, `rensha_turbo_unit_test.cpp`), 3
  integration (`rensha_turbo_integration_test.cpp`, `hbf1xv_m25_pause_integration_test.cpp`,
  `hbf1xv_m25_speed_controller_integration_test.cpp`), 1 system
  (`hbf1xv_m25_speed_pause_rensha_system_test.cpp`) — 6 total, all new this cycle.
- **New non-shipped tool** (`tools/openmsx-m25-rensha-parity.ps1`) — the openMSX A/B evidence
  harness for Ren-Sha Turbo; independently re-run by me this cycle (§2, §5.2).
- **Full regression suite**: the entire M1-M24 corpus (124 tests) plus 6 new M25 tests (130 total,
  128 fast + the pre-existing, re-run `hbf1xv_m24_zexall_system_test`, ~22-27 min).
- **Two explicit design-judgment calls** this sign-off is asked to make (not force-resolved
  upstream): the PAUSE button's toggle-vs-hold semantics (A-M25-1) and the Speed Controller's
  `kPeriodFrames=8` duty-cycle range (A-M25-3) — both documented as first-principles design
  defaults, not measured hardware facts.
- **One license-sensitivity judgment call already made upstream** (A-M25-5/R-M25-3): citing
  RenShaTurbo's real `min_ints=47`/`max_ints=221` scalar config values from
  `Sony_HB-F1XV.xml`, argued safe under the project's standing "zero license-sensitive future
  work" directive.

---

## 2. Regression Matrix Status

| Area | Verification performed by QA | Result |
|---|---|---|
| Clean rebuild | Deleted `build/` entirely; `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` (fresh configure); `cmake --build build --config Debug` | Build succeeded, zero errors (only the pre-existing C4819 code-page warnings, the same class already present in every prior milestone's build) |
| Fast test subset | `ctest --test-dir build -C Debug -LE m24_slow_full_sweep --output-on-failure` | **129/129 passed, 0 failed, 6.92s** — matches the developer's/coordinator's own claimed figures exactly, all 6 new M25 tests included and passing |
| **Slow full-sweep system test** (pre-existing M24 asset, re-run here as the zero-regression check for the CPU core) | `ctest --test-dir build -C Debug -R hbf1xv_m24_zexall_system_test --output-on-failure`, run to genuine completion myself, directly/synchronously (not nested behind a background-wait pattern within my own turn — kicked off via `run_in_background` and awaited via the harness's own completion notification, never a manual sleep-poll loop) | **1/1 passed, 1350.80 sec (~22m31s)**. Read the exact captured stderr directly from `build/Testing/Temporary/LastTest.log`: `ZEXALL: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0` and `ZEXDOC: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0` — a fresh, independent, from-scratch reproduction of the identical instruction count and 67/0 marker split, exercising the M24-hardened `Zexall_ZeroErrorMarkers`/`Zexdoc_ZeroErrorMarkers` hard assertions (confirmed present at `tests/system/hbf1xv_m24_zexall_system_test.cpp:154,167`). Combined with the fast subset: **130/130, 0 failed.** |
| `git diff v1.0.24` scope, 9 non-M25 device categories | `git diff v1.0.24 --name-only -- src/devices/cpu/ src/devices/video/ src/devices/audio/ src/devices/rtc/ src/devices/fdc/ src/devices/cartridge/ src/devices/memory/ src/devices/halnote/ src/devices/kanji/ src/core/` (ran myself) | **Empty**, exit 0 — zero changes, confirmed independently |
| **12-file zero-tolerance CPU-timing-oracle diff** (the single highest-sensitivity confirmation this milestone requires) | Ran the exact 12-file `git diff v1.0.24 --stat` command from `docs/m25-implementation-report.md` §4.1 myself | **Empty, exit 0** — all 12 named files (`z80a_unprefixed_unit_test.cpp`, `z80a_trace_export_unit_test.cpp`, `hbf1xv_cpu_parity_integration_test.cpp`, `hbf1xv_m11_parity_checkpoint_integration_test.cpp`, `hbf1xv_m13_mem_parity_checkpoint_integration_test.cpp`, `hbf1xv_parity_checkpoint_integration_test.cpp`, `hbf1xv_indexed_program_integration_test.cpp`, `hbf1xv_cb_program_integration_test.cpp`, `hbf1xv_ldir_program_integration_test.cpp`, `hbf1xv_interrupt_ack_integration_test.cpp`, `hbf1xv_cpu_step_integration_test.cpp`, `hbf1xv_m23_halt_r_integration_test.cpp`) confirmed byte-for-byte unchanged, independently reproduced |
| `git diff v1.0.24 --stat -- src/ CMakeLists.txt tests/CMakeLists.txt` | Ran directly | Exactly 8 files, 203 insertions/1 deletion — matches the implementation report's own stat exactly |
| `git diff v1.0.24 -- src/machine/hbf1xv_machine.cpp` / `.h` (full diff, not stat) | Read the complete diffs myself | Confirmed purely additive: `wire_bus()` gains 3 attach lines, `cold_boot()` gains 2 `.reset()` calls, `run_frame()` gains exactly ONE new line (`pause_controller_.on_vsync();`) immediately alongside the pre-existing `vdp_.on_vsync()`, `step_cpu_instruction()` gains an early-return gate at the exact top of the function (BEFORE the pre-existing `if (vdp_.irq_active())` VDP-IRQ-sample line), 4 new accessors, one new `RenshaTurboClock` X-pattern nested class, 2 new owned members. Zero lines of any pre-existing function body touched below the insertion points. |
| `git diff v1.0.24 -- src/peripherals/keyboard_matrix.{h,cpp} src/peripherals/joystick.{h,cpp}` | Read the complete diffs myself | Confirmed purely additive: one new `attach_rensha_turbo()` method + one new private pointer member per class; `keyboard_row(8)`/`read_port_a()` gain an `if (rensha_ != nullptr)` OR-mask branch, applied AFTER all pre-existing bit computation (including, for `read_port_a()`, after the existing cassette-input-bit branch) — never before, never replacing anything |
| `mb670836_pause.{h,cpp}` design review | Read both files in full, end-to-end (92 + 49 lines) | Matches the planner's §2.4 design exactly: `reset()`/`press_pause_button()` (toggle)/`set_speed_level()` (clamped 0..7)/`on_vsync()` (pre-increment duty-cycle evaluation, `(f % 8) < speed_level_`)/`cpu_should_pause() = engaged_ \|\| speed_paused_`. Every numeric/behavioral choice is either cited to a fact-sheet quote or explicitly labeled a first-principles design default in the header comment itself, not merely in prose docs. |
| `rensha_turbo.{h,cpp}` design review | Read both files in full, end-to-end (107 + 55 lines) | Matches the planner's §2.5 design exactly. Independently re-derived the frequency formula myself (§4 below) and confirmed it is mathematically equivalent to openMSX's own `Autofire.cc:79-87` `setClock()` expression, via a genuinely different reasoning path (the `Autofire.hh` doc-comment's "50 periods" framing), not a transcription. Spot-verified the interpolation boundaries: speed=100 -> `ints=47=kDefaultMinInts`; speed=1 -> `ints=220` (matches the unit test's own hand-computed oracle). |
| Hardware PAUSE freeze/non-bypass claims | Read `tests/integration/machine/hbf1xv_m25_pause_integration_test.cpp` and `tests/system/hbf1xv_m25_speed_pause_rensha_system_test.cpp` Phase 1 in full; independently confirmed by static inspection that `pause_controller_` is referenced ONLY in `cold_boot()`, `run_frame()`, `step_cpu_instruction()`, and its two accessors (`grep -rn "pause_controller_"` over the machine files) — no I/O port dispatch table anywhere references it | Confirmed: PC/every register (`af`/`bc`/`de`/`hl`/`ix`/`iy`/`sp`/`i`/`r`/shadow registers)/memory are asserted byte-for-byte frozen across 20 (integration test) and 50 (system test) paused steps, `elapsed_cycles()` advances by exactly 1 T-state/call, and a dedicated 10-step I/O-port+arithmetic probe program run while paused never advances PC nor disengages the gate. No CPU-visible API (`Z80aCpu`, `CpuBusClient`, any I/O port) can reach `pause_controller_` — confirmed both by the passing test and independently by grep-based inspection, satisfying Acceptance Criterion 3 by construction, not merely by one test's absence of a counterexample. |
| Speed Controller duty-cycle determinism | Read `tests/unit/devices/chipset/mb670836_pause_unit_test.cpp`, `tests/integration/machine/hbf1xv_m25_speed_controller_integration_test.cpp`, and system-test Phase 2 in full; hand-recomputed the expected sequences myself for `speed_level=3` (`[T,T,T,F,F,F,F,F]`) and `speed_level=6` (`[T,T,T,T,T,T,F,F]`) | All three independent hand-computed oracles (unit/integration/system) match the code's actual output; the integration test's documented `kStepsPerWindow=40` testing simplification (vs. the literal 59736 T-state frame quantum) is clearly disclosed as a testing-only simplification, not a production-code change, and does not affect the correctness of what it proves (that `on_vsync()`-driven duty cycle genuinely gates real CPU execution) |
| Ren-Sha Turbo R-M25-6 invariant (never forces a spurious press) | Read `rensha_turbo_integration_test.cpp` case (b) (an exhaustive sweep across 100 sample points spanning multiple toggle periods) and system-test Phase 3's negative control (3000 real CPU-driven steps, sampled every 37th step) in full | Both independently confirmed exhaustive, not a token check; the OR-mask methods (`keyboard_row8_or_mask()`/`joystick_trigger_a_or_mask()`) can only return `0x00` or a strictly-additive bit, verified by direct code read (`std::uint8_t RenshaTurbo::keyboard_row8_or_mask() const { return signal() ? 0x01 : 0x00; }` — no code path can clear an already-set bit) |
| Regression guards (default/unattached behavior byte-for-byte unchanged) | Read `rensha_turbo_integration_test.cpp` case (a) and `hbf1xv_m25_pause_integration_test.cpp` case (a) (the exact `hbf1xv_cpu_step_integration_test.cpp` program, re-asserted byte-for-byte: `t0=8,a=0x2A`; `t1=5,a=0x2B`; `t2=5,halted`; `t3=5,pc unchanged`; `elapsed_cycles()==23`) | Confirmed both explicit (`attach_rensha_turbo(nullptr)`) and implicit (never-attached) paths reproduce pre-M25 behavior byte-for-byte; the pause-controller regression guard reproduces the referenced pre-M25 CPU-timing oracle exactly |
| **openMSX A/B evidence — independently re-run end-to-end, not merely re-read** | Ran `powershell.exe -File tools/openmsx-m25-rensha-parity.ps1 -DiffOut docs/m25-qa-rensha-parity-rerun.md` myself against the real WSL openMSX 19.1 `Sony_HB-F1XV` machine (confirmed present: `/usr/bin/openmsx`, `openMSX 19.1 flavour: debian components: ALSAMIDI CORE GL LASERDISC`) | **Reproduced: `M25 RESULT: Ren-Sha Turbo PARITY (not-held invariant + held alternation). Hardware PAUSE/Speed Controller: BLOCKED (honest, expected).`** Read the generated artifact and the raw Tcl sample output (`build/m25_rensha_openmsx_raw.txt`) directly, not just the script's own exit code: 10/10 not-held samples idle (`row8=FF r14=FF`) for both the keyboard and PSG paths; 40 held samples genuinely alternate between `row8=FE` (pressed) and `row8=FF` (released) at a real cadence (20/20 split), a live, non-fabricated cross-engine confirmation. Scratch re-run artifact deleted after inspection (the committed `docs/m25-parity-trace-diff.md` already carries the developer's own, textually-identical result). |
| openMSX source citations underpinning the BLOCKED disposition | Independently re-read `references/openmsx-21.0/src/SG1000Pause.hh`, `MSXTurboRPause.{hh,cc}`, `MSXPPI.cc`, `sound/MSXPSG.cc`, `Autofire.{hh,cc}`, and ALL Sony `HB-F1*` machine XML files in `references/openmsx-21.0/share/machines/` (`find` for `Sony_HB-F1*.xml`, not trusting the cited list) | All quoted content verified verbatim (exact line numbers off by 1-2 in a couple of citations, consistent with this project's established citation-granularity tolerance). **One completeness gap found** (see §3, Finding 1): the BLOCKED disposition's own text says "five" Sony machine XML definitions; there are actually **six** (`Sony_HB-F1XDmk2.xml` was missed). Independently confirmed the sixth file also wires no Pause/SpeedController device and its own description ("identical to its predecessor," i.e. the HB-F1XD, whose own description explicitly says "not emulated") is consistent with the same conclusion — the miscount does not change the substantive finding. |
| License-sensitivity judgment (RenShaTurbo `min_ints=47`/`max_ints=221`) | Read `feedback_license_sensitive_scope.md` in full; compared its explicit carve-out ("small, discrete numeric facts... a handful of named constants, a single formula" vs. "large, exacting data tables (hundreds of entries)") against the actual embedded constants in `rensha_turbo.h` | Judgment concurred: two scalar integers extracted from a real per-machine XML config file is categorically the former, not the latter — consistent with the established M17-M24 pattern of citing SHA1 hashes, port numbers, wait-state counts, and ROM sizes from openMSX without issue |
| Design-choice reasonableness: PAUSE toggle-vs-hold (A-M25-1) | Re-read the full S1985 fact-sheet §9 and the cited `MSXTurboRPause.hh` doc comment directly; confirmed neither fact-sheet documents the real button's electrical behavior | Judged reasonable: toggle semantics is the more usable pattern for an eventual keyboard-bound PAUSE key (C9), matches common era-appropriate consumer-electronics PAUSE-button UX, produces clean deterministic/testable semantics, and is explicitly labeled non-authoritative in both the code comment and this report — easily revisable if real hardware data ever surfaces |
| Design-choice reasonableness: `kPeriodFrames=8` (A-M25-3) | Re-read the full Zilog Z80A fact-sheet §6 item 3 directly; confirmed no numeric duty-cycle range is documented anywhere in either fact-sheet or `references/openmsx-21.0/` | Judged reasonable: 8 VBlank frames (~133ms at NTSC 60Hz) gives a fine-enough, power-of-two duty-cycle granularity (8 discrete speed levels) for a "slow motion" effect, is fully deterministic/hand-computable, and is explicitly documented as a design default, not a measured fact |
| `run_until_cycle()`/`run_cycles()` interaction with PAUSE | Read both functions directly | Confirmed CPU-agnostic (never call `cpu_.step()` or `step_cpu_instruction()`), so PAUSE has no interaction with this path at all — consistent with the architecture's stated design (PAUSE affects only the CPU's own clock domain) |
| Interrupt-loss risk while paused | Read `step_cpu_instruction()`'s gate position (before the `vdp_.irq_active()` sample) and confirmed the VDP holds `/INT` as a LEVEL, not an edge | Confirmed no interrupt can be lost: the VDP's IRQ line is re-sampled unconditionally on every non-paused call: as soon as PAUSE releases, the next call correctly re-samples and services any still-pending interrupt |
| Deferred-backlog / 34-row review | Read `agent-protocol/state/deferred-backlog.md` C8 row and its surrounding context in full | C8 correctly disposed `IN-PROGRESS (M25 implementation complete, pending QA)`, matching the implementation report exactly; status-column flip correctly left un-flipped, pending this sign-off |
| `agent-protocol/state/current-phase.md`, `milestones.md`, `definition-of-done.yaml` | Read directly | M25 correctly shown as implementation-complete-pending-QA everywhere (`overall_done: false`, `signoff_decision_recorded: false`); not prematurely marked done anywhere |
| Evidence-gate scripts | Ran myself: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` | `Asset validation result: True` (7 BIOS files + 2 ROM files present); checksum report refreshed successfully |
| `tests/CMakeLists.txt` / `CMakeLists.txt` build wiring | Read both diffs in full | Purely additive: 2 new source-registration lines in `CMakeLists.txt`; 6 new `add_executable`/`target_link_libraries`/`add_test` blocks in `tests/CMakeLists.txt`, no existing target's registration touched, no `SONY_MSX_*_DIR`-style asset-root wiring needed (nothing new reads a ROM/BIOS asset) |

---

## 3. Failures and Risk Ranking

**No test failures. No production-code defect found.** All substantive technical claims —
hardware PAUSE's freeze/non-bypass guarantee, the Speed Controller's deterministic duty cycle,
Ren-Sha Turbo's R-M25-6 invariant, zero regression across 130/130 tests including a fresh,
from-scratch 22m31s re-run of the ZEXALL/ZEXDOC sweep, and genuine live openMSX A/B PARITY for
Ren-Sha Turbo — are independently verified and sound.

### Finding 1 (Low, documentation-accuracy, non-blocking) — the BLOCKED disposition undercounts the Sony machine XML inventory ("five" instead of "six")

`docs/m25-planner-package.md` §2.2/§4 Acceptance Criterion 9, `docs/m25-implementation-report.md`
§5.2, `docs/m25-parity-trace-diff.md`, and `tools/openmsx-m25-rensha-parity.ps1`'s own static
report text all say: *"None of the five real Sony MSX machine XML definitions wire a Pause or
SpeedController device."* I ran `find references/openmsx-21.0/share/machines -iname
"Sony_HB-F1*"` myself and found **six** files, not five: `Sony_HB-F1.xml`, `Sony_HB-F1II.xml`,
`Sony_HB-F1XD.xml`, `Sony_HB-F1XDJ.xml`, `Sony_HB-F1XDmk2.xml` (missed by every cited search this
cycle), and `Sony_HB-F1XV.xml`.

I independently checked the missed file: `Sony_HB-F1XDmk2.xml` also carries a real
`<RenShaTurbo min_ints=47 max_ints=221>` block (interesting in its own right — a seventh
data point corroborating the same calibration, not a contradiction), does **not** wire any
Pause/SpeedController device (`grep -in "pause\|speedcontroller"` over the file: no hits besides
the RenShaTurbo XML tag name itself), and its own `<description>` ("A newer version of the Sony
HB-F1XD, but in emulation it's identical to its predecessor") is consistent with the same
"not emulated" conclusion the other four machines state explicitly (its predecessor, HB-F1XD,
explicitly says "speed controller (not emulated)").

**This does not change the substantive BLOCKED disposition** — the sixth machine reinforces
rather than undermines the conclusion. It is a citation-completeness gap in the search's own
"exhaustive, not assumed" claim, analogous in kind (though lower-stakes, since it affects prose
documentation rather than code or a numeric grounding fact) to the "one minor citation-location
error" the coordinator already found and disposed as non-blocking during the planner-package
review (RESP-M25-001). I am treating this the same way: a real, disclosed gap, not gating.

### No other findings

- License isolation: sound — every touched `src/` file read in full; the two embedded
  RenShaTurbo scalar constants are within the standing memory's explicit carve-out.
- Zero regression: independently confirmed (129/129 fast + 1/1 slow, clean rebuild, 130/130
  total).
- The 12-file zero-tolerance CPU-timing-oracle diff: independently confirmed empty.
- Hardware PAUSE freeze/non-bypass: independently confirmed both by test and by static
  inspection (no I/O port wired to the gate).
- Speed Controller duty cycle: independently confirmed deterministic/hand-computable across
  three separate oracles (unit/integration/system).
- Ren-Sha Turbo R-M25-6: independently confirmed exhaustive, both in isolation and through real
  CPU-driven cycles.
- openMSX A/B: independently re-run from scratch by me this cycle (the one piece of evidence
  explicitly left for QA) — genuine PARITY reproduced, raw sample data inspected directly.
- Two self-caught bugs disclosed in the implementation report (a test loop-length constant, a
  PowerShell backtick-escaping bug) — both are honest, transparent, test/tooling-only
  disclosures with zero production-code risk; I read the actual fixed test file and confirmed
  `kN=4` (not `5`) is what is committed. This reflects positively on process discipline, not a
  concern in itself.
- Design-choice reasonableness (PAUSE toggle-vs-hold, `kPeriodFrames=8`): both independently
  judged reasonable, well-documented as non-authoritative, and easily revisable.
- Backlog/state consistency: independently confirmed, no drift; nothing prematurely marked done.

---

## 4. Required Fixes

### Required before this milestone can be called a clean, unconditional PASS

**None.** No blocker-level or regression-safety-affecting gap was found.

### Recommended, non-blocking follow-ups for the coordinator at closure

1. Fix Finding 1's "five" -> "six" wording in `docs/m25-planner-package.md`,
   `docs/m25-implementation-report.md`, `docs/m25-parity-trace-diff.md`, and
   `tools/openmsx-m25-rensha-parity.ps1`'s static report text, adding the `Sony_HB-F1XDmk2.xml`
   citation alongside the existing five. Purely a documentation-accuracy correction — the
   underlying technical conclusion (openMSX has zero Sony PAUSE/SpeedController modeling) is
   unaffected and remains true including the sixth machine.
2. Consider implementing the developer-flagged, planner's-judgment-optional
   `DebugEventType::Pause`/`Resume` + state-dump section (per `docs/m25-implementation-report.md`
   §7) in a future cycle, likely alongside C9 (SDL3 frontend) when PAUSE/Speed-Controller/Ren-Sha
   Turbo get their first real input binding — not required for C8's own closure.

No other action is required. All of §2's Regression Matrix rows passed independent QA
reproduction with no other gap found.

---

## 5. Explicit Judgment Calls

### 5.1 Are the two first-principles design defaults (A-M25-1 toggle semantics, A-M25-3
`kPeriodFrames=8`) reasonable, not just present?

**Ruling: Yes, both are reasonable design choices, appropriately labeled as non-authoritative.**

- **PAUSE toggle-vs-hold (A-M25-1):** no Sony MB670836 datasheet or community measurement of the
  real button's electrical behavior exists anywhere in this project's grounding material — I
  re-confirmed this by re-reading the full S1985 fact-sheet §9 myself, not merely trusting the
  planner's claim. Given the absence of grounding data, toggle semantics is a sound choice on
  independent merits: it is the more usable pattern for a PAUSE feature that will eventually be
  bound to a single keyboard key (deferred to C9) — a "hold to pause" semantic would require
  continuously holding a key down, unusual for a pause feature — and it produces clean,
  deterministic, exhaustively-testable semantics. The `MSXTurboRPause` citation is correctly
  scoped as design-inspiration only (explicitly caveated as a different chipset/machine in both
  the planner package and the code comments), never presented as grounding evidence for the
  actual Sony circuit.
- **`kPeriodFrames=8` (A-M25-3):** similarly ungrounded in any datasheet (re-confirmed by
  re-reading Z80A fact-sheet §6 item 3 in full). Eight VBlank frames at NTSC 60Hz is a
  ~133ms period — a reasonable "slow motion" granularity — and a power-of-two period gives clean
  modular arithmetic and 8 discrete speed levels. The choice is fully deterministic, hand-computable
  (verified via three independent oracles), and explicitly documented as provisional.

Both are correctly distinguished throughout the codebase and documentation from the
hardware-grounded facts (A-M25-2's literal PC/R freeze reading, A-M25-4's OR-combine
architecture, A-M25-5/A-M25-7's RenShaTurbo calibration/wiring) — this project's own established
convention of clearly separating "cited hardware fact" from "first-principles default" is
followed correctly here, not blurred.

### 5.2 Is the license-sensitivity judgment (citing RenShaTurbo's `min_ints=47`/`max_ints=221`)
sound under the standing "zero license-sensitive future work" directive?

**Ruling: Yes, sound.** I read `feedback_license_sensitive_scope.md` in full. Its own explicit
text draws the line at "large, exacting data tables (hundreds of entries)" (the concrete example
given is the ~340-entry VDP access-slot timing table), while carving out "small, discrete
numeric facts (a handful of named constants, a single formula)" as the established, accepted
pattern used throughout M20-M23. Two scalar integers extracted from one real per-machine XML
config file — now corroborated by a THIRD real machine's XML carrying the identical values
(`Sony_HB-F1XDmk2.xml`, found during my own independent search, Finding 1) — is unambiguously the
carved-out case, not the banned one. This is categorically the same pattern already used
extensively for SHA1 hashes, port numbers, wait-state counts, and ROM sizes throughout M11-M24.

### 5.3 Is the PAUSE/Speed-Controller BLOCKED A/B disposition genuinely justified?

**Ruling: Yes, with one non-blocking completeness correction (Finding 1).** I independently
re-ran the same openMSX source searches the planner/developer performed — grep across
`references/openmsx-21.0/` for PAUSE-adjacent classes, and a fresh `find` for every
`Sony_HB-F1*.xml` file rather than trusting the cited enumeration. The substantive conclusion
(openMSX 21.0 has zero Sony-specific PAUSE/speed-controller modeling for the S1985/MB670836
chipset family) holds and is, if anything, reinforced by the sixth machine I found. `SG1000Pause`
(different machine family, NMI mechanism) and `MSXTurboRPause` (different chipset, S1990,
architecturally-incompatible whole-session `getMotherBoard().pause()`) are both genuinely
unrelated, confirmed by direct source read. This BLOCKED disposition correctly does not gate C8's
closure per Acceptance Criterion 9, consistent with the M21/C3-M24 precedents.

---

## 6. Independent Verification Detail

### A. Clean rebuild (directly observed, not trusted from any prior report)

```
rm -rf build
cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF     -> configured from scratch
cmake --build build --config Debug                  -> succeeded, zero errors
ctest --test-dir build -C Debug -LE m24_slow_full_sweep --output-on-failure
  100% tests passed, 0 tests failed out of 129
  Total Test time (real) = 6.92 sec
ctest --test-dir build -C Debug -R hbf1xv_m24_zexall_system_test --output-on-failure
  1/1 Test #124: hbf1xv_m24_zexall_system_test ....   Passed  1350.80 sec
  100% tests passed, 0 tests failed out of 1
```

### B. Exact captured output for the slow test (read from `build/Testing/Temporary/LastTest.log`)

```
ZEXALL: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0
ZEXDOC: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0
```

Byte-for-byte identical instruction count and marker split to every prior independent run this
project has recorded (M24 developer/coordinator/QA, and this M25 QA cycle's own fresh run) —
only wall-clock time differs (1350.80s here vs. 1359-1638s in prior cycles), as expected from
differing machine load.

### C. `git diff v1.0.24` scope (ran myself)

```
git diff v1.0.24 --name-only -- src/devices/cpu/ src/devices/video/ src/devices/audio/ \
    src/devices/rtc/ src/devices/fdc/ src/devices/cartridge/ src/devices/memory/ \
    src/devices/halnote/ src/devices/kanji/ src/core/          -> (empty)
git diff v1.0.24 --stat -- <12 named CPU-timing-oracle test files>   -> (empty)
git diff v1.0.24 --stat -- src/ CMakeLists.txt tests/CMakeLists.txt
    -> 8 files changed, 203 insertions(+), 1 deletion(-)  (exact match to implementation report)
git diff v1.0.24 --name-only -- src/main.cpp                    -> (empty; no CLI wiring)
```

### D. Independent openMSX A/B re-run (WSL openMSX 19.1, ran myself end-to-end)

```
openMSX: /usr/bin/openmsx (openMSX 19.1 flavour: debian components: ALSAMIDI CORE GL LASERDISC)
openMSX raw output written to: build/m25_rensha_openmsx_raw.txt
M25 RESULT: Ren-Sha Turbo PARITY (not-held invariant + held alternation).
Hardware PAUSE/Speed Controller: BLOCKED (honest, expected).
```

Raw sample inspection (`build/m25_rensha_openmsx_raw.txt`, read directly):
```
OMSETUP renshaturbo_set=100
OMSETUP space_pressed=1
OMSAMPLE notheld 0 row8=FF r14=FF   (... 10/10 idle ...)
OMSAMPLE held 0 row8=FE r14=FF      (pressed)
OMSAMPLE held 1 row8=FF r14=FF      (released)
OMSAMPLE held 5 row8=FE r14=FF      (pressed again)
   ... genuine alternation across 40 held samples, 20 pressed / 20 released ...
OMDONE
```

### E. Static-inspection confirmation of PAUSE non-bypassability (Acceptance Criterion 3)

```
grep -rn "pause_controller_" src/machine/hbf1xv_machine.cpp src/machine/hbf1xv_machine.h
  -> cold_boot() [.reset()], run_frame() [.on_vsync()], step_cpu_instruction() [gate check],
     pause_controller() const/non-const accessors. NO I/O port dispatch table, NO CpuBusClient
     write path, references this member anywhere else.
```

### F. Evidence-gate scripts (ran myself)

```
tools/validate-assets.ps1        -> Asset validation result: True (7 BIOS + 2 ROM files present)
tools/checksum-assets.ps1        -> Checksum report written to docs/asset-checksums.txt
```

---

## Summary for the coordinator

- **Full regression independently reproduced by QA from a genuinely clean rebuild**: 129/129 fast
  (6.92s) + 1/1 slow (1350.80s) = **130/130, zero failures.**
- **The 12-file zero-tolerance CPU-timing-oracle diff is independently confirmed empty.** The
  single highest-sensitivity edit this milestone makes (`step_cpu_instruction()`'s early-return
  gate) is correctly positioned, purely additive, and provably non-bypassable via any CPU-visible
  API (confirmed both by test and by static inspection).
- **The live openMSX A/B evidence — the one piece explicitly left for QA — is independently
  reproduced end-to-end**, not merely re-read: genuine PARITY for Ren-Sha Turbo (not-held
  zero-effect invariant + held alternation, both against the real `Sony_HB-F1XV` machine), honest
  BLOCKED disposition for hardware PAUSE/Speed-Controller (re-confirmed sound, with one
  non-blocking completeness correction, Finding 1).
- **Both first-principles design defaults (PAUSE toggle semantics, `kPeriodFrames=8`) are judged
  reasonable**, not merely present — appropriately documented as provisional and easily
  revisable, correctly separated from the hardware-grounded facts throughout.
- **The license-sensitivity judgment for RenShaTurbo's `47`/`221` scalars is sound** under the
  standing directive, independently re-checked against the memory's own explicit text.
- **The one finding (Finding 1, Low severity)**: the BLOCKED disposition's citation of "five" Sony
  machine XML definitions should be "six" (`Sony_HB-F1XDmk2.xml` was missed) — a documentation
  completeness gap that reinforces rather than undermines the substantive conclusion, non-blocking.

**Recommendation: Pass.** All of this milestone's substantive technical claims are independently
verified and sound: zero regression (130/130, including a fresh from-scratch 22m31s ZEXALL/ZEXDOC
re-run), hardware PAUSE genuinely and provably freezes CPU state and cannot be bypassed, the Speed
Controller's duty cycle is deterministic and hand-computable (three independent oracles), Ren-Sha
Turbo never forces a spurious press (exhaustively tested both in isolation and through real
CPU-driven cycles), genuine live openMSX A/B PARITY for Ren-Sha Turbo (independently reproduced by
me this cycle), a sound and honestly-justified BLOCKED disposition for PAUSE/Speed-Controller, a
sound license-sensitivity judgment, and a complete, accurate 34-row backlog review. The single
finding (Finding 1) is a Low-severity, non-blocking documentation-accuracy nit that does not
affect any code, any test, or the substantive technical conclusion it appears in — it does not
gate this milestone's closure, and I recommend fixing it as a same-cycle or next-cycle cheap
follow-up (§4) rather than routing back to the developer before tagging.

Per this milestone's own standing directive: this is a clean PASS, so the coordinator may proceed
through the release-decision/tag step (`v1.0.25`) without an additional human pause.
